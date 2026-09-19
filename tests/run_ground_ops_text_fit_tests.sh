#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$test_dir/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
cc -std=c99 -Wall -Wextra -Werror -I"$repo_dir/src" \
    -I"$repo_dir/../libacfutils/src" -c \
    "$repo_dir/src/ground_ops_state.c" -o "$test_root/state.o"
cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_dir/../libacfutils/src" -c \
    "$test_dir/intl_test_stub.c" -o "$test_root/intl_test_stub.o"
c++ -std=c++11 -O1 -Wall -Wextra -Werror -I"$repo_dir/src" -I"$repo_dir/src/imgui" \
    "$test_dir/ground_ops_text_fit_test.cpp" "$test_root/state.o" \
    "$test_root/intl_test_stub.o" \
    "$repo_dir/src/imgui/imgui.cpp" "$repo_dir/src/imgui/imgui_draw.cpp" \
    "$repo_dir/src/imgui/imgui_widgets.cpp" "$repo_dir/src/imgui/imgui_tables.cpp" \
    -o "$test_root/text_fit"
"$test_root/text_fit" "$@"
