#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "telemetry.h"

int
main(int argc, char **argv)
{
    bp_telemetry_t telemetry;
    bp_telemetry_metadata_t metadata = {
        .plugin_version = "test-version",
        .aircraft_icao = "B738",
        .aircraft_file = "test,aircraft.acf",
        .aircraft_path = "/test/path",
        .tug_name = "AST-3F.tug",
        .aircraft_mass_kg = 70000,
        .aircraft_tail_arm_m = 16.5,
        .effective_max_fwd_speed_mps = 1.11,
        .effective_max_rev_speed_mps = 1.11,
        .push_start_accel_ramp_time_s = 3,
        .push_start_jerk_limit_mps3 = 1.0 / 12.0,
        .turn_profile_transition_m = 10,
        .turn_profile_steer_rate_dps = 6,
        .route_path_deadband_deg = 1,
        .route_path_max_correction_deg = 12,
        .route_path_terminal_fade_m = 8,
        .tug_mass_kg = 13000,
        .tug_max_tractive_effort_n = 480000
    };
    bp_telemetry_sample_t sample = {
        .sim_time_s = 100,
        .step = 12,
        .step_name = "pushing",
        .segment_type = "turn",
        .segment_backward = 1,
        .turn_profile_progress_m = 2.8,
        .turn_profile_total_m = 50.7,
        .turn_profile_target_deg = -3.2,
        .controller_target_steer_deg = -4.1,
        .tail_feedback_steer_deg = -0.9,
        .tail_feedback_weight = 0.5,
        .route_path_correction_deg = 2.25,
        .route_path_weight = 0.75,
        .route_reference_x_m = 123.4,
        .route_reference_z_m = -56.7,
        .route_cross_track_m = 0.42,
        .route_heading_error_deg = -1.3,
        .tail_cross_track_m = 1.2,
        .tail_along_remaining_m = 25,
        .final_heading_error_deg = 4,
        .applied_steer_cmd_deg = -3.0,
        .aircraft_speed_mps = -0.1,
        .aircraft_accel_mps2 = -0.2,
        .applied_force_n = -12000,
        .force_limit_n = 480000
    };
    FILE *fp;
    char data[16384];
    size_t n;

    assert(argc == 2);
    assert(bp_telemetry_open(&telemetry, argv[1], &metadata, 10));
    assert(bp_telemetry_write(&telemetry, &sample, B_FALSE));
    sample.sim_time_s += 0.05;
    assert(!bp_telemetry_write(&telemetry, &sample, B_FALSE));
    sample.pause_requested = 1;
    assert(bp_telemetry_write(&telemetry, &sample, B_FALSE));
    sample.pause_held = 1;
    assert(bp_telemetry_write(&telemetry, &sample, B_FALSE));
    sample.sim_time_s += 0.10;
    sample.aircraft_accel_mps2 = -0.1;
    assert(bp_telemetry_write(&telemetry, &sample, B_FALSE));
    sample.sim_time_s += 0.01;
    sample.step = 13;
    sample.step_name = "stopping";
    assert(bp_telemetry_write(&telemetry, &sample, B_FALSE));
    bp_telemetry_close(&telemetry);

    fp = fopen(argv[1], "r");
    assert(fp != NULL);
    n = fread(data, 1, sizeof(data) - 1, fp);
    data[n] = '\0';
    fclose(fp);

    assert(strstr(data, "# schema_version,7") != NULL);
    assert(strstr(data, "# tug_name,\"AST-3F.tug\"") != NULL);
    assert(strstr(data, "# effective_max_rev_speed_mps,1.110000000") !=
        NULL);
    assert(strstr(data, "# push_start_accel_ramp_time_s,3.000000000") !=
        NULL);
    assert(strstr(data, "# push_start_jerk_limit_mps3,0.083333333") !=
        NULL);
    assert(strstr(data, "# turn_profile_transition_m,10.000000000") !=
        NULL);
    assert(strstr(data, "# route_path_deadband_deg,1.000000000") != NULL);
    assert(strstr(data, "# route_path_max_correction_deg,12.000000000") !=
        NULL);
    assert(strstr(data, "# route_path_terminal_fade_m,8.000000000") !=
        NULL);
    assert(strstr(data, "# aircraft_tail_arm_m,16.500000000") != NULL);
    assert(strstr(data, "\"test,aircraft.acf\"") != NULL);
    assert(strstr(data, "aircraft_jerk_mps3") != NULL);
    assert(strstr(data, "command_active,pause_requested,pause_held,") !=
        NULL);
    assert(strstr(data, "turn_profile_progress_m,turn_profile_total_m,"
        "turn_profile_target_deg,controller_target_steer_deg,"
        "tail_feedback_steer_deg,tail_feedback_weight,") != NULL);
    assert(strstr(data, "route_path_correction_deg,route_path_weight,"
        "route_reference_x_m,route_reference_z_m,route_cross_track_m,"
        "route_heading_error_deg") != NULL);
    assert(strstr(data, "tail_cross_track_m,tail_along_remaining_m,"
        "final_heading_error_deg") != NULL);
    assert(strstr(data, "\"pushing\",\"turn\",1") != NULL);
    assert(strstr(data, "\"stopping\",\"turn\",1") != NULL);

    return (0);
}
