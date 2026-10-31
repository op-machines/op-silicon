// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
#include "display.h"

// The 6581 in plan view, lid off (from OP-PLAYGROUND's "6581"): the resting pose
// below, and while notes are held the parts that work light up: AUDIO OUT (pin 27),
// the scopes, one voice cell per note, and CAP1/CAP2 (pins 1-4) when the filter is on.

namespace silsid {
// Mutable outlines: 4 × 64 points, Q10, relative to their node.
static const opxy::i32 rest_paths[4][64][2] = {
    {{-65536, -9745}, {-61308, 13331}, {-57080, 14094}, {-52852, 14089}, {-48623, 14087}, {-44395, 14087}, {-40167, 14087}, {-35939, 14087}, {-31711, 14087}, {-27483, 14087}, {-23255, 14087}, {-19027, 14087}, {-14798, -18890}, {-10570, -14158}, {-6342, -14081}, {-2114, -14086}, {2114, -14087}, {6342, -14087}, {10570, -14087}, {14798, -14087}, {19027, -14087}, {23255, -14087}, {27483, -14087}, {31711, -14087}, {35939, -14087}, {40167, -14087}, {44395, -14087}, {48623, -14087}, {52852, -14087}, {57080, -14087}, {61308, -14087}, {65536, -14087}, {65536, -13472}, {61308, -13472}, {57080, -13472}, {52852, -13472}, {48623, -13472}, {44395, -13472}, {40167, -13472}, {35939, -13472}, {31711, -13472}, {27483, -13472}, {23255, -13472}, {19027, -13472}, {14798, -13472}, {10570, -13472}, {6342, -13472}, {2114, -13472}, {-2114, -13472}, {-6342, -13466}, {-10570, -13543}, {-14798, -18275}, {-19027, 14701}, {-23255, 14701}, {-27483, 14701}, {-31711, 14701}, {-35939, 14701}, {-40167, 14701}, {-44395, 14701}, {-48623, 14701}, {-52852, 14703}, {-57080, 14708}, {-61308, 13946}, {-65536, -9130}},
    {{-65536, 0}, {-61308, 0}, {-57080, 0}, {-52852, 0}, {-48623, 0}, {-44395, -1}, {-40167, -1}, {-35939, -1}, {-31711, -2}, {-27483, -3}, {-23255, -5}, {-19027, -8}, {-14798, -12}, {-10570, -19}, {-6342, -29}, {-2114, -46}, {2114, -72}, {6342, -112}, {10570, -175}, {14798, -272}, {19027, -423}, {23255, -656}, {27483, -1011}, {31711, -1542}, {35939, -2297}, {40167, -3227}, {44395, -3838}, {48623, -2479}, {52852, 2744}, {57080, 10810}, {61308, 19651}, {65536, 28417}, {65536, 29032}, {61308, 20266}, {57080, 11424}, {52852, 3358}, {48623, -1865}, {44395, -3223}, {40167, -2612}, {35939, -1683}, {31711, -928}, {27483, -397}, {23255, -41}, {19027, 191}, {14798, 342}, {10570, 440}, {6342, 502}, {2114, 543}, {-2114, 568}, {-6342, 585}, {-10570, 596}, {-14798, 602}, {-19027, 607}, {-23255, 609}, {-27483, 611}, {-31711, 612}, {-35939, 613}, {-40167, 614}, {-44395, 614}, {-48623, 614}, {-52852, 614}, {-57080, 614}, {-61308, 614}, {-65536, 614}},
    {{-32768, -20480}, {-30654, -10693}, {-28540, -9675}, {-26426, -9570}, {-24312, -9559}, {-22198, -9557}, {-20084, -9557}, {-17970, -9557}, {-15855, -9557}, {-13741, -9557}, {-11627, -9557}, {-9513, -9557}, {-7399, -9557}, {-5285, -9557}, {-3171, -9557}, {-1057, -9557}, {1057, -9557}, {3171, -9557}, {5285, -9557}, {7399, -9557}, {9513, 13182}, {11627, 19721}, {13741, 20401}, {15855, 20472}, {17970, 20479}, {20084, 20480}, {22198, 20480}, {24312, 20480}, {26426, 20480}, {28540, 20480}, {30654, 20480}, {32768, 20480}, {32768, 21094}, {30654, 21094}, {28540, 21094}, {26426, 21094}, {24312, 21094}, {22198, 21094}, {20084, 21094}, {17970, 21094}, {15855, 21086}, {13741, 21016}, {11627, 20336}, {9513, 13796}, {7399, -8943}, {5285, -8943}, {3171, -8943}, {1057, -8943}, {-1057, -8943}, {-3171, -8943}, {-5285, -8943}, {-7399, -8943}, {-9513, -8943}, {-11627, -8943}, {-13741, -8943}, {-15855, -8943}, {-17970, -8943}, {-20084, -8943}, {-22198, -8943}, {-24312, -8944}, {-26426, -8955}, {-28540, -9061}, {-30654, -10078}, {-32768, -19866}},
    {{-32768, 0}, {-30654, 0}, {-28540, 0}, {-26426, 0}, {-24312, 0}, {-22198, 0}, {-20084, 0}, {-17970, 0}, {-15855, 0}, {-13741, 0}, {-11627, 0}, {-9513, 0}, {-7399, 0}, {-5285, 0}, {-3171, 0}, {-1057, 0}, {1057, 0}, {3171, 0}, {5285, 0}, {7399, 0}, {9513, 0}, {11627, 0}, {13741, 0}, {15855, 0}, {17970, 0}, {20084, 0}, {22198, 0}, {24312, 0}, {26426, 0}, {28540, 0}, {30654, 0}, {32768, 0}, {32768, 614}, {30654, 614}, {28540, 614}, {26426, 614}, {24312, 614}, {22198, 614}, {20084, 614}, {17970, 614}, {15855, 614}, {13741, 614}, {11627, 614}, {9513, 614}, {7399, 614}, {5285, 614}, {3171, 614}, {1057, 614}, {-1057, 614}, {-3171, 614}, {-5285, 614}, {-7399, 614}, {-9513, 614}, {-11627, 614}, {-13741, 614}, {-15855, 614}, {-17970, 614}, {-20084, 614}, {-22198, 614}, {-24312, 614}, {-26426, 614}, {-28540, 614}, {-30654, 614}, {-32768, 614}},
};

// Control 0..1 in Q31 to an index 0..n-1, as the engine picks it.
inline opxy::u32 pick(opxy::i32 p, opxy::u32 n) {
    const opxy::u32 i = (static_cast<opxy::u32>(p) >> 16) * n >> 15;
    return i < n ? i : n - 1;
}
inline void name(char* value, const char* text) {
    opxy::u32 i = 0;
    for (; text[i] && i + 1 < opxy::draw::header_capacity; ++i) value[i] = text[i];
    value[i] = 0;
}

inline void display_reset(DisplayState& state) { state.phase = 0; state.header.reset(); }

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    (void)state;
    const char* const titles[8] = {"wave", "width", "cutoff", "reso", "filter", "mod", "env", "arp"};
    char values[8][header_capacity] = {};
    for (opxy::u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    static const char* const waves[8] = {"tri", "saw", "pulse", "noise", "s+t", "p+t", "p+s", "p+s+t"};
    static const char* const modes[6] = {"off", "lp", "bp", "hp", "notch", "lp+bp"};
    static const char* const arps[6] = {"off", "oct", "maj", "min", "sus", "7th"};
    name(values[0], waves[pick(input.slots[0], 8)]);
    name(values[4], modes[pick(input.slots[4], 6)]);
    if ((static_cast<opxy::u32>(input.slots[5]) >> 16) < 1311) name(values[5], "off");
    name(values[7], arps[pick(input.slots[7], 6)]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    page_header(state.header, input, frame, titles, values, 122, slots);
    for (opxy::u32 p = 0; p < Frame::path_count; ++p) {
        frame.paths[p].key = 1;
        for (opxy::u32 i = 0; i < Frame::point_count; ++i) {
            frame.paths[p].points[i].x = rest_paths[p][i][0];
            frame.paths[p].points[i].y = rest_paths[p][i][1];
        }
    }
    place(frame, 0, 163840, 115712, 1382, 1, stroke);
    place(frame, 1, 385024, 120832, 563, 1, stroke);
    place(frame, 2, 303104, 115712, 1126, 1, stroke);
    place(frame, 3, 303104, 168960, 1075, 2, stroke);
    place(frame, 4, 20480, 57344, 1024, 4, stroke);
    place(frame, 5, 20480, 133120, 1024, 4, fill_and_stroke);
    place(frame, 6, 32768, 196608, 1024, 2, stroke);
    place(frame, 7, 51200, 57344, 1024, 1, fill_and_stroke);
    place(frame, 8, 81101, 57344, 1024, 1, fill_and_stroke);
    place(frame, 9, 111002, 57344, 1024, 1, fill_and_stroke);
    place(frame, 10, 140902, 57344, 1024, 1, fill_and_stroke);
    place(frame, 11, 170803, 57344, 1024, 1, fill_and_stroke);
    place(frame, 12, 200704, 57344, 1024, 1, fill_and_stroke);
    place(frame, 13, 230605, 57344, 1024, 1, fill_and_stroke);
    place(frame, 14, 260506, 57344, 1024, 1, fill_and_stroke);
    place(frame, 15, 290406, 57344, 1024, 1, fill_and_stroke);
    place(frame, 16, 320307, 57344, 1024, 1, fill_and_stroke);
    place(frame, 17, 350208, 57344, 1024, 1, fill_and_stroke);
    place(frame, 18, 380109, 57344, 1024, 1, fill_and_stroke);
    place(frame, 19, 410010, 57344, 1024, 1, fill_and_stroke);
    place(frame, 20, 439910, 57344, 1024, 1, fill_and_stroke);
    place(frame, 21, 51200, 208896, 1024, 1, fill_and_stroke);
    place(frame, 22, 81101, 208896, 1024, 1, fill_and_stroke);
    place(frame, 23, 111002, 208896, 1024, 1, fill_and_stroke);
    place(frame, 24, 140902, 208896, 1024, 1, fill_and_stroke);
    place(frame, 25, 170803, 208896, 1024, 1, fill_and_stroke);
    place(frame, 26, 200704, 208896, 1024, 1, fill_and_stroke);
    place(frame, 27, 230605, 208896, 1024, 1, fill_and_stroke);
    place(frame, 28, 260506, 208896, 1024, 1, fill_and_stroke);
    place(frame, 29, 290406, 208896, 1024, 1, fill_and_stroke);
    place(frame, 30, 320307, 208896, 1024, 1, fill_and_stroke);
    place(frame, 31, 350208, 208896, 1024, 1, fill_and_stroke);
    place(frame, 32, 380109, 208896, 1024, 1, fill_and_stroke);
    place(frame, 33, 410010, 208896, 1024, 1, fill_and_stroke);
    place(frame, 34, 439910, 208896, 1024, 1, fill_and_stroke);
    place(frame, 35, 63488, 75776, 1024, 1, stroke);
    place(frame, 36, 51200, 58368, 1024, 2, fill_and_stroke);
    place(frame, 37, 81101, 58368, 1024, 2, fill_and_stroke);
    place(frame, 38, 111002, 58368, 1024, 2, fill_and_stroke);
    place(frame, 39, 140902, 58368, 1024, 2, fill_and_stroke);
    place(frame, 40, 170803, 58368, 1024, 2, fill_and_stroke);
    place(frame, 41, 200704, 58368, 1024, 2, fill_and_stroke);
    place(frame, 42, 230605, 58368, 1024, 2, fill_and_stroke);
    place(frame, 43, 260506, 58368, 1024, 2, fill_and_stroke);
    place(frame, 44, 290406, 58368, 1024, 2, fill_and_stroke);
    place(frame, 45, 320307, 58368, 1024, 2, fill_and_stroke);
    place(frame, 46, 350208, 58368, 1024, 2, fill_and_stroke);
    place(frame, 47, 380109, 58368, 1024, 2, fill_and_stroke);
    place(frame, 48, 410010, 58368, 1024, 2, fill_and_stroke);
    place(frame, 49, 439910, 58368, 1024, 2, fill_and_stroke);
    place(frame, 50, 51200, 207872, 1024, 2, fill_and_stroke);
    place(frame, 51, 81101, 207872, 1024, 2, fill_and_stroke);
    place(frame, 52, 111002, 207872, 1024, 2, fill_and_stroke);
    place(frame, 53, 140902, 207872, 1024, 2, fill_and_stroke);
    place(frame, 54, 170803, 207872, 1024, 2, fill_and_stroke);
    place(frame, 55, 200704, 207872, 1024, 2, fill_and_stroke);
    place(frame, 56, 230605, 207872, 1024, 2, fill_and_stroke);
    place(frame, 57, 260506, 207872, 1024, 2, fill_and_stroke);
    place(frame, 58, 290406, 207872, 1024, 2, fill_and_stroke);
    place(frame, 59, 320307, 207872, 1024, 2, fill_and_stroke);
    place(frame, 60, 350208, 207872, 1024, 2, fill_and_stroke);
    place(frame, 61, 380109, 207872, 1024, 2, fill_and_stroke);
    place(frame, 62, 410010, 207872, 1024, 2, fill_and_stroke);
    place(frame, 63, 439910, 207872, 1024, 2, fill_and_stroke);
    place(frame, 64, 75776, 78848, 1024, 2, fill_and_stroke);
    place(frame, 65, 75776, 187392, 1024, 2, fill_and_stroke);
    place(frame, 66, 101888, 78848, 1024, 2, fill_and_stroke);
    place(frame, 67, 101888, 187392, 1024, 2, fill_and_stroke);
    place(frame, 68, 128000, 78848, 1024, 2, fill_and_stroke);
    place(frame, 69, 128000, 187392, 1024, 2, fill_and_stroke);
    place(frame, 70, 154112, 78848, 1024, 2, fill_and_stroke);
    place(frame, 71, 154112, 187392, 1024, 2, fill_and_stroke);
    place(frame, 72, 180224, 78848, 1024, 2, fill_and_stroke);
    place(frame, 73, 180224, 187392, 1024, 2, fill_and_stroke);
    place(frame, 74, 206336, 78848, 1024, 2, fill_and_stroke);
    place(frame, 75, 206336, 187392, 1024, 2, fill_and_stroke);
    place(frame, 76, 232448, 78848, 1024, 2, fill_and_stroke);
    place(frame, 77, 232448, 187392, 1024, 2, fill_and_stroke);
    place(frame, 78, 258560, 78848, 1024, 2, fill_and_stroke);
    place(frame, 79, 258560, 187392, 1024, 2, fill_and_stroke);
    place(frame, 80, 284672, 78848, 1024, 2, fill_and_stroke);
    place(frame, 81, 284672, 187392, 1024, 2, fill_and_stroke);
    place(frame, 82, 310784, 78848, 1024, 2, fill_and_stroke);
    place(frame, 83, 310784, 187392, 1024, 2, fill_and_stroke);
    place(frame, 84, 336896, 78848, 1024, 2, fill_and_stroke);
    place(frame, 85, 336896, 187392, 1024, 2, fill_and_stroke);
    place(frame, 86, 363008, 78848, 1024, 2, fill_and_stroke);
    place(frame, 87, 363008, 187392, 1024, 2, fill_and_stroke);
    place(frame, 88, 389120, 78848, 1024, 2, fill_and_stroke);
    place(frame, 89, 389120, 187392, 1024, 2, fill_and_stroke);
    place(frame, 90, 415232, 78848, 1024, 2, fill_and_stroke);
    place(frame, 91, 415232, 187392, 1024, 2, fill_and_stroke);
    place(frame, 92, 69632, 83968, 1024, 2, stroke);
    place(frame, 93, 69632, 151552, 1024, 3, stroke);
    place(frame, 94, 264192, 83968, 1024, 2, stroke);
    place(frame, 95, 264192, 151552, 1024, 2, stroke);
    place(frame, 96, 348160, 83968, 1024, 2, stroke);
    place(frame, 97, 73728, 115712, 1024, 3, fill_and_stroke);
    place(frame, 98, 268288, 168960, 1024, 3, fill_and_stroke);
    place(frame, 99, 73728, 155648, 1024, 2, stroke);
    place(frame, 100, 135168, 155648, 1024, 2, stroke);
    place(frame, 101, 196608, 155648, 1024, 2, stroke);
    place(frame, 102, 83968, 178176, 1024, 2, fill_and_stroke);
    place(frame, 103, 95232, 178176, 1024, 3, fill_and_stroke);
    place(frame, 104, 106496, 178176, 1024, 3, fill_and_stroke);
    place(frame, 105, 117760, 178176, 1024, 3, fill_and_stroke);
    place(frame, 106, 145408, 178176, 1024, 2, fill_and_stroke);
    place(frame, 107, 156672, 178176, 1024, 3, fill_and_stroke);
    place(frame, 108, 167936, 178176, 1024, 3, fill_and_stroke);
    place(frame, 109, 179200, 178176, 1024, 3, fill_and_stroke);
    place(frame, 110, 206848, 178176, 1024, 2, fill_and_stroke);
    place(frame, 111, 218112, 178176, 1024, 3, fill_and_stroke);
    place(frame, 112, 229376, 178176, 1024, 3, fill_and_stroke);
    place(frame, 113, 240640, 178176, 1024, 3, fill_and_stroke);
    place(frame, 114, 412205, 119837, 1270, 1, fill_and_stroke);
    place(frame, 115, 303104, 115712, 1024, 3, fill_and_stroke);
    place(frame, 116, 364544, 174080, 1024, 2, fill_and_stroke);
    place(frame, 117, 364544, 178176, 1024, 2, fill_and_stroke);
    place(frame, 118, 370688, 163840, 1024, 2, fill_and_stroke);
    place(frame, 119, 393216, 174080, 1024, 2, fill_and_stroke);
    place(frame, 120, 393216, 178176, 1024, 2, fill_and_stroke);
    place(frame, 121, 399360, 163840, 1024, 2, fill_and_stroke);

    // Palette: 0 bright, 1 mid, 2 dim.
    const opxy::u32 held = input.held_notes <= 128 ? input.held_notes : 0;
    if (!held) return;
    for (opxy::u32 n = 0; n < 3; ++n) shade(frame, n, 0, stroke);  // wave, filter, env scopes
    if ((static_cast<opxy::u32>(input.slots[5]) >> 16) >= 1311) shade(frame, 3, 0, stroke);
    shade(frame, 7 + 1, 0, fill_and_stroke);    // pin 27 (the second top leg from the left)
    shade(frame, 36 + 1, 0, fill_and_stroke);   // its bond wire
    for (opxy::u32 v = 0; v < 3 && v < held; ++v) shade(frame, 99 + v, 0, stroke);
    if (pick(input.slots[4], 6) != 0) {
        for (opxy::u32 k = 0; k < 4; ++k) {      // pins 1-4 and their wires
            shade(frame, 21 + k, 0, fill_and_stroke);
            shade(frame, 50 + k, 0, fill_and_stroke);
        }
        for (opxy::u32 c = 116; c < 122; ++c) shade(frame, c, 0, fill_and_stroke);
    }
}
}
