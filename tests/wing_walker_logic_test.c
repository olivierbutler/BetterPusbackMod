#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#include "wing_walker_logic.h"

static void
test_normal_final_sequence(void)
{
    for (int step = PB_STEP_OFF; step < PB_STEP_STOPPED; step++) {
        assert(wing_walker_signal_for_step((pushback_step_t)step, false) ==
            WING_WALKER_SIGNAL_HIDDEN);
    }

    for (int step = PB_STEP_STOPPED;
        step <= PB_STEP_UNGRABBING; step++) {
        assert(wing_walker_signal_for_step((pushback_step_t)step, false) ==
            WING_WALKER_SIGNAL_STOP);
    }

    for (int step = PB_STEP_WAITING4OK2DISCO;
        step <= PB_STEP_MOVING2CLEAR; step++) {
        assert(wing_walker_signal_for_step((pushback_step_t)step, false) ==
            WING_WALKER_SIGNAL_STANDBY);
    }

    assert(wing_walker_signal_for_step(PB_STEP_CLEAR_SIGNAL, false) ==
        WING_WALKER_SIGNAL_CLEAR);
    assert(wing_walker_signal_for_step(PB_STEP_DRIVING_AWAY, false) ==
        WING_WALKER_SIGNAL_HIDDEN);
}

static void
test_reconnect_holds_stop_until_push_can_resume(void)
{
    assert(wing_walker_signal_for_step(PB_STEP_GRABBING, true) ==
        WING_WALKER_SIGNAL_STOP);
    assert(wing_walker_signal_for_step(PB_STEP_LIFTING, true) ==
        WING_WALKER_SIGNAL_STOP);
    assert(wing_walker_signal_for_step(PB_STEP_CONNECTED, true) ==
        WING_WALKER_SIGNAL_STOP);
    assert(wing_walker_signal_for_step(PB_STEP_STARTING, true) ==
        WING_WALKER_SIGNAL_HIDDEN);
    assert(wing_walker_signal_for_step(PB_STEP_PUSHING, true) ==
        WING_WALKER_SIGNAL_HIDDEN);
}

static bool
nearly_equal(double left, double right)
{
    return (fabs(left - right) < 1e-9);
}

static void
test_display_policy(void)
{
    assert(wing_walker_should_allocate(true, true));
    assert(!wing_walker_should_allocate(false, true));
    assert(!wing_walker_should_allocate(true, false));
    assert(!wing_walker_should_allocate(false, false));
}

static void
test_anchor_is_captured_once_and_faces_the_aircraft(void)
{
    wing_walker_anchor_t anchor = {0};

    assert(wing_walker_anchor_capture(&anchor, 100.0, 200.0, 0.0, 5.0,
        27.432, 1.5));
    assert(anchor.captured);
    assert(nearly_equal(anchor.x, 98.5));
    assert(nearly_equal(anchor.y, 232.432));
    assert(nearly_equal(anchor.heading, 0.0));

    assert(!wing_walker_anchor_capture(&anchor, 900.0, 800.0, 180.0, 20.0,
        27.432, 1.5));
    assert(nearly_equal(anchor.x, 98.5));
    assert(nearly_equal(anchor.y, 232.432));
    assert(nearly_equal(anchor.heading, 0.0));
}

static void
test_anchor_rotates_with_initial_aircraft_heading(void)
{
    wing_walker_anchor_t anchor = {0};

    assert(wing_walker_anchor_capture(&anchor, 100.0, 200.0, 450.0, 5.0,
        27.432, 1.5));
    assert(nearly_equal(anchor.x, 132.432));
    assert(nearly_equal(anchor.y, 201.5));
    assert(nearly_equal(anchor.heading, 90.0));
}

int
main(void)
{
    test_normal_final_sequence();
    test_reconnect_holds_stop_until_push_can_resume();
    test_display_policy();
    test_anchor_is_captured_once_and_faces_the_aircraft();
    test_anchor_rotates_with_initial_aircraft_heading();
    puts("wing walker logic tests passed");
    return (0);
}
