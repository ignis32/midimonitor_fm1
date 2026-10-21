# MIDIMON for the M-VAVE FM-1

A MIDI monitor, thru router and clock generator for the FM-1: MIDI from USB, the TRS jack and Bluetooth (the FM-1 as a
peripheral and as a central) on the screen, in hex or in words, filtered by source, type and channel.

**Status: just started.** `app/` is still fm1-base's template.

## Based on fm1-base

This repository starts from [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal, commit
`71128a3` (2026-10-21), with [felucca-bsp](https://github.com/hugelton/felucca-bsp) commit `33cac69` in `bsp/`. Their
code was copied in and is developed here as our own; it is not a submodule. fm1-base's files keep their licence
(MPL-2.0) and their notices: see [LICENSING.md](LICENSING.md) and [CREDITS.md](CREDITS.md). fm1-base's own README is
kept in [docs/fm1-base-README.md](docs/fm1-base-README.md).

## Build

    tools/build.py app                # app/build/midimon.fwsc (JieLi toolchain, AC79 SDK)
    tools/build.py app --host         # the host simulator
