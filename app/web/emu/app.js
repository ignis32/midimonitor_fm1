// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// MIDIMON's part of fm1-base's emulator page (fm1-base/web/emu/index.html imports it): a Web MIDI input (a keyboard,
// a controller, a DAW's port on this computer) into the firmware, as the FM-1's USB, TRS or Bluetooth MIDI would bring
// it, so the monitor shows it and the thru routes take it (app_worklet.js). Both choices are kept in localStorage.
const IN_KEY = "midimon-emu-midi-in", PORT_KEY = "midimon-emu-midi-port";
const get = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
const put = (k, v) => { try { if (v === null) localStorage.removeItem(k); else localStorage.setItem(k, v); } catch { /* private window */ } };
const PORTS = [["USB", 0], ["TRS", 2], ["Bluetooth", 1]];   // (fm1-base/include/plat.h PORT_*)

let node = null, port = 0;

function panel(status) {
  const box = document.createElement("section");
  box.className = "speak";
  box.setAttribute("aria-labelledby", "midimon-h");
  const sel = `font: inherit; color: var(--ink); background: var(--card); border: 1px solid var(--muted); border-radius: 8px; padding: .25em .5em; max-width: 60vw`;
  box.innerHTML = `<h2 id="midimon-h" style="font: 600 1.1rem 'Barlow Semi Condensed', 'Barlow', sans-serif; margin: 0 0 .6rem">MIDI in</h2>
<div class="row" style="margin: 0; display: flex; flex-wrap: wrap; gap: .6rem 1.4rem">
  <label style="display: inline-flex; gap: .6rem; align-items: center">From
    <select id="midimon-in" style="${sel}"><option value="">None</option></select></label>
  <label style="display: inline-flex; gap: .6rem; align-items: center">as
    <select id="midimon-port" style="${sel}">${PORTS.map(([n, v]) => `<option value="${v}">${n}</option>`).join("")}</select></label>
</div>
<p class="note">A MIDI device or program on this computer, through Web MIDI, into the monitor as if it came on the
FM-1's USB, its TRS jack or Bluetooth: everything it sends, SysEx and clock too.</p>`;
  document.getElementById("speak").before(box);

  const ps = box.querySelector("#midimon-port");
  port = +(get(PORT_KEY) ?? 0) || 0;
  if (!PORTS.some(([, v]) => v === port)) port = 0;
  ps.value = String(port);
  ps.addEventListener("change", () => { port = +ps.value; put(PORT_KEY, ps.value); });

  const ins = box.querySelector("#midimon-in");
  if (!navigator.requestMIDIAccess) {
    ins.disabled = true;
    ins.options[0].textContent = "No Web MIDI in this browser";
    return;
  }
  let input = null;
  const use = (id, inputs) => {
    if (input) input.onmidimessage = null;
    input = inputs.get(id) || null;
    if (input) input.onmidimessage = (e) => node && node.port.postMessage({ type: "app", cmd: "midi", port, d: Array.from(e.data) });
    put(IN_KEY, input ? input.name : null);
  };
  navigator.requestMIDIAccess({ sysex: true }).catch(() => navigator.requestMIDIAccess({ sysex: false })).then((acc) => {
    const fill = () => {
      const want = input ? input.id : null, saved = get(IN_KEY);
      ins.length = 1;
      for (const i of acc.inputs.values()) ins.add(new Option(i.name, i.id));
      const again = [...acc.inputs.values()].find((i) => i.id === want || (!want && i.name === saved));
      ins.value = again ? again.id : "";
      use(ins.value, acc.inputs);
    };
    fill();
    acc.onstatechange = fill;
    ins.addEventListener("change", () => use(ins.value, acc.inputs));
  }).catch(() => {
    ins.disabled = true;
    ins.options[0].textContent = "MIDI not allowed";
    status("Web MIDI was not allowed: the panel still plays.");
  });
}

export function attach(p) {
  node = p.node;
  panel(p.status);
}
