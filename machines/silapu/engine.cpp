// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// A clean-room NES APU (Ricoh 2A03, NTSC clock 1.789773 MHz) from its public
// register documentation: an 11-bit pulse timer with four duty sequences,
// the 32-step triangle, the 15-bit noise register in long and short mode,
// the frame counter's 4-bit decay envelope (240 Hz) and sweep (120 Hz), and
// the nonlinear mixer, followed by a high-pass near the console's 90 Hz.
#include "core.h"
#include <opxy/q31.hpp>

namespace silapu {
using opxy::i32;
using opxy::u32;

// Mixer, scaled to Q15 x 1.8: pulse 95.88 / (8128 / p + 100), tnd 163.67 / (24329 / (3t + 2n) + 100).
const i32 pulse_table[16] = {0, 687, 1358, 2013, 2652, 3277, 3888, 4484, 5067, 5638, 6195, 6741, 7275, 7798, 8309, 8810};
const i32 tnd_table[76] = {0, 395, 787, 1176, 1561, 1944, 2323, 2700, 3073, 3444, 3811, 4176, 4538, 4897, 5253, 5606,
    5957, 6305, 6650, 6993, 7333, 7670, 8005, 8338, 8668, 8995, 9320, 9643, 9963, 10281, 10597, 10910, 11221, 11530,
    11836, 12141, 12443, 12743, 13041, 13337, 13630, 13922, 14212, 14499, 14785, 15068, 15350, 15629, 15907, 16183,
    16457, 16729, 16999, 17268, 17534, 17799, 18062, 18324, 18583, 18841, 19097, 19352, 19605, 19856, 20105, 20353,
    20599, 20844, 21087, 21329, 21569, 21808, 22045, 22280, 22514, 22747};
// clock / 16 * 2^32 / 44100, shifted right by 8: pulse timer (t + 1) from a phase increment.
const u32 timer_constant = 42555850u;
// clock / 44100 in Q16: noise timer counts per sample.
const u32 noise_constant = 2659741u;
// The noise channel's sixteen NTSC periods in CPU cycles.
const u32 noise_periods[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};
// 240 Hz in Q16 per sample.
const u32 quarter_frame = 357u;

inline u32 choice(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

// The timer value (t + 1) nearest to an increment, for a timer constant.
inline u32 timer(u32 increment, u32 constant) {
    u32 coarse = increment >> 8;
    if (coarse == 0) coarse = 1;
    const u32 period = (constant + coarse / 2) / coarse;
    return period < 1 ? 1 : period > 2048 ? 2048 : period;
}

inline i32 sine(u32 phase) {
    const i32 x = static_cast<i32>(phase) >> 16;
    const i32 y = (x * (32768 - (x < 0 ? -x : x))) >> 13;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

// PolyBLEP residual in Q16 for an upward step of 2 at phase 0.
inline i32 blep(u32 phase, u32 increment, u32 reciprocal) {
    if (phase < increment) {
        const i32 d = (65536 - static_cast<i32>(((phase >> 16) * reciprocal) >> 16)) >> 1;   // halved: 65536^2 does not fit 32 bits
        return -((d * d) >> 14);
    }
    if (phase > ~increment) {
        const i32 d = (65536 - static_cast<i32>((((0u - phase) >> 16) * reciprocal) >> 16)) >> 1;   // halved: 65536^2 does not fit 32 bits
        return (d * d) >> 14;
    }
    return 0;
}

inline void reset(Voice& voice) {
    voice.phase_pulse = 0; voice.phase_triangle = 0;
    voice.noise = 1; voice.noise_clock = 0;
    voice.frame_clock = 0; voice.half = 0;
    voice.env_divider = 0; voice.env_level = 15;
    voice.sweep_divider = 0; voice.period = 0; voice.muted = 0;
    voice.lfo = 0; voice.dc = 0; voice.filtered = 0;
}

inline void initialize(Voice& voice, Shared&, u32) { reset(voice); }
inline void start(Voice& voice, Shared&, u32) {
    const u32 lfo = voice.lfo, noise = voice.noise;
    reset(voice);
    voice.lfo = lfo;
    voice.noise = noise;
}
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& voice, Shared&, u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const u32 duty = choice(parameters[0], 4);  // 12.5, 25, 50, 75 %
    const i32 sweep = (parameters[1] >> 16) - 16384;  // left down, centre off, right up
    const u32 noise_mode = choice(parameters[2], 3);  // off, long, short
    const u32 env = choice(parameters[3], 17);  // 0 constant, else decay period 0..15
    const i32 mix = parameters[4] >> 16;  // pulse .. both .. triangle
    const i32 vibrato = parameters[5] >> 16;
    const i32 lowpass = parameters[6] >> 16;
    const i32 gain = parameters[7] >> 15;

    voice.lfo += 5357u * 64u;
    u32 base = pitch[0];
    const i32 bend = (sine(voice.lfo) * vibrato >> 15) * 950 >> 15;
    base += static_cast<u32>(static_cast<i32>(base >> 15) * bend);

    // Sweep: |amount| sets the shift (7..1) and the divider (7..0); off near the centre.
    const i32 amount = sweep < 0 ? -sweep : sweep;
    const bool sweeping = amount >= 1024;
    const u32 strength = sweeping ? static_cast<u32>(amount - 1024) * 15u / 15360u : 0;
    const u32 shift = 7 - strength * 6 / 15, sweep_period = 7 - strength * 7 / 15;
    if (!sweeping || voice.period == 0) { voice.period = timer(base, timer_constant); voice.muted = 0; }

    u32 inc_pulse = (timer_constant / voice.period) << 8;
    const u32 inc_triangle = ((timer_constant >> 1) / timer(base, timer_constant >> 1)) << 8;
    const u32 recip = 0xffffffffu / ((inc_pulse >> 16) | 1);

    // Noise period from the note: higher notes, shorter periods.
    u32 zeros = 0;
    for (u32 v = base; v && !(v & 0x80000000u); v <<= 1) ++zeros;
    i32 index = (static_cast<i32>(zeros) - 1) * 3 / 2;
    index = index < 0 ? 0 : index > 15 ? 15 : index;
    const u32 noise_step = noise_constant / noise_periods[index];

    // Duty: the sequence is high from 1/8 to the duty's end (75 % is 25 % inverted).
    const u32 rise = 0x20000000u;
    const u32 fall = duty == 0 ? 0x40000000u : duty == 2 ? 0xa0000000u : 0x60000000u;
    const bool inverted = duty == 3;
    const i32 pulse_scale = mix < 16384 ? 32767 : (32767 - mix) * 2;
    const bool triangle_on = mix > 6554;
    // The console's fixed 14 kHz one-pole low-pass, darkened further by the control.
    const i32 smooth = (32767 - (lowpass * 31 >> 5)) * 28316 >> 15;

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        voice.frame_clock += quarter_frame;
        if (voice.frame_clock >= 65536u) {
            voice.frame_clock -= 65536u;
            if (env) {
                if (voice.env_divider == 0) { voice.env_divider = env - 1; if (voice.env_level) --voice.env_level; }
                else --voice.env_divider;
            }
            voice.half ^= 1u;
            if (voice.half && sweeping) {
                if (voice.sweep_divider == 0) {
                    voice.sweep_divider = sweep_period;
                    const u32 change = (voice.period - 1) >> shift;
                    // Rising pitch is the first pulse channel's negate: ones' complement.
                    if (sweep > 0) voice.period = change + 1 < voice.period ? voice.period - change - 1 : 1;
                    else if (voice.period + change > 0x800u) voice.muted = 1;
                    else voice.period += change;
                    inc_pulse = (timer_constant / voice.period) << 8;
                } else --voice.sweep_divider;
            }
        }
        const u32 volume = env ? voice.env_level : 15;
        const bool silent = voice.muted || voice.period < 9;  // timer below 8 mutes the channel

        // Pulse, band-limited at its two edges; level in Q16 of 0..15.
        const u32 p = voice.phase_pulse;
        i32 wave = (p >= rise && p < fall) ? 65536 : 0;
        wave += (blep(p - rise, inc_pulse, recip) - blep(p - fall, inc_pulse, recip)) >> 1;
        if (inverted) wave = 65536 - wave;
        const u32 pulse_volume = silent ? 0 : static_cast<u32>(static_cast<i32>(volume) * pulse_scale >> 15);
        i32 level = wave * static_cast<i32>(pulse_volume);  // Q16 of 0..15
        level = level < 0 ? 0 : level > 15 * 65536 ? 15 * 65536 : level;
        const u32 index_p = static_cast<u32>(level) >> 16, frac = static_cast<u32>(level) & 0xffffu;
        const i32 pulse_out = pulse_table[index_p] + (index_p < 15 ? static_cast<i32>(((pulse_table[index_p + 1] - pulse_table[index_p]) * static_cast<i32>(frac >> 1)) >> 15) : 0);
        voice.phase_pulse = p + inc_pulse;

        const u32 step = voice.phase_triangle >> 27;
        const u32 triangle = triangle_on ? (step < 16 ? 15 - step : step - 16) : 0;
        voice.phase_triangle += inc_triangle;

        u32 noise = 0;
        if (noise_mode) {
            voice.noise_clock += noise_step;
            while (voice.noise_clock >= 65536u) {
                voice.noise_clock -= 65536u;
                const u32 bit = (voice.noise ^ (voice.noise >> (noise_mode == 2 ? 6 : 1))) & 1u;
                voice.noise = (voice.noise >> 1) | (bit << 14);
            }
            noise = voice.noise & 1u ? 0 : volume;
        }

        const i32 mixed = pulse_out + tnd_table[3 * triangle + 2 * noise];
        // The console's 90 Hz one-pole high-pass (coefficient 0.0127 in Q15); DC kept in Q8.
        voice.dc += (((mixed << 8) - voice.dc) >> 7) * 417 >> 8;
        const i32 high = mixed - (voice.dc >> 8);
        voice.filtered += ((high - voice.filtered) * smooth) >> 15;
        i32 out = (voice.filtered * gain) >> 16;
        out = out < -16384 ? -16384 : out > 16384 ? 16384 : out;
        output[i] = out * 65536;
    }
}
}
