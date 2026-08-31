#include "netwar/engine.hpp"

#include <algorithm>
#include <utility>

namespace netwar {

Engine::Engine(Graph graph) : graph_(std::move(graph)) {}

void Engine::tick() {
    generate();
    route();
    decay();
    // TODO(M1): combat(); — issue #3
    ++tick_;
}

void Engine::generate() {
    for (const auto& source : graph_.nodes) {
        if (source.kind != NodeKind::Source) continue;

        for (const auto& conn : graph_.connections) {
            if (conn.from != source.id) continue;

            Node* target = graph_.find(conn.to);
            if (target == nullptr || target->kind != NodeKind::Pool) continue;

            deposit(*target, std::min(source.generation, conn.throughput));
        }
    }
}

void Engine::route() {
    // Fixed drains first: domestic upkeep is non-negotiable (GDD 3B), so it
    // is satisfied before the routers compete for the remaining bandwidth.
    for (auto& drain : graph_.nodes) {
        if (drain.kind != NodeKind::Drain || drain.consumption == 0) continue;

        for (const auto& conn : graph_.connections) {
            if (conn.to != drain.id) continue;

            Node* upstream = graph_.find(conn.from);
            if (upstream == nullptr || upstream->kind != NodeKind::Pool) continue;

            const Signal take =
                std::min({drain.consumption, conn.throughput, upstream->stored});
            upstream->stored -= take;
            drain.consumed += take;
        }
    }

    // Gates pull from their upstream pool and forward through their outbound
    // line in one motion — routers hold no signal between ticks. Node vector
    // order fixes the contention priority deterministically.
    for (const auto& gate : graph_.nodes) {
        if (gate.kind != NodeKind::Gate) continue;

        for (const auto& in : graph_.connections) {
            if (in.to != gate.id) continue;

            Node* upstream = graph_.find(in.from);
            if (upstream == nullptr || upstream->kind != NodeKind::Pool) continue;

            for (const auto& out : graph_.connections) {
                if (out.from != gate.id) continue;

                Node* downstream = graph_.find(out.to);
                if (downstream == nullptr || downstream->kind != NodeKind::Pool) continue;

                const Signal transfer = std::min(
                    {gate.allocation, in.throughput, out.throughput, upstream->stored});
                upstream->stored -= transfer;
                deposit(*downstream, transfer);
            }
        }
    }
}

void Engine::decay() {
    // Environmental leakage is proportional to what the pool holds, not to
    // line throughput — the connection only says where the loss is booked.
    // Integer division truncates toward zero, identically on every machine.
    for (auto& drain : graph_.nodes) {
        if (drain.kind != NodeKind::Drain || drain.decay_per_mille == 0) continue;

        for (const auto& conn : graph_.connections) {
            if (conn.to != drain.id) continue;

            Node* upstream = graph_.find(conn.from);
            if (upstream == nullptr || upstream->kind != NodeKind::Pool) continue;

            const Signal loss = upstream->stored * drain.decay_per_mille / 1000;
            upstream->stored -= loss;
            drain.consumed += loss;
        }
    }
}

void Engine::deposit(Node& pool, Signal amount) {
    pool.stored += amount;
    if (pool.capacity > 0 && pool.stored > pool.capacity) {
        heat_ += pool.stored - pool.capacity;
        pool.stored = pool.capacity;
    }
}

} // namespace netwar
