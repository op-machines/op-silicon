// SPDX-License-Identifier: MIT
// The machine list from data/machines.json, with an audition per machine: its preview
// build (engine.wasm) plays in an AudioWorklet at 44.1 kHz, controls and a small keyboard.
import './style.css';

type Control = { id: string; label: string; default: number; labels?: string[] };
type Entry = {
  id: string; name: string; description: string; licence: string;
  controls: Control[]; cpu: number | null; source: string; preview: boolean;
};

const repo = 'https://github.com/op-machines/op-silicon/tree/main/';
const Q31 = 0x7fffffff;
const keys = 'awsedftgyhujk';  // one octave from C on a computer keyboard

const status = document.querySelector<HTMLElement>('#status')!;
const list = document.querySelector<HTMLElement>('#machines')!;

let audio: AudioContext | undefined;
let playing: { id: string; node: AudioWorkletNode; values: number[]; card: HTMLElement } | undefined;

const el = <K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = '') => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
};

// A normalized value as the Q31 word the kernel reads; enum steps sit in the middle of their range.
const word = (v: number) => Math.round(Math.min(1, Math.max(0, v)) * Q31);

async function open(m: Entry, card: HTMLElement) {
  stop();
  audio ??= new AudioContext({ sampleRate: 44100 });
  await audio.resume();
  await audio.audioWorklet.addModule('./worklet.mjs');
  const bytes = await (await fetch(`./data/${m.id}/engine.wasm`)).arrayBuffer();
  const values = m.controls.map((c) => c.default);
  const node = new AudioWorkletNode(audio, 'silicon', {
    outputChannelCount: [2],
    processorOptions: { bytes, parameters: values.map(word) },
  });
  node.port.onmessage = ({ data }) => { if (data.error) console.warn(m.id, data.error); };
  node.connect(audio.destination);
  playing = { id: m.id, node, values, card };
  card.classList.add('open');
}

function stop() {
  if (!playing) return;
  playing.node.port.postMessage({ type: 'panic' });
  playing.node.disconnect();
  playing.card.classList.remove('open');
  playing = undefined;
}

const send = () => playing?.node.port.postMessage({ type: 'parameters', value: playing.values.map(word) });
const note = (n: number, on: boolean) =>
  playing?.node.port.postMessage(on ? { type: 'on', note: n, velocity: 0.8 } : { type: 'off', note: n });

function controlRow(m: Entry, c: Control, n: number) {
  const row = el('label', 'control');
  row.append(el('span', 'label', c.label));
  if (c.labels) {
    const seg = el('span', 'segments');
    const count = c.labels.length;
    c.labels.forEach((label, k) => {
      const b = el('button', 'segment', label);
      b.type = 'button';
      const pick = () => {
        if (playing?.id !== m.id) return;
        playing.values[n] = (k + 0.5) / count;
        seg.querySelectorAll('.segment').forEach((s, j) => s.classList.toggle('on', j === k));
        send();
      };
      b.addEventListener('click', pick);
      if (k === Math.min(count - 1, Math.floor(c.default * count))) b.classList.add('on');
      seg.append(b);
    });
    row.append(seg);
  } else {
    const range = el('input');
    range.type = 'range'; range.min = '0'; range.max = '1'; range.step = '0.001';
    range.value = String(c.default);
    range.addEventListener('input', () => {
      if (playing?.id !== m.id) return;
      playing.values[n] = Number(range.value);
      send();
    });
    row.append(range);
  }
  return row;
}

function keyboard() {
  const kb = el('div', 'keys');
  for (let k = 0; k < 13; k++) {
    const black = [1, 3, 6, 8, 10].includes(k % 12);
    const b = el('button', black ? 'key black' : 'key');
    b.type = 'button';
    b.setAttribute('aria-label', `Note ${48 + k}`);
    const down = (e: PointerEvent) => { b.setPointerCapture(e.pointerId); note(48 + k, true); };
    const up = () => note(48 + k, false);
    b.addEventListener('pointerdown', down);
    b.addEventListener('pointerup', up);
    b.addEventListener('pointercancel', up);
    kb.append(b);
  }
  return kb;
}

function card(m: Entry) {
  const c = el('article', 'card machine');
  const head = el('div', 'head');
  head.append(el('h2', '', m.name), el('code', 'id', m.id));
  const chip = el('span', m.licence === 'MIT' ? 'chip' : 'chip gpl', m.licence);
  head.append(chip);
  c.append(head, el('p', 'desc', m.description));
  if (m.licence !== 'MIT') c.append(el('p', 'gplnote', 'Only this machine is GPL. All other machines are MIT.'));

  const facts = el('p', 'facts');
  facts.textContent = `${m.controls.length} controls · CPU ${m.cpu ? `about ${m.cpu.toLocaleString('en')} per voice (host estimate, not measured on the device)` : 'not yet estimated'}`;
  const src = el('a', '', 'source');
  src.href = repo + m.source;
  facts.append(' · ', src);
  c.append(facts);

  const panel = el('div', 'audition');
  m.controls.forEach((ctl, n) => panel.append(controlRow(m, ctl, n)));
  panel.append(keyboard(), el('p', 'hint', 'Keys A to K play an octave from C.'));

  const button = el('button', 'listen', 'Listen');
  button.type = 'button';
  button.disabled = !m.preview;
  if (!m.preview) button.title = 'No preview build yet';
  button.addEventListener('click', async () => {
    if (playing?.id === m.id) { stop(); button.textContent = 'Listen'; return; }
    document.querySelectorAll('.listen').forEach((b) => (b.textContent = 'Listen'));
    button.textContent = 'Loading …';
    try {
      await open(m, c);
      button.textContent = 'Stop';
    } catch (e) {
      button.textContent = 'Listen';
      status.textContent = `Could not play ${m.name}: ${(e as Error).message}`;
      status.hidden = false;
    }
  });
  c.append(button, panel);
  return c;
}

const held = new Set<string>();
addEventListener('keydown', (e) => {
  const k = keys.indexOf(e.key.toLowerCase());
  if (k < 0 || e.repeat || held.has(e.key) || e.target instanceof HTMLInputElement || !playing) return;
  held.add(e.key);
  note(48 + k, true);
});
addEventListener('keyup', (e) => {
  const k = keys.indexOf(e.key.toLowerCase());
  if (k < 0 || !held.delete(e.key)) return;
  note(48 + k, false);
});

fetch('./data/machines.json')
  .then((r) => r.json() as Promise<Entry[]>)
  .then((machines) => {
    // MIT machines first, the GPL one last, so the licence split reads at a glance.
    machines.sort((a, b) => Number(a.licence !== 'MIT') - Number(b.licence !== 'MIT'));
    list.append(...machines.map(card));
    status.hidden = true;
  })
  .catch(() => { status.textContent = 'The machine list is missing. Run node scripts/list.ts first.'; });
