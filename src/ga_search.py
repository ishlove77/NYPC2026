#!/usr/bin/env python3
"""
Genetic-algorithm hyperparameter search for jyprush_refactored.cpp.

Search method: a real GA (population of parameter genomes, tournament
selection, uniform crossover, per-gene mutation, elitism).

Fitness of a genome  = win-rate of the bot compiled with that genome's -D
overrides, played against the fixed BASELINE build (default params), over
NSEEDS deterministic maps x 2 side assignments (= 2*NSEEDS games).
win=1.0, draw=0.5, loss=0.0.  Default genome -> 0.5 (it IS the baseline).

Everything is deterministic given (map, side), so fitness is noise-free and
caching is exact.
"""
import os, sys, json, time, random, hashlib, subprocess, argparse, glob
from concurrent.futures import ThreadPoolExecutor, as_completed

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
CPP   = "jyprush_refactored.cpp"
TOOL  = "nation-providing/testing-tool.py"
BIN   = "bin"
BASE  = os.path.join(BIN, "baseline")

# ----------------------------------------------------------------------------
# Parameter space:  name -> (kind, low, high, default)
# kind: 'int' or 'bool'.  bools are 0/1.  Movement-flag bitmasks, memory-only
# and combat-cap knobs are intentionally left fixed (not tuned).
# ----------------------------------------------------------------------------
SPEC = [
    # global switches
    ("BUILD_RESERVE",                        'int',   0,  600, 300),
    ("EXPANSION_MOVE_RESERVE",               'int',   0,  100,  10),
    ("ENABLE_ENEMY_BASE_CAPTURE",            'bool',  0,    1,   1),
    ("ENABLE_ENEMY_HQ_ATTACK",               'bool',  0,    1,   1),
    ("NORMAL_EXTRA_GARRISON",                'int',   0,    3,   1),
    # opening neutral-first
    ("ENABLE_OPENING_NEUTRAL_FIRST",         'bool',  0,    1,   1),
    ("OPENING_NEUTRAL_FIRST_MAX_TURN",       'int',  60,  140, 100),
    ("OPENING_NEUTRAL_FIRST_TARGET_PERCENT", 'int',  50,   95,  75),
    ("OPENING_NEUTRAL_FIRST_MIN_TARGETS",    'int',   4,   14,   8),
    ("OPENING_NEUTRAL_FIRST_MAX_TARGETS",    'int',  10,   24,  16),
    ("OPENING_RELAX_HQ_KEEP_UNTIL_TURN",     'int',  20,   70,  45),
    ("OPENING_DELAY_HQ_UPGRADE_UNTIL_TURN",  'int',   0,   30,  12),
    ("OPENING_DELAY_HQ_UPGRADE_MIN_HANDLED", 'int',   0,    6,   2),
    ("OPENING_FORCE_TRAIN_IF_STUCK",         'bool',  0,    1,   1),
    # all-game neutral / thin opening
    ("ENABLE_ALLGAME_NEUTRAL_ONE_BY_ONE",    'bool',  0,    1,   1),
    ("RESERVE_BASE_GOLD_FOR_NEUTRAL_CLAIM",  'bool',  0,    1,   1),
    ("OPENING_FORCE_ONE_NEUTRAL_PER_TURN_UNTIL",'int',40, 120,  80),
    ("OPENING_FORCE_TRAIN_NEUTRAL_UNTIL",    'int',  30,  100,  60),
    ("ENABLE_THIN_OPENING_GRAB",             'bool',  0,    1,   1),
    ("THIN_OPENING_MAX_TURN",                'int',  40,  120,  80),
    ("THIN_OPENING_RUSH_SAFETY_TURNS",       'int',   0,    8,   3),
    # opening abort heuristics
    ("OPENING_NEUTRAL_ABORT_ENEMY_MASS_ETA", 'int',   5,   16,  10),
    ("OPENING_NEUTRAL_ABORT_ENEMY_MASS",     'int',   3,   10,   6),
    # endgame
    ("ENDGAME_ATTACK_TURNS",                 'int',   8,   30,  16),
    ("ENDGAME_GOLD_SAFETY",                  'int',   0,  200,   0),
    ("ENDGAME_SYNC_START_TURN",              'int', 150,  200, 185),
    ("ENDGAME_SYNC_ARRIVAL_TURN",            'int', 150,  200, 195),
    # forward staging
    ("ENABLE_FORWARD_STAGING",               'bool',  0,    1,   1),
    ("FORWARD_STAGING_MAX_PER_SOURCE",       'int',   1,    6,   3),
    ("FORWARD_STAGING_EXTRA_CAP",            'int',   2,   16,   8),
    # home defense
    ("ENABLE_HOME_DEFENSE",                  'bool',  0,    1,   1),
    ("DEFENSE_SAFETY_MARGIN",                'int',   0,    5,   2),
    ("DEFENSE_IMMEDIATE_RADIUS",             'int',   1,    5,   2),
    ("DEFENSE_HARD_ETA",                     'int',   3,   12,   6),
    ("DEFENSE_RECALL_EXTRA",                 'int',   0,    8,   3),
    ("HARD_DEFENSE_SKIPS_ECONOMY_MOVES",     'bool',  0,    1,   1),
    ("DEFENSE_KEEP_EXTRA_WORKERS",           'int',   0,    5,   2),
    # stack guard / cleanup
    ("ENABLE_STACK_ONLY_GUARD",              'bool',  0,    1,   1),
    ("STACK_DANGER_COUNT",                   'int',   1,    5,   2),
    ("STACK_LOOKAHEAD",                      'int',   4,   20,  12),
    ("STACK_TARGET_DANGER_COUNT",            'int',   1,    5,   2),
    ("ENABLE_STACK_ONLY_CLEANUP",            'bool',  0,    1,   1),
    ("STACK_CLEANUP_MIN_ENEMY",              'int',   2,    6,   3),
    ("STACK_CLEANUP_MAX_TARGET_ETA",         'int',   6,   20,  12),
    ("STACK_CLEANUP_RATIO_NUM",              'int',   2,    5,   3),
    ("STACK_CLEANUP_RATIO_DEN",              'int',   1,    3,   2),
    ("STACK_CLEANUP_EXTRA_MARGIN",           'int',   0,    4,   1),
    ("STACK_CLEANUP_MIN_WAVE",               'int',   2,    6,   3),
    ("STACK_CLEANUP_MAX_SEND_EXTRA",         'int',   0,    5,   2),
    # capture / attack policy
    ("ENABLE_INITIAL_SYNC_CAPTURE",          'bool',  0,    1,   1),
    ("ENABLE_NO_DRIP_ATTACK",                'bool',  0,    1,   1),
    ("MIN_ATTACK_WAVE_UNITS",                'int',   1,    5,   2),
    ("ENABLE_STRICT_SAME_ETA_ATTACK_WAVE",   'bool',  0,    1,   1),
    # rally stack
    ("ENABLE_RALLY_STACK_ATTACK",            'bool',  0,    1,   1),
    ("RALLY_STACK_STAGE_START_TURN",         'int',  20,   80,  45),
    ("RALLY_STACK_HQ_ATTACK_START_TURN",     'int',  90,  170, 130),
    ("RALLY_STACK_MIN_LAUNCH_UNITS",         'int',   2,   10,   4),
    ("RALLY_STACK_MAX_STAGE_MOVES_PER_TURN", 'int',   6,   30,  16),
    ("RALLY_STACK_KEEP_AT_RALLY",            'int',   0,    3,   1),
    # combat sim / hq economy
    ("HQ_SAVE_LOOKAHEAD_TURNS",              'int',   1,   10,   4),
    ("BASE_DUMMY_HP_DIV",                    'int',   1,    6,   3),
    ("MAX_CAPTURE_SIM_DAYS",                 'int',  40,  120,  80),
    ("HQ_SIM_MAX_REINFORCE_DAYS",            'int',  40,  120,  80),
    # late tiebreak / cash dump
    ("LATE_TIEBREAK_START_TURN",             'int', 130,  180, 155),
    ("LATE_TIEBREAK_SAVE_LOOKAHEAD_TURNS",   'int',  20,   70,  45),
    ("CASH_DUMP_TRAIN_START_TURN",           'int',  90,  160, 115),
    ("CASH_DUMP_MIN_GOLD",                   'int', 500, 1400, 900),
    ("FINAL_HQ5_CASH_DUMP_START_TURN",       'int', 150,  195, 175),
    ("FINAL_HQ5_HARD_DUMP_START_TURN",       'int', 165,  198, 185),
    ("FINAL_HQ5_MIN_TRAIN_GOLD",             'int',  60,  240, 120),
]
NAMES   = [s[0] for s in SPEC]
LOW     = {s[0]: s[2] for s in SPEC}
HIGH    = {s[0]: s[3] for s in SPEC}
DEFAULT = {s[0]: s[4] for s in SPEC}

def gkey(g):       return tuple(g[n] for n in NAMES)
def ghash(g):      return hashlib.sha1((",".join(map(str, gkey(g)))).encode()).hexdigest()[:16]

# ----------------------------------------------------------------------------
# compile + play
# ----------------------------------------------------------------------------
def compile_genome(g):
    h = ghash(g)
    out = os.path.join(BIN, f"g_{h}")
    if os.path.exists(out):
        return out
    flags = [f"-D{n}={g[n]}" for n in NAMES]
    tmp = f"{out}.{os.getpid()}.tmp"
    cmd = ["g++", "-O2", "-std=gnu++17", *flags, "-o", tmp, CPP]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    os.replace(tmp, out)
    return out

def play_game(cand_bin, mapfile, cand_side):
    """Return candidate score for one game.  cand_side in {'L','R'}."""
    if cand_side == 'L':
        a, b, win = cand_bin, BASE, "LEFT_WIN"
    else:
        a, b, win = BASE, cand_bin, "RIGHT_WIN"
    try:
        r = subprocess.run([sys.executable, TOOL, "-i", mapfile, "-l", "/dev/null",
                            "-a", a, "-b", b],
                           capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return 0.0
    line = ""
    for ln in r.stdout.splitlines():
        if ln.startswith("RESULT"):
            line = ln
    if not line:
        return 0.0
    if win in line:   return 1.0
    if "DRAW" in line: return 0.5
    return 0.0

# ----------------------------------------------------------------------------
# population evaluation (parallel over all games of all uncached genomes)
# ----------------------------------------------------------------------------
def evaluate(pop, MAPS, pool, cache, compile_fail):
    todo = {}
    for g in pop:
        k = gkey(g)
        if k not in cache and k not in todo:
            todo[k] = g
    # compile (parallel)
    binmap = {}
    futs = {pool.submit(compile_genome, g): k for k, g in todo.items()}
    for f in as_completed(futs):
        k = futs[f]
        try:
            binmap[k] = f.result()
        except Exception as e:
            cache[k] = 0.0           # compile failure -> worst fitness
            compile_fail[0] += 1
    # play (parallel over individual games)
    tasks = []
    for k, g in todo.items():
        if k not in binmap:
            continue
        cb = binmap[k]
        for mf in MAPS:
            tasks.append((k, cb, mf, 'L'))
            tasks.append((k, cb, mf, 'R'))
    agg = {}
    futs = {pool.submit(play_game, cb, mf, sd): k for (k, cb, mf, sd) in tasks}
    for f in as_completed(futs):
        k = futs[f]
        agg.setdefault(k, []).append(f.result())
    for k, scores in agg.items():
        cache[k] = sum(scores) / len(scores)
    return [cache[gkey(g)] for g in pop]

def prune_bins():
    """Cached genomes never need their binary again, so drop candidate
    binaries after each generation to keep disk bounded (~pop files)."""
    for p in glob.glob(os.path.join(BIN, "g_*")):
        try:
            os.remove(p)
        except OSError:
            pass

# ----------------------------------------------------------------------------
# GA operators
# ----------------------------------------------------------------------------
def rand_gene(n):
    return random.randint(LOW[n], HIGH[n])

def random_genome():
    return {n: rand_gene(n) for n in NAMES}

def mutate(g, rate):
    ng = dict(g)
    for n in NAMES:
        if random.random() < rate:
            if HIGH[n] - LOW[n] == 1:                 # bool
                ng[n] = 1 - ng[n]
            elif random.random() < 0.65:              # local step
                span = max(1, int(round((HIGH[n] - LOW[n]) * 0.12)))
                ng[n] = min(HIGH[n], max(LOW[n], ng[n] + random.randint(-span, span)))
            else:                                     # resample
                ng[n] = rand_gene(n)
    return ng

def crossover(a, b):
    return {n: (a[n] if random.random() < 0.5 else b[n]) for n in NAMES}

def tournament(pop, fit, k=3):
    best = None
    for _ in range(k):
        i = random.randrange(len(pop))
        if best is None or fit[i] > fit[best]:
            best = i
    return pop[best]

def diff_from_default(g):
    return {n: g[n] for n in NAMES if g[n] != DEFAULT[n]}

# ----------------------------------------------------------------------------
# checkpoint (for long, resumable runs)
# ----------------------------------------------------------------------------
def save_ckpt(path, pop, cache, best_g, best_f, history, gen):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump({
            "gen": gen,
            "best_f": best_f,
            "best_g": best_g,
            "history": history,
            "pop": pop,
            "cache": {",".join(map(str, k)): v for k, v in cache.items()},
            "rng": list(random.getstate()),
        }, f)
    os.replace(tmp, path)

def load_ckpt(path):
    d = json.load(open(path))
    cache = {tuple(int(x) for x in k.split(",")): v for k, v in d["cache"].items()}
    rs = d["rng"]
    random.setstate((rs[0], tuple(rs[1]), rs[2]))
    return d["pop"], cache, d["best_g"], d["best_f"], d["history"], d["gen"]

# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pop", type=int, default=64)
    ap.add_argument("--gens", type=int, default=100000)    # bounded by --max-min
    ap.add_argument("--nseeds", type=int, default=50)      # games/genome = 2*nseeds
    ap.add_argument("--workers", type=int,
                    default=int(os.environ.get("SLURM_CPUS_PER_TASK", os.cpu_count())))
    ap.add_argument("--elite", type=int, default=3)
    ap.add_argument("--mut", type=float, default=0.12)
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--max-min", type=float, default=1400.0, help="wall-time budget (minutes)")
    ap.add_argument("--out", default="champion.json")
    ap.add_argument("--ckpt", default="pop_checkpoint.json")
    ap.add_argument("--resume", action="store_true")
    args = ap.parse_args()

    random.seed(args.seed)
    MAPS = [os.path.join("maps", f"m{i:03d}.txt") for i in range(args.nseeds)]
    MAPS = [m for m in MAPS if os.path.exists(m)]
    print(f"# workers={args.workers} pop={args.pop} gens={args.gens} "
          f"maps={len(MAPS)} games/genome={2*len(MAPS)}", flush=True)

    pool = ThreadPoolExecutor(max_workers=args.workers)
    cache = {}
    compile_fail = [0]
    t0 = time.time()
    start_gen = 0

    if args.resume and os.path.exists(args.ckpt):
        pop, cache, best_g, best_f, history, last_gen = load_ckpt(args.ckpt)
        start_gen = last_gen + 1
        print(f"# resumed from {args.ckpt}: gen={last_gen} champ={best_f:.3f} "
              f"cache={len(cache)}", flush=True)
    else:
        # initial population: default is a strong attractor, so bias toward
        # local mutants of the default at varied strengths (fine-tune) + some
        # randoms (exploration / diversity).
        pop = [dict(DEFAULT)]
        n_local = int(args.pop * 0.65)
        for i in range(n_local):
            rate = random.choice([0.05, 0.08, 0.12, 0.18, 0.25])
            pop.append(mutate(DEFAULT, rate))
        while len(pop) < args.pop:
            pop.append(random_genome())
        best_g, best_f, history = dict(DEFAULT), -1.0, []

    for gen in range(start_gen, args.gens):
        fit = evaluate(pop, MAPS, pool, cache, compile_fail)
        prune_bins()
        order = sorted(range(len(pop)), key=lambda i: fit[i], reverse=True)
        if fit[order[0]] > best_f:
            best_f = fit[order[0]]
            best_g = dict(pop[order[0]])
        mean = sum(fit) / len(fit)
        el = time.time() - t0
        print(f"[gen {gen:02d}] best={fit[order[0]]:.3f} mean={mean:.3f} "
              f"champ={best_f:.3f} cache={len(cache)} cfail={compile_fail[0]} "
              f"t={el/60:.1f}m", flush=True)
        history.append({"gen": gen, "best": fit[order[0]], "mean": mean, "champ": best_f,
                        "t_min": round(el/60, 1)})
        # --- save best periodically (every generation) ---
        with open(args.out, "w") as f:
            json.dump({"fitness": best_f, "n_games": 2*len(MAPS), "gen": gen,
                       "diff_from_default": diff_from_default(best_g),
                       "genome": best_g, "history": history,
                       "build_cmd": "g++ -O2 -std=gnu++17 " +
                          " ".join(f"-D{n}={best_g[n]}" for n in sorted(diff_from_default(best_g)))
                          + " -o jyprush jyprush_refactored.cpp"},
                      f, indent=2)
        # --- full checkpoint (population + cache + rng) for resume ---
        save_ckpt(args.ckpt, pop, cache, best_g, best_f, history, gen)

        if gen == args.gens - 1 or el > args.max_min * 60:
            print(f"# stopping (gen={gen}, elapsed={el/60:.1f}m)", flush=True)
            break

        # next generation: elitism + champion-refining immigrants + offspring.
        # immigrants keep a long run exploring instead of converging early.
        newpop = [dict(pop[order[i]]) for i in range(args.elite)]
        n_imm = max(1, int(args.pop * 0.15))
        for _ in range(n_imm):
            if random.random() < 0.75:
                newpop.append(mutate(best_g, random.choice([0.05, 0.10, 0.15, 0.20])))
            else:
                newpop.append(random_genome())
        while len(newpop) < args.pop:
            p1 = tournament(pop, fit); p2 = tournament(pop, fit)
            child = mutate(crossover(p1, p2), args.mut)
            newpop.append(child)
        pop = newpop

    pool.shutdown(wait=False)
    print(f"# DONE champion fitness={best_f:.3f}", flush=True)
    print("# diff_from_default:", json.dumps(diff_from_default(best_g)), flush=True)

if __name__ == "__main__":
    main()
