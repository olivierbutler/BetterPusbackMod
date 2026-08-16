#include "emergency_tow.h"

typedef struct {
    bool active;
    bool planner_launch_claimed;
} emergency_tow_state_t;

static emergency_tow_state_t state;

void
emergency_tow_reset(void)
{
    state = (emergency_tow_state_t){0};
}

bool
emergency_tow_begin(void)
{
    if (state.active)
        return false;
    state.active = true;
    state.planner_launch_claimed = false;
    return true;
}

bool
emergency_tow_is_active(void)
{
    return state.active;
}

bool
emergency_tow_claim_planner_launch(void)
{
    if (!state.active || state.planner_launch_claimed)
        return false;
    state.planner_launch_claimed = true;
    return true;
}

bool
emergency_tow_finish(void)
{
    bool was_active = state.active;

    emergency_tow_reset();
    return was_active;
}

bool
emergency_tow_allows_persistent_routes(void)
{
    return !state.active;
}

bool
emergency_tow_allows_wing_walker(void)
{
    return !state.active;
}
