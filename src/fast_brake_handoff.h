#ifndef BP_FAST_BRAKE_HANDOFF_H
#define BP_FAST_BRAKE_HANDOFF_H

#include <stdbool.h>

#define BP_FAST_BRAKE_HOLD_SECONDS 1.5
#define BP_FAST_BRAKE_VERIFY_SECONDS 1.0

typedef enum {
    BP_FAST_BRAKE_HOLDING,
    BP_FAST_BRAKE_RELEASED,
    BP_FAST_BRAKE_VERIFIED
} bp_fast_brake_phase_t;

typedef struct {
    bp_fast_brake_phase_t phase;
    bool required;
    bool hold_started;
    double hold_started_at;
    double released_at;
} bp_fast_brake_handoff_t;

typedef enum {
    BP_FAST_BRAKE_HOLD,
    BP_FAST_BRAKE_RELEASE,
    BP_FAST_BRAKE_VERIFY,
    BP_FAST_BRAKE_RESTORE,
    BP_FAST_BRAKE_COMPLETE
} bp_fast_brake_action_t;

static inline void
bp_fast_brake_handoff_reset(bp_fast_brake_handoff_t *handoff, bool required)
{
    handoff->phase = BP_FAST_BRAKE_HOLDING;
    handoff->required = required;
    handoff->hold_started = false;
    handoff->hold_started_at = 0;
    handoff->released_at = 0;
}

static inline bool
bp_fast_brake_handoff_enabled(const bp_fast_brake_handoff_t *handoff,
    bool fast, bool slave, bool ignore_parking_brake)
{
    return handoff->required && fast && !slave && !ignore_parking_brake;
}

static inline bp_fast_brake_action_t
bp_fast_brake_handoff_update(bp_fast_brake_handoff_t *handoff, double now,
    bool parking_brake_set)
{
    if (handoff->phase == BP_FAST_BRAKE_RELEASED) {
        if (!parking_brake_set) {
            handoff->phase = BP_FAST_BRAKE_HOLDING;
            handoff->hold_started = false;
            return BP_FAST_BRAKE_RESTORE;
        }
        if (now - handoff->released_at < BP_FAST_BRAKE_VERIFY_SECONDS)
            return BP_FAST_BRAKE_VERIFY;
        handoff->phase = BP_FAST_BRAKE_VERIFIED;
    }

    if (handoff->phase == BP_FAST_BRAKE_VERIFIED) {
        if (parking_brake_set)
            return BP_FAST_BRAKE_COMPLETE;
        handoff->phase = BP_FAST_BRAKE_HOLDING;
        handoff->hold_started = false;
        return BP_FAST_BRAKE_RESTORE;
    }

    if (!parking_brake_set) {
        handoff->hold_started = false;
        return BP_FAST_BRAKE_HOLD;
    }
    if (!handoff->hold_started) {
        handoff->hold_started = true;
        handoff->hold_started_at = now;
    }
    if (now - handoff->hold_started_at < BP_FAST_BRAKE_HOLD_SECONDS)
        return BP_FAST_BRAKE_HOLD;

    handoff->phase = BP_FAST_BRAKE_RELEASED;
    handoff->released_at = now;
    return BP_FAST_BRAKE_RELEASE;
}

#endif
