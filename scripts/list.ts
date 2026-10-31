// SPDX-License-Identifier: MIT
// Writes site/public/data/machines.json from machines/<id>/, and copies each machine's
// preview build (engine.wasm, panel.svg, manifest.json) next to it when one is given.
//   node scripts/list.ts [--previews <dir with <id>-preview/ folders>]
// Exits 1 when a machine's SPDX heads and its LICENSE disagree.
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const root = join(import.meta.dirname, '..');
const out = join(root, 'site/public/data');
const i = process.argv.indexOf('--previews');
const previews = i > 0 ? process.argv[i + 1] : undefined;

const read = (f: string) => (existsSync(f) ? readFileSync(f, 'utf8') : '');
const licenceOf = (text: string) =>
  text.includes('GNU GENERAL PUBLIC LICENSE') ? 'GPL-2.0-or-later' : /^MIT License/m.test(text) ? 'MIT' : '';

type Param = { id: string; label: string; default: number; mapping: { kind: string; labels?: string[] } };
let failed = false;
const list = [];
mkdirSync(out, { recursive: true });

for (const id of readdirSync(join(root, 'machines')).sort()) {
  const dir = join(root, 'machines', id);
  const def = JSON.parse(read(join(dir, 'definition.json')) || 'null');
  if (!def) continue;
  const meta = JSON.parse(read(join(dir, 'machine.json')) || '{}');
  const licence = licenceOf(read(join(dir, 'LICENSE')));
  const spdx = read(join(dir, 'engine.cpp')).match(/SPDX-License-Identifier: ([\w.+-]+)/)?.[1];
  if (!licence || spdx !== licence) {
    console.error(`${id}: LICENSE (${licence || 'unbekannt'}) passt nicht zum SPDX-Kopf (${spdx ?? 'fehlt'})`);
    failed = true;
    continue;
  }
  // CPU estimate from the machine's report (reports/ stays local), else none.
  const report = readdirSync(join(root, 'reports')).filter((f) => f.endsWith('.md'))
    .map((f) => read(join(root, 'reports', f))).find((r) => new RegExp(`^# .*: ${id}\\b`).test(r));
  const hit = report?.match(/(?:about|ca\.) \**([\d,.]+)\** Blackfin/);
  const cpu = hit?.[1].replace(/[,.]/g, '');
  let preview = false;
  if (previews) {
    const src = join(previews, `${id}-preview`);
    if (existsSync(join(src, 'engine.wasm'))) {
      mkdirSync(join(out, id), { recursive: true });
      for (const f of ['engine.wasm', 'panel.svg', 'manifest.json']) copyFileSync(join(src, f), join(out, id, f));
      preview = true;
    } else console.warn(`${id}: kein Preview-Build in ${src}`);
  } else preview = existsSync(join(out, id, 'engine.wasm'));
  list.push({
    id,
    name: def.name,
    description: meta.description ?? '',
    licence,
    controls: (def.parameters as Param[]).map((p) => ({ id: p.id, label: p.label, default: p.default, labels: p.mapping.labels })),
    cpu: cpu ? Number(cpu) : null,
    source: `machines/${id}/`,
    preview,
  });
}

writeFileSync(join(out, 'machines.json'), JSON.stringify(list, null, 1) + '\n');
console.log(`site/public/data/machines.json: ${list.length} Maschinen`);
if (failed) process.exit(1);
