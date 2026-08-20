# NETWAR — Idea Notebook

Unfiltered brainstorm space. Nothing here is committed design; promote ideas into the README/GDD when they survive playtesting.

## 1. Lore

**The Silence.** A century after the orbital relays burned, no signal crosses the planet without wire, dish, or drone. Warfare didn't end — it became *routing*. Armies are cheap; the ability to command them is not. Every faction fights for the same scarce resource: coherent bandwidth across a poisoned electromagnetic spectrum.

- Commanders are physically remote — bunkered "conductors" whose authority literally travels as packets. Losing your relay doesn't kill your soldiers; it orphans them. (This is why spoofing/hijacking makes narrative sense: orphaned units obey whoever speaks their protocol.)
- The map's neutral relays are pre-Silence infrastructure ruins — capturing them is archaeology as much as conquest.
- Fog of war is diegetic: it's not "unseen", it's *unheard* — sectors where you have no signal presence.

## 2. Races / Factions — asymmetry through the economy graph

Each faction is a different bend of the same Machinations graph. Balance lever: same total power, different topology.

### The Chorus (mesh decentralists)
- **Graph twist:** no Command Hub. Instead, every Relay is a weak generator (`base_signal_gen` 3/tick each, but they *stack*). No `hub_bandwidth_cap` — but also no big buffer to ride out spikes.
- **Unique node — Mesh Link:** relays share signal with neighbor relays at 2/tick, so the network self-heals around a destroyed node.
- **Playstyle:** expansion is literally power; map control = generation. Weak early, unkillable late.
- **Weakness:** jamming one relay leaks into neighbors (decay propagates across mesh links).

### The Spire (centralized maximalists)
- **Graph twist:** one colossal hub — `base_signal_gen` 20/tick, `hub_bandwidth_cap` 250 — but router allocation shrinks with distance from the Spire (signal attenuation: −1 throughput per map ring).
- **Unique node — Uplink Pylon:** deployable repeater that resets the attenuation counter; expensive, visible from across the map (permanent signal beacon in fog).
- **Playstyle:** overwhelming force projection near home, logistics chess far away.
- **Weakness:** the Death Ball faction by temptation — the game's anti-clumping math punishes their instincts.

### The Drift (analog broadcasters)
- **Graph twist:** no lines at all — wireless broadcast. Throughput isn't capped by connection but falls off with distance (inverse square, tabulated for determinism). `signal_decay` is doubled (0.10) everywhere: analog is lossy.
- **Unique node — Carrier Wave:** a mobile unit that *is* a relay. Their whole network can walk.
- **Playstyle:** nomadic, unjammable in the conventional sense (nothing to cut), thrives on raids.
- **Weakness:** signal tracing lights them up — broadcasting is shouting. Low ceiling on sustained front-line intensity.

### The Parasite (fourth faction / expansion?)
- **Graph twist:** minimal own generation; unique nodes tap *enemy* connections, siphoning a percentage of whatever flows past.
- Probably too gimmicky for launch — revisit after E-war (M5) exists.

## 3. Graphics & Visual Direction

**Terminal era (M2):** don't fight the medium — lean in. Phosphor-CRT command console: amber/green monochrome, scanline flicker on brownouts, relay buffers as VU meters, intensity waves as an oscilloscope strip. The FTXUI dashboard *is* the fiction: you're a conductor at a console.

**Graphical client (M5+):** the map is a dark circuit board / night-time network topology. Ideas:
- Bandwidth is the only bright thing. Signal travels as visible light pulses along lines; line quality = pulse density (copper: sparse dots → fiber: solid stream). You read your economy at a glance, no numbers needed.
- Units are dim glyphs that *brighten with control tier*: DIRECT = sharp and saturated, SEMI-AUTONOMOUS = soft glow, BLACKOUT = grey outlines drifting home.
- Brownout = the sector visually browns out: desaturation, static grain, UI elements in that region literally degrade (fonts glitch, icons drop frames).
- Jamming renders as spreading analog noise/moiré; spoofing recolors hijacked units with a corrupted-palette shimmer.
- Faction palettes: Chorus = cool white lattice, Spire = single blazing gold column, Drift = shifting FM-radio rainbow static.
- Aesthetic references: TRON's light economy, Mirror's Edge minimalism, DEFCON's map austerity, oscilloscope art.

## 4. Plot, Campaign & Tutorial

**Tutorial = campaign prologue** (one mechanic per mission, mirrors roadmap order):

1. **"Handshake"** — one front, no decay. Learn: hub → router → relay flow; keep a single skirmish supplied.
2. **"Packet Loss"** — decay enabled. Learn: buffers leak; throughput must outrun decay + demand.
3. **"Duplex"** — two out-of-phase fronts. Learn: the core juggle; set-and-forget loses.
4. **"Last Mile"** — copper → coax → fiber. Learn: upgrade timing beats raw spending.
5. **"Jam Tomorrow"** — enemy E-war debut. Learn: reading decay spikes, hardening the network.
6. **"Man in the Middle"** — you spoof an enemy sector. Learn: starvation as a weapon. Prologue ends: the enemy does it back to you, at scale — cliffhanger into Act I.

**Campaign arc sketch (3 acts):**
- **Act I — Listeners:** border skirmishes; the enemy ("The Null"?) wins battles without fighting — every loss traces to a silenced relay. Theme: learning that infrastructure *is* the war.
- **Act II — Carriers:** offensive across dead-spectrum wastelands; escort a mobile uplink convoy (Drift tech) — a campaign of moving networks, not taking ground.
- **Act III — Root:** assault the Null's core and discover it isn't a faction but a protocol — an automated pre-Silence defense system still executing its last order. Final mission: you can't out-shoot it; you must out-*route* it, then spoof the root node itself. Victory = sending one packet: `HALT`.
- Mission variety knobs: fixed-network defense (tower-defense flavored), blackout stealth (operate under total jamming with pre-programmed unit orders), asymmetric faction missions playing each race's economy.

**Skirmish flavor:** procedural "spectrum weather" — solar storms that raise global `signal_decay` for 30-tick windows, forcing both players into planned brownouts. Forecast shown in advance: strategy, not RNG punishment.
