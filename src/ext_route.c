/*
 * External interface: push routes from and to other plugins
 * (README-EXTERNAL-API.md, "External routes"). Another plugin writes a route
 * into bp/route_in and runs BetterPushback/load_route; the answer comes back
 * in bp/route_status and bp/route_reason. The current route is published in
 * the same text form in bp/route_current. The stand's two saved-route slots
 * are saved and loaded with BetterPushback/save_route_slot and
 * load_route_slot.
 */

#include <string.h>

#include <XPLMUtilities.h>

#include <acfutils/dr.h>
#include <acfutils/intl.h>
#include <acfutils/log.h>

#include "bp.h"
#include "bp_cam.h"
#include "ext_api.h"
#include "ext_api_route.h"
#include "xplane.h"

#define STAND_REFRESH_TICKS 50     /* ext_route_refresh runs ~10 times a second */
#define STAND_RETRY_TICKS 600      /* a minute before initialising is tried again */

enum {
    ROUTE_STATUS_NONE = 0,
    ROUTE_STATUS_ACCEPTED = 1,
    ROUTE_STATUS_REJECTED = 2
};

static bool_t inited = B_FALSE;
static char route_in[BP_EXT_ROUTE_TEXT_LEN];
static char route_current[BP_EXT_ROUTE_TEXT_LEN];
static char route_reason[BP_EXT_ROUTE_REASON_LEN];
static int route_seq = 0;
static int route_status = ROUTE_STATUS_NONE;
static int route_source = BP_ROUTE_SOURCE_NONE;
static char route_source_name[16] = "none";
static uint64_t current_signature = 0;

/* Indexed by bp_route_source_t (published numbers: never change). */
static const char *const source_names[] = {
    "none", "planner", "saved", "external"
};

static dr_t route_in_dr, route_current_dr, route_reason_dr;
static dr_t route_seq_dr, route_status_dr;
static dr_t route_source_dr, route_source_name_dr;
static XPLMCommandRef load_cmd = NULL, check_cmd = NULL, clear_cmd = NULL;

/* Saved routes (version 6). */
static int route_slot = 1;                 /* written by the other plugin */
static char route_stand[48];
static int route_slot_saved[2] = { -1, -1 };
static int stand_ticks = 0;
static bool_t stand_wanted = B_FALSE;       /* another plugin reads the slots */
static dr_t route_slot_dr, route_stand_dr, route_slot1_dr, route_slot2_dr;
static dr_t onground_dr, groundspeed_dr;
static XPLMCommandRef save_slot_cmd = NULL, load_slot_cmd = NULL;

static void
answer(bool_t accepted, const char *reason)
{
    route_seq++;
    route_status = accepted ? ROUTE_STATUS_ACCEPTED : ROUTE_STATUS_REJECTED;
    (void)snprintf(route_reason, sizeof (route_reason), "%s",
        reason != NULL ? reason : "");
    if (!accepted)
        logMsg(BP_WARN_LOG "External route rejected: %s", route_reason);
}

/* BetterPushback/load_route (refcon NULL) and check_route (refcon non-NULL). */
static int
load_route_cb(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon)
{
    bp_ext_pose_t poses[BP_EXT_ROUTE_MAX_POSES];
    char reason[BP_EXT_ROUTE_REASON_LEN] = "";
    bool_t check_only = (refcon != NULL);
    int n;

    (void)cmd;
    if (phase != xplm_CommandBegin)
        return (1);
    route_in[sizeof (route_in) - 1] = '\0';
    n = bp_ext_route_parse(route_in, poses, BP_EXT_ROUTE_MAX_POSES, reason,
        sizeof (reason));
    if (n < 0) {
        answer(B_FALSE, reason);
        return (1);
    }
    if (check_only) {
        bool_t fits = bp_route_check_external(poses, n, reason,
            sizeof (reason));
        answer(fits, fits ? "" : reason);
        return (1);
    }
    if (!bp_route_load_external(poses, n, reason, sizeof (reason))) {
        answer(B_FALSE, reason);
        return (1);
    }
    ext_route_refresh();
    answer(B_TRUE, "");
    return (1);
}

static int
clear_route_cb(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon)
{
    char reason[BP_EXT_ROUTE_REASON_LEN] = "";

    (void)cmd;
    (void)refcon;
    if (phase != xplm_CommandBegin)
        return (1);
    if (!bp_route_clear_external(reason, sizeof (reason))) {
        answer(B_FALSE, reason);
        return (1);
    }
    ext_route_refresh();
    answer(B_TRUE, "");
    return (1);
}

static void
clear_stand(void)
{
    route_stand[0] = '\0';
    route_slot_saved[0] = route_slot_saved[1] = -1;
}

/* Which saved routes the stand has; -1 when not on a published stand. */
static void
refresh_stand(void)
{
    char reason[BP_EXT_ROUTE_REASON_LEN];
    bool_t saved[2];

    stand_ticks = 0;
    if (!bp_cam_stand_routes(route_stand, sizeof (route_stand), saved,
        reason, sizeof (reason))) {
        clear_stand();
        return;
    }
    route_slot_saved[0] = saved[0] ? 1 : 0;
    route_slot_saved[1] = saved[1] ? 1 : 0;
}

/*
 * Another plugin reads a slot: it wants the stand, so the stand is worked out
 * even before BetterPushback is otherwise used (see ext_route_refresh).
 */
static void
stand_read_cb(dr_t *dr, void *value_out)
{
    (void)dr;
    (void)value_out;
    if (!stand_wanted) {
        stand_wanted = B_TRUE;
        stand_ticks = STAND_REFRESH_TICKS;
    }
}

/*
 * On the ground and standing still: read here, as BetterPushback's own check
 * needs it initialised.
 */
static bool_t
acf_idle_on_ground(void)
{
    return (dr_geti(&onground_dr) == 1 && dr_getf(&groundspeed_dr) < 1);
}

static int
slot_cb(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon)
{
    char reason[BP_EXT_ROUTE_REASON_LEN] = "";
    bool_t save = (refcon != NULL);
    bool_t ok;

    (void)cmd;
    if (phase != xplm_CommandBegin)
        return (1);
    if (route_slot != 1 && route_slot != 2) {
        answer(B_FALSE, "bp/route_slot must be 1 or 2");
        return (1);
    }
    ok = save ?
        bp_cam_route_slot_save((unsigned)route_slot - 1, reason,
        sizeof (reason)) :
        bp_cam_route_slot_load((unsigned)route_slot - 1, reason,
        sizeof (reason));
    refresh_stand();
    ext_route_refresh();
    answer(ok, ok ? "" : reason);
    return (1);
}

void
ext_route_refresh(void)
{
    bp_ext_pose_t poses[BP_EXT_ROUTE_MAX_POSES];
    uint64_t signature;
    int n;

    /*
     * The stand only matters while a route may still change. BetterPushback
     * initialises only once it is used, and loading a flight or an aircraft
     * undoes that; until then the stand is unknown, unless another plugin
     * reads the slots: then it is worked out as soon as the aircraft stands
     * still on the ground (initialising there is quiet).
     */
    if (++stand_ticks >= STAND_REFRESH_TICKS) {
        if (!bp_is_inited() && !(stand_wanted && acf_idle_on_ground())) {
            clear_stand();
            stand_ticks = 0;
        } else if (bp_route_change_refused() == NULL) {
            refresh_stand();
        } else if (!bp_is_inited()) {
            clear_stand();          /* it cannot initialise here */
            stand_ticks = -STAND_RETRY_TICKS;
        }
    }

    route_source = (int)bp_route_source();
    (void)snprintf(route_source_name, sizeof (route_source_name), "%s",
        route_source >= 0 && route_source <
        (int)(sizeof (source_names) / sizeof (source_names[0])) ?
        source_names[route_source] : "unknown");
    signature = bp_route_signature();
    if (signature == current_signature && route_current[0] != '\0')
        return;
    current_signature = signature;
    n = bp_route_export(poses, BP_EXT_ROUTE_MAX_POSES);
    if (n == 0 || !bp_ext_route_format(poses, n, route_current,
        sizeof (route_current)))
        route_current[0] = '\0';
}

void
ext_route_init(void)
{
    if (inited)
        return;
    memset(route_in, 0, sizeof (route_in));
    route_current[0] = route_reason[0] = '\0';
    route_seq = 0;
    route_status = ROUTE_STATUS_NONE;
    route_source = BP_ROUTE_SOURCE_NONE;
    (void)snprintf(route_source_name, sizeof (route_source_name), "none");
    current_signature = 0;

    dr_create_b(&route_in_dr, route_in, sizeof (route_in), B_TRUE,
        "bp/route_in");
    dr_create_b(&route_current_dr, route_current, sizeof (route_current),
        B_FALSE, "bp/route_current");
    dr_create_b(&route_reason_dr, route_reason, sizeof (route_reason),
        B_FALSE, "bp/route_reason");
    dr_create_i(&route_seq_dr, &route_seq, B_FALSE, "bp/route_seq");
    dr_create_i(&route_status_dr, &route_status, B_FALSE, "bp/route_status");
    dr_create_i(&route_source_dr, &route_source, B_FALSE, "bp/route_source");
    dr_create_b(&route_source_name_dr, route_source_name,
        sizeof (route_source_name), B_FALSE, "bp/route_source_name");

    load_cmd = XPLMCreateCommand("BetterPushback/load_route",
        _("Load the pushback route another plugin wrote"));
    check_cmd = XPLMCreateCommand("BetterPushback/check_route",
        _("Check whether another plugin's pushback route fits"));
    clear_cmd = XPLMCreateCommand("BetterPushback/clear_route",
        _("Clear the pushback route"));
    XPLMRegisterCommandHandler(load_cmd, load_route_cb, 1, NULL);
    XPLMRegisterCommandHandler(check_cmd, load_route_cb, 1, (void *)1);
    XPLMRegisterCommandHandler(clear_cmd, clear_route_cb, 1, NULL);

    route_slot = 1;
    clear_stand();
    stand_ticks = STAND_REFRESH_TICKS;
    stand_wanted = B_FALSE;
    fdr_find(&onground_dr, "sim/flightmodel/failures/onground_any");
    fdr_find(&groundspeed_dr, "sim/flightmodel/position/groundspeed");
    dr_create_i(&route_slot_dr, &route_slot, B_TRUE, "bp/route_slot");
    dr_create_b(&route_stand_dr, route_stand, sizeof (route_stand), B_FALSE,
        "bp/route_stand");
    dr_create_i_cfg(&route_slot1_dr, &route_slot_saved[0],
        (dr_cfg_t){ .read_cb = stand_read_cb }, "bp/route_slot1");
    dr_create_i_cfg(&route_slot2_dr, &route_slot_saved[1],
        (dr_cfg_t){ .read_cb = stand_read_cb }, "bp/route_slot2");
    save_slot_cmd = XPLMCreateCommand("BetterPushback/save_route_slot",
        _("Save the pushback route to the stand's slot in bp/route_slot"));
    load_slot_cmd = XPLMCreateCommand("BetterPushback/load_route_slot",
        _("Load the stand's saved pushback route from bp/route_slot"));
    XPLMRegisterCommandHandler(save_slot_cmd, slot_cb, 1, (void *)1);
    XPLMRegisterCommandHandler(load_slot_cmd, slot_cb, 1, NULL);
    inited = B_TRUE;
}

void
ext_route_fini(void)
{
    if (!inited)
        return;
    XPLMUnregisterCommandHandler(load_cmd, load_route_cb, 1, NULL);
    XPLMUnregisterCommandHandler(check_cmd, load_route_cb, 1, (void *)1);
    XPLMUnregisterCommandHandler(clear_cmd, clear_route_cb, 1, NULL);
    XPLMUnregisterCommandHandler(save_slot_cmd, slot_cb, 1, (void *)1);
    XPLMUnregisterCommandHandler(load_slot_cmd, slot_cb, 1, NULL);
    dr_delete(&route_slot_dr);
    dr_delete(&route_stand_dr);
    dr_delete(&route_slot1_dr);
    dr_delete(&route_slot2_dr);
    dr_delete(&route_in_dr);
    dr_delete(&route_current_dr);
    dr_delete(&route_reason_dr);
    dr_delete(&route_seq_dr);
    dr_delete(&route_status_dr);
    dr_delete(&route_source_dr);
    dr_delete(&route_source_name_dr);
    inited = B_FALSE;
}
