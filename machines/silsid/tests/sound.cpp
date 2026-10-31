// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
// Sound checks, compiled for the host and run by python sdk.py test --machine <folder>.
#include "../engine.cpp"
#include <cstdio>

int main() {
    static silsid::Voice voice;
    static silsid::Shared shared;
    opxy::i32 parameters[8] = {644245094, 644245094, 1288490188, 858993459, 429496729, 0, 644245094, 0};
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 42852281u;  // 440 Hz at 44.1 kHz
    opxy::i32 output[opxy::block_frames];
    silsid::initialize(voice, shared, 1);
    bool bounded = true;
    unsigned sounding = 0;
    for (unsigned block = 0; block < 2760; ++block) {
        // Every control at 0, at full and in between, each wave, mode, mod and arp; restarts.
        if (block % 690 == 0) silsid::start(voice, shared, 1);
        for (unsigned j = 0; j < 8; ++j)
            parameters[j] = static_cast<opxy::i32>(((block / 23 + j * 5) % 9) * 0x0fffffffu);
        silsid::render(voice, shared, 1, output, parameters, pitch);
        bool any = false;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            any = any || output[i] != 0;
            bounded = bounded && output[i] >= -1073741824 && output[i] <= 1073741824;
        }
        sounding += any;
    }
    if (sounding < 1000) std::printf("FAIL mostly silent (%u of 2760 blocks)\n", sounding);
    if (!bounded) std::puts("FAIL above the declared peak");
    return sounding >= 1000 && bounded ? 0 : 1;
}
