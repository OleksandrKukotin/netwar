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
    std::printf("%5s %11s %11s %7s %11s %7s %8s %8s\n",
                "tick", "hub buffer", "west relay", "w.int", "east relay", "e.int",
                "upkeep", "heat");

    for (int i = 0; i < 40; ++i) {
        engine.tick();
        const auto& g = engine.graph();
        std::printf("%5llu %11.1f %11.2f %7.2f %11.2f %7.2f %8.1f %8.1f\n",
                    static_cast<unsigned long long>(engine.current_tick()),
                    as_units(g.find(netwar::ids::kHubBuffer)->stored),
                    as_units(g.find(netwar::ids::kWestRelay)->stored),
                    as_units(g.find(netwar::ids::kWestIntensity)->value),
                    as_units(g.find(netwar::ids::kEastRelay)->stored),
                    as_units(g.find(netwar::ids::kEastIntensity)->value),
                    as_units(g.find(netwar::ids::kUpkeepDrain)->consumed),
                    as_units(engine.wasted_heat()));
    }

    std::printf("\n(brownout tiers land next — issue #4)\n");
    return 0;
}
