---
name: felucca-to-fm1-base
description: Move an M-VAVE FM-1 firmware that is based on Felucca (GPL-3.0) onto felucca-bsp and fm1-base (MPL-2.0), keeping the firmware's own licence, and, only if its author chooses to, relicense it to MPL-2.0 (needed only to link JieLi's Bluetooth or WiFi libraries). Use when asked to move a Felucca-based firmware to fm1-base or felucca-bsp, to put a Felucca fork on the shared platform, to relicense a Felucca fork or "make it MPL", or to check whether a firmware still contains Felucca code.
---

# Move a Felucca-based firmware onto fm1-base

Felucca (github.com/hugelton/Felucca) is GPL-3.0-only, and so is a firmware made from it. Two MPL-2.0 projects carry
the parts every FM-1 firmware needs:

- **felucca-bsp** (github.com/hugelton/felucca-bsp): the chip and board layer, the update loader, the package and
  install tools. Leo Kuroshita approved his Felucca code here under MPL-2.0 (felucca-bsp#4), and keremimo his part
  of `usb.c` (felucca-bsp#3).
- **fm1-base** (github.com/charlesvestal/fm1-base): everything above the hardware (screen and drawing, panel and
  keys, audio, MIDI over USB and TRS, storage, safe mode, the screen reader, Bluetooth, WiFi, USB audio, screen
  mirroring, backup and restore), a host simulator and a browser emulator, the build, and a template app. It includes
  felucca-bsp as `bsp/`.

The move puts the firmware on them: fm1-base as a git submodule, the firmware's own code as an app on it, its copy
of Felucca's platform gone. The firmware then gets the platform's fixes and parts with every other firmware, instead
of carrying its own copy of Felucca.

**The move does not change the firmware's licence.** MPL-2.0 files may be part of a GPL work (they stay MPL-2.0, file
by file), so a GPL firmware stays GPL on fm1-base. Its licence is its authors' to choose, not this skill's.
Relicensing to MPL-2.0 is a separate, optional step, and it is needed for one thing only: linking JieLi's closed
Bluetooth or WiFi libraries, which a GPL firmware cannot ship. Ask; never assume.

Worked examples: Polly (github.com/charlesvestal/fm1-polly: moved, kept GPL-3.0), CTL-1 and Radio
(github.com/charlesvestal/fm1-ctl, fm1-radio: moved and relicensed, for Bluetooth and WiFi). Their `LICENSING.md`
files show the result.

## Before you start

- **Ask the author about the licence.** Explain the choice in a few lines: the move keeps the licence; relicensing to
  MPL-2.0 is only needed for Bluetooth or WiFi (and those cost flash and RAM: fm1-base's README has the figures;
  check the firmware has room for them before suggesting it). Default: keep it. Only if they choose to relicense,
  follow [RULES.md](RULES.md) as well (steps 1 and 2 below gain the provenance work, step 5 the licence change).
- **Who may relicense.** Only the people who wrote the code. If the author is not the only one, each author's
  agreement is needed, on the record, before any code is relicensed. Code that came from another GPL firmware
  (X0X, Melodee, Jangada...) needs its author's agreement too, or a rewrite. None of this is needed to move a
  firmware that keeps its licence.
- **What it takes to build.** The JieLi toolchain and the AC79 SDK, and on macOS Docker or podman to run the
  toolchain (fm1-base's `README.md` and `tools/get_toolchain.sh`); Python 3 with Pillow (with libraqm, for the fonts);
  `gh`, GitHub's command line, signed in (the provenance check reads Felucca's pull requests with it); an FM-1 on USB
  for step 4. Emscripten and Node.js are optional (the browser builds).
- **Words used below.** The *package* is the installable firmware file (`.fwsc`); its *identity* (`FM-1_` and
  digits, set by `id_prefix` in `fm1.toml` and the version) is how the installer and the FM-1 tell firmwares and
  versions apart. *Store objects* are what a firmware saves in flash, each at a fixed address (`APP_STORE_MAP`).
  *Scenarios* are scripts the host simulator runs the app with (keys, knobs, checks).

## The steps

Work through them in order, and tell the user what each one found before going on.

### 1. Inventory: what is platform, what is the app

Sort every file of the firmware (write the table down; it becomes part of `LICENSING.md`):

| Kind | What happens to it |
| --- | --- |
| Felucca's platform (`firmware/hal/`, and in `firmware/src/` the boot, main loop, audio interrupt, USB, LCD, panel and input, storage, update, libc, gfx; the loader; the build, package and install tools; the web installer; the tests of these) | dropped: fm1-base and felucca-bsp provide it ([PORTING.md](PORTING.md) has the map) |
| The firmware's own code (its sound, sequencer, UI, project format, its own tools, tests, docs, site) | moved into `app/` (and the repository's tools and tests), its licence unchanged |
| Platform features the firmware added for itself (a USB command, a flash region, a screen-to-host feature) | a candidate for fm1-base (step 2) |
| Third-party code (DSP libraries, decoders, fonts, samples) | kept under its own licence |

When relicensing, add the provenance check here: [RULES.md](RULES.md) "Checking where the code came from".

Show the user the table. Ask about anything unclear (who wrote a file, where a library came from). Don't guess
authorship.

### 2. Decide what fm1-base needs

Map the firmware's interface to the hardware onto fm1-base's `include/plat.h` (PORTING.md). For each thing the
firmware needs that fm1-base lacks, decide with the user: is it general (other firmwares would use it)? Then it
belongs in fm1-base or felucca-bsp, optional where it costs flash or RAM (an `fm1.toml` key or an `app.h` hook);
discuss it with their maintainers. Is it the firmware's own? Then it stays in the app, behind a hook. Prefer what
fm1-base already has (its dim lights, its remote control) over a second copy.

When relicensing, also decide what has to be rewritten (RULES.md).

### 3. Port: the app on fm1-base

Work on a branch (a worktree keeps the user's uncommitted work out of the way), with fm1-base as `base/` and the app
in `app/`, the way the worked examples are laid out. Then follow [PORTING.md](PORTING.md): the app contract
(`app.h`), `fm1.toml`, the platform interface, the build, the simulator, and the device. Keep the firmware's package
identity (`id_prefix`) and its store objects' flash offsets (`APP_STORE_MAP`) if it already has releases, so the new
firmware installs over the old one as an update and keeps what players saved. Keep its USB names, its published
screen-reader words files, and any data format a player keeps outside the FM-1 (a backup file).

Before porting anything, run every scenario through the firmware's old simulator and keep everything they write
(screens, audio, MIDI, the screen reader's words, saved files): the baseline for step 4.

### 4. Check it

- **The app behaves exactly as before**: run the same scenarios on fm1-base's simulator and compare what they write
  with the baseline, byte for byte. Explain every difference; don't call a port done on "it looks right". For a
  difference that comes from the simulators rather than the port (PORTING.md "Comparing with the old simulator"),
  prove it: run the old simulator with that difference matched (an added `wait`, a gain) and get identical output.
- The build is clean (`./build.sh`), fm1-base's tests and the firmware's pass, the browser build runs.
- **On an FM-1.** Before installing anything, say what an install overwrites (firmwares share the settings
  sectors), and take a backup of what is on it with its own firmware's tool. Then the real path: install the latest
  release, keep something (a saved setting, a song), back it up, install the port over it, and check that it is all
  there; then boot, screen, every button and knob, keys, sound (the user listens), MIDI in and out, USB audio, a
  setting saved across a power cycle, a restore of the old backup, an update started from the port itself, and the
  firmware's own transfers (its banks, its files). `base/tools/fm1_remote.py` drives it and reads its screen and
  crash log; don't drive it while the user's hands are on it.
- fm1-base's build prints a note if any function has a 64-bit multiply inlined beside a load. On the FM-1 such code
  now and then corrupts a register (felucca-bsp `docs/CPU.md`). Move those multiplies into `noinline` helpers.
- When relicensing: RULES.md's checks too.

### 5. Licensing files and release

- **Keeping the licence**: the files keep their SPDX lines. `LICENSING.md`: the firmware's licence; that fm1-base
  and felucca-bsp (in `base/`) are MPL-2.0, file by file, from Felucca by Leo Kuroshita (Hügelton Instruments) with
  his permission (felucca-bsp#4); the table from step 1 as it is now; third-party code with its licence. Code the
  firmware gave to fm1-base is MPL-2.0 there (its author's choice when contributing it).
- **Relicensing**: RULES.md "Licensing files".
- `README.md` credits: fm1-base and felucca-bsp, "from Felucca by Leo Kuroshita (Hügelton Instruments)"; the
  third-party code. No Felucca, M-VAVE or Hügelton names in the product's name.
- The docs and the manual: every path and link still there. Commands such as `python3 tools/fm1_install.py` become
  `python3 base/tools/fm1_install.py`, and a link to a script the firmware no longer has points to fm1-base's or
  felucca-bsp's copy.
- The site: the installer page and package code came from Felucca; use fm1-base's (`base/tools/publish_site.sh`, or
  the firmware's own page script with `base/web/installer.html`). The firmware's own pages stay.
- The version: after the latest release and tag (`gh release list`, `git tag`): the package identity must go up, or
  players would be offered an older firmware than the one they run.
- Release: `./build.sh --release X.Y`, install that exact file on an FM-1 and check it once more, then the site, and
  a GitHub release with the `.fwsc` and notes for players: what changes for them (on the device, little or nothing),
  and that their saved songs and settings carry over.
- Ask the maintainers of felucca-bsp to list the firmware (felucca-bsp `LISTING.md`) if the user wants that.

## What not to do

- Don't change a firmware's licence as part of the move, or suggest it is needed. Ask; the author decides.
- Don't relicense anyone's code without their recorded agreement.
- Don't copy a file from Felucca into the firmware or into fm1-base because "it's Leo's": only what felucca-bsp and
  fm1-base already carry is under MPL-2.0.
- Don't install over a player's firmware without saying what it overwrites, and without a backup when there is
  something to keep.
