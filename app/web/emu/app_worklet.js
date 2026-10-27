// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// MIDIMON's part of fm1-base's emulator worklet (fm1-base/web/emu/worklet.js loads it first): a Web MIDI message
// from the page (app.js), every kind (SysEx, clock too), into the firmware as the chosen port brings it: the MIDI tap,
// which the log reads, and the app's queue (patches/fm1-base/0005: web_midi_bytes).
globalThis.fm1App = {
  message(fm1, m) {
    if (m.cmd !== "midi" || !m.d || !m.d.length) return;
    const ex = fm1.ex, buf = new Uint8Array(fm1.mem.buffer, ex.web_midi_buf(), 512), n = Math.min(m.d.length, 512);
    buf.set(m.d.subarray ? m.d.subarray(0, n) : m.d.slice(0, n));
    ex.web_midi_bytes(m.port >>> 0, n);
  },
};
