// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
//
// The MOS6581 as SiDJ plays it (reSID 0.16 in fixed point, sid.inc): the
// controls become its registers, as in OP-PLAYGROUND's "6581". Each voice
// owns a chip, so the filter is per note.
#include "core.h"
#include <opxy/q31.hpp>
#include "sid.inc"
#include "sid_tables.inc"

namespace silsid {
// Control 0..1 in Q31 to an index 0..n-1, and to 0..scale.
inline opxy::u32 pick(opxy::i32 p, opxy::u32 n) {
    const opxy::u32 i = (static_cast<opxy::u32>(p) >> 16) * n >> 15;
    return i < n ? i : n - 1;
}
inline opxy::u32 scaled(opxy::i32 p, opxy::u32 scale) {
    return ((static_cast<opxy::u32>(p) >> 16) * scale + 16384u) >> 15;
}

// Waveform bits: tri, saw, pulse, noise, then the combined ones.
const opxy::u8 waves[8] = {0x10, 0x20, 0x40, 0x80, 0x30, 0x50, 0x60, 0x70};
// Their levels evened out (Q8, measured at C4): the 6581's combined waves are quiet,
// up to +20 dB for them, so no stop of the knob is a dead zone.
const opxy::i32 wave_gain[8] = {320, 320, 203, 320, 2560, 362, 1440, 2560};
// Filter modes in $18; 0 routes nothing through the filter.
const opxy::u8 modes[6] = {0x00, 0x10, 0x20, 0x40, 0x50, 0x30};
// Arp chords in semitones and their lengths.
const opxy::u8 chords[6][4] = {{0}, {0, 12}, {0, 4, 7}, {0, 3, 7}, {0, 5, 7}, {0, 4, 7, 10}};
const opxy::u8 chord_length[6] = {1, 2, 3, 3, 3, 4};
// 2^(n/12) in Q16 for 0..12 semitones.
const opxy::u32 semitone[13] = {65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193,
                                104032, 110218, 116772, 123715, 131072};
// 44100 / 985248 (OP-XY rate over the PAL clock) in Q31.
const opxy::i32 rate_to_clock = 96123342;
// A PAL frame (C64 players write the SID once per frame) in samples.
const opxy::u32 frame_samples = 882;

inline void poke(Voice& voice, opxy::u32 reg, opxy::u32 value) {
    if (voice.written[reg] && voice.regs[reg] == value) return;
    voice.written[reg] = 1;
    voice.regs[reg] = static_cast<opxy::u8>(value);
    sid_write(&voice.chip, static_cast<opxy::u8>(reg), static_cast<opxy::u8>(value));
}

// The 16-bit frequency register for a phase step per sample, times ratio (Q16).
// (Unclamped: render folds what does not fit the register down, so it stays in tune.)
inline opxy::u32 frequency(opxy::u32 step, opxy::u32 ratio) {
    opxy::u32 f = static_cast<opxy::u32>(opxy::q31::multiply(static_cast<opxy::i32>(step >> 1), rate_to_clock)) >> 7;
    const opxy::u32 r = ratio >> 4;   // in two halves: no 64-bit multiply on the Blackfin
    return (f >> 12) * r + ((f & 4095) * r >> 12);
}
// Ring ratios that stay in tune (Q16): 1, 3/2 (sounds an octave down), 2, 3, 4.
const opxy::u32 ring_ratios[5] = {65536, 98304, 131072, 196608, 262144};
// The register for about 2.5 kHz at the PAL clock.
const opxy::u32 above_top = 42600;
// The register for about 900 Hz: above it ring modulation rests.
const opxy::u32 ring_top = 15300;

inline void initialize(Voice& voice, Shared&, opxy::u32) {
    sid_init(&voice.chip);
    sid_set_model(&voice.chip, SID_MOS6581);
    sid_set_sampling(&voice.chip, 985248, 22050);
    sid_set_averaging(&voice.chip, 1);
    sid_set_floating_dac(&voice.chip, 1);
    for (opxy::u32 r = 0; r < 0x19; ++r) voice.written[r] = 0;
    voice.samples = 0;
    voice.restart = 0;
    voice.last = 0;
}
inline void start(Voice& voice, Shared&, opxy::u32) { voice.samples = 0; voice.restart = 1; }
inline void destroy(Voice&, Shared&, opxy::u32) {}

inline void render(Voice& voice, Shared&, opxy::u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const opxy::u32 wave = pick(parameters[0], 8), bits = waves[wave];
    // Pulse duty 50 % down to 8 % (thinner only fades away), as the 12-bit register.
    const opxy::u32 pw = 2048 - scaled(parameters[1], 1720);
    // Cutoff over the register span the filter answers, 300 Hz to 4 kHz (its
    // ceiling at these steps): every bit of the knob moves the sound.
    const opxy::u32 fc = 386 + scaled(parameters[2], 944 - 386);
    const opxy::u32 res = scaled(parameters[3], 15);
    const opxy::u32 mode = modes[pick(parameters[4], 6)];
    // Mod: off below 4 %, then sync, then ring. Sync: voice 3 (the master) keeps the
    // note, voice 1 sweeps 1 to 4 times above it, so the pitch never moves, only the
    // colour. Ring: voice 1 keeps the note, voice 3 steps through ring_ratios.
    const opxy::u32 m = static_cast<opxy::u32>(parameters[5]) >> 16;  // Q15
    const opxy::u32 kind = m < 1311 ? 0 : m < 17039 ? 1 : 2;
    const opxy::u32 ratio = kind == 1 ? 65536 + (m - 1311) * 3 * 65536 / 15728
                          : kind == 2 ? ring_ratios[pick(static_cast<opxy::i32>((m - 17039) * 32768u / 15729u << 16), 5)]
                          : 65536;
    // Env: attack 0; decay grows as sustain falls (the stock envelope does the release).
    const opxy::u32 dr = 1 + scaled(parameters[6], 10), sustain = 15 - scaled(parameters[6], 15);
    const opxy::u32 arp = pick(parameters[7], 6);
    const opxy::u32 semis = chords[arp][(voice.samples / frame_samples) % chord_length[arp]];

    // The note and the arp step, an octave down while it would pass the register's top
    // (about 3.9 kHz), so high notes stay in tune. Above the note only the colour clips.
    opxy::u32 base = semitone[semis % 12] << (semis / 12);
    while (base > 4096 && frequency(pitch[0], base) > 65535) base >>= 1;
    const opxy::u32 note = frequency(pitch[0], base);
    // Above the note: at most about 2.5 kHz, where the 22.05 kHz chip still plays it
    // clean. Sync clips there (only the colour changes); ring steps down its ratios,
    // so it stays in tune.
    opxy::u32 above = frequency(pitch[0], base * (ratio >> 8) >> 8);
    if (kind == 2)
        for (int r = 4; r >= 0 && above > above_top; --r)
            if (ring_ratios[r] <= ratio || r == 0) above = frequency(pitch[0], base * (ring_ratios[r] >> 8) >> 8);
    if (above > above_top) above = above_top > note ? above_top : note;
    // Where nothing is left above the note, or ring's hard flips would fold back at the
    // chip's rate (notes over about 900 Hz), the note plays plain: no off-key partials.
    const opxy::u32 kind_on = (kind == 1 && above <= note + (note >> 4)) || (kind == 2 && note > ring_top) ? 0 : kind;
    const opxy::u32 f = kind_on == 1 ? above : note;
    const opxy::u32 f3 = kind_on == 1 ? note : kind_on == 2 ? above : 0;
    opxy::u32 control = bits | 1u | (kind_on == 1 ? 0x02u : kind_on == 2 ? 0x04u : 0u);
    poke(voice, 0, f & 0xff);
    poke(voice, 1, f >> 8);
    poke(voice, 2, pw & 0xff);
    poke(voice, 3, pw >> 8);
    poke(voice, 5, dr);
    poke(voice, 6, (sustain << 4) | dr);
    if (voice.restart) { poke(voice, 4, control & ~1u); voice.restart = 0; }
    poke(voice, 4, control);
    // Voice 3, silent (no waveform, 3OFF): the sync master or the ring modulator.
    poke(voice, 14, f3 & 0xff);
    poke(voice, 15, f3 >> 8);
    poke(voice, 18, 0);
    poke(voice, 0x15, fc & 7);
    poke(voice, 0x16, fc >> 3);
    poke(voice, 0x17, (res << 4) | (mode ? 0x01u : 0u));
    poke(voice, 0x18, (mode ? mode : 0x10u) | 0x80u | 0x0fu);

    // The chip runs at half the rate (22.05 kHz), interpolated up: at the full rate a
    // voice estimated about 26,000 Blackfin instructions a block, over the 24,000 limit.
    sid_render(&voice.chip, voice.buffer, opxy::block_frames / 2);
    // Band- and high-pass lose level as the cutoff rises: made up, so the knob only
    // changes the colour.
    const opxy::u32 lift = mode == 0x20 ? 640 : mode == 0x40 ? 1180 : mode == 0x50 ? 300 : 0;  // Q8 at full cutoff
    const opxy::i32 gain = static_cast<opxy::i32>(wave_gain[wave] * (256 + scaled(parameters[2], lift)) >> 8);
    opxy::i32 last = voice.last;
    for (unsigned i = 0; i < opxy::block_frames / 2; ++i) {
        opxy::i32 x = voice.buffer[i] * gain >> 8;
        x = x > 32767 ? 32767 : x < -32767 ? -32767 : x;
        output[2 * i] = (last + x) * 16384;
        output[2 * i + 1] = x * 32768;
        last = x;
    }
    voice.last = last;
    voice.samples += opxy::block_frames;
}
}
