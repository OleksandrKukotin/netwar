#include <catch2/catch_test_macros.hpp>

#include <netwar/engine.hpp>
#include <netwar/scenario.hpp>

using namespace netwar;

TEST_CASE("readme scenario wires the core graph") {
    const Graph g = make_readme_scenario();

    const Node* hub = g.find(ids::kCommandHub);
    REQUIRE(hub != nullptr);
    CHECK(hub->kind == NodeKind::Source);
    CHECK(hub->generation == units(12));

    const Node* buffer = g.find(ids::kHubBuffer);
    REQUIRE(buffer != nullptr);
    CHECK(buffer->capacity == units(100));

    REQUIRE(g.find(ids::kWestRelay) != nullptr);
    REQUIRE(g.find(ids::kEastRelay) != nullptr);
    CHECK(g.find(ids::kWestRelay)->stored == units(20));
    CHECK(g.find(ids::kEastRelay)->stored == units(10));
}

TEST_CASE("tick counter advances") {
    Engine engine(make_readme_scenario());
    REQUIRE(engine.current_tick() == 0);
    engine.tick();
    engine.tick();
    CHECK(engine.current_tick() == 2);
}

TEST_CASE("hub buffer never exceeds its cap") {
    Engine engine(make_readme_scenario());
    const Signal cap = engine.graph().find(ids::kHubBuffer)->capacity;

    for (int i = 0; i < 200; ++i) {
        engine.tick();
        CHECK(engine.graph().find(ids::kHubBuffer)->stored <= cap);
    }
}

TEST_CASE("copper lines throttle routing to 1 unit per tick") {
    Engine engine(make_readme_scenario());
    engine.tick();
    CHECK(engine.graph().find(ids::kWestRelay)->stored == units(21));
    CHECK(engine.graph().find(ids::kEastRelay)->stored == units(11));
}

TEST_CASE("upkeep drains 3 per tick from the hub buffer") {
    Engine engine(make_readme_scenario());
    engine.tick();
    // 12 generated - 3 upkeep - 1 west - 1 east
    CHECK(engine.graph().find(ids::kHubBuffer)->stored == units(7));
    CHECK(engine.graph().find(ids::kUpkeepDrain)->consumed == units(3));
}

TEST_CASE("upgraded lines carry more, but never beyond the router allocation") {
    Graph g = make_readme_scenario();
    g.find(ids::kHubBuffer)->stored = units(50);
    for (auto& conn : g.connections) {
        if (conn.id == ids::kWestLine) conn.throughput = units(4);  // coax
        if (conn.id == ids::kEastLine) conn.throughput = units(16); // beyond fiber
    }

    Engine engine(g);
    engine.tick();
    CHECK(engine.graph().find(ids::kWestRelay)->stored == units(24));
    // 16/tick line, but the router allocation (8) caps the transfer
    CHECK(engine.graph().find(ids::kEastRelay)->stored == units(18));
}

TEST_CASE("routers stop pulling when the hub buffer runs dry") {
    Graph g = make_readme_scenario();
    g.find(ids::kCommandHub)->generation = 0;
    g.find(ids::kHubBuffer)->stored = units(4);

    Engine engine(g);
    engine.tick();
    // Upkeep takes 3, the west router (first in node order) the last unit
    CHECK(engine.graph().find(ids::kHubBuffer)->stored == 0);
    CHECK(engine.graph().find(ids::kWestRelay)->stored == units(21));
    CHECK(engine.graph().find(ids::kEastRelay)->stored == units(10));
}

TEST_CASE("buffer surplus overflows to heat once the cap is reached") {
    Engine engine(make_readme_scenario());
    // Net +7/tick (12 in, 3 upkeep, 1+1 routed): 98 buffered after tick 14,
    // then generation overshoots the 100 cap by 10 once and by 7 per tick after.
    for (int i = 0; i < 40; ++i) engine.tick();
    CHECK(engine.graph().find(ids::kHubBuffer)->stored == units(95));
    CHECK(engine.wasted_heat() == units(10 + 7 * 25));
}

TEST_CASE("signal is conserved across storage, drains, and heat") {
    Engine engine(make_readme_scenario());
    const Signal seeded = units(20 + 10); // initial relay charges

    for (int i = 0; i < 100; ++i) engine.tick();

    Signal stored = 0;
    Signal consumed = 0;
    for (const auto& node : engine.graph().nodes) {
        stored += node.stored;
        consumed += node.consumed;
    }
    CHECK(stored + consumed + engine.wasted_heat() - seeded == units(12) * 100);
}

TEST_CASE("pools never go negative") {
    Engine engine(make_readme_scenario());

    for (int i = 0; i < 200; ++i) {
        engine.tick();
        for (const auto& node : engine.graph().nodes) {
            if (node.kind == NodeKind::Pool) {
                CHECK(node.stored >= 0);
            }
        }
    }
}
