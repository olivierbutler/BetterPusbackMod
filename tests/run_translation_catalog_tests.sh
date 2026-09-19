#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
libacfutils_dir=$(CDPATH= cd -- "$repo_dir/../libacfutils" && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror \
    -D_GNU_SOURCE -DAPL=0 -DIBM=0 -DLIN=1 \
    -DXPLM200=1 -DXPLM300=1 -DXPLM400=1 \
    -I"$repo_dir/src" \
    -I"$libacfutils_dir/src" \
    -I"$libacfutils_dir/SDK/CHeaders/XPLM" \
    "$test_dir/translation_catalog_test.c" \
    "$libacfutils_dir/src/intl.c" "$libacfutils_dir/src/avl.c" \
    -o "$test_root/translation_catalog_test"

"$test_root/translation_catalog_test" "$repo_dir"
