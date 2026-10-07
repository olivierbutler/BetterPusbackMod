#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-ext-api-route-test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/src" \
    "$repo_dir/src/ext_api_route.c" \
    "$test_dir/ext_api_route_test.c" \
    -lm -o "$test_bin"

"$test_bin"
rm -f "$test_bin"
