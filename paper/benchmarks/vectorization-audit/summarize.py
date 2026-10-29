# usage: uv run --locked python summarize.py N out/*.top  -> markdown rows: workload | total Ir | top N functions (self %)
# TinyCC's generated model code (anonymous addresses) is summed into one "model code (JIT)" entry, libstdc++'s
# to_chars/format internals and string appends into "std::format/to_chars (text output)".
import re, sys
N = int(sys.argv[1]); files = sys.argv[2:]
def label(name):
    if re.search(r'\?\?\?:0x0+4[0-9a-f]{6}\b', name): return 'model code (TinyCC JIT)'
    if '1a5d80' in name or 'to_chars' in name or '/include/c++/' in name and ('format' in name or 'basic_string' in name or 'stl_iterator' in name): return 'std::format/to_chars text output'
    name = name.replace('(anonymous namespace)', 'anon'); name = re.sub(r' \[.*\]$', '', name)
    name = re.sub(r'^\S*/(core|third_party)/', '', name); name = re.sub(r'\(.*', '', name)
    if 'memset' in name or 'memmove' in name or 'memcmp' in name or 'memchr' in name: return 'libc mem*'
    return name[:48]
for f in sorted(files):
    tot = None; agg = {}
    for line in open(f):
        m = re.match(r'\s*([\d,]+) \(\s*([\d.]+)%\)\s+(.*)', line)
        if not m: continue
        if 'PROGRAM TOTALS' in line: tot = m.group(1); continue
        k = label(m.group(3)); agg[k] = agg.get(k, 0) + float(m.group(2))
    rows = sorted(agg.items(), key=lambda kv: -kv[1])[:N]
    print('| %s | %s | %s |' % (f.split('/')[-1][:-4], tot, '; '.join('%s %.1f%%' % (n, p) for n, p in rows)))
