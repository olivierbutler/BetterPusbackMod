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

#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <stdio.h>

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_TELEMETRY_SCHEMA_VERSION 7

typedef struct {
    const char *plugin_version;
    const char *aircraft_icao;
    const char *aircraft_file;
    const char *aircraft_path;
    const char *tug_name;
    double aircraft_mass_kg;
    double aircraft_mtow_kg;
    double aircraft_wheelbase_m;
    double aircraft_tail_arm_m;
    double aircraft_max_steer_deg;
    double effective_max_fwd_speed_mps;
    double effective_max_rev_speed_mps;
    double push_start_accel_ramp_time_s;
    double push_start_jerk_limit_mps3;
    double turn_profile_transition_m;
    double turn_profile_steer_rate_dps;
    double route_path_deadband_deg;
    double route_path_max_correction_deg;
    double route_path_terminal_fade_m;
    double tug_mass_kg;
    double tug_wheelbase_m;
    double tug_max_steer_deg;
    double tug_max_fwd_speed_mps;
    double tug_max_rev_speed_mps;
    double tug_max_tow_fwd_speed_mps;
    double tug_max_tow_rev_speed_mps;
    double tug_max_accel_mps2;
    double tug_max_decel_mps2;
    double tug_max_tractive_effort_n;
} bp_telemetry_metadata_t;

typedef struct {
    double sim_time_s;
    int step;
    const char *step_name;
    const char *segment_type;
    int segment_backward;
    double segment_end_distance_m;
    double segment_end_heading_deg;
    double segment_radius_m;
    double turn_profile_progress_m;
    double turn_profile_total_m;
    double turn_profile_target_deg;
    double applied_steer_cmd_deg;
    double controller_target_steer_deg;
    double tail_feedback_steer_deg;
    double tail_feedback_weight;
    double route_path_correction_deg;
    double route_path_weight;
    double route_reference_x_m;
    double route_reference_z_m;
    double route_cross_track_m;
    double route_heading_error_deg;
    double planned_end_x_m;
    double planned_end_z_m;
    double planned_end_heading_deg;
    double main_gear_x_m;
    double main_gear_z_m;
    double tail_x_m;
    double tail_z_m;
    double target_tail_x_m;
    double target_tail_z_m;
    double tail_cross_track_m;
    double tail_along_remaining_m;
    double final_heading_error_deg;
    double aircraft_x_m;
    double aircraft_z_m;
    double aircraft_heading_deg;
    double aircraft_speed_mps;
    double aircraft_accel_mps2;
    double aircraft_yaw_rate_dps;
    double tug_x_m;
    double tug_z_m;
    double tug_heading_deg;
    double tug_speed_mps;
    double tug_steer_deg;
    double tug_turn_radius_m;
    double nosewheel_steer_deg;
    double tug_aircraft_heading_delta_deg;
    int command_active;
    int pause_requested;
    int pause_held;
    double route_steer_cmd_deg;
    double nosewheel_steer_request_deg;
    double target_speed_raw_mps;
    double target_speed_limited_mps;
    double max_accel_cmd_mps2;
    int decelerating;
    double applied_force_n;
    double force_limit_n;
    double heading_error_deg;
    double left_brake_ratio;
    double right_brake_ratio;
    int parking_brake_set;
    int runway_friction;
    double frame_dt_s;
} bp_telemetry_sample_t;

typedef struct {
    FILE *fp;
    double start_sim_time_s;
    double last_sample_time_s;
    double last_flush_time_s;
    double last_accel_mps2;
    double sample_period_s;
    int last_step;
    int last_pause_requested;
    int last_pause_held;
    bool_t have_last_accel;
} bp_telemetry_t;

bool_t bp_telemetry_open(bp_telemetry_t *telemetry, const char *filename,
    const bp_telemetry_metadata_t *metadata, double sample_hz);

bool_t bp_telemetry_write(bp_telemetry_t *telemetry,
    const bp_telemetry_sample_t *sample, bool_t force);

void bp_telemetry_close(bp_telemetry_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* _TELEMETRY_H_ */
