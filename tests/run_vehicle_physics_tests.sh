#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-vehicle-physics-test"
telemetry_bin="${TMPDIR:-/tmp}/betterpushback-telemetry-test"
telemetry_csv=$(mktemp)
trap 'rm -f "$telemetry_csv"' EXIT

cc -std=c99 -Wall -Wextra -Werror -I"$repo_dir/src" \
    "$repo_dir/src/vehicle_physics.c" "$test_dir/vehicle_physics_test.c" \
    -lm -o "$test_bin"
"$test_bin"

cc -std=c99 -Wall -Wextra -Werror -I"$repo_dir/src" \
    -DBP_ENABLE_RUNTIME_TELEMETRY=1 \
    -I"$repo_dir/../libacfutils/src" \
    "$repo_dir/src/telemetry.c" "$test_dir/telemetry_test.c" \
    -lm -o "$telemetry_bin"
"$telemetry_bin" "$telemetry_csv"
