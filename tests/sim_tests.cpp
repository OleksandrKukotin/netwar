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
