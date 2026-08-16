#include <assert.h>
#include <math.h>

#include "vehicle_physics.h"

#define CLOSE(a, b) (fabs((a) - (b)) < 1e-6)

int
main(void)
{
    double left_radius = vehicle_turn_radius(2.345, -60);
    double right_radius = vehicle_turn_radius(2.345, 60);
    double accel = 0;
    double speed = 0;

    assert(CLOSE(fabs(left_radius), fabs(right_radius)));
    assert(left_radius < 0 && right_radius > 0);
    assert(CLOSE(right_radius, 2.345 / tan(acos(-1) / 3)));

    assert(CLOSE(vehicle_steering_speed_limit(6, 6, 3, -60, 60),
        vehicle_steering_speed_limit(6, 6, 3, 60, 60)));
    assert(CLOSE(vehicle_steering_speed_limit(-3, 6, 3, -60, 60),
        vehicle_steering_speed_limit(-3, 6, 3, 60, 60)));

    assert(CLOSE(vehicle_speed_step(0, 2, 1, 0.5, 1), 1));
    assert(CLOSE(vehicle_speed_step(2, 0, 1, 0.5, 1), 1.5));
    assert(CLOSE(vehicle_speed_step(0.25, -2, 1, 0.5, 1), 0));
    assert(CLOSE(vehicle_speed_step(0, -2, 1, 0.5, 1), -1));

    assert(CLOSE(vehicle_accel_step(0, 0.25, 0.125, 1), 0.125));
    assert(CLOSE(vehicle_accel_step(0.125, 0.25, 0.125, 1), 0.25));
    assert(CLOSE(vehicle_accel_step(0.25, 0, 0.125, 1), 0.125));
    assert(CLOSE(vehicle_accel_step(0.1, 0.25, 0.125, 0), 0.1));
    for (unsigned i = 0; i < 200; i++) {
        double next_accel = vehicle_accel_step(accel, 0.25, 0.125,
            0.01);
        assert(next_accel - accel <= 0.00125 + 1e-9);
        accel = next_accel;
        speed += accel * 0.01;
    }
    assert(CLOSE(accel, 0.25));
    assert(fabs(speed - 0.25125) < 1e-6);

    assert(CLOSE(vehicle_steering_step(0, -20, 6, 0.5), -3));
    assert(CLOSE(vehicle_steering_step(-19, -20, 6, 0.5), -20));
    assert(CLOSE(vehicle_steering_step(-3, 0, 6, 0.5), 0));

    {
        double x = 0, z = 0, heading = 0;

        vehicle_bicycle_step(10, 2, 0, 5, &x, &z, &heading);
        assert(CLOSE(x, 0));
        assert(CLOSE(z, 10));
        assert(CLOSE(heading, 0));

        x = 0;
        z = 0;
        heading = 0;
        vehicle_bicycle_step(10, 2, 45, 5 * acos(-1) / 2,
            &x, &z, &heading);
        assert(fabs(x - 10) < 1e-6);
        assert(fabs(z - 10) < 1e-6);
        assert(fabs(heading - 90) < 1e-6);

        x = 0;
        z = 0;
        heading = 0;
        vehicle_bicycle_step(10, -2, -45, 5 * acos(-1) / 2,
            &x, &z, &heading);
        assert(fabs(x + 10) < 1e-6);
        assert(fabs(z + 10) < 1e-6);
        assert(fabs(heading - 90) < 1e-6);
    }

    {
        const double wheelbase = 12.688960195;
        const double radius = 32.267953946;
        const double total_distance = radius * acos(-1) / 2;
        const double transition_distance = 8;
        const unsigned steps = 100000;
        const double ds = total_distance / steps;
        double integrated_curvature = 0;
        double peak;

        assert(CLOSE(vehicle_turn_profile_steer(wheelbase, radius,
            total_distance, 0, transition_distance, -1, 50), 0));
        assert(CLOSE(vehicle_turn_profile_steer(wheelbase, radius,
            total_distance, total_distance, transition_distance, -1,
            50), 0));
        assert(CLOSE(vehicle_turn_profile_steer(wheelbase, radius,
            total_distance, 2, transition_distance, -1, 50),
            vehicle_turn_profile_steer(wheelbase, radius, total_distance,
            total_distance - 2, transition_distance, -1, 50)));

        peak = vehicle_turn_profile_steer(wheelbase, radius,
            total_distance, total_distance / 2, transition_distance, -1,
            50);
        assert(peak < 0);
        assert(fabs(peak) > atan(wheelbase / radius) * 180 / acos(-1));

        for (unsigned i = 0; i < steps; i++) {
            double distance = (i + 0.5) * ds;
            double steer = vehicle_turn_profile_steer(wheelbase, radius,
                total_distance, distance, transition_distance, -1, 50);
            integrated_curvature += tan(fabs(steer) * acos(-1) / 180) /
                wheelbase * ds;
        }
        assert(fabs(integrated_curvature - total_distance / radius) <
            1e-6);
        assert(fabs(vehicle_turn_profile_steer(wheelbase, 1,
            total_distance, total_distance / 2, transition_distance, 1,
            35)) <= 35);
    }

    assert(CLOSE(vehicle_tail_steer_correction(4, 6, 0, 0.5, 0.35,
        8, 1), 0));
    assert(vehicle_tail_steer_correction(4, 6, 1, 0.5, 0.35, 8, 1) < 0);
    assert(vehicle_tail_steer_correction(4, 6, 1, 0.5, 0.35, 8, 0) > 0);
    assert(CLOSE(vehicle_tail_steer_correction(100, 100, 1, 0.5,
        0.35, 8, 1), -8));
    assert(CLOSE(vehicle_tail_steer_correction(-100, -100, 1, 0.5,
        0.35, 8, 1), 8));

    assert(CLOSE(vehicle_path_steer_correction(50, 38, 1, 12, 1),
        11));
    assert(CLOSE(vehicle_path_steer_correction(-50, 50, 1, 12, 1),
        -12));
    assert(CLOSE(vehicle_path_steer_correction(38.5, 38, 1, 12, 1),
        0));
    assert(CLOSE(vehicle_path_steer_correction(50, 38, 1, 12, 0.5),
        5.5));
    assert(CLOSE(vehicle_path_steer_correction(NAN, 0, 1, 12, 1), 0));

    assert(CLOSE(vehicle_path_terminal_weight(100, 8, 0), 1));
    assert(CLOSE(vehicle_path_terminal_weight(8, 8, 1), 1));
    assert(CLOSE(vehicle_path_terminal_weight(4, 8, 1), 0.5));
    assert(CLOSE(vehicle_path_terminal_weight(0, 8, 1), 0));
    assert(CLOSE(vehicle_path_terminal_weight(-1, 8, 1), 0));

    assert(CLOSE(vehicle_force_limit(500000, 212000), 212000));
    assert(CLOSE(vehicle_force_limit(150000, 212000), 150000));

    return (0);
}
