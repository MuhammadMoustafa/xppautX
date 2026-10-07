# xppautX — notes for Claude Code

@AGENTS.md

This file adds the coordinating session's own duties (review, merge, models);
every rule for agents is in AGENTS.md, imported above.

The reviewer (the main session) reviews, refactors, merges, runs the
5-task tier, pushes when the user says so, and then closes the finished
cards' issues (above). A new roadmap card gets its GitHub issue at once.
Every card's review, before its merge, does three things (maintainer,
2026-10-01): checks the card is met and every "review" row of Code
quality holds; checks the agent followed this section (it stayed in its
card's scope, searched before writing logic and duplicated none: each new
function or block in the diff is grepped for an existing one, ran the
gates its report claims, squashed its wip commits,
closed its register lines, moved the docs, reported what it checked at a
trust boundary), naming a broken rule in the card's note so the next
brief says it louder; and simplifies and refactors the diff even when it
passes: what the change does not need is removed (a dead branch, a flag
nothing varies, a check of what cannot happen, a comment that restates
the code), a new helper moves into its owner, code the diff copied is
merged, a function it made long is split, names say what things are. The
behaviour stays: the card's gates are rerun after it, and it is committed
apart from the agent's work as "Review: ...".

Models are named by family, never by version (maintainer, 2026-10-01),
so a newer one is used the day it ships: the agents' `model:` is the bare
alias (haiku, sonnet, opus). Codex agents (maintainer, 2026-10-01) are a
second pool: docs and one-file mechanical edits luna (low effort), every
other code card sol (medium); a card whose cause or design is still
unknown goes to `task-hard` on sonnet or opus (the Agent tool's model
override; sonnet, high did W159), or to Codex sol at high effort; never
astra, which is token hungry (maintainer, 2026-10-01). Start each card on the cheapest model and
effort that may do it and adapt (maintainer, 2026-10-01: agents code
well from clear instructions): the ladder, effort by effort and within one
the cheaper model first (maintainer): low, then medium, then high, each
with luna, haiku, sol, sonnet, opus in that order; a
failure that shows the card is clearly harder than its step skips ahead
rather than climbing one rung at a time (a Claude agent's effort is its definition's:
`task-easy` low, `task` medium, `task-hard` high, the model set by the
Agent tool's override); any card may start at its bottom
when its brief is precise (the files and lines, what done means, the
exact gates), a card the brief cannot pin down starts higher; when the
review finds the work fell short (a missed requirement, a wrong cause, a
fix the reviewer had to redo), the card goes back one step up with the
review's list, and the step that did the work is noted
on the card, so the starting point per kind of card follows the record
(maintainer, 2026-10-03, from this repo's and laser_pointer's record of
about 30 cards: any code card starts at sol, medium, or sonnet, medium
when Codex has no room, which did about 17 of them, a 36-file refactor
and the protocol and trust-boundary cards included, with at most one
review round; luna and haiku only for docs and one-file mechanical edits,
since luna fell short on 5 of 7 other cards and haiku on 2 of 2; size is
not difficulty: a large, precisely briefed change stays at medium, as sol
at high on four such cards brought the same review fixes at far more
tokens; high effort only for an unknown, a cause to find or a design the
brief cannot settle, as sonnet, high did W159's numerics, or as the climb
when medium fell short); each the newest listed model of its family
(the task-board skill's `codex_model.py <family>`: Codex has no aliases),
run by the reviewer with `codex exec -C <worktree> -s workspace-write -m
<model> -o <report>` and registered by the reviewer. The `delegate` skill lists
every command between Claude and Codex (resume, `codex queue`, stopping a
run by its PID, a session's id, testing its sandbox). A Codex agent follows
every rule of this section as written, with four differences of its
sandbox: it writes only in its worktree and the system's temp folder, so
it starts no background runs and does not touch the register; it cannot commit (the worktree's git
data is in the main checkout's .git, whose lock file the Windows sandbox
refuses even with `--add-dir`, W153), so it leaves its work uncommitted
and its report ends with the commit message, which the reviewer commits
with the Codex trailer before the review; it runs its gates from Git Bash in the
worktree (the UCRT build `mingw32-make -j4 xppautx BUILDDIR=build/ucrt
WERROR=1`, its unit tests, servercheck/autocheck against that exe,
web2's checks and `web2check --only`, run as any agent runs them: the
reviewer starts Codex with `XPP_CHECK_NO_BROWSER_SANDBOX=1` in its
environment, which its commands inherit, since Chrome's own sandbox
cannot start inside Codex's, W168); and the WSL gates
(`tools/wslrun.sh`) are the reviewer's, run on its branch at review, with
the card's web2check sections again (the task-board skill's
`codex_gates.sh`, before the review: a failure goes back to the same
Codex session). Before a batch the
reviewer reads both pools' limits (the app's usage for Claude, the
task-board skill's `codex_limits.py` for Codex) and gives each card to the
pool with room, saying which in the batch proposal.

