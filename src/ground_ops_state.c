/*
 * Pure read-only Ground Operations state mapper. This file deliberately has
 * no XPLM, route, dataref, audio, networking, or allocation dependencies.
 */

#include <stdio.h>
#include <string.h>

#include <acfutils/intl.h>
#include "ground_ops_state.h"

static const char *const stage_names[GROUND_OPS_STAGE_COUNT] = {
    "Tug", "Connect", "Comms", "Push", "Clear"
};

static const char *const step_names[PB_STEP_COUNT] = {
    "off",
    "tug_load",
    "start",
    "driving_up_close",
    "waiting_for_doors",
    "opening_cradle",
    "waiting_for_parking_brake",
    "driving_up_connect",
    "grabbing",
    "lifting",
    "connected",
    "starting",
    "pushing",
    "stopping",
    "stopped",
    "lowering",
    "ungrabbing",
    "waiting_to_disconnect",
    "moving_away",
    "closing_cradle",
    "starting_to_clear",
    "moving_to_clear",
    "clear_signal",
    "driving_away"
};

static const char *const prep_names[] = {
    "airport_data",
    "planner_review",
    "complete"
};

static const char *const caption_text[GROUND_OPS_CAPTION_COUNT] = {
    "",
    "Planner opened. Define the route.",
    "Pushback plan saved.",
    "Ground crew: Tug approaching.",
    "Ground crew: Set parking brake.",
    "Ground crew: Ready to connect.",
    "Ground crew: Winching nose gear.",
    "Ground crew: Tug connected.",
    "Ground crew: Starting pushback.",
    "Ground crew: Starting tow.",
    "Ground crew: Start engines later.",
    "Ground crew: Tow; start later.",
    "Ground crew: Pushback complete.",
    "Ground crew: Disconnecting tug.",
    "Ground crew: Clear on the right.",
    "Ground crew: Clear on the left."
};

static void
copy_text(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0)
        return;
    (void)snprintf(destination, capacity, "%s",
        source != NULL ? source : "");
}

const char *
ground_ops_stage_name(ground_ops_stage_t stage)
{
    if (stage < GROUND_OPS_STAGE_TUG || stage > GROUND_OPS_STAGE_CLEAR)
        return _("unknown");
    return (stage_names[stage]);
}

const char *
ground_ops_step_name(pushback_step_t step)
{
    if (step < PB_STEP_OFF || step >= PB_STEP_COUNT)
        return _("unknown");
    return (step_names[step]);
}

const char *
ground_ops_prep_name(ground_ops_prep_state_t prep)
{
    if (prep < GROUND_OPS_PREP_AIRPORT_DATA ||
        prep > GROUND_OPS_PREP_COMPLETE)
        return _("unknown");
    return (prep_names[prep]);
}

static bool
raw_equal(const ground_ops_raw_state_t *left,
    const ground_ops_raw_state_t *right)
{
    return (left->operation_active == right->operation_active &&
        left->emergency_tow == right->emergency_tow &&
        left->step == right->step &&
        left->prep_state == right->prep_state &&
        left->prep_state_active == right->prep_state_active &&
        left->tug_staged == right->tug_staged &&
        left->late_plan == right->late_plan &&
        left->plan_complete == right->plan_complete &&
        left->planner_open == right->planner_open &&
        left->awaiting_plan == right->awaiting_plan &&
        left->replan_available == right->replan_available &&
        left->pause_requested == right->pause_requested &&
        left->pause_held == right->pause_held &&
        left->clear_signal_displayed == right->clear_signal_displayed &&
        left->clear_signal_acknowledged == right->clear_signal_acknowledged &&
        left->disconnect_approved == right->disconnect_approved &&
        strcmp(left->airport_ident, right->airport_ident) == 0 &&
        strcmp(left->flight, right->flight) == 0 &&
        strcmp(left->schedule, right->schedule) == 0 &&
        strcmp(left->weather, right->weather) == 0 &&
        strcmp(left->pressure, right->pressure) == 0 &&
        strcmp(left->advisory, right->advisory) == 0 &&
        strcmp(left->data_source, right->data_source) == 0 &&
        left->captions_enabled == right->captions_enabled &&
        left->caption_active == right->caption_active &&
        left->caption == right->caption &&
        left->caption_sequence == right->caption_sequence &&
        left->speed_valid == right->speed_valid &&
        left->speed_tenths_mps == right->speed_tenths_mps &&
        left->distance_valid == right->distance_valid &&
        left->distance_m == right->distance_m);
}

ground_ops_stage_visual_t
ground_ops_stage_visual(const ground_ops_snapshot_t *snapshot,
    ground_ops_stage_t stage)
{
    if (snapshot == NULL || stage < GROUND_OPS_STAGE_TUG ||
        stage > GROUND_OPS_STAGE_CLEAR) {
        return (GROUND_OPS_STAGE_VISUAL_FUTURE);
    }

    switch (snapshot->stages[stage]) {
    case GROUND_OPS_STAGE_COMPLETE:
        return (GROUND_OPS_STAGE_VISUAL_COMPLETE);
    case GROUND_OPS_STAGE_CURRENT:
        return (snapshot->action_required ?
            GROUND_OPS_STAGE_VISUAL_BLOCKED :
            GROUND_OPS_STAGE_VISUAL_ACTIVE);
    case GROUND_OPS_STAGE_FUTURE:
    default:
        return (GROUND_OPS_STAGE_VISUAL_FUTURE);
    }
}

ground_ops_button_style_t
ground_ops_button_style(const ground_ops_snapshot_t *snapshot,
    ground_ops_action_t action)
{
    if (action == GROUND_OPS_ACTION_END_DISCONNECT)
        return (GROUND_OPS_BUTTON_DESTRUCTIVE);
    if (snapshot == NULL || !snapshot->action_required ||
        action != snapshot->primary_action)
        return (GROUND_OPS_BUTTON_SECONDARY);

    /* Only the action that advances the current task is required. For
     * example, Change plan is optional while brake release is required. */
    switch (action) {
    case GROUND_OPS_ACTION_CALL_TUG:
    case GROUND_OPS_ACTION_OPEN_PLANNER:
    case GROUND_OPS_ACTION_RESUME:
    case GROUND_OPS_ACTION_DISCONNECT_TUG:
    case GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR:
        return (GROUND_OPS_BUTTON_REQUIRED);
    default:
        return (GROUND_OPS_BUTTON_SECONDARY);
    }
}

static bool
semantic_equal(const ground_ops_raw_state_t *left,
    const ground_ops_raw_state_t *right)
{
    return (left->operation_active == right->operation_active &&
        left->emergency_tow == right->emergency_tow &&
        left->step == right->step &&
        left->prep_state == right->prep_state &&
        left->prep_state_active == right->prep_state_active &&
        left->tug_staged == right->tug_staged &&
        left->late_plan == right->late_plan &&
        left->plan_complete == right->plan_complete &&
        left->planner_open == right->planner_open &&
        left->awaiting_plan == right->awaiting_plan &&
        left->replan_available == right->replan_available &&
        left->pause_requested == right->pause_requested &&
        left->pause_held == right->pause_held &&
        left->clear_signal_displayed == right->clear_signal_displayed &&
        left->clear_signal_acknowledged == right->clear_signal_acknowledged &&
        left->disconnect_approved == right->disconnect_approved &&
        strcmp(left->airport_ident, right->airport_ident) == 0 &&
        left->captions_enabled == right->captions_enabled &&
        left->caption_active == right->caption_active &&
        left->caption == right->caption &&
        left->caption_sequence == right->caption_sequence);
}

static ground_ops_raw_state_t
normalize_raw(const ground_ops_raw_state_t *input)
{
    ground_ops_raw_state_t raw = *input;

    if (!raw.operation_active) {
        raw.step = PB_STEP_OFF;
        raw.prep_state_active = true;
        raw.awaiting_plan = false;
        raw.replan_available = false;
        raw.pause_requested = false;
        raw.pause_held = false;
    }
    if (raw.step < PB_STEP_OFF || raw.step >= PB_STEP_COUNT)
        raw.step = PB_STEP_OFF;
    if (raw.step != PB_STEP_CLEAR_SIGNAL) {
        raw.clear_signal_displayed = false;
        raw.clear_signal_acknowledged = false;
    }
    if (raw.step != PB_STEP_WAITING4OK2DISCO)
        raw.disconnect_approved = false;
    if (raw.prep_state < GROUND_OPS_PREP_AIRPORT_DATA ||
        raw.prep_state > GROUND_OPS_PREP_COMPLETE)
        raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    raw.airport_ident[sizeof(raw.airport_ident) - 1] = '\0';
    raw.flight[sizeof(raw.flight) - 1] = '\0';
    raw.schedule[sizeof(raw.schedule) - 1] = '\0';
    raw.weather[sizeof(raw.weather) - 1] = '\0';
    raw.pressure[sizeof(raw.pressure) - 1] = '\0';
    raw.advisory[sizeof(raw.advisory) - 1] = '\0';
    raw.data_source[sizeof(raw.data_source) - 1] = '\0';
    if (!raw.captions_enabled || !raw.caption_active ||
        raw.caption <= GROUND_OPS_CAPTION_NONE ||
        raw.caption >= GROUND_OPS_CAPTION_COUNT) {
        raw.caption_active = false;
        raw.caption = GROUND_OPS_CAPTION_NONE;
        raw.caption_sequence = 0;
    }
    if (!raw.speed_valid)
        raw.speed_tenths_mps = 0;
    if (!raw.distance_valid)
        raw.distance_m = 0;
    if (raw.distance_m < 0)
        raw.distance_m = 0;
    if (!raw.pause_requested)
        raw.pause_held = false;
    return (raw);
}

static void
set_view(ground_ops_snapshot_t *snapshot, ground_ops_stage_t stage,
    const char *eyebrow, const char *status, const char *detail,
    const char *task, bool action_required)
{
    snapshot->stage = stage;
    snapshot->action_required = action_required;
    copy_text(snapshot->stage_name, sizeof(snapshot->stage_name),
        ground_ops_stage_name(stage));
    copy_text(snapshot->eyebrow, sizeof(snapshot->eyebrow), eyebrow);
    copy_text(snapshot->status, sizeof(snapshot->status), status);
    copy_text(snapshot->detail, sizeof(snapshot->detail), detail);
    copy_text(snapshot->current_task, sizeof(snapshot->current_task), task);
}

static void
set_actions(ground_ops_snapshot_t *snapshot,
    ground_ops_action_t primary, const char *primary_label,
    ground_ops_action_t secondary, const char *secondary_label)
{
    snapshot->primary_action = primary;
    snapshot->secondary_action = secondary;
    copy_text(snapshot->primary_action_label,
        sizeof(snapshot->primary_action_label), primary_label);
    copy_text(snapshot->secondary_action_label,
        sizeof(snapshot->secondary_action_label), secondary_label);
}

static void
map_prep(const ground_ops_raw_state_t *raw,
    ground_ops_snapshot_t *snapshot)
{
    switch (raw->prep_state) {
    case GROUND_OPS_PREP_PLANNER_REVIEW:
        set_view(snapshot, GROUND_OPS_STAGE_COMMS, "REVIEW",
            _("Reviewing pushback plan"),
            _("The overhead planner is authoritative"),
            _("Complete or cancel the route"), true);
        break;
    case GROUND_OPS_PREP_COMPLETE:
        snapshot->operation_complete = true;
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "COMPLETE",
            _("Ground operation complete"),
            _("Aircraft and equipment are clear"),
            _("Call tow assistance if the\naircraft must return"), false);
        set_actions(snapshot, GROUND_OPS_ACTION_CALL_EMERGENCY_TOW,
            _("Call tow back"), GROUND_OPS_ACTION_NONE, "");
        break;
    case GROUND_OPS_PREP_AIRPORT_DATA:
    default:
        set_view(snapshot, GROUND_OPS_STAGE_TUG, "PILOT ACTION",
            _("Tug available"),
            raw->airport_ident[0] != '\0' ?
            _("Airport data loaded; ground crew is ready") :
            _("Ground crew is ready; airport data unavailable"),
            _("Call the tug to begin connection"), true);
        set_actions(snapshot, GROUND_OPS_ACTION_CALL_TUG, _("Call tug"),
            GROUND_OPS_ACTION_NONE, "");
        break;
    }
}

static void
map_step(const ground_ops_raw_state_t *raw,
    ground_ops_snapshot_t *snapshot)
{
    switch (raw->step) {
    case PB_STEP_TUG_LOAD:
        set_view(snapshot, GROUND_OPS_STAGE_TUG, "ACTIVE",
            _("Selecting a compatible tug"), _("Using the local tug library"),
            _("Wait for tug selection"), false);
        break;
    case PB_STEP_START:
        if (raw->tug_staged) {
            set_view(snapshot, GROUND_OPS_STAGE_TUG, "WAITING",
                _("Tug standing by"), _("The tug is ready to approach"),
                _("Ground crew will begin the approach"), false);
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_TUG, "ACTIVE",
                _("Ground crew dispatching tug"), _("Ground operation has started"),
                _("Wait for the tug"), false);
        }
        break;
    case PB_STEP_DRIVING_UP_CLOSE:
        set_view(snapshot, GROUND_OPS_STAGE_TUG, "ACTIVE",
            _("Tug approaching aircraft"), _("Maintaining safe approach speed"),
            _("Monitor tug approach"), false);
        break;
    case PB_STEP_WAITING_FOR_DOORS:
        if (raw->late_plan) {
            set_view(snapshot, GROUND_OPS_STAGE_TUG, "GROUND CREW",
                _("Clearing aircraft services"),
                _("Doors, GPU, or ASU are not yet clear"),
                _("Ground crew connection checks"), false);
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_TUG, "WAITING",
                _("Waiting for aircraft service"),
                _("Doors, GPU, or ASU are not clear"),
                _("Secure doors and equipment"), true);
        }
        break;
    case PB_STEP_OPENING_CRADLE:
        set_view(snapshot, GROUND_OPS_STAGE_TUG, "ACTIVE",
            _("Preparing the tug cradle"), _("Connection equipment is opening"),
            _("Wait for equipment preparation"), false);
        break;
    case PB_STEP_WAITING_FOR_PBRAKE:
        if (raw->late_plan) {
            set_view(snapshot, GROUND_OPS_STAGE_CONNECT, "GROUND CREW",
                _("Performing connection checks"),
                _("Aircraft is being secured for tug connection"),
                _("Wait for automatic connection"), false);
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_CONNECT, "WAITING",
                _("Parking brake required"), _("The tug is ready to connect"),
                _("Set the parking brake"), true);
        }
        break;
    case PB_STEP_DRIVING_UP_CONNECT:
        set_view(snapshot, GROUND_OPS_STAGE_CONNECT, "ACTIVE",
            _("Positioning tug to connect"), _("Closing the final safe distance"),
            _("Monitor tug connection"), false);
        break;
    case PB_STEP_GRABBING:
        set_view(snapshot, GROUND_OPS_STAGE_CONNECT, "ACTIVE",
            _("Securing the nose gear"), _("Tug connection is in progress"),
            _("Wait for nose-gear capture"), false);
        break;
    case PB_STEP_LIFTING:
        if (raw->awaiting_plan) {
            set_view(snapshot, GROUND_OPS_STAGE_COMMS, "PILOT ACTION",
                raw->emergency_tow ? _("Tug connected; plan the tow") :
                _("Tug connected; plan the push"),
                raw->emergency_tow ?
                _("Nose gear captured; saved routes are disabled") :
                _("Nose gear captured; no lift has been performed"),
                raw->emergency_tow ? _("Plan a one-time tow route") :
                _("Open the planner and accept a route"), true);
            set_actions(snapshot, GROUND_OPS_ACTION_OPEN_PLANNER,
                raw->emergency_tow ? _("Plan tow") : _("Plan push"),
                GROUND_OPS_ACTION_END_DISCONNECT,
                _("End operation"));
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_CONNECT, "ACTIVE",
                _("Lifting the nose gear"), _("Connection is nearly complete"),
                _("Wait for lift completion"), false);
        }
        break;
    case PB_STEP_CONNECTED:
        if (raw->late_plan && !raw->plan_complete) {
            set_view(snapshot, GROUND_OPS_STAGE_COMMS, "ACTION",
                raw->emergency_tow ? _("Tug connected; tow plan required") :
                _("Tug connected; plan required"),
                raw->emergency_tow ?
                _("Emergency Tow uses one-time manual routes only") :
                _("Late-planning mode is active"),
                _("Open and complete the planner"), true);
            set_actions(snapshot, GROUND_OPS_ACTION_OPEN_PLANNER,
                raw->emergency_tow ? _("Plan tow") : _("Plan push"),
                GROUND_OPS_ACTION_END_DISCONNECT,
                _("End operation"));
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_COMMS, "ACTION",
                _("Ready for brake release"), _("Tug connected and crew ready"),
                _("Release the parking brake"), true);
            if (raw->replan_available) {
                set_actions(snapshot, GROUND_OPS_ACTION_CHANGE_PLAN,
                    _("Change plan"), GROUND_OPS_ACTION_NONE, "");
            }
        }
        break;
    case PB_STEP_STARTING:
        set_view(snapshot, GROUND_OPS_STAGE_PUSH, "ACTIVE",
            raw->emergency_tow ? _("Starting emergency tow") :
            _("Starting pushback"), _("Steering and motion are engaging"),
            _("Monitor initial movement"), false);
        break;
    case PB_STEP_PUSHING:
        if (raw->pause_requested) {
            set_view(snapshot, GROUND_OPS_STAGE_PUSH, "HOLD",
                raw->pause_held ?
                (raw->emergency_tow ? _("Emergency tow paused") :
                _("Pushback paused")) :
                (raw->emergency_tow ? _("Pausing emergency tow") :
                _("Pausing pushback")),
                _("Accepted route and steering state are retained"),
                raw->pause_held ?
                _("Release parking brake if set, then resume") :
                _("Controlled deceleration in progress"), raw->pause_held);
            set_actions(snapshot, GROUND_OPS_ACTION_RESUME,
                raw->emergency_tow ? _("Resume tow") : _("Resume push"),
                GROUND_OPS_ACTION_END_DISCONNECT, _("End operation"));
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_PUSH, "ACTIVE",
                raw->emergency_tow ? _("Emergency tow in progress") :
                _("Pushback in progress"),
                _("Pause retains route; End disconnects"),
                raw->emergency_tow ? _("Monitor tow progress") :
                _("Monitor pushback progress"), false);
            set_actions(snapshot, GROUND_OPS_ACTION_PAUSE,
                raw->emergency_tow ? _("Pause tow") : _("Pause push"),
                GROUND_OPS_ACTION_END_DISCONNECT, _("End operation"));
        }
        break;
    case PB_STEP_STOPPING:
        set_view(snapshot, GROUND_OPS_STAGE_PUSH, "ACTIVE",
            raw->emergency_tow ? _("Stopping emergency tow") :
            _("Stopping the aircraft"), _("Decelerating to a controlled stop"),
            _("Wait for the aircraft to stop"), false);
        break;
    case PB_STEP_STOPPED:
        set_view(snapshot, GROUND_OPS_STAGE_PUSH, "ACTION",
            _("Aircraft stopped"), _("Parking brake confirmation is required"),
            _("Set the parking brake"), true);
        break;
    case PB_STEP_LOWERING:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Lowering the nose gear"), _("Beginning tug disconnection"),
            _("Wait for nose-gear lowering"), false);
        break;
    case PB_STEP_UNGRABBING:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Releasing the nose gear"), _("Connection equipment is opening"),
            _("Wait for nose-gear release"), false);
        break;
    case PB_STEP_WAITING4OK2DISCO:
        if (raw->disconnect_approved) {
            set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "CONFIRMED",
                _("Disconnect confirmed"), _("Ground crew is preparing to move"),
                _("Wait for tug clearance"), false);
            break;
        }
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "READY",
            _("Ready to disconnect"), _("Ground crew is awaiting approval"),
            _("Cleared to disconnect"), true);
        set_actions(snapshot, GROUND_OPS_ACTION_DISCONNECT_TUG,
            _("Disconnect tug"), GROUND_OPS_ACTION_RECONNECT_TUG,
            _("Reconnect"));
        break;
    case PB_STEP_MOVING_AWAY:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Tug moving clear"), _("Separating safely from the aircraft"),
            _("Monitor tug clearance"), false);
        break;
    case PB_STEP_CLOSING_CRADLE:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Closing the tug cradle"), _("Stowing connection equipment"),
            _("Wait for equipment stowage"), false);
        break;
    case PB_STEP_STARTING2CLEAR:
    case PB_STEP_MOVING2CLEAR:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Driver moving to clear"), _("Ground crew is taking position"),
            _("Monitor ground crew"), false);
        break;
    case PB_STEP_CLEAR_SIGNAL:
        if (raw->clear_signal_acknowledged) {
            set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "CONFIRMED",
                _("Clear signal acknowledged"), _("Ground crew will depart shortly"),
                _("Wait for tug departure"), false);
        } else {
            set_view(snapshot, GROUND_OPS_STAGE_CLEAR,
                raw->clear_signal_displayed ? "PILOT ACTION" : "CLEAR",
                _("Clear signal displayed"), _("Pin and equipment are clear"),
                _("Verify the clear signal"), raw->clear_signal_displayed);
            if (raw->clear_signal_displayed) {
                set_actions(snapshot, GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR,
                    _("Acknowledge"), GROUND_OPS_ACTION_NONE, "");
            }
        }
        break;
    case PB_STEP_DRIVING_AWAY:
        set_view(snapshot, GROUND_OPS_STAGE_CLEAR, "ACTIVE",
            _("Tug returning to station"), _("Aircraft area is being cleared"),
            _("Finalize cockpit checklist"), false);
        break;
    case PB_STEP_OFF:
    default:
        set_view(snapshot, GROUND_OPS_STAGE_TUG, "STANDBY",
            _("Controller idle"), _("No active pushback controller state"),
            _("Use classic controls to begin"), false);
        break;
    }
}

static void
build_snapshot(const ground_ops_raw_state_t *raw,
    ground_ops_snapshot_t *snapshot, uint64_t revision,
    uint64_t transition_sequence)
{
    int current_stage;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->revision = revision;
    snapshot->transition_sequence = transition_sequence;
    snapshot->operation_active = raw->operation_active;
    snapshot->emergency_tow = raw->emergency_tow;
    snapshot->prep_state_active = raw->prep_state_active;
    snapshot->controller_step = raw->step;
    snapshot->prep_state = raw->prep_state;

    if (raw->prep_state_active)
        map_prep(raw, snapshot);
    else
        map_step(raw, snapshot);

    current_stage = (int)snapshot->stage;
    for (int index = 0; index < GROUND_OPS_STAGE_COUNT; index++) {
        if (snapshot->operation_complete || index < current_stage)
            snapshot->stages[index] = GROUND_OPS_STAGE_COMPLETE;
        else if (index == current_stage)
            snapshot->stages[index] = GROUND_OPS_STAGE_CURRENT;
        else
            snapshot->stages[index] = GROUND_OPS_STAGE_FUTURE;
    }

    if (raw->speed_valid) {
        (void)snprintf(snapshot->speed, sizeof(snapshot->speed),
            _("Speed %.1f m/s"), raw->speed_tenths_mps / 10.0);
    } else {
        copy_text(snapshot->speed, sizeof(snapshot->speed), _("Speed --"));
    }
    if (raw->distance_valid) {
        (void)snprintf(snapshot->distance, sizeof(snapshot->distance),
            _("Remaining %d m"), raw->distance_m);
    } else {
        copy_text(snapshot->distance, sizeof(snapshot->distance),
            _("Remaining --"));
    }

    snapshot->caption_visible = raw->caption_active;
    if (raw->caption_active) {
        copy_text(snapshot->caption, sizeof(snapshot->caption),
            _(caption_text[raw->caption]));
    }
    copy_text(snapshot->flight, sizeof(snapshot->flight),
        raw->flight[0] != '\0' ? raw->flight : _("Flight --"));
    copy_text(snapshot->schedule, sizeof(snapshot->schedule),
        raw->schedule[0] != '\0' ? raw->schedule : _("EOBT --"));
    copy_text(snapshot->weather, sizeof(snapshot->weather),
        raw->weather[0] != '\0' ? raw->weather : _("SIM WX unavailable"));
    copy_text(snapshot->pressure, sizeof(snapshot->pressure),
        raw->pressure[0] != '\0' ? raw->pressure : _("QNH unavailable"));
    copy_text(snapshot->advisory, sizeof(snapshot->advisory),
        raw->advisory[0] != '\0' ? raw->advisory :
        _("METAR -- | ATIS --"));
    copy_text(snapshot->source, sizeof(snapshot->source),
        raw->data_source[0] != '\0' ? raw->data_source :
        _("Local plugin state"));
    if (raw->airport_ident[0] != '\0') {
        (void)snprintf(snapshot->airport, sizeof(snapshot->airport),
            _("Airport %s"), raw->airport_ident);
    } else {
        copy_text(snapshot->airport, sizeof(snapshot->airport),
            _("Airport ----"));
    }
    (void)snprintf(snapshot->hover, sizeof(snapshot->hover), "%s\n%s",
        snapshot->status, snapshot->action_required ?
        snapshot->current_task : _("No pilot action required"));
}

void
ground_ops_state_init(ground_ops_state_t *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

bool
ground_ops_state_update(ground_ops_state_t *state,
    const ground_ops_raw_state_t *input)
{
    ground_ops_raw_state_t raw;
    bool transition_changed;
    uint64_t revision, transition_sequence;

    if (state == NULL || input == NULL)
        return (false);
    raw = normalize_raw(input);
    if (state->initialized && raw_equal(&state->previous_raw, &raw))
        return (false);

    transition_changed = !state->initialized ||
        !semantic_equal(&state->previous_raw, &raw);
    revision = state->snapshot.revision + 1;
    transition_sequence = state->snapshot.transition_sequence +
        (transition_changed ? 1 : 0);
    build_snapshot(&raw, &state->snapshot, revision, transition_sequence);
    state->previous_raw = raw;
    state->initialized = true;
    return (true);
}

const ground_ops_snapshot_t *
ground_ops_state_get(const ground_ops_state_t *state)
{
    if (state == NULL || !state->initialized)
        return (NULL);
    return (&state->snapshot);
}
