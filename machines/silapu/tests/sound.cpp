// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
//
// silapu's sound checks, run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>

using opxy::i32;
using opxy::u32;
static silapu::Voice voice;
static silapu::Shared shared;
static i32 output[opxy::block_frames];
static u32 pitch[opxy::block_frames];
static int failures = 0;
static bool bounded = true;

static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }

static i32 run(const i32 (&parameters)[8], unsigned blocks) {
    i32 peak = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        silapu::render(voice, shared, 1, output, parameters, pitch);
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
    silapu::initialize(voice, shared, 1);
    silapu::start(voice, shared, 1);
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
            // Sounding unless a fast decay or a downward sweep has ended the note.
            if (!((corner >> 3) & 1) && !((corner >> 1) & 1) && n < 2) check(peak > 0, "silent corner");
        }
    check(bounded, "above the declared peak");

    // Pulse timer: 440 Hz is t = 253, so (t + 1) = 254.
    check(silapu::timer(42852281u, silapu::timer_constant) == 254, "pulse timer");

    // A fast decay (period 0: 15 steps at 240 Hz, 62.5 ms) ends in silence.
    const i32 decay[8] = {0x50000000, 0x40000000, 0, 0x7fffffff / 17 + 0x04000000, 0, 0, 0, 0x40000000};
    note(42852281u);
    run(decay, 100);
    check(run(decay, 50) <= 65536, "decay does not end in silence");

    // An upward sweep raises the pitch: the timer value falls.
    const i32 up[8] = {0x50000000, 0x7fffffff, 0, 0, 0, 0, 0, 0x40000000};
    note(42852281u);
    run(up, 100);
    check(voice.period < 254, "sweep up does not raise the pitch");
    // A downward sweep mutes once the target passes the 11-bit timer.
    const i32 down[8] = {0x50000000, 0, 0, 0, 0, 0, 0, 0x40000000};
    note(1948294u * 4);
    run(down, 2000);
    check(voice.muted == 1, "sweep down does not mute");

    // Short noise repeats after 93 steps; long noise does not within that.
    u32 reg = 1;
    for (u32 i = 0; i < 93; ++i) reg = (reg >> 1) | (((reg ^ (reg >> 6)) & 1u) << 14);
    u32 other = 1;
    for (u32 i = 0; i < 93; ++i) other = (other >> 1) | (((other ^ (other >> 1)) & 1u) << 14);
    check(reg == 1 && other != 1, "noise register periods");
    return failures ? 1 : 0;
}
