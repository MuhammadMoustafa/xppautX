#!/usr/bin/env -S uv run --locked python
"""W472 study driver: every model x compiler. Run: uv run --locked python paper/benchmarks/compiler-study/run.py
(from the checkout root). Writes results.json beside it. Needs ./harness (build_harness.sh) and
data/<model>/{model0.c,model0.manifest,output.dat} (collect.sh)."""
import json, os, re, statistics, struct, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = ["mu26", "ma23", "motoneuron6c", "PY_S1Bf_bench", "lecdiff200", "kuramot100", "amari", "clustor",
          "episodic", "izh_2003_rs", "izh_2003_fs", "izh_2004c_tonic_bursting", "izh_2004e_mixed_mode",
          "izh_2004g_class1_excitable"]
K = 200
CC_FLAGS = ["-O2", "-ffp-contract=off", "-fno-semantic-interposition", "-fPIC", "-shared"]
BACKENDS = ["tcc", "gcc", "clang", "mir1", "mir2", "mir3"]
H = os.path.join(HERE, "harness")
BUILD = os.path.join(HERE, "build")
os.makedirs(BUILD, exist_ok=True)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, check=True, **kw)


def build_so(model, cc):
    out = os.path.join(BUILD, f"{model}.{cc}.so")
    src = os.path.join(HERE, "data", model, "model0.c")
    times = []
    for _ in range(5):
        t = time.perf_counter()
        run([cc, *CC_FLAGS, "-o", out, src])
        times.append((time.perf_counter() - t) * 1000)
    return out, statistics.median(times)


def spec(model, b):
    if b in ("gcc", "clang"):
        return "so:" + os.path.join(BUILD, f"{model}.{b}.so")
    return b


def instructions(model, b):
    d = os.path.join(HERE, "data", model)
    cg = os.path.join(BUILD, "cg.out")
    run(["valgrind", "--tool=callgrind", "--toggle-collect=call_rhs", "--smc-check=all-non-file",
         f"--callgrind-out-file={cg}", H, spec(model, b), d, "count", str(K)])
    txt = run(["callgrind_annotate", "--auto=no", "--threshold=100", cg]).stdout
    total = int(re.search(r"([\d,]+) \(100\.0%\)\s+PROGRAM TOTALS", txt).group(1).replace(",", ""))
    code = 0
    for m in re.finditer(r"^\s*([\d,]+) \(\s*[\d.]+%\)\s+(.*)$", txt, re.M):
        line = m.group(2)
        if line.startswith("PROGRAM"):
            continue
        obj = re.search(r"\[([^\]]*)\]\s*$", line)
        if obj and not obj.group(1).endswith("harness"):
            code += int(m.group(1).replace(",", ""))
    return total / K, code / K


def timing(model, b):
    d = os.path.join(HERE, "data", model)
    v = [float(run(["taskset", "-c", "2", H, spec(model, b), d, "time", str(K)]).stdout) for _ in range(5)]
    return min(v)


def compile_ms(model, b, so_ms):
    if b in ("gcc", "clang"):
        return so_ms[b]
    d = os.path.join(HERE, "data", model)
    return float(run([H, b, d, "compile"]).stdout)


def dump(model, b):
    out = os.path.join(BUILD, f"{model}.{b}.bin")
    run([H, spec(model, b), os.path.join(HERE, "data", model), "dump", str(K), out])
    return open(out, "rb").read()


def compare(ref, got, model):
    if ref == got:
        return {"identical": True}
    man = open(os.path.join(HERE, "data", model, "model0.manifest")).read()
    node = int(re.search(r"^node (\d+)", man, re.M).group(1))
    nvar = int(re.search(r"^nvar (\d+)", man, re.M).group(1))
    rec = node + nvar
    a = struct.unpack(f"{len(ref)//8}d", ref)
    b = struct.unpack(f"{len(got)//8}d", got)
    diff = [i for i in range(len(a)) if struct.pack("d", a[i]) != struct.pack("d", b[i])]
    first = diff[0]
    return {"identical": False, "differing_values": len(diff), "first_state": first // rec,
            "first_slot": ("ydot" if first % rec < node else "v") + str(first % rec if first % rec < node else first % rec - node),
            "ref": a[first], "got": b[first]}


results = json.load(open(os.path.join(HERE, "results.json"))) if os.path.exists(os.path.join(HERE, "results.json")) else {}
retime = os.environ.get("RETIME") == "1"  # only the times again, into the existing results.json
only = sys.argv[1:] or MODELS
for model in only:
    if retime:
        for b in BACKENDS:
            results[model][b]["ns_per_call"] = timing(model, b)
            print(model, b, results[model][b]["ns_per_call"], flush=True)
        json.dump(results, open(os.path.join(HERE, "results.json"), "w"), indent=1)
        continue
    so_ms = {}
    for cc in ("gcc", "clang"):
        _, so_ms[cc] = build_so(model, cc)
    ref = dump(model, "tcc")
    r = {}
    for b in BACKENDS:
        tot, code = instructions(model, b)
        e = {"instr_per_call": tot, "model_code_instr": code, "ns_per_call": timing(model, b),
             "compile_ms": compile_ms(model, b, so_ms)}
        e.update(compare(ref, dump(model, b), model))
        r[b] = e
        print(model, b, e, flush=True)
    results[model] = r
    json.dump(results, open(os.path.join(HERE, "results.json"), "w"), indent=1)
