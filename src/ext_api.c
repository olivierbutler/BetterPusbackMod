/*
 * External interface: publishes the operation state for other plugins
 * (README-EXTERNAL-API.md). The state comes from the same Ground Operations
 * snapshot the panel draws, refreshed here whether or not the panel is shown,
 * so the datarefs and the panel always agree.
 */

#include <string.h>

#include <XPLMProcessing.h>

#include <acfutils/dr.h>
#include <acfutils/helpers.h>
#include <acfutils/log.h>

#include "bp.h"
#include "ext_api.h"
#include "ext_api_msgs.h"
#include "ext_api_state.h"
#include "ground_ops_ui.h"
#include "msg.h"
#include "xplane.h"

#define EXT_API_INTERVAL 0.1f      /* seconds between refreshes */

/* The crew line last started (README-EXTERNAL-API.md, "Crew lines"). */
typedef struct {
    int seq;
    int msg;
    char key[BP_EXT_MSG_KEY_LEN];
    char text[BP_EXT_MSG_TEXT_LEN];
    char caption[BP_EXT_MSG_CAPTION_LEN];
    char voice[BP_EXT_MSG_VOICE_LEN];
    float duration;
    int playing;
} ext_line_t;

static bool_t inited = B_FALSE;
static XPLMFlightLoopID refresh_loop = NULL;
static ground_ops_state_t snapshot_cache;
static bp_ext_state_t published;
static ext_line_t line;
static uint64_t line_source_seq = 0;

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
static dr_t blocker_dr;
static dr_t blocker_name_dr;
static dr_t blocker_item_dr;
static dr_t blocker_item_kind_dr;
static dr_t status_text_dr;
static dr_t msg_seq_dr;
static dr_t msg_dr;
static dr_t msg_key_dr;
static dr_t msg_text_dr;
static dr_t msg_caption_dr;
static dr_t msg_voice_dr;
static dr_t msg_duration_dr;
static dr_t msg_playing_dr;
static dr_t voice_mode_dr;
static dr_t voice_done_seq_dr;
static dr_t voice_heartbeat_dr;
static dr_t voice_external_dr;
static int voice_external = 0;

/*
 * Publishes the crew line msg.c last started. bp/msg_seq is msg.c's own line
 * counter, which moves whenever a line starts (even with the simulator's
 * sound off): a reader sees it jump by two if two lines start within one
 * refresh interval, and later parts of the interface refer to lines by it.
 */
static void
refresh_line(void)
{
    msg_caption_state_t state;

    msg_get_caption_state(&state);
    line.playing = state.active ? 1 : 0;
    if (state.sequence == line_source_seq)
        return;
    line_source_seq = state.sequence;
    line.seq = (int)state.sequence;
    if (state.spoken) {
        /* Said by X-Plane's speech: the text as spoken, no recording. */
        line.msg = bp_ext_msg_spoken_public(state.spoken_kind);
        strlcpy(line.key, bp_ext_msg_key(line.msg), sizeof (line.key));
        strlcpy(line.text, state.spoken_text, sizeof (line.text));
        strlcpy(line.caption, state.spoken_text, sizeof (line.caption));
        strlcpy(line.voice, "xplane", sizeof (line.voice));
        /* About 15 characters a second; X-Plane does not say how long. */
        line.duration = (float)strlen(state.spoken_text) / 15.0f;
        return;
    }
    line.msg = bp_ext_msg_public(state.message);
    strlcpy(line.key, bp_ext_msg_key(line.msg), sizeof (line.key));
    strlcpy(line.text, bp_ext_msg_text(line.msg), sizeof (line.text));
    strlcpy(line.caption,
        ground_ops_caption_text(bp_ext_msg_caption(line.msg)),
        sizeof (line.caption));
    strlcpy(line.voice, msg_voice_pack(), sizeof (line.voice));
    line.duration = mgs_initiated() ? (float)msg_dur(state.message) : 0.0f;
}

static int
blocker_public(bp_blocker_t blocker)
{
    switch (blocker) {
    case BP_BLOCKER_AIRCRAFT_NOT_READY:
        return (BP_EXT_BLOCKER_AIRCRAFT_NOT_READY);
    case BP_BLOCKER_SET_PARKING_BRAKE:
        return (BP_EXT_BLOCKER_SET_PARKING_BRAKE);
    case BP_BLOCKER_RELEASE_PARKING_BRAKE:
        return (BP_EXT_BLOCKER_RELEASE_PARKING_BRAKE);
    case BP_BLOCKER_PLAN_REQUIRED:
        return (BP_EXT_BLOCKER_PLAN_REQUIRED);
    case BP_BLOCKER_NONE:
    default:
        return (BP_EXT_BLOCKER_NONE);
    }
}

static void
refresh(void)
{
    ground_ops_raw_state_t raw;
    bp_ext_state_t next;
    bp_ext_blocker_in_t blocker;

    if (!ground_ops_ui_collect_state(&raw)) {
        memset(&raw, 0, sizeof (raw));
        raw.step = PB_STEP_OFF;
    }
    (void)ground_ops_state_update(&snapshot_cache, &raw);
    blocker.blocker = blocker_public(bp_current_blocker());
    blocker.item = bp_blocker_item();
    blocker.status = bp_status_text();
    bp_ext_state_from(&raw, ground_ops_state_get(&snapshot_cache), &blocker,
        &published, &next);
    published = next;
    msg_ext_voice_poll();
    voice_external = msg_ext_voice_active() ? 1 : 0;
    refresh_line();
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
    memset(&line, 0, sizeof (line));
    line_source_seq = 0;
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
    dr_create_i(&blocker_dr, &published.blocker, B_FALSE, "bp/blocker");
    dr_create_b(&blocker_name_dr, published.blocker_name,
        sizeof (published.blocker_name), B_FALSE, "bp/blocker_name");
    dr_create_b(&blocker_item_dr, published.blocker_item,
        sizeof (published.blocker_item), B_FALSE, "bp/blocker_item");
    dr_create_b(&blocker_item_kind_dr, published.blocker_item_kind,
        sizeof (published.blocker_item_kind), B_FALSE,
        "bp/blocker_item_kind");
    dr_create_b(&status_text_dr, published.status, sizeof (published.status),
        B_FALSE, "bp/status_text");

    dr_create_i(&msg_seq_dr, &line.seq, B_FALSE, "bp/msg_seq");
    dr_create_i(&msg_dr, &line.msg, B_FALSE, "bp/msg");
    dr_create_b(&msg_key_dr, line.key, sizeof (line.key), B_FALSE,
        "bp/msg_key");
    dr_create_b(&msg_text_dr, line.text, sizeof (line.text), B_FALSE,
        "bp/msg_text");
    dr_create_b(&msg_caption_dr, line.caption, sizeof (line.caption),
        B_FALSE, "bp/msg_caption");
    dr_create_b(&msg_voice_dr, line.voice, sizeof (line.voice), B_FALSE,
        "bp/msg_voice");
    dr_create_f(&msg_duration_dr, &line.duration, B_FALSE,
        "bp/msg_duration");
    dr_create_i(&msg_playing_dr, &line.playing, B_FALSE, "bp/msg_playing");

    dr_create_i(&voice_mode_dr, &msg_ext_voice_mode, B_TRUE,
        "bp/voice_mode");
    dr_create_i(&voice_done_seq_dr, &msg_ext_voice_done_seq, B_TRUE,
        "bp/voice_done_seq");
    dr_create_i(&voice_heartbeat_dr, &msg_ext_voice_heartbeat, B_TRUE,
        "bp/voice_heartbeat");
    dr_create_i(&voice_external_dr, &voice_external, B_FALSE,
        "bp/voice_external");

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
    dr_delete(&blocker_dr);
    dr_delete(&blocker_name_dr);
    dr_delete(&blocker_item_dr);
    dr_delete(&blocker_item_kind_dr);
    dr_delete(&status_text_dr);
    dr_delete(&msg_seq_dr);
    dr_delete(&msg_dr);
    dr_delete(&msg_key_dr);
    dr_delete(&msg_text_dr);
    dr_delete(&msg_caption_dr);
    dr_delete(&msg_voice_dr);
    dr_delete(&msg_duration_dr);
    dr_delete(&msg_playing_dr);
    dr_delete(&voice_mode_dr);
    dr_delete(&voice_done_seq_dr);
    dr_delete(&voice_heartbeat_dr);
    dr_delete(&voice_external_dr);
    inited = B_FALSE;
}
