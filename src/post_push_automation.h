/* Pure post-push automation policy, independent of the simulator SDK. */
#ifndef BP_POST_PUSH_AUTOMATION_H
#define BP_POST_PUSH_AUTOMATION_H

#include <stdbool.h>

#include "clear_signal_gate.h"

static inline bool
bp_post_push_should_auto_disconnect(bool automatic_enabled, bool slave_mode,
    bool disconnect_approved)
{
    return automatic_enabled && !slave_mode && !disconnect_approved;
}

static inline bool
bp_post_push_auto_acknowledge_clear(bp_clear_signal_gate_t *gate,
    bool automatic_enabled, bool slave_mode)
{
    if (!automatic_enabled || slave_mode)
        return false;
    return bp_clear_signal_acknowledge(gate, true);
}

#endif /* BP_POST_PUSH_AUTOMATION_H */
