// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
#pragma once
#include <opxy/audio.hpp>

namespace silpaula {
// One OP-XY note uses three of Paula's four channels: the carrier, its
// attached modulator and a detuned copy. The fourth is the screen's filter LED.
struct Voice {
    opxy::u32 carrier, modulator, copy;  // 32-bit phases through the tables
    opxy::u32 tick;                      // 50 Hz software ticks in Q16
    opxy::i32 volume;                    // 0..64 in Q8, slid down by the envelope
    opxy::i32 rc, low, band;             // A500 one-pole, LED two-pole states
};
struct Shared {};
}
