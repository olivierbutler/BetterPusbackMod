#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

for runner in \
    run_emergency_tow_tests.sh \
    run_gate_route_math_tests.sh \
    run_gate_route_slots_tests.sh \
    run_ground_ops_data_tests.sh \
    run_ground_ops_state_tests.sh \
    run_ground_ops_window_state_tests.sh \
    run_planner_cache_tests.sh \
    run_vehicle_physics_tests.sh \
    run_wing_walker_logic_tests.sh
do
    printf 'Running %s\n' "$runner"
    sh "$test_dir/$runner"
done

printf 'Running wing_walker_asset_test.py\n'
python3 "$test_dir/wing_walker_asset_test.py"

printf 'Running runtime telemetry disabled test\n'
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
telemetry_test_dir=$(mktemp -d)
trap 'rm -rf "$telemetry_test_dir"' EXIT HUP INT TERM
cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/src" -I"$repo_dir/../libacfutils/src" \
    "$repo_dir/src/telemetry.c" "$test_dir/telemetry_disabled_test.c" \
    -lm -o "$telemetry_test_dir/telemetry_disabled_test"
"$telemetry_test_dir/telemetry_disabled_test" \
    "$telemetry_test_dir/must_not_exist.csv"

printf 'All BetterPushback regression tests passed.\n'
