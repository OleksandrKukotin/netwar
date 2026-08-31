#pragma once

#include "core.hpp"

#include <string>
#include <vector>

namespace netwar {

enum class NodeKind {
    Source,   // generates signal every tick (Command Hub)
    Pool,     // buffers signal, optionally capped with overflow-to-waste (Hub Buffer, Relays)
    Gate,     // throttles how much may be pulled upstream per tick (Routers)
    Drain,    // permanently consumes signal (Combat, Decay, Upkeep)
    Register, // computes a value each tick that drives other nodes (Combat Intensity)
};

struct Node {
    NodeId id{};
    NodeKind kind{};
    std::string name;

    // Source
    Signal generation{}; // units/tick produced

    // Pool
    Signal stored{};   // current buffer contents
    Signal capacity{}; // 0 = unlimited; surplus overflows and is wasted as heat

    // Gate
    Signal allocation{}; // max units/tick the gate may pull from its upstream pool

    // Drain
    Signal consumption{}; // fixed pull per tick; 0 = driven by another phase (decay, combat)
    Signal consumed{};    // lifetime total destroyed here (drains only)
};

// Directed edge carrying signal between nodes, throttled per tick.
// throughput 1 = copper, 4 = coax, 8 = fiber (see GDD section 4B).
struct Connection {
    std::uint32_t id{};
    NodeId from{};
    NodeId to{};
    Signal throughput{};
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Connection> connections;

    Node* find(NodeId id);
    const Node* find(NodeId id) const;
};

} // namespace netwar
