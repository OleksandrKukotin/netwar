# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

NETWAR — an RTS where command authority is a physical network resource ("Command Bandwidth"): orders are data packets routed from a hub over throttled lines to front-line relays. The design doc is `docs/GDD.md` (node IDs, constants, and mechanics referenced throughout the code), the milestone plan is `docs/ROADMAP.md` (tracked in GitHub issues #1–#6), and `ideas.md` is a non-binding brainstorm notebook.

## Build & test

Requires CMake ≥ 3.24 and a C++20 compiler. **Do not use MSVC** — the owner wants a Microsoft-free toolchain. On this Windows machine use w64devkit GCC; on Linux, plain gcc/clang works with no changes.

```sh
# Windows (Git Bash): w64devkit/bin MUST be on PATH or g++ can't find its assembler ("cannot execute 'as'")
PATH="/c/Users/Oleksandr/w64devkit/bin:$PATH" "/c/Program Files/CMake/bin/cmake.exe" -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
PATH="/c/Users/Oleksandr/w64devkit/bin:$PATH" "/c/Program Files/CMake/bin/cmake.exe" --build build -j 8

# Tests (Catch2 + FTXUI via FetchContent — first configure needs network)
"/c/Program Files/CMake/bin/ctest.exe" --test-dir build --output-on-failure
./build/tests/netwar_tests.exe "hub buffer never exceeds its cap"   # single test by name
./build/cli/netwar.exe [seed]                                       # the playable FTXUI console (needs a real terminal)
./build/cli/netwar_cli.exe                                          # 40-tick economy dry run
```

## Architecture

Three targets with one-way dependencies: `sim/` (static lib `netwar_sim`) ← `cli/` ← nothing else yet. Future layers (tactical, netcode, graphical client) must link `netwar_sim` unchanged — never add I/O, rendering, timers, or platform code to `sim/`.

`sim/` is a Machinations-style resource graph mirroring the GDD exactly:

- `graph.hpp` — `Node` (kinds: Source/Pool/Gate/Drain/Register) and `Connection` (throttled edge). Node and connection IDs (202, 203, 214…) are the GDD's IDs — keep them in sync with `docs/GDD.md`.
- `engine.hpp/.cpp` — `Engine::tick()` runs phases in fixed order: `generate → route → decay → combat` (route/decay/combat are M1 work, issues #1–#3).
- `scenario.hpp/.cpp` — `make_readme_scenario()` builds the GDD's two-front economy; it is the fixture for tests and the CLI.
- `tier.hpp` — brownout tiers (DIRECTED >15 / SEMI 5–15 / BLACKOUT <5) read off a relay level.
- `match.hpp/.cpp` — `Match`, the playable ruleset layered on `Engine`: player commands (line upgrades, router allocation, priority), matter, front-line hold, seeded flare jitter/escalation, storms, outcome. **Provisional playtest rules, not GDD canon.** Its constants were tuned with the bots in `tests/match_tests.cpp`; the "balance" test there fails if a rules change shifts the balance — retune deliberately rather than loosening the test.

`cli/src/play.cpp` is the FTXUI game (`netwar` target); it only renders `Match` and maps keys to its commands. The clock thread never touches the match — it posts closures to the UI thread.

### Determinism rules (non-negotiable)

The sim must produce bit-identical results on every machine — lockstep multiplayer (M4) and replay support depend on it:

- All signal math is integer fixed-point: `Signal` = int64 milliunits (`units(12)` = 12000). No floats in `sim/`.
- Rates like decay are per-mille integers (`decay_per_mille = 50` ≡ 5%).
- No `std::sin`/`std::cos` — combat intensity waves (issue #3) use an integer lookup table.
- Iterate nodes/connections in a fixed order; never depend on pointer or hash order.

Tests enforce invariants (pools never negative, caps never exceeded); M1 ends with a golden-master test reproducing the 40-tick chart in `docs/GDD.md` section 5 — treat that chart as an executable spec.

## Licensing intent

No LICENSE file yet (issue #5). Agreed plan: GPL-3.0 for code, all-rights-reserved for design docs (`docs/`, `ideas.md`). Don't add a different license.
