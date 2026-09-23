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

**Playable prototype.** The deterministic economy core (M0/M1) now drives a terminal game: a conductor console where you juggle command bandwidth between two fronts (early M2). The match rules are a provisional playtest ruleset — see [How to play](#how-to-play). See **[docs/ROADMAP.md](docs/ROADMAP.md)** (playtest findings: [docs/PLAYTEST.md](docs/PLAYTEST.md)) and the [open issues](https://github.com/OleksandrKukotin/netwar/issues).

## Building

Requires CMake ≥ 3.24 and a C++20 compiler (GCC/Clang). First configure fetches Catch2, so network access is needed once.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build          # run the test suite
./build/cli/netwar              # play (optional arg: seed, e.g. ./build/cli/netwar 42)
./build/cli/netwar_cli          # 40-tick economy dry run
```

First configure also fetches FTXUI (terminal UI). Use a terminal with Unicode and 256/true-colour support (Windows Terminal, any modern Linux terminal) at least ~110 columns wide.

## How to play

You command a base, not an army. The Command Hub makes 12 signal per tick; 3 go to upkeep, and two routers push the rest down forward lines into the West and East relays. The enemy flares the fronts in turn, and combat drains each relay. The relay's level sets the front's command tier:

| Relay | Tier | Front line |
|---|---|---|
| > 15 | **DIRECTED** | pushes forward |
| 5–15 | SEMI-AUTONOMOUS | slowly gives ground |
| < 5 | BLACKOUT | falls back fast |

Push either front to 100% for a **breakthrough**; let either collapse to 0% and you lose. Lines start as copper (1/tick) — spend matter (+1/tick) on coax (4/tick, 30) and fiber (8/tick, 50). Once your lines can carry more than the hub makes, you have to choose who gets fed: router allocation and priority. The scope shows 20 ticks of enemy intel, so fill a relay *before* its flare. Spectrum storms triple relay decay, and the enemy escalates every 100 ticks.

| Key | Action |
|---|---|
| ← → / a d / Tab | select front |
| ↑ ↓ / w s | router allocation ±1 |
| u | upgrade the selected line |
| p | give the selected router priority on the hub buffer |
| space | pause / resume |
| + − | tick speed (0.25–3 s; default 1 s) |
| n | single step while paused |
| h / r / q | briefing / new match / quit |

Balance is tuned with bots in `tests/match_tests.cpp`: doing nothing loses in ~80 ticks, buying upgrades and never touching the routers wins about 1 match in 4, and a crude juggling bot wins ~9 in 10 at ~250 ticks.

## Project structure

```
netwar/
├── sim/     libnetwar_sim — pure economy core: node graph + tick engine (no I/O)
├── cli/     terminal game (FTXUI) + economy dry run
├── tests/   Catch2 suite: invariants + golden-master scenarios
└── docs/    game design document, roadmap
```

The `sim` library is deliberately headless and deterministic (integer-only math, fixed tick order) — the foundation for lockstep multiplayer and replays later.
