#!/bin/sh
# MIDIMON's builds and tests, one command each:
#   sh build.sh            the firmware: app/build/midimon.fwsc
#   sh build.sh test       the host simulator, then every script in app/tests (screens in app/build/host)
#   sh build.sh release V  the release package, identity FM-1_88XXYYZZ
#   sh build.sh web        the browser emulator: app/build/emu
# The toolchain and the SDK: JIELI_TOOLCHAIN and AC79_SDK, or a local.env beside this file (not in git) that sets them.
set -e
cd "$(dirname "$0")"
[ -f local.env ] && . ./local.env
export JIELI_TOOLCHAIN AC79_SDK
case "$1" in
test)
    python3 tools/build.py app --host
    fail=0
    for t in app/tests/*.txt; do
        if app/build/host/midimon_host "$t" app/build/host > "app/build/host/$(basename "$t" .txt).log" 2>&1; then
            echo "ok    $t"
        else
            echo "FAIL  $t"
            grep -E "FAIL|said!" "app/build/host/$(basename "$t" .txt).log" | head -20
            fail=1
        fi
    done
    exit $fail
    ;;
release)
    python3 tools/build.py app --release "$2"
    ;;
web)
    python3 tools/build.py app --web
    ;;
*)
    python3 tools/build.py app "$@"
    ;;
esac
