/*
 * External interface: pure mapping from the Ground Operations snapshot to the
 * published operation state. No XPLM, dataref, audio or allocation
 * dependencies, so it is unit tested on its own (tests/ext_api_state_test.c).
 */

#include <stdio.h>
#include <string.h>

#include "ext_api_state.h"

/* pushback_step_t -> public step number (see bp_ext_step_t). */
static const int step_public[PB_STEP_COUNT] = {
    [PB_STEP_OFF] = BP_EXT_STEP_OFF,
    [PB_STEP_TUG_LOAD] = BP_EXT_STEP_TUG_LOAD,
    [PB_STEP_START] = BP_EXT_STEP_START,
    [PB_STEP_DRIVING_UP_CLOSE] = BP_EXT_STEP_DRIVING_UP_CLOSE,
    [PB_STEP_WAITING_FOR_DOORS] = BP_EXT_STEP_WAITING_FOR_DOORS,
    [PB_STEP_OPENING_CRADLE] = BP_EXT_STEP_OPENING_CRADLE,
    [PB_STEP_WAITING_FOR_PBRAKE] = BP_EXT_STEP_WAITING_FOR_PBRAKE,
    [PB_STEP_DRIVING_UP_CONNECT] = BP_EXT_STEP_DRIVING_UP_CONNECT,
    [PB_STEP_GRABBING] = BP_EXT_STEP_GRABBING,
    [PB_STEP_LIFTING] = BP_EXT_STEP_LIFTING,
    [PB_STEP_CONNECTED] = BP_EXT_STEP_CONNECTED,
    [PB_STEP_STARTING] = BP_EXT_STEP_STARTING,
    [PB_STEP_PUSHING] = BP_EXT_STEP_PUSHING,
    [PB_STEP_STOPPING] = BP_EXT_STEP_STOPPING,
    [PB_STEP_STOPPED] = BP_EXT_STEP_STOPPED,
    [PB_STEP_LOWERING] = BP_EXT_STEP_LOWERING,
    [PB_STEP_UNGRABBING] = BP_EXT_STEP_UNGRABBING,
    [PB_STEP_WAITING4OK2DISCO] = BP_EXT_STEP_WAITING_TO_DISCONNECT,
    [PB_STEP_MOVING_AWAY] = BP_EXT_STEP_MOVING_AWAY,
    [PB_STEP_CLOSING_CRADLE] = BP_EXT_STEP_CLOSING_CRADLE,
    [PB_STEP_STARTING2CLEAR] = BP_EXT_STEP_STARTING_TO_CLEAR,
    [PB_STEP_MOVING2CLEAR] = BP_EXT_STEP_MOVING_TO_CLEAR,
    [PB_STEP_CLEAR_SIGNAL] = BP_EXT_STEP_CLEAR_SIGNAL,
    [PB_STEP_DRIVING_AWAY] = BP_EXT_STEP_DRIVING_AWAY
};

/* Indexed by the public step number: machine names, never translated. */
static const char *const step_names[] = {
    "off", "tug_load", "start", "driving_up_close", "waiting_for_doors",
    "opening_cradle", "waiting_for_parking_brake", "driving_up_connect",
    "grabbing", "lifting", "connected", "starting", "pushing", "stopping",
    "stopped", "lowering", "ungrabbing", "waiting_to_disconnect",
    "moving_away", "closing_cradle", "starting_to_clear", "moving_to_clear",
    "clear_signal", "driving_away"
};

/* ground_ops_action_t -> public action number (see bp_ext_action_t). */
static const struct {
    ground_ops_action_t action;
    int public_action;
} action_public[] = {
    { GROUND_OPS_ACTION_NONE, BP_EXT_ACTION_NONE },
    { GROUND_OPS_ACTION_CALL_TUG, BP_EXT_ACTION_CALL_TUG },
    { GROUND_OPS_ACTION_CALL_EMERGENCY_TOW, BP_EXT_ACTION_CALL_EMERGENCY_TOW },
    { GROUND_OPS_ACTION_OPEN_PLANNER, BP_EXT_ACTION_OPEN_PLANNER },
    { GROUND_OPS_ACTION_CHANGE_PLAN, BP_EXT_ACTION_CHANGE_PLAN },
    { GROUND_OPS_ACTION_PAUSE, BP_EXT_ACTION_PAUSE },
    { GROUND_OPS_ACTION_RESUME, BP_EXT_ACTION_RESUME },
    { GROUND_OPS_ACTION_DISCONNECT_TUG, BP_EXT_ACTION_DISCONNECT_TUG },
    { GROUND_OPS_ACTION_RECONNECT_TUG, BP_EXT_ACTION_RECONNECT_TUG },
    { GROUND_OPS_ACTION_END_DISCONNECT, BP_EXT_ACTION_END_DISCONNECT },
    { GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR, BP_EXT_ACTION_ACKNOWLEDGE_CLEAR }
};

/* Indexed by the public action number. */
static const char *const action_names[] = {
    "none", "call_tug", "call_emergency_tow", "open_planner", "change_plan",
    "pause", "resume", "disconnect_tug", "reconnect_tug", "end_disconnect",
    "acknowledge_clear"
};

/* Indexed by ground_ops_stage_t: machine names, never translated. */
static const char *const stage_names[GROUND_OPS_STAGE_COUNT] = {
    "tug", "connect", "comms", "push", "clear"
};

#define ARRAY_LEN(a) (sizeof (a) / sizeof ((a)[0]))

int
bp_ext_step_public(pushback_step_t step)
{
    if ((int)step < 0 || (int)step >= PB_STEP_COUNT)
        return (BP_EXT_STEP_OFF);
    return (step_public[step]);
}

const char *
bp_ext_step_name(int public_step)
{
    if (public_step < 0 || public_step >= (int)ARRAY_LEN(step_names))
        return ("unknown");
    return (step_names[public_step]);
}

int
bp_ext_action_public(ground_ops_action_t action)
{
    for (size_t i = 0; i < ARRAY_LEN(action_public); i++) {
        if (action_public[i].action == action)
            return (action_public[i].public_action);
    }
    return (BP_EXT_ACTION_NONE);
}

const char *
bp_ext_action_name(int public_action)
{
    if (public_action < 0 || public_action >= (int)ARRAY_LEN(action_names))
        return ("unknown");
    return (action_names[public_action]);
}

const char *
bp_ext_stage_name(int stage)
{
    if (stage < 0 || stage >= GROUND_OPS_STAGE_COUNT)
        return ("idle");
    return (stage_names[stage]);
}

static bool
same_state(const bp_ext_state_t *a, const bp_ext_state_t *b)
{
    return (a->active == b->active && a->emergency_tow == b->emergency_tow &&
        a->step == b->step && a->stage == b->stage &&
        a->action == b->action && a->paused == b->paused);
}

void
bp_ext_state_from(const ground_ops_raw_state_t *raw,
    const ground_ops_snapshot_t *snapshot, const bp_ext_state_t *previous,
    bp_ext_state_t *out)
{
    bp_ext_state_t state;

    memset(&state, 0, sizeof (state));
    state.active = snapshot->operation_active ? 1 : 0;
    state.emergency_tow = (state.active && snapshot->emergency_tow) ? 1 : 0;
    state.step = state.active ?
        bp_ext_step_public(snapshot->controller_step) : BP_EXT_STEP_OFF;
    state.stage = state.active ? (int)snapshot->stage : -1;
    state.action = snapshot->action_required ?
        bp_ext_action_public(snapshot->primary_action) : BP_EXT_ACTION_NONE;
    state.paused = (state.active &&
        (raw->pause_requested || raw->pause_held)) ? 1 : 0;
    (void)snprintf(state.step_name, sizeof (state.step_name), "%s",
        bp_ext_step_name(state.step));
    (void)snprintf(state.stage_name, sizeof (state.stage_name), "%s",
        bp_ext_stage_name(state.stage));
    (void)snprintf(state.action_name, sizeof (state.action_name), "%s",
        bp_ext_action_name(state.action));

    if (previous == NULL)
        state.state_seq = 1;
    else if (same_state(previous, &state))
        state.state_seq = previous->state_seq;
    else
        state.state_seq = previous->state_seq + 1;
    *out = state;
}
