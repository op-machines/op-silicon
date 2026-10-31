// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// silpaula's sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>

using opxy::i32;
using opxy::u32;
static silpaula::Voice voice;
static silpaula::Shared shared;
static i32 output[opxy::block_frames];
static u32 pitch[opxy::block_frames];
static int failures = 0;
static bool bounded = true;

static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }

static i32 run(const i32 (&parameters)[8], unsigned blocks) {
    i32 peak = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        silpaula::render(voice, shared, 1, output, parameters, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const i32 a = output[i] < 0 ? -output[i] : output[i];
            peak = a > peak ? a : peak;
            bounded = bounded && output[i] >= -1073741824 && output[i] <= 1073741824;
        }
    }
    return peak;
}

static void note(u32 increment) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = increment;
    silpaula::initialize(voice, shared, 1);
    silpaula::start(voice, shared, 1);
}

int main() {
    const u32 notes[3] = {1948294u, 42852281u, 685636496u};  // 20 Hz, 440 Hz, 7 kHz
    for (u32 n = 0; n < 3; ++n)
        for (u32 corner = 0; corner < 256; ++corner) {
            i32 parameters[8];
            for (u32 j = 0; j < 8; ++j) parameters[j] = (corner >> j) & 1 ? 0x7fffffff : 0;
            if (!parameters[7]) parameters[7] = 0x40000000;
            note(notes[n]);
            const i32 peak = run(parameters, 20);
            if (!((corner >> 4) & 1)) check(peak > 0, "silent corner");
        }
    check(bounded, "above the declared peak");

    // Every wave stays in 8 bits and is not silent.
    for (u32 w = 0; w < 8; ++w) {
        i32 low = 0, high = 0;
        for (u32 i = 0; i < 128; ++i) {
            low = silpaula::waves[w][i] < low ? silpaula::waves[w][i] : low;
            high = silpaula::waves[w][i] > high ? silpaula::waves[w][i] : high;
        }
        check(high - low > 100, "flat wave");
    }

    // Nearest neighbour: with length 32 and filter off, the output holds steps.
    const i32 raw[8] = {0x10000000, 0, 0, 0, 0, 0, 0, 0x40000000};
    note(1948294u * 4);  // 80 Hz: 2560 samples per second, about 17 output samples each
    run(raw, 2);
    u32 repeats = 0;
    for (unsigned i = 1; i < opxy::block_frames; ++i) repeats += output[i] == output[i - 1];
    check(repeats > 48, "samples are interpolated");

    // A fast slide ends in silence within 64 / 16 ticks at 50 Hz (80 ms).
    const i32 slide[8] = {0x10000000, 0x40000000, 0, 0, 0x7fffffff, 0, 0, 0x40000000};
    note(42852281u);
    run(slide, 80);
    check(run(slide, 10) == 0, "volume slide does not end in silence");

    // Attached volume at full depth with the slow modulator dips the level.
    const i32 tremolo[8] = {0x10000000, 0x40000000, 0x5c000000, 0x7fffffff, 0, 0, 0, 0x40000000};
    note(42852281u);
    i32 lowest = 0x7fffffff, highest = 0;
    for (u32 b = 0; b < 200; ++b) {
        const i32 peak = run(tremolo, 1);
        lowest = peak < lowest ? peak : lowest;
        highest = peak > highest ? peak : highest;
    }
    check(lowest < highest / 4, "attached volume does not modulate");
    return failures ? 1 : 0;
}
