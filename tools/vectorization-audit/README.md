# W476 vectorization audit: how to repeat the profiles

`profile.py` runs the 40 workloads of docs/studies/w476-vectorization-audit.md under `valgrind --tool=callgrind` (integration
and post-processing runs as `xppautX MODEL --silent --noout`, the AUTO runs through tools/auto-profile/profile.py, the page's
data work through tools/xppclient.py's Server) and writes each one's self-cost table to OUT/NAME.txt;
`uv run --locked python tools/vectorization-audit/profile.py BINARY OUT [WORKLOAD...]` runs them (about 6 minutes for all),
`--summary` prints the functions above 1% per workload, `--lines FILE --min PERCENT` the source lines of one file above that
share, and `--compare OUT2` the totals of two builds side by side. `vecinfo.sh OUT FILE.cpp...` compiles core files alone
with the normal flags and `-fopt-info-vec-all`, one `OUT/FILE.vec` each, to ask gcc why a loop was not vectorized. Both read
only the repository and write only to OUT (outside the repo; callgrind and the binary are the only requirements).
