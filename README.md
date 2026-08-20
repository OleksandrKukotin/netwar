# NETWAR

> *The line of communication is the line of fire.*

NETWAR is a reinvention of the RTS genre: command authority is not a magical, instant input but a physical, vulnerable network resource — **Command Bandwidth**. Your orders are literal data packets, generated at your base and routed across physical infrastructure to the front lines. Cut the line, and the army goes dark.

## Why it's different

- **No death ball.** Massing your army in one place spikes local command demand past what your network can deliver — the sector browns out and you lose micro-control. Force distribution is survival.
- **No turtling.** Base defense costs constant upkeep bandwidth. Staying home starves your field armies; you must expand to capture relays.
- **APM replaced by allocation.** Combat demand oscillates out-of-phase between fronts. Skill is reading the waves and re-routing bandwidth in real time — not clicking faster.
- **Graceful degradation.** Starved units don't freeze; they step down through autonomy tiers — direct micro → squad AI → blackout and retreat.
- **Electronic warfare.** Jam enemy relays, hijack blacked-out armies, trace a micro-heavy opponent's signal back to their hidden infrastructure.

📖 Full design doc: **[docs/GDD.md](docs/GDD.md)** · Ideas & lore: **[ideas.md](ideas.md)**

## Status

**M0 complete** — deterministic economy core scaffolded (fixed-point signal math, node-graph model from the GDD, first invariant tests). **M1 in progress** — the full tick pipeline. See **[docs/ROADMAP.md](docs/ROADMAP.md)** and the [open issues](https://github.com/OleksandrKukotin/netwar/issues).

## Building

Requires CMake ≥ 3.24 and a C++20 compiler (GCC/Clang). First configure fetches Catch2, so network access is needed once.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build          # run the test suite
./build/cli/netwar_cli          # 40-tick economy dry run
```

## Project structure

```
netwar/
├── sim/     libnetwar_sim — pure economy core: node graph + tick engine (no I/O)
├── cli/     terminal frontend driving the sim
├── tests/   Catch2 suite: invariants + golden-master scenarios
└── docs/    game design document, roadmap
```

The `sim` library is deliberately headless and deterministic (integer-only math, fixed tick order) — the foundation for lockstep multiplayer and replays later.
