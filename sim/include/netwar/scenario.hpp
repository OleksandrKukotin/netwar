#pragma once

#include "graph.hpp"

namespace netwar {

// Node and connection IDs from the GDD (section 4).
namespace ids {
inline constexpr NodeId kCommandHub = 202;
inline constexpr NodeId kHubBuffer = 203;
inline constexpr NodeId kWestRelay = 205;
inline constexpr NodeId kWestDecay = 206;
inline constexpr NodeId kWestCombat = 207;
inline constexpr NodeId kWestIntensity = 208;
inline constexpr NodeId kWestRouter = 214;
inline constexpr NodeId kEastRouter = 215;
inline constexpr NodeId kEastCombat = 216;
inline constexpr NodeId kEastRelay = 218;
inline constexpr NodeId kEastDecay = 219;
inline constexpr NodeId kEastIntensity = 220;
inline constexpr NodeId kUpkeepDrain = 226; // drain fed by connection 227

inline constexpr std::uint32_t kWestLine = 222;
inline constexpr std::uint32_t kEastLine = 224;
inline constexpr std::uint32_t kUpkeepLine = 227;
} // namespace ids

// Builds the baseline two-front economy from the GDD:
// Hub (12/tick) -> Buffer (cap 100) -> two 8/tick routers over 1/tick
// copper lines -> relays (west starts at 20, east at 10) drained by
// out-of-phase combat intensity and 5% decay.
Graph make_readme_scenario();

} // namespace netwar
