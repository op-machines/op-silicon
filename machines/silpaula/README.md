<!-- SPDX-License-Identifier: MIT -->
# silpaula: Paula

The Amiga's Paula sound chip as an OP-XY wavetable synth. A clean-room model
written from the published hardware documentation, **MIT** (`LICENSE`). The
waves are computed for this machine (`waves.h`); no samples are taken from
anywhere.

## Sound

Every OP-XY voice uses three of Paula's channels at the PAL clock (3.546895 MHz):

- **Carrier**: an 8-bit signed wave of 32, 64 or 128 samples, read at
  `clock / period` with an integer period and **no interpolation**, as the chip
  does: short loops and high notes alias.
- **Volume**: 6 bits, 0..64.
- **Attached modulator**: like Paula's channel attach, a second channel's
  words drive the carrier's volume (am) or period (fm), at the note's rate or
  at 6 Hz (am lfo, fm lfo). Depth mixes between the plain and the attached value.
- **Detuned copy**: a third channel, up to half a semitone sharp.
- **Envelope**: Paula has none; trackers slide the volume once per 50 Hz frame.
  Env does the same: hold, or 1 to 16 steps per frame.
- **Filter**: off, the A500's fixed 6 dB low-pass near 4.4 kHz, or that plus
  the switchable 12 dB "LED" filter at 3.3 kHz.

The SDK has no note-off; the stock envelope releases the note.

## Controls

| Page | 1 | 2 | 3 | 4 |
| --- | --- | --- | --- | --- |
| 1 | Wave (sine, tri, saw, square, pulse, organ, vox, grit) | Length (32, 64, 128) | Mod (off, am, fm, am lfo, fm lfo) | Depth |
| 2 | Env (hold, 1280–80 ms) | Filter (off, a500, led) | Detune | Gain |

## Screen (proposal)

The wave at its length and the modulator as outlines, sixteen volume cells
following the slide while a note is held, four channel lamps (carrier,
modulator, copy, filter LED). Grey at rest except the LED lamp when the LED
filter is on. One accent colour (`palette_14`, a proposal: `#ff6a1a`).
Colours and layout are the user's to decide.

## Sources

Clean room: no emulator source code was read or used.

- Commodore-Amiga, *Amiga Hardware Reference Manual*, 3rd edition, chapter 5
  "Audio Hardware" (period and sampling rate, 8-bit data, volume 0..64,
  attach for volume and period modulation, output filter), appendix on the
  audio registers.
- The filter corners (about 4.4 kHz 6 dB fixed, 3.3 kHz 12 dB switchable) are
  the published A500 figures; the exact curves were not measured.

## License

MIT, Copyright (c) 2026 op-machines.
