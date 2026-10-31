// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
// Renders a short piece with silsid's engine (host build) to song.wav.
#include "engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

struct Sound { double p[8]; };
// wave, width, cutoff, reso, filter, mod, env, arp (0..1)
const Sound bass = {{0.3125, 0.35, 0.30, 0.60, 0.25, 0.00, 0.45, 0.00}};
const Sound arpm = {{0.3125, 0.62, 0.52, 0.45, 0.25, 0.00, 0.55, 0.583}};
const Sound arpM = {{0.3125, 0.62, 0.52, 0.45, 0.25, 0.00, 0.55, 0.417}};
const Sound lead = {{0.1875, 0.30, 0.62, 0.70, 0.25, 0.22, 0.25, 0.00}};

struct Note { int start, len, midi; const Sound* s; double vel, pan; };
const int sr = 44100, six = sr * 60 / 112 / 4;  // a sixteenth at 112 BPM

struct Live {
    silsid::Voice v; silsid::Shared sh; Note n; double env = 0; bool on = false;
};

int main() {
    std::vector<Note> notes;
    // Am F C G, twice; then Dm F G Am, Am F C G, last bar Am held.
    const int roots[17] = {45, 41, 48, 43, 45, 41, 48, 43, 50, 41, 43, 45, 45, 41, 48, 43, 45};
    const bool minor[17] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1};
    for (int bar = 0; bar < 17; ++bar) {
        const int b0 = bar * 16, r = roots[bar];
        if (bar == 16) { notes.push_back({b0 * six, 16 * six, r - 12, &bass, 1, 0}); notes.push_back({b0 * six, 16 * six, r + 12, &arpm, 0.8, -0.4}); break; }
        // Bass: root eighths with an octave jump on the off-beat sixteenth.
        for (int i = 0; i < 16; i += 2)
            notes.push_back({(b0 + i) * six, six * 3 / 2, r - 12 + ((i % 8 == 6) ? 12 : 0), &bass, i % 4 ? 0.8 : 1, 0});
        // Arp: one held chord a half bar from bar 2 on (the engine arps it).
        if (bar >= 2)
            for (int h = 0; h < 2; ++h)
                notes.push_back({(b0 + h * 8) * six, 7 * six, r + 12, minor[bar] ? &arpm : &arpM, 0.55, -0.45});
    }
    // Lead from bar 4: a phrase over each four bars (MIDI, sixteenth start, length).
    const int phrase[][3] = {{69, 0, 6}, {72, 6, 2}, {76, 8, 8}, {77, 16, 6}, {76, 22, 2}, {72, 24, 8},
                             {72, 32, 4}, {74, 36, 4}, {76, 40, 8}, {74, 48, 6}, {71, 54, 2}, {67, 56, 8}};
    const int phrase2[][3] = {{74, 0, 6}, {77, 6, 2}, {81, 8, 8}, {81, 16, 4}, {79, 20, 4}, {77, 24, 8},
                              {79, 32, 6}, {77, 38, 2}, {74, 40, 8}, {76, 48, 12}, {72, 60, 4}};
    for (auto& p : phrase) notes.push_back({(64 + p[1]) * six, p[2] * six - six / 3, p[0], &lead, 0.7, 0.35});
    for (auto& p : phrase2) notes.push_back({(128 + p[1]) * six, p[2] * six - six / 3, p[0], &lead, 0.75, 0.35});
    for (auto& p : phrase) notes.push_back({(192 + p[1]) * six, p[2] * six - six / 3, p[0] + 12 * (p[1] >= 48), &lead, 0.8, 0.35});

    const int total = 17 * 16 * six + sr * 2;
    std::vector<float> L(total), R(total);
    static Live live[24];   // six voices per sound at most, as the device keeps
    std::vector<bool> done(notes.size());
    const double att = 1.0 / (0.003 * sr), rel = 1.0 / (0.18 * sr);
    opxy::i32 out[opxy::block_frames];
    opxy::u32 pitch[opxy::block_frames];
    opxy::i32 par[8];
    for (int t = 0; t < total; t += opxy::block_frames) {
        for (size_t i = 0; i < notes.size(); ++i) {
            if (done[i] || notes[i].start >= t + (int)opxy::block_frames) continue;
            done[i] = true;
            Live* pick = nullptr;
            for (auto& l : live) if (!l.on && l.env <= 0) { pick = &l; break; }
            if (!pick) for (auto& l : live) if (!l.on) { pick = &l; break; }
            if (!pick) continue;
            silsid::initialize(pick->v, pick->sh, 1);
            silsid::start(pick->v, pick->sh, 1);
            pick->n = notes[i]; pick->on = true; pick->env = 0;
        }
        for (auto& l : live) {
            if (!l.on && l.env <= 0) continue;
            for (int j = 0; j < 8; ++j) par[j] = (opxy::i32)(l.n.s->p[j] * 2147483647.0);
            const double hz = 440.0 * std::pow(2.0, (l.n.midi - 69) / 12.0);
            for (auto& p : pitch) p = (opxy::u32)(hz / sr * 4294967296.0);
            silsid::render(l.v, l.sh, 1, out, par, pitch);
            const double gl = l.n.vel * std::sqrt(0.5 * (1 - l.n.pan)), gr = l.n.vel * std::sqrt(0.5 * (1 + l.n.pan));
            for (unsigned k = 0; k < opxy::block_frames && t + (int)k < total; ++k) {
                if (l.on && t + (int)k >= l.n.start + l.n.len) l.on = false;
                l.env = l.on ? std::fmin(1.0, l.env + att) : std::fmax(0.0, l.env - rel);
                const double x = out[k] / 1073741824.0 * l.env * 0.32;
                L[t + k] += x * gl; R[t + k] += x * gr;
            }
        }
    }
    // A dotted-eighth echo on the right-leaning parts, then a soft limit.
    const int d = six * 3;
    for (int i = d; i < total; ++i) { L[i] += 0.28 * R[i - d]; R[i] += 0.22 * L[i - d]; }
    double peak = 0;
    for (int i = 0; i < total; ++i) peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
    const double g = 0.89 / peak;
    FILE* f = std::fopen("song.wav", "wb");
    auto u32 = [&](unsigned v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](unsigned short v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + total * 4); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16); std::fwrite("data", 1, 4, f); u32(total * 4);
    for (int i = 0; i < total; ++i) {
        short a = (short)std::lrint(std::tanh(L[i] * g) * 32767), b = (short)std::lrint(std::tanh(R[i] * g) * 32767);
        std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
    }
    std::fclose(f);
    std::printf("peak %.3f, %d s\n", peak, total / sr);
}
