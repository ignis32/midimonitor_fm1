# MIDIMON for the M-VAVE FM-1

A MIDI monitor and thru router for the M-VAVE FM-1, in a green phosphor terminal's look. It shows every MIDI message
that comes in from USB, the TRS jack and Bluetooth, in hex, in words or both, and passes MIDI on between them.

**Status: early (0.1).** It runs on the FM-1. Expect changes.

## What it does

- **Four sources**, each its own: `USB`, `TRS` (the MIDI IN jack), `BLE-P` (a computer or tablet connected to the
  FM-1 over Bluetooth: the FM-1 is the peripheral) and `BLE-C` (a Bluetooth MIDI device the FM-1 connected to: the
  FM-1 is the central). Both Bluetooth roles at once.
- **Everything as it came**: SysEx (assembled, the maker named, its length), system common, clock and transport,
  active sensing, reset, running status (marked), and the faults: a stray byte, a message cut short, a SysEx without
  its end, messages lost to a full buffer.
- **LOG**: the last 4096 messages, HEX, HUMAN or BOTH, the time between lines or since power-on; pause, scroll back.
- **FILTER**: per source, per channel (white keys), per message type (black keys). Clock and active sensing hidden by
  default, as in MIDI-OX.
- **STATS**: messages and bytes per source, messages a second, what was lost and sent, Bluetooth's state, the tempo
  of a clock coming in.
- **THRU**: routes from each source to any of USB, BLE-P and BLE-C, each route with its own filter (channels and
  message types). The FM-1 as a Bluetooth MIDI bridge: a Bluetooth device to the computer over USB, a keyboard on
  the jack to an iPad, and so on. (SysEx goes to USB only, for now.)
- **GLO**: Bluetooth on or off, connecting to a Bluetooth MIDI device, C3 or C4 for note 60; a second page (GLO
  again) for the skin (green, amber, cyan, white, red) and whether the FM-1's own SysEx over USB (screen reader,
  updates) is logged.
- **Lights**: the source buttons flash as messages come in, the keys light with the notes held, PLAY blinks green on
  the beat of a clock coming in and turns red while paused or on an error.

## The panel

| Control | Does |
| --- | --- |
| PRESETS | the page: LOG, FILTER, STATS, THRU, GLO |
| GLO | the settings, page 1, page 2, then back |
| SELECT | LOG: scroll back (the view stops; back at the end it is live again). THRU: the source |
| PLAY | pause / go on (what comes meanwhile is still logged) |
| FX, SEL, ENV, LFO | show / hide USB, TRS, BLE-P, BLE-C |
| K1, K2, K3 | LOG: HEX / HUMAN / BOTH; the time; the scanlines. THRU: the source's route to USB, BLE-P, BLE-C |
| K4 | THRU: thru on / off |
| EDIT | THRU: a route's filter, and back. FILTER: the default filter |
| HOME | clear the log (asks) |
| SAVE | keep the settings (lit when something changed) |
| OCT- / OCT+ | a question's NO / YES. FILTER and a route's filter: nothing / everything |

## Installing

Install the stock firmware V15 first if the FM-1 runs another custom firmware, then MIDIMON, with the web
installer or `python3 tools/fm1_install.py midimon-X.Y.fwsc`. An update of MIDIMON over MIDIMON needs no V15. Its
settings live in the flash's data area, which V15 wipes; another firmware's settings at 0xFC000 are left alone.

Releases are built on GitHub by The Firmware Hub's shared workflow
([zvenson/fm1-build](https://github.com/zvenson/fm1-build)) and carry a build attestation. To check a file:

    gh attestation verify midimon-X.Y.fwsc --repo ignis32/midimonitor_fm1 \
        --signer-workflow zvenson/fm1-build/.github/workflows/build.yml

## Building

    sh build.sh               # app/build/midimon.fwsc (development, identity FM-1_8800)
    sh build.sh test          # the host simulator and the scripted tests (app/tests), screens in app/build/host
    sh build.sh release       # app/build/midimon-VERSION.fwsc (identity FM-1_88XXYYZZ)

It needs JieLi's Linux toolchain and the AC79 SDK (AC79NN_SDK_V1.2.1, commit d179b44): `JIELI_TOOLCHAIN` and
`AC79_SDK`, or a `local.env` beside `build.sh` that sets them. A release is a tag `vX.Y` matching `VERSION`; the
workflow (`.github/workflows/release.yml`) builds, tests, attests and publishes it.

## Based on fm1-base

This repository starts from [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal, commit
`71128a3` (2026-10-21), with [felucca-bsp](https://github.com/hugelton/felucca-bsp) commit `33cac69` in `bsp/`. Their
code was copied in and is developed here (it is not a submodule): MIDIMON's changes to the platform are the MIDI tap
(`device/midi_tap.c`: every byte from every source, stamped), the two Bluetooth links addressed apart
(`TO_BLE_P`, `TO_BLE_C`) and the host simulator's `raw` command. fm1-base's files keep their licence (MPL-2.0) and
their notices: see [LICENSING.md](LICENSING.md) and [CREDITS.md](CREDITS.md). Only what MIDIMON uses was kept (not
fm1-base's template, examples, web pages, backup / mirror / site tools or its own tests); fm1-base's documentation
is upstream.

## Licence

MPL-2.0, as fm1-base (`LICENSE`). A build with Bluetooth links JieLi's binary Bluetooth libraries from the AC79 SDK
(Apache-2.0), taken from the SDK at build time and never kept in this repository.
