// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 op-machines
#pragma once
#include <opxy/page_header.hpp>

namespace silsid {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<4, 64, 130, 16, 14>;
struct DisplayState { opxy::u32 phase; opxy::draw::PageHeader header; };
}
