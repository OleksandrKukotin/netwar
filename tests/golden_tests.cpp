// Golden master for M1 (issue #4): the 40-tick run of the coax network that
// the GDD section 5 chart depicts. The table is the executable spec — any
// change to the tick pipeline, the wave table or the integer rounding shows
// up here as a diff in milliunits. The property tests below it state what
// the chart is meant to show, so a deliberate rebalance can regenerate the
// table without losing the design intent.

#include <catch2/catch_test_macros.hpp>

#include <netwar/engine.hpp>
#include <netwar/scenario.hpp>
#include <netwar/tier.hpp>

#include <array>
#include <cstddef>

using namespace netwar;

namespace {

// State after each tick, in milliunits.
struct Row {
    Tick tick;
    Signal hub;
    Signal west_relay;
    Signal east_relay;
    Signal west_intensity;
    Signal east_intensity;
};

constexpr std::array<Row, 40> kGolden = {{
    //   hub   w.relay  e.relay  w.int  e.int
    { 1,   1000,  22800,   5300,     0,  8000},
    { 2,   2000,  22988,   1227,  2472,  7608},
    { 3,   3000,  20935,      0,  4704,  6472},
    { 4,   4000,  17217,      0,  6472,  4704},
    { 5,   5000,  12549,   1328,  7608,  2472},
    { 6,   6000,   7722,   5062,  8000,     0},
    { 7,   7000,   3528,   8609,  7608,     0},
    { 8,   8000,    680,  11979,  6472,     0},
    { 9,   9000,      0,  15181,  4704,     0},
    {10,  10000,   1328,  18222,  2472,     0},
    {11,  11000,   5062,  21111,     0,     0},
    {12,  12000,   8609,  23856,     0,     0},
    {13,  13000,  11979,  26464,     0,     0},
    {14,  14000,  15181,  28941,     0,     0},
    {15,  15000,  18222,  31294,     0,     0},
    {16,  16000,  21111,  33530,     0,     0},
    {17,  17000,  23856,  33182,     0,  2472},
    {18,  18000,  26464,  30619,     0,  4704},
    {19,  19000,  28941,  26417,     0,  6472},
    {20,  20000,  31294,  21289,     0,  7608},
    {21,  21000,  33530,  16025,     0,  8000},
    {22,  22000,  33182,  11416,  2472,  7608},
    {23,  23000,  30619,   8174,  4704,  6472},
    {24,  24000,  26417,   6862,  6472,  4704},
    {25,  25000,  21289,   7847,  7608,  2472},
    {26,  26000,  16025,  11255,  8000,     0},
    {27,  27000,  11416,  14493,  7608,     0},
    {28,  28000,   8174,  17569,  6472,     0},
    {29,  29000,   6862,  20491,  4704,     0},
    {30,  30000,   7847,  23267,  2472,     0},
    {31,  31000,  11255,  25904,     0,     0},
    {32,  32000,  14493,  28409,     0,     0},
    {33,  33000,  17569,  30789,     0,     0},
    {34,  34000,  20491,  33050,     0,     0},
    {35,  35000,  23267,  35198,     0,     0},
    {36,  36000,  25904,  37239,     0,     0},
    {37,  37000,  28409,  36706,     0,  2472},
    {38,  38000,  30789,  33967,     0,  4704},
    {39,  39000,  33050,  29597,     0,  6472},
    {40,  40000,  35198,  24310,     0,  7608},
}};

struct FrontIds {
    NodeId relay;
    NodeId intensity;
};
constexpr FrontIds kFronts[] = {{ids::kWestRelay, ids::kWestIntensity},
                                {ids::kEastRelay, ids::kEastIntensity}};

} // namespace

TEST_CASE("golden: the coax network reproduces the 40-tick run exactly") {
    Engine engine(make_golden_scenario());
    const Graph& g = engine.graph();

    for (const Row& row : kGolden) {
        engine.tick();
        INFO("tick " << row.tick);
        REQUIRE(engine.current_tick() == row.tick);
        CHECK(g.find(ids::kHubBuffer)->stored == row.hub);
        CHECK(g.find(ids::kWestRelay)->stored == row.west_relay);
        CHECK(g.find(ids::kEastRelay)->stored == row.east_relay);
        CHECK(g.find(ids::kWestIntensity)->value == row.west_intensity);
        CHECK(g.find(ids::kEastIntensity)->value == row.east_intensity);
    }

    // Net income of 9/tick against 8/tick of forward lines: the hub creeps
    // up by one unit a tick and never vents heat inside the run.
    CHECK(engine.wasted_heat() == 0);
}

TEST_CASE("golden: relays replenish during every lull") {
    // A quiet front is fed 4/tick and leaks 5% of at most ~37 units, so its
    // relay must rise on every tick without combat.
    Engine engine(make_golden_scenario());

    std::array<Signal, 2> before{};
    for (int i = 0; i < 40; ++i) {
        for (std::size_t f = 0; f < 2; ++f) before[f] = engine.graph().find(kFronts[f].relay)->stored;
        engine.tick();
        for (std::size_t f = 0; f < 2; ++f) {
            const Graph& g = engine.graph();
            if (g.find(kFronts[f].intensity)->value != 0) continue;
            INFO("front " << f << " tick " << engine.current_tick());
            CHECK(g.find(kFronts[f].relay)->stored > before[f]);
        }
    }
}

TEST_CASE("golden: each front passes through every brownout tier") {
    Engine engine(make_golden_scenario());

    // Tiers seen per front, indexed by Tier.
    std::array<std::array<bool, 3>, 2> seen{};
    for (int i = 0; i < 40; ++i) {
        engine.tick();
        for (std::size_t f = 0; f < 2; ++f) {
            const Tier t = tier_of(engine.graph().find(kFronts[f].relay)->stored);
            seen[f][static_cast<std::size_t>(t)] = true;
        }
    }

    for (std::size_t f = 0; f < 2; ++f) {
        INFO("front " << f);
        CHECK(seen[f][static_cast<std::size_t>(Tier::Blackout)]);
        CHECK(seen[f][static_cast<std::size_t>(Tier::SemiAutonomous)]);
        CHECK(seen[f][static_cast<std::size_t>(Tier::Directed)]);
    }
}

TEST_CASE("golden: the fronts flare out of phase") {
    // No tick in the run has both fronts at their peak, and the peaks
    // alternate: each front's flare peaks while the other is quiet.
    for (const Row& row : kGolden) {
        INFO("tick " << row.tick);
        if (row.west_intensity == units(8)) CHECK(row.east_intensity == 0);
        if (row.east_intensity == units(8)) CHECK(row.west_intensity == 0);
    }
}
