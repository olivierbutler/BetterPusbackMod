#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "gate_route_slots.h"

static void
test_first_empty_slot(void)
{
    const bool none[2] = {false, false};
    const bool first[2] = {true, false};
    const bool second[2] = {false, true};
    const bool full[2] = {true, true};

    assert(gate_route_first_empty_slot(none, 2) == 0);
    assert(gate_route_first_empty_slot(first, 2) == 1);
    assert(gate_route_first_empty_slot(second, 2) == 0);
    assert(gate_route_first_empty_slot(full, 2) == -1);
}

static void
test_save_policy(void)
{
    assert(gate_route_slot_save_policy(false, true, false, -1, 0, true,
        true) == GATE_ROUTE_SAVE_NONE);
    assert(gate_route_slot_save_policy(true, true, true, -1, -1, true,
        true) == GATE_ROUTE_SAVE_NONE);
    assert(gate_route_slot_save_policy(true, true, false, 0, 0, false,
        false) == GATE_ROUTE_SAVE_UNCHANGED);
    assert(gate_route_slot_save_policy(true, true, false, 0, 0, true,
        false) == GATE_ROUTE_SAVE_SLOT);
    assert(gate_route_slot_save_policy(true, true, false, -1, 1, true,
        true) == GATE_ROUTE_SAVE_SLOT);
    assert(gate_route_slot_save_policy(true, true, false, -1, -1, true,
        true) == GATE_ROUTE_SAVE_NEEDS_REPLACEMENT);
    assert(gate_route_slot_save_policy(true, false, false, -1, -1, false,
        true) == GATE_ROUTE_SAVE_NONE);
}

int
main(void)
{
    test_first_empty_slot();
    test_save_policy();
    puts("gate route slot policy tests passed");
    return 0;
}
