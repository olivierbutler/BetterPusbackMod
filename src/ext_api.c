/*
 * External interface: publishes the operation state for other plugins
 * (README-EXTERNAL-API.md). The state comes from the same Ground Operations
 * snapshot the panel draws, refreshed here whether or not the panel is shown,
 * so the datarefs and the panel always agree.
 */

#include <string.h>

#include <XPLMProcessing.h>

#include <acfutils/dr.h>
#include <acfutils/log.h>

#include "bp.h"
#include "ext_api.h"
#include "ext_api_state.h"
#include "ground_ops_ui.h"
#include "xplane.h"

#define EXT_API_INTERVAL 0.1f      /* seconds between refreshes */

static bool_t inited = B_FALSE;
static XPLMFlightLoopID refresh_loop = NULL;
static ground_ops_state_t snapshot_cache;
static bp_ext_state_t published;

static int api_version = BP_EXT_API_VERSION;
static char plugin_version[32] = BP_PLUGIN_VERSION;
static float push_speed = -1;              /* m/s, -1 when not known */
static float push_distance = -1;           /* m left to push, -1 when not known */

static dr_t plugin_version_dr;
static dr_t push_speed_dr;
static dr_t push_distance_dr;
static dr_t api_version_dr;
static dr_t state_seq_dr;
static dr_t emergency_tow_dr;
static dr_t step_dr;
static dr_t step_name_dr;
static dr_t stage_dr;
static dr_t stage_name_dr;
static dr_t action_dr;
static dr_t action_name_dr;
static dr_t paused_dr;

static void
refresh(void)
{
    ground_ops_raw_state_t raw;
    bp_ext_state_t next;

    if (!ground_ops_ui_collect_state(&raw)) {
        memset(&raw, 0, sizeof (raw));
        raw.step = PB_STEP_OFF;
    }
    (void)ground_ops_state_update(&snapshot_cache, &raw);
    bp_ext_state_from(&raw, ground_ops_state_get(&snapshot_cache),
        &published, &next);
    published = next;
}

/* The tug's speed and the push distance left: the figures the panel shows. */
static void
refresh_progress(void)
{
    double speed, distance;
    bool_t speed_valid, distance_valid;

    bp_get_ground_ops_metrics(&speed, &speed_valid, &distance,
        &distance_valid);
    push_speed = speed_valid ? (float)speed : -1;
    push_distance = distance_valid ? (float)distance : -1;
}

static float
refresh_cb(float elapsed, float elapsed_flight, int counter, void *refcon)
{
    (void)elapsed;
    (void)elapsed_flight;
    (void)counter;
    (void)refcon;
    refresh_progress();
    refresh();
    return (EXT_API_INTERVAL);
}

void
ext_api_init(void)
{
    XPLMCreateFlightLoop_t loop = {
        sizeof (loop), xplm_FlightLoop_Phase_AfterFlightModel,
        refresh_cb, NULL
    };

    if (inited)
        return;
    ground_ops_state_init(&snapshot_cache);
    memset(&published, 0, sizeof (published));
    refresh();

    dr_create_i(&api_version_dr, &api_version, B_FALSE, "bp/api_version");
    dr_create_b(&plugin_version_dr, plugin_version, sizeof (plugin_version),
        B_FALSE, "bp/plugin_version");
    dr_create_f(&push_speed_dr, &push_speed, B_FALSE, "bp/push_speed");
    dr_create_f(&push_distance_dr, &push_distance, B_FALSE,
        "bp/push_distance_remaining");
    dr_create_i(&state_seq_dr, &published.state_seq, B_FALSE,
        "bp/state_seq");
    dr_create_i(&emergency_tow_dr, &published.emergency_tow, B_FALSE,
        "bp/emergency_tow");
    dr_create_i(&step_dr, &published.step, B_FALSE, "bp/step");
    dr_create_b(&step_name_dr, published.step_name,
        sizeof (published.step_name), B_FALSE, "bp/step_name");
    dr_create_i(&stage_dr, &published.stage, B_FALSE, "bp/stage");
    dr_create_b(&stage_name_dr, published.stage_name,
        sizeof (published.stage_name), B_FALSE, "bp/stage_name");
    dr_create_i(&action_dr, &published.action, B_FALSE, "bp/action");
    dr_create_b(&action_name_dr, published.action_name,
        sizeof (published.action_name), B_FALSE, "bp/action_name");
    dr_create_i(&paused_dr, &published.paused, B_FALSE, "bp/paused");

    refresh_loop = XPLMCreateFlightLoop(&loop);
    if (refresh_loop != NULL)
        XPLMScheduleFlightLoop(refresh_loop, EXT_API_INTERVAL, 1);
    else
        logMsg(BP_ERROR_LOG "External interface: unable to create the "
            "refresh loop; bp/ state datarefs will not update");
    inited = B_TRUE;
    logMsg(BP_INFO_LOG "External interface version %d published",
        BP_EXT_API_VERSION);
}

void
ext_api_fini(void)
{
    if (!inited)
        return;
    if (refresh_loop != NULL) {
        XPLMDestroyFlightLoop(refresh_loop);
        refresh_loop = NULL;
    }
    dr_delete(&api_version_dr);
    dr_delete(&plugin_version_dr);
    dr_delete(&push_speed_dr);
    dr_delete(&push_distance_dr);
    dr_delete(&state_seq_dr);
    dr_delete(&emergency_tow_dr);
    dr_delete(&step_dr);
    dr_delete(&step_name_dr);
    dr_delete(&stage_dr);
    dr_delete(&stage_name_dr);
    dr_delete(&action_dr);
    dr_delete(&action_name_dr);
    dr_delete(&paused_dr);
    inited = B_FALSE;
}
