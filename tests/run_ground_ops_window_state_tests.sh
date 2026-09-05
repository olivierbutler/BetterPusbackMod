#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-ground-ops-window-state-test"
mac_scale_test_bin="${TMPDIR:-/tmp}/betterpushback-ground-ops-window-state-mac-scale-test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/src" \
    "$repo_dir/src/ground_ops_window_state.c" \
    "$test_dir/ground_ops_window_state_test.c" \
    -o "$test_bin"

"$test_bin"

cc -std=c99 -Wall -Wextra -Werror \
    -DBP_EMULATE_MAC_UI_SCALE=1 \
    -I"$repo_dir/src" \
    "$repo_dir/src/ground_ops_window_state.c" \
    "$test_dir/ground_ops_window_state_test.c" \
    -o "$mac_scale_test_bin"

"$mac_scale_test_bin"
rm -f "$test_bin" "$mac_scale_test_bin"
