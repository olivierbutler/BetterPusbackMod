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
publish(const ground_ops_raw_state_t *raw, const bp_ext_state_t *previous)
{
    ground_ops_state_t state;
    bp_ext_state_t out;

    ground_ops_state_init(&state);
    assert(ground_ops_state_update(&state, raw));
    bp_ext_state_from(raw, ground_ops_state_get(&state), previous, &out);
    return (out);
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

int
main(void)
{
    test_every_step_has_a_unique_public_number_and_name();
    test_every_action_has_a_unique_public_number_and_name();
    test_idle_publishes_off_and_no_stage();
    test_pushing_publishes_step_stage_and_pause();
    test_state_seq_changes_only_when_the_state_does();
    test_emergency_tow_is_flagged();
    printf("ext_api_state tests passed\n");
    return (0);
}
