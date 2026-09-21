#pragma once

#include "core.hpp"

#include <array>
#include <cstddef>

namespace netwar {

// Combat intensity follows a wave, but std::sin is implementation-defined in
// its low bits — two machines could disagree and desync a lockstep match (M4).
// So the wave is a precomputed integer table: one full period, sampled every
// 18 degrees, in per-mille. sin(18k degrees) * 1000, rounded to nearest.
inline constexpr std::int64_t kWavePeriod = 20;

inline constexpr std::array<std::int64_t, kWavePeriod> kSineTable = {
    0,    309,  588,  809,  951,  1000, 951,  809,  588,  309,
    0,    -309, -588, -809, -951, -1000, -951, -809, -588, -309,
};

// Sine of `step` ticks into the wave, in per-mille. Negative steps wrap the
// same way positive ones do, so a phase offset may run either direction.
constexpr std::int64_t sine_per_mille(std::int64_t step) {
    const auto index = static_cast<std::size_t>(((step % kWavePeriod) + kWavePeriod) % kWavePeriod);
    return kSineTable[index];
}

// A cosine is the same table read a quarter period ahead — that quarter-period
// offset is what puts the two fronts out of phase (GDD section 3C).
inline constexpr std::int64_t kCosineOffset = kWavePeriod / 4;

} // namespace netwar
