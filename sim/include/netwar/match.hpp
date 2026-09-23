#pragma once

#include "engine.hpp"
#include "tier.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace netwar {

// A playable single-player match layered over the GDD two-front economy.
//
// The Engine is a pure resource graph; Match adds everything that turns it
// into a game: player commands, a matter economy to pay for line upgrades,
// front lines that move with the command tier, escalating and jittered
// flares, and spectrum-weather storms. These rules are a PROVISIONAL M2
// playtest ruleset, not GDD canon — they exist to find out whether juggling
// bandwidth between fronts is fun.
//
// Everything here stays integer and seeded, so two machines fed the same
// seed and the same commands on the same ticks produce the same match (M4).

enum class Front : std::size_t { West = 0, East = 1 };
inline constexpr std::size_t kFrontCount = 2;

enum class LineGrade { Copper, Coax, Fiber };

enum class Outcome { InProgress, Victory, Defeat };

// Front line position in milli-percent: 0 = collapsed, 100000 = breakthrough.
using Hold = std::int64_t;
inline constexpr Hold kHoldMax = 100'000;

struct MatchRules {
    std::uint64_t seed = 1;

    // Matter: the conventional economy (GDD 1A). Spent on line upgrades,
    // never on signal.
    Signal starting_matter = units(30);
    Signal matter_income = units(1); // per tick
    Signal coax_cost = units(30);    // copper -> coax
    Signal fiber_cost = units(50);   // coax -> fiber

    // Field transceivers hold a limited buffer; surplus is vented as heat,
    // so a quiet front cannot be pre-filled without limit.
    Signal relay_capacity = units(40);

    // Front line movement per tick = enemy intensity x factor (per-mille of
    // a hold point per unit of intensity), chosen by the relay's tier.
    Hold starting_hold = 50'000;
    std::int64_t directed_push = 175;
    std::int64_t semi_push = -25;
    std::int64_t blackout_push = -300;

    // Deployment window: flares that begin before this tick are silent, so
    // the player gets a moment to read the board before the first contact.
    Tick opening_grace = 10;

    // Enemy pressure: each flare's amplitude is base + escalation, then
    // jittered, so the waves are never perfectly predictable (GDD 4C note).
    Signal base_amplitude = units(8);
    Signal escalation_step = units(1);
    Tick escalation_period = 100;
    Signal max_amplitude = units(16);
    std::int64_t flare_jitter_per_mille = 250;

    // Spectrum weather (GDD 6A): forecast storms that raise relay decay.
    std::int64_t storm_decay_per_mille = 150;
    Tick storm_warning = 20;
    Tick storm_length = 15;
    Tick storm_gap_min = 60;
    Tick storm_gap_max = 120;
};

class Match {
public:
    explicit Match(MatchRules rules = {});

    // --- Player commands (valid between ticks) ---------------------------

    // Upgrades the forward line copper -> coax -> fiber. Returns false when
    // the line is already fiber or the matter is not there.
    bool upgrade_line(Front front);
    // Router allocation, clamped to 0..router maximum (8 units/tick).
    void set_allocation(Front front, Signal allocation);
    // The router with priority pulls from the hub buffer first.
    void set_priority(Front front);

    // Advances one tick. Does nothing once the match is decided.
    void step();

    // --- State ------------------------------------------------------------

    [[nodiscard]] Tick tick() const { return engine_.current_tick(); }
    [[nodiscard]] Outcome outcome() const { return outcome_; }
    [[nodiscard]] const Engine& engine() const { return engine_; }
    [[nodiscard]] const MatchRules& rules() const { return rules_; }

    [[nodiscard]] Signal matter() const { return matter_; }
    [[nodiscard]] Signal hub_buffer() const;
    [[nodiscard]] Signal hub_capacity() const;
    [[nodiscard]] Front priority() const;

    [[nodiscard]] LineGrade grade(Front front) const { return side(front).grade; }
    [[nodiscard]] Signal line_throughput(Front front) const;
    [[nodiscard]] Signal allocation(Front front) const;
    [[nodiscard]] Signal router_max() const { return router_max_; }
    // Matter needed for the next upgrade; 0 when the line is already fiber.
    [[nodiscard]] Signal upgrade_cost(Front front) const;

    [[nodiscard]] Signal relay(Front front) const;
    [[nodiscard]] Signal relay_capacity() const { return rules_.relay_capacity; }
    [[nodiscard]] Tier tier(Front front) const { return tier_of(relay(front)); }
    [[nodiscard]] Hold hold(Front front) const { return side(front).hold; }

    // Enemy combat intensity sampled on the last tick (0 during lulls).
    [[nodiscard]] Signal intensity(Front front) const;
    [[nodiscard]] bool flaring(Front front) const { return intensity(front) > 0; }
    // Peak of the flare in progress, or of the last one during a lull.
    [[nodiscard]] Signal flare_peak(Front front) const { return side(front).current_peak; }
    // Intel: the peak of the next flare is known one flare in advance.
    [[nodiscard]] Signal next_flare_peak(Front front) const { return side(front).next_peak; }
    // Ticks until the front's next flare begins (0 while flaring).
    [[nodiscard]] Tick ticks_to_flare(Front front) const;
    // Intel forecast: the intensity the front will sample `ahead` ticks from
    // now (1 = the next step). Exact within one wave period, barring
    // escalation of flares that have not been forecast yet.
    [[nodiscard]] Signal forecast_intensity(Front front, Tick ahead) const;

    [[nodiscard]] bool storm_active() const;
    [[nodiscard]] Tick storm_start() const { return storm_start_; }
    [[nodiscard]] Tick storm_end() const { return storm_start_ + rules_.storm_length; }
    // True while a storm is inside its forecast window or raging.
    [[nodiscard]] bool storm_forecast() const;

private:
    struct Side {
        NodeId relay{};
        NodeId router{};
        NodeId intensity{};
        NodeId decay{};
        std::uint32_t line{};
        std::int64_t phase{};
        LineGrade grade = LineGrade::Copper;
        Hold hold{};
        Signal current_peak{};
        Signal next_peak{};
    };

    Side& side(Front front) { return sides_[static_cast<std::size_t>(front)]; }
    const Side& side(Front front) const { return sides_[static_cast<std::size_t>(front)]; }

    Connection& line(Front front);
    const Connection& line(Front front) const;
    // Wave table step the front's register will sample on the next tick.
    [[nodiscard]] std::int64_t wave_step(Front front) const;

    std::uint64_t next_random();
    Signal roll_peak();
    void schedule_storm(Tick after);
    void update_holds();

    MatchRules rules_;
    Engine engine_;
    std::array<Side, kFrontCount> sides_{};
    Signal router_max_{};
    Signal matter_{};
    Tick storm_start_{};
    std::uint64_t rng_{};
    Outcome outcome_ = Outcome::InProgress;
};

} // namespace netwar
