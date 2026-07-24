# NEXT NATION — Genetic-Algorithm Bot (Mixture of Experts)

A competition bot for the **NEXT NATION** RTS game, evolved with a genetic
algorithm rather than trained with ML. The final submission is a single C++
program whose strategy parameters are **routed by map size** — a
*Mixture-of-Experts* (MoE) design where four independently-evolved parameter
sets each specialize in one band of map sizes.

**Final submission:** [`submission/107691.cpp`](submission/107691.cpp)

**How it works:** see [`docs/STRATEGY.md`](docs/STRATEGY.md) for the full strategy + genetic-algorithm writeup.

---

## The game (brief)

Two bots fight on a procedurally-generated graph of `N` regions
(`N` ∈ [51, 109]). Each side owns an HQ, trains warriors, captures neutral
strongholds to build bases (income), and tries to destroy the enemy HQ.
If neither HQ falls by turn 200, the winner is decided by **HQ hit points**
(so late-game economy → HQ upgrades is the tiebreak). Full rules:
[`docs/RULES_ko.md`](docs/RULES_ko.md). Engine + local judge:
[`engine/nation-providing/`](engine/nation-providing/).

## Approach: evolve parameters, route by map size

The bot's logic (`src/species2.cpp`) is a large hand-written strategy with
**~120 numeric parameters** exposed as `#define`s — attack timings, defense
simulation radii, opening/expansion thresholds, army-parity floors, etc. The
genetic algorithm searches parameter *values*; the code structure is fixed.

The key insight that produced the final bot: **the best parameters differ by
map size.** A small 51-region map and a large 109-region map reward almost
opposite tuning (reinforcement distances, opening tempo, how aggressively to
contest the center). So instead of one global parameter set, we evolve **four
experts**, each only ever playing — and being scored on — maps in its own band:

| expert | map sizes `N` | (`NP = (N-1)/2`) |
|--------|---------------|------------------|
| 0 — small      | 51 – 63  | 25 – 31 |
| 1 — mid-small  | 65 – 79  | 32 – 39 |
| 2 — mid-large  | 81 – 93  | 40 – 46 |
| 3 — large      | 95 – 109 | 47 – 54 |

At submission time the four parameter sets are baked into **one** program that,
after reading the map, routes to the right expert:

```c
int e = (N <= 63) ? 0 : (N <= 79) ? 1 : (N <= 93) ? 2 : 3;
```

(19 parameters are compiled into the code shape via `#if` and so must be
identical across experts — they are frozen; the other ~103 vary per expert.)

## Fitness: score vs. a fixed opponent panel — no gates, no self-play

Each candidate genome is scored by its **mean win-rate against a fixed set of
opponents** (win 1.0 / draw 0.5 / loss 0.0), sides alternating, on fresh maps
in the expert's band every generation. There is no gate/threshold logic and
past champions are *not* re-used as opponents — the target stays fixed so
scores are comparable across the whole run. The opponents
([`opponents/`](opponents/)):

| opponent | what it is |
|----------|------------|
| `85052.cpp`  | strong economy/macro bot (earlier champion) |
| `83616.cpp`  | strong economy/macro bot (earlier champion) |
| `placeholder2.cpp` | the reference **HQ timing-rush** (trains a squad, sends it straight at the HQ) |
| `attacker_a23_submit.cpp` | staged base-rush attacker |
| `g15off.cpp` | macro opponent |
| `anchor_rush2.cpp` | **rolling / continuous-pressure rush** — pipelines wave after wave; the hardest attack to defend on mid/large maps |
| `center2_attack.cpp` | **center-vector attack** — out-develops the defender through the map center |

The last two were *discovered during development* as attacks that beat earlier
champions, then added to the panel so evolution would harden against them.

## Genetic algorithm

Standard generational GA (`src/ga_moe.py`), one process per expert:

- **Population** 48, **elitism** 3, plus ~15% immigrants (mutants-of-best /
  random) each generation.
- **Selection** tournament (k=3) → uniform crossover → per-gene mutation
  (bool flip; int creep ±12% of range, or occasional full reset).
- **Racing** to fight the winner's curse: the top-4 finalists are re-evaluated
  on +200 extra fresh games and re-ranked on the pooled ~250-game score before
  a champion is chosen. (Even so, the noisiest cells — rolling-rush defense on
  mid-size maps — needed 1000-game verification to trust.)
- Every generation's champion is appended to
  [`champions/`](champions/) with its per-opponent scores.

Run on a SLURM cluster, one 32-CPU job per expert
([`src/run_moe.sbatch`](src/run_moe.sbatch), a `--array=0-3` job).

## Repository layout

```
submission/   107691.cpp          — the final MoE bot (this is what was submitted)
src/
  ga_moe.py                       — the MoE genetic-algorithm runner
  bake_moe_submit.py              — bakes 4 expert genomes → one routed .cpp
  ga_species2.py                  — parameter spec (names, ranges, defaults)
  ga_search.py                    — shared GA/eval framework
  species2.cpp                    — the tunable strategy source (~120 #define params)
  run_moe.sbatch                  — SLURM launcher (4-way array, one job per expert)
opponents/                        — the 7 fixed scoring opponents
champions/                        — per-generation champion genomes + scores (JSONL, one per expert)
engine/nation-providing/          — game engine + local testing tool (testing-tool.py)
docs/RULES_ko.md                  — official game rules (Korean)
```

## Reproduce

```bash
# 1. evolve one expert band (e.g. expert 2 = maps N 81–93)
cd src
sbatch run_moe.sbatch                 # cluster: runs all 4 experts as an array
# or a single expert locally:
python3 ga_moe.py --expert 2 --pop 48 --nseeds 50 \
        --race-top 4 --race-games 200 --workers "$(nproc)"

# 2. bake the four experts' best champions into one routed submission
python3 bake_moe_submit.py --out ../submission/my_bot.cpp
# or pin specific generations per expert:
python3 bake_moe_submit.py --gen 0:6 --gen 1:9 --gen 2:9 --gen 3:10 --out ../submission/my_bot.cpp

# 3. play a game locally (N via --NP, e.g. NP 40 → N=81)
cd ../engine/nation-providing
python3 testing-tool.py --seed 1 --NP 40 --KP 6 -l game.log \
        -a ../../submission/107691.cpp.bin -b ../../opponents/placeholder2.cpp.bin
```

(`testing-tool.py` takes compiled binaries; build with
`g++ -O2 -std=gnu++17 -o bot.bin bot.cpp`.)

## Notes & lessons

- **Map-size routing was the breakthrough.** A single all-size genome averaged
  away edges that per-band specialists could hold; the MoE bot beats the best
  single-parameter bot decisively while only reshuffling which bands win.
- **The winner's curse is real and severe.** In-run scores on high-variance
  cells (mid-map rolling-rush defense) routinely inflated by 0.05–0.15 and had
  to be confirmed by ~1000-game verification before trusting any champion.
- **Attack timing/priority tuning fixed the center-vector weakness** where
  defensive (retreat) tuning could not — the two attacks fail for different
  reasons (one economic, one a defensive breakthrough), and needed different
  levers.
- **One cell stayed hard to the end:** defending a *rolling* rush (continuous
  back-to-back waves) on mid-size maps, where reinforcement can't out-pace the
  wave cadence. It's the honest open problem of this bot.
## Result
Congratulations! We have advanced to the final round. Rank: 13/1604(Qualification Round)
