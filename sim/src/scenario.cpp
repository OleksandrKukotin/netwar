#include "netwar/scenario.hpp"

namespace netwar {

Graph make_readme_scenario() {
    Graph g;

    // A. Core generation & storage
    g.nodes.push_back({.id = ids::kCommandHub,
                       .kind = NodeKind::Source,
                       .name = "Command Hub",
                       .generation = units(12)}); // base_signal_gen
    g.nodes.push_back({.id = ids::kHubBuffer,
                       .kind = NodeKind::Pool,
                       .name = "Hub Signal Buffer",
                       .capacity = units(100)}); // hub_bandwidth_cap

    // B. Distribution
    g.nodes.push_back({.id = ids::kWestRouter,
                       .kind = NodeKind::Gate,
                       .name = "West Router",
                       .allocation = units(8)}); // router_bandwidth
    g.nodes.push_back({.id = ids::kEastRouter,
                       .kind = NodeKind::Gate,
                       .name = "East Router",
                       .allocation = units(8)});

    // C. Consumption & environment
    g.nodes.push_back({.id = ids::kWestRelay,
                       .kind = NodeKind::Pool,
                       .name = "West Relay",
                       .stored = units(20)});
    g.nodes.push_back({.id = ids::kEastRelay,
                       .kind = NodeKind::Pool,
                       .name = "East Relay",
                       .stored = units(10)});
    g.nodes.push_back({.id = ids::kWestCombat, .kind = NodeKind::Drain, .name = "West Combat"});
    g.nodes.push_back({.id = ids::kEastCombat, .kind = NodeKind::Drain, .name = "East Combat"});
    g.nodes.push_back({.id = ids::kWestDecay,
                       .kind = NodeKind::Drain,
                       .name = "West Decay",
                       .decay_per_mille = 50}); // signal_decay = 0.05
    g.nodes.push_back({.id = ids::kEastDecay,
                       .kind = NodeKind::Drain,
                       .name = "East Decay",
                       .decay_per_mille = 50});
    g.nodes.push_back({.id = ids::kWestIntensity, .kind = NodeKind::Register, .name = "West Combat Intensity"});
    g.nodes.push_back({.id = ids::kEastIntensity, .kind = NodeKind::Register, .name = "East Combat Intensity"});
    g.nodes.push_back({.id = ids::kUpkeepDrain,
                       .kind = NodeKind::Drain,
                       .name = "Local Defense Upkeep",
                       .consumption = units(3)});

    // Wiring. Forward lines start as unshielded copper (1/tick); coax
    // upgrades raise them to 4, fiber to the router maximum of 8.
    g.connections.push_back({.from = ids::kCommandHub, .to = ids::kHubBuffer, .throughput = units(12)});
    g.connections.push_back({.from = ids::kHubBuffer, .to = ids::kWestRouter, .throughput = units(8)});
    g.connections.push_back({.from = ids::kHubBuffer, .to = ids::kEastRouter, .throughput = units(8)});
    g.connections.push_back({.id = ids::kWestLine, .from = ids::kWestRouter, .to = ids::kWestRelay, .throughput = units(1)});
    g.connections.push_back({.id = ids::kEastLine, .from = ids::kEastRouter, .to = ids::kEastRelay, .throughput = units(1)});
    g.connections.push_back({.id = ids::kUpkeepLine, .from = ids::kHubBuffer, .to = ids::kUpkeepDrain, .throughput = units(3)});
    g.connections.push_back({.from = ids::kWestRelay, .to = ids::kWestCombat, .throughput = units(8)});
    g.connections.push_back({.from = ids::kEastRelay, .to = ids::kEastCombat, .throughput = units(8)});
    g.connections.push_back({.from = ids::kWestRelay, .to = ids::kWestDecay, .throughput = units(8)});
    g.connections.push_back({.from = ids::kEastRelay, .to = ids::kEastDecay, .throughput = units(8)});

    return g;
}

} // namespace netwar
