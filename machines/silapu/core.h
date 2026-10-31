// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
#pragma once
#include <opxy/audio.hpp>

namespace silapu {
// One OP-XY note plays one APU: a pulse channel, the triangle and the noise
// channel, with the frame counter's envelope and sweep.
struct Voice {
    opxy::u32 phase_pulse, phase_triangle;
    opxy::u32 noise, noise_clock;   // 15-bit shift register, Q16 shift clock
    opxy::u32 frame_clock, half;    // 240 Hz ticks in Q16; every second tick is a 120 Hz tick
    opxy::u32 env_divider, env_level;
    opxy::u32 sweep_divider, period, muted;  // period 0: follow the note
    opxy::u32 lfo;
    opxy::i32 dc, filtered;  // DC estimate in Q8, lowpass state
};
struct Shared {};
}
