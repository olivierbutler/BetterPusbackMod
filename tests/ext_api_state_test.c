#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ext_api_state.h"
#include "intl_test_stub.h"

static ground_ops_raw_state_t
raw_for_step(pushback_step_t step)
{
    ground_ops_raw_state_t raw;

    memset(&raw, 0, sizeof (raw));
    raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    raw.captions_enabled = true;
    raw.operation_active = (step != PB_STEP_OFF);
    raw.prep_state_active = !raw.operation_active;
    raw.step = step;
    return (raw);
}

static bp_ext_state_t
publish_blocked(const ground_ops_raw_state_t *raw,
    const bp_ext_blocker_in_t *blocker, const bp_ext_state_t *previous)
{
    ground_ops_state_t state;
    bp_ext_state_t out;

    ground_ops_state_init(&state);
    assert(ground_ops_state_update(&state, raw));
    bp_ext_state_from(raw, ground_ops_state_get(&state), blocker, previous,
        &out);
    return (out);
}

static bp_ext_state_t
publish(const ground_ops_raw_state_t *raw, const bp_ext_state_t *previous)
{
    return (publish_blocked(raw, NULL, previous));
}

static void
test_every_step_has_a_unique_public_number_and_name(void)
{
    bool seen[PB_STEP_COUNT] = { false };

    for (int s = 0; s < PB_STEP_COUNT; s++) {
        int pub = bp_ext_step_public((pushback_step_t)s);

        assert(pub >= 0 && pub < PB_STEP_COUNT);
        assert(!seen[pub]);
        seen[pub] = true;
        assert(strcmp(bp_ext_step_name(pub), "unknown") != 0);
    }
    /* The published numbers are the interface: these must never change. */
    assert(bp_ext_step_public(PB_STEP_OFF) == 0);
    assert(bp_ext_step_public(PB_STEP_PUSHING) == 12);
    assert(bp_ext_step_public(PB_STEP_WAITING4OK2DISCO) == 17);
    assert(bp_ext_step_public(PB_STEP_DRIVING_AWAY) == 23);
    assert(strcmp(bp_ext_step_name(12), "pushing") == 0);
    assert(strcmp(bp_ext_step_name(-1), "unknown") == 0);
    assert(strcmp(bp_ext_step_name(PB_STEP_COUNT), "unknown") == 0);
}

static void
test_every_action_has_a_unique_public_number_and_name(void)
{
    static const ground_ops_action_t actions[] = {
        GROUND_OPS_ACTION_NONE, GROUND_OPS_ACTION_CALL_TUG,
        GROUND_OPS_ACTION_CALL_EMERGENCY_TOW, GROUND_OPS_ACTION_OPEN_PLANNER,
        GROUND_OPS_ACTION_CHANGE_PLAN, GROUND_OPS_ACTION_PAUSE,
        GROUND_OPS_ACTION_RESUME, GROUND_OPS_ACTION_DISCONNECT_TUG,
        GROUND_OPS_ACTION_RECONNECT_TUG, GROUND_OPS_ACTION_END_DISCONNECT,
        GROUND_OPS_ACTION_ACKNOWLEDGE_CLEAR
    };
    bool seen[sizeof (actions) / sizeof (actions[0])] = { false };

    for (size_t i = 0; i < sizeof (actions) / sizeof (actions[0]); i++) {
        int pub = bp_ext_action_public(actions[i]);

        assert(pub >= 0 && pub < (int)(sizeof (seen) / sizeof (seen[0])));
        assert(!seen[pub]);
        seen[pub] = true;
        assert(strcmp(bp_ext_action_name(pub), "unknown") != 0);
    }
    assert(bp_ext_action_public(GROUND_OPS_ACTION_CALL_TUG) == 1);
    assert(strcmp(bp_ext_action_name(1), "call_tug") == 0);
    assert(strcmp(bp_ext_action_name(10), "acknowledge_clear") == 0);
}

static void
test_idle_publishes_off_and_no_stage(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_OFF);
    bp_ext_state_t out = publish(&raw, NULL);

    assert(out.step == BP_EXT_STEP_OFF);
    assert(strcmp(out.step_name, "off") == 0);
    assert(out.stage == -1);
    assert(strcmp(out.stage_name, "idle") == 0);
    assert(out.emergency_tow == 0 && out.paused == 0);
    assert(out.state_seq == 1);
    assert(strcmp(out.action_name, bp_ext_action_name(out.action)) == 0);
}

static void
test_pushing_publishes_step_stage_and_pause(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_PUSHING);
    bp_ext_state_t out;

    raw.plan_complete = true;
    out = publish(&raw, NULL);
    assert(out.step == BP_EXT_STEP_PUSHING);
    assert(strcmp(out.step_name, "pushing") == 0);
    assert(out.stage == GROUND_OPS_STAGE_PUSH);
    assert(strcmp(out.stage_name, "push") == 0);
    assert(out.paused == 0);

    raw.pause_requested = true;
    raw.pause_held = true;
    out = publish(&raw, NULL);
    assert(out.paused == 1);
    assert(out.action == BP_EXT_ACTION_RESUME);
    assert(strcmp(out.action_name, "resume") == 0);
}

static void
test_state_seq_changes_only_when_the_state_does(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_CONNECTED);
    bp_ext_state_t first = publish(&raw, NULL);
    bp_ext_state_t same = publish(&raw, &first);
    bp_ext_state_t next;

    assert(same.state_seq == first.state_seq);
    raw = raw_for_step(PB_STEP_PUSHING);
    next = publish(&raw, &same);
    assert(next.state_seq == same.state_seq + 1);
}

static void
test_emergency_tow_is_flagged(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_PUSHING);
    bp_ext_state_t out;

    raw.emergency_tow = true;
    out = publish(&raw, NULL);
    assert(out.emergency_tow == 1);
}

static void
test_blockers_are_published_with_the_open_item(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_OPENING_CRADLE);
    bp_ext_blocker_in_t in = { BP_EXT_BLOCKER_AIRCRAFT_NOT_READY,
        "laminar/B738/gpu_available",
        "Waiting for doors/GPU/ASU closed/disconnected" };
    bp_ext_state_t first = publish_blocked(&raw, &in, NULL);
    bp_ext_state_t closed;

    assert(first.blocker == BP_EXT_BLOCKER_AIRCRAFT_NOT_READY);
    assert(strcmp(first.blocker_name, "aircraft_not_ready") == 0);
    assert(strcmp(first.blocker_item, "laminar/B738/gpu_available") == 0);
    assert(strcmp(first.blocker_item_kind, "gpu") == 0);
    assert(strstr(first.status, "Waiting for doors") != NULL);

    /* The GPU is removed: the blocker clears and state_seq moves. */
    in.blocker = BP_EXT_BLOCKER_NONE;
    in.item = NULL;
    in.status = "Opening the cradle";
    closed = publish_blocked(&raw, &in, &first);
    assert(closed.blocker == BP_EXT_BLOCKER_NONE);
    assert(strcmp(closed.blocker_name, "none") == 0);
    assert(closed.blocker_item[0] == '\0' && closed.blocker_item_kind[0] == '\0');
    assert(closed.state_seq == first.state_seq + 1);
}

static void
test_an_item_is_only_published_for_aircraft_not_ready(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_CONNECTED);
    bp_ext_blocker_in_t in = { BP_EXT_BLOCKER_RELEASE_PARKING_BRAKE,
        "737u/doors/L1", NULL };
    bp_ext_state_t out = publish_blocked(&raw, &in, NULL);

    assert(out.blocker == BP_EXT_BLOCKER_RELEASE_PARKING_BRAKE);
    assert(strcmp(out.blocker_name, "release_parking_brake") == 0);
    assert(out.blocker_item[0] == '\0');
}

static void
test_nothing_is_blocked_without_an_operation(void)
{
    ground_ops_raw_state_t raw = raw_for_step(PB_STEP_OFF);
    bp_ext_blocker_in_t in = { BP_EXT_BLOCKER_SET_PARKING_BRAKE, NULL,
        "stale" };
    bp_ext_state_t out = publish_blocked(&raw, &in, NULL);

    assert(out.blocker == BP_EXT_BLOCKER_NONE && out.status[0] == '\0');
    in.blocker = 99;                    /* unknown values are not passed on */
    raw = raw_for_step(PB_STEP_PUSHING);
    out = publish_blocked(&raw, &in, NULL);
    assert(out.blocker == BP_EXT_BLOCKER_NONE);
}

static void
test_item_kinds_come_from_the_dataref_name(void)
{
    assert(strcmp(bp_ext_item_kind("laminar/B738/gpu_available"), "gpu") == 0);
    assert(strcmp(bp_ext_item_kind("ToLiss/GPU/connected"), "gpu") == 0);
    assert(strcmp(bp_ext_item_kind("ixeg/733/asu/asu_connected"), "asu") == 0);
    assert(strcmp(bp_ext_item_kind("737u/doors/cargos"), "cargo_door") == 0);
    assert(strcmp(bp_ext_item_kind("737u/doors/L1"), "door") == 0);
    assert(strcmp(bp_ext_item_kind("acf/hatch_open"), "door") == 0);
    /* "asu" inside a word is not the ASU. */
    assert(strcmp(bp_ext_item_kind("sim/cockpit/pressure_measured"),
        "other") == 0);
    assert(strcmp(bp_ext_item_kind(""), "") == 0);
    assert(strcmp(bp_ext_item_kind(NULL), "") == 0);
}

int
main(void)
{
    test_every_step_has_a_unique_public_number_and_name();
    test_every_action_has_a_unique_public_number_and_name();
    test_idle_publishes_off_and_no_stage();
    test_pushing_publishes_step_stage_and_pause();
    test_state_seq_changes_only_when_the_state_does();
    test_emergency_tow_is_flagged();
    test_blockers_are_published_with_the_open_item();
    test_an_item_is_only_published_for_aircraft_not_ready();
    test_nothing_is_blocked_without_an_operation();
    test_item_kinds_come_from_the_dataref_name();
    printf("ext_api_state tests passed\n");
    return (0);
}
