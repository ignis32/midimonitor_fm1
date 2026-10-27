# MIDIMON

MIDIMON is a firmware for the M-VAVE FM-1: a MIDI monitor in a green phosphor terminal's look, and a thru router.
It shows notes, CCs, clock and every other message from TRS, USB, USB host and both Bluetooth roles, with filters,
and routes MIDI between those ports as a matrix. With THRU on, the FM-1 is a Bluetooth MIDI dongle that shows what
passes through it.

Version {{VERSION}}. Early: expect changes. Installing is at your own risk; the installer can put M-VAVE's own
firmware back. Without an RP2040 dongle to recover a hard brick, install the custom bootloader with
[m-flasher](https://github.com/jvitkauskas/m-flasher/releases) first.

- [Install](install/): put MIDIMON on your FM-1, from Chrome or Edge, the FM-1 connected by USB.
- [Simulator](emu/): the same code the FM-1 runs, in the browser. A MIDI device or program on this computer for each of
  its ports (USB, TRS, Bluetooth) plays into it through Web MIDI. No FM-1 needed.
- [Download {{PKG}}](firmware/{{PKG}}): for the command-line installer, `python3 fm1-base/tools/fm1_install.py {{PKG}}`.
- [Source](https://github.com/ignis32/midimonitor_fm1): MPL-2.0. Built on fm1-base (Charles Vestal) and felucca-bsp,
  from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. MIDIMON is not affiliated with them.
