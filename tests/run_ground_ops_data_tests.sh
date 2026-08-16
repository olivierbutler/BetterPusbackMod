#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror -Isrc \
    "$repo_root/tests/ground_ops_data_test.c" \
    "$repo_root/src/ground_ops_data.c" -lm \
    -o "$tmpdir/ground_ops_data_test"
"$tmpdir/ground_ops_data_test"
