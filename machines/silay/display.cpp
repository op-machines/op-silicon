// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// The screen: the envelope shape and the mixer's wave as outlines, sixteen
// volume cells that follow the envelope while a note is held, and lamps for
// tone A, tone B and noise. At rest everything is grey and still.
#include "display.h"

namespace silay {
using opxy::i32;
using opxy::u32;

const u32 first_cell = 10, first_lamp = 26;
const u32 dim = 3, idle = 6, lit = 12, accent = 14;  // palette entries (a proposal)

inline u32 pick(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

inline u32 env_level(u32 shape, u32 cycle, u32 step) {
    const bool attack = shape & 4;
    if (cycle == 0) return attack ? step : 15 - step;
    if (!(shape & 8)) return 0;
    if (shape & 1) return (attack != static_cast<bool>(shape & 2)) ? 15 : 0;
    const bool up = attack != ((shape & 2) && (cycle & 1));
    return up ? step : 15 - step;
}

inline void number(char* out, u32 value, const char* suffix) {
    char digits[8];
    u32 n = 0;
    do { digits[n++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && n < 5);
    u32 k = 0;
    while (n) out[k++] = digits[--n];
    while (*suffix && k < opxy::draw::header_capacity - 1) out[k++] = *suffix++;
    out[k] = 0;
}

// A ribbon outline through 32 heights (pixels, y down), 120 px wide.
template <typename P>
inline void ribbon(P& path, const i32 (&y)[32]) {
    using opxy::draw::unit;
    for (u32 i = 0; i < 32; ++i) {
        const i32 x = (-62 + static_cast<i32>(i) * 4) * unit;
        path.points[i] = {x, y[i] * unit};
        path.points[63 - i] = {x, (y[i] + 2) * unit};
    }
}

inline void display_reset(DisplayState& state) {
    state.header.reset();
    state.serial = 0; state.env_phase = 0; state.env_cycle = 0;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    const u32 shapes[9] = {0, 9, 8, 10, 11, 12, 13, 14, 15};
    const char* const mixers[3] = {"tone", "t+n", "noise"};
    const char* const shape_names[9] = {"off", "decay", "saw dn", "tri", "dec hi", "saw up", "attack", "tri up", "att 0"};
    const char* const syncs[4] = {"/4", "/2", "1:1", "x2"};
    const u32 mixer = pick(input.slots[0], 3);
    const u32 noise_period = 31 - pick(input.slots[1], 31);
    const u32 shape_index = pick(input.slots[2], 9), shape = shapes[shape_index];
    const i32 rate = input.slots[3] >> 16;

    const char* const titles[8] = {"Mixer", "Noise", "Shape", "Rate", "Detune", "Vibrato", "Lowpass", "Gain"};
    char values[8][header_capacity];
    copy(values[0], header_capacity, mixers[mixer]);
    number(values[1], noise_period, "");
    copy(values[2], header_capacity, shape_names[shape_index]);
    u32 tenths = 0;  // envelope cycles per second, in tenths
    if (rate < 22938) {
        const u32 x = static_cast<u32>(rate) * 18432u / 22938u << 5;
        const u32 f = x & 0xffffu;
        tenths = (65536u + ((f * (43024u + ((f * 22512u) >> 16))) >> 16)) << (x >> 16) >> 16;
        number(values[3], tenths, "/10s");
        if (tenths >= 10) number(values[3], tenths / 10, "Hz");
    } else {
        copy(values[3], header_capacity, syncs[pick((rate - 22938) * 32767 / 9830 << 16, 4)]);
    }
    for (u32 i = 4; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    page_header(state.header, input, frame, titles, values, 2, slots);

    const bool held = input.held_notes > 0 && input.held_notes <= 128;
    if (input.note_serial != state.serial) { state.serial = input.note_serial; state.env_phase = 0; state.env_cycle = 0; }

    // Envelope shape over two cycles.
    i32 heights[32];
    for (u32 i = 0; i < 32; ++i) {
        const u32 level = shape ? env_level(shape, i >> 4, i & 15) : 15;
        heights[i] = 24 - static_cast<i32>(level) * 3;
    }
    ribbon(frame.paths[0], heights);
    frame.paths[0].key = 1 + shape_index;

    // The mixer's wave: tone, tone AND noise, or noise, from a fixed register run.
    u32 lfsr = 0x1abcd;
    for (u32 i = 0; i < 32; ++i) {
        for (u32 s = 0; s < noise_period / 8 + 1; ++s) lfsr = (lfsr >> 1) | (((lfsr ^ (lfsr >> 3)) & 1u) << 16);
        const bool tone = mixer == 2 || ((i >> 3) & 1) == 0;
        const bool noise = mixer == 0 || (lfsr & 1u);
        heights[i] = tone && noise ? -20 : 18;
    }
    ribbon(frame.paths[1], heights);
    frame.paths[1].key = 1 + mixer * 32 + noise_period;

    place(frame, 0, 120 * unit, 110 * unit, unit, held ? lit : idle, stroke);
    place(frame, 1, 360 * unit, 110 * unit, unit, held ? lit : idle, stroke);

    // Volume cells: the envelope's level while a note is held (free rates only).
    u32 level = 0;
    if (held && !input.reduced_motion) {
        if (!shape) level = 15;
        else if (rate >= 22938) level = 15;
        else {
            level = env_level(shape, state.env_cycle, state.env_phase >> 28);
            const u32 step = tenths * 419430u;  // 2^32 / 10 / 1024: phase per Q10 second
            const u32 next = state.env_phase + step * static_cast<u32>(clamp(input.elapsed, 0, 102));
            if (next < state.env_phase) state.env_cycle = state.env_cycle == 1 ? 2 : 1;
            state.env_phase = next;
        }
    } else if (held) level = 15;
    for (u32 i = 0; i < 16; ++i)
        place(frame, first_cell + i, 0, 0, unit, i < level ? (i == 15 ? accent : lit) : dim, fill_and_stroke);

    const bool lamps[3] = {mixer != 2, (input.slots[4] >> 16) > 0 && mixer != 2, mixer != 0};
    for (u32 i = 0; i < 3; ++i)
        place(frame, first_lamp + i, 0, 0, unit, lamps[i] ? (held ? accent : lit) : dim, fill_and_stroke);
}
}
