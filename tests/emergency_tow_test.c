#include <assert.h>
#include <stdio.h>

#include "emergency_tow.h"

static void
test_idle_policy(void)
{
    emergency_tow_reset();
    assert(!emergency_tow_is_active());
    assert(emergency_tow_allows_persistent_routes());
    assert(emergency_tow_allows_wing_walker());
    assert(!emergency_tow_claim_planner_launch());
    assert(!emergency_tow_finish());
}

static void
test_active_session_is_isolated(void)
{
    assert(emergency_tow_begin());
    assert(!emergency_tow_begin());
    assert(emergency_tow_is_active());
    assert(!emergency_tow_allows_persistent_routes());
    assert(!emergency_tow_allows_wing_walker());
    assert(emergency_tow_claim_planner_launch());
    assert(!emergency_tow_claim_planner_launch());
}

static void
test_finish_restores_normal_policy(void)
{
    assert(emergency_tow_finish());
    assert(!emergency_tow_is_active());
    assert(emergency_tow_allows_persistent_routes());
    assert(emergency_tow_allows_wing_walker());
    assert(!emergency_tow_finish());
    assert(emergency_tow_begin());
    emergency_tow_reset();
    assert(!emergency_tow_is_active());
}

int
main(void)
{
    test_idle_policy();
    test_active_session_is_isolated();
    test_finish_restores_normal_policy();
    puts("emergency tow tests passed");
    return 0;
}
