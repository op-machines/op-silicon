// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// The screen: the pulse's duty sequence and the triangle's 32 steps as
// outlines, sixteen volume cells that follow the decay envelope while a note
// is held, and lamps for pulse, triangle and noise. Grey and still at rest.
#include "display.h"

namespace silapu {
using opxy::i32;
using opxy::u32;

const u32 first_cell = 10, first_lamp = 26;
const u32 dim = 3, idle = 6, lit = 12, accent = 14;  // palette entries (a proposal)

inline u32 pick(i32 parameter, u32 count) { return static_cast<u32>((parameter >> 16) * static_cast<i32>(count) >> 15); }

inline void number(char* out, u32 value, const char* prefix, const char* suffix) {
    char digits[8];
    u32 n = 0, k = 0;
    while (*prefix && k < 3) out[k++] = *prefix++;
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
    const char* const duties[4] = {"12.5%", "25%", "50%", "75%"};
    const char* const noises[3] = {"off", "long", "short"};
    const u32 duty = pick(input.slots[0], 4);
    const i32 sweep = (input.slots[1] >> 16) - 16384;
    const u32 noise = pick(input.slots[2], 3);
    const u32 env = pick(input.slots[3], 17);
    const i32 mix = input.slots[4] >> 16;

    const char* const titles[8] = {"Duty", "Sweep", "Noise", "Env", "Mix", "Vibrato", "Lowpass", "Gain"};
    char values[8][header_capacity];
    copy(values[0], header_capacity, duties[duty]);
    const i32 amount = sweep < 0 ? -sweep : sweep;
    if (amount < 1024) copy(values[1], header_capacity, "off");
    else number(values[1], static_cast<u32>(amount - 1024) * 15u / 15360u + 1, sweep > 0 ? "up " : "dn ", "");
    copy(values[2], header_capacity, noises[noise]);
    if (!env) copy(values[3], header_capacity, "const");
    else number(values[3], env * 125u / 2u, "", "ms");  // 15 steps of env / 240 s
    for (u32 i = 4; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    page_header(state.header, input, frame, titles, values, 2, slots);

    const bool held = input.held_notes > 0 && input.held_notes <= 128;
    if (input.note_serial != state.serial) { state.serial = input.note_serial; state.time = 0; }
    else if (held && state.time < 60u * 1024u) state.time += static_cast<u32>(clamp(input.elapsed, 0, 102));

    // Duty sequence, two periods of eight steps.
    i32 heights[32];
    for (u32 i = 0; i < 32; ++i) {
        const u32 step = (i >> 1) & 7;
        const u32 end = duty == 0 ? 2 : duty == 2 ? 5 : 3;
        bool high = step >= 1 && step < end;
        if (duty == 3) high = !high;
        heights[i] = high ? -20 : 18;
    }
    ribbon(frame.paths[0], heights);
    frame.paths[0].key = 1 + duty;
    // Triangle: 32 steps, 15 down to 0 and up again.
    for (u32 i = 0; i < 32; ++i) {
        const i32 level = i < 16 ? 15 - static_cast<i32>(i) : static_cast<i32>(i) - 16;
        heights[i] = 20 - level * 40 / 15;
    }
    ribbon(frame.paths[1], heights);
    frame.paths[1].key = 1;

    const bool pulse_on = mix < 32700, triangle_on = mix > 6554;
    place(frame, 0, 120 * unit, 110 * unit, unit, pulse_on ? (held ? lit : idle) : dim, stroke);
    place(frame, 1, 360 * unit, 110 * unit, unit, triangle_on ? (held ? lit : idle) : dim, stroke);

    u32 level = 0;
    if (held) {
        level = 15;
        if (env && !input.reduced_motion) {
            const u32 ticks = state.time * 240u >> 10;
            const u32 fallen = ticks / env;
            level = fallen >= 15 ? 0 : 15 - fallen;
        }
    }
    for (u32 i = 0; i < 16; ++i)
        place(frame, first_cell + i, 0, 0, unit, i < level ? (i == 15 ? accent : lit) : dim, fill_and_stroke);

    const bool lamps[3] = {pulse_on, triangle_on, noise != 0};
    for (u32 i = 0; i < 3; ++i)
        place(frame, first_lamp + i, 0, 0, unit, lamps[i] ? (held ? accent : lit) : dim, fill_and_stroke);
}
}
