# W545 What the crossing rule costs

Card [#610](https://github.com/MuhammadMoustafa/xppautX/issues/610), 2026-10-29: callgrind instruction counts of master's tip
`2e8e1266` against the same tip with W545's change (the discarded end's test kept after a located zero, a search that ends at ITMX
bisected, both accepted by the crossing rule: docs/auto-references.md W545), as the maintainer asked, recorded with the fix. No wall
time is measured.

## Method

Both builds use the comparison's flags (tools/comparison-setup.sh: `WINDOW=0 'OPT=-g -O2' CCACHE= XPPAUTX_VERSION=paper-comparison`), the
same compiler (Ubuntu g++ 15.2.0), `-std=c++23 -Werror -ffp-contract=off`, each from its own source copy (`git archive master` and the
worktree's files). Valgrind 3.26.0, x86-64: each workload runs as `valgrind --tool=callgrind` under tools/auto-profile/callgrind_wrap.sh's
invocation, driven by tools/autocompare.py's `compute()` at one thread (`auto.set_threads`) through `--server`, whole-process `Ir` from
callgrind's `summary:` (loading, the continuation, the output, shutdown). The comparison's continuation workloads P1, E1, P2, E2 and T1 are
built as paper/benchmarks/comparison/continuation.py's `workloads()` builds them (PY_S1Bf from paper/benchmarks/auto/models, converted by
the binary under test; the motoneuron and Mu26 descriptions of tests/auto with the same closures and P2's parameter window), the same
settings. The suite is every description of tests/auto that runs (62 of 71 descriptions; the other nine are skipped by their own reasons),
each as its own process, eight at a time, the counts summed. Both binaries' session diagrams are compared byte for byte.

## Result (Ir, whole process; ratio = fix / master)

| Workload | master | fix | ratio | points | labels |
|---|---:|---:|---:|---:|---:|
| P1, PY_S1Bf periodic | 22,881,704,467 | 22,881,912,907 | 1.0000 | 741 | 4 |
| E1, PY_S1Bf steady | 245,645,892 | 245,898,314 | 1.0010 | 402 | 2 |
| P2, ModelDB 267710 periodic | 80,115,382,814 | 80,118,482,019 | 1.0000 | 1,189 | 12 |
| E2, ModelDB 267710 steady | 13,332,190,515 | 13,332,390,991 | 1.0000 | 1,080 | 10 |
| T1, Mu26 Hopf curve of the soma | 2,570,557,677 | 2,570,949,356 | 1.0002 | 268 | 6 |
| the 60 descriptions of tests/auto that existed (1 thread) | 190,992,154,714 | 191,034,296,085 | 1.0002 | | |

Points and labels are the same on both builds (the diagrams are byte for byte the same in every workload and in all 60 descriptions):
no workload is more than 0.1% slower. The two descriptions this card adds (pp2_coarse_second_fold, pp2_coarse_bisected_fold) give
different diagrams on purpose (master 128,984,696 and 201,141,612 Ir; fix 144,575,029 and 213,626,166, the new locations' solves).

Over 1% in a single description, each explained by the searches W545 adds (a bracket after a located zero, a bisection: one
solve each, about a million Ir on these models; the counts are from a temporary trace of the locations): global_heteroclinic +10.8% (22 million,
one bracket after a located zero and fifteen bisection solves, two folds refused and one branch point refused before searching),
henon_rot_th +7.1% (seven million, ten bisection solves for the branch point it refuses), pp2_heteroclinic_coarse +2.9% (eight million, two brackets
and four bisection solves, three folds refused), tstauto_ab_hopf +1.1% (2.5 million, no added search in the trace; not traced further). What
a rejected bracket costs buys its refusal: the rule needs the last bracket of a search to be measured, and a bisection is the search that
does not use up ITMX again.

## What the table leaves out

ode_mu26_blocked and ode_mu26_control (87 s and 24 s at one thread natively, 37 times that under callgrind) are not in the suite total. The
trace of every bracket they search shows none new (no bracket after a located zero, no bisection: the trace of the two models' brackets is the master's),
so the extra instructions are the rule's own, which the five workloads (T1 is Mu26's Hopf curve) and the 60 descriptions bound at
+0.02%.
