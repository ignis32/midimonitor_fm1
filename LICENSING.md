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

## The USB MIDI host (patches/felucca-bsp/0002-usb-midi-host.patch)

`chip/usb_host.c`, `chip/usb_host_desc.h` and the two host helpers in `chip/fm1_usb.h` that this patch adds to
felucca-bsp are Adrien Tibi's (Recursive Field Instruments), from Melodee pull request
[#75](https://github.com/keremimo/melodee/pull/75) (commit 198c8fd), where they are GPL-3.0-only. They are here under
**MPL-2.0 on one ground only: their author's permission in that pull request**, quoted here verbatim with its links:

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

This matters because a build with Bluetooth links JieLi's binary libraries: no GPL code may be in it. The same quote
is in each of those files' headers and in the patch's header. Only the code of that pull request is covered: MIDI out
to the controller, which keremimo added later in Melodee 1.5 RC, is not here. The register sequences in it follow the
JieLi AC79 SDK (Apache-2.0).

## Not part of fm1-base

| What | Licence | Where |
| --- | --- | --- |
| Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `fm1-base/assets/fonts/ter-u*.bdf`, `fm1-base/assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK files a package needs (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | Apache-2.0 | read from your SDK checkout at build time; never in this repository |
| JieLi's Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, ...: binary only) and the few SDK sources and headers they need | Apache-2.0 | copied from your SDK checkout into the build by a build with `ble = true`; never in this repository |
