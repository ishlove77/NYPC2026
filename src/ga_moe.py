#!/usr/bin/env python3
"""
MoE (Mixture of Experts) GA for species2.cpp (= 89644 base), user 2026-07-06.

- 4 independent pools ("experts"), split by MAP SIZE. Each expert's games are
  played ONLY on maps of its own size band (via the simulator's --NP):
    expert 0 small     N =  51..63   (NP 25..31)
    expert 1 mid-small N =  65..79   (NP 32..39)
    expert 2 mid-large N =  81..93   (NP 40..46)
    expert 3 large     N =  95..109  (NP 47..54)
- NO gates, NO champion opponents. Fitness = MEAN score vs 5 FIXED opponents:
    85052, 83616, placeholder2, attacker_a23_submit (v33), g15off
  (win 1.0 / draw 0.5 / loss 0.0, sides alternate, fresh maps per generation).
- The generation champion is only STORED (submissions/genomes_moe{K}.jsonl);
  it is never added to the scoring set.
- 19 params used in preprocessor #if directives are FROZEN at the 89644
  source values so all experts share one code shape (required for the single
  MoE submission binary that routes params by N at runtime).
"""
import os, sys, json, time, random, subprocess, argparse, glob, threading, re
from concurrent.futures import ThreadPoolExecutor, as_completed
import ga_species2 as S     # SPEC/NAMES/LOW/HIGH/SRC_DEFAULT, mutate machinery

TOOL = "nation-providing/testing-tool.py"

EXPERT_BANDS = {   # expert -> (NP_min, NP_max); N = 2*NP+1
    0: (25, 31),   # N 51..63
    1: (32, 39),   # N 65..79
    2: (40, 46),   # N 81..93
    3: (47, 54),   # N 95..109
}

OPPONENTS = [      # FIXED scoring set; 5 standard + 2 new attackers (user 2026-07-08)
    {"label": "m85052",  "src": "../85052.cpp"},
    {"label": "m83616",  "src": "../83616.cpp"},
    {"label": "P2",      "src": "placeholder2.cpp"},
    {"label": "a23sub",  "src": "../attacker_a23_submit.cpp"},
    {"label": "g15off",  "src": "species2b_gen15_submit.cpp"},
    # rolling / bases-until-clear continuous-pressure rush (beats 97281 ~0.44)
    {"label": "rush2",   "src": "../anchor_rush2.cpp",  "weight": 1.0},
    # center-vector attack — WEIGHT 1 (user 2026-07-08: all anchors uniform)
    {"label": "center2", "src": "../center2_attack.cpp", "weight": 1.0},
]
for _o in OPPONENTS:
    _o.setdefault("weight", 1.0)
WSUM = sum(_o["weight"] for _o in OPPONENTS)

# gene set computed DYNAMICALLY from the species file (robust to code changes
# that add/remove params, e.g. 100655 dropped ENABLE_ATTACK_PREDICT).
# PRESENT = params with a #define in the species; FROZEN = those used in #if
# directives (compile-time, uniform across experts); FREE = the rest (evolved).
_TXT = S._SRC_TEXT
PRESENT = [n for n in S.NAMES
           if re.search(rf"^[ \t]*#define[ \t]+{re.escape(n)}[ \t]", _TXT, re.M)]
FROZEN = set()
for _line in _TXT.splitlines():
    _ls = _line.strip()
    if _ls.startswith(('#if ', '#elif ', '#if(')):
        for n in PRESENT:
            if re.search(rf"\b{re.escape(n)}\b", _ls): FROZEN.add(n)
FREE = [n for n in PRESENT if n not in FROZEN]

def rand_gene(n): return random.randint(S.LOW[n], S.HIGH[n])

def random_genome():
    g = dict(S.SRC_DEFAULT)
    for n in FREE: g[n] = rand_gene(n)
    return g

def mutate(g, rate):
    ng = dict(g)
    for n in FREE:
        if random.random() < rate:
            if S.HIGH[n] - S.LOW[n] == 1:
                ng[n] = 1 - ng[n]
            elif random.random() < 0.65:
                span = max(1, int(round((S.HIGH[n] - S.LOW[n]) * 0.12)))
                ng[n] = min(S.HIGH[n], max(S.LOW[n], ng[n] + random.randint(-span, span)))
            else:
                ng[n] = rand_gene(n)
    return ng

def crossover(a, b):
    g = {n: (a[n] if random.random() < 0.5 else b[n]) for n in S.NAMES}
    for n in FROZEN: g[n] = S.SRC_DEFAULT[n]
    return g

def tournament(pop, fit, k=3):
    best = None
    for _ in range(k):
        i = random.randrange(len(pop))
        if best is None or fit[i] > fit[best]:
            best = i
    return pop[best]

class Ctx:
    def __init__(self, expert):
        self.expert = expert
        self.bindir = f"bin/moe{expert}"
        os.makedirs(self.bindir, exist_ok=True)
        self.npmin, self.npmax = EXPERT_BANDS[expert]

    def opp_bin(self, o): return os.path.join(self.bindir, o['label'])

    def compile_opponents(self):
        for o in OPPONENTS:
            out = self.opp_bin(o)
            if os.path.exists(out):
                continue
            src = os.path.join(os.path.dirname(__file__) if os.path.dirname(__file__) else ".", o['src']) if not o['src'].startswith('/') else o['src']
            if os.path.exists(o['src']):
                subprocess.run(["g++", "-O2", "-std=gnu++17", "-o", out, o['src']],
                               check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            else:
                # source missing: fall back to a prebuilt binary (e.g. a23sub whose
                # .cpp was removed; bin/<label>_prebuilt is the frozen build)
                pre = os.path.join("bin", o['label'] + "_prebuilt")
                if os.path.exists(pre):
                    import shutil as _sh; _sh.copy2(pre, out)
                else:
                    raise FileNotFoundError(f"anchor {o['label']}: no src {o['src']} and no prebuilt {pre}")

    def compile_genome(self, g):
        h = S.ghash(g)
        out = os.path.join(self.bindir, f"g_{h}")
        if os.path.exists(out): return out
        text = S._SRC_TEXT
        for n in S.NAMES:
            text = S._PAT[n].sub(rf"\g<1>{g[n]}", text)
        tid = threading.get_ident()
        src, tmp = f"{out}.{tid}.cpp", f"{out}.{tid}.tmp"
        open(src, "w").write(text)
        try:
            subprocess.run(["g++", "-O2", "-std=gnu++17", "-o", tmp, src],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            os.replace(tmp, out)
        finally:
            try: os.remove(src)
            except OSError: pass
        return out

    def np_kp_for_seed(self, seed):
        np_ = self.npmin + (seed % (self.npmax - self.npmin + 1))
        # tool constraint: K = 2*KP+1 must be odd in [ceil(0.15N), floor(0.2N)]
        N = 2 * np_ + 1
        K_lo = (3 * N + 19) // 20
        K_hi = N // 5
        K_lo += (K_lo % 2 == 0)
        K_hi -= (K_hi % 2 == 0)
        kp_lo, kp_hi = (K_lo - 1) // 2, (K_hi - 1) // 2
        kp = kp_lo + ((seed >> 8) % (kp_hi - kp_lo + 1))
        return np_, kp

    def play(self, cand_bin, opp_bin, seed, side):
        np_, kp_ = self.np_kp_for_seed(seed)
        if side == 'L': a, b, win = cand_bin, opp_bin, "LEFT_WIN"
        else:           a, b, win = opp_bin, cand_bin, "RIGHT_WIN"
        try:
            r = subprocess.run([sys.executable, TOOL, "--seed", str(seed), "--NP", str(np_),
                                "--KP", str(kp_), "-l", "/dev/null", "-a", a, "-b", b],
                               capture_output=True, text=True, timeout=90)
        except subprocess.TimeoutExpired:
            return None
        line = ""
        for ln in r.stdout.splitlines():
            if ln.startswith("RESULT"): line = ln
        if not line:       return None
        if win in line:    return 1.0
        if "DRAW" in line: return 0.5
        return 0.0

    def prune(self):
        for p in glob.glob(os.path.join(self.bindir, "g_*")):
            try: os.remove(p)
            except OSError: pass

def gen_games(gen, n, base):
    return [(base + gen * n + i, 'L' if i % 2 == 0 else 'R') for i in range(n)]

def evaluate(ctx, pop, games, pool, gcache, compile_fail):
    """gcache[(gkey, label)] = mean score vs that opponent on `games`."""
    todo = {}
    for g in pop:
        k = S.gkey(g)
        if any((k, o['label']) not in gcache for o in OPPONENTS):
            todo[k] = g
    binmap = {}
    futs = {pool.submit(ctx.compile_genome, g): k for k, g in todo.items()}
    for f in as_completed(futs):
        k = futs[f]
        try: binmap[k] = f.result()
        except Exception:
            for o in OPPONENTS: gcache[(k, o['label'])] = 0.0
            compile_fail[0] += 1
    tasks = {}
    for k, b in binmap.items():
        for o in OPPONENTS:
            if (k, o['label']) in gcache: continue
            for (seed, sd) in games:
                tasks[pool.submit(ctx.play, b, ctx.opp_bin(o), seed, sd)] = (k, o['label'])
    agg = {}
    for f in as_completed(tasks):
        v = f.result()
        if v is not None:
            agg.setdefault(tasks[f], []).append(v)
    for key, sc in agg.items():
        gcache[key] = (sum(sc) / len(sc)) if sc else 0.5
    return [sum(gcache[(S.gkey(g), o['label'])] * o['weight'] for o in OPPONENTS) / WSUM
            for g in pop]

def save_ckpt(path, pop, best, history, gen):
    tmp = path + ".tmp"
    json.dump({"gen": gen, "best": best, "history": history, "pop": pop,
               "rng": list(random.getstate())}, open(tmp, "w"))
    os.replace(tmp, path)

def load_ckpt(path):
    d = json.load(open(path))
    rs = d["rng"]; random.setstate((rs[0], tuple(rs[1]), rs[2]))
    return d["pop"], d["best"], d["history"], d["gen"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--expert", type=int, required=True, choices=[0, 1, 2, 3])
    ap.add_argument("--pop", type=int, default=48)
    ap.add_argument("--gens", type=int, default=100000)
    ap.add_argument("--nseeds", type=int, default=50)
    ap.add_argument("--seed-base", type=int, default=1_000_000)
    ap.add_argument("--workers", type=int,
                    default=int(os.environ.get("SLURM_CPUS_PER_TASK", os.cpu_count())))
    ap.add_argument("--elite", type=int, default=3)
    ap.add_argument("--mut", type=float, default=0.03)
    ap.add_argument("--race-top", type=int, default=8)
    ap.add_argument("--race-games", type=int, default=100)
    ap.add_argument("--seed", type=int, default=99)
    ap.add_argument("--max-min", type=float, default=1400.0)
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--warm-genome", default="", help="JSON file with {'genome':...} to seed the pool")
    args = ap.parse_args()

    K = args.expert
    random.seed(args.seed + 1000 * K)
    ctx = Ctx(K)
    ctx.compile_opponents()
    ckpt = f"moe{K}_checkpoint.json"
    champ_file = f"submissions/genomes_moe{K}.jsonl"
    os.makedirs("submissions", exist_ok=True)
    pool = ThreadPoolExecutor(max_workers=args.workers)
    compile_fail = [0]; t0 = time.time(); start_gen = 0

    if args.resume and os.path.exists(ckpt):
        pop, best, history, last = load_ckpt(ckpt)
        start_gen = last + 1
        print(f"# [moe{K}] resumed gen={last}", flush=True)
    else:
        seedg = dict(S.SRC_DEFAULT)
        if args.warm_genome and os.path.exists(args.warm_genome):
            wg = json.load(open(args.warm_genome))["genome"]
            seedg = {n: wg.get(n, S.SRC_DEFAULT[n]) for n in S.NAMES}
            for n in FROZEN: seedg[n] = S.SRC_DEFAULT[n]
            print(f"# [moe{K}] pool seeded from {args.warm_genome}", flush=True)
        pop = [dict(seedg)] + [mutate(seedg, random.choice([0.05, 0.08, 0.12, 0.18, 0.25]))
                               for _ in range(args.pop - 1)]
        best = dict(seedg); history = []

    nmin, nmax = 2 * ctx.npmin + 1, 2 * ctx.npmax + 1
    print(f"# MoE expert {K}: maps N={nmin}..{nmax} pop={args.pop} free-params={len(FREE)} "
          f"(frozen {len(FROZEN)}) games/gen={args.nseeds}+race{args.race_games} "
          f"opponents={[o['label'] for o in OPPONENTS]} (FIXED, no gates, no champ scoring)", flush=True)

    for gen in range(start_gen, args.gens):
        games = gen_games(gen, args.nseeds, args.seed_base + 10_000_000 * K)
        gcache = {}
        fit = evaluate(ctx, pop, games, pool, gcache, compile_fail)
        if args.race_top > 0:
            pre = sorted(range(len(pop)), key=lambda i: fit[i], reverse=True)[:min(args.race_top, len(pop))]
            finalists = [pop[i] for i in pre]
            games_x = gen_games(gen, args.race_games, args.seed_base + 10_000_000 * K + 50_000_000)
            gc2 = {}
            evaluate(ctx, finalists, games_x, pool, gc2, compile_fail)
            n1, n2 = len(games), len(games_x)
            for g in finalists:
                k = S.gkey(g)
                for o in OPPONENTS:
                    key = (k, o['label'])
                    if key in gc2 and key in gcache:
                        gcache[key] = (n1 * gcache[key] + n2 * gc2[key]) / (n1 + n2)
            refined = [sum(gcache[(S.gkey(g), o['label'])] * o['weight'] for o in OPPONENTS) / WSUM
                       for g in finalists]
            for idx, i in enumerate(pre):
                fit[i] = refined[idx]
        ctx.prune()
        el = time.time() - t0
        order = sorted(range(len(pop)), key=lambda i: fit[i], reverse=True)
        bg = pop[order[0]]; best = dict(bg); af = fit[order[0]]
        scores = {o['label']: gcache.get((S.gkey(bg), o['label']), float('nan')) for o in OPPONENTS}
        sstr = " ".join(f"{l}={v:.3f}" for l, v in scores.items())
        mean = sum(fit) / len(fit)
        print(f"[moe{K} gen {gen:02d}] fit={af:.3f} {sstr} mean={mean:.3f} "
              f"cfail={compile_fail[0]} t={el/60:.1f}m", flush=True)
        history.append({"gen": gen, "fit": round(af, 3),
                        "scores": {l: round(v, 3) for l, v in scores.items()},
                        "mean": round(mean, 3), "t_min": round(el/60, 1)})
        # champion: STORED ONLY (never scored against)
        with open(champ_file, "a") as fh:
            fh.write(json.dumps({"expert": K, "gen": gen, "fit": round(af, 4),
                                 "scores": {l: round(v, 4) for l, v in scores.items()},
                                 "genome": bg}) + "\n")
        save_ckpt(ckpt, pop, best, history, gen)
        if gen == args.gens - 1 or el > args.max_min * 60:
            print(f"# [moe{K}] stopping (gen={gen}, {el/60:.1f}m)", flush=True); break

        newpop = [dict(pop[order[i]]) for i in range(args.elite)]
        for _ in range(max(1, int(args.pop * 0.15))):
            if random.random() < 0.75:
                newpop.append(mutate(best, random.choice([0.05, 0.10, 0.15, 0.20])))
            else:
                newpop.append(random_genome())
        while len(newpop) < args.pop:
            newpop.append(mutate(crossover(tournament(pop, fit), tournament(pop, fit)), args.mut))
        pop = newpop

    pool.shutdown(wait=False)

if __name__ == "__main__":
    main()
