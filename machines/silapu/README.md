<!-- SPDX-License-Identifier: MIT -->
# silapu: APU

The sound of the NES APU (Ricoh 2A03, NTSC) as an OP-XY synth. A clean-room
model written from the chip's public register documentation, **MIT** (`LICENSE`).

## Sound

Every OP-XY voice owns one APU at the NTSC clock (1.789773 MHz):

- **Pulse**: an 11-bit timer (`f = clock / 16 / (t + 1)`), so high notes land
  slightly off, and the four 8-step duty sequences (12.5, 25, 50, 75 %).
  Band-limited at its edges (polyBLEP). A timer below 8 mutes it, as on the chip.
- **Sweep** (120 Hz): the timer moves by `t >> shift` per divider period; up
  uses the first pulse channel's ones' complement, down mutes past the 11-bit range.
- **Triangle**: the 32-step sequence at the same note (`clock / 32 / (t + 1)`),
  no volume control, as on the chip.
- **Noise**: the 15-bit register, long mode (taps 0 and 1) or short (taps 0
  and 6, 93 steps), one of the sixteen NTSC periods picked from the note.
- **Envelope** (240 Hz): the 4-bit decay, or constant volume 15.
- **Mixer**: the documented nonlinear formulas for pulse and triangle/noise,
  then a high-pass near the console's 90 Hz output stage.

The SDK has no note-off; the stock envelope releases the note.

## Controls

| Page | 1 | 2 | 3 | 4 |
| --- | --- | --- | --- | --- |
| 1 | Duty (12.5/25/50/75) | Sweep (down, off, up) | Noise (off, long, short) | Env (const, decay 62–1000 ms) |
| 2 | Mix (pulse → both → triangle) | Vibrato | Lowpass | Gain |

## Screen (proposal)

Duty sequence and triangle steps as outlines, sixteen volume cells following
the decay while a note is held, lamps for pulse, triangle and noise. Grey at
rest, one accent colour (`palette_14`, a proposal: `#ff6a1a`). Colours and
layout are the user's to decide.

## Sources

Clean room: no emulator source code was read or used.

- NESdev Wiki, *APU* and its pages *APU Pulse*, *APU Triangle*, *APU Noise*,
  *APU Envelope*, *APU Sweep*, *APU Frame Counter*, *APU Mixer* (register
  behaviour, duty tables, noise periods, mixer formulas, output filters):
  <https://www.nesdev.org/wiki/APU>
- The DMC (sample channel) is not modelled.

## License

MIT, Copyright (c) 2026 op-machines.
