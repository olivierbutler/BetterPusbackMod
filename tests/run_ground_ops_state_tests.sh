#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-ground-ops-state-test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/src" \
    "$repo_dir/src/ground_ops_state.c" \
    "$test_dir/ground_ops_state_test.c" \
    -o "$test_bin"

"$test_bin"
rm -f "$test_bin"
