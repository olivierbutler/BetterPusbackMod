#ifndef GATE_ROUTE_MATH_H
#define GATE_ROUTE_MATH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GATE_ROUTE_START_POSITION_TOLERANCE_METERS 1.0
#define GATE_ROUTE_START_HEADING_TOLERANCE_DEGREES 1.0

bool gate_route_start_pose_matches(double distance_m,
    double heading_delta_deg);

double gate_route_heading_delta(double reference_heading_deg,
    double heading_deg);

double gate_route_heading_from_delta(double reference_heading_deg,
    double heading_delta_deg);

void gate_route_point_to_relative(double point_x, double point_y,
    double anchor_x, double anchor_y, double frame_heading_deg,
    double *relative_x, double *relative_y);

void gate_route_point_from_relative(double relative_x, double relative_y,
    double anchor_x, double anchor_y, double frame_heading_deg,
    double *point_x, double *point_y);

#ifdef __cplusplus
}
#endif

#endif
