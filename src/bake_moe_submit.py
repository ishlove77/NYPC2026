#!/usr/bin/env python3
"""
Bake the MoE SUBMISSION: one .cpp that, given the map, routes to one of the
4 experts' parameter sets at runtime (user 2026-07-06).

Routing (by total region count N, known after READY/init):
    expert 0: N <= 63   | expert 1: N <= 79 | expert 2: N <= 93 | expert 3: N >= 95

Mechanics
- The 104 runtime-safe params: each `#define NAME <val>` becomes
      static int P_NAME = <expert1 default>;
      #define NAME P_NAME
  (in place, so declaration order and #ifndef guards are preserved), plus a
  generated `moe_apply_expert_params(N)` that assigns all of them from a
  4-column table, called in main() right after parse_init.
- The 19 preprocessor-bound params (#if guards) stay compile-time; all four
  expert genomes must agree on them (they are FROZEN in ga_moe.py). On
  disagreement the majority value is used and a loud warning printed.

Usage:
  python3 bake_moe_submit.py                # best champion per expert (by fit)
  python3 bake_moe_submit.py --gen 0:12 --gen 2:30   # pin expert gens
  python3 bake_moe_submit.py --out ../species2b_moe_submit.cpp
"""
import re, sys, json, argparse, subprocess, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga_species2 as GS
from ga_moe import FROZEN, EXPERT_BANDS

from ga_moe import FREE as RUNTIME   # dynamic: present, non-frozen genes

def load_expert(k, pin_gen=None):
    path = f"submissions/genomes_moe{k}.jsonl"
    best = None
    try:
        for line in open(path):
            d = json.loads(line)
            if pin_gen is not None:
                if d["gen"] == pin_gen: best = d
            else:
                if best is None or d["fit"] >= best["fit"]: best = d
    except FileNotFoundError:
        pass
    if best is None:
        print(f"# WARNING: expert {k}: no champion found "
              f"({'gen '+str(pin_gen) if pin_gen is not None else path}) - using source defaults")
        return {"gen": None, "fit": None, "genome": dict(GS.SRC_DEFAULT), "scores": {}}
    return best

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen", action="append", default=[],
                    help="pin expert generation, format K:GEN (repeatable)")
    ap.add_argument("--src", default="species2.cpp")
    ap.add_argument("--out", default="../species2b_moe_submit.cpp")
    args = ap.parse_args()
    pins = {}
    for s in args.gen:
        k, g = s.split(":"); pins[int(k)] = int(g)

    experts = [load_expert(k, pins.get(k)) for k in range(4)]
    genomes = [e["genome"] for e in experts]

    # frozen params: must agree; majority + warn otherwise
    frozen_vals = {}
    for n in sorted(FROZEN):
        vals = [g[n] for g in genomes]
        if len(set(vals)) > 1:
            maj = max(set(vals), key=vals.count)
            print(f"# WARNING: frozen param {n} disagrees across experts {vals} -> using {maj}")
            frozen_vals[n] = maj
        else:
            frozen_vals[n] = vals[0]

    text = open(args.src).read()

    # 1) frozen params: bake the uniform value
    for n, v in frozen_vals.items():
        pat = re.compile(rf"^([ \t]*#define[ \t]+{re.escape(n)}[ \t]+).*$", re.M)
        assert pat.search(text), n
        text = pat.sub(rf"\g<1>{v}", text)

    # 2) runtime params: variable + macro alias, declared in place
    for n in RUNTIME:
        pat = re.compile(rf"^[ \t]*#define[ \t]+{re.escape(n)}[ \t]+.*$", re.M)
        assert len(pat.findall(text)) == 1, f"{n}: {len(pat.findall(text))} defines"
        repl = (f"static int P_{n} = {genomes[1][n]};   /* MoE runtime param */\n"
                f"#define {n} P_{n}")
        text = pat.sub(repl.replace("\\", "\\\\"), text, count=1)

    # 3) expert tables + router, injected before main()
    lines = [ "/* ===== MoE expert routing (user 2026-07-06): params by map size =====",
              "   expert 0: N<=63 | expert 1: N<=79 | expert 2: N<=93 | expert 3: N>=95" ]
    for k, e in enumerate(experts):
        lines.append(f"   expert {k}: gen={e['gen']} fit={e['fit']} scores={e.get('scores')}")
    lines.append("*/")
    lines.append("static int g_moe_expert = -1;")
    lines.append("static void moe_apply_expert_params(int N) {")
    lines.append("  int e = (N <= 63) ? 0 : (N <= 79) ? 1 : (N <= 93) ? 2 : 3;")
    lines.append("  g_moe_expert = e;")
    for n in RUNTIME:
        vals = ", ".join(str(g[n]) for g in genomes)
        lines.append(f"  {{ static const int t[4] = {{{vals}}}; P_{n} = t[e]; }}")
    lines.append("}")
    router = "\n".join(lines) + "\n\n"

    m = re.search(r"^int main\(", text, re.M)
    assert m, "main() not found"
    text = text[:m.start()] + router + text[m.start():]

    # 4) call the router right after parse_init in main()
    call_pat = re.compile(r"(parse_init\(&M, &S\);.*\n)")
    assert call_pat.search(text[m.start():]), "parse_init call not found in main"
    head, tail = text[:m.start()], text[m.start():]
    tail = call_pat.sub(r"\1  moe_apply_expert_params(M.N);   /* MoE routing */\n", tail, count=1)
    text = head + tail

    open(args.out, "w").write(text)
    print(f"# wrote {args.out}")
    r = subprocess.run(["g++", "-O2", "-std=gnu++17", "-o", "/tmp/moe_submit_test", args.out],
                       capture_output=True, text=True)
    print("# compile:", "OK" if r.returncode == 0 else r.stderr[:400])
    return 0 if r.returncode == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
