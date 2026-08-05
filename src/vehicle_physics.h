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

#ifndef _VEHICLE_PHYSICS_H_
#define _VEHICLE_PHYSICS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Signed fixed-axle path radius for a simple bicycle steering model. */
double vehicle_turn_radius(double wheelbase, double steer_deg);

/* Applies the same steering speed envelope to left and right turns. */
double vehicle_steering_speed_limit(double requested_speed,
    double max_fwd_speed, double max_rev_speed, double steer_deg,
    double max_steer_deg);

/* Advances speed using distinct acceleration and braking capabilities. */
double vehicle_speed_step(double current_speed, double target_speed,
    double max_accel, double max_decel, double d_t);

/* Moves an acceleration command toward its target at a bounded jerk rate. */
double vehicle_accel_step(double current_accel, double target_accel,
    double max_jerk, double d_t);

/* Moves a steering command toward its target at a bounded angular rate. */
double vehicle_steering_step(double current_steer, double target_steer,
    double max_rate_deg_s, double d_t);

/*
 * Advances a rear-axle pose through one constant-control bicycle-model step.
 * Heading is in simulator degrees (zero north, positive clockwise).
 */
void vehicle_bicycle_step(double wheelbase, double speed_mps,
    double steer_deg, double d_t, double *x_m, double *z_m,
    double *heading_deg);

/*
 * Returns a feed-forward steering command for a smooth turn. Curvature uses
 * symmetric smootherstep transitions whose integral preserves the requested
 * total heading change.
 */
double vehicle_turn_profile_steer(double wheelbase, double radius,
    double total_distance, double distance_into_turn,
    double transition_distance, double direction, double max_steer_deg);

/*
 * Converts the legacy route controller's demand into a small correction
 * around the smooth feed-forward profile. This deliberately prevents a
 * large line-chasing request from replacing the continuous turn profile.
 */
double vehicle_path_steer_correction(double route_steer_deg,
    double profile_steer_deg, double deadband_deg,
    double max_correction_deg, double weight);

/*
 * Smoothly hands the terminal portion of the route from path tracking to
 * endpoint capture. Non-terminal segments always return full weight.
 */
double vehicle_path_terminal_weight(double along_remaining_m,
    double fade_distance_m, int terminal_segment);

/*
 * Produces a bounded steering correction from tail cross-track and aircraft
 * heading error. `capture_fraction' runs from zero at the beginning of a
 * turn to one on the final alignment. Cross-track is positive to the right
 * of the target aircraft centerline.
 */
double vehicle_tail_steer_correction(double tail_cross_track_m,
    double heading_error_deg, double capture_fraction,
    double cross_track_gain_deg_m, double heading_gain,
    double max_correction_deg, int backward);

/* Returns the lower of aircraft safety force and available tug effort. */
double vehicle_force_limit(double aircraft_force_limit,
    double tug_tractive_effort);

#ifdef __cplusplus
}
#endif

#endif /* _VEHICLE_PHYSICS_H_ */
