// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// silay's sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>

using opxy::i32;
using opxy::u32;
static silay::Voice voice;
static silay::Shared shared;
static i32 output[opxy::block_frames];
static u32 pitch[opxy::block_frames];
static int failures = 0;

static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }

// Renders blocks and returns the peak absolute output; sets bounded false above the declared peak.
static i32 run(i32 (&parameters)[8], unsigned blocks, bool& bounded) {
    i32 peak = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        silay::render(voice, shared, 1, output, parameters, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const i32 a = output[i] < 0 ? -output[i] : output[i];
            peak = a > peak ? a : peak;
            bounded = bounded && output[i] >= -1073741824 && output[i] <= 1073741824;
        }
    }
    return peak;
}

int main() {
    const u32 notes[3] = {1948294u, 42852281u, 685636496u};  // 20 Hz, 440 Hz, 7 kHz
    bool bounded = true;
    // Every control corner at three pitches, with restarts: bounded and sounding.
    for (u32 n = 0; n < 3; ++n) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = notes[n];
        for (u32 corner = 0; corner < 256; ++corner) {
            i32 parameters[8];
            for (u32 j = 0; j < 8; ++j) parameters[j] = (corner >> j) & 1 ? 0x7fffffff : 0;
            parameters[7] = parameters[7] ? 0x7fffffff : 0x40000000;
            silay::initialize(voice, shared, 1);
            silay::start(voice, shared, 1);
            const i32 peak = run(parameters, 20, bounded);
            const bool envelope_may_end = (corner >> 2) & 1;  // shape "decay" at the top corner is "att 0"
            if (!envelope_may_end) check(peak > 0, "silent corner");
        }
    }
    check(bounded, "above the declared peak");

    // Tone period: 440 Hz at 2 MHz is period 284, so the chip plays 440.14 Hz.
    check(silay::quantize(42852281u) >> 8 == silay::tone_constant / 284, "tone period quantization");

    // Shape "decay" (9) falls to silence after one cycle and stays there.
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 42852281u;
    i32 decay[8] = {0, 0x40000000, static_cast<i32>(0x7fffffff / 9 * 1 + 0x07000000), 0x7fffffff / 2, 0, 0, 0, 0x40000000};
    silay::initialize(voice, shared, 1);
    silay::start(voice, shared, 1);
    run(decay, 400, bounded);
    check(run(decay, 50, bounded) <= 65536, "decay shape does not end in silence");  // filter rounding: one Q15 step

    // Envelope logic against the datasheet's shape table.
    check(silay::envelope(8, 1, 0) == 15 && silay::envelope(10, 1, 0) == 0, "sawtooth and triangle repeat");
    check(silay::envelope(11, 3, 5) == 15 && silay::envelope(13, 3, 5) == 15 && silay::envelope(15, 3, 5) == 0, "hold shapes");

    // Noise alone is not periodic at the tone's period.
    i32 noisy[8] = {0x7fffffff, 0x7fffffff, 0, 0, 0, 0, 0, 0x40000000};
    silay::initialize(voice, shared, 1);
    silay::start(voice, shared, 1);
    run(noisy, 4, bounded);
    u32 changes = 0;
    for (unsigned i = 1; i < opxy::block_frames; ++i) changes += output[i] != output[i - 1];
    check(changes > 4, "noise does not move");
    return failures ? 1 : 0;
}
