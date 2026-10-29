#!/usr/bin/env -S uv run --locked python
"""Prints results.json as the README's tables: uv run --locked python paper/benchmarks/compiler-study/report.py"""
import json, os, statistics
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, "results.json")))
B = ["tcc", "gcc", "clang", "mir1", "mir2"]
names = {"tcc": "TinyCC", "gcc": "gcc -O2", "clang": "clang -O2", "mir1": "MIR O1", "mir2": "MIR O2 (default)"}
print("| model | compiler | instr/call (total) | model-code instr/call | ns/call | compile ms | identical to TinyCC |")
print("|---|---|---:|---:|---:|---:|---|")
for m, r in R.items():
    for b in B:
        e = r[b]
        print(f"| {m} | {names[b]} | {e['instr_per_call']:.0f} | {e['model_code_instr']:.0f} | {e['ns_per_call']:.0f} | {e['compile_ms']:.1f} | {'yes' if e['identical'] else 'NO'} |")
print()
print("Geometric-mean ratios against TinyCC over the models (below 1 = fewer / faster):")
print("| compiler | model-code instr | total instr | ns/call | compile ms (x) |")
print("|---|---:|---:|---:|---:|")
def gm(xs):
    p = 1.0
    for x in xs: p *= x
    return p ** (1 / len(xs))
for b in B[1:]:
    f = lambda k: gm([r[b][k] / r["tcc"][k] for r in R.values()])
    print(f"| {names[b]} | {f('model_code_instr'):.2f} | {f('instr_per_call'):.2f} | {f('ns_per_call'):.2f} | {f('compile_ms'):.0f} |")
