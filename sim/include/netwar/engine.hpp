#pragma once

#include "graph.hpp"

namespace netwar {

struct EngineConfig {
    // signal_decay: fraction of stored signal a relay leaks per tick.
    // Kept in per-mille (50 = 5%) so the sim stays in integer math.
    std::int64_t decay_per_mille = 50;
};

class Engine {
public:
    explicit Engine(Graph graph, EngineConfig config = {});

    // Advances the simulation by exactly one tick. Phases run in a fixed
    // order so every machine computes the identical result:
    //   generate -> route -> decay -> combat
    void tick();

    [[nodiscard]] Tick current_tick() const { return tick_; }
    [[nodiscard]] const Graph& graph() const { return graph_; }
    Graph& graph() { return graph_; }

private:
    void generate(); // Sources push into downstream pools (cap + overflow)
    // TODO(M1): route()  — gates pull from the hub buffer over throttled lines
    // TODO(M1): decay()  — relays leak decay_per_mille of their stored signal
    // TODO(M1): combat() — intensity registers (sine/cosine) drive combat drains

    Graph graph_;
    EngineConfig config_;
    Tick tick_ = 0;
};

} // namespace netwar
