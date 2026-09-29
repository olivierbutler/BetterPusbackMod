/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy is also available in the repository's COPYING file.
 *
 * CDDL HEADER END
 */

#include "wing_walker_logic.h"

#include <math.h>
#include <stddef.h>

#define DEG2RAD_FACTOR (3.14159265358979323846 / 180.0)

wing_walker_signal_t
wing_walker_signal_for_step(pushback_step_t step, bool reconnecting)
{
    /*
     * A reconnect from the final disconnect gate returns directly to the
     * grabbing sequence. Keep the STOP signal displayed while equipment is
     * being reattached, but remove the worker before any resumed push begins.
     */
    if (reconnecting && step >= PB_STEP_GRABBING &&
        step <= PB_STEP_CONNECTED) {
        return (WING_WALKER_SIGNAL_STOP);
    }

    if (step >= PB_STEP_STOPPED && step <= PB_STEP_UNGRABBING)
        return (WING_WALKER_SIGNAL_STOP);

    if (step >= PB_STEP_WAITING4OK2DISCO &&
        step <= PB_STEP_MOVING2CLEAR)
        return (WING_WALKER_SIGNAL_STANDBY);

    if (step == PB_STEP_CLEAR_SIGNAL)
        return (WING_WALKER_SIGNAL_CLEAR);

    return (WING_WALKER_SIGNAL_HIDDEN);
}

bool
wing_walker_should_allocate(bool display_enabled,
    bool operation_allows_walker)
{
    return (display_enabled && operation_allows_walker);
}

bool
wing_walker_anchor_capture(wing_walker_anchor_t *anchor,
    double aircraft_x, double aircraft_y, double aircraft_heading,
    double aircraft_nose_forward, double nose_clearance,
    double captain_offset)
{
    double heading_radians, direction_x, direction_y, forward_distance;

    if (anchor == NULL || anchor->captured)
        return (false);

    aircraft_heading = fmod(aircraft_heading, 360.0);
    if (aircraft_heading < 0)
        aircraft_heading += 360.0;
    heading_radians = aircraft_heading * DEG2RAD_FACTOR;
    direction_x = sin(heading_radians);
    direction_y = cos(heading_radians);
    forward_distance = aircraft_nose_forward + nose_clearance;

    anchor->x = aircraft_x + direction_x * forward_distance -
        direction_y * captain_offset;
    anchor->y = aircraft_y + direction_y * forward_distance +
        direction_x * captain_offset;
    /* The current OBJ's visible front is opposite X-Plane's object axis. */
    anchor->heading = aircraft_heading;
    anchor->captured = true;
    return (true);
}
