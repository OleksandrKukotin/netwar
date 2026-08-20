# NETWAR — Game Design Document

> *The line of communication is the line of fire.*

## 1. Design Philosophy: The Netwar Conflict

Traditional Real-Time Strategy (RTS) games operate on an unrealistic assumption: the player is an omniscient, omnipresent deity capable of executing instantaneous micro-actions across an entire map. This expectation shifts the genre's skill ceiling away from deep strategy and toward raw mechanical physical execution (Actions Per Minute).

NETWAR is a fundamental reinvention of the RTS genre. It physicalizes command authority, transforming it from a magical, instant input into a tangible, fluid, and vulnerable network resource called **Command Bandwidth**.

In NETWAR, commands are literal packets of data generated at your primary base and transmitted across physical network topologies to the front lines. If your infrastructure is disrupted, your units lose operational efficiency. If your front line becomes too chaotic, your signal networks suffer "command brownouts." By replacing high mechanical APM with spatial, logistical bandwidth management, NETWAR bridges the gap between grand strategy and high-stakes tactical combat.

## 2. Core Architecture & System Flow

The tactical battlefield in NETWAR is governed by a split-route logistics network where bandwidth must be active, buffered, and constantly divided between active combat fronts:

```
                                    ┌──[West Router] ──(1)──> [West Relay] ──> [West Combat/Decay]
[Command Hub] ──(12)──> [Hub Buffer] ──┤
                                    ├──[East Router] ──(1)──> [East Relay] ──> [East Combat/Decay]
                                    └──[Local Defense Upkeep] ──(3)──> [Drain]
```

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

## 3. Resolving Classic RTS Flaws Mathematically

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

### A. The Death Ball Solution

In traditional RTS games, clumping your entire army into one coordinate on the map is the optimal strategic default. In NETWAR, doing so is a death sentence.

Massing forces in a single theater spikes the local Combat Intensity register toward its peak (8 units/tick). When combat intensity (8) and natural signal leakage exceed your active transmission rate, the local Forward Relay buffer drains to zero. This triggers an immediate **Command Brownout**, stripping the player of micro-control over that specific army. To fight effectively, forces must be distributed across multiple network zones to balance the regional command load.

### B. The Anti-Turtling Tax

To prevent passive defensive strategies, maintaining base structures requires a constant, non-negotiable Domestic Upkeep Cost (3 units/tick).

Because your Command Hub produces a flat 12 units/tick, a turtling player who spends all their resources at home is left with a maximum net surplus of 9 units/tick to distribute elsewhere. If they expand their domestic defenses further, the base cost scales up, leaving their active field armies completely starved of operational bandwidth. Players must push outward to capture and establish new, decentralized network relays to stay viable.

### C. Active Bandwidth Management (Mechanical APM Transformed)

In NETWAR, mechanical speed is replaced by strategic resource-allocation. Because the West and East Combat Intensities are out-of-phase (Sine vs. Cosine), the demand on each front oscillates.

When the West front flares up, the East front cools down. Players cannot simply set-and-forget their routers; they must actively shift and redirect transmission priorities from their base, prioritizing routing lanes to the theater that needs it most in real time.

## 4. Advanced Gameplay Systems

By structuring the core loop around network logistics, NETWAR unlocks several deep, emergent layers of gameplay:

### A. The Unit Autonomy State Machine

When a local Forward Relay begins to starve of bandwidth, units in that sector do not freeze or vanish. Instead, they scale down through distinct behavioral tiers based on remaining signal volume:

```
[Signal Strength in Relay]
 ├── High (>15 units)  ───> DIRECT MICRO: High-fidelity manual targeting, instant ability execution, precise pathing.
 ├── Mid  (5-15 units) ───> SEMI-AUTONOMOUS: Units follow general move-attack vectors and rely on local squad AI.
 └── Low  (<5 units)   ───> BLACKOUT / RETREAT: Control link severed. Units default to defensive AI and head home.
```

### B. Infrastructure Upgrades: Upgrading the Forward Line

At the start of a match, the connection from your Gate Routers to your Relays is bottlenecked at a throughput of 1 unit/tick. This represents baseline, unshielded copper transmission lines.

- **Coaxial Upgrades** — Increases line throughput from 1 to 4 units/tick.
- **Fiber-Optic Deployment** — Fully unleashes the line to the router's maximum limit of 8 units/tick, allowing your front lines to sustain high-intensity combat without buffer starvation.

### C. Electronic Warfare (E-War) Tactics

By making command systems physical, the electromagnetic spectrum becomes a core battleground:

- **Jamming Strikes** — Support units can deploy directional jamming waves. This does not damage enemy health; instead, it temporarily spikes the enemy's regional `signal_decay` variable from 0.05 to 0.20, rapidly draining their local Relay buffers and forcing a command blackout.
- **Signal Spoofing (Hijacking)** — If an enemy's Relay buffer is completely starved to 0, specialized hacking units can "spoof" the command protocols, gaining temporary operational control of the abandoned, uncoordinated enemy units in that sector.
- **Signal Tracing** — Heavy micro-management (sending rapid, high-density command packets) creates visible signal pulses in the fog of war, allowing the enemy to trace the line of transmission back to the exact location of your hidden, vulnerable Forward Relays.
