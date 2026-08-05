#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "route_realign.h"

static int
near(double first, double second)
{
    return fabs(first - second) < 1e-9;
}

static void
test_cache_pose_limits(void)
{
    assert(route_cache_pose_matches(11.01, -0.2));
    assert(route_cache_pose_matches(15.0, 5.0));
    assert(!route_cache_pose_matches(15.01, 0.0));
    assert(!route_cache_pose_matches(0.0, 5.01));
    assert(!route_cache_pose_matches(NAN, 0.0));
}

static void
test_translation(void)
{
    double x, y;

    route_realign_point(100.0, 200.0, 100.0, 200.0,
        150.0, 250.0, 0.0, &x, &y);
    assert(near(x, 150.0));
    assert(near(y, 250.0));

    route_realign_point(110.0, 180.0, 100.0, 200.0,
        150.0, 250.0, 0.0, &x, &y);
    assert(near(x, 160.0));
    assert(near(y, 230.0));
}

static void
test_clockwise_rotation_and_heading_wrap(void)
{
    double x, y;
    double delta = route_realign_heading_delta(350.0, 10.0);

    assert(near(delta, 20.0));
    assert(near(route_realign_heading(350.0, delta), 10.0));
    assert(near(route_realign_heading(5.0, -10.0), 355.0));

    route_realign_point(0.0, 10.0, 0.0, 0.0,
        20.0, 30.0, 90.0, &x, &y);
    assert(near(x, 30.0));
    assert(near(y, 30.0));
}

static void
test_shared_segment_endpoint_stays_shared(void)
{
    double first_x, first_y, second_x, second_y;

    route_realign_point(4.0, 7.0, 1.0, 2.0,
        30.0, 40.0, -37.0, &first_x, &first_y);
    route_realign_point(4.0, 7.0, 1.0, 2.0,
        30.0, 40.0, -37.0, &second_x, &second_y);
    assert(near(first_x, second_x));
    assert(near(first_y, second_y));
}

int
main(void)
{
    test_cache_pose_limits();
    test_translation();
    test_clockwise_rotation_and_heading_wrap();
    test_shared_segment_endpoint_stays_shared();
    puts("route realignment tests passed");
    return 0;
}
