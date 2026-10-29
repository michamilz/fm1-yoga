#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Build the yoga firmware with fm1-base (base/, a git submodule).
#   ./build.sh [--release X.Y] [--host | --web]
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK; three of its files go in the package)
# On macOS the toolchain runs in a linux/amd64 container: podman, through the docker shim in tools/podman-bin.
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
command -v docker >/dev/null 2>&1 && docker --version 2>/dev/null | grep -qi podman || PATH="$PWD/tools/podman-bin:$PATH"
export PATH
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
exec "${PYTHON:-python3}" base/tools/build.py app "$@"
