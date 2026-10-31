// SPDX-License-Identifier: MIT
// Copyright (c) 2026 op-machines
#pragma once
#include <opxy/page_header.hpp>

namespace silay {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<2, 64, 29, 16, 16>;
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::u32 serial, env_phase, env_cycle;
};
}
