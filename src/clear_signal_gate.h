/* Pilot confirmation policy, independent of drawing and the simulator SDK. */
#ifndef BP_CLEAR_SIGNAL_GATE_H
#define BP_CLEAR_SIGNAL_GATE_H
#include <stdbool.h>

#define BP_CLEAR_SIGNAL_MIN_SECONDS 15.0
typedef struct {
    bool displayed;
    bool acknowledged;
} bp_clear_signal_gate_t;

static inline void
bp_clear_signal_reset(bp_clear_signal_gate_t *gate)
{
    gate->displayed = false;
    gate->acknowledged = false;
}

static inline bool
bp_clear_signal_acknowledge(bp_clear_signal_gate_t *gate, bool at_signal)
{
    if (!at_signal || !gate->displayed || gate->acknowledged)
        return false;
    gate->acknowledged = true;
    return true;
}

static inline bool
bp_clear_signal_can_depart(const bp_clear_signal_gate_t *gate, double elapsed)
{
    return gate->displayed && gate->acknowledged &&
        elapsed >= BP_CLEAR_SIGNAL_MIN_SECONDS;
}
#endif
