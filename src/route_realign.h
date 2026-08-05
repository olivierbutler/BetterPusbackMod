#ifndef ROUTE_REALIGN_H
#define ROUTE_REALIGN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A cached route is identified by its starting aircraft pose.  Fifteen
 * metres accepts the legacy Gate 8 cache (11 m from the current WED start)
 * while avoiding the original 30 m overlap between nearby ramp starts.
 */
#define ROUTE_CACHE_DISTANCE_LIMIT_METERS 15.0
#define ROUTE_CACHE_HEADING_LIMIT_DEGREES 5.0

bool route_cache_pose_matches(double distance_m, double heading_delta_deg);

double route_realign_heading_delta(double saved_heading_deg,
    double current_heading_deg);

double route_realign_heading(double heading_deg, double heading_delta_deg);

void route_realign_point(double saved_x, double saved_y,
    double saved_origin_x, double saved_origin_y,
    double current_origin_x, double current_origin_y,
    double heading_delta_deg, double *current_x, double *current_y);

#ifdef __cplusplus
}
#endif

#endif
