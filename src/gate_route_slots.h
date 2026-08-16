#ifndef GATE_ROUTE_SLOTS_H
#define GATE_ROUTE_SLOTS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GATE_ROUTE_SAVE_NONE,
    GATE_ROUTE_SAVE_UNCHANGED,
    GATE_ROUTE_SAVE_SLOT,
    GATE_ROUTE_SAVE_NEEDS_REPLACEMENT
} gate_route_save_policy_t;

int gate_route_first_empty_slot(const bool *occupied, size_t slot_count);

gate_route_save_policy_t gate_route_slot_save_policy(bool recognized,
    bool have_route, bool suppress_save, int loaded_slot, int save_slot,
    bool dirty, bool new_route);

#ifdef __cplusplus
}
#endif

#endif
