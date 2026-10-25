# patches: MIDIMON's changes to its platform

MIDIMON builds on [fm1-base](https://github.com/charlesvestal/fm1-base) (`fm1-base/`, a pinned git submodule) with
[felucca-bsp](https://github.com/hugelton/felucca-bsp) inside it (`fm1-base/bsp/`, fm1-base's own submodule). What
MIDIMON needs changed there, and only that, is kept here as patches, in plain sight, and applied to the pinned trees
before every build. Nothing is committed inside `fm1-base/`: these files are the record. The aim is to follow
upstream with as little friction as possible: anything that can live in `app/` does.

| Folder | Applied in | Order |
| --- | --- | --- |
| `felucca-bsp/NNNN-*.patch` | `fm1-base/bsp/` | first, by name |
| `fm1-base/NNNN-*.patch` | `fm1-base/` | after, by name |

Each patch starts with a header: what it changes, why MIDIMON needs it, the upstream commit it applies to, and
whether it has been offered upstream. Patched files are MPL-2.0 as the originals, so the patches are too.

## Working with them (`tools/patches.sh`)

- Each repository's patches are a series, applied in name order: a patch may build on an earlier one (the tap's
  `felucca-bsp/0002` hooks into the host driver that `0001` adds). What is applied is remembered in the submodule's
  own git directory (`midimon-patches/`, a copy of each patch as applied), never in its tree.
- `sh build.sh` runs `tools/patches.sh apply` (and fetches the submodules first if they are missing): nothing to do
  when the series in `patches/` is the one applied; else the applied one comes out and the new one goes in. A patch
  that does not fit stops the build with its name.
- `tools/patches.sh status`: each patch, applied / changed (edited since it was applied) / open.
- Changing a patch: edit the files in `fm1-base/` or `fm1-base/bsp/`, then `tools/patches.sh refresh
  patches/…/NNNN-x.patch`: it is written again against upstream plus the patches before it (its header kept), the
  patches after it taken out meanwhile and put back. New files need nothing special.
- `.gitmodules` has `ignore = dirty` for `fm1-base`: the patched submodule doesn't show as changed in MIDIMON's status.

## Updating fm1-base

1. `tools/patches.sh revert` (the trees as upstream left them).
2. Bump the submodule (`git -C fm1-base fetch && git -C fm1-base checkout <commit> && git -C fm1-base submodule
   update`), commit the new pointer.
3. `tools/patches.sh apply`. A patch upstream has taken in: delete it. One that no longer fits: apply what fits by
   hand, `refresh` it, and say in its header which upstream commit it now applies to.
4. `sh build.sh test` and `sh build.sh`.

## The patches

| Patch | What |
| --- | --- |
| `felucca-bsp/0001-usb-midi-host-pr7.patch` | felucca-bsp's own [pull request #7](https://github.com/hugelton/felucca-bsp/pull/7), unchanged: the USB MIDI host, in and out (Adrien Tibi's and Kerem Kilic's, MPL-2.0 by their word: quoted in the patch and LICENSING.md). Goes when felucca-bsp merges it |
| `felucca-bsp/0002-midi-tap-usb.patch` | USB's side of the MIDI tap: every USB-MIDI packet in, from a computer or from a device on the host port, copied to the tap before it is sorted |
| `fm1-base/0001-midi-tap.patch` | the MIDI tap (`plat_tap`, `device/midi_tap.c`): every byte from USB, the jack and each Bluetooth link, as it came; the simulator's `raw` |
| `fm1-base/0002-usb-host.patch` | the USB host for apps: `FM1B_USB_HOST`, `plat_usb_host` / `_state` / `_event`, the main loop's `usb_host_task`, the simulator's `usbhost` |

Upstream took in what MIDIMON's earlier copy of fm1-base changed for Bluetooth (each link addressed apart:
`TO_BLE_HOST`, `TO_BLE_DEV`; the send stall per link), so those are no longer patches.
