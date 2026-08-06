#include "gate_route_slots.h"

#include <assert.h>

int
gate_route_first_empty_slot(const bool *occupied, size_t slot_count)
{
    assert(occupied != NULL || slot_count == 0);
    for (size_t slot = 0; slot < slot_count; slot++) {
        if (!occupied[slot])
            return (int)slot;
    }
    return -1;
}

gate_route_save_policy_t
gate_route_slot_save_policy(bool recognized, bool have_route,
    bool suppress_save, int loaded_slot, int save_slot, bool dirty,
    bool new_route)
{
    if (!recognized || !have_route || suppress_save)
        return GATE_ROUTE_SAVE_NONE;
    if (loaded_slot >= 0 && !dirty)
        return GATE_ROUTE_SAVE_UNCHANGED;
    if (save_slot >= 0)
        return GATE_ROUTE_SAVE_SLOT;
    if (new_route)
        return GATE_ROUTE_SAVE_NEEDS_REPLACEMENT;
    return GATE_ROUTE_SAVE_NONE;
}
