#ifndef BP_FAST_BRAKE_HANDOFF_H
#define BP_FAST_BRAKE_HANDOFF_H

#include <stdbool.h>

typedef enum {
    BP_FAST_BRAKE_HOLDING,
    BP_FAST_BRAKE_WAITING_PEDALS,
    BP_FAST_BRAKE_VERIFIED
} bp_fast_brake_phase_t;

typedef struct {
    bp_fast_brake_phase_t phase;
    bool required;
} bp_fast_brake_handoff_t;

typedef enum {
    BP_FAST_BRAKE_HOLD,
    BP_FAST_BRAKE_WITHDRAW,
    BP_FAST_BRAKE_WAIT_PEDALS,
    BP_FAST_BRAKE_RESTORE,
    BP_FAST_BRAKE_COMPLETE
} bp_fast_brake_action_t;

static inline void
bp_fast_brake_handoff_reset(bp_fast_brake_handoff_t *handoff, bool required)
{
    handoff->phase = BP_FAST_BRAKE_HOLDING;
    handoff->required = required;
}

static inline bool
bp_fast_brake_handoff_enabled(const bp_fast_brake_handoff_t *handoff,
    bool fast, bool slave, bool ignore_parking_brake)
{
    return handoff->required && fast && !slave && !ignore_parking_brake;
}

static inline bp_fast_brake_action_t
bp_fast_brake_handoff_update(bp_fast_brake_handoff_t *handoff,
    bool parking_brake_set, bool pedals_released)
{
    if (!parking_brake_set) {
        bool withdrawn = handoff->phase != BP_FAST_BRAKE_HOLDING;
        handoff->phase = BP_FAST_BRAKE_HOLDING;
        return withdrawn ? BP_FAST_BRAKE_RESTORE : BP_FAST_BRAKE_HOLD;
    }
    if (handoff->phase == BP_FAST_BRAKE_VERIFIED)
        return BP_FAST_BRAKE_COMPLETE;
    if (handoff->phase == BP_FAST_BRAKE_HOLDING) {
        /* Observe a subsequent frame without any BPB toe-brake write. */
        handoff->phase = BP_FAST_BRAKE_WAITING_PEDALS;
        return BP_FAST_BRAKE_WITHDRAW;
    }
    if (!pedals_released) {
        handoff->phase = BP_FAST_BRAKE_WAITING_PEDALS;
        return BP_FAST_BRAKE_WAIT_PEDALS;
    }
    handoff->phase = BP_FAST_BRAKE_VERIFIED;
    return BP_FAST_BRAKE_COMPLETE;
}

#endif
