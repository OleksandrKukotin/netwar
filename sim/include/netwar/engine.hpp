#pragma once

#include "graph.hpp"

namespace netwar {

class Engine {
public:
    explicit Engine(Graph graph);

    // Advances the simulation by exactly one tick. Phases run in a fixed
    // order so every machine computes the identical result:
    //   generate -> route -> decay -> combat
    void tick();

    [[nodiscard]] Tick current_tick() const { return tick_; }
    [[nodiscard]] const Graph& graph() const { return graph_; }
    Graph& graph() { return graph_; }

    // Cumulative signal lost to pool overflow (GDD: "wasted as system heat").
    [[nodiscard]] Signal wasted_heat() const { return heat_; }

private:
    void generate(); // Sources push into downstream pools (cap + overflow)
    void route();    // Fixed drains, then gates pull from pools over throttled lines
    void decay();    // Decay drains leak a per-mille slice of their upstream pool
    void combat();   // Intensity registers sample the wave, then feed combat drains

    // Adds signal to a pool, clamping to capacity; the surplus becomes heat.
    void deposit(Node& pool, Signal amount);

    Graph graph_;
    Tick tick_ = 0;
    Signal heat_ = 0;
};

} // namespace netwar
