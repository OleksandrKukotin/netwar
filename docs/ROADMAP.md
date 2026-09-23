# NETWAR — Development Roadmap

The project is built C++-first: a deterministic, headless simulation core proven in the terminal before any graphics exist. Stack: C++20, CMake, Catch2 for tests; FTXUI planned for the terminal UI.

```
netwar/
├── sim/     libnetwar_sim — pure economy core: node graph + tick engine (no I/O)
├── cli/     terminal frontend driving the sim
└── tests/   Catch2 suite: invariants + golden-master scenarios
```

## M0 — Toolchain & skeleton ✅

- [x] CMake multi-target project (`sim` static lib, `cli` executable, `tests`)
- [x] Fixed-point `Signal` type (integer milliunits) for cross-machine determinism
- [x] Node/Connection graph model mirroring the GDD (Source, Pool, Gate, Drain, Register)
- [x] GDD scenario builder with the design doc's node IDs and constants
- [x] First invariant tests (buffer cap respected, pools non-negative)

## M1 — Deterministic economy core

The heart of the game, fully playable by math alone.

- [x] Tick pipeline in fixed phase order: `generate → route → decay → combat` ([#1](https://github.com/OleksandrKukotin/netwar/issues/1))
- [x] Routing: gates pull from the Hub Buffer, throttled by line throughput (1/4/8) ([#1](https://github.com/OleksandrKukotin/netwar/issues/1))
- [x] Upkeep drain (3/tick) and buffer overflow-to-heat ([#1](https://github.com/OleksandrKukotin/netwar/issues/1))
- [x] Relay decay: 5% of stored signal per tick (`signal_decay = 0.05`) ([#2](https://github.com/OleksandrKukotin/netwar/issues/2))
- [x] Combat intensity registers: out-of-phase sine/cosine waves (integer lookup table, not `std::sin`, to guarantee determinism) ([#3](https://github.com/OleksandrKukotin/netwar/issues/3))
- [x] Brownout detection: relay state tiers High / Mid / Low (>15, 5–15, <5) ([#4](https://github.com/OleksandrKukotin/netwar/issues/4))
- [ ] **Acceptance test:** 40-tick golden run reproducing the chart in the GDD — West Relay replenishes during lulls, East Relay deploys at 10, intensities oscillate out of phase ([#4](https://github.com/OleksandrKukotin/netwar/issues/4))

## M2 — Terminal command console (FTXUI) — first playable: v0.1.0-alpha

Prove the core bet: routing bandwidth is more fun than APM. ([#6](https://github.com/OleksandrKukotin/netwar/issues/6))

- [x] Live dashboard: hub buffer gauge, relay plots, intensity waves, brownout alerts
- [x] Interactive controls: reallocate router priority, buy line upgrades (copper → coax → fiber) mid-run
- [x] Tick duration as a live runtime control (GDD section 6)
- [x] Provisional match ruleset (`sim/include/netwar/match.hpp`): matter economy for upgrades, front lines moved by command tier, jittered and escalating flares with 20-tick intel, spectrum-weather storms, win/lose — balance guarded by bot tests
- [ ] Scenario files (JSON) so constants can be tuned without recompiling
- [ ] Playtest checkpoint: does actively juggling the two fronts feel engaging? — first signal positive, see [PLAYTEST.md](PLAYTEST.md)

## M3 — Tactical layer

- [ ] Map zones with deployable/capturable relays (anti-turtling expansion loop)
- [ ] Units with the Autonomy State Machine: DIRECT MICRO / SEMI-AUTONOMOUS / BLACKOUT-RETREAT driven by local relay volume
- [ ] Combat drains driven by actual unit activity instead of synthetic waves
- [ ] Scaling domestic upkeep with base structures

## M4 — Multiplayer lockstep

- [ ] Command log: player inputs as timestamped packets (the netcode mirrors the game fiction)
- [ ] Deterministic replays from the command log
- [ ] State-hash checks per N ticks for desync detection
- [ ] LAN/loopback two-player match in the terminal client

## M5 — Electronic warfare & graphical client

- [ ] Jamming: temporarily spike enemy regional `signal_decay` 0.05 → 0.20
- [ ] Spoofing: hijack units in sectors whose relay is starved to 0
- [ ] Signal tracing: high-density command traffic reveals relay positions through fog
- [ ] Graphical client (raylib/SFML, or Godot via GDExtension) linking the same untouched `netwar_sim` library
