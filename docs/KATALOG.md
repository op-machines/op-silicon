<!-- SPDX-License-Identifier: MIT -->
# Open-XY catalog checklist

Drawn from `open-xy/docs/catalog.md`, `docs/profiling.md` and `tooling/catalog.py`
(SDK commit 8d4bb49). The SDK's own documents win where they differ.

## Rules
- **ID** = `definition.json` `id`: `[a-z][a-z0-9_]{0,14}`, not a stock type, C++ keyword or reserved
  prefix, unique across the catalog, held for good (presets save it). Ours: `silsid`, `silay`,
  `silapu`, `silpaula` (checked with `stock_name` and `catalog.validate`, 2026-10-30).
- **Fields:** `reserved`: exactly `id kind maintainer status`. `draft`/`published`: plus
  `repository tag commit path` and a `cpu` record. Nothing else.
- `repository` `https://github.com/<owner>/<repo>`; `commit` the full 40-hex SHA of the tag;
  `path` `machines/<id>`; `maintainer` a GitHub user name.
- **Status:** `reserved` → `draft` → `published` (back to `draft` allowed), `retired` is final.
  Entries are never deleted.
- **CPU:** a hardware sweep (`opxy-synth-c8-sweep-v1`) of exactly the tagged commit; host
  estimates are refused. Recommended ≤ 3.5 % of the block budget (higher passes with a warning).
- **Controls** keep index, `id`, `mapping` across releases; changes need the `controls-changed` label.

## Each submission
1. PR against `main` that changes only `catalog/machines.json` (from a fork).
2. `python sdk.py catalog check --base origin/main` green.
3. `sdk.py test --machine machines/<id>` and `--firmware` green; CI in our repo green.
4. Licence allows redistribution; vendored licences included (`THIRD_PARTY.md`, per-machine `LICENSE`).
5. Description present (`machine.json`), README per machine.

## What maintainers review
- Only public headers: no assembly, raw addresses or platform code.
- Audio callbacks never allocate, block, log or do I/O.
- Sensible name, redistributable licence.
- Controls and header after `docs/ux.md` (5–8 controls: two fixed rows, whole-page switching).
- Sounds right in the browser preview from CI.

## Steps for op-silicon
1. Reserve: one PR adding four `reserved` entries.
2. Measure each machine on hardware (`python sdk.py profile`, open-xy docs/profiling.md), commit, tag `v1.0.0`.
3. PR with `draft` (or `published`) entries: release fields plus `cpu` from `catalog-cpu.json`.
