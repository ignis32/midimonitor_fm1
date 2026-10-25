#!/bin/sh
# MIDIMON's builds and tests, one command each, on fm1-base's build (fm1-base/: a pinned git submodule, felucca-bsp in
# fm1-base/bsp; MIDIMON's changes to them in patches/, applied first: patches/README.md):
#   sh build.sh            the firmware: app/build/midimon.fwsc
#   sh build.sh test       the host simulator, then every script in app/tests (screens in app/build/host)
#   sh build.sh screens    the README's screenshots, from docs/screens.txt into docs/
#   sh build.sh release    the release package of VERSION (this file's neighbour): app/build/midimon-V.fwsc,
#                          identity FM-1_88XXYYZZ (a version may be given: sh build.sh release 0.2)
#   sh build.sh version    print VERSION
#   sh build.sh web        the browser emulator: app/build/emu
# The toolchain and the SDK: JIELI_TOOLCHAIN and AC79_SDK, or a local.env beside this file (not in git) that sets them.
set -e
cd "$(dirname "$0")"
[ -f local.env ] && . ./local.env
export JIELI_TOOLCHAIN AC79_SDK
export X0X_FONTSET=terminus              # the screen's faces (fm1-base/tools/gen_font.py)
[ "$1" = version ] && { cat VERSION; exit 0; }
[ -f fm1-base/bsp/chip/usb.c ] || git submodule update --init --recursive
sh tools/patches.sh apply
B="python3 fm1-base/tools/build.py app"
case "$1" in
test)
    $B --host
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
screens)
    $B --host
    app/build/host/midimon_host docs/screens.txt docs
    ;;
release)
    $B --release "${2:-$(cat VERSION)}"
    ;;
web)
    $B --web
    ;;
*)
    $B "$@"
    ;;
esac
