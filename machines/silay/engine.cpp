// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// A clean-room AY-3-8910 / YM2149 voice from the datasheet's register model:
// 12-bit tone periods (f = clock / 16 / TP), a 5-bit noise period, the
// mixer's AND of tone and noise, sixteen envelope shapes from the CONT, ATT,
// ALT and HOLD bits, and a 16-level logarithmic volume. Clock: 2 MHz.
#include "core.h"
#include <opxy/q31.hpp>

namespace silay {
using opxy::i32;
using opxy::u32;

// 4-bit volume, about 3 dB per step as the datasheet's output curve; 0 is silent. Q15.
const i32 volume[16] = {0, 260, 368, 519, 734, 1036, 1464, 2067, 2920, 4125, 5827, 8231, 11626, 16422, 23197, 32767};
// clock / 16 * 2^32 / 44100, shifted right by 8: tone period from a phase increment.
const u32 tone_constant = 47554467u;
// clock / 16 / 44100 in Q16: noise shifts per sample at noise period 1.
const u32 noise_constant = 185760u;
// Envelope shape register values for the Shape choices 1..8 (0 is off).
const u32 shapes[9] = {0, 9, 8, 10, 11, 12, 13, 14, 15};

inline u32 choice(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

// 2^(x / 65536) in Q16 for x in [0, 15 * 65536).
inline u32 exp2_q16(u32 x) {
    const u32 f = x & 0xffffu;
    const u32 m = 65536u + ((f * (43024u + ((f * 22512u) >> 16))) >> 16);
    return m << (x >> 16);
}

// The chip's pitch: the nearest 12-bit tone period, and the increment it really plays.
inline u32 quantize(u32 increment) {
    u32 coarse = increment >> 8;
    if (coarse == 0) coarse = 1;
    u32 period = (tone_constant + coarse / 2) / coarse;
    period = period < 1 ? 1 : period > 4095 ? 4095 : period;
    return (tone_constant / period) << 8;
}

// Envelope level 0..15 for shape bits CONT=8, ATT=4, ALT=2, HOLD=1.
inline u32 envelope(u32 shape, u32 cycle, u32 step) {
    const bool attack = shape & 4;
    if (cycle == 0) return attack ? step : 15 - step;
    if (!(shape & 8)) return 0;
    if (shape & 1) return (attack != static_cast<bool>(shape & 2)) ? 15 : 0;
    const bool up = attack != ((shape & 2) && (cycle & 1));
    return up ? step : 15 - step;
}

// Rough sine of a 32-bit phase in Q15 (parabola).
inline i32 sine(u32 phase) {
    const i32 x = static_cast<i32>(phase) >> 16;  // -32768..32767 over one cycle
    const i32 y = (x * (32768 - (x < 0 ? -x : x))) >> 13;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

// PolyBLEP residual in Q16 for an upward step at phase 0; reciprocal = 2^32 / (increment >> 16).
inline i32 blep(u32 phase, u32 increment, u32 reciprocal) {
    if (phase < increment) {
        const i32 t = static_cast<i32>(((phase >> 16) * reciprocal) >> 16);
        const i32 d = 65536 - t;
        return -((d * d) >> 16);
    }
    if (phase > ~increment) {
        const i32 u = static_cast<i32>((((0u - phase) >> 16) * reciprocal) >> 16);
        const i32 d = 65536 - u;
        return (d * d) >> 16;
    }
    return 0;
}

inline void reset(Voice& voice) {
    voice.phase_a = 0; voice.phase_b = 0x40000000u;
    voice.noise = 1; voice.noise_clock = 0;
    voice.env_phase = 0; voice.env_cycle = 0;
    voice.lfo = 0; voice.filtered = 0;
}

inline void initialize(Voice& voice, Shared&, u32) { reset(voice); }
inline void start(Voice& voice, Shared&, u32) {
    const u32 lfo = voice.lfo, noise = voice.noise;
    reset(voice);
    voice.lfo = lfo;  // free-running vibrato and noise, as on the chip
    voice.noise = noise;
}
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& voice, Shared&, u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const u32 mixer = choice(parameters[0], 3);  // 0 tone, 1 tone AND noise, 2 noise
    const u32 noise_period = 31 - choice(parameters[1], 31);  // 31..1, brighter to the right
    const u32 shape = shapes[choice(parameters[2], 9)];
    const i32 rate = parameters[3] >> 16;  // Q15
    const i32 detune = parameters[4] >> 16;
    const i32 vibrato = parameters[5] >> 16;
    const i32 lowpass = parameters[6] >> 16;
    const i32 gain = parameters[7] >> 15;  // 0..65535, 0.5 = unity at half scale

    // Vibrato: 5.5 Hz, up to half a semitone (2.9 %), applied once per block like a register write.
    voice.lfo += 5357u * 64u;
    u32 base = pitch[0];
    const i32 bend = (sine(voice.lfo) * vibrato >> 15) * 950 >> 15;  // Q15 fraction, up to 0.029
    base += static_cast<u32>(static_cast<i32>(base >> 15) * bend);
    const u32 inc_a = quantize(base);
    const bool two = detune > 0;
    const u32 inc_b = two ? quantize(base + (base >> 15) * static_cast<u32>(detune * 983 >> 15)) : 0;  // up to 3 %, half a semitone
    const u32 recip_a = 0xffffffffu / ((inc_a >> 16) | 1);
    const u32 recip_b = two ? 0xffffffffu / ((inc_b >> 16) | 1) : 0;
    const u32 noise_step = noise_constant / noise_period;

    // Envelope: below 70 % a free rate, 0.1 to 51 Hz per cycle; above, locked to the tone (buzz).
    u32 env_step;
    if (rate < 22938) {
        const u32 e = exp2_q16(static_cast<u32>(rate) * 18432u / 22938u << 5);
        env_step = (e >> 8) * 9739u >> 8;
    } else {
        const u32 ratio = choice((rate - 22938) * 32767 / 9830 << 16, 4);  // /4, /2, 1:1, x2
        env_step = ratio == 3 ? inc_a << 1 : inc_a >> (2 - ratio);
    }
    const i32 fixed = volume[15];
    const i32 smooth = 32767 - (lowpass * 31 >> 5);  // 1 = open
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        // Noise: shift the 17-bit register (taps 0 and 3) as often as the period asks.
        voice.noise_clock += noise_step;
        while (voice.noise_clock >= 65536u) {
            voice.noise_clock -= 65536u;
            const u32 bit = (voice.noise ^ (voice.noise >> 3)) & 1u;
            voice.noise = (voice.noise >> 1) | (bit << 16);
        }
        const bool noise_bit = voice.noise & 1u;

        i32 level = fixed;
        if (shape) {
            level = volume[envelope(shape, voice.env_cycle, voice.env_phase >> 28)];
            const u32 next = voice.env_phase + env_step;
            if (next < voice.env_phase) voice.env_cycle = voice.env_cycle == 1 ? 2 : 1;
            voice.env_phase = next;
        }

        i32 sum = 0;
        for (unsigned c = 0; c < (two ? 2u : 1u); ++c) {
            const u32 phase = c ? voice.phase_b : voice.phase_a;
            const u32 inc = c ? inc_b : inc_a;
            const u32 recip = c ? recip_b : recip_a;
            i32 wave;  // Q16, -65536..65536
            if (mixer == 0) {
                wave = (phase < 0x80000000u ? 65536 : -65536) + blep(phase, inc, recip) - blep(phase + 0x80000000u, inc, recip);
            } else {
                const bool tone_bit = mixer == 2 || phase < 0x80000000u;
                wave = tone_bit && noise_bit ? 65536 : -65536;
            }
            sum += (wave >> 2) * level >> 14;  // Q15
            if (c) voice.phase_b += inc; else voice.phase_a += inc;
        }
        if (two) sum >>= 1;
        voice.filtered += ((sum - voice.filtered) * smooth) >> 15;
        i32 out = (voice.filtered * gain) >> 16;
        out = out < -16384 ? -16384 : out > 16384 ? 16384 : out;
        output[i] = out * 65536;
    }
}
}
