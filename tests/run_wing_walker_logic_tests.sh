#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror -I"$repo_dir/src" \
    "$repo_dir/src/wing_walker_logic.c" \
    "$test_dir/wing_walker_logic_test.c" \
    -o "$build_dir/wing_walker_logic_test"

"$build_dir/wing_walker_logic_test"
