// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// MIDIMON's part of fm1-base's emulator page (fm1-base/web/emu/index.html imports it): a Web MIDI input (a keyboard,
// a controller, a DAW's port on this computer) for each of the FM-1's MIDI ports, USB, TRS and Bluetooth, into the
// firmware as that port would bring it, so the monitor shows it and the thru routes take it (app_worklet.js). The
// choices are kept in localStorage, by the inputs' names.
const KEY = "midimon-emu-midi-in";                  // {USB: name, TRS: name, BLE: name}
const get = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
const put = (k, v) => { try { localStorage.setItem(k, v); } catch { /* private window */ } };
const PORTS = [["USB", "USB", 0], ["TRS", "TRS", 2], ["BLE", "Bluetooth", 1]];   // (fm1-base/include/plat.h PORT_*)

let node = null;

function panel(status) {
  const box = document.createElement("section");
  box.className = "speak";
  box.setAttribute("aria-labelledby", "midimon-h");
  const css = `font: inherit; color: var(--ink); background: var(--card); border: 1px solid var(--muted); border-radius: 8px; padding: .25em .5em; max-width: 60vw`;
  box.innerHTML = `<h2 id="midimon-h" style="font: 600 1.1rem 'Barlow Semi Condensed', 'Barlow', sans-serif; margin: 0 0 .6rem">MIDI in</h2>
<div class="row" style="margin: 0; display: flex; flex-wrap: wrap; gap: .6rem 1.4rem">
${PORTS.map(([k, n]) => `  <label style="display: inline-flex; gap: .6rem; align-items: center">${n}
    <select data-port="${k}" style="${css}"><option value="">None</option></select></label>`).join("\n")}
</div>
<p class="note">A MIDI device or program on this computer for each of the FM-1's ports, through Web MIDI: into the
monitor as if it came on the FM-1's USB, its TRS jack or Bluetooth. Everything it sends, SysEx and clock too.</p>`;
  document.getElementById("speak").before(box);
  const sels = [...box.querySelectorAll("select[data-port]")];

  if (!navigator.requestMIDIAccess) {
    for (const s of sels) { s.disabled = true; s.options[0].textContent = "No Web MIDI in this browser"; }
    return;
  }
  let saved = {};
  try { saved = JSON.parse(get(KEY) || "{}") || {}; } catch { /* left as none */ }
  navigator.requestMIDIAccess({ sysex: true }).catch(() => navigator.requestMIDIAccess({ sysex: false })).then((acc) => {
    const wire = () => {                            // each input to the ports that chose it
      const to = new Map();
      for (const s of sels) if (s.value) to.set(s.value, [...(to.get(s.value) || []), PORTS.find(([k]) => k === s.dataset.port)[2]]);
      for (const i of acc.inputs.values()) {
        const ports = to.get(i.id);
        i.onmidimessage = ports ? (e) => { if (node) for (const port of ports) node.port.postMessage({ type: "app", cmd: "midi", port, d: Array.from(e.data) }); } : null;
      }
    };
    const fill = () => {
      for (const s of sels) {
        const want = s.value, name = saved[s.dataset.port];
        s.length = 1;
        for (const i of acc.inputs.values()) s.add(new Option(i.name, i.id));
        const again = [...acc.inputs.values()].find((i) => i.id === want || (!want && i.name === name));
        s.value = again ? again.id : "";
      }
      wire();
    };
    fill();
    acc.onstatechange = fill;
    for (const s of sels) s.addEventListener("change", () => {
      const i = acc.inputs.get(s.value);
      if (i) saved[s.dataset.port] = i.name; else delete saved[s.dataset.port];
      put(KEY, JSON.stringify(saved));
      wire();
    });
  }).catch(() => {
    for (const s of sels) { s.disabled = true; s.options[0].textContent = "MIDI not allowed"; }
    status("Web MIDI was not allowed: the panel still plays.");
  });
}

export function attach(p) {
  node = p.node;
  panel(p.status);
}
