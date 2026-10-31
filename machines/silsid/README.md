<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# silsid: 6581

The MOS6581 sound chip as an OP-XY synth, played by reSID.

**This machine is GPL-2.0-or-later** (`LICENSE`, `COPYING`). The other machines
in this repository are MIT and contain no code from it.

## Sound

Every OP-XY voice owns a whole emulated chip: its voice 1 plays the note, its
voice 3 runs silent as the modulator for sync and ring (on the chip, voice 1
is synced by voice 3). The filter is therefore per note. The chip runs at
22.05 kHz and is interpolated to 44.1 kHz (at the full rate one voice estimated
about 26,000 Blackfin instructions per block, over the SDK's 24,000 limit). The
filter integrates in three equal steps a sample instead of reSID's 8-cycle steps,
and the external filter runs once a sample (its 16 kHz low-pass lies above the
Nyquist limit there). With that a voice estimates about 13,000 to 14,500 instructions;
the machine plays 6 voices.
The chip's gate opens on each note; its attack is 0, the release is the
OP-XY's own amp envelope (the SDK has no note-off for synths).

## Controls

| Page | Control | What it does |
|---|---|---|
| 1 | wave | tri, saw, pulse, noise, and the combined s+t, p+t, p+s, p+s+t |
| 1 | width | pulse duty 50 % down to 8 % |
| 1 | cutoff | the filter cutoff, 300 Hz to 4 kHz (the span the filter answers); band- and high-pass keep their level |
| 1 | reso | the 4-bit resonance register |
| 2 | filter | off, lp, bp, hp, notch, lp+bp |
| 2 | mod | off, then sync (the sound sweeps 1 to 4 times above the note, the pitch stays), then ring (ratios 1, 3/2, 2, 3, 4; ring acts on the triangle) |
| 2 | env | decay and sustain: decay grows as sustain falls |
| 2 | arp | off, oct, maj, min, sus, 7th, one step per PAL frame (50 Hz) |

## Origin

- `sid.h`, `sid.inc`, `sid_tables.h`, `sid_tables.inc`: SiDJ's fixed-point C
  port of reSID 0.16 (Copyright (C) 2004 Dag Lem; port Copyright (C) 2026
  Thomas Jansson), https://github.com/thomasj/fm1-sidj (`sid/`, commit
  1e74137). Changed for the Open-XY SDK, which allows only `.cpp/.h/.inc`
  sources, its own headers and no conditional compilation: `sid.c` and
  `sid_tables.c` renamed to `.inc`, include guards replaced by `#pragma once`,
  `<stdint.h>` replaced by `sid_stdint.h`, the `extern "C"` blocks removed. In `sid.inc`, `filter_clock` steps three times a sample (Q12
  coefficient) and `extfilt_clock` once a sample (2026-10-31).
  The code is otherwise unchanged.
- `engine.cpp`, `core.h`, `display.*`, `panel.svg.fragment`, `tests/`: ours,
  after OP-PLAYGROUND's "6581" screen and register mapping.

Chip names are used descriptively; no manufacturer logos.

## Only sweet spots

Every stop of every control sounds in tune and at a usable level: the wave
levels are evened out (the 6581's combined waves get up to +20 dB), notes above
the register's 3.9 kHz top play an octave down, sync and ring never move the
pitch, the sound above the note stops at about 2.5 kHz (where the 22.05 kHz chip
still plays it clean) and ring rests on notes above about 900 Hz.

## Presets and demo

`presets/`: `bass`, `arp-minor`, `sync-lead`. `demos/silsid/silsid-demo.m4a`
(38 s, 112 BPM, Am F C G) is these three sounds, rendered on the host from
`demos/silsid/song.cpp` with the engine as built for the device.
