#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror -Isrc \
    "$repo_root/tests/emergency_tow_test.c" \
    "$repo_root/src/emergency_tow.c" \
    -o "$tmpdir/emergency_tow_test"
"$tmpdir/emergency_tow_test"
