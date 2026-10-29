#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# The yoga firmware's tests: the instrument on the host (tests/yoga_test.c), then the whole app in fm1-base's host
# simulator (tests/scenarios/*.txt), screenshots and audio into app/build/scenarios/.
set -e
cd "$(dirname "$0")/.."
mkdir -p build app/build/scenarios
cc -std=c11 -O2 -Wall -Wextra -fwrapv -ffp-contract=off -DPLAT_HOST -D_DEFAULT_SOURCE \
    -Ibase/include -Iapp -Iapp/dsp tests/yoga_test.c -lm -o build/yoga_test
build/yoga_test test
./build.sh --host >/dev/null
for s in tests/scenarios/*.txt; do
    echo "scenario $s"
    app/build/host/yoga_host "$s" app/build/scenarios
done
