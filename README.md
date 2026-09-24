# NETWAR

> *The line of communication is the line of fire.*

**v0.1.0-alpha**: the first playable build. It is a single-player terminal prototype of the core loop. See [What you can play today](#what-you-can-play-today).

NETWAR is a reinvention of the RTS genre. Command authority is not a magical, instant input but a physical, vulnerable network resource: **Command Bandwidth**. It is generated at your HQ and carried over wire you lay yourself to the structures that need it. You never command an army. You command a base, and every moment you decide **which part of your machine is allowed to be smart right now.**

## Why it's different

- **Signal is authority, not fuel.** NETWAR runs two economies. *Matter* builds things. *Signal* decides whether a structure acts under your direction or on its own. A structure starved of signal never stops working; it gets dumber.
- **You command a base, not an army.** There is no unit selection, no move orders, no ability clicks, no control groups. Nothing you do touches a soldier, so there is nothing to out-click. APM is replaced by allocating attention under scarcity.
- **Graceful degradation.** Each structure steps down through tiers as its local relay runs dry: DIRECTED, then SEMI-AUTONOMOUS, then BLACKOUT. Browning out a quiet sector to fund a flare elsewhere is correct play, and a dark sector is also a quiet one.
- **No death ball, no turtling.** Concentrating in one theater spikes local demand past what the wire can carry. Staying home pays a constant upkeep tax on bandwidth. Expansion means projecting authority further by capturing relays.
- **The line is the target.** Cutting a wire doesn't destroy anything. It orphans whatever sits beyond it, and a network can be dismantled while the base still stands.
- **Electronic warfare.** Jam an enemy's relays, spoof sectors that have gone silent, and trace heavy command traffic back to hidden infrastructure.

📖 Full design doc: **[docs/GDD.md](docs/GDD.md)** · Ideas & lore: **[ideas.md](ideas.md)**

## What you can play today

v0.1.0-alpha is the **conductor console**: one base, two fronts, one hub, played in the terminal against scripted enemy pressure. It exists to test the core bet: *is routing bandwidth more fun than clicking?* The first playtest says yes. It also says the game is currently too hard and explains too little. Findings and planned fixes are in **[docs/PLAYTEST.md](docs/PLAYTEST.md)**.

Not in this build: the map, structures, the matter economy beyond line upgrades, a human opponent, and electronic warfare. The match rules are a **provisional playtest ruleset** (`sim/include/netwar/match.hpp`), not the final design. Enemy pressure is synthetic: out-of-phase waves with jitter and escalation. The finished game gives that job to the opponent.

### How to play

The Command Hub makes 12 signal per tick. 3 go to upkeep, and two routers push the rest down forward lines into the West and East relays. The enemy flares the fronts in turn, and combat drains each relay. A relay's level sets its front's command tier:

| Relay | Tier | Front line |
|---|---|---|
| > 15 | **DIRECTED** | pushes forward |
| 5–15 | SEMI-AUTONOMOUS | slowly gives ground |
| < 5 | BLACKOUT | falls back fast |

Push either front to 100% for a **breakthrough**. Let either collapse to 0% and you lose.

- **Upgrade your lines.** They start as copper (1/tick). Spend matter (+1/tick) on coax (4/tick, costs 30) and then fiber (8/tick, costs 50). This is your first move.
- **Choose who gets fed.** Once your lines can carry more than the hub makes, use router allocation and priority.
- **Read the intel.** The scope shows the next 20 ticks of enemy flares, dimmed. Feed a relay *before* its flare hits.
- **Watch the weather.** Spectrum storms are forecast in advance and triple relay decay. The enemy escalates every 100 ticks.

| Key | Action |
|---|---|
| ← → / a d / Tab | select front |
| ↑ ↓ / w s | router allocation ±1 |
| u | upgrade the selected line |
| p | give the selected router priority on the hub buffer |
| space | pause / resume |
| + − | tick speed (0.25–3 s, default 1 s) |
| n | single step while paused |
| h / r / q | briefing / new match / quit |
| l | switch language (English / українська) |

Every match is seeded (the seed is shown in the header). `./build/cli/netwar 42` replays the same enemy.

The console speaks English and Ukrainian. At start-up it asks which one, with the cursor on the locale's language (`LANG=uk_UA.UTF-8` suggests Ukrainian). `--lang uk` or `--lang en` skips the question, and `l` switches mid-match. Keys work on a Ukrainian keyboard layout too.

## Building

Requires CMake ≥ 3.24 and a C++20 compiler (GCC or Clang). The first configure fetches Catch2 and FTXUI, so network access is needed once.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build          # run the test suite
./build/cli/netwar              # play (optional: a seed, e.g. ./build/cli/netwar 42)
./build/cli/netwar_cli          # 40-tick economy dry run, no UI
```

On Windows, build with MinGW-w64 GCC (e.g. [w64devkit](https://github.com/skeeto/w64devkit)) and add `-G "MinGW Makefiles"` to the configure step. Play in a terminal with Unicode and 256-colour or true-colour support, at least ~110 columns wide (Windows Terminal or any modern Linux terminal).

## Status

- **M0 and M1: deterministic economy core.** Done, except the golden-master test (#4), which waits on an open design question in GDD §5.
- **M2: terminal console.** The first playable build is v0.1.0-alpha. Still to come: UX fixes from the playtest, JSON scenario files, and more playtesting.
- **M3 and later:** the tactical layer, lockstep multiplayer, electronic warfare, and a graphical client.

Details: **[docs/ROADMAP.md](docs/ROADMAP.md)** and the [open issues](https://github.com/OleksandrKukotin/netwar/issues). For a guided read of the simulation code, see **[docs/CODE_TOUR.md](docs/CODE_TOUR.md)**.

## Project structure

```
netwar/
├── sim/     libnetwar_sim: the economy graph, tick engine and match rules (pure, no I/O)
├── cli/     netwar: the FTXUI terminal game · netwar_cli: headless economy dry run
├── tests/   Catch2 suite: engine invariants, match rules, bot-based balance guard
└── docs/    design doc, roadmap, playtest log, code tour
```

The `sim` library is deliberately headless and deterministic: integer-only math, a fixed tick order, and seeded randomness. The same seed and the same commands always produce the same match, which is the foundation for replays and lockstep multiplayer. Balance is guarded by bots in `tests/match_tests.cpp`. Doing nothing loses in ~80 ticks, buying upgrades without touching the routers wins about 1 match in 4, and a crude juggling bot wins about 9 in 10.
