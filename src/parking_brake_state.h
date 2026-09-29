#ifndef BP_PARKING_BRAKE_STATE_H
#define BP_PARKING_BRAKE_STATE_H

#include <stdbool.h>

static inline bool
bp_parking_brake_is_set(bool custom, bool xp122_or_newer, double parkbrake,
    double brake_ratio, bool valve_closed)
{
    if (custom)
        return parkbrake != 0.0;
    /* X-Plane 12.2 wheel_brake_ratio includes service-brake demand. */
    if (xp122_or_newer)
        return parkbrake != 0.0 || valve_closed;
    return parkbrake != 0.0 || brake_ratio != 0.0;
}

static inline bool
bp_zibo_parking_brake_is_set(double lever, double brake_ratio)
{
    return lever >= 0.5 && brake_ratio >= 0.95;
}

#endif
