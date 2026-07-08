#!/usr/bin/env python3
"""
Single-species co-evolution for species2.cpp (species1 dropped).

- One pool (species2), source `species2.cpp`, now with 77 tunable params
  (the 70 documented + 7 new HP_RATIO_TRAIN_PRIORITY_* knobs).
- Archive = a PERMANENT fixed anchor `placeholder.cpp` (never evicted, present
  every round) + species2 champions (added over time, capped).
- Champions start empty (cleared); the anchor is the only initial opponent.
- Fitness = mean win-rate vs the whole archive (anchor + champions).
- Fresh maps every game via the simulator's --seed (never repeating; same
  seeds within a generation for fair selection).
- Warm-starts the population from a prior pool (the two-species checkpoint's
  species2 pool), extending each genome with defaults for the 7 new params.
"""
import os, sys, json, time, random, shutil, subprocess, argparse, hashlib, re, threading
from concurrent.futures import ThreadPoolExecutor, as_completed
import ga_search as G   # base SPEC, TOOL/BIN paths, prune_bins; chdir's to ga_work

SRC     = "species2.cpp"
# Permanent fixed anchor(s), present every round, never evicted. Each has its OWN gate.
ANCHORS = [
    {"hash": "anchor2", "label": "placeholder2", "genome": None, "bin": "bin/placeholder2", "src": "placeholder2.cpp", "gate": 0.6},
    # SECOND GATE = our strongest bot (OFF-lineage gen15: beat 260703_15 0.815,
    # 52235 0.927). The anchor-route pool must hold >=0.5 vs it.
    {"hash": "anchor_g15off", "label": "g15off", "genome": None, "bin": "bin/opp_g15off", "src": "species2b_gen15_submit.cpp", "gate": 0.5},
    # RUSH GATES = the v32 ANCHOR-RUSH attacker starting its rush at 2, 3 and
    # 4 bases respectively (user 2026-07-05): candidates must hold >= 0.6
    # against early, mid and late rush timings.
    {"hash": "anchor_a23", "label": "a23rB2", "genome": None, "bin": "bin/opp_a23", "src": "attacker_a23_v32.cpp", "gate": 0.7},
    # champion anchors (user 2026-07-06): break the shield-vs-economy frontier -
    # champions must now BEAT both live leaderboard bots, not just survive attackers
    {"hash": "anchor_85052", "label": "m85052", "genome": None, "bin": "bin/opp85052", "src": "../85052.cpp", "gate": 0.6},
    {"hash": "anchor_83616", "label": "m83616", "genome": None, "bin": "bin/opp83616", "src": "../83616.cpp", "gate": 0.6},
]
# External opponents: count toward champScore (like champions), NO gate, never
# evicted. genome=None but flagged ext_champ so evaluate() treats them as champs.
EXT_CHAMPS = []
def fresh_archive():
    return [dict(a) for a in ANCHORS] + [dict(e) for e in EXT_CHAMPS]

# Log-replay defense scenarios (deterministic, exact maps + scripted attackers
# reconstructed from leaderboard losses 1-66/1-67). A genome only PASSES the
# gates if it is NOT HQ-destroyed as the RIGHT player in every scenario.
# score: 1.0 win, 0.5 draw, 0.25 loss on points, 0.0 loss by HQ destruction.
# scripted-replay scenarios removed (user 2026-07-05): the a23rush gate covers
# early-rush survival; gates only.
SCENARIOS = []
ARCH    = os.path.join(G.BIN, "arch3")

# 7 new tunable params in species2.cpp (defaults chosen behaviourally-identical
# to the source: MAX_PER_TURN>=3 == unlimited since HQ train-cap<=3).
NEW = [
    ("ENABLE_HP_RATIO_TRAIN_PRIORITY",          'bool', 0,   1,   1),
    ("HP_RATIO_TRAIN_PRIORITY_KEEP_SOLVENT",    'bool', 0,   1,   1),
    ("HP_RATIO_TRAIN_PRIORITY_X_NUM",           'int',  1,   9,   3),
    ("HP_RATIO_TRAIN_PRIORITY_X_DEN",           'int',  1,   9,   5),
    ("HP_RATIO_TRAIN_PRIORITY_MIN_ENEMY_HP",    'int',  1,  12,   1),
    ("HP_RATIO_TRAIN_PRIORITY_MAX_TURN",        'int', 50, 200, 200),
    ("HP_RATIO_TRAIN_PRIORITY_MAX_PER_TURN",    'int',  1,   4,   4),
]
# 5 new "owned base emergency defense" params (added in the latest species2.cpp)
NEW2 = [
    ("ENABLE_OWNED_BASE_EMERGENCY_DEFENSE", 'bool', 0,  1,  1),
    ("OWNED_BASE_CRISIS_RELEASE_RADIUS",     'int', 1,  6,  2),
    ("OWNED_BASE_CRISIS_SIM_DAYS",           'int', 5, 40, 18),
    ("OWNED_BASE_MAX_EMERGENCY_PULL",        'int', 1, 20, 10),
    ("OWNED_BASE_REINFORCE_SOURCE_RADIUS",   'int', 2, 14,  7),
]
# 4 new "retreat losing fights" params (each its own #ifndef block in species2.cpp)
NEW4 = [
    ("ENABLE_RETREAT_LOSING_FIGHTS",   'bool', 0,   1,   1),
    ("ENABLE_RETREAT_FOLLOWUP_BLOCK",  'bool', 0,   1,   0),
    ("RETREAT_SIM_DAYS",                'int', 10, 200,  80),
    ("RETREAT_MAX_UNITS",               'int',  1, 256, 256),
]
# 8 new "anchor route attacks" params (each its own #ifndef block): attackers
# gather at one owned BASE anchor first, then attack from there.
NEW6 = [
    ("ENABLE_ANCHOR_ROUTE_ATTACKS",       'bool', 1,   1,  1),   # FIXED ON (user)
    ("ANCHOR_ROUTE_START_TURN",            'int', 0, 200,  0),
    ("ANCHOR_ROUTE_ANCHOR_MODE",           'int', 0,   3,  0),
    ("ANCHOR_ROUTE_MAX_STAGE_PER_TURN",    'int', 1,  30,  8),
    ("ANCHOR_ROUTE_KEEP_EXTRA_AT_SOURCE",  'int', 0,  20,  0),
    ("ANCHOR_ROUTE_MAX_STACK",             'int', 1,  64, 18),
    ("ANCHOR_ROUTE_LEAVE_AT_ANCHOR",       'int', 0,  10,  1),
    ("ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR", 'bool', 0,   1,  1),
]
# 11 more anchor-route params (2026-07-03 code update). Each its own guard.
# ANCHOR_ROUTE_MIN_ATTACKERS: user constraint — must stay >= 3.
NEW7 = [
    ("ANCHOR_ROUTE_MIN_ATTACKERS",              'int', 3,  30,  4),
    ("ANCHOR_ROUTE_MIN_OWNED_BASES_TO_ATTACK",  'int', 0,  15,  0),
    ("ANCHOR_ROUTE_START_BASES_A_NUM",          'int', 0,   9,  0),
    ("ANCHOR_ROUTE_START_BASES_A_DEN",          'int', 1,   9,  1),
    ("ANCHOR_ROUTE_START_BASES_B",              'int', 0,  10,  0),
    ("ANCHOR_ROUTE_LEAVE_ON_CAPTURE",           'int', 0,  10,  1),
    ("ANCHOR_ROUTE_BUILD_CAPTURED_FIRST",      'bool', 0,   1,  1),
    ("ANCHOR_ROUTE_RETURN_AFTER_CAPTURE",      'bool', 0,   1,  1),
    ("ANCHOR_ROUTE_STAGE_WITHOUT_READY_TARGET",'bool', 0,   1,  1),
    ("ANCHOR_ROUTE_STICKY_ANCHOR",             'bool', 0,   1,  1),
    ("ANCHOR_ROUTE_STRICT_OFFENSE_ONLY",       'bool', 0,   1,  1),
]
# 5 exact-sim HQ defense params (2026-07-04, added by assistant per user request:
# "use simulator in code to find efficient warriors to defend", no approximation)
NEW8 = [
    ("ENABLE_HQ_EXACT_SIM_DEFENSE", 'bool', 1, 1, 1),  # PINNED ON (user 2026-07-05)
    ("HQ_SIM_DEFENSE_THREAT_RADIUS",    'int', 1,  8,  2),
    ("HQ_SIM_DEFENSE_DAYS",             'int', 8, 40, 24),
    ("HQ_SIM_DEFENSE_MAX_RECALL",       'int', 2, 24, 12),
    ("HQ_SIM_DEFENSE_TRAIN_CAP",        'int', 0,  3,  3),
    ("BASE_SIM_DEFENSE_THREAT_RADIUS",  'int', 0,  4,  1),
]
# 3 unsavable-base fallback params (2026-07-04: evacuate doomed bases, hold the
# next base toward HQ; fixes leaderboard losses 1-66/1-67)
NEW10 = [
    ("ENABLE_UNSAVABLE_BASE_FALLBACK", 'bool', 0,  1, 1),
    ("FALLBACK_MAX_PULL",               'int', 2, 16, 8),
    ("FALLBACK_TRAIN_CAP",              'int', 0,  3, 2),
]
# NEW12: HQ-as-defense-donor (user 2026-07-05: rescue sims count warriors at
# other bases INCLUDING the HQ army; keep-floor tunable)
NEW12 = [
    ("HQ_DONOR_KEEP", 'int', 1, 6, 2),
]
# NEW13: 1-68 defense doctrine (user 2026-07-05): army parity floor, rescue
# trains at HQ, deterministic attack-path prediction
NEW13 = [
    ("ENABLE_ARMY_PARITY_TRAIN",  'bool', 1, 1, 1),   # PINNED ON (user 2026-07-05)
    ("ARMY_PARITY_MARGIN",         'int', -2, 4, 0),
    ("ARMY_PARITY_EXPANSION_SLACK",'int', 0, 0, 0),   # PINNED 0 (user: no expansion while outnumbered)
    ("PARITY_MAX_OPP_CAP_RATIO",   'int', 1, 5, 2),
    ("OWNED_BASE_RESCUE_EXTRA",    'int', 0, 3, 1),
    # ("STAGE_ALERT_RADIUS",       'int', 1, 3, 2),   # not in 82804 base
    # ("STAGE_MIN_GROUP",          'int', 2, 6, 3),   # not in 82804 base
    # ("RECALL_MIN_TURN",          'int', 10, 60, 25), # not in 82804 base
    ("ENABLE_ATTACK_PREDICT",     'bool', 1, 1, 1),   # PINNED ON (user 2026-07-05)
    ("PREDICT_MIN_GROUP",          'int', 3, 6, 4),
    ("ENABLE_BASE_EMERGENCY_TRAIN",'bool', 0, 1, 1),
]
SPEC    = list(G.SPEC) + NEW + NEW2 + NEW4 + NEW6 + NEW7 + NEW8 + NEW10 + NEW12 + NEW13
NAMES   = [s[0] for s in SPEC]
LOW     = {s[0]: s[2] for s in SPEC}
HIGH    = {s[0]: s[3] for s in SPEC}
DEFAULT = {s[0]: s[4] for s in SPEC}

# species2.cpp ships TUNED #define defaults (differ from stock for most params).
# diff/baking MUST compare against the SOURCE defaults, not the stock ones, or a
# baked bot silently reverts changed params to species2.cpp's tuned defaults.
def _parse_src_defaults(path):
    txt = open(path).read(); d = {}
    for m in re.finditer(r'^#ifndef[ \t]+(\w+)\s*\n#define[ \t]+(\w+)[ \t]+(.+)$', txt, re.M):
        if m.group(1) == m.group(2):
            v = m.group(3).strip()
            try: d[m.group(1)] = int(v)
            except ValueError: pass          # expression default (e.g. MAX_TURN): skip
    return d
_SRC_RAW = _parse_src_defaults(SRC)
SRC_DEFAULT = {n: _SRC_RAW.get(n, DEFAULT[n]) for n in NAMES}
SRC_DEFAULT['ENABLE_ANCHOR_ROUTE_ATTACKS'] = 1   # FIXED ON (user) — source ships 0
# the bot EXACTLY as the user wrote it (source #define values) — used to seed the
# population and as a permanent champScore opponent that anchors generality.
SRCG = dict(SRC_DEFAULT)

def rand_gene(n):  return random.randint(LOW[n], HIGH[n])
def random_genome(): return {n: rand_gene(n) for n in NAMES}
def gkey(g):  return tuple(g[n] for n in NAMES)
def ghash(g): return hashlib.sha1((",".join(map(str, gkey(g)))).encode()).hexdigest()[:16]
def diff_default(g): return {n: g[n] for n in NAMES if g[n] != SRC_DEFAULT[n]}

def mutate(g, rate):
    ng = dict(g)
    for n in NAMES:
        if random.random() < rate:
            if HIGH[n] - LOW[n] == 1:
                ng[n] = 1 - ng[n]
            elif random.random() < 0.65:
                span = max(1, int(round((HIGH[n] - LOW[n]) * 0.12)))
                ng[n] = min(HIGH[n], max(LOW[n], ng[n] + random.randint(-span, span)))
            else:
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

# Compile by EDITING the #define values (not -D), so shared #ifndef blocks (one
# guard covering several #defines) stay intact — -D on a guard would skip the
# whole block and leave sibling macros undefined.
_SRC_TEXT = open(SRC).read()
_PAT = {n: re.compile(rf"^([ \t]*#define[ \t]+{re.escape(n)}[ \t]+).*$", re.M) for n in NAMES}

def compile_genome(g):
    h = ghash(g); out = os.path.join(G.BIN, f"g_{h}")
    if os.path.exists(out):
        return out
    text = _SRC_TEXT
    for n in NAMES:
        text = _PAT[n].sub(rf"\g<1>{g[n]}", text)
    tid = threading.get_ident()
    src = f"{out}.{tid}.cpp"; tmp = f"{out}.{tid}.tmp"
    open(src, "w").write(text)
    try:
        subprocess.run(["g++", "-O2", "-std=gnu++17", "-o", tmp, src],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        os.replace(tmp, out)
    finally:
        try: os.remove(src)
        except OSError: pass
    return out

def play_vs(cand_bin, opp_bin, seed, side):
    if side == 'L': a, b, win = cand_bin, opp_bin, "LEFT_WIN"
    else:           a, b, win = opp_bin, cand_bin, "RIGHT_WIN"
    try:
        r = subprocess.run([sys.executable, G.TOOL, "--seed", str(seed), "-l", "/dev/null",
                            "-a", a, "-b", b], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return None
    line = ""
    for ln in r.stdout.splitlines():
        if ln.startswith("RESULT"):
            line = ln
    if not line:      return None
    if win in line:   return 1.0
    if "DRAW" in line: return 0.5
    return 0.0                        # any loss = 0 (user 2026-07-05)

def play_scenario(cand_bin, scn):
    try:
        r = subprocess.run([sys.executable, G.TOOL, "-i", scn["map"], "-l", "/dev/null",
                            "-a", scn["attacker"], "-b", cand_bin],
                           capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        return 0.0
    line = ""
    for ln in r.stdout.splitlines():
        if ln.startswith("RESULT"):
            line = ln
    if "RIGHT_WIN" in line: return 1.0
    if "DRAW" in line:      return 0.5
    if "HQ" in line:        return 0.0     # HQ destroyed: defense failed
    return 0.25                             # lost on points but survived

def gen_games(gen, n, base):
    return [(base + gen * n + i, 'L' if i % 2 == 0 else 'R') for i in range(n)]

def evaluate(pop, archive, games, pool, gcache, compile_fail, mode='mean', gate=0.7):
    todo = {}; needed = []
    for g in pop:
        k = gkey(g)
        for opp in archive:
            if (k, opp['hash']) not in gcache:
                todo[k] = g; needed.append((k, opp))
    binmap = {}
    futs = {pool.submit(compile_genome, g): k for k, g in todo.items()}
    for f in as_completed(futs):
        k = futs[f]
        try: binmap[k] = f.result()
        except Exception:
            for opp in archive: gcache[(k, opp['hash'])] = 0.0
            compile_fail[0] += 1
    tasks = []
    for (k, opp) in needed:
        if k not in binmap: continue
        for (seed, sd) in games:
            tasks.append((k, opp['hash'], binmap[k], opp['bin'], seed, sd))
    agg = {}
    futs = {pool.submit(play_vs, cb, ob, seed, sd): (k, oh)
            for (k, oh, cb, ob, seed, sd) in tasks}
    # defense scenarios: one deterministic game per genome per scenario
    scn_futs = {}
    for g in pop:
        k = gkey(g)
        if k not in binmap:
            # compiled earlier or failed; try existing bin path
            b = os.path.join(G.BIN, f"g_{ghash(g)}")
            if not os.path.exists(b): continue
            binmap.setdefault(k, b)
        for scn in SCENARIOS:
            if (k, scn['hash']) not in gcache and (k, scn['hash']) not in scn_futs.values():
                scn_futs[pool.submit(play_scenario, binmap[k], scn)] = (k, scn['hash'])
    for f in as_completed(futs):
        v = f.result()
        if v is not None:
            agg.setdefault(futs[f], []).append(v)
    for key, sc in agg.items():
        gcache[key] = (sum(sc) / len(sc)) if sc else 0.5
    for f in as_completed(scn_futs):
        gcache[scn_futs[f]] = f.result()
    if mode == 'gated':
        # survive only if EVERY anchor is beaten >= that anchor's OWN gate; then rank by
        # champion-storage score. survivors -> 1.0 + champ_mean (in [1,2]). non-survivors ->
        # mean fraction-of-gate achieved (in [0,1)) so they rank below survivors but climb
        # toward passing each gate.
        anchors = [o for o in archive if o['genome'] is None and not o.get('ext_champ')]
        champs  = [o for o in archive if o['genome'] is not None or o.get('ext_champ')]
        res = []
        for g in pop:
            k = gkey(g)
            passed = all(gcache[(k, a['hash'])] >= a.get('gate', gate) for a in anchors)
            scn_ok = all(gcache.get((k, s['hash']), 0.0) > 0.0 for s in SCENARIOS)
            passed = passed and scn_ok
            if passed:
                cs = [gcache[(k, c['hash'])] for c in champs]
                cmean = (sum(cs) / len(cs)) if cs else \
                        (sum(gcache[(k, a['hash'])] for a in anchors) / len(anchors))
                res.append(1.0 + cmean)
            else:
                prog = [min(gcache[(k, a['hash'])] / max(a.get('gate', gate), 1e-9), 1.0) for a in anchors]
                prog += [1.0 if gcache.get((k, s['hash']), 0.0) > 0.0 else 0.0 for s in SCENARIOS]
                res.append(sum(prog) / len(prog))
        return res
    if mode == 'min':   # worst-case over the archive -> forces beating EVERY opponent
        return [min(gcache[(gkey(g), opp['hash'])] for opp in archive) for g in pop]
    return [sum(gcache[(gkey(g), opp['hash'])] for opp in archive) / len(archive) for g in pop]

def add_champion(g, archive, champ_cap, label):
    h = ghash(g)
    if any(o['hash'] == h for o in archive):
        return False
    dst = os.path.join(ARCH, h)
    shutil.copy2(compile_genome(g), dst)
    archive.append({"hash": h, "label": label, "genome": g, "bin": dst})
    champs = [i for i, o in enumerate(archive)
              if o['genome'] is not None and not o.get('perm')]
    while len(champs) > champ_cap:   # anchors (genome=None) + perm entries never evicted
        drop = archive.pop(champs[0])
        try: os.remove(drop['bin'])
        except OSError: pass
        champs = [i for i, o in enumerate(archive)
                  if o['genome'] is not None and not o.get('perm')]
    return True

def ensure_perm_default(archive):
    """Permanent champScore opponent = the source-default bot (never evicted, no gate).
    Anchors generality: champions must keep beating the bot as the user wrote it."""
    h = ghash(SRCG)
    if any(o['hash'] == h for o in archive):
        for o in archive:
            if o['hash'] == h: o['perm'] = True
        return
    dst = os.path.join(ARCH, h)
    shutil.copy2(compile_genome(SRCG), dst)
    archive.append({"hash": h, "label": "default", "genome": SRCG, "bin": dst, "perm": True})

def save_ckpt(path, pop, archive, best, history, gen, promoted):
    tmp = path + ".tmp"
    json.dump({"gen": gen, "best": best, "history": history, "pop": pop,
               "archive": archive, "promoted": promoted,
               "rng": list(random.getstate())}, open(tmp, "w"))
    os.replace(tmp, path)

def load_ckpt(path):
    d = json.load(open(path))
    archive = d["archive"]
    for o in archive:
        if not os.path.exists(o['bin']):
            if o['genome'] is not None:
                shutil.copy2(compile_genome(o['genome']), o['bin'])
    rs = d["rng"]; random.setstate((rs[0], tuple(rs[1]), rs[2]))
    return d["pop"], archive, d["best"], d["history"], d["gen"], d.get("promoted", False)

def warm_pop(path, popn):
    """Seed from a prior pool (two-species checkpoint's species2 pool),
    extending each genome with defaults for params it lacks (the new ones)."""
    d = json.load(open(path))
    src = d.get("pop2") or d.get("pop") or []
    seen = set(); pop = []
    for g in ([d.get("best", {}).get("2")] if d.get("best") else []) + src:
        if not g: continue
        gg = {n: g.get(n, DEFAULT[n]) for n in NAMES}
        k = gkey(gg)
        if k in seen: continue
        seen.add(k); pop.append(gg)
        if len(pop) >= popn: break
    while len(pop) < popn:
        pop.append(mutate(DEFAULT, random.choice([0.05, 0.10, 0.15])))
    return pop

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pop", type=int, default=48)
    ap.add_argument("--gens", type=int, default=100000)
    ap.add_argument("--nseeds", type=int, default=50)
    ap.add_argument("--seed-base", type=int, default=1_000_000)
    ap.add_argument("--champ-cap", type=int, default=10)   # champions kept (anchor is extra)
    ap.add_argument("--add-every", type=int, default=2)
    ap.add_argument("--workers", type=int,
                    default=int(os.environ.get("SLURM_CPUS_PER_TASK", os.cpu_count())))
    ap.add_argument("--elite", type=int, default=3)
    ap.add_argument("--mut", type=float, default=0.03)   # ~3 genes/child (was 0.12 ≈ 13)
    ap.add_argument("--race-top", type=int, default=8,
                    help="racing: re-evaluate the top-N with extra games before ranking (0=off)")
    ap.add_argument("--race-games", type=int, default=100,
                    help="extra games per finalist opponent (added to nseeds)")
    ap.add_argument("--seed", type=int, default=99)
    ap.add_argument("--max-min", type=float, default=1400.0)
    ap.add_argument("--ckpt", default="species2_checkpoint.json")
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--warm-pop", default="")
    ap.add_argument("--clear-champions", action="store_true", help="on resume, drop the champion list (keep pool)")
    ap.add_argument("--fitness", choices=["mean", "min", "gated"], default="mean",
                    help="gated = must beat anchors >= --anchor-gate to survive, then rank by champion score")
    ap.add_argument("--anchor-gate", type=float, default=0.7,
                    help="gated mode: min anchor win-rate a genome must clear to survive")
    ap.add_argument("--promote-thresh", type=float, default=0.5,
                    help="min mode: once champion beats both anchors by this, latch fitness -> mean")
    ap.add_argument("--out", default="champion_species2_only.json")
    args = ap.parse_args()

    random.seed(args.seed)
    os.makedirs(ARCH, exist_ok=True)
    pool = ThreadPoolExecutor(max_workers=args.workers)
    compile_fail = [0]; t0 = time.time(); start_gen = 0

    if args.resume and os.path.exists(args.ckpt):
        pop, old_archive, best, history, last, promoted = load_ckpt(args.ckpt)
        start_gen = last + 1
        if args.clear_champions:
            champ_entries = []
        else:
            champ_entries = [o for o in old_archive if o['genome'] is not None]  # PRESERVE champions
        archive = fresh_archive() + champ_entries                               # reconcile anchors (+ champions)
        print(f"# resumed gen={last} pool kept; champions {'CLEARED' if args.clear_champions else 'PRESERVED ('+str(len(champ_entries))+')'}; "
              f"anchors={[(a['label'],a.get('gate')) for a in fresh_archive()]}", flush=True)
    else:
        archive = fresh_archive()
        promoted = False
        if args.warm_pop and os.path.exists(args.warm_pop):
            pop = warm_pop(args.warm_pop, args.pop)
            print(f"# warm-started {len(pop)} genomes from {args.warm_pop} "
                  f"(extended with {len(NEW)} new params)", flush=True)
        else:
            # seed from the SOURCE defaults (the user's hand-tuned values), not stock
            pop = [dict(SRCG)] + [mutate(SRCG, random.choice([0.05, 0.08, 0.12, 0.18, 0.25]))
                                  for _ in range(args.pop - 1)]
        best = dict(SRCG); history = []

    print(f"# SPECIES2-ONLY workers={args.workers} pop={args.pop} params={len(NAMES)} "
          f"games/gen={args.nseeds} (fresh maps) champ_cap={args.champ_cap} "
          f"fitness={args.fitness} anchors={[a['label'] for a in ANCHORS]} (fixed every round)", flush=True)

    for gen in range(start_gen, args.gens):
        games = gen_games(gen, args.nseeds, args.seed_base)
        gcache = {}
        eff_mode = args.fitness if args.fitness == 'gated' else ('mean' if promoted else args.fitness)
        fit = evaluate(pop, archive, games, pool, gcache, compile_fail, eff_mode, args.anchor_gate)
        # RACING: 50-game scores have SE ~0.07 -> ranking the whole pop on them picks
        # lucky genomes. Re-evaluate the top-N on extra fresh games, pool the win-rates
        # (weighted by game count), and re-rank the finalists on the refined numbers.
        if args.race_top > 0:
            pre = sorted(range(len(pop)), key=lambda i: fit[i], reverse=True)[:min(args.race_top, len(pop))]
            finalists = [pop[i] for i in pre]
            games_x = gen_games(gen, args.race_games, args.seed_base + 50_000_000)
            gc2 = {}
            evaluate(finalists, archive, games_x, pool, gc2, compile_fail, 'mean', args.anchor_gate)
            n1, n2 = len(games), len(games_x)
            for g in finalists:
                k = gkey(g)
                for opp in archive:
                    key = (k, opp['hash'])
                    if key in gc2 and key in gcache:
                        gcache[key] = (n1 * gcache[key] + n2 * gc2[key]) / (n1 + n2)
            refined = evaluate(finalists, archive, [], pool, gcache, compile_fail, eff_mode, args.anchor_gate)
            for idx, i in enumerate(pre):
                fit[i] = refined[idx]
        G.prune_bins()
        el = time.time() - t0
        order = sorted(range(len(pop)), key=lambda i: fit[i], reverse=True)
        bg = pop[order[0]]; best = dict(bg); af = fit[order[0]]
        anchors = [o for o in archive if o['genome'] is None and not o.get('ext_champ')]
        exts    = [o for o in archive if o.get('ext_champ')]
        champs  = [o for o in archive if o['genome'] is not None or o.get('ext_champ')]
        # ext opponents shown alongside anchors (vs52235=...) but carry no gate
        ascores = {o['label']: gcache.get((gkey(bg), o['hash']), float('nan')) for o in anchors + exts}
        for s in SCENARIOS:
            ascores[s['label']] = gcache.get((gkey(bg), s['hash']), float('nan'))
        amin = min(ascores.values()) if ascores else float('nan')
        cmean = (sum(gcache.get((gkey(bg), c['hash']), 0.0) for c in champs) / len(champs)) if champs else float('nan')
        mean = sum(fit) / len(fit)
        n_champs = len(champs)
        astr = " ".join(f"vs{lbl.replace('placeholder','P')}={v:.3f}" for lbl, v in ascores.items())
        print(f"[gen {gen:02d}] fit={af:.3f} {astr} champScore={cmean:.3f} mean={mean:.3f} "
              f"archive={len(archive)} (champs {n_champs}) cfail={compile_fail[0]} t={el/60:.1f}m",
              flush=True)
        history.append({"gen": gen, "archFit": round(af, 3),
                        "vs_anchors": {l: round(v, 3) for l, v in ascores.items()},
                        "champScore": round(cmean, 3),
                        "mean": round(mean, 3), "archive": len(archive), "t_min": round(el/60, 1)})
        diff = diff_default(bg)
        json.dump({"gen": gen, "archive_fitness": af, "vs_anchors": ascores, "champ_score": cmean,
                   "fitness": af, "n_games": args.nseeds, "n_archive": len(archive),
                   "diff_from_default": diff, "genome": bg,
                   # bake ALL params explicitly -> bullet-proof regardless of source defaults
                   "build_cmd": "g++ -O2 -std=gnu++17 " +
                      " ".join(f"-D{n}={bg[n]}" for n in NAMES) +
                      " -o species2_tuned species2.cpp"},
                  open(args.out, "w"), indent=2)

        if gen % args.add_every == 0:
            if add_champion(bg, archive, args.champ_cap, f"gen{gen}"):
                print(f"#   + champion gen{gen} (archive {len(archive)})", flush=True)

        if args.fitness == 'min' and not promoted and amin >= args.promote_thresh:
            promoted = True
            print(f"#   >>> all anchors beaten (min={amin:.2f}) "
                  f"-> fitness latched to MEAN (Phase B)", flush=True)
        save_ckpt(args.ckpt, pop, archive, best, history, gen, promoted)
        if gen == args.gens - 1 or el > args.max_min * 60:
            print(f"# stopping (gen={gen}, {el/60:.1f}m)", flush=True); break

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
    print(f"# DONE archive={len(archive)} vsPlaceholder={vph:.3f}", flush=True)

if __name__ == "__main__":
    main()
