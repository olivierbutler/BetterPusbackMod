#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
c++ -std=c++11 -O1 -Wall -Wextra -Werror -DLIN=1 -DXPLM200=1 -DXPLM400=1 \
    -I"$repo_dir/src" -I"$repo_dir/../libacfutils/src" \
    -I"$repo_dir/../libacfutils/SDK/CHeaders/XPLM" \
    "$test_dir/ui_click_sound_test.cpp" "$repo_dir/src/ui_click_sound.cpp" \
    -o "$test_root/ui_click"
"$test_root/ui_click"
