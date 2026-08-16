/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license in the file COPYING
 * or http://www.opensource.org/licenses/CDDL-1.0.
 *
 * CDDL HEADER END
 */

#ifndef _REALISM_CONFIG_H_
#define _REALISM_CONFIG_H_

/* Shared by the live controller and the planner's trajectory prediction. */
#define PUSH_START_ACCEL_RAMP_TIME 3.0    /* seconds */
#define PUSH_START_JERK_LIMIT \
    (NORMAL_ACCEL / PUSH_START_ACCEL_RAMP_TIME) /* m/s^3 */
#define TURN_PROFILE_TRANSITION_DIST 10.0    /* meters */
#define TURN_PROFILE_STEER_RATE 6.0    /* degrees per second */
#define ROUTE_PATH_STEER_DEADBAND 1.0    /* degrees */
#define ROUTE_PATH_MAX_CORRECTION 12.0    /* degrees */
#define ROUTE_PATH_TERMINAL_FADE_DIST 8.0    /* meters */
#define TAIL_CROSS_TRACK_GAIN 0.5    /* steer degrees per meter */
#define TAIL_HEADING_GAIN 0.35    /* steer degrees per heading degree */
#define TAIL_MAX_STEER_CORRECTION 8.0    /* degrees */
#define TAIL_CROSS_TRACK_DEADBAND 0.15    /* meters */
#define TAIL_HEADING_DEADBAND 0.3    /* degrees */

#endif /* _REALISM_CONFIG_H_ */
