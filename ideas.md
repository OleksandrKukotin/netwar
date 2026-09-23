# NETWAR — Idea Notebook

Unfiltered brainstorm space. Nothing here is committed design; promote ideas into the README/GDD when they survive playtesting.

## 1. Lore

**The Silence.** A century after the orbital relays burned, no signal crosses the planet without wire, dish, or drone. Warfare didn't end — it became *routing*. Armies are cheap; the ability to command them is not. Every faction fights for the same scarce resource: coherent bandwidth across a poisoned electromagnetic spectrum.

- Commanders are physically remote — bunkered "conductors" whose authority literally travels as packets. Losing your relay doesn't kill your soldiers; it orphans them. (This is why spoofing/hijacking makes narrative sense: orphaned units obey whoever speaks their protocol.)
- The map's neutral relays are pre-Silence infrastructure ruins — capturing them is archaeology as much as conquest.
- Fog of war is diegetic: it's not "unseen", it's *unheard* — sectors where you have no signal presence.
- The fiction now carries the interface too: since the player never touches a soldier (GDD 1B), the in-world reason is simply that a conductor *cannot*. You are a hundred kilometres away in a bunker. You do not see the battle; you see your own telemetry. Everything on your screen is something your network reported back.

## 2. Races / Factions — asymmetry through the economy graph

Each faction is a different bend of the same Machinations graph. Balance lever: same total power, different topology.

Now that the base is the played object (GDD 2), a faction is really defined by three answers: *where is authority generated*, *how does it travel*, and *what happens to a structure that loses it*. The third one is the least explored and probably the richest.

### The Chorus (mesh decentralists)
- **Graph twist:** no Command Hub. Instead, every Relay is a weak generator (`base_signal_gen` 3/tick each, but they *stack*). No `hub_bandwidth_cap` — but also no big buffer to ride out spikes.
- **Unique node — Mesh Link:** relays share signal with neighbor relays at 2/tick, so the network self-heals around a destroyed node.
- **Autonomy flavor:** their structures degrade *gracefully and slowly* — everything is built to run half-deaf. Lowest ceiling in DIRECTED, highest floor in BLACKOUT.
- **Playstyle:** expansion is literally power; map control = generation. Weak early, unkillable late.
- **Weakness:** jamming one relay leaks into neighbors (decay propagates across mesh links). Cutting a single line hurts them least — which also means raiding, their opponent's best tool, is blunted against them. Watch this for balance.

### The Spire (centralized maximalists)
- **Graph twist:** one colossal hub — `base_signal_gen` 20/tick, `hub_bandwidth_cap` 250 — but router allocation shrinks with distance from the Spire (signal attenuation: −1 throughput per map ring).
- **Unique node — Uplink Pylon:** deployable repeater that resets the attenuation counter; expensive, visible from across the map (permanent signal beacon in fog).
- **Autonomy flavor:** brutal cliff. Spire structures are built to be directed and are nearly useless without it — a blacked-out Spire hub is a very expensive wall.
- **Playstyle:** overwhelming force projection near home, logistics chess far away.
- **Weakness:** the Death Ball faction by temptation — the game's anti-clumping math punishes their instincts. Also the faction most punished by line-cutting, which makes them the natural tutorial antagonist for teaching raids.

### The Drift (analog broadcasters)
- **Graph twist:** no lines at all — wireless broadcast. Throughput isn't capped by connection but falls off with distance (inverse square, tabulated for determinism). `signal_decay` is doubled (0.10) everywhere: analog is lossy.
- **Unique node — Carrier Wave:** a mobile unit that *is* a relay. Their whole network can walk.
- **Autonomy flavor:** they live in SEMI-AUTONOMOUS by default and are balanced around it — for the Drift, brownout is home.
- **Playstyle:** nomadic, unjammable in the conventional sense (nothing to cut), thrives on raids.
- **Weakness:** signal tracing lights them up — broadcasting is shouting. Low ceiling on sustained front-line intensity.
- **Design note:** having no lines means the entire "the line is the target" layer (GDD 7B) does not apply to them. That is either their defining strength or a hole that breaks the game's central attack vector. Needs testing before it is promoted anywhere.

### The Parasite (fourth faction / expansion?)
- **Graph twist:** minimal own generation; unique nodes tap *enemy* connections, siphoning a percentage of whatever flows past.
- Probably too gimmicky for launch — revisit after E-war (M5) exists.

## 3. Uncommitted mechanics

Things that would change the core loop. None are in the GDD yet; each needs a reason to exist beyond elegance.

### Propagation delay — commands take time to arrive

Today routing is instantaneous: signal leaves the hub and reaches the relay inside the same tick. But the fiction says orders are packets travelling down a wire, and the name of the game is NETWAR. The most thematically honest mechanic available is the one currently missing.

If a packet takes N ticks to reach its relay — N scaling with line length and inversely with line quality — then the player **cannot react at all**. They can only anticipate. You commit bandwidth to where demand will be five ticks from now; guess wrong and a front sits dark while your authority travels to the wrong place.

- It converts the core skill from reaction speed into forecast quality, which is exactly the anti-APM thesis stated positively rather than as a prohibition.
- It makes line upgrades deeper: copper → fiber would buy not just volume but a **shorter prediction horizon**, so the upgrade decision stops being "bigger number good."
- It gives map geometry a second meaning — a long cable is slow as well as exposed.
- **Risk:** it may simply feel bad. Latency between intent and effect is famously unpleasant, and there is a real chance this reads as unresponsiveness rather than as strategy. Prototype it behind a flag in M2 and be prepared to throw it away.

### Doctrine as the unit interface

If armies exist but are never directly controlled (GDD 1B), the player still needs *some* verb for them. Candidate: each combat hub carries a **doctrine** — a small set of standing orders (hold / probe / press / withdraw-on-contact) that its fielded forces execute. Signal tier decides how fast a doctrine change actually propagates and how intelligently it is interpreted.

This keeps the promise (no micro) while giving the player something to *do* with their armies beyond building them. It also means an army's behavior is an attribute of the network, not of the units — which is the whole thesis.

Open: is doctrine set per hub, per sector, or globally? Per hub is most expressive and most clicky; global is cleanest and possibly too coarse.

### Signal as a positive stealth choice

Already half-committed in GDD 3A, but worth pushing further: if DIRECTED sectors are traceable, then a deliberately dark flank is a *feint*. Going quiet becomes an offensive tool, not just an economy measure — you blind yourself in a sector to hide that anything is there. Possibly the most interesting unexplored space in the whole design.

## 4. Tempo & the M2 playtest

The GDD now fixes a target band of 0.5–1 s per tick (section 6), but that number was reasoned, not felt. The terminal client exists to settle it.

**Protocol.** Build tick duration as a live runtime control — keys that change it mid-run. Then play the same scenario at 0.3 / 0.5 / 1 / 2 s and record, honestly:

1. Where does it stop being a flicker and start being a decision?
2. Where does it start being a wait?
3. **Is there a meaningful decision at least every 10–15 s, with no dead stretch over ~30 s?**

If question 3 fails at *every* tick rate, the problem is not speed — it is that nothing in the scenario changes. Reach for a source of change (GDD 6A) rather than a different clock: vein depletion and spectrum weather are the two cheapest to fake in a terminal prototype.

**Spectrum weather is a tempo generator, not flavor.** Procedural solar storms that raise global `signal_decay` for 30-tick windows, forecast in advance, force both players into planned brownouts. Framed as strategy, not RNG punishment — and structurally, it is the metronome that keeps a quiet match from going static.

## 5. Graphics & Visual Direction

**Terminal era (M2):** don't fight the medium — lean in. Phosphor-CRT command console: amber/green monochrome, scanline flicker on brownouts, relay buffers as VU meters, intensity waves as an oscilloscope strip. The FTXUI dashboard *is* the fiction: you're a conductor at a console.

**Graphical client (M5+):** the map is a dark circuit board / night-time network topology. Ideas:
- Bandwidth is the only bright thing. Signal travels as visible light pulses along lines; line quality = pulse density (copper: sparse dots → fiber: solid stream). You read your economy at a glance, no numbers needed.
- Structures are dim shapes that *brighten with control tier*: DIRECTED = sharp and saturated, SEMI-AUTONOMOUS = soft glow, BLACKOUT = grey outline still stubbornly working.
- A cut line is the money shot: the pulses stop, and you watch the light drain out of everything downstream, one hub at a time.
- Brownout = the sector visually browns out: desaturation, static grain, UI elements in that region literally degrade (fonts glitch, icons drop frames).
- Jamming renders as spreading analog noise/moiré; spoofing recolors hijacked structures with a corrupted-palette shimmer.
- Faction palettes: Chorus = cool white lattice, Spire = single blazing gold column, Drift = shifting FM-radio rainbow static.
- Aesthetic references: TRON's light economy, Mirror's Edge minimalism, DEFCON's map austerity, oscilloscope art.
- Since the player is a remote conductor (section 1), there is a case for never rendering a "real" battlefield at all — only telemetry. Unclear whether that is brilliant or alienating.

## 6. Plot, Campaign & Tutorial

**Tutorial = campaign prologue** (one mechanic per mission, mirrors roadmap order):

1. **"Handshake"** — one front, no decay. Learn: hub → router → relay flow; keep a single skirmish supplied.
2. **"Packet Loss"** — decay enabled. Learn: buffers leak; throughput must outrun demand.
3. **"Duplex"** — two out-of-phase fronts. Learn: the core juggle; set-and-forget loses.
4. **"Brownout"** — a mission you *cannot* fully supply. Learn: choosing which sector goes autonomous, and discovering that a dark sector is also an unseen one. This is the mission that teaches the game's actual thesis.
5. **"Last Mile"** — copper → coax → fiber. Learn: upgrade timing beats raw spending.
6. **"Jam Tomorrow"** — enemy E-war debut. Learn: reading decay spikes, hardening the network.
7. **"Man in the Middle"** — you spoof an enemy sector. Learn: starvation as a weapon. Prologue ends: the enemy does it back to you, at scale — cliffhanger into Act I.

Note that no mission ever teaches unit control, because there isn't any. The difficulty curve is entirely about network shape and attention — which means the tutorial has an unusual job: teaching players to *stop reaching* for the verbs every other RTS gave them. Mission 1 might deliberately let them try.

**Campaign arc sketch (3 acts):**
- **Act I — Listeners:** border skirmishes; the enemy ("The Null"?) wins battles without fighting — every loss traces to a silenced relay. Theme: learning that infrastructure *is* the war. Mechanically this is the campaign teaching GDD 7B — you are being raided on the wire and cannot see it.
- **Act II — Carriers:** offensive across dead-spectrum wastelands; escort a mobile uplink convoy (Drift tech) — a campaign of moving networks, not taking ground.
- **Act III — Root:** assault the Null's core and discover it isn't a faction but a protocol — an automated pre-Silence defense system still executing its last order. Final mission: you can't out-shoot it; you must out-*route* it, then spoof the root node itself. Victory = sending one packet: `HALT`.
- Mission variety knobs: fixed-network defense (tower-defense flavored), blackout stealth (a whole mission spent in BLACKOUT, where you set doctrine in advance and then watch, powerless, as it plays out), asymmetric faction missions playing each race's economy.

The Null works especially well against the new autonomy rules: an enemy that is *pure baseline behavior*, infinitely patient, never adapting, and impossible to out-think because there is nothing in there thinking. The horror is that it never browns out — it has no intelligence left to lose.
