#pragma once

#include <cstdint>

namespace netwar {

// Signal is fixed-point: 1 unit of bandwidth == 1000 milliunits.
// Integer math keeps the simulation bit-identical across machines,
// which deterministic lockstep multiplayer (M4) depends on.
using Signal = std::int64_t;
inline constexpr Signal kSignalScale = 1000;

constexpr Signal units(std::int64_t whole) { return whole * kSignalScale; }

using Tick = std::uint64_t;
using NodeId = std::uint32_t;

} // namespace netwar
