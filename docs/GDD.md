# NETWAR — Game Design Document

> *The line of communication is the line of fire.*

## 1. Design Philosophy: The Netwar Conflict

Traditional Real-Time Strategy (RTS) games operate on an unrealistic assumption: the player is an omniscient, omnipresent deity capable of executing instantaneous micro-actions across an entire map. This expectation shifts the genre's skill ceiling away from deep strategy and toward raw mechanical physical execution (Actions Per Minute).

NETWAR is a fundamental reinvention of the RTS genre. It physicalizes command authority, transforming it from a magical, instant input into a tangible, fluid, and vulnerable network resource called **Command Bandwidth**.

In NETWAR, commands are literal packets of data generated at your primary base and transmitted across physical network topologies to the front lines. If your infrastructure is disrupted, your units lose operational efficiency. If your front line becomes too chaotic, your signal networks suffer "command brownouts." By replacing high mechanical APM with spatial, logistical bandwidth management, NETWAR bridges the gap between grand strategy and high-stakes tactical combat.

### A. Signal is authority, not fuel

NETWAR runs **two separate economies**, and conflating them collapses the design:

- **Matter** — conventional resources. Harvested from the map, spent on construction. This is the familiar RTS economy and it behaves in the familiar way.
- **Signal (Command Bandwidth)** — *not* fuel. Signal never powers a structure on or off. It determines whether a structure operates **under your direction** or **on its own recognisance**.

The distinction is load-bearing. If a structure starved of signal simply stopped working, signal would be electricity, and NETWAR would be an energy-management game that already exists several times over. So the rule is absolute:

> **A structure starved of signal never stops. It gets dumber.**

Every structure has a baseline autonomous behavior that costs nothing and always runs. Signal buys *intelligence* on top of it: adaptation, target selection, retasking, efficiency. A blacked-out mining hub keeps mining — it just keeps mining a vein that ran out.

### B. You command a base, not an army

There is **no manual army control in NETWAR**. No unit selection, no move orders, no ability clicks, no control groups. The player's hands never touch a soldier.

What the player commands is the **base**: what to build, where to extend the network, and — continuously — where to spend command authority. Armies, where they exist, are a projection of the combat structures that produced them, and they fight on standing doctrine.

This is what makes the anti-APM thesis structural rather than aspirational. You cannot out-click anyone, because there is nothing to click. The only competitive axis left is the quality of your network and your allocation of attention across it.

### C. The core loop

You will never have enough signal to make your entire base intelligent at once. Base growth *dilutes* authority: every new structure is another claimant on the same bandwidth. The moment-to-moment game is therefore one continuous question:

> **Which part of my machine is allowed to be smart right now?**

That is the replacement for mechanical APM — not speed of execution, but allocation of attention under scarcity. Expansion punishes itself without any artificial penalty, and there is no "finished" build order, because the right allocation changes as the situation does.

## 2. The Base as the Unit

The base is not a backdrop for the army; it *is* the played object. It is a graph: structures are nodes, the lines you lay between them are edges, and command authority flows from the HQ outward along those edges.

### A. Structure classes

| Class | Function | With full signal | Starved (autonomous) |
|---|---|---|---|
| **Command HQ** | Origin of all command authority | — | — (losing it is losing the war) |
| **Resource Hub** | Consumes signal, produces matter | Optimises extraction, switches veins as they deplete, adapts hauling routes | Keeps extracting at baseline efficiency; will not notice an exhausted vein |
| **Control Hub** | Extends the network's reach; enables expansion and map presence | Projects authority forward, supports new construction, extends the signal horizon | Holds existing ground only; no further extension |
| **Defensive Hub** | Protects territory and infrastructure | Selects targets, focuses fire, coordinates with neighbours | Fires at whatever walks into range |
| **Offensive Hub** | Projects force into contested territory | Coordinated pressure, responsive doctrine, retasking mid-engagement | Committed to its last standing order until it dies or wins |

### B. Relays are the map objective

Signal originates at the HQ, which creates a structural hazard: if authority only radiates from one point, the entire game reduces to distance-from-base, and every faction converges on the same radial shape.

**Relays are the answer.** They are pre-Silence infrastructure ruins scattered across the map — capturable, repairable, and the only way to carry authority beyond the HQ's natural horizon. Expansion in NETWAR is therefore not "more territory" but "authority projected further." The map reads as a network to be wired rather than an area to be covered, and the fight for it is a fight for wire and repeaters rather than for ground.

## 3. Structure Autonomy: Graceful Degradation

Signal starvation is a **gradient, never a cliff**. Nothing in a NETWAR base is ever switched off by an enemy; it is progressively deafened. Each structure steps down through behavioral tiers based on the signal volume available in its local relay:

```
[Signal Strength in Local Relay]
 ├── High (>15 units)  ───> DIRECTED: full intelligence — adaptive, coordinated, retaskable in real time.
 ├── Mid  (5-15 units) ───> SEMI-AUTONOMOUS: acts on standing doctrine and local heuristics; slow to retask.
 └── Low  (<5 units)   ───> BLACKOUT: baseline behavior only. Still functioning, no longer listening.
```

Where armies exist, they inherit the tier of the structure that fielded them: DIRECTED formations manoeuvre and focus fire, SEMI-AUTONOMOUS ones follow general move-attack vectors on squad AI, and blacked-out ones default to defensive behavior and head home.

### A. Brownout is a tool, not a failure state

Because degradation is graceful and reversible, **deliberately browning out a quiet sector to fund a flare elsewhere is correct play**, not a mistake. This is the juggle the whole game is built around, and it only works if SEMI-AUTONOMOUS is a genuinely usable mode rather than a punishment tier.

It therefore carries a real compensation, drawn directly from the fiction (see section 7C, Signal Tracing): **dense command traffic is loud.** A DIRECTED sector radiates a traceable signal pulse; an autonomous one is quiet. Precision costs visibility. A player who keeps their whole base intelligent is a player broadcasting their entire layout to the enemy.

The tiers are thus not a quality scale but a trade: intelligence against stealth, everywhere, all the time.

## 4. Core Architecture & System Flow

The tactical battlefield in NETWAR is governed by a split-route logistics network where bandwidth must be active, buffered, and constantly divided between active combat fronts:

```
                                    ┌──[West Router] ──(1)──> [West Relay] ──> [West Combat/Decay]
[Command Hub] ──(12)──> [Hub Buffer] ──┤
                                    ├──[East Router] ──(1)──> [East Relay] ──> [East Combat/Decay]
                                    └──[Local Defense Upkeep] ──(3)──> [Drain]
```

This is the baseline two-front economy: the smallest graph that still contains every mechanic. It is also the fixture the simulation core and its tests are built on, so the node IDs and constants below are **normative** — they are mirrored exactly in `sim/src/scenario.cpp` and must stay in sync with it.

This continuous resource pipeline is structurally modeled via three functional layers:

### A. Core Generation & Storage Layer

- **Command Hub** (Source | ID 202) — The central mainframe of the player's base, constantly broadcasting a raw signal generation of 12 units/tick (`base_signal_gen`).
- **Hub Signal Buffer** (Pool | ID 203) — The primary command capacitor at the home base. It has a strict maximum storage limit of 100 units (`hub_bandwidth_cap`). Any surplus bandwidth generated beyond this cap automatically overflows (`overflow: drain`) and is permanently wasted as system heat.

### B. Distribution Layer (Tactical Routing)

- **Domestic Base Costs** (Resource Connection | ID 227) — A static background drain of 3 units/tick representing the continuous computational energy required to maintain automated base defenses and protective shields.
- **West & East Routers** (Gates | ID 214, 215) — Grid routers managing outbound data packages. Each router has an allocation limit of 8 units/tick (`router_bandwidth`) to pull from the Hub Signal Buffer.
- **West & East Forward Lines** (Resource Connections | ID 222, 224) — These lines represent your transmission infrastructure. In their baseline state, they are bottlenecked to carry only 1 unit/tick to their respective relays. Upgrading your physical relays on the map scales this connection's throughput up to the router's maximum limit of 8.

### C. Consumption & Environmental Layer

- **West & East Relays** (Pools | ID 205, 218) — Deployable field transceivers positioned near active fronts, buffering local command data. The West Relay begins with 20 stored units; the East Relay begins with 10.
- **West & East Combat** (Drains | ID 207, 216) — Live combat operations that draw bandwidth directly from their local relays to execute unit pathing, ability activations, and dynamic targeting.
- **West & East Combat Intensity** (Registers | ID 208, 220) — Out-of-phase tactical cycles playing the natural cadence of war. West Front Intensity is evaluated via a sine wave; East Front Intensity via a cosine wave, so demand oscillates between the two fronts.
- **West & East Decay** (Drains | ID 206, 219) — Environmental signal decay. Relays naturally leak bandwidth proportional to their current volume, losing 5% (`signal_decay = 0.05`) of their stored signal to background electromagnetic noise every tick.

> **The intensity waves are scaffolding.** Sine and cosine are a stand-in that lets the economy be proven before any tactical layer exists. They are also *predictable*, which means a patient player can solve the optimal routing policy once and then stop making decisions — precisely the failure this design exists to prevent. In the finished game the demand curve on each front is drawn by **the opponent**, who will deliberately spike intensity where the network is thin. Replacing the waves with real activity (M3) is therefore not a refinement; it is the point at which the core bet becomes testable at all.

## 5. Resolving Classic RTS Flaws Mathematically

NETWAR implements a series of hard mathematical limitations to systematically dismantle archaic RTS exploits:

```
Bandwidth / Intensity
 100 ────────────────────────────────────────── [Hub Signal Buffer] (Cap: 100 with active overflow)
  90 ──────────────────────────────────────────
  80 ──────────────────────────────────────────
  70 ──────────────────────────────────────────
  60 ──────────────────────────────────────────
  50 ─●────────────────────────────────────────
  40 ───●──────────────────────────────────────
  30 ────●───────●───●───────────────────────── [West Relay] (Replenishes during combat lulls)
  20 ─────●─────●─────●────────────────────────
  10 ──────●───●───────●─────────────────────── [East Relay] (Deploys with lower baseline buffer)
   0 ────────●───────────●───────────────────── [Combat Intensities] (Asymmetric, out-of-phase curves)
     0    5   10   15   20   25   30   35   40  Step (Tick)
```

> **⚠ Open question — resolve before the golden-master test (issue #4).** The curve above is drawn for relays that can actually replenish between flares. The baseline constants in section 4 cannot produce it. Copper lines deliver **1 unit/tick** against an average rectified combat demand of **~2.53 units/tick** (amplitude 8 × the mean of the positive half of the wave table), plus 5% decay on top. In the real 40-tick run both relays sit at zero from roughly tick 5 onward, while the Hub Buffer saturates at its cap and vents 7 units/tick as heat from tick 14. The chart also starts the West Relay near 50 where section 4C specifies 20.
>
> Three ways out, to be chosen deliberately: (a) the chart depicts an **upgraded** network (coaxial, 4/tick), and the golden test is written against that scenario while the copper baseline stands as an intentionally desperate opening position; (b) the **constants are rebalanced** so the baseline sustains itself; (c) the chart is **illustrative only**, and the acceptance test pins observed behavior instead. Until this is closed, treat the chart as intent, not as spec.

### A. The Death Ball Solution

In traditional RTS games, clumping your entire army into one coordinate on the map is the optimal strategic default. In NETWAR, doing so is a death sentence.

Massing forces in a single theater spikes the local Combat Intensity register toward its peak (8 units/tick). When combat intensity (8) and natural signal leakage exceed your active transmission rate, the local Forward Relay buffer drains to zero. This triggers an immediate **Command Brownout**, stripping the player of directed control over that specific theater. To fight effectively, forces must be distributed across multiple network zones to balance the regional command load.

### B. The Anti-Turtling Tax

To prevent passive defensive strategies, maintaining base structures requires a constant, non-negotiable Domestic Upkeep Cost (3 units/tick).

Because your Command Hub produces a flat 12 units/tick, a turtling player who spends all their resources at home is left with a maximum net surplus of 9 units/tick to distribute elsewhere. If they expand their domestic defenses further, the base cost scales up, leaving their active field armies completely starved of operational bandwidth. Players must push outward to capture and establish new, decentralized network relays to stay viable.

### C. Active Bandwidth Management (Mechanical APM Transformed)

In NETWAR, mechanical speed is replaced by strategic resource-allocation. Because the West and East Combat Intensities are out-of-phase (Sine vs. Cosine), the demand on each front oscillates.

When the West front flares up, the East front cools down. Players cannot simply set-and-forget their routers; they must actively shift and redirect transmission priorities from their base, prioritizing routing lanes to the theater that needs it most in real time.

## 6. Tempo: What a Tick Is Worth

The entire design is specified in units **per tick**, and for a long time this document never said how much real time a tick buys. That coefficient is not a detail — it decides whether the core mechanic is playable or merely present.

The wave period is **20 ticks**: one full rotation from a West flare to an East flare and back. That period is the juggle cycle, and its real-time length is the game's pulse:

| Tick length | Juggle cycle | Result |
|---|---|---|
| 100 ms | 2 s | Fronts alternate faster than a gauge can be read. Not a decision — a flicker. |
| **0.5–1 s** | **10–20 s** | You watch a front spin up, have seconds to decide, and commit. **Target band.** |
| 3 s | 60 s | Long stretches of dead air between decisions. |

For calibration: StarCraft II's larva inject cycle is roughly 29 seconds and is considered a comfortable macro rhythm. A 20-minute match at 1 s/tick is ~1200 ticks; the 40-tick acceptance run is the first 40 seconds of a game.

**Target: 0.5–1 s per tick, to be confirmed by playtest rather than by argument.** The terminal client (M2) must expose tick duration as a live runtime control so the value can be found by hand in a single sitting.

### A. Decision density

Correct tick length is necessary but not sufficient. Attention allocation only generates decisions while the situation keeps moving; against a static picture the player finds the optimum once and then watches. The design must therefore supply continuous sources of change:

1. **The opponent** — the honest source, and the reason the core bet is only truly testable in PvP.
2. **Vein depletion** — resource hubs exhaust their ground, forcing attention and new wire outward. The natural metronome of expansion.
3. **Spectrum weather** — forecast windows of elevated global `signal_decay`. Announced in advance, so they are planned brownouts to prepare for, not random punishment.
4. **Propagation delay** — if commands take time to arrive, every decision must be made ahead of need (see `ideas.md`; not yet committed).

**Acceptance criterion for the M2 playtest:** a meaningful decision at least every 10–15 seconds, and no stretch longer than ~30 seconds spent merely waiting. Dead air indicates a missing source of change from the list above, not a wrong tick rate.

## 7. Advanced Gameplay Systems

By structuring the core loop around network logistics, NETWAR unlocks several deep, emergent layers of gameplay:

### A. Infrastructure Upgrades: Upgrading the Forward Line

At the start of a match, the connection from your Gate Routers to your Relays is bottlenecked at a throughput of 1 unit/tick. This represents baseline, unshielded copper transmission lines.

- **Coaxial Upgrades** — Increases line throughput from 1 to 4 units/tick.
- **Fiber-Optic Deployment** — Fully unleashes the line to the router's maximum limit of 8 units/tick, allowing your front lines to sustain high-intensity combat without buffer starvation.

### B. The Line Is the Target

Because a base is a graph, its most valuable attack surface is not a building but **the wire between buildings**.

Severing the line to a forward resource hub does not destroy it. It *orphans* it: the hub keeps mining, blindly, at baseline efficiency, and is no longer yours to retask. A raid that cuts three lines and kills nothing can be more damaging than one that levels a structure — and it leaves the victim's map looking untouched.

This has three consequences the rest of the design depends on:

- **Raiding acquires a purpose distinct from damage.** Small, fast forces have a job that scales into the late game.
- **Map geometry becomes a real decision.** Where your cable runs — short and exposed, or long and safe — is a commitment you live with.
- **It is possible to win without destroying anything.** An opponent whose network has been quietly dismantled loses while their base still stands.

### C. Electronic Warfare (E-War) Tactics

By making command systems physical, the electromagnetic spectrum becomes a core battleground:

- **Jamming Strikes** — Support units can deploy directional jamming waves. This does not damage enemy health; instead, it temporarily spikes the enemy's regional `signal_decay` variable from 0.05 to 0.20, rapidly draining their local Relay buffers and forcing a command blackout.
- **Signal Spoofing (Hijacking)** — If an enemy's Relay buffer is completely starved to 0, specialized hacking units can "spoof" the command protocols, gaining temporary operational control of the abandoned, uncoordinated enemy units in that sector.
- **Signal Tracing** — Heavy command traffic creates visible signal pulses in the fog of war, allowing the enemy to trace the line of transmission back to the exact location of your hidden, vulnerable Forward Relays. This is the counterweight that makes autonomy a trade rather than a penalty (section 3A).
