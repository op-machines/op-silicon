// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// A clean-room Amiga Paula voice from the Amiga Hardware Reference Manual:
// 8-bit signed samples read without interpolation at clock / period (PAL
// clock 3.546895 MHz, integer periods), 6-bit volume 0..64, attached channels
// where one channel's data words drive the next one's volume or period, and
// the A500 output filters: a fixed 6 dB low-pass near 4.4 kHz and the
// switchable 12 dB "LED" filter at 3.3 kHz. The waves are our own.
#include "core.h"
#include "waves.h"
#include <opxy/q31.hpp>

namespace silpaula {
using opxy::i32;
using opxy::u32;

// clock * 2^32 / 44100 / length, shifted right by 8, for 32, 64 and 128 samples: period from an increment.
const u32 period_constant[3] = {42167675u, 21083837u, 10541918u};
const u32 min_period = 124u;  // fastest audio DMA fetch
const u32 tick_rate = 74u;  // 50 Hz in Q16 per sample
const i32 rc_coefficient = 15312;  // one-pole at 4.42 kHz, Q15
const i32 led_frequency = 15151, led_damping = 23170;  // two-pole 3.275 kHz, Q15 / Q14 (sqrt 2)

inline u32 choice(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

// The integer period nearest to an increment, and the increment it really plays.
inline u32 quantize(u32 increment, u32 constant) {
    u32 coarse = increment >> 8;
    if (coarse == 0) coarse = 1;
    u32 period = (constant + coarse / 2) / coarse;
    period = period < 1 ? 1 : period > 65535 ? 65535 : period;
    return (constant / period) << 8;
}

inline void initialize(Voice& voice, Shared&, u32) {
    voice.carrier = 0; voice.modulator = 0; voice.copy = 0; voice.tick = 0;
    voice.volume = 64 << 8; voice.rc = 0; voice.low = 0; voice.band = 0;
}
inline void start(Voice& voice, Shared& shared, u32 owner) { initialize(voice, shared, owner); }
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& voice, Shared&, u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const signed char* wave = waves[choice(parameters[0], 8)];
    const u32 base = pitch[0];
    // 32, 64, 128 samples; high notes drop to a shorter wave so the period stays at
    // the DMA minimum of 124 or above, as on the chip.
    u32 length = choice(parameters[1], 3);
    while (length > 0 && period_constant[length] / ((base >> 8) | 1) < min_period) --length;
    const u32 skip = 2 - length;                  // read every 4th, 2nd or every sample
    const u32 shift = 27 - length;                // phase bits below the index
    const u32 mode = choice(parameters[2], 5);    // off, AM, FM, AM slow, FM slow
    const i32 depth = parameters[3] >> 16;
    const u32 slide = choice(parameters[4], 17);  // 0 hold, else volume steps per 50 Hz tick
    const u32 filter = choice(parameters[5], 3);  // off, A500, LED
    const i32 detune = parameters[6] >> 16;
    const i32 gain = parameters[7] >> 15;

    const u32 inc = quantize(base, period_constant[length]);
    const bool doubled = detune > 0;
    const u32 inc_copy = doubled ? quantize(base + (base >> 15) * static_cast<u32>(detune * 983 >> 15), period_constant[length]) : 0;
    // The modulator plays its 32 words once per note period, or at 6 Hz for the slow modes.
    const u32 inc_mod = mode >= 3 ? 584351u : base;

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        voice.tick += tick_rate;
        if (voice.tick >= 65536u) {
            voice.tick -= 65536u;
            voice.volume -= static_cast<i32>(slide) << 8;
            if (voice.volume < 0) voice.volume = 0;
        }
        const i32 word = modulator_wave[voice.modulator >> 27];
        voice.modulator += inc_mod;

        i32 volume = voice.volume >> 8;  // 0..64
        u32 step = inc;
        if (mode == 1 || mode == 3) {
            // Attached volume: the word (0..64) replaces the volume, mixed by depth.
            const i32 attached = (word + 128) >> 2;
            volume = volume * (32768 - depth + (depth * attached >> 6)) >> 15;
        } else if (mode == 2 || mode == 4) {
            // Attached period: the word bends the period by up to a half.
            const i32 bend = (word * depth) >> 8;  // Q15 fraction, |bend| < 0.5
            step = inc - static_cast<u32>(static_cast<i32>(inc >> 15) * bend);
        }

        i32 sum = wave[(voice.carrier >> shift) << skip] * volume;  // +-8128
        voice.carrier += step;
        if (doubled) {
            sum = (sum + wave[(voice.copy >> shift) << skip] * volume) >> 1;
            voice.copy += inc_copy;
        }
        i32 x = sum << 2;  // Q15
        if (filter) {
            voice.rc += ((x - voice.rc) * rc_coefficient) >> 15;
            x = voice.rc;
            if (filter == 2) {
                voice.low += (led_frequency * voice.band) >> 15;
                const i32 high = x - voice.low - ((led_damping * voice.band) >> 14);
                voice.band += (led_frequency * high) >> 15;
                x = voice.low;
            }
        }
        i32 out = (x * gain) >> 16;
        out = out < -16384 ? -16384 : out > 16384 ? 16384 : out;
        output[i] = out * 65536;
    }
}
}
