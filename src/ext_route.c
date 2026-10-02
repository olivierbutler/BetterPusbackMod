/*
 * External interface: push routes from and to other plugins
 * (README-EXTERNAL-API.md, "External routes"). Another plugin writes a route
 * into bp/route_in and runs BetterPushback/load_route; the answer comes back
 * in bp/route_status and bp/route_reason. The current route is published in
 * the same text form in bp/route_current.
 */

#include <string.h>

#include <XPLMUtilities.h>

#include <acfutils/dr.h>
#include <acfutils/intl.h>
#include <acfutils/log.h>

#include "bp.h"
#include "ext_api.h"
#include "ext_api_route.h"
#include "xplane.h"

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

void
ext_route_refresh(void)
{
    bp_ext_pose_t poses[BP_EXT_ROUTE_MAX_POSES];
    uint64_t signature = bp_route_signature();
    int n;

    route_source = (int)bp_route_source();
    (void)snprintf(route_source_name, sizeof (route_source_name), "%s",
        route_source >= 0 && route_source <
        (int)(sizeof (source_names) / sizeof (source_names[0])) ?
        source_names[route_source] : "unknown");
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
    dr_delete(&route_in_dr);
    dr_delete(&route_current_dr);
    dr_delete(&route_reason_dr);
    dr_delete(&route_seq_dr);
    dr_delete(&route_status_dr);
    dr_delete(&route_source_dr);
    dr_delete(&route_source_name_dr);
    inited = B_FALSE;
}
