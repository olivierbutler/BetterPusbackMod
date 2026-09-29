#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_bin="${TMPDIR:-/tmp}/betterpushback-airport-cache-manifest-test"

cc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_dir/src" \
    "$repo_dir/src/airport_cache_manifest.c" \
    "$test_dir/airport_cache_manifest_test.c" \
    -o "$test_bin"

"$test_bin"
rm -f "$test_bin"
