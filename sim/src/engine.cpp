#include "netwar/engine.hpp"

#include <algorithm>
#include <utility>

namespace netwar {

Engine::Engine(Graph graph, EngineConfig config)
    : graph_(std::move(graph)), config_(config) {}

void Engine::tick() {
    generate();
    // TODO(M1): route();
    // TODO(M1): decay();
    // TODO(M1): combat();
    ++tick_;
}

void Engine::generate() {
    for (const auto& source : graph_.nodes) {
        if (source.kind != NodeKind::Source) continue;

        for (const auto& conn : graph_.connections) {
            if (conn.from != source.id) continue;

            Node* target = graph_.find(conn.to);
            if (target == nullptr || target->kind != NodeKind::Pool) continue;

            target->stored += std::min(source.generation, conn.throughput);
            if (target->capacity > 0 && target->stored > target->capacity) {
                // Surplus overflows the buffer and is wasted as system heat.
                target->stored = target->capacity;
            }
        }
    }
}

} // namespace netwar
