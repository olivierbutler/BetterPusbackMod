#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "gate_route_math.h"

static int
near(double first, double second)
{
    return fabs(first - second) < 1e-9;
}

static void
test_published_start_guardrail(void)
{
    assert(gate_route_start_pose_matches(0.0, 0.0));
    assert(gate_route_start_pose_matches(1.0, -1.0));
    assert(!gate_route_start_pose_matches(1.0001, 0.0));
    assert(!gate_route_start_pose_matches(0.0, 1.0001));
    assert(!gate_route_start_pose_matches(NAN, 0.0));
}

static void
test_heading_delta(void)
{
    assert(near(gate_route_heading_delta(350.0, 10.0), 20.0));
    assert(near(gate_route_heading_delta(10.0, 350.0), -20.0));
    assert(near(gate_route_heading_from_delta(350.0, 20.0), 10.0));
}

static void
test_anchor_relative_round_trip(void)
{
    double relative_x, relative_y, restored_x, restored_y;

    gate_route_point_to_relative(1234.5, -987.25, 1000.0, -900.0,
        269.2, &relative_x, &relative_y);
    gate_route_point_from_relative(relative_x, relative_y, 1000.0, -900.0,
        269.2, &restored_x, &restored_y);
    assert(near(restored_x, 1234.5));
    assert(near(restored_y, -987.25));
}

static void
test_nosewheel_anchor_reconstructs_main_gear(void)
{
    const double anchor_x = 4120.0;
    const double anchor_y = -735.0;
    const double heading = 269.2;
    const double wheelbase = 12.688960195;
    const double angle = heading * 3.14159265358979323846 / 180.0;
    double main_x, main_y, nose_x, nose_y;

    gate_route_point_from_relative(0.0, -wheelbase, anchor_x, anchor_y,
        heading, &main_x, &main_y);
    nose_x = main_x + sin(angle) * wheelbase;
    nose_y = main_y + cos(angle) * wheelbase;
    assert(near(nose_x, anchor_x));
    assert(near(nose_y, anchor_y));
}

static void
test_same_offsets_rebuild_at_new_local_origin(void)
{
    double saved_relative_x, saved_relative_y;
    double rebuilt_x, rebuilt_y;

    gate_route_point_to_relative(110.0, 170.0, 100.0, 200.0, 90.0,
        &saved_relative_x, &saved_relative_y);
    gate_route_point_from_relative(saved_relative_x, saved_relative_y,
        5000.0, -3000.0, 90.0, &rebuilt_x, &rebuilt_y);
    assert(near(rebuilt_x, 5010.0));
    assert(near(rebuilt_y, -3030.0));
}

int
main(void)
{
    test_published_start_guardrail();
    test_heading_delta();
    test_anchor_relative_round_trip();
    test_nosewheel_anchor_reconstructs_main_gear();
    test_same_offsets_rebuild_at_new_local_origin();
    puts("gate route math tests passed");
    return 0;
}
