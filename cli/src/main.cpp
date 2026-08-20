#include <netwar/engine.hpp>
#include <netwar/scenario.hpp>

#include <cstdio>

namespace {

double as_units(netwar::Signal s) {
    return static_cast<double>(s) / netwar::kSignalScale;
}

} // namespace

int main() {
    netwar::Engine engine(netwar::make_readme_scenario());

    std::printf("NETWAR economy core — 40-tick dry run\n");
    std::printf("%5s %12s %12s %12s\n", "tick", "hub buffer", "west relay", "east relay");

    for (int i = 0; i < 40; ++i) {
        engine.tick();
        const auto& g = engine.graph();
        std::printf("%5llu %12.1f %12.1f %12.1f\n",
                    static_cast<unsigned long long>(engine.current_tick()),
                    as_units(g.find(netwar::ids::kHubBuffer)->stored),
                    as_units(g.find(netwar::ids::kWestRelay)->stored),
                    as_units(g.find(netwar::ids::kEastRelay)->stored));
    }

    std::printf("\n(routing, decay and combat phases land in M1 — see README roadmap)\n");
    return 0;
}
