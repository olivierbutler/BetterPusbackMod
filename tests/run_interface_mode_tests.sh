#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-interface-mode-test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/src" \
    "$repo_dir/src/interface_mode.c" \
    "$test_dir/interface_mode_test.c" \
    -o "$test_bin"

"$test_bin"
rm -f "$test_bin"
