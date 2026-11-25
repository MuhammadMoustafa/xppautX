# W857 (#938): does follow_all_branches apply inside a script whose runs are explicit?

A script run is one named analysis with an explicit step sign ("both directions are two runs", docs/manual/09-autox.md), yet
`follow_all_branches` defaults to on and follows both directions of a run's start and every branch it finds. The lab's three scripts
(tools/models/pvn_slowfast_hcn_w857.autox, pvn_slowfast_cat_w857.autox, pvn_bif_current_w857.autox, written under that default) stored
stray and repeated branches. The maintainer asked for both answers to be built and compared; this page is the comparison, not a choice.

Common to every variant but master (commit A): a run refused before its first step adds no branch and no point, shows as that run's error
in the Script panel (never Done), and `--silent` exits non-zero. Concretely: a run that follows all branches from a start strictly outside
the parameter window is refused (master stored the start alone as a one-point branch).

- **master**: origin/master 4482a431, no change.
- **A**: branch `task/W857-script-runs-what-it-names`, commit 1 alone (the refusal and its reporting; follow_all as before).
- **(a)**: the same branch, commit 2 on A: follow_all is off in scripts unless a run's own options set `follow_all_branches: true`.
- **(b)**: branch `task/W857-variant-b`, cut from A: follow_all stays on; a script run whose start lies on a branch the diagram holds is
  not made (W862's `autox_on_branch`), the Script panel row says Skipped and why, the log says it once.

## Method

`xppautX SCRIPT --silent` of each binary in a fresh folder, `taskset -c 0`, median of 5 wall times (pvn_bif_current: 1 run, 160 to 220 s
each; other agents' builds and checks ran on the machine meanwhile, so the times show only that no variant is slower by a large factor).
Branches and points are the rows of the diagram CSV. A repeated point is one that lies within 8 x 1e-4 (autox_same_point_factor x the default
EPSS, scaled by 1 + |x|) of a point of an earlier branch in the parameter and every state's maximum; it also counts the Hopf point an
equilibrium branch and its cycles share, so a few repeats are shared points, not retraced branches (hodgkin_huxley: 3). The two published
scripts that use follow_all on a script do not exist in the tree (no .autox file besides the lab's): lrz_w857.autox (AUTO-07p's lrz demo, the
Lorenz equations, Lorenz 1963) and hodgkin_huxley_w857.autox (Hodgkin and Huxley 1952) are the same runs as tests/autox/lrz_follow_all.json and
hodgkin_huxley_follow_all.json written as scripts, each naming `follow_all_branches: true` itself (lrz from the model's own start r = 13.9).

## Results

Runs the script names / runs that ran: master | A | (a) | (b).

| script (runs named) | build | exit | branches (points each) | points | repeated | median | runs that ran once |
|---|---|---|---|---|---|---|---|
| pvn_slowfast_hcn (2) | master | 0 | 3 (2979, 202, 1) | 3182 | 10 | 0.57 s | h up ran both ways (2 branches); h down stored its start (1 point) |
| | A | 1 | 2 (2979, 202) | 3181 | 9 | 0.56 s | h up both ways; h down refused (error) |
| | (a) | 1 | 1 (2979) | 2979 | 0 | 0.47 s | h up one way; h down refused (error) |
| | (b) | 1 | 2 (2979, 202) | 3181 | 9 | 0.56 s | as A |
| pvn_slowfast_cat (2) | master | 0 | 3 (39, 69, 1) | 109 | 2 | 0.03 s | cat up both ways; cat down 1 point |
| | A | 1 | 2 (39, 69) | 108 | 1 | 0.02 s | cat up both ways; cat down refused |
| | (a) | 0 | 2 (39, 1) | 40 | 1 | 0.01 s | cat up one way; cat down 1 point (see below) |
| | (b) | 1 | 2 (39, 69) | 108 | 1 | 0.02 s | as A |
| pvn_bif_current (3) | master | 0 | 7 (3000, 278, 476, 3000, 614, 476, 476) | 8320 | 3461 | 160 s | rest up both ways; rest down both ways (3000 again); cycles: 476 three times |
| | A | 0 | same as master | 8320 | 3461 | 222 s | as master |
| | (a) | 1 | 1 (3000) | 3000 | 0 | 1.2 s | rest up one way; rest down fails (Newton from the model's rest state at the end value of ihold); cycles not run |
| | (b) | 0 | 4 (3000, 278, 476, 476) | 4230 | 290 | 114 s | rest up both ways; rest down skipped; cycles: 476 twice |
| lrz (1, names follow_all) | all four | 0 | 6 (35, 28, 92, 92, 72, 72) | 391 | 5 | 0.05 s | the one run, every branch |
| hodgkin_huxley (1, names follow_all) | all four | 0 | 3 (59, 13, 58) | 130 | 3 | 0.10 s | the one run, every branch |

## What each variant does, for a user who wrote the script

**A alone** keeps the default: a run named "h up" is the equilibria both ways from its start, plus every branch they find. What it fixes
is the refusal: "h down" no longer leaves a one-point branch and an exit 0; it is an error on its row. The stray branches stay (the 202
points that "h up" stored going down, bif_current's 476-point cycle three times), because they are what follow_all asked for. A user who
wrote "h up" and "h down" as two runs gets the downward branch twice or more in the diagram and a script that cannot say which run made it.

**(a) off in scripts** makes a run mean what it says: one branch in the direction of its step's sign, so `h up` is 2979 points and no more,
the diagram has one branch per run that ran, nothing repeats, and a run that wants every branch says `follow_all_branches: true` (lrz and
hodgkin_huxley are unchanged). The price is that scripts written under the old default change meaning: pvn_bif_current's "rest down" starts
from the end of "rest up", not at rest, and Newton fails there, so the script stops with an error (exit 1) before "cycles";
the author must give each run its start. Open point found here: pvn_slowfast_cat's "cat down" starts 0.0031 past the window with a step of
0.001, which the single-run refusal (a start is refused only when the step points away) lets through, and the run stores its start alone
(a one-point branch, exit 0). Refusing every start outside the window would also refuse autoxcheck's "extend the grabbed periodic branch"
(its end is 0.0011 past the bound and the step brings it back in), so a rule that tells the two apart (can the first step return?) is a
follow-up card.

**(b) on, repeats skipped** keeps the old meaning of follow_all (both ways, every branch) and removes only what the diagram already holds:
bif_current's "rest down" is not made and its row says so (3000 points fewer, 114 s against 160 s), the refusal of "h down" is as in A. What
it does not remove is what one run repeats inside itself (the cycles' 476-point branch appears twice in "cycles", 290 points counted as
repeated) or the second direction a run was never asked for ("h up" still stores the 202-point branch going down). A user who wrote two
runs gets an explicit message for the repeat but still gets branches no run named.

## Not decided here

The choice is the maintainer's. The three scripts are the evidence: (a) is the only variant in which "one branch per named run that ran" holds
on hcn, and it needs bif_current's author to write per-run starts; (b) leaves hcn and cat as under A and bif_current with one 476 repeat.
