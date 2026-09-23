#pragma once

#include "core.hpp"

namespace netwar {

// Structure autonomy tiers read off the local relay level (GDD section 3).
// Starvation is a gradient: a structure never stops, it gets dumber.
enum class Tier {
    Blackout,       // < 5 units: baseline behavior only
    SemiAutonomous, // 5-15 units: standing doctrine, slow to retask
    Directed,       // > 15 units: full intelligence
};

inline constexpr Signal kDirectedAbove = units(15);
inline constexpr Signal kBlackoutBelow = units(5);

constexpr Tier tier_of(Signal relay) {
    if (relay > kDirectedAbove) return Tier::Directed;
    if (relay >= kBlackoutBelow) return Tier::SemiAutonomous;
    return Tier::Blackout;
}

} // namespace netwar
