# m-bootloader, as MIDIMON's installer offers it

[m-bootloader](https://github.com/jvitkauskas/m-bootloader) by Julius Vitkauskas (MIT, `LICENSE`): hold PLAY at
power-on for its menu (MIDI MODE reinstalls a firmware over USB MIDI from a browser), REC for update mode.

| File | From |
| --- | --- |
| `fm1-uboot-2.2.fwsc` | release [v2.2](https://github.com/jvitkauskas/m-bootloader/releases/tag/v2.2) (2.2-64f7c9c), its `fm1-uboot.fwsc` unchanged: SHA-256 `cb6128ee753a1661e6399002317464959231854024f259520755621954438981` (its `SHA256SUMS` and GitHub's digest), identity `FM-1_099` |
| `LICENSE` | the repository's at v2.2 |

fm1-base's installer carries 1.2 (`fm1-base/web/m-bootloader/`, `tools/make_site.py` MBOOT_VERSION). 1.2 stops at
`BAD CONFIG` on a unit with stock V16's boot area (fixed in 1.3), and one FM-1 with the newer UC25HQ80 flash did not
start after 1.1 or 1.2. MIDIMON's site takes this one instead: `app/fm1.toml` `mboot` / `mboot_version`
(patches/fm1-base/0005-site-mboot-from-app.patch). When fm1-base carries this version or a newer one, the two keys
go and this folder with them.

Not tested by m-bootloader's author on the UC25HQ80 (`0xB36014`): see its release notes.

A new version: its `fm1-uboot.fwsc` as `fm1-uboot-X.Y.fwsc` here (the checksum checked against the release's
`SHA256SUMS`), `mboot_version` in `app/fm1.toml`, the table above.
