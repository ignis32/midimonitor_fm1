# Changes

## Next

- **USB name back to `MIDIMON FM-1`**: 0.3's `FM1_MONITOR` on USB lost the "FM-1" the firmware installers look
  for. Bluetooth stays `FM1_MONITOR`.
- **Battery** on GLO 1/3: a percentage (an estimate), a bar, the volts and the raw reading; the reading now follows
  the battery (fm1-base's average stopped moving within 15 of its first value until the next power-on).
- **Scanlines** moved to GLO 3/3 (K3); K3 on the LOG tab is free.

## 0.3

**The FM-1 as `FM1_MONITOR`.** It shows up under that name on USB and on Bluetooth (was "MIDIMON FM-1" and "FM-1").

**USB host mode** (new): the USB-C port can host a class-compliant USB-MIDI controller or synth instead of talking to
a computer: GLO 1/3, K2 (it asks first). What the device sends is logged as `USB`, and USB out goes to it, THRU
included: a Bluetooth controller can play a USB synth with no computer around. Full-speed devices, alone or behind
one hub; the FM-1 gives no power on the port, so a powered hub or an OTG adapter that powers the device is needed.
Never kept: every power-on is device mode again, so updates always work. The driver is Adrien Tibi's and Kerem
Kilic's (Melodee), from felucca-bsp's pull request #7, under MPL-2.0 by their word (LICENSING.md).

**A BLUETOOTH page** (GLO 2/3):
- BLE-Periphery on / off (a computer or tablet connects to the FM-1).
- BLE-Central OFF / FIRST / CHOOSE: FIRST connects to the first MIDI device it sees (and again when that one goes);
  CHOOSE to the one picked from a list (K3) that shows each device's name, the last four digits of its address and
  its signal. In the list: SELECT or K3 move, OCT+ connects, OCT- goes back, PLAY scans again.
- RECONNECT: back to the chosen device at power-on and whenever it goes (found by its address).
- Both links' state on the page. The radio is off only when both roles are.

**GLO is three pages:** 1/3 note 60 and the USB port, 2/3 Bluetooth, 3/3 the skin and the platform's SysEx.

**HOME goes home:** to the LOG tab from any page, then from KEYS, CC, CHANS or CLOCK to the log itself; only on the
log does it ask to clear it.

**Words:** USB is in DEVICE MODE or HOST MODE; Bluetooth's roles are BLE-Central and BLE-Periphery, BLE-C and BLE-P
where space is short.

**Under the hood:** fm1-base is now a git submodule (`fm1-base/`, felucca-bsp inside it) at its latest commit,
used as upstream has it; MIDIMON's few changes to it are patches in `patches/`, so upstream updates come in easily.
Upstream already had the Bluetooth link handling MIDIMON 0.2 carried itself.

**Note:** the settings are kept in a new form: after this update the FM-1 starts once from the defaults (BLE-P on,
BLE-C CHOOSE, RECONNECT on, THRU off). Set things up again and SAVE.

Install over MIDIMON, the stock firmware or another custom firmware (tested over Felucca and fm1-radio).

## 0.2

The LOG tab's views on ALGO: KEYS (notes held per channel, a zoomed keyboard with velocity and poly pressure, double
note-ons and note-offs without a note-on counted), CC (bend and channel pressure graphs, the controllers last moved
in rows that stay put), CHANS (activity per channel and source, each channel's last message), CLOCK (tempo,
transport, position, song position, clock spacing, MTC).

## 0.1

The first release: the log (HEX, HUMAN, BOTH), FILTER, STATS, THRU with per-route filters, GLO, skins; USB, the TRS
jack and both Bluetooth roles at once.
