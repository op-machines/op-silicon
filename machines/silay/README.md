<!-- SPDX-License-Identifier: MIT -->
# silay: AY

The AY-3-8910 / YM2149 sound chip (Atari ST, ZX Spectrum, MSX) as an OP-XY
synth. A clean-room model written from the chip's published register
description, **MIT** (`LICENSE`).

## Sound

Every OP-XY voice owns one chip at a 2 MHz clock:

- **Tone A** plays the note as a square wave. The chip can only play
  `clock / 16 / TP` with a 12-bit period `TP`, so high notes land slightly
  off, as on the real thing. Tone-only squares are band-limited (polyBLEP).
- **Tone B** joins when Detune is above zero, up to half a semitone sharp.
- **Noise**: a 17-bit shift register clocked at `clock / 16 / NP` (5-bit period).
- **Mixer**: the chip ANDs tone and noise, so "t+n" gates the square with noise.
- **Envelope**: the hardware envelope, 16 steps, shapes from the CONT, ATT,
  ALT and HOLD bits (the eight distinct ones). Above 70 % the rate locks to
  the tone's period at /4, /2, 1:1 or x2: the "buzz" sound.
- **Volume**: 16 logarithmic levels, about 3 dB apart.

The SDK has no note-off; the stock envelope releases the note.

## Controls

| Page | 1 | 2 | 3 | 4 |
| --- | --- | --- | --- | --- |
| 1 | Mixer (tone, t+n, noise) | Noise (period 31..1) | Shape (off + 8 shapes) | Rate (0.1–51 Hz, then buzz /4 /2 1:1 x2) |
| 2 | Detune | Vibrato (5.5 Hz, ±½ semitone, per block) | Lowpass | Gain |

## Screen (proposal)

Envelope shape and mixer wave as outlines, sixteen volume cells following the
envelope while a note is held, lamps for tone A, tone B and noise. Grey at
rest, one accent colour (`palette_14`, a proposal: `#ff6a1a`). Colours and
layout are the user's to decide.

## Sources

Clean room: no emulator source code was read or used.

- General Instrument, *AY-3-8910/8912 Programmable Sound Generator Data Manual*
  (registers, tone/noise/envelope periods, mixer, envelope shape table, output
  level curve). Copy: <https://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf>
- Yamaha, *YM2149 Software-Controlled Sound Generator* datasheet (same
  register model, 32-step envelope; not modelled here).
- The noise register's taps (17 bits, taps 0 and 3) follow the public
  description of the chip; the datasheet does not give them. Not measured.

## License

MIT, Copyright (c) 2026 op-machines.
