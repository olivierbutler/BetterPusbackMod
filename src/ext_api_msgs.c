/*
 * External interface: pure mapping from msg.c's crew lines to the published
 * line number, key, nominal English text and Ground Operations caption.
 * Unit tested on its own (tests/ext_api_msgs_test.c).
 */

#include <stddef.h>

#include "ext_api_msgs.h"

/*
 * Indexed by message_t. The text is the nominal line from
 * data/msgs/msg_text.txt; recordings may vary it slightly. The _NOPARK and
 * _NOSTART variants are the same recordings cut short (the parking brake is
 * already set; the engines cannot or should not be started now).
 */
static const struct {
    int public_msg;
    const char *key;
    const char *text;
    ground_ops_caption_t caption;
} lines[MSG_NUM_MSGS] = {
    [MSG_PLAN_START] = { BP_EXT_MSG_PLAN_START, "plan_start",
        "Ground to cockpit. Please show me where you want to go.",
        GROUND_OPS_CAPTION_PLAN_START },
    [MSG_PLAN_END] = { BP_EXT_MSG_PLAN_END, "plan_end",
        "Ground to cockpit. Plan acknowledged, call me through the menu "
        "when you are ready.", GROUND_OPS_CAPTION_PLAN_END },
    [MSG_DRIVING_UP] = { BP_EXT_MSG_DRIVING_UP, "driving_up",
        "Ground to cockpit. Tow is driving up.",
        GROUND_OPS_CAPTION_DRIVING_UP },
    [MSG_RDY2CONN] = { BP_EXT_MSG_READY_TO_CONNECT, "ready2conn",
        "Ok, all doors and hatches are closed, ready to connect. Set parking "
        "brake.", GROUND_OPS_CAPTION_READY_TO_CONNECT },
    [MSG_RDY2CONN_NOPARK] = { BP_EXT_MSG_READY_TO_CONNECT_BRAKE_SET,
        "ready2conn_nopark",
        "Ok, all doors and hatches are closed, ready to connect.",
        GROUND_OPS_CAPTION_READY_TO_CONNECT_NOPARK },
    [MSG_WINCH] = { BP_EXT_MSG_WINCH, "winch",
        "Winching strap and adapter in position. Release parking brake when "
        "ready to start pushback.", GROUND_OPS_CAPTION_WINCH },
    [MSG_CONNECTED] = { BP_EXT_MSG_CONNECTED, "connected",
        "Tow connected and bypass pin inserted. Release parking brake.",
        GROUND_OPS_CAPTION_CONNECTED },
    [MSG_START_PB] = { BP_EXT_MSG_START_PUSHBACK, "start_pb",
        "Starting pushback and you may start engines.",
        GROUND_OPS_CAPTION_START_PUSHBACK },
    [MSG_START_TOW] = { BP_EXT_MSG_START_TOW, "start_tow",
        "Starting tow and you may start engines.",
        GROUND_OPS_CAPTION_START_TOW },
    [MSG_START_PB_NOSTART] = { BP_EXT_MSG_START_PUSHBACK_NO_ENGINE_START,
        "start_pb_nostart", "Starting pushback.",
        GROUND_OPS_CAPTION_START_PUSHBACK_NOSTART },
    [MSG_START_TOW_NOSTART] = { BP_EXT_MSG_START_TOW_NO_ENGINE_START,
        "start_tow_nostart", "Starting tow.",
        GROUND_OPS_CAPTION_START_TOW_NOSTART },
    [MSG_OP_COMPLETE] = { BP_EXT_MSG_OPERATION_COMPLETE, "op_complete",
        "Operation complete, set parking brake.",
        GROUND_OPS_CAPTION_OPERATION_COMPLETE },
    [MSG_DISCO] = { BP_EXT_MSG_DISCONNECTING, "disco",
        "Disconnecting tow. Stand by.", GROUND_OPS_CAPTION_DISCONNECT },
    [MSG_DONE_RIGHT] = { BP_EXT_MSG_DONE_RIGHT, "done_right",
        "Tow is disconnected and bypass pin has been removed, hand signal on "
        "the right, we'll see you next time and have a safe flight.",
        GROUND_OPS_CAPTION_DONE_RIGHT },
    [MSG_DONE_LEFT] = { BP_EXT_MSG_DONE_LEFT, "done_left",
        "Tow is disconnected and bypass pin has been removed, hand signal on "
        "the left, we'll see you next time and have a safe flight.",
        GROUND_OPS_CAPTION_DONE_LEFT }
};

/*
 * Lines spoken through X-Plane's speech (msg_speak). The warnings and the
 * chocks lines have a fixed English text (bp.c says the chocks lines from
 * here); system messages vary, so they have none here.
 */
static const struct {
    msg_spoken_t kind;
    int public_msg;
    const char *key;
    const char *text;
} spoken[] = {
    { MSG_SPOKEN_DOORS_GPU, BP_EXT_MSG_DOORS_GPU_OPEN, "doors_gpu_open",
        "Some doors are still opened or the GPU or the ASU are still "
        "connected. I'm waiting for all of them closed and disconnected then "
        "I will proceed." },
    { MSG_SPOKEN_LIGHTS, BP_EXT_MSG_LIGHTS_WARNING, "lights_warning",
        "Hey! Quit blinding me with your lights! Turn them off!" },
    { MSG_SPOKEN_SYSTEM, BP_EXT_MSG_SYSTEM, "system", "" },
    { MSG_SPOKEN_CHOCKS_PB, BP_EXT_MSG_START_PUSHBACK_CHOCKS,
        "start_pb_chocks",
        "Removing chocks, and beginning pushback. You may start engines." },
    { MSG_SPOKEN_CHOCKS_TOW, BP_EXT_MSG_START_TOW_CHOCKS, "start_tow_chocks",
        "Removing chocks, and beginning the tow. You may start engines." },
    { MSG_SPOKEN_CHOCKS_PB_NOSTART,
        BP_EXT_MSG_START_PUSHBACK_CHOCKS_NO_ENGINE_START,
        "start_pb_chocks_nostart", "Removing chocks, and beginning pushback." },
    { MSG_SPOKEN_CHOCKS_TOW_NOSTART,
        BP_EXT_MSG_START_TOW_CHOCKS_NO_ENGINE_START,
        "start_tow_chocks_nostart", "Removing chocks, and beginning the tow." }
};

#define SPOKEN_COUNT ((int)(sizeof (spoken) / sizeof (spoken[0])))

static int
index_of(int public_msg)
{
    for (int m = 0; m < MSG_NUM_MSGS; m++) {
        if (lines[m].public_msg == public_msg)
            return (m);
    }
    return (-1);
}

static int
spoken_index_of(int public_msg)
{
    for (int s = 0; s < SPOKEN_COUNT; s++) {
        if (spoken[s].public_msg == public_msg)
            return (s);
    }
    return (-1);
}

int
bp_ext_msg_public(message_t msg)
{
    if ((int)msg < 0 || (int)msg >= MSG_NUM_MSGS)
        return (BP_EXT_MSG_NONE);
    return (lines[msg].public_msg);
}

int
bp_ext_msg_spoken_public(msg_spoken_t kind)
{
    for (int s = 0; s < SPOKEN_COUNT; s++) {
        if (spoken[s].kind == kind)
            return (spoken[s].public_msg);
    }
    return (BP_EXT_MSG_SYSTEM);
}

const char *
bp_ext_msg_key(int public_msg)
{
    int m = index_of(public_msg), s;

    if (m >= 0)
        return (lines[m].key);
    s = spoken_index_of(public_msg);
    return (s < 0 ? "" : spoken[s].key);
}

const char *
bp_ext_msg_text(int public_msg)
{
    int m = index_of(public_msg), s;

    if (m >= 0)
        return (lines[m].text);
    s = spoken_index_of(public_msg);
    return (s < 0 ? "" : spoken[s].text);
}

ground_ops_caption_t
bp_ext_msg_caption(int public_msg)
{
    int m = index_of(public_msg);
    return (m < 0 ? GROUND_OPS_CAPTION_NONE : lines[m].caption);
}
