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

## Not part of fm1-base

| What | Licence | Where |
| --- | --- | --- |
| Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `fm1-base/assets/fonts/ter-u*.bdf`, `fm1-base/assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK files a package needs (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | Apache-2.0 | read from your SDK checkout at build time; never in this repository |
| JieLi's Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, ...: binary only) and the few SDK sources and headers they need | Apache-2.0 | copied from your SDK checkout into the build by a build with `ble = true`; never in this repository |
