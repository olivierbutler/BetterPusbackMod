#!/bin/bash

# CDDL HEADER START
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#
# CDDL HEADER END

# Copyright 2022 Saso Kiselkov. All rights reserved.

#########################################################################
# Set here the correct path
# docker's interval path /xpl_dev is mapped one level up the current folder
# the libacfutils folder is expected to be at the same level of this project, if not
# modify docket-compose.yml accordingly.

# Host folders         | Internal docker folders
# ---------------------|---------------------
# ../projects/          | /xpl_dev/
# ├─ libacfutils/      | ├── libacfutils/
# └─ BetterPusbackMod/ | └── BetterPusbackMod/

PROJECT_PATH='BetterPusbackMod'
#########################################################################



CMAKE_OPTS_COMMON="-DCMAKE_BUILD_TYPE=Release"



while getopts "nfh" o; do
	case "${o}" in
	f)
	if [[ $(uname) = "Darwin" ]]; then
		full=1
	else 
		echo "Option ignored when compiling from linux host"
	fi
	;;	
	n)
		if [[ $(uname) != "Darwin" ]]; then
			echo "Codesigning and notarization can only be done" \
			    "on macOS" >&2
			exit 1
		fi
		notarize=1
		;;
	h)
		cat << EOF
Usage: $0 [-fnh]
	-f : (macOS-only) Cross-compile to linux and window
		using the docker image provided by docker-compose.yml.
		See README-docker.md for more information.
    -n : (macOS-only) Codesign & notarize the resulting XPL after build
         Note: requires that you create a file named user.make in the
         notarize directory with DEVELOPER_USERNAME and DEVELOPER_PASSWORD
         set. See notarize/notarize.make for more information.
    -h : show this help screen and exit.
EOF
		exit
		;;
	*)
		echo "Unknown option $o. Try $0 -h for help." >&2
		exit 1
		;;
	esac
done


set -e

# We'll try to build on N+1 CPUs we have available. The extra +1 is to allow
# for one make instance to be blocking on disk.
HOST_OS="$(uname)"
if [[ "$HOST_OS" = "Darwin" ]]; then
	NCPUS=$(( $(sysctl -n hw.ncpu) + 1 ))
else
	NCPUS=$(( $(grep 'processor[[:space:]]\+:' /proc/cpuinfo  | wc -l) + \
	    1 ))
fi

case `uname` in
Linux)
	( cd src && rm -f CMakeCache.txt && \
	    cmake $CMAKE_OPTS_COMMON -DCMAKE_TOOLCHAIN_FILE=XCompile.cmake \
	    -DHOST=x86_64-w64-mingw32 . && make clean && make -j${NCPUS} &&
	    rm -f libBetterPushback.xpl.dll.a )
	( cd src && rm -f CMakeCache.txt && cmake $CMAKE_OPTS_COMMON . \
	    && make clean && make -j${NCPUS} )
	strip {win_x64,lin_x64}/BetterPushback.xpl
	cp -r {win_x64,lin_x64} BetterPushback
	;;
Darwin)
	( cd src && rm -f CMakeCache.txt && cmake $CMAKE_OPTS_COMMON . \
	    && make clean && make -j${NCPUS} )
	if [ -n "$notarize" ]; then
		make -f notarize/notarize.make notarize
	fi
	strip -x mac_x64/BetterPushback.xpl
	cp -r mac_x64 BetterPushback
	;;
*)
	echo "Unsupported platform" >&2
	exit 1
	;;
esac

set +e


if [ "$full" ]; then
	docker compose run --rm win-lin-build bash -c 'cd '${PROJECT_PATH}' && ./build_xpl.sh "$@"' -- "$@"
fi
