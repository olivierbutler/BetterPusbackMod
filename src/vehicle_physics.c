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

#include <math.h>
#include <stddef.h>

#include "vehicle_physics.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD_LOCAL(x) ((x) * M_PI / 180.0)
#define RAD2DEG_LOCAL(x) ((x) * 180.0 / M_PI)
#define STEER_EPSILON 1e-6

static double
clamp(double value, double minimum, double maximum)
{
    return (fmin(fmax(value, minimum), maximum));
}

double
vehicle_turn_radius(double wheelbase, double steer_deg)
{
    if (fabs(steer_deg) < STEER_EPSILON)
        return (copysign(HUGE_VAL, steer_deg));

    return (wheelbase / tan(DEG2RAD_LOCAL(steer_deg)));
}

double
vehicle_steering_speed_limit(double requested_speed, double max_fwd_speed,
    double max_rev_speed, double steer_deg, double max_steer_deg)
{
    double max_speed, steer_fraction, speed_fraction;

    if (requested_speed == 0)
        return (0);

    max_speed = (requested_speed > 0 ? max_fwd_speed : max_rev_speed);
    steer_fraction = clamp(fabs(steer_deg) / max_steer_deg, 0, 1);
    /* Preserve the original 10% crawl speed at full steering lock. */
    speed_fraction = clamp(1.1 - steer_fraction, 0.1, 1);
    max_speed *= speed_fraction;

    return (clamp(requested_speed, -max_speed, max_speed));
}

double
vehicle_speed_step(double current_speed, double target_speed,
    double max_accel, double max_decel, double d_t)
{
    double delta, rate;

    if (d_t <= 0 || current_speed == target_speed)
        return (current_speed);

    /* Complete a direction change by braking to zero before accelerating. */
    if (current_speed * target_speed < 0) {
        delta = -current_speed;
        rate = max_decel;
    } else {
        delta = target_speed - current_speed;
        rate = (fabs(target_speed) > fabs(current_speed) ? max_accel :
            max_decel);
    }

    return (current_speed + clamp(delta, -rate * d_t, rate * d_t));
}

double
vehicle_accel_step(double current_accel, double target_accel,
    double max_jerk, double d_t)
{
    double max_change;

    if (d_t <= 0 || max_jerk <= 0 || current_accel == target_accel)
        return (current_accel);

    max_change = max_jerk * d_t;
    return (current_accel + clamp(target_accel - current_accel,
        -max_change, max_change));
}

double
vehicle_steering_step(double current_steer, double target_steer,
    double max_rate_deg_s, double d_t)
{
    double max_change;

    if (d_t <= 0 || max_rate_deg_s <= 0 || current_steer == target_steer)
        return (current_steer);

    max_change = max_rate_deg_s * d_t;
    return (current_steer + clamp(target_steer - current_steer,
        -max_change, max_change));
}

void
vehicle_bicycle_step(double wheelbase, double speed_mps,
    double steer_deg, double d_t, double *x_m, double *z_m,
    double *heading_deg)
{
    double distance, heading, curvature, heading_change;

    if (x_m == NULL || z_m == NULL || heading_deg == NULL ||
        wheelbase <= 0 || d_t <= 0 || !isfinite(speed_mps) ||
        !isfinite(steer_deg) || !isfinite(*x_m) || !isfinite(*z_m) ||
        !isfinite(*heading_deg))
        return;

    distance = speed_mps * d_t;
    heading = DEG2RAD_LOCAL(*heading_deg);
    curvature = tan(DEG2RAD_LOCAL(clamp(steer_deg, -89.9, 89.9))) /
        wheelbase;
    heading_change = distance * curvature;

    if (fabs(curvature) < STEER_EPSILON) {
        *x_m += sin(heading) * distance;
        *z_m += cos(heading) * distance;
    } else {
        *x_m += (cos(heading) - cos(heading + heading_change)) /
            curvature;
        *z_m += (sin(heading + heading_change) - sin(heading)) /
            curvature;
    }

    *heading_deg = fmod(*heading_deg + RAD2DEG_LOCAL(heading_change), 360);
    if (*heading_deg < 0)
        *heading_deg += 360;
}

static double
smootherstep(double value)
{
    value = clamp(value, 0, 1);
    return (value * value * value * (value * (value * 6 - 15) + 10));
}

double
vehicle_turn_profile_steer(double wheelbase, double radius,
    double total_distance, double distance_into_turn,
    double transition_distance, double direction, double max_steer_deg)
{
    double geometric_curvature, maximum_curvature, peak_curvature;
    double maximum_transition, entry, exit, profile, steer;

    radius = fabs(radius);
    total_distance = fabs(total_distance);
    max_steer_deg = fmin(fabs(max_steer_deg), 89.9);
    if (wheelbase <= 0 || radius <= 0 || total_distance <= 0 ||
        max_steer_deg <= 0 || direction == 0 ||
        !isfinite(radius) || !isfinite(total_distance))
        return (0);

    distance_into_turn = clamp(distance_into_turn, 0, total_distance);
    if (distance_into_turn <= 0 || distance_into_turn >= total_distance)
        return (0);

    geometric_curvature = 1.0 / radius;
    maximum_curvature = tan(DEG2RAD_LOCAL(max_steer_deg)) / wheelbase;
    if (maximum_curvature <= geometric_curvature) {
        transition_distance = 0;
        peak_curvature = maximum_curvature;
    } else {
        /*
         * Two symmetric transitions lose one transition-distance worth of
         * curvature area. Raise plateau curvature by the exact amount needed
         * to preserve the planned total heading change, while respecting the
         * steering stop.
         */
        maximum_transition = total_distance *
            (1.0 - geometric_curvature / maximum_curvature);
        transition_distance = clamp(fabs(transition_distance), 0,
            fmin(total_distance / 2.0, maximum_transition));
        peak_curvature = geometric_curvature * total_distance /
            (total_distance - transition_distance);
    }

    if (transition_distance <= STEER_EPSILON) {
        profile = 1;
    } else {
        entry = smootherstep(distance_into_turn / transition_distance);
        exit = smootherstep((total_distance - distance_into_turn) /
            transition_distance);
        profile = fmin(entry, exit);
    }

    steer = RAD2DEG_LOCAL(atan(wheelbase * peak_curvature * profile));
    return (copysign(fmin(steer, max_steer_deg), direction));
}

double
vehicle_path_steer_correction(double route_steer_deg,
    double profile_steer_deg, double deadband_deg,
    double max_correction_deg, double weight)
{
    double difference;

    if (!isfinite(route_steer_deg) || !isfinite(profile_steer_deg) ||
        !isfinite(weight) || deadband_deg < 0 || max_correction_deg <= 0)
        return (0);

    difference = route_steer_deg - profile_steer_deg;
    if (fabs(difference) <= deadband_deg)
        return (0);
    difference -= copysign(deadband_deg, difference);

    return (clamp(difference, -max_correction_deg, max_correction_deg) *
        clamp(weight, 0, 1));
}

double
vehicle_path_terminal_weight(double along_remaining_m,
    double fade_distance_m, int terminal_segment)
{
    if (!terminal_segment)
        return (1);
    if (!isfinite(along_remaining_m) || !isfinite(fade_distance_m) ||
        fade_distance_m <= 0)
        return (0);

    return (smootherstep(along_remaining_m / fade_distance_m));
}

double
vehicle_tail_steer_correction(double tail_cross_track_m,
    double heading_error_deg, double capture_fraction,
    double cross_track_gain_deg_m, double heading_gain,
    double max_correction_deg, int backward)
{
    double correction, weight;

    if (!isfinite(tail_cross_track_m) || !isfinite(heading_error_deg) ||
        !isfinite(capture_fraction) || cross_track_gain_deg_m < 0 ||
        heading_gain < 0 || max_correction_deg <= 0)
        return (0);

    weight = smootherstep(capture_fraction);
    correction = tail_cross_track_m * cross_track_gain_deg_m +
        heading_error_deg * heading_gain;
    if (backward)
        correction = -correction;

    return (clamp(correction * weight, -max_correction_deg,
        max_correction_deg));
}

double
vehicle_force_limit(double aircraft_force_limit, double tug_tractive_effort)
{
    return (fmin(aircraft_force_limit, tug_tractive_effort));
}
