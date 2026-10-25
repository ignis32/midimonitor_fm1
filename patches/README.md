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

- `sh build.sh` runs `tools/patches.sh apply` (and fetches the submodules first if they are missing): each patch
  not applied yet goes in; one already applied is left; one that fits neither way stops the build with its name.
- `tools/patches.sh status`: each patch, applied / open / conflict.
- Changing a patch: edit the files in `fm1-base/` or `fm1-base/bsp/`, then `tools/patches.sh refresh
  patches/…/NNNN-x.patch` (its header kept; the other patches on that tree are taken out meanwhile and put back). A
  new file needs `git -C fm1-base add -N FILE` first, and `git -C fm1-base reset -q FILE` after, so the submodule's
  index stays as upstream left it.
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
| `felucca-bsp/0001-midi-tap-usb.patch` | USB's side of the MIDI tap: every USB-MIDI packet in, copied to the tap before usb.c sorts it |
| `fm1-base/0001-midi-tap.patch` | the MIDI tap (`plat_tap`, `device/midi_tap.c`): every byte from USB, the jack and each Bluetooth link, as it came; the simulator's `raw` |

Upstream took in what MIDIMON's earlier copy of fm1-base changed for Bluetooth (each link addressed apart:
`TO_BLE_HOST`, `TO_BLE_DEV`; the send stall per link), so those are no longer patches.
