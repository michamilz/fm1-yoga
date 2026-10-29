#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Release VERSION and publish the site (web installer, emulator, download) to this repository's gh-pages branch:
#   tools/publish.sh 1.0
# fm1-base's tools/publish_site.sh, with the toolchain's paths and the podman shim as ./build.sh sets them.
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/publish.sh VERSION}"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
command -v docker >/dev/null 2>&1 && docker --version 2>/dev/null | grep -qi podman || PATH="$PWD/tools/podman-bin:$PATH"
export PATH
python3 tools/gen_docs_figures.py >/dev/null 2>&1 || true
cp docs/img/panel.png app/site/img/panel.png
exec sh base/tools/publish_site.sh app "$V"
