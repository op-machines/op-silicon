// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// The screen: the wave as Paula reads it and the attached modulator as
// outlines, sixteen volume cells that follow the volume slide while a note is
// held, and four channel lamps: carrier, modulator, detuned copy and the
// filter LED. Grey and still at rest.
#include "display.h"
#include "waves.h"

namespace silpaula {
using opxy::i32;
using opxy::u32;

const u32 first_cell = 10, first_lamp = 26;
const u32 dim = 3, idle = 6, lit = 12, accent = 14;  // palette entries (a proposal)

inline u32 pick(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

inline void number(char* out, u32 value, const char* suffix) {
    char digits[8];
    u32 n = 0, k = 0;
    do { digits[n++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && n < 5);
    while (n) out[k++] = digits[--n];
    while (*suffix && k < opxy::draw::header_capacity - 1) out[k++] = *suffix++;
    out[k] = 0;
}

template <typename P>
inline void ribbon(P& path, const i32 (&y)[32]) {
    using opxy::draw::unit;
    for (u32 i = 0; i < 32; ++i) {
        const i32 x = (-62 + static_cast<i32>(i) * 4) * unit;
        path.points[i] = {x, y[i] * unit};
        path.points[63 - i] = {x, (y[i] + 2) * unit};
    }
}

inline void display_reset(DisplayState& state) { state.header.reset(); state.serial = 0; state.time = 0; }

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    const char* const names[8] = {"sine", "tri", "saw", "square", "pulse", "organ", "vox", "grit"};
    const char* const lengths[3] = {"32", "64", "128"};
    const char* const modes[5] = {"off", "am", "fm", "am lfo", "fm lfo"};
    const char* const filters[3] = {"off", "a500", "led"};
    const u32 wave = pick(input.slots[0], 8), length = pick(input.slots[1], 3), mode = pick(input.slots[2], 5);
    const u32 slide = pick(input.slots[4], 17), filter = pick(input.slots[5], 3);

    const char* const titles[8] = {"Wave", "Length", "Mod", "Depth", "Env", "Filter", "Detune", "Gain"};
    char values[8][header_capacity];
    copy(values[0], header_capacity, names[wave]);
    copy(values[1], header_capacity, lengths[length]);
    copy(values[2], header_capacity, modes[mode]);
    header_amount(values[3], header_capacity, input.slots[3]);
    if (!slide) copy(values[4], header_capacity, "hold");
    else number(values[4], 1280u / slide, "ms");  // 64 steps at 50 Hz
    copy(values[5], header_capacity, filters[filter]);
    header_amount(values[6], header_capacity, input.slots[6]);
    header_amount(values[7], header_capacity, input.slots[7]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    page_header(state.header, input, frame, titles, values, 2, slots);

    const bool held = input.held_notes > 0 && input.held_notes <= 128;
    if (input.note_serial != state.serial) { state.serial = input.note_serial; state.time = 0; }
    else if (held && state.time < 60u * 1024u) state.time += static_cast<u32>(clamp(input.elapsed, 0, 102));

    // The wave at its length: 32 points, each held for 32 / length of a sample.
    i32 heights[32];
    const u32 samples = 32u << length;
    for (u32 i = 0; i < 32; ++i) {
        const u32 index = (i * samples / 32) * (128 / samples);
        heights[i] = -waves[wave][index] * 24 / 127;
    }
    ribbon(frame.paths[0], heights);
    frame.paths[0].key = 1 + wave * 3 + length;
    for (u32 i = 0; i < 32; ++i) heights[i] = -modulator_wave[i] * 20 / 127;
    ribbon(frame.paths[1], heights);
    frame.paths[1].key = 1;

    place(frame, 0, 120 * unit, 110 * unit, unit, held ? lit : idle, stroke);
    place(frame, 1, 360 * unit, 110 * unit, unit, mode ? (held ? lit : idle) : dim, stroke);

    u32 level = 0;
    if (held) {
        level = 16;
        if (slide && !input.reduced_motion) {
            const u32 fallen = (state.time * 50u >> 10) * slide;
            level = fallen >= 64 ? 0 : (64 - fallen) >> 2;
        }
    }
    for (u32 i = 0; i < 16; ++i)
        place(frame, first_cell + i, 0, 0, unit, i < level ? (i == 15 ? accent : lit) : dim, fill_and_stroke);

    const bool lamps[4] = {true, mode != 0, (input.slots[6] >> 16) > 0, filter == 2};
    for (u32 i = 0; i < 4; ++i)
        place(frame, first_lamp + i, 0, 0, unit, lamps[i] ? (held || i == 3 ? accent : lit) : dim, fill_and_stroke);
}
}
