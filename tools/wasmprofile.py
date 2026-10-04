#!/usr/bin/env python3
"""W184: aggregate Node worker CPU samples and verify isolated math fingerprints.

Usage: python3 tools/wasmprofile.py PROFILE_DIR NATIVE_MATH_LOG WASM_MATH_LOG
Self costs count sampled frames once. Inclusive costs count each ancestor
name once per sample, so recursive evaluator calls are not double counted.
Idle worker time is reported separately; percentages need an integration
frame denominator, not the total across simultaneously idle workers.
"""
import collections
import json
import pathlib
import re
import sys

own = collections.Counter()
inclusive = collections.Counter()
profiles = list(pathlib.Path(sys.argv[1]).glob('*.cpuprofile'))
if not profiles:
    raise SystemExit('no CPU profiles found')
for path in profiles:
    data = json.loads(path.read_text())
    nodes = {node['id']: node for node in data['nodes']}
    parent = {child: node['id'] for node in data['nodes'] for child in node.get('children', [])}
    for sample, microseconds in zip(data.get('samples', []), data.get('timeDeltas', [])):
        own[nodes[sample]['callFrame']['functionName']] += microseconds
        visited = set()
        while sample in nodes:
            name = nodes[sample]['callFrame']['functionName']
            if name not in visited:
                inclusive[name] += microseconds
                visited.add(name)
            if sample not in parent:
                break
            sample = parent[sample]
for label, values in [('self', own), ('inclusive', inclusive)]:
    print(label)
    for name, microseconds in values.most_common(22):
        print(f'{microseconds / 1e6:.3f}s {name}')
summary = pathlib.Path(sys.argv[1]) / 'summary.json'
summary.write_text(json.dumps({'self_us': dict(own), 'inclusive_us': dict(inclusive)}, indent=2))
print(f'profile summary: {summary}')

def fingerprints(path):
    return dict(re.findall(r'^(\w+) fingerprint=([0-9a-f]+)', pathlib.Path(path).read_text(), re.M))

native = fingerprints(sys.argv[2])
wasm = fingerprints(sys.argv[3])
if len(native) != 3 or native != wasm:
    raise SystemExit(f'math fingerprints differ or are missing: {native} / {wasm}')
print('PASS: all three complete math-kernel fingerprints match')
