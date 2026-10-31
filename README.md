# op-silicon

Chip synths for OP-XY: classic sound chips as machines for the
[Open-XY SDK](https://github.com/etcetc000/open-xy).

**Unofficial and experimental.** Not affiliated with teenage engineering;
OP-XY is their trademark.

## Machines

| ID | Chip | Licence |
|---|---|---|
| `silsid` | SID (6581/8580), via reSID | GPL-2.0-or-later |
| `silay` | AY-3-8910 / YM2149 | MIT |
| `silapu` | NES 2A03 sound | MIT |
| `silpaula` | Amiga Paula wavetable | MIT |

Each machine in `machines/<id>/` carries its own `LICENSE`. **GPL code lives only
in `machines/silsid/`**; everything else, including the repository frame, is MIT.
The non-SID machines are clean-room work from public datasheets and
documentation; each machine's README lists its sources.

## Build and install

Follow the Open-XY documentation
([first machine](https://github.com/etcetc000/open-xy/blob/staging/docs/first-machine.md),
[build](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)), e.g.
`python sdk.py test --machine path/to/op-silicon/machines/<id>`.

`sh scripts/check-licenses.sh` checks the licence separation.
