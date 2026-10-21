# Relicensing a moved firmware to MPL-2.0

Only when the firmware's authors choose to (SKILL.md "Before you start"): it is needed only to link JieLi's Bluetooth
or WiFi libraries. A firmware that keeps its licence needs none of this.


## The records

- Felucca is GPL-3.0-only, and stays so: github.com/hugelton/Felucca.
- Leo Kuroshita approved his Felucca code in felucca-bsp and fm1-base under MPL-2.0, with his name in the credits:
  https://github.com/hugelton/felucca-bsp/issues/4
- keremimo agreed for his part of `usb.c` (MIDI clock and transport sync, the TRS ring fix):
  https://github.com/hugelton/felucca-bsp/pull/3
- felucca-bsp's `LICENSING.md`, "Forks of Felucca": a fork does not become MPL-2.0 by using felucca-bsp; start a new
  project and import it.

## The rule

A firmware relicensed to MPL-2.0 is made of:

1. **felucca-bsp and fm1-base**, as a submodule, unchanged or changed under MPL-2.0 (changes to their files are
   published under MPL-2.0, file by file).
2. **The author's own code**: written by the person relicensing it, or by people who agreed, on the record, to
   MPL-2.0 for their part.
3. **Third-party code** under a licence that allows it beside MPL-2.0 files and in a firmware that links JieLi's
   libraries: MIT, BSD, ISC, zlib, Apache-2.0, CC0, public domain, SIL OFL (fonts). Not GPL or LGPL code, which
   would make the firmware GPL again. Each with its licence text and listed in `LICENSING.md`.

Leo's permission covers his code as it is in felucca-bsp and fm1-base; it does not relicense Felucca itself. So
nothing else from Felucca: not its engines, sequencer, UI, MIDI handling, editor or web pages, Leo's included. If
a firmware needs a part of Felucca that is in neither, ask the maintainers of felucca-bsp and fm1-base whether it
belongs there; once it is in, it may be used like the rest. What
the firmware needs of them is replaced by fm1-base's, taken from the upstream library they came from (Felucca's
third-party parts are listed in its `LICENSING.md` and `CREDITS.md`: take those from their own repositories, under
their own licences), or written again.

## The author's own code

Code is the author's when they wrote it, in this project, without copying it from Felucca. Check, don't assume:

- The provenance tool (`tools/provenance.py`) finds lines copied from Felucca, including those that came in through
  contributors' pull requests that Leo applied under his own name. Run it on the whole project.
- A file that started as a copy of a Felucca file and was then changed is still Felucca's, however much it changed,
  unless it was rewritten from scratch.
- In the project's own history (`git log --follow FILE`), commits by other people are theirs: they need to agree
  to MPL-2.0 for their part, on the record (an issue or pull request comment, as felucca-bsp#3), or their part is
  rewritten.
- Some FM-1 firmwares took code from other GPL forks (X0X, Melodee, Jangada and others). That code is GPL too,
  unless its author relicenses it.

## Rewriting

When a feature has to be written again, write it from what it does (its behaviour on the device, its manual), not
from Felucca's source. Don't keep Felucca's file open beside the new one and translate it. A rewrite that follows
the original's structure line for line is a derived work. If the user can't rewrite something cleanly, it stays out.

## Why it matters

JieLi's Bluetooth and WiFi libraries are binaries under Apache-2.0. A GPL-3.0 firmware that links them can't be
distributed without an extra permission from the GPL code's copyright holders. An MPL-2.0 firmware can: MPL-2.0
applies file by file, and its files' sources are all published.

## Checking where the code came from

1. Get a fresh Felucca (`git clone https://github.com/hugelton/Felucca`) and run the provenance check from fm1-base:

   ```sh
   python3 base/tools/provenance.py PROJECT Felucca
   ```

   It lists each file of PROJECT that has runs of Felucca's lines that are not in felucca-bsp or fm1-base, and whose
   they are (Leo's, or a contributor's pull request: Leo applied many under his own name, so `git blame` alone would
   hide them). `--lines` shows the lines. It needs `gh` to fetch the credited pull requests once. Lines that are also
   in felucca-bsp or fm1-base count as allowed. Short shell boilerplate (`cd "$(dirname "$0")"`, the toolchain's
   default paths) shows up in `build.sh`: ask the author whether it matters to them rather than rewriting it unasked.
2. Run it against every other GPL firmware the code may have come from (a checkout as the second argument,
   `--prs ""`), and read the code's own comments and history for credits ("after", "adapted from", "from X0X"). A
   comment that says code was adapted is a question to answer, not to ignore: compare the code with the original
   function by function (decomposition, control flow, data, constants, comments). The same design, implemented on
   other data, is the author's own; the same code translated is not.
3. Every file in the table's "Felucca's other code" or another fork's rows: rewritten by the author from the
   behaviour, replaced (fm1-base's equivalent, or an upstream library), or kept only with its author's recorded
   agreement. Agree the list with the author before porting.

A clean report is necessary, not sufficient: it finds copies, not rewrites that follow the original too closely.

## Licensing files

- `LICENSE`: the MPL-2.0 text. An SPDX line (`SPDX-License-Identifier: MPL-2.0`) and the author's copyright at the
  top of every file of the app.
- `LICENSING.md`: say that the versions before the change stay under their old licence, and give the table from
  step 1, as it is now: what each part is, where it came from, its licence (CTL-1's and Radio's show the form).
  Third-party code with its licence file beside it.
- Don't say the firmware is MPL-2.0 until the checks are clean and the author has seen the table.
