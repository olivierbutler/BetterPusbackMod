#include "gate_route_math.h"

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
gate_route_start_pose_matches(double distance_m, double heading_delta_deg)
{
    return isfinite(distance_m) && isfinite(heading_delta_deg) &&
        distance_m <= GATE_ROUTE_START_POSITION_TOLERANCE_METERS &&
        fabs(heading_delta_deg) <=
        GATE_ROUTE_START_HEADING_TOLERANCE_DEGREES;
}

double
gate_route_heading_delta(double reference_heading_deg, double heading_deg)
{
    double delta = normalize_heading(heading_deg) -
        normalize_heading(reference_heading_deg);

    if (delta > 180.0)
        delta -= 360.0;
    else if (delta < -180.0)
        delta += 360.0;
    return delta;
}

double
gate_route_heading_from_delta(double reference_heading_deg,
    double heading_delta_deg)
{
    return normalize_heading(reference_heading_deg + heading_delta_deg);
}

void
gate_route_point_to_relative(double point_x, double point_y,
    double anchor_x, double anchor_y, double frame_heading_deg,
    double *relative_x, double *relative_y)
{
    const double angle = frame_heading_deg * M_PI / 180.0;
    const double cosine = cos(angle);
    const double sine = sin(angle);
    const double x = point_x - anchor_x;
    const double y = point_y - anchor_y;

    *relative_x = x * cosine - y * sine;
    *relative_y = x * sine + y * cosine;
}

void
gate_route_point_from_relative(double relative_x, double relative_y,
    double anchor_x, double anchor_y, double frame_heading_deg,
    double *point_x, double *point_y)
{
    const double angle = frame_heading_deg * M_PI / 180.0;
    const double cosine = cos(angle);
    const double sine = sin(angle);

    *point_x = anchor_x + relative_x * cosine + relative_y * sine;
    *point_y = anchor_y - relative_x * sine + relative_y * cosine;
}
