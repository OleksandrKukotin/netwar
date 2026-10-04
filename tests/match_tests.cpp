#include <catch2/catch_test_macros.hpp>

#include <netwar/match.hpp>
#include <netwar/scenario.hpp>
#include <netwar/tier.hpp>

#include <stdexcept>
#include <utility>

using namespace netwar;

namespace {

constexpr Front kFronts[] = {Front::West, Front::East};

Front other(Front f) { return f == Front::West ? Front::East : Front::West; }

// The front that needs bandwidth first: the one flaring, the emptier of two
// flaring fronts, or the one whose next flare comes sooner.
Front urgent(const Match& m) {
    if (m.flaring(Front::West) != m.flaring(Front::East)) {
        return m.flaring(Front::West) ? Front::West : Front::East;
    }
    if (m.flaring(Front::West)) {
        return m.relay(Front::West) <= m.relay(Front::East) ? Front::West : Front::East;
    }
    return m.ticks_to_flare(Front::West) <= m.ticks_to_flare(Front::East) ? Front::West
                                                                          : Front::East;
}

// Upgrades both lines as soon as matter allows, then never touches the
// routers — the set-and-forget player the design exists to punish.
void set_and_forget(Match& m) {
    const Front first = m.grade(Front::West) <= m.grade(Front::East) ? Front::West : Front::East;
    if (!m.upgrade_line(first)) m.upgrade_line(other(first));
}

// Same upgrades, plus a crude juggle: priority to the urgent front, and a
// full quiet relay is throttled so the hub does not vent into it as heat.
void juggler(Match& m) {
    const Front weaker = m.grade(Front::West) < m.grade(Front::East)   ? Front::West
                         : m.grade(Front::East) < m.grade(Front::West) ? Front::East
                         : m.hold(Front::West) <= m.hold(Front::East)  ? Front::West
                                                                       : Front::East;
    m.upgrade_line(weaker);

    Front focus = urgent(m);
    Front rest = other(focus);
    if (m.relay(focus) > m.relay_capacity() * 3 / 4 && m.ticks_to_flare(rest) < 6) {
        std::swap(focus, rest);
    }
    m.set_priority(focus);
    m.set_allocation(focus, m.router_max());
    const bool rest_full = !m.flaring(rest) && m.relay(rest) > m.relay_capacity() - units(4);
    m.set_allocation(rest, rest_full ? units(1) : m.router_max());
}

template <class Policy>
Match play(std::uint64_t seed, Policy policy, Tick limit = 2000) {
    Match m(MatchRules{.seed = seed});
    while (m.outcome() == Outcome::InProgress && m.tick() < limit) {
        policy(m);
        m.step();
    }
    return m;
}

} // namespace

TEST_CASE("brownout tiers follow the GDD thresholds") {
    CHECK(tier_of(units(16)) == Tier::Directed);
    CHECK(tier_of(units(15) + 1) == Tier::Directed);
    CHECK(tier_of(units(15)) == Tier::SemiAutonomous);
    CHECK(tier_of(units(5)) == Tier::SemiAutonomous);
    CHECK(tier_of(units(5) - 1) == Tier::Blackout);
    CHECK(tier_of(0) == Tier::Blackout);
}

TEST_CASE("line upgrades cost matter and step copper -> coax -> fiber") {
    Match m;
    const MatchRules& r = m.rules();
    REQUIRE(m.matter() == r.starting_matter);
    REQUIRE(m.line_throughput(Front::West) == units(1));

    REQUIRE(m.upgrade_line(Front::West));
    CHECK(m.grade(Front::West) == LineGrade::Coax);
    CHECK(m.line_throughput(Front::West) == units(4));
    CHECK(m.matter() == r.starting_matter - r.coax_cost);

    // Not enough matter for fiber yet: nothing changes.
    CHECK_FALSE(m.upgrade_line(Front::West));
    CHECK(m.grade(Front::West) == LineGrade::Coax);

    while (m.matter() < r.fiber_cost) m.step();
    REQUIRE(m.upgrade_line(Front::West));
    CHECK(m.line_throughput(Front::West) == units(8));
    CHECK(m.upgrade_cost(Front::West) == 0);
    CHECK_FALSE(m.upgrade_line(Front::West));
}

TEST_CASE("router allocation is clamped to the router maximum") {
    Match m;
    m.set_allocation(Front::East, units(50));
    CHECK(m.allocation(Front::East) == m.router_max());
    m.set_allocation(Front::East, -units(3));
    CHECK(m.allocation(Front::East) == 0);
}

TEST_CASE("priority puts a router first in line for the hub buffer") {
    Match m;
    CHECK(m.priority() == Front::West); // scenario node order
    m.set_priority(Front::East);
    CHECK(m.priority() == Front::East);
    m.set_priority(Front::East); // idempotent
    CHECK(m.priority() == Front::East);
    m.set_priority(Front::West);
    CHECK(m.priority() == Front::West);
}

TEST_CASE("a router set to zero routes nothing to its relay") {
    Match m;
    m.set_allocation(Front::West, 0);
    for (int i = 0; i < 5; ++i) {
        const Signal before = m.relay(Front::West);
        m.step();
        CHECK(m.relay(Front::West) <= before); // only decay and combat move it
    }
}

TEST_CASE("a router moves what its line allows, not what it asks for") {
    Match m;
    CHECK(m.requested_flow(Front::West) == units(1)); // router 8/t over copper 1/t
    m.set_allocation(Front::West, units(3));
    CHECK(m.requested_flow(Front::West) == units(1)); // still the line
    REQUIRE(m.upgrade_line(Front::West));             // coax 4/t
    CHECK(m.requested_flow(Front::West) == units(3)); // now the router
    m.step();
    CHECK(m.delivered(Front::West) == units(3));
    CHECK(m.delivered(Front::East) == units(1));
}

TEST_CASE("a short hub serves the priority router first") {
    Match m(MatchRules{.starting_matter = units(200)});
    for (Front f : {Front::West, Front::East}) {
        REQUIRE(m.upgrade_line(f));
        REQUIRE(m.upgrade_line(f)); // fiber: 8 + 8 outruns the hub's 9/t
    }
    m.set_priority(Front::East);
    for (int i = 0; i < 30; ++i) m.step();
    CHECK(m.delivered(Front::East) == m.requested_flow(Front::East));
    CHECK(m.delivered(Front::West) < m.requested_flow(Front::West));
    CHECK(m.delivered(Front::West) + m.delivered(Front::East) == units(9));
}

TEST_CASE("relays never exceed their field capacity") {
    Match m;
    for (int i = 0; i < 300 && m.outcome() == Outcome::InProgress; ++i) {
        m.upgrade_line(Front::West);
        m.upgrade_line(Front::East);
        m.step();
        for (Front f : kFronts) {
            CHECK(m.relay(f) >= 0);
            CHECK(m.relay(f) <= m.relay_capacity());
            CHECK(m.hold(f) >= 0);
            CHECK(m.hold(f) <= kHoldMax);
        }
    }
}

TEST_CASE("the deployment window keeps the opening quiet") {
    Match m;
    for (Tick t = 0; t < m.rules().opening_grace; ++t) {
        m.step();
        CHECK(m.intensity(Front::West) == 0);
        CHECK(m.intensity(Front::East) == 0);
    }
}

TEST_CASE("ticks_to_flare counts down to the first audible flare") {
    // Issue #9: inside the deployment window the wave turns positive but the
    // flare is silent, and the countdown must skip it rather than read 0.
    for (const Tick grace : {Tick{0}, Tick{10}, Tick{25}}) {
        Match m(MatchRules{.seed = 5, .opening_grace = grace});
        for (int t = 0; t < 80 && m.outcome() == Outcome::InProgress; ++t) {
            for (Front f : kFronts) {
                const Tick wait = m.ticks_to_flare(f);
                Match probe = m;
                for (Tick i = 0; i < wait; ++i) {
                    probe.step();
                    CHECK(probe.intensity(f) == 0);
                }
                probe.step();
                CHECK(probe.intensity(f) > 0);
            }
            juggler(m);
            m.step();
        }
    }
}

TEST_CASE("intel forecasts the next wave period exactly") {
    Match m(MatchRules{.seed = 3});
    // Check forecasts made from many different wave positions, including
    // inside the deployment window.
    for (int start = 0; start < 60 && m.outcome() == Outcome::InProgress; ++start) {
        Signal forecast[kFrontCount][20];
        for (Front f : kFronts) {
            for (Tick ahead = 1; ahead <= 20; ++ahead) {
                forecast[static_cast<std::size_t>(f)][ahead - 1] = m.forecast_intensity(f, ahead);
            }
        }
        Match probe = m;
        for (Tick ahead = 1; ahead <= 20 && probe.outcome() == Outcome::InProgress; ++ahead) {
            probe.step();
            for (Front f : kFronts) {
                CHECK(probe.intensity(f) == forecast[static_cast<std::size_t>(f)][ahead - 1]);
            }
        }
        juggler(m);
        m.step();
    }
}

TEST_CASE("the same seed and commands replay to the same match") {
    const Match a = play(7, juggler, 400);
    const Match b = play(7, juggler, 400);
    CHECK(a.tick() == b.tick());
    CHECK(a.outcome() == b.outcome());
    CHECK(a.matter() == b.matter());
    CHECK(a.engine().wasted_heat() == b.engine().wasted_heat());
    for (Front f : kFronts) {
        CHECK(a.hold(f) == b.hold(f));
        CHECK(a.relay(f) == b.relay(f));
    }

    const Match c = play(8, juggler, 400);
    CHECK((c.tick() != a.tick() || c.hold(Front::West) != a.hold(Front::West)));
}

TEST_CASE("a match is decided and then frozen") {
    Match m = play(1, [](Match&) {});
    REQUIRE(m.outcome() == Outcome::Defeat);
    const Tick end = m.tick();
    m.step();
    CHECK(m.tick() == end);
    CHECK_FALSE(m.upgrade_line(Front::West));
}

// Balance guard for the provisional M2 ruleset. The numbers were tuned with
// these bots; if a rules change breaks this, the change moved the balance.
TEST_CASE("balance: doing nothing loses, juggling beats set-and-forget") {
    int idle_losses = 0;
    int static_wins = 0;
    int juggler_wins = 0;
    constexpr int kSeeds = 40;

    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        idle_losses += play(seed, [](Match&) {}).outcome() == Outcome::Defeat;
        static_wins += play(seed, set_and_forget).outcome() == Outcome::Victory;
        juggler_wins += play(seed, juggler).outcome() == Outcome::Victory;
    }

    CHECK(idle_losses == kSeeds);
    CHECK(juggler_wins >= kSeeds * 3 / 4);
    CHECK(static_wins <= kSeeds / 3);
}

TEST_CASE("inconsistent rules are rejected at construction") {
    CHECK_THROWS_AS(Match(MatchRules{.escalation_period = 0}), std::invalid_argument);
    CHECK_THROWS_AS(Match(MatchRules{.storm_gap_min = 90, .storm_gap_max = 60}),
                    std::invalid_argument);
    CHECK_THROWS_AS(Match(MatchRules{.flare_jitter_per_mille = 1001}), std::invalid_argument);
    CHECK_THROWS_AS(Match(MatchRules{.flare_jitter_per_mille = -1}), std::invalid_argument);
    CHECK_NOTHROW(Match(MatchRules{.flare_jitter_per_mille = 1000}));
    CHECK_NOTHROW(Match(MatchRules{.storm_gap_min = 60, .storm_gap_max = 60}));
}

TEST_CASE("calm weather restores the scenario's own relay decay") {
    const Graph scenario = make_readme_scenario();
    const std::int64_t base = scenario.find(ids::kWestDecay)->decay_per_mille;

    Match m;
    bool saw_storm = false;
    bool saw_calm_after_storm = false;
    for (int t = 0; t < 400 && m.outcome() == Outcome::InProgress; ++t) {
        juggler(m);
        const bool storm = m.storm_active(); // step() sets decay from this
        m.step();
        const std::int64_t decay = m.engine().graph().find(ids::kWestDecay)->decay_per_mille;
        CHECK(decay == (storm ? m.rules().storm_decay_per_mille : base));
        saw_calm_after_storm = saw_calm_after_storm || (saw_storm && !storm);
        saw_storm = saw_storm || storm;
    }
    CHECK(saw_calm_after_storm);
}

TEST_CASE("escalation shows up when the stronger flare is forecast") {
    Match m;
    const MatchRules& r = m.rules();
    CHECK(m.flare_base() == r.base_amplitude);

    Tick escalated_at = 0;
    for (int t = 0; t < 400 && m.outcome() == Outcome::InProgress && escalated_at == 0; ++t) {
        juggler(m);
        m.step();
        if (m.flare_base() > r.base_amplitude) escalated_at = m.tick();
    }
    REQUIRE(escalated_at > 0);
    // Peaks are rolled at a flare's start, so the first escalated roll comes
    // after the escalation tick, and it lands in the forecast, not the flare.
    CHECK(escalated_at > r.escalation_period);
    CHECK(m.flare_base() == r.base_amplitude + r.escalation_step);
}
