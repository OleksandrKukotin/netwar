# NETWAR — Playtest Log

Findings from playing the M2 terminal console (`./build/cli/netwar`), and the proposals they led to. This is the working buffer between sessions: review the **Open proposals** before the next coding session, decide, then move each item to *Done* or *Rejected*.

The match ruleset (`sim/include/netwar/match.hpp`) is provisional, so the changes proposed here are expected.

---

## Playtest #1 — 2026-09-23 (owner, 4 matches)

**Build:** commit `086cc1c`, default rules, mostly 1 s/tick.

**Result:** 0 wins out of 4. The best run lasted about 400 ticks. West and East each collapsed twice.

**Verdict on the core bet: positive.** "It was definitely not boring. My brain really engaged with the goal of winning, watched what was happening, and tracked patterns." This is the first human signal that bandwidth allocation can carry a game (issue #6, playtest checkpoint).

### What happened, match by match

1. Didn't know what to do and collapsed almost at once.
2. Lasted longer, but only after discovering that `u` (upgrade line) helps.
3. and 4. Played more or less deliberately. Noticed `p` (priority) but never understood what it does.

The deaths seemed to come **during spectrum storms, with nothing left in the buffer.**

### Diagnosis

**D1 — The first match gives no guidance.** The briefing explains the *rules* but not the *first action*. `u` is the single most important early verb, and it was found by accident in match 2. The key bar and the `[u] ->` hint in each front panel are not enough.

**D2 — `p` gives no visible feedback.** Priority only matters when the hub cannot feed both routers. Hub net income is 9/t (12 − 3 upkeep). The lines carry 2/t on copper and 8/t on coax, so before any fiber the hub buffer sits full and vents heat, and `p` does literally nothing. It starts to matter once one line is fiber (8 + 4 = 12 > 9). Runs of ~400 ticks almost certainly reached that point, so the failure is also **legibility**. Nothing on screen shows that a router is starved, or how much each line actually delivered this tick. The player cannot see the effect of the lever, so the lever cannot be learned.

**D3 — Storms are punishment, not preparation.** The storm forecast tells the player to "fill the relays", but there is nothing to do. Routers are already at maximum by default, and a relay is limited by its line. During a storm, decay goes from 5% to 15%. A coax-fed relay then settles around 4 / 0.15 ≈ 27 units *without any combat*. Put a flare on top (base amplitude 12 at tick 400 due to escalation, jittered up to ~15) and it browns out. There is no counter-play, which contradicts GDD 6A: storms should be "planned brownouts to prepare for, not random punishment".

**D4 — Difficulty.** The juggling bot wins ~35/40, but it acts every tick with exact numbers. A human needs a margin. D1–D3 probably explain most of the gap, so fix those before retuning numbers.

## Playtest #2 — 2026-09-29 (a friend, first time)

**Build:** v0.2.0-alpha.

**Result:** "He didn't understand anything." His one concrete request was to make the interface bigger, so he could see what the Up and Down arrows change.

**Diagnosis.** This was more than a display problem. At the start of a match, ↑↓ had **no effect at all**. A front receives `min(router, line, hub)`. Routers start at their 8/t maximum, so ↑ is clamped, and the copper line carries 1/t, so ↓ changes nothing until the router drops below 1. The only feedback was a small `■■■■□□□□` row that changed while the relay kept behaving the same. This is D2 again, now affecting ↑↓ as well as `p`.

**Response (UX, no rules change):**
- The router bar is twice as wide, and each unit is colour-coded: cyan if the line can carry it, dim red if the router asks for more than the line can carry.
- A new **SUPPLY** row in each front shows how much reaches the relay per tick and which link limits it: router, line, a full relay, or **HUB SHORT** (with the `[p]` hint when the front lacks priority).
- Every ↑↓ press writes a log line, for example "WEST router 6 -> 5/t: supply stays 1/t, the COPPER line is the limit." Held keys collapse into one line per tick.
- The briefing gained one line explaining SUPPLY = the smallest of router, line and hub.
- **The scope now shows the future.** Right of the bar, the LEVEL row continues 20 ticks ahead (dimmed), showing where the relay will be if nothing changes. A new TIER strip (`█` directed, `▓` semi, `▒` blackout) makes the tiers readable at a glance, and a one-line verdict sits under it (`⚠ BLACKOUT in 7t`, `✖ FRONT FALLS in 9t`, `▲ DIRECTED in 4t`, `✔ holds`). The console computes this by running a copy of `Match` forward after every tick and every command. The copy sees nothing extra: the wave period equals the 20-tick intel window, and storms are announced 20 ticks ahead. Because every key press redraws the future, each verb shows its effect immediately.

## Playtest #3 — 2026-10-02 (Vlad, professional software tester)

**Build:** not stated. Either the v0.2.0-alpha release or `main` at `3fed100` (Playtest #2 response, pushed 2026-09-30). Ask.

**Source:** issue [#7](https://github.com/OleksandrKukotin/netwar/issues/7), written as a restaurant-inspector review (the "Ревізор" TV show). The attached image is a torn ace of clubs, a joke rather than a game screenshot.

**Report, verbatim:** "Я не виграв. Звуків нема. Румуни бігають. Їсти не дають. 1/т. — ревізор не рекомендує." ("I didn't win. No sound. Romanians are running around. They don't feed you. 1/t. The inspector does not recommend.")

**Reading it:**
- **No win.** Three testers, zero human wins. This is consistent with D1–D4.
- **No sound.** The console is silent. The owner replied with a promise to add sound and make the game clearer.
- **"They don't feed you. 1/t."** Most likely the copper line: a front starts on 1/t of supply, and that is all it gets until the player buys coax. This is the same wall as Playtest #2. The opening feels like starvation, and the way out (`u`) is not obvious. This is D1 again.
- **"Romanians are running around."** Unclear. It could be a joke, or it could be something on screen that moves without explanation (the scrolling scope, the log, the forecast row). Ask.

**Follow-up:** Vlad tests software for a living. The next round should get a structured report: build or commit, terminal and OS, number of matches, how each one ended, and the first moment he did not know what to do.

---

## Open proposals

### UX: no design decision needed

- [ ] **Contextual hints** in the log/status line, driven by state. Examples: "30 matter: press `u` to lay coax on the selected front", "EAST flare in 5t and its relay is at 6: feed it". Show them heavily in the first match and back off once the player has used each verb.
- [x] **Show the actual flow per line** in each front panel: requested vs delivered this tick (e.g. `ROUTER 8/t -> delivered 3.0/t`).
- [x] **A "HUB SHORT" indicator** when routers pull more than the buffer holds, naming who got served first. This is the moment `p` matters, so make it loud.
- [ ] **Post-match debrief:** a per-flare timeline of each front's tier and hold delta, heat vented while the buffer was full, time spent in each tier, and the storm windows. The playtester was already reading patterns; the debrief should feed that.
- [ ] **Remove the "Fill the relays" storm advice** until there is something the player can actually do (see D3).
- [ ] **Audio cues** (Playtest #3; promised in #7). Start with the terminal bell on brownout, a front falling, and a storm arriving. The owner decides later whether real audio belongs in the console or waits for the graphical client. Keep it in `cli/`: no sound code in `sim/`.

### Design: owner decides

- [ ] **Make allocation matter from minute one.** Today the juggle is switched off until the first fiber. Options:
  - (a) lines start as coax and fiber is the first upgrade, so hub contention arrives with the first purchase;
  - (b) a narrower hub early on (e.g. growing generation);
  - (c) keep the numbers and lean on a rule that already exists: routers keep pulling into a full relay (cap 40), and the surplus is vented as heat. That makes throttling a quiet front a real decision once the hub runs short. Today it is invisible, so show that waste per front on screen. On its own this is probably too weak, because it does nothing while the hub is in surplus.
- [ ] **Regional storms with counter-play.** Instead of a global storm, forecast *which* front it will hit. The player pre-shifts routers and priority toward that front, or deliberately lets it brown out (GDD 3A: brownout is a tool). This turns weather into another allocation decision and creates the moments where `p` matters.
- [ ] **Difficulty presets** (`MatchRules` profiles such as recruit / standard / veteran), re-verified with the bots. Do this after the UX fixes and the two design items, not before.

---

## Done

*(nothing yet)*

## Rejected

*(nothing yet)*
