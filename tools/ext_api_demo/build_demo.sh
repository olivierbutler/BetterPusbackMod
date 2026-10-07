#!/bin/sh
# Builds the BetterPushback API demo plugin (tools/ext_api_demo) for Windows
# and Linux, in the same container as the plugin (README-docker.md):
#   docker run --rm -v "$PWD/..:/xpl_dev" cross-m-w-l:latest \
#       sh -c "cd BetterPusbackMod && sh tools/ext_api_demo/build_demo.sh"
# Output: tools/ext_api_demo/BPExtDemo/{win_x64,lin_x64}/BPExtDemo.xpl
# Install by copying the BPExtDemo folder to X-Plane/Resources/plugins.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk="$here/../../../libacfutils/SDK"
out="$here/BPExtDemo"
flags="-std=c99 -O2 -Wall -Wextra -Werror -fvisibility=hidden
    -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM303=1
    -I$sdk/CHeaders/XPLM"

mkdir -p "$out/win_x64" "$out/lin_x64"
x86_64-w64-mingw32-gcc $flags -DIBM=1 -shared -static-libgcc \
    -o "$out/win_x64/BPExtDemo.xpl" "$here/ext_api_demo.c" \
    "$sdk/Libraries/Win/XPLM_64.lib" -lm
gcc $flags -DLIN=1 -fPIC -shared \
    -o "$out/lin_x64/BPExtDemo.xpl" "$here/ext_api_demo.c" -lm
ls -l "$out/win_x64/BPExtDemo.xpl" "$out/lin_x64/BPExtDemo.xpl"
