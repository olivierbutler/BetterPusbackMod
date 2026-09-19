#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/src" -I"$repo_root/../libacfutils/src" \
    "$repo_root/tests/ground_ops_data_test.c" \
    "$repo_root/tests/intl_test_stub.c" \
    "$repo_root/src/ground_ops_data.c" -lm \
    -o "$tmpdir/ground_ops_data_test"
"$tmpdir/ground_ops_data_test"
