# NETWAR — Code Tour

A guided walkthrough of the simulation core for anyone (including future-you) who wants to understand *why* the code looks the way it does, not just what it does. Read it top-down; each section builds on the previous one.

```
sim/include/netwar/core.hpp      the number system (fixed-point Signal)
sim/include/netwar/graph.hpp     the data model (nodes + connections)
sim/include/netwar/engine.hpp    the simulation loop (tick phases)
sim/include/netwar/scenario.hpp  the GDD's two-front economy as data
sim/src/*.cpp                    implementations
cli/src/main.cpp                 40-tick dry run printer
tests/sim_tests.cpp              invariants + exact-value checks
```

---

## 1. `core.hpp` — why there are no floats anywhere

```cpp
using Signal = std::int64_t;
inline constexpr Signal kSignalScale = 1000;
constexpr Signal units(std::int64_t whole) { return whole * kSignalScale; }
```

The single most important decision in the codebase: **all bandwidth quantities are 64-bit integers counting milliunits.** `units(12)` is the number `12000`. When you see `20.95` in the CLI output, the sim internally holds `20950`.

Why not `double`? Because M4 (lockstep multiplayer) requires every machine to compute *bit-identical* results from the same inputs. Floating-point math is not guaranteed to be identical across compilers, CPUs, and optimization flags — an `x * 0.05` that differs in the last bit on one player's machine would desync the match minutes later, and you'd have no idea why. Integer math has no such freedom: `20950 * 50 / 1000` is `1047` on every machine ever built.

The same reasoning produces two conventions you'll see everywhere:

- **Rates are per-mille integers**, not fractions: `decay_per_mille = 50` means 5%. The decay computation is `stored * 50 / 1000` — integer multiply, integer divide, truncation toward zero. Deterministic.
- **No `std::sin`/`std::cos`**: trig functions are implementation-defined in their low bits, so combat waves read a precomputed integer table instead — `wave.hpp`, twenty entries of sine in per-mille.

`Tick` (`uint64_t`) and `NodeId` (`uint32_t`) are just readable aliases.

## 2. `graph.hpp` — one struct to rule all node kinds

The sim is a [Machinations](https://machinations.io/)-style resource graph: signal flows along directed edges between typed nodes.

```cpp
enum class NodeKind { Source, Pool, Gate, Drain, Register };
```

| Kind | Game meaning | Behavior |
|---|---|---|
| `Source` | Command Hub | creates `generation` signal per tick |
| `Pool` | Hub Buffer, Relays | stores signal; optional `capacity` with overflow |
| `Gate` | West/East Routers | throttles flow, holds nothing itself |
| `Drain` | Upkeep, Decay, Combat | destroys signal permanently |
| `Register` | Combat Intensity | samples the wave each tick into `value`, which a drain reads |

Notice that `Node` is **one flat struct with fields for every kind**, rather than a class hierarchy with virtual methods:

```cpp
struct Node {
    NodeId id{};  NodeKind kind{};  std::string name;
    Signal generation{};              // Source
    Signal stored{}, capacity{};      // Pool
    Signal allocation{};              // Gate
    Signal consumption{};             // Drain: fixed pull per tick
    std::int64_t decay_per_mille{};   // Drain: % leak of upstream pool
    Signal consumed{};                // Drain: lifetime total destroyed
};
```

This is deliberate "C-style plain data" rather than OOP. Three reasons:

1. **Determinism again** — a `std::vector<Node>` of plain structs iterates in one fixed, obvious order. Polymorphic nodes behind pointers invite accidental dependence on allocation order.
2. **Serialization later** — replays and netcode (M4) need to snapshot and hash game state. Plain data copies and hashes trivially; virtual objects don't.
3. **The unused fields cost nothing.** A Pool's `allocation` is just `0`.

The convention: a field being zero means "this behavior is off." That's how one drain struct covers three different drains — upkeep has `consumption = 3000` (a fixed pull), decay drains have `decay_per_mille = 50` (a proportional leak), and combat drains have both at zero because their pull comes from `driven_by`, the register that sets their demand each tick.

`Connection` is a directed edge with a `throughput` cap per tick:

```cpp
struct Connection { uint32_t id; NodeId from, to; Signal throughput; };
```

`throughput` is the copper/coax/fiber upgrade lever: `units(1)` → `units(4)` → `units(8)`. Upgrading a line in-game is literally writing a bigger number into this field — see the test `"upgraded lines carry more..."`, which does exactly that.

`Graph::find(id)` is a linear scan returning a pointer (or `nullptr`). Linear because the graph has ~13 nodes; a hash map would be faster asymptotically but iterates in **unspecified order**, which the determinism rules forbid touching anyway.

## 3. `engine.hpp` / `engine.cpp` — the tick pipeline

The heart of the sim is one method:

```cpp
void Engine::tick() {
    generate();   // sources create signal
    route();      // upkeep + routers move it
    decay();      // relays leak it
    combat();     // intensity waves consume it
    ++tick_;
}
```

**Phase order is fixed and load-bearing.** Within one tick, generation always lands in the buffer *before* routers pull, and routing always tops up relays *before* decay bites. Reordering these changes every number in the sim — which is why the tests pin exact values, so an accidental reorder fails loudly.

### `generate()`

For every Source, find its outgoing connections and push `min(generation, throughput)` into the target pool. In the GDD scenario that's one edge: Hub → Buffer at 12/tick.

### `route()` — two sub-steps, in priority order

**First, fixed drains** (currently just Domestic Upkeep):

```cpp
const Signal take = std::min({drain.consumption, conn.throughput, upstream->stored});
```

Upkeep runs *before* the routers on purpose: the GDD calls the 3/tick domestic cost "non-negotiable" — it's the anti-turtling tax, so it gets first claim on the buffer. If the buffer holds only 2 units, upkeep takes both and the routers get nothing.

**Then, gates.** Each router pulls from its upstream pool and forwards through its outbound line *in one motion* — a router is a valve, not a tank; it holds no signal between ticks. The transfer amount is the minimum of four constraints:

```cpp
std::min({gate.allocation,     // router hardware limit (8/tick)
          in.throughput,       // buffer→router edge (8/tick)
          out.throughput,      // the forward line: 1 copper / 4 coax / 8 fiber
          upstream->stored})   // can't pull what isn't there
```

At baseline the binding constraint is the copper line (1). After a fiber upgrade it's the allocation (8). When the buffer runs dry it's `stored` — and here **iteration order becomes gameplay**: nodes are processed in `std::vector` order, and the scenario pushes West Router before East Router, so West wins contested bandwidth. That's arbitrary but *deterministic*, which is the actual requirement; player-controlled priority is an M2 feature that will replace this.

### `decay()`

For every drain with `decay_per_mille > 0`, leak a proportional slice of the upstream pool:

```cpp
const Signal loss = upstream->stored * drain.decay_per_mille / 1000;
```

Note what it *doesn't* do: it ignores the connection's `throughput`. Decay is environmental noise, not transmission — the relay→decay edge only says where the loss is booked. And integer division truncates, which has a fun consequence covered in §6.

Why does the rate live on the drain node instead of a global config? Because M5 jamming spikes decay *regionally* (one relay's 0.05 → 0.20 while the other stays calm). A global constant can't express that; a per-node field can, and a jamming strike becomes a one-field mutation.

### `deposit()` and the heat ledger

All signal entering a pool goes through one helper:

```cpp
pool.stored += amount;
if (pool.capacity > 0 && pool.stored > pool.capacity) {
    heat_ += pool.stored - pool.capacity;   // GDD: "wasted as system heat"
    pool.stored = pool.capacity;
}
```

Overflow isn't silently discarded — it's *accounted for*. Same idea as drains tracking `consumed`: every milliunit the hub ever generated is somewhere — stored in a pool, booked in a drain's `consumed`, or in `heat_`. That's not just tidiness; it makes the conservation test possible (§5), and a conservation check is the single best desync detector you can have going into M4.

## 4. `scenario.hpp` / `scenario.cpp` — the GDD as data

`make_readme_scenario()` builds the exact graph from GDD §4: hub (12/tick) → buffer (cap 100) → upkeep (3) + two routers (8) over copper lines (1) → relays (west seeded 20, east 10) → combat/decay drains.

Two things worth knowing:

- **The magic numbers 202, 203, 214… are not arbitrary** — they're the GDD's node IDs, kept in `ids::` constants so code, tests, and design doc all speak the same language. If you renumber the GDD, renumber `scenario.hpp`.
- The `{.id = ..., .kind = ...}` syntax is C++20 **designated initializers**. One gotcha: fields must appear in declaration order, so if you add a field to `Node`, initializer lists that name later fields still compile only if the order matches the struct.

The scenario is the shared fixture for both the CLI and most tests — one source of truth for "the baseline economy."

## 5. `tests/sim_tests.cpp` — three tiers of confidence

The suite deliberately mixes three kinds of tests:

**Invariants** — properties that must hold for *any* number of ticks: the buffer never exceeds its cap, pools never go negative. These loop 200 ticks and check after each one. They'll survive every future feature; if one ever fails, the engine is broken, full stop.

**Exact-value checks** — "after one tick the buffer holds exactly `units(7)`" (12 generated − 3 upkeep − 1 west − 1 east). These are intentionally brittle: in a determinism-first sim, *any* change to the numbers is a real change in behavior and should force a conscious test update. The decay slice shows up in these as `units(21) - units(21) * 50 / 1000` — written as the formula rather than `19950` so you can read *why* the number is what it is.

**Conservation** — the accountant's audit:

```
stored(everywhere) + consumed(all drains) + heat − initial seed == 12/tick × ticks
```

Every phase that creates, moves, or destroys signal has to keep this ledger balanced. A phase that leaks (subtracts from a pool without booking the loss anywhere) fails this test even if every other test passes. When issue #3 adds combat, its drains feed the same `consumed` ledger and the test keeps working unchanged.

Also note the *setup* style: tests mutate the scenario graph directly (`g.find(ids::kWestRouter)->allocation = 0`) to isolate one mechanism — e.g. silencing the routers so decay acts alone. No mocking framework; the data model *is* the test API.

## 6. A worked example: the first tick, and where 19 comes from

Tick 1, milliunits in parentheses:

1. `generate()` — hub pushes 12 into the buffer. Buffer: 12 (12000).
2. `route()` — upkeep takes 3 → buffer 9. West router: min(8, 8, **1**, 9) = 1 → buffer 8, west relay 21. East router: same → buffer 7, east relay 11.
3. `decay()` — west loses 21000·50/1000 = 1050 → 19950 (*19.95*). East loses 550 → 10450.

End state: buffer 7.0, west 19.95, east 10.45 — exactly the CLI's first row.

Run this forward and the buffer climbs +7/tick until the 100 cap, after which 7/tick burns off as heat (the game is telling you: your infrastructure can't spend what you generate — upgrade your lines). The relays converge to an equilibrium where the copper inflow balances the leak: solve *x = (x + 1) · 0.95* and you get *x = 19*.

But because integer decay **truncates**, the "equilibrium" is actually a *band*: for any stored value in [19000, 19020) milliunits, the leak rounds down to exactly 1000 and the state reproduces itself. West drifts down from 20 into the band; east climbs up from 10 into it — possibly landing on *different* values inside it. The convergence test asserts the band, not a single number. This kind of quantization artifact is the honest price of determinism, and it's harmless as long as you know it's there.

## 7. `cli/src/main.cpp` — the dry run

Forty ticks, seven columns (buffer, each relay with its combat intensity, upkeep consumed, heat), printed with `printf`. The only float math in the whole project lives here — `as_units()` divides by 1000 *purely for display*, after the sim has already produced its integer truth. Presentation may round; simulation never does.

---

## Where the code goes next

- **Issue #4 — brownouts**: relay volume maps to High / Mid / Low tiers (>15 / 5–15 / <5), and a golden-master test pins the full 40-tick run against the GDD chart.
- **M2 — the game**: FTXUI dashboard + live reallocation, where the fixed west-first priority in `route()` becomes a player decision.
