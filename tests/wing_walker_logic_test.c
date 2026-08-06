#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

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

int
main(void)
{
    test_normal_final_sequence();
    test_reconnect_holds_stop_until_push_can_resume();
    puts("wing walker logic tests passed");
    return (0);
}
