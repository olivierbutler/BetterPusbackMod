/*
 * Detailed BetterPushback controller states shared by the controller and
 * read-only presentation layers.
 */

#ifndef _PUSHBACK_STEP_H_
#define _PUSHBACK_STEP_H_

#include <stdbool.h>

typedef enum {
    PB_STEP_OFF,
    PB_STEP_TUG_LOAD,
    PB_STEP_START,
    PB_STEP_DRIVING_UP_CLOSE,
    PB_STEP_WAITING_FOR_DOORS,
    PB_STEP_OPENING_CRADLE,
    PB_STEP_WAITING_FOR_PBRAKE,
    PB_STEP_DRIVING_UP_CONNECT,
    PB_STEP_GRABBING,
    PB_STEP_LIFTING,
    PB_STEP_CONNECTED,
    PB_STEP_STARTING,
    PB_STEP_PUSHING,
    PB_STEP_STOPPING,
    PB_STEP_STOPPED,
    PB_STEP_LOWERING,
    PB_STEP_UNGRABBING,
    PB_STEP_WAITING4OK2DISCO,
    PB_STEP_MOVING_AWAY,
    PB_STEP_CLOSING_CRADLE,
    PB_STEP_STARTING2CLEAR,
    PB_STEP_MOVING2CLEAR,
    PB_STEP_CLEAR_SIGNAL,
    PB_STEP_DRIVING_AWAY
} pushback_step_t;

#define PB_STEP_COUNT (PB_STEP_DRIVING_AWAY + 1)

/*
 * Ending from a completed pause hold is a stationary handoff. The terminating
 * controller must not reintroduce creep merely to neutralize steering.
 */
static inline bool
pushback_stop_is_stationary_handoff(pushback_step_t step,
    bool pause_requested, bool pause_held)
{
    return (step == PB_STEP_PUSHING && pause_requested && pause_held);
}

#endif /* _PUSHBACK_STEP_H_ */
