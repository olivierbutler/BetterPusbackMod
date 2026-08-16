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

#include <float.h>
#include <math.h>
#include <string.h>

#include "telemetry.h"

#define TELEMETRY_FLUSH_INTERVAL 1.0

static void
write_csv_string(FILE *fp, const char *value)
{
    fputc('"', fp);
    if (value != NULL) {
        for (const char *p = value; *p != '\0'; p++) {
            if (*p == '"')
                fputc('"', fp);
            fputc(*p, fp);
        }
    }
    fputc('"', fp);
}

static void
write_csv_double(FILE *fp, double value)
{
    if (isfinite(value))
        fprintf(fp, "%.9f", value);
}

static void
write_metadata_string(FILE *fp, const char *key, const char *value)
{
    fprintf(fp, "# %s,", key);
    write_csv_string(fp, value);
    fputc('\n', fp);
}

static void
write_metadata_double(FILE *fp, const char *key, double value)
{
    fprintf(fp, "# %s,", key);
    write_csv_double(fp, value);
    fputc('\n', fp);
}

bool_t
bp_telemetry_open(bp_telemetry_t *telemetry, const char *filename,
    const bp_telemetry_metadata_t *metadata, double sample_hz)
{
    if (telemetry == NULL || filename == NULL || metadata == NULL ||
        sample_hz <= 0)
        return (B_FALSE);

    memset(telemetry, 0, sizeof(*telemetry));
    telemetry->fp = fopen(filename, "w");
    if (telemetry->fp == NULL)
        return (B_FALSE);

    telemetry->sample_period_s = 1.0 / sample_hz;
    telemetry->last_sample_time_s = -DBL_MAX;
    telemetry->last_flush_time_s = -DBL_MAX;
    telemetry->last_step = -1;

    fprintf(telemetry->fp, "# schema_version,%d\n",
        BP_TELEMETRY_SCHEMA_VERSION);
    fprintf(telemetry->fp, "# sample_hz,%.3f\n", sample_hz);
    write_metadata_string(telemetry->fp, "plugin_version",
        metadata->plugin_version);
    write_metadata_string(telemetry->fp, "aircraft_icao",
        metadata->aircraft_icao);
    write_metadata_string(telemetry->fp, "aircraft_file",
        metadata->aircraft_file);
    write_metadata_string(telemetry->fp, "aircraft_path",
        metadata->aircraft_path);
    write_metadata_string(telemetry->fp, "tug_name", metadata->tug_name);
    write_metadata_double(telemetry->fp, "aircraft_mass_kg",
        metadata->aircraft_mass_kg);
    write_metadata_double(telemetry->fp, "aircraft_mtow_kg",
        metadata->aircraft_mtow_kg);
    write_metadata_double(telemetry->fp, "aircraft_wheelbase_m",
        metadata->aircraft_wheelbase_m);
    write_metadata_double(telemetry->fp, "aircraft_tail_arm_m",
        metadata->aircraft_tail_arm_m);
    write_metadata_double(telemetry->fp, "aircraft_max_steer_deg",
        metadata->aircraft_max_steer_deg);
    write_metadata_double(telemetry->fp, "effective_max_fwd_speed_mps",
        metadata->effective_max_fwd_speed_mps);
    write_metadata_double(telemetry->fp, "effective_max_rev_speed_mps",
        metadata->effective_max_rev_speed_mps);
    write_metadata_double(telemetry->fp, "push_start_accel_ramp_time_s",
        metadata->push_start_accel_ramp_time_s);
    write_metadata_double(telemetry->fp, "push_start_jerk_limit_mps3",
        metadata->push_start_jerk_limit_mps3);
    write_metadata_double(telemetry->fp, "turn_profile_transition_m",
        metadata->turn_profile_transition_m);
    write_metadata_double(telemetry->fp, "turn_profile_steer_rate_dps",
        metadata->turn_profile_steer_rate_dps);
    write_metadata_double(telemetry->fp, "route_path_deadband_deg",
        metadata->route_path_deadband_deg);
    write_metadata_double(telemetry->fp, "route_path_max_correction_deg",
        metadata->route_path_max_correction_deg);
    write_metadata_double(telemetry->fp, "route_path_terminal_fade_m",
        metadata->route_path_terminal_fade_m);
    write_metadata_double(telemetry->fp, "tug_mass_kg",
        metadata->tug_mass_kg);
    write_metadata_double(telemetry->fp, "tug_wheelbase_m",
        metadata->tug_wheelbase_m);
    write_metadata_double(telemetry->fp, "tug_max_steer_deg",
        metadata->tug_max_steer_deg);
    write_metadata_double(telemetry->fp, "tug_max_fwd_speed_mps",
        metadata->tug_max_fwd_speed_mps);
    write_metadata_double(telemetry->fp, "tug_max_rev_speed_mps",
        metadata->tug_max_rev_speed_mps);
    write_metadata_double(telemetry->fp, "tug_max_tow_fwd_speed_mps",
        metadata->tug_max_tow_fwd_speed_mps);
    write_metadata_double(telemetry->fp, "tug_max_tow_rev_speed_mps",
        metadata->tug_max_tow_rev_speed_mps);
    write_metadata_double(telemetry->fp, "tug_max_accel_mps2",
        metadata->tug_max_accel_mps2);
    write_metadata_double(telemetry->fp, "tug_max_decel_mps2",
        metadata->tug_max_decel_mps2);
    write_metadata_double(telemetry->fp, "tug_max_tractive_effort_n",
        metadata->tug_max_tractive_effort_n);

    fprintf(telemetry->fp,
        "elapsed_s,sim_time_s,step,step_name,segment_type,"
        "segment_backward,segment_end_distance_m,segment_end_heading_deg,"
        "segment_radius_m,turn_profile_progress_m,turn_profile_total_m,"
        "turn_profile_target_deg,controller_target_steer_deg,"
        "tail_feedback_steer_deg,tail_feedback_weight,"
        "route_path_correction_deg,route_path_weight,"
        "route_reference_x_m,route_reference_z_m,route_cross_track_m,"
        "route_heading_error_deg,"
        "applied_steer_cmd_deg,planned_end_x_m,planned_end_z_m,"
        "planned_end_heading_deg,main_gear_x_m,main_gear_z_m,tail_x_m,"
        "tail_z_m,target_tail_x_m,target_tail_z_m,tail_cross_track_m,"
        "tail_along_remaining_m,final_heading_error_deg,aircraft_x_m,"
        "aircraft_z_m,aircraft_heading_deg,"
        "aircraft_speed_mps,aircraft_accel_mps2,aircraft_jerk_mps3,"
        "aircraft_yaw_rate_dps,tug_x_m,tug_z_m,tug_heading_deg,"
        "tug_speed_mps,tug_steer_deg,tug_turn_radius_m,"
        "nosewheel_steer_deg,tug_aircraft_heading_delta_deg,"
        "command_active,pause_requested,pause_held,route_steer_cmd_deg,"
        "nosewheel_steer_request_deg,"
        "target_speed_raw_mps,target_speed_limited_mps,"
        "max_accel_cmd_mps2,decelerating,applied_force_n,force_limit_n,"
        "force_fraction,heading_error_deg,left_brake_ratio,"
        "right_brake_ratio,parking_brake_set,runway_friction,frame_dt_s\n");
    fflush(telemetry->fp);

    return (B_TRUE);
}

bool_t
bp_telemetry_write(bp_telemetry_t *telemetry,
    const bp_telemetry_sample_t *sample, bool_t force)
{
    double elapsed, jerk = NAN, force_fraction = NAN;
    bool_t state_changed;

    if (telemetry == NULL || telemetry->fp == NULL || sample == NULL)
        return (B_FALSE);

    state_changed = (sample->step != telemetry->last_step ||
        sample->pause_requested != telemetry->last_pause_requested ||
        sample->pause_held != telemetry->last_pause_held);
    if (!force && !state_changed && sample->sim_time_s -
        telemetry->last_sample_time_s < telemetry->sample_period_s *
        (1.0 - 1e-6))
        return (B_FALSE);

    if (telemetry->last_sample_time_s == -DBL_MAX)
        telemetry->start_sim_time_s = sample->sim_time_s;
    elapsed = sample->sim_time_s - telemetry->start_sim_time_s;

    if (telemetry->have_last_accel && sample->sim_time_s >
        telemetry->last_sample_time_s) {
        jerk = (sample->aircraft_accel_mps2 - telemetry->last_accel_mps2) /
            (sample->sim_time_s - telemetry->last_sample_time_s);
    }
    if (sample->force_limit_n > 0)
        force_fraction = fabs(sample->applied_force_n) /
            sample->force_limit_n;

    write_csv_double(telemetry->fp, elapsed);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->sim_time_s);
    fprintf(telemetry->fp, ",%d,", sample->step);
    write_csv_string(telemetry->fp, sample->step_name);
    fprintf(telemetry->fp, ",");
    write_csv_string(telemetry->fp, sample->segment_type);
    fprintf(telemetry->fp, ",%d,", sample->segment_backward);
    write_csv_double(telemetry->fp, sample->segment_end_distance_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->segment_end_heading_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->segment_radius_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->turn_profile_progress_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->turn_profile_total_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->turn_profile_target_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->controller_target_steer_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_feedback_steer_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_feedback_weight);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_path_correction_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_path_weight);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_reference_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_reference_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_cross_track_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->route_heading_error_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->applied_steer_cmd_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->planned_end_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->planned_end_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->planned_end_heading_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->main_gear_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->main_gear_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->target_tail_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->target_tail_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_cross_track_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tail_along_remaining_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->final_heading_error_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_heading_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_speed_mps);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_accel_mps2);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, jerk);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->aircraft_yaw_rate_dps);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_x_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_z_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_heading_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_speed_mps);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_steer_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_turn_radius_m);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->nosewheel_steer_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->tug_aircraft_heading_delta_deg);
    fprintf(telemetry->fp, ",%d,%d,%d,", sample->command_active,
        sample->pause_requested, sample->pause_held);
    write_csv_double(telemetry->fp, sample->route_steer_cmd_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->nosewheel_steer_request_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->target_speed_raw_mps);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->target_speed_limited_mps);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->max_accel_cmd_mps2);
    fprintf(telemetry->fp, ",%d,", sample->decelerating);
    write_csv_double(telemetry->fp, sample->applied_force_n);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->force_limit_n);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, force_fraction);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->heading_error_deg);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->left_brake_ratio);
    fprintf(telemetry->fp, ",");
    write_csv_double(telemetry->fp, sample->right_brake_ratio);
    fprintf(telemetry->fp, ",%d,%d,", sample->parking_brake_set,
        sample->runway_friction);
    write_csv_double(telemetry->fp, sample->frame_dt_s);
    fputc('\n', telemetry->fp);

    telemetry->last_sample_time_s = sample->sim_time_s;
    telemetry->last_accel_mps2 = sample->aircraft_accel_mps2;
    telemetry->last_step = sample->step;
    telemetry->last_pause_requested = sample->pause_requested;
    telemetry->last_pause_held = sample->pause_held;
    telemetry->have_last_accel = B_TRUE;
    if (force || state_changed || sample->sim_time_s -
        telemetry->last_flush_time_s >= TELEMETRY_FLUSH_INTERVAL) {
        fflush(telemetry->fp);
        telemetry->last_flush_time_s = sample->sim_time_s;
    }

    return (B_TRUE);
}

void
bp_telemetry_close(bp_telemetry_t *telemetry)
{
    if (telemetry == NULL)
        return;
    if (telemetry->fp != NULL)
        fclose(telemetry->fp);
    memset(telemetry, 0, sizeof(*telemetry));
}
