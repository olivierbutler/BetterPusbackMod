#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_ops_state.h"
#include "clear_signal_gate.h"

static ground_ops_raw_state_t
idle_raw(void)
{
    ground_ops_raw_state_t raw = {0};

    raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    raw.captions_enabled = true;
    return (raw);
}

static ground_ops_snapshot_t
snapshot_for_step(pushback_step_t step)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = step;
    assert(ground_ops_state_update(&state, &raw));
    return (*ground_ops_state_get(&state));
}

static void
test_every_controller_step_maps(void)
{
    static const ground_ops_stage_t expected[PB_STEP_COUNT] = {
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_TUG,
        GROUND_OPS_STAGE_CONNECT,
        GROUND_OPS_STAGE_CONNECT,
        GROUND_OPS_STAGE_CONNECT,
        GROUND_OPS_STAGE_CONNECT,
        GROUND_OPS_STAGE_COMMS,
        GROUND_OPS_STAGE_PUSH,
        GROUND_OPS_STAGE_PUSH,
        GROUND_OPS_STAGE_PUSH,
        GROUND_OPS_STAGE_PUSH,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR,
        GROUND_OPS_STAGE_CLEAR
    };

    for (int step = PB_STEP_OFF; step < PB_STEP_COUNT; step++) {
        ground_ops_snapshot_t snapshot = snapshot_for_step(
            (pushback_step_t)step);

        assert(snapshot.stage == expected[step]);
        assert(snapshot.status[0] != '\0');
        assert(snapshot.detail[0] != '\0');
        assert(snapshot.current_task[0] != '\0');
        assert(strcmp(ground_ops_step_name((pushback_step_t)step),
            "unknown") != 0);
    }
}

static void
test_stage_progression(void)
{
    ground_ops_snapshot_t snapshot = snapshot_for_step(PB_STEP_CONNECTED);

    assert(snapshot.stages[GROUND_OPS_STAGE_TUG] ==
        GROUND_OPS_STAGE_COMPLETE);
    assert(snapshot.stages[GROUND_OPS_STAGE_CONNECT] ==
        GROUND_OPS_STAGE_COMPLETE);
    assert(snapshot.stages[GROUND_OPS_STAGE_COMMS] ==
        GROUND_OPS_STAGE_CURRENT);
    assert(snapshot.stages[GROUND_OPS_STAGE_PUSH] ==
        GROUND_OPS_STAGE_FUTURE);
    assert(snapshot.stages[GROUND_OPS_STAGE_CLEAR] ==
        GROUND_OPS_STAGE_FUTURE);
}

static void
test_action_markers(void)
{
    assert(snapshot_for_step(PB_STEP_WAITING_FOR_DOORS).action_required);
    assert(snapshot_for_step(PB_STEP_WAITING_FOR_PBRAKE).action_required);
    assert(snapshot_for_step(PB_STEP_CONNECTED).action_required);
    assert(snapshot_for_step(PB_STEP_STOPPED).action_required);
    assert(snapshot_for_step(PB_STEP_WAITING4OK2DISCO).action_required);
    assert(!snapshot_for_step(PB_STEP_PUSHING).action_required);
    assert(!snapshot_for_step(PB_STEP_MOVING_AWAY).action_required);
}

static void
test_disconnect_gate_exposes_both_pilot_choices(void)
{
    ground_ops_snapshot_t snapshot =
        snapshot_for_step(PB_STEP_WAITING4OK2DISCO);

    assert(snapshot.stage == GROUND_OPS_STAGE_CLEAR);
    assert(snapshot.action_required);
    assert(strcmp(snapshot.status, "Ready to disconnect") == 0);
    assert(strcmp(snapshot.detail,
        "Ground crew is awaiting approval") == 0);
    assert(strcmp(snapshot.current_task,
        "Cleared to disconnect") == 0);
    assert(snapshot.primary_action == GROUND_OPS_ACTION_DISCONNECT_TUG);
    assert(strcmp(snapshot.primary_action_label, "Disconnect tug") == 0);
    assert(snapshot.secondary_action == GROUND_OPS_ACTION_RECONNECT_TUG);
    assert(strcmp(snapshot.secondary_action_label, "Reconnect") == 0);
}

static void
test_prep_states_are_separate_from_controller(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();
    const ground_ops_snapshot_t *snapshot;

    ground_ops_state_init(&state);
    for (int prep = GROUND_OPS_PREP_AIRPORT_DATA;
        prep <= GROUND_OPS_PREP_COMPLETE; prep++) {
        raw.prep_state = (ground_ops_prep_state_t)prep;
        assert(ground_ops_state_update(&state, &raw));
        snapshot = ground_ops_state_get(&state);
        assert(snapshot->controller_step == PB_STEP_OFF);
        assert(snapshot->status[0] != '\0');
    }
    snapshot = ground_ops_state_get(&state);
    assert(snapshot->operation_complete);
    assert(!snapshot->action_required);
    assert(snapshot->primary_action ==
        GROUND_OPS_ACTION_CALL_EMERGENCY_TOW);
    assert(strcmp(snapshot->primary_action_label, "Call tow back") == 0);
    for (int index = 0; index < GROUND_OPS_STAGE_COUNT; index++)
        assert(snapshot->stages[index] == GROUND_OPS_STAGE_COMPLETE);

    raw.operation_active = true;
    raw.step = PB_STEP_START;
    raw.prep_state_active = true;
    raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    assert(ground_ops_state_update(&state, &raw));
    snapshot = ground_ops_state_get(&state);
    assert(snapshot->operation_active);
    assert(snapshot->prep_state_active);
    assert(snapshot->controller_step == PB_STEP_START);
    assert(strcmp(snapshot->status, "Tug available") == 0);
    assert(snapshot->primary_action == GROUND_OPS_ACTION_CALL_TUG);
}

static void
test_formatting_only_changes_for_display_values(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();
    uint64_t transition;

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = PB_STEP_PUSHING;
    raw.speed_valid = true;
    raw.speed_tenths_mps = 12;
    raw.distance_valid = true;
    raw.distance_m = 82;
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.speed, "Speed 1.2 m/s") == 0);
    assert(strcmp(state.snapshot.distance, "Remaining 82 m") == 0);
    transition = state.snapshot.transition_sequence;

    assert(!ground_ops_state_update(&state, &raw));
    raw.speed_tenths_mps = 13;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.transition_sequence == transition);
    assert(strcmp(state.snapshot.speed, "Speed 1.3 m/s") == 0);

    raw.step = PB_STEP_STOPPING;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.transition_sequence == transition + 1);
}

static void
test_captions_follow_message_state(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.caption_active = true;
    raw.caption = GROUND_OPS_CAPTION_CONNECTED;
    raw.caption_sequence = 7;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.caption_visible);
    assert(strcmp(state.snapshot.caption,
        "Ground crew: Tug connected.") == 0);

    raw.captions_enabled = false;
    assert(ground_ops_state_update(&state, &raw));
    assert(!state.snapshot.caption_visible);
    assert(state.snapshot.caption[0] == '\0');
}

static void
test_waiting_state_has_no_timer_progress(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();
    ground_ops_snapshot_t before;

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = PB_STEP_WAITING_FOR_PBRAKE;
    assert(ground_ops_state_update(&state, &raw));
    before = state.snapshot;
    assert(!ground_ops_state_update(&state, &raw));
    assert(memcmp(&before, &state.snapshot, sizeof(before)) == 0);
}

static void
test_stage_visuals_distinguish_blocking_from_automatic_progress(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.action_required);
    assert(ground_ops_stage_visual(&state.snapshot, GROUND_OPS_STAGE_TUG) ==
        GROUND_OPS_STAGE_VISUAL_BLOCKED);
    assert(ground_ops_stage_visual(&state.snapshot,
        GROUND_OPS_STAGE_CONNECT) == GROUND_OPS_STAGE_VISUAL_FUTURE);

    /* Call tug accepted: the current Tug stage is healthy automatic work. */
    raw.operation_active = true;
    raw.prep_state_active = false;
    raw.step = PB_STEP_START;
    assert(ground_ops_state_update(&state, &raw));
    assert(!state.snapshot.action_required);
    assert(ground_ops_stage_visual(&state.snapshot, GROUND_OPS_STAGE_TUG) ==
        GROUND_OPS_STAGE_VISUAL_ACTIVE);

    raw.step = PB_STEP_DRIVING_UP_CONNECT;
    assert(ground_ops_state_update(&state, &raw));
    assert(ground_ops_stage_visual(&state.snapshot, GROUND_OPS_STAGE_TUG) ==
        GROUND_OPS_STAGE_VISUAL_COMPLETE);
    assert(ground_ops_stage_visual(&state.snapshot,
        GROUND_OPS_STAGE_CONNECT) == GROUND_OPS_STAGE_VISUAL_ACTIVE);
    assert(ground_ops_stage_visual(&state.snapshot, GROUND_OPS_STAGE_COMMS) ==
        GROUND_OPS_STAGE_VISUAL_FUTURE);

    raw.step = PB_STEP_LIFTING;
    raw.late_plan = true;
    raw.awaiting_plan = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.action_required);
    assert(ground_ops_stage_visual(&state.snapshot, GROUND_OPS_STAGE_COMMS) ==
        GROUND_OPS_STAGE_VISUAL_BLOCKED);

    assert(ground_ops_stage_visual(NULL, GROUND_OPS_STAGE_TUG) ==
        GROUND_OPS_STAGE_VISUAL_FUTURE);
}

static void
test_pre_lift_connection_hold_exposes_plan_action(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();
    const ground_ops_snapshot_t *snapshot;

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = PB_STEP_LIFTING;
    raw.late_plan = true;
    raw.awaiting_plan = true;
    assert(ground_ops_state_update(&state, &raw));
    snapshot = ground_ops_state_get(&state);
    assert(snapshot->stage == GROUND_OPS_STAGE_COMMS);
    assert(snapshot->stages[GROUND_OPS_STAGE_CONNECT] ==
        GROUND_OPS_STAGE_COMPLETE);
    assert(snapshot->action_required);
    assert(snapshot->primary_action == GROUND_OPS_ACTION_OPEN_PLANNER);
    assert(snapshot->secondary_action ==
        GROUND_OPS_ACTION_END_DISCONNECT);
    assert(strcmp(snapshot->status, "Tug connected; plan the push") == 0);
    assert(strstr(snapshot->detail, "no lift") != NULL);
}

static void
test_pause_resume_actions_preserve_push_stage(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = PB_STEP_PUSHING;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_PAUSE);
    assert(state.snapshot.secondary_action ==
        GROUND_OPS_ACTION_END_DISCONNECT);
    assert(!state.snapshot.action_required);

    raw.pause_requested = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.stage == GROUND_OPS_STAGE_PUSH);
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_RESUME);
    assert(strcmp(state.snapshot.status, "Pausing pushback") == 0);
    assert(!state.snapshot.action_required);

    raw.pause_held = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.status, "Pushback paused") == 0);
    assert(state.snapshot.action_required);
    assert(strcmp(state.snapshot.detail,
        "Accepted route and steering state are retained") == 0);
    assert(strstr(state.snapshot.current_task, "parking brake") != NULL);

    raw.pause_requested = false;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_PAUSE);
    assert(strcmp(state.snapshot.status, "Pushback in progress") == 0);
}

static void
test_end_from_pause_hold_uses_stationary_handoff(void)
{
    assert(pushback_stop_is_stationary_handoff(PB_STEP_PUSHING, true, true));
    assert(!pushback_stop_is_stationary_handoff(PB_STEP_PUSHING, true,
        false));
    assert(!pushback_stop_is_stationary_handoff(PB_STEP_PUSHING, false,
        true));
    assert(!pushback_stop_is_stationary_handoff(PB_STEP_STOPPING, true,
        true));
}

static void
test_idle_workflow_requires_pilot_tug_call(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    (void)strcpy(raw.airport_ident, "KDEN");
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_CALL_TUG);
    assert(state.snapshot.action_required);
    assert(strcmp(state.snapshot.status, "Tug available") == 0);
    assert(strcmp(state.snapshot.airport, "Airport KDEN") == 0);
    assert(strstr(state.snapshot.current_task, "Call the tug") != NULL);
}

static void
test_completed_task_has_bounded_two_line_copy(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.prep_state = GROUND_OPS_PREP_COMPLETE;
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.current_task,
        "Call tow assistance if the\naircraft must return") == 0);
}

static void
test_provider_context_is_display_only(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();
    uint64_t transition;

    ground_ops_state_init(&state);
    (void)strcpy(raw.flight, "Flight KLM511");
    (void)strcpy(raw.schedule, "EOBT --");
    (void)strcpy(raw.weather, "SIM 275/10KT 18C");
    (void)strcpy(raw.pressure, "QNH 1013 hPa / 29.92 inHg");
    (void)strcpy(raw.advisory, "METAR -- | ATIS --");
    (void)strcpy(raw.data_source, "Simulator | Current");
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.flight, "Flight KLM511") == 0);
    assert(strcmp(state.snapshot.weather,
        "SIM 275/10KT 18C") == 0);
    assert(strcmp(state.snapshot.pressure,
        "QNH 1013 hPa / 29.92 inHg") == 0);
    assert(strcmp(state.snapshot.source, "Simulator | Current") == 0);
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_CALL_TUG);
    transition = state.snapshot.transition_sequence;

    (void)strcpy(raw.weather, "SIM 280/11KT 18C");
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.transition_sequence == transition);
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_CALL_TUG);
}

static void
test_called_connection_has_no_second_gate_before_capture(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.late_plan = true;
    raw.step = PB_STEP_WAITING_FOR_DOORS;
    assert(ground_ops_state_update(&state, &raw));
    assert(!state.snapshot.action_required);

    raw.step = PB_STEP_WAITING_FOR_PBRAKE;
    assert(ground_ops_state_update(&state, &raw));
    assert(!state.snapshot.action_required);

    raw.step = PB_STEP_LIFTING;
    raw.awaiting_plan = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.action_required);
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_OPEN_PLANNER);
}

static void
test_change_plan_is_only_offered_during_connected_hold(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.step = PB_STEP_CONNECTED;
    raw.plan_complete = true;
    raw.replan_available = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_CHANGE_PLAN);
    assert(strcmp(state.snapshot.primary_action_label, "Change plan") == 0);

    raw.replan_available = false;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_NONE);

    raw.replan_available = true;
    raw.step = PB_STEP_STARTING;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.primary_action == GROUND_OPS_ACTION_NONE);
}

static void
test_emergency_tow_labels_and_actions(void)
{
    ground_ops_state_t state;
    ground_ops_raw_state_t raw = idle_raw();

    ground_ops_state_init(&state);
    raw.operation_active = true;
    raw.emergency_tow = true;
    raw.late_plan = true;
    raw.awaiting_plan = true;
    raw.step = PB_STEP_LIFTING;
    assert(ground_ops_state_update(&state, &raw));
    assert(state.snapshot.emergency_tow);
    assert(strcmp(state.snapshot.status,
        "Tug connected; plan the tow") == 0);
    assert(strcmp(state.snapshot.primary_action_label, "Plan tow") == 0);
    assert(strstr(state.snapshot.detail, "saved routes") != NULL);

    raw.awaiting_plan = false;
    raw.late_plan = false;
    raw.step = PB_STEP_PUSHING;
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.status,
        "Emergency tow in progress") == 0);
    assert(strcmp(state.snapshot.primary_action_label, "Pause tow") == 0);

    raw.pause_requested = true;
    raw.pause_held = true;
    assert(ground_ops_state_update(&state, &raw));
    assert(strcmp(state.snapshot.status, "Emergency tow paused") == 0);
    assert(strcmp(state.snapshot.primary_action_label, "Resume tow") == 0);
}

static void
test_button_roles_follow_the_task(void)
{
    ground_ops_snapshot_t snapshot = {0};
    /* Exercise every action both as a primary and as an alternative. */
    for (int action = GROUND_OPS_ACTION_NONE;
        action <= GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR; action++) {
        snapshot.primary_action = (ground_ops_action_t)action;
        snapshot.action_required = true;
        ground_ops_button_style_t expected = GROUND_OPS_BUTTON_SECONDARY;
        switch (action) {
        case GROUND_OPS_ACTION_CALL_TUG:
        case GROUND_OPS_ACTION_OPEN_PLANNER:
        case GROUND_OPS_ACTION_RESUME:
        case GROUND_OPS_ACTION_DISCONNECT_TUG:
        case GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR:
            expected = GROUND_OPS_BUTTON_REQUIRED;
            break;
        case GROUND_OPS_ACTION_END_DISCONNECT:
            expected = GROUND_OPS_BUTTON_DESTRUCTIVE;
            break;
        }
        assert(ground_ops_button_style(&snapshot,
            (ground_ops_action_t)action) == expected);
        snapshot.action_required = false;
        assert(ground_ops_button_style(&snapshot,
            (ground_ops_action_t)action) ==
            (action == GROUND_OPS_ACTION_END_DISCONNECT ?
            GROUND_OPS_BUTTON_DESTRUCTIVE : GROUND_OPS_BUTTON_SECONDARY));
        snapshot.action_required = true;
        snapshot.primary_action = GROUND_OPS_ACTION_NONE;
        assert(ground_ops_button_style(&snapshot,
            (ground_ops_action_t)action) != GROUND_OPS_BUTTON_REQUIRED);
    }

    snapshot = snapshot_for_step(PB_STEP_WAITING4OK2DISCO);
    assert(ground_ops_button_style(&snapshot, snapshot.primary_action) ==
        GROUND_OPS_BUTTON_REQUIRED);
    assert(ground_ops_button_style(&snapshot, snapshot.secondary_action) ==
        GROUND_OPS_BUTTON_SECONDARY);
    snapshot = snapshot_for_step(PB_STEP_CONNECTED);
    snapshot.primary_action = GROUND_OPS_ACTION_CHANGE_PLAN;
    assert(snapshot.action_required); /* Required brake release != Change plan. */
    assert(ground_ops_button_style(&snapshot, snapshot.primary_action) ==
        GROUND_OPS_BUTTON_SECONDARY);
}

static void
test_tug_return_is_an_informational_checklist_reminder(void)
{
    ground_ops_snapshot_t snapshot = snapshot_for_step(PB_STEP_DRIVING_AWAY);

    assert(strcmp(snapshot.status, "Tug returning to station") == 0);
    assert(strcmp(snapshot.current_task, "Finalize cockpit checklist") == 0);
    assert(!snapshot.action_required);
    assert(snapshot.primary_action == GROUND_OPS_ACTION_NONE);
    assert(snapshot.secondary_action == GROUND_OPS_ACTION_NONE);
    assert(snapshot.stage == GROUND_OPS_STAGE_CLEAR);
}

int
main(void)
{
    bp_clear_signal_gate_t gate = {0};
    assert(!bp_clear_signal_acknowledge(&gate, false));
    assert(!bp_clear_signal_acknowledge(&gate, true));
    gate.displayed = true;
    assert(!bp_clear_signal_can_depart(&gate, 3600.0));
    assert(!bp_clear_signal_acknowledge(&gate, false));
    assert(bp_clear_signal_acknowledge(&gate, true));
    assert(!bp_clear_signal_acknowledge(&gate, true));
    assert(!bp_clear_signal_can_depart(&gate, 14.999));
    assert(bp_clear_signal_can_depart(&gate, 15.0));
    bp_clear_signal_reset(&gate);
    assert(!bp_clear_signal_can_depart(&gate, 3600.0));

    ground_ops_state_t ack_state;
    ground_ops_raw_state_t ack_raw = idle_raw();
    ground_ops_state_init(&ack_state);
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(strcmp(ack_state.snapshot.speed, "Speed --") == 0);
    assert(strcmp(ack_state.snapshot.distance, "Remaining --") == 0);
    ack_raw.operation_active = true;
    ack_raw.step = PB_STEP_CLEAR_SIGNAL;
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(ack_state.snapshot.primary_action == GROUND_OPS_ACTION_NONE);
    ack_raw.clear_signal_displayed = true;
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(ack_state.snapshot.action_required);
    assert(strcmp(ack_state.snapshot.current_task, "Verify the clear signal") == 0);
    assert(strcmp(ack_state.snapshot.primary_action_label, "Acknowledge") == 0);
    assert(ack_state.snapshot.primary_action == GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR);
    assert(ack_state.snapshot.secondary_action == GROUND_OPS_ACTION_NONE);
    assert(ack_state.snapshot.stages[4] == GROUND_OPS_STAGE_CURRENT);
    ack_raw.clear_signal_acknowledged = true;
    ack_raw.speed_valid = ack_raw.distance_valid = true;
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(!ack_state.snapshot.action_required);
    assert(ack_state.snapshot.primary_action == GROUND_OPS_ACTION_NONE);
    assert(strcmp(ack_state.snapshot.status, "Clear signal acknowledged") == 0);
    assert(strcmp(ack_state.snapshot.speed, "Speed 0.0 m/s") == 0);
    assert(strcmp(ack_state.snapshot.distance, "Remaining 0 m") == 0);
    ack_raw.step = PB_STEP_WAITING4OK2DISCO;
    ack_raw.disconnect_approved = true;
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(strcmp(ack_state.snapshot.status, "Disconnect confirmed") == 0);
    assert(!ack_state.snapshot.action_required);
    assert(ack_state.snapshot.primary_action == GROUND_OPS_ACTION_NONE);
    ack_raw.operation_active = false;
    assert(ground_ops_state_update(&ack_state, &ack_raw));
    assert(!ack_state.previous_raw.clear_signal_displayed);
    assert(!ack_state.previous_raw.clear_signal_acknowledged);
    assert(!ack_state.previous_raw.disconnect_approved);

    test_every_controller_step_maps();
    test_stage_progression();
    test_action_markers();
    test_button_roles_follow_the_task();
    test_tug_return_is_an_informational_checklist_reminder();
    test_disconnect_gate_exposes_both_pilot_choices();
    test_prep_states_are_separate_from_controller();
    test_formatting_only_changes_for_display_values();
    test_captions_follow_message_state();
    test_waiting_state_has_no_timer_progress();
    test_stage_visuals_distinguish_blocking_from_automatic_progress();
    test_pre_lift_connection_hold_exposes_plan_action();
    test_pause_resume_actions_preserve_push_stage();
    test_end_from_pause_hold_uses_stationary_handoff();
    test_idle_workflow_requires_pilot_tug_call();
    test_completed_task_has_bounded_two_line_copy();
    test_provider_context_is_display_only();
    test_called_connection_has_no_second_gate_before_capture();
    test_change_plan_is_only_offered_during_connected_hold();
    test_emergency_tow_labels_and_actions();
    assert(sizeof(ground_ops_snapshot_t) < 1024);
    puts("ground_ops_state tests passed");
    return (0);
}
