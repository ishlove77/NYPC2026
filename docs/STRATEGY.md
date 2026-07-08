# Strategy & Logic

How the bot actually plays, and how the genetic algorithm produced it. Two
layers: (1) the **in-game strategy** hand-written in `src/species2.cpp` with
~120 tunable knobs, and (2) the **Mixture-of-Experts genetic algorithm** that
searched those knobs — evolving a separate parameter set per map-size band and
baking all four into the single routed submission `submission/107691.cpp`.

---

## 1. The game in one paragraph

Two players fight on a random graph of `N` regions (`N ∈ [51, 109]`). Edge cost
= ⌈Euclidean distance⌉ between region centers; a warrior follows the
minimum-cost path one region per turn (ties → lower region number, and a moving
warrior **cannot be redirected** until it arrives — so movement is fully
deterministic and *predictable*). Each side starts with an HQ and 3 warriors,
500 gold. Each turn you may **train** (120g, capped by HQ level), **move**
(10g/warrior, free onto your own building), and **upgrade/build** (base 300g;
HQ upgrades 600→3600g). Income = `15 × min(#stationed warriors, work_cap)` per
owned building; upkeep = `2 × #warriors`. A building under enemy occupation is
*sieged* and loses HP. If neither HQ is destroyed by turn 200, **the winner is
the higher HQ hit-points** — so late-game gold → HQ upgrades is the tiebreak.

Two consequences shape everything:
- **Deterministic movement ⇒ attacks are predictable.** If ≥4 enemy warriors
  move as a group, you can compute their possible targets and eliminate
  candidates each turn until only one base/HQ remains.
- **HQ-HP tiebreak ⇒ economy is a weapon.** Most games between strong bots never
  see an HQ fall; they're decided by who converted more territory into HQ
  upgrades by turn 200.

## 2. The bot's strategy (`species2.cpp`)

`decide()` runs each turn as a prioritized pipeline. Each stage is governed by
tunable parameters (the GA's search space); the *structure* is fixed.

### 2.1 Opening & expansion
- **Thin opening / neutral-first.** Send single warriors from the HQ to capture
  the nearest neutral strongholds (home-side outward), build a base on arrival,
  garrison one worker for income (`OPENING_NEUTRAL_FIRST_*`, `THIN_OPENING_*`,
  `RESERVE_BASE_GOLD_FOR_NEUTRAL_CLAIM`).
- **Build-ready-on-arrival dispatch.** A claim party is sent to a stronghold
  only if the base can actually be paid for when it arrives — this kills deep,
  idle claims that never build.
- **Garrison invariant.** Every owned building keeps ≥1 stationary worker (for
  income); the opening never sends a building's last worker away.

### 2.2 Economy & the parity floor
- **Work income** scales with garrisoned warriors up to each building's
  `work_cap`; HQ upgrades (L1→L5) raise HP, turret, `train_cap`, `work_cap`.
- **Army parity (hard rule).** Never field fewer warriors than the opponent:
  train to match `theirs + margin` before economy gold is spent, and **freeze
  expansion while outnumbered** (`ENABLE_ARMY_PARITY_TRAIN`,
  `ARMY_PARITY_MARGIN`). Exception — **force-training-rush detection**
  (`PARITY_MAX_OPP_CAP_RATIO`): if the enemy's army vastly exceeds its work
  capacity it's running on upkeep debt; copying it bankrupts you, so parity
  stands down and the exact-sim defense handles the rush instead.

### 2.3 Defense — exact simulation, not heuristics
The heart of the bot. When enemy warriors threaten an owned base:
1. **Build an attack schedule.** For each enemy warrior compute an ETA to the
   base — *committed* (already moving at it), *staged* (sitting within a threat
   radius → assume it launches now, pessimistically), or *predicted* (the
   ≥4-mover group-attack path prediction). `BASE_SIM_DEFENSE_THREAT_RADIUS`,
   `OWNED_BASE_CRISIS_*`.
2. **Simulate the fight day-by-day** — turret fire first, then arrivals by ETA,
   combat resolution — to see if the base holds with its current garrison.
3. **Minimal rescue.** If not, pull the *fewest* reinforcements that make the
   re-simulation hold (candidates ranked by arrival time), **including the HQ
   army as a donor** (`HQ_DONOR_KEEP`, `OWNED_BASE_MAX_EMERGENCY_PULL`,
   `OWNED_BASE_RESCUE_EXTRA` for a margin against unseen waves).
4. **Rescue-trains.** The schedule can include *hypothetical* HQ trainees, so
   the sim knows a base can be saved by training + walking a fresh warrior.
5. **Unsavable-base fallback.** If no rescue holds in time, don't feed the base
   — evacuate it and hold the next building toward the HQ
   (`ENABLE_UNSAVABLE_BASE_FALLBACK`, `FALLBACK_*`).
6. **HQ exact-sim defense.** The same simulation guards the HQ with its own
   radius / recall / train-cap knobs (`HQ_SIM_DEFENSE_*`).

Design principle: *use the simulator inside the bot to compute exactly how many
warriors are needed — no approximations.*

### 2.4 Offense — anchor-route & rally-stack
- **Anchor-route attacks.** Rather than dribble warriors one at a time, gather
  them at one owned base (an "anchor") and strike from there in a stack
  (`ANCHOR_ROUTE_MIN_ATTACKERS` ≥3, `ANCHOR_ROUTE_MAX_STACK`,
  `ANCHOR_ROUTE_START_*`, capture-then-return/build behavior).
- **Rally-stack HQ attack.** Stage a stack at a rally point, launch at the enemy
  HQ once big enough (`RALLY_STACK_*`, `MIN_ATTACK_WAVE_UNITS`,
  `ENABLE_STRICT_SAME_ETA_ATTACK_WAVE` so waves land together, not piecemeal).
- **HQ-anchor attack priority** — the change that made `107691` the strongest
  bot. Biasing offensive routing toward the enemy HQ anchor was what finally
  improved defense against the *center-vector* attack, because that attack
  beats you *economically*: more decisive attacking (an offensive lever), not
  more defending, was the right fix.

### 2.5 Endgame — win the turn-200 tiebreak
Because most games end on HQ HP, the late game converts surplus into HQ
survivability: **cash-dump training** past a gold threshold, **final-HQ5 dumps**,
and a **late-tiebreak** mode that spends the last turns maximizing/protecting HQ
HP (`CASH_DUMP_*`, `FINAL_HQ5_*`, `LATE_TIEBREAK_*`, `ENDGAME_SYNC_*`).

### 2.6 Retreat (a double-edged knob)
`ENABLE_RETREAT_LOSING_FIGHTS` pulls warriors out of fights the sim says they'll
lose. Powerful, but **coupled to timing-rush defense**: tune retreat too
aggressively (the `no_friendly_base_retreat` experiments) and the army stays
forward while a plain HQ timing-rush walks in — so it must stay balanced.

## 3. The Mixture-of-Experts genetic algorithm

### 3.1 Why map size?
The single biggest lever: **the best parameters depend on map size.** On a
51-region board reinforcement paths are 2–3 hops and aggression pays; on a
109-region board paths are long, the center is contested, and the same
aggressive tuning gets punished. A single global parameter set *averages away*
edges a size-specialist could hold. So we evolve four experts, each seeing only
its own band, and route between them at runtime:

| expert | map size `N` | `NP = (N−1)/2` |
|--------|--------------|----------------|
| 0 small      | 51 – 63  | 25 – 31 |
| 1 mid-small  | 65 – 79  | 32 – 39 |
| 2 mid-large  | 81 – 93  | 40 – 46 |
| 3 large      | 95 – 109 | 47 – 54 |

### 3.2 Evolved vs. frozen genes
Of the ~120 `#define` parameters, **19 appear in `#if` directives** — they
compile whole code blocks in or out, so they can't be runtime variables. Those
are **frozen** (identical across all experts). The other **~103 are
runtime-safe** and evolve independently per expert. `ga_moe.py` discovers this
split automatically by scanning the species file (`PRESENT` / `FROZEN` / `FREE`),
so it survives code edits that add or remove parameters.

### 3.3 Fitness — a fixed opponent panel
Each genome's fitness is the **mean win-rate against 7 fixed opponents** (win
1.0 / draw 0.5 / loss 0.0), sides alternating, on fresh maps *in the expert's
band* every generation. Deliberately:
- **No gates / thresholds** — a scalar mean, so scores are comparable across the
  whole run.
- **No self-play / champion pool** — the target never moves, keeping the fitness
  signal stationary (earlier champion-pool experiments drifted as the pool
  changed).

The panel (`opponents/`): two strong **economy champions** (`85052`, `83616`),
the reference **HQ timing-rush** (`placeholder2`), a **staged base-rush**
(`attacker_a23_submit`), a **macro** bot (`g15off`), and — added mid-project
after we *discovered* them beating earlier champions — the **rolling /
continuous-pressure rush** (`anchor_rush2`) and the **center-vector attack**
(`center2_attack`).

### 3.4 GA mechanics (`ga_moe.py`)
- Population 48; 3 elites; ~15% immigrants each generation (75% mutant-of-best at
  rate 0.05–0.20, 25% random).
- Tournament selection (k=3) → uniform crossover (frozen genes forced to source
  value) → per-gene mutation (bool flip; int creep ±12% of range, else full
  reset) at ~0.03/gene.
- **Racing against the winner's curse.** A 50-game screen is noisy (SE ≈ 0.07),
  so the top finalists are re-fought on +200 fresh games and re-ranked on the
  pooled ~250-game score before a champion is chosen. *Even then*, the noisiest
  cell — rolling-rush defense on mid-size maps — needed **1000-game
  verification**; in-run scores there inflated by 0.05–0.15, and a
  4-generation streak still evaporated on verification.
- Every generation's champion is appended to `champions/expertK_*.jsonl` with
  its per-opponent scores. One 32-CPU SLURM job per expert (`run_moe.sbatch`,
  a `--array=0-3`).

### 3.5 Baking the submission (`bake_moe_submit.py`)
Takes the four experts' chosen genomes and emits one `.cpp`:
- Each runtime `#define NAME v` becomes `static int P_NAME = …; #define NAME
  P_NAME` (variable + alias, in place so `#ifndef` guards survive), plus a
  generated `moe_apply_expert_params(N)` that loads all of them from a 4-column
  table.
- The router — one line — is called in `main()` right after the map is parsed:
  ```c
  int e = (N <= 63) ? 0 : (N <= 79) ? 1 : (N <= 93) ? 2 : 3;
  ```
- Frozen params must agree across experts (majority + warning otherwise).
- Verified end-to-end: baking all four experts at the source defaults reproduces
  the base bot's behavior *exactly* (identical-genome mirrors draw 100% of
  games), so the transform changes only which numbers load per map — nothing
  else.

## 4. What worked, and the honest open problem

- **Map-size routing was the breakthrough.** The MoE bot beats the strongest
  single-parameter bot decisively while only reshuffling *which bands* win —
  proof the size specialization is real, not noise.
- **Different weaknesses need different levers.** The center-vector attack beats
  you *economically* and was fixed by **attack/priority** tuning; the timing
  rush is a *defensive* problem fixed by retreat/HQ-sim tuning. Editing the
  wrong subsystem (e.g. retreat code for an economy loss) never helped.
- **The winner's curse is severe and must be verified away.** Any champion
  chosen on in-run scores of a high-variance cell must be re-checked at ~1000
  games before it's believed; several "improvements" were sampling luck.
- **Open problem:** defending a *rolling* rush (continuous back-to-back waves)
  on mid-size maps (bands 1–2), where reinforcement cadence can't out-pace the
  waves. Every approach — hand-editing and the GA over many generations — left
  that one cell (~0.45–0.55 win-rate) below the rest. It is the honest ceiling
  of this bot and the natural place for future work (likely a new *mechanism*
  for pipelined-wave defense, not more parameter tuning).
