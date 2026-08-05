#include "route_realign.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double
normalize_heading(double heading_deg)
{
    heading_deg = fmod(heading_deg, 360.0);
    if (heading_deg < 0.0)
        heading_deg += 360.0;
    return heading_deg;
}

bool
route_cache_pose_matches(double distance_m, double heading_delta_deg)
{
    return isfinite(distance_m) && isfinite(heading_delta_deg) &&
        distance_m <= ROUTE_CACHE_DISTANCE_LIMIT_METERS &&
        fabs(heading_delta_deg) <= ROUTE_CACHE_HEADING_LIMIT_DEGREES;
}

double
route_realign_heading_delta(double saved_heading_deg,
    double current_heading_deg)
{
    double delta = normalize_heading(current_heading_deg) -
        normalize_heading(saved_heading_deg);

    if (delta > 180.0)
        delta -= 360.0;
    else if (delta < -180.0)
        delta += 360.0;
    return delta;
}

double
route_realign_heading(double heading_deg, double heading_delta_deg)
{
    return normalize_heading(heading_deg + heading_delta_deg);
}

void
route_realign_point(double saved_x, double saved_y,
    double saved_origin_x, double saved_origin_y,
    double current_origin_x, double current_origin_y,
    double heading_delta_deg, double *current_x, double *current_y)
{
    const double angle = heading_delta_deg * M_PI / 180.0;
    const double cosine = cos(angle);
    const double sine = sin(angle);
    const double x = saved_x - saved_origin_x;
    const double y = saved_y - saved_origin_y;

    /* x is east, y is north; positive heading changes rotate clockwise. */
    *current_x = current_origin_x + x * cosine + y * sine;
    *current_y = current_origin_y - x * sine + y * cosine;
}
