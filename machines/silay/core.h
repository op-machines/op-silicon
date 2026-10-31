// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
#pragma once
#include <opxy/audio.hpp>

namespace silay {
// One OP-XY note plays one AY: tone A at the note, tone B detuned, the shared
// noise generator and the hardware envelope.
struct Voice {
    opxy::u32 phase_a, phase_b;   // tone counters as 32-bit phases
    opxy::u32 noise, noise_clock; // 17-bit shift register, Q16 shift clock
    opxy::u32 env_phase, env_cycle; // 16 steps per cycle in the top 4 bits; cycle 0, then 1/2 alternating
    opxy::u32 lfo;                // vibrato phase
    opxy::i32 filtered;           // one-pole lowpass state, Q15
};
struct Shared {};
}
