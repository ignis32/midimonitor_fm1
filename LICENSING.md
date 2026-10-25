# Licensing

**MIDIMON is MPL-2.0** (full text in `LICENSE`): its app (`app/`), its tools and its patches to the platform
(`patches/`, which change MPL-2.0 files and are MPL-2.0 as they are).

## The platform (submodules, as upstream has them)

| Code | Where | Licence |
| --- | --- | --- |
| [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal | `fm1-base/` | MPL-2.0, file by file (its `LICENSING.md`) |
| [felucca-bsp](https://github.com/hugelton/felucca-bsp) by Leo Kuroshita | `fm1-base/bsp/` | MPL-2.0 |

fm1-base's `LICENSING.md` says where its code comes from: Leo Kuroshita's Felucca code used under MPL-2.0 with his
permission ([hugelton/felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)), Charles Vestal's own, and
the parts of `fm1-base/ble/` that repeat the JieLi AC79 SDK (Apache-2.0, `MPL-2.0 AND Apache-2.0` in their headers).

## The USB MIDI host (patches/felucca-bsp/0001-usb-midi-host-pr7.patch)

This patch is felucca-bsp's own pull request [#7](https://github.com/hugelton/felucca-bsp/pull/7), unchanged (open
when taken; once merged, the patch goes). It adds to felucca-bsp `chip/usb_host.c`, `chip/usb_host_desc.h`, the host
routines in `chip/fm1_usb.h` and the host's integration in `chip/usb.c`: Melodee's USB MIDI host, where it is
GPL-3.0-only, by two authors. Its own `NOTICE` and `LICENSING.md` record the source commits. It is here under
**MPL-2.0 on these grounds only: each author's own word**, quoted verbatim with its links.

Adrien Tibi (Recursive Field Instruments), for his part (the host: enumeration, hubs, input, the PHY setup, the
descriptor parser), in Melodee pull request [#75](https://github.com/keremimo/melodee/pull/75):

> **keremimo**, 2026-10-24 ([#issuecomment-6383989315](https://github.com/keremimo/melodee/pull/75#issuecomment-6383989315)):
> "Are you okay with licencing your part of the code as MPL? That would enable me to get this code into the fm1_base
> and also Saadet as well."
>
> **recursivefieldinstruments**, 2026-10-24 ([#issuecomment-6384184014](https://github.com/keremimo/melodee/pull/75#issuecomment-6384184014)):
> "Hey! Excellent. It works super well here. I've been using various controllers and that's so convenient.
> For the licensing, of course, feel free to do so :)
>
> Kind regards,
> Adrien Tibi (Recursive Field Instruments)"

Kerem Kilic (Ellic Studio, keremimo), for his part (MIDI out, recovery, the disconnect debounce, the tests, the
felucca-bsp integration), in felucca-bsp pull request [#7](https://github.com/hugelton/felucca-bsp/pull/7)'s
description:

> "I also license my contributions to this port under MPL-2.0."

This matters because a build with Bluetooth links JieLi's binary libraries: no GPL code may be in it. These quotes
are in the patch's header too, and the pull request's files carry both authors' notices. The permissions cover
these contributions only, not the rest of Melodee. The register sequences follow the public JieLi AC79 SDK.

## Not part of fm1-base

| What | Licence | Where |
| --- | --- | --- |
| Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `fm1-base/assets/fonts/ter-u*.bdf`, `fm1-base/assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK files a package needs (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | Apache-2.0 | read from your SDK checkout at build time; never in this repository |
| JieLi's Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, ...: binary only) and the few SDK sources and headers they need | Apache-2.0 | copied from your SDK checkout into the build by a build with `ble = true`; never in this repository |
