// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
#pragma once
#include <opxy/audio.hpp>
#include "sid.h"

namespace silsid {
// One OP-XY voice is one whole 6581: its voice 1 plays the note, its voice 3
// runs silent as the modulator for sync and ring (the chip wires voice 1 to
// voice 3), voice 2 stays off.
struct Voice {
    sid_t chip;
    opxy::u32 samples;   // since the note started, for the arp's PAL frames
    opxy::u32 restart;   // the gate goes off for one block, then on: a new note
    opxy::i16 buffer[opxy::block_frames];
    opxy::i32 last;
    opxy::u8 regs[0x19];
    opxy::u8 written[0x19];
};
struct Shared {};
}
