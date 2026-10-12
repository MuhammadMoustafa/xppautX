# W309 debate: one named set, or three kinds?

On 2026-10-12 the maintainer chose XPPAUT's syntax for W309 (the `par`/`init` lines are the
defaults, `set NAME { }` holds any mix), then asked whether sets should be split into three
kinds: parameters, settings and initial conditions. Claude answered "keep one" and, at the
maintainer's request, debated it with Codex Astra (gpt-6-astra, read-only, high effort) for two
rounds. The full exchange follows the outcome.

## Outcome: what both sides sign

- **One `set`.** A set holds any mix. Separate sets are written when parameters and initial
  values are meant to be reused independently. The interface shows what each set holds, and a
  mixed set stays visibly mixed.
- **Settings nested under `options:` inside a set; model names flat:**

  ```
  set vvst {
    iapp: 0.1,
    options: { plot: { xp: t, yp: v }, numerics: { total_time: 100 } }
  }
  ```

  The collision is real (Astra, from the source: core/load_eqn.cpp:295 and XPPAUT 8.0's
  load_eqn.c:845). With `par dt = 2`, `set trial { dt: 0.1 }` changes the parameter, not the
  step. A parameter named `method` makes `method: stiff` fail rather than pick the solver.
  Nesting under `options` (already a reserved word) makes every top-level key a model name.
  The converter resolves XPPAUT's flat keys by XPPAUT's own precedence (model names first),
  then nests. This keeps XPPAUT's structure, not its literal text.
- **Apply: a patch over the current values** (the maintainer's "dict update"), not
  reset-by-kind. The set is checked whole, then only its entries change, in one step.
  Unlisted values stay. A parameter-only set leaves the initial values alone. A failure changes
  nothing. Reset-by-kind was rejected because it changes what the set does not say: one plot
  setting in a set would reset every plot setting changed by hand. Two disjoint parameter sets
  (forcing, damping) could not combine. A set naming one state would reset all the others.
- **Restore defaults.** Per panel, as XPPAUT's Default buttons: parameters, initial values,
  and each settings group (settings go back to the built-in defaults with the model's
  `options` over them). Plus **Restore all model defaults**, one core operation, for a fresh
  start after mixed sets and for scripts.
- **Save:**
  - Save defaults writes the `par`/`init` lines (and options) the user chooses.
  - Save set writes into a set the user picks, never the last one applied. It shows the target
    and what it holds; widening its scope is an explicit choice.
  - Save as writes a complete capture of the chosen kinds, saying initial or current values.
  - Export writes a separate file.
  - All keep comments, detect a file changed on disk, and replace atomically.
- **Reproducibility lives in captures,** within their scope: a parameters-only capture does not
  reproduce a figure whose initial values and settings it left out.

## What the maintainer decides

1. Settings nested under `options:` inside a set.
2. The patch rule, replacing the recorded reset-by-kind rule.
3. Whether Restore all appears in the window, or only in scripts and the protocol.
4. Choosing what Save defaults and Save set write (parameters, initial values, settings).

---

## Round 1: Claude's brief

W309-SETS-DEBATE: one named set, or three kinds?

You are reviewing a design for xppautX (a modernized fork of XPPAUT, the ODE/bifurcation tool).
You are read-only: read the files, think, and answer in text. Be a thinker, not a yes-man: push
back where something does not feel right, including on Claude (the coordinator) and on the
maintainer. We go several rounds until we settle.

## Context (read these)

- docs/studies/w309-values-design/design.md and debate.md: W309's earlier design (typed named
  groups `par NAME default { }`, `init NAME { }`, `options NAME { }`, `preset` bundles).
- docs/studies/w314-components/debate.md: a later debate (with you, in another session) that
  ended with components being parked as "a whole new program".
- docs/roadmap.md row W309 (#368): REVISED on 2026-10-12. The maintainer: "either keep the old
  syntax or our new syntax, I think both together will be a problem". Decided: XPPAUT's
  syntax. `par a = 1` / `init x = 1` lines are the defaults; `set NAME { key: value }` (already
  in .odex, docs/odex.md "Statements") holds any mix of parameters, initial values and
  settings; applying a set is a dict update of the defaults, each kind it names (parameters,
  states, each settings group) reset to the defaults first, then its keys applied, so a
  parameter-only set and a states-only set combine. Saving: Save as appends a set of what the
  user chooses to capture; Save writes back to the applied set or the par/init lines; Export
  writes a separate .odex. Typed groups, `default` and `preset` dropped.
- Existing sets, e.g. examples/ (grep `^set ` in *.odex): lecar's
  `set vvst { xp: t, yp: v, xlo: 0, xhi: 100, ylo: -.6, yhi: .5, total_time: 100, iapp: 0.1 }`
  mixes plot settings, a numerics setting and a parameter; `set hopf { v3: 0, v4: 0.3, ... }`
  is parameters only. XPPAUT's manual (docs/manual/02-ode-files.md around line 719-735) says a
  set changes "the parameters, initial conditions, and DeltaT".
- The menu still says "Get par set" (XPPAUT's wording); W316 (#376) renames it "Named sets".
- core/model.h `InternalSet`, core/commands.cpp around line 105, and wherever a set's items
  are applied: check how a key is resolved.

## The maintainer's question

"Do you recommend splitting the sets to three different sets: parameters, settings, initial
conditions?"

## Claude's answer (argue with it)

No: keep one `set`.
1. Three kinds is W309's typed groups again, the syntax the maintainer just chose not to mix
   with XPPAUT's.
2. A regime is often a mix (lecar's vvst: plot settings + iapp; a "start near the cycle" set:
   parameters, initial values and total_time); split, the user saves and applies three things
   to get one regime.
3. Mix-and-match already works through the reset-by-kind rule; the interface can mark what each
   set holds and Save as asks which kinds to capture.

One real problem: option keys are not reserved words (docs/odex.md: `stiff`, `total_time` can
name a variable), so a model with a parameter or variable named like a settings key (`dt`,
`method`, `xlo`) makes `set x { dt: 0.1 }` ambiguous. Claude has not verified how the code
resolves it today. Proposed fix: settings inside a set are nested under their group, as the
`options` block writes them:

    set vvst { iapp: 0.1, plot: { xp: t, yp: v }, numerics: { total_time: 100 } }

so a top-level key is always a model name; the converter writes XPPAUT's flat set lines this
way.

## Questions for you

1. One set or three kinds? Weigh learning cost for XPPAUT users (mathematicians of every field),
   how users actually build and reuse regimes, saving and the UI, and the maintainer's "not both
   syntaxes" rule. If one: is anything lost that three kinds would give?
2. Verify in the code how a set key that is both a model name and an option key resolves today.
   Is the ambiguity real? Is nesting the right fix, or a rule (model names win; an option key
   that is also a model name is an error at load), or something else?
3. Is "reset each kind it names to the defaults, then apply" the right apply rule, against
   XPPAUT's own (apply over the current values) and a plain update of the defaults? Consider:
   a settings group reset when a set names one setting of it (applying `vvst` resets every plot
   setting the user changed by hand); a set naming one state (does every other state reset?).
4. Anything else wrong or missing.

Answer in at most about 800 words, numbered points.

## Round 1: Astra

1. **Keep one `set`, but Claude’s first argument is too strong.** Three kinds would not necessarily restore the entire rejected design: typed alternatives could coexist with ordinary `par`/`init` defaults without named defaults or bundles. The better argument is practical: users sometimes vary parameters and starting states independently, and sometimes save one experiment containing both plus settings. One mixed container supports both without requiring three selections.

   Three kinds do offer something: guaranteed scope (“this cannot change my parameters”), clearer selection ownership, and natural section-specific menus. Recover that through visible contents, filters, and capture choices. A mixed set must remain visibly mixed even when found through the parameters panel. One set does **not** automatically provide independent reuse of its constituent parts; authors should create separate sets when they want that independence.

2. **The collision is real, but resolution today is deterministic.** In [load_eqn.cpp](../../../core/load_eqn.cpp:295), validation recognizes model initial-condition/parameter names before checking options. Application checks initial-condition names first, parameters second, and options last. Thus, with `par dt = 2`, `set trial { dt: 0.1 }` changes the parameter, not the integration step. A parameter named `method` makes `method: stiff` fail numeric validation rather than select a solver. The [converter](../../../core/odex_convert.cpp:1180) likewise checks model names first; [XPPAUT 8.0](../../../reference/xppaut-8.0/load_eqn.c:845) uses that precedence too. These are source findings, not executed tests.

   “Model names win” documents existing behavior but leaves shadowed settings inaccessible inside sets. Rejecting collisions instead restricts legitimate mathematical names and makes adding an option potentially break models.

   I favor explicit settings nesting, with one adjustment to Claude’s proposal:

   ```text
   set vvst {
     iapp: 0.1,
     options: {
       plot: { xp: t, yp: v },
       numerics: { total_time: 100 }
     }
   }
   ```

   `options` is already reserved; `plot` and `numerics` need not become special top-level model names. Use whichever group vocabulary W309 ultimately adopts. Keep model assignments flat and settings exclusively under `options`. Conversion must resolve original XPPAUT keys **before** nesting or renaming them.

   This is a syntax change. The maintainer should acknowledge that “XPPAUT’s syntax” here means preserving its familiar structure, not literal identity.

3. **I would reopen—and reject—the implicit reset-by-kind rule.** My earlier acceptance concerned explicitly typed selectors. Moving that behavior behind an ordinary mixed `set` weakens its justification.

   Consider two disjoint parameter sets, one changing forcing and another damping: reset-by-kind prevents their combination. A set naming one state resets every other initial value. `vvst` can discard manually adjusted plot settings and integration tolerances merely because it mentions axes and `total_time`. Those are substantial effects absent from the written entries.

   The alternatives have distinct costs:

   - **Patch current values:** only listed entries change; composition works, but partial sets depend on prior values.
   - **Defaults plus this set, globally:** deterministic overall configuration, but applying a states-only set erases a previously chosen parameter regime.
   - **Reset touched kinds/groups:** preserves some combinations but makes hidden reset boundaries part of the language. Regrouping settings can then change scientific behavior.

   I recommend XPPAUT’s patch rule for `set`, with an explicit **Restore model defaults** action. Reproducible scripts start from defaults once, then apply their ordered sets and overrides. Complete captures provide reproducible values for their captured scope. This keeps one set semantics; restoring defaults is a separate operation. History dependence is a tradeoff, not inherently an XPPAUT bug.

4. **“Save writes back to the applied set” is underspecified.** After applying parameter set A and initial set B, both contribute. After mixed A and parameter-only B, A remains only partly effective. The current [single remembered set name](../../../core/commands.cpp:136) cannot represent that.

   Make **Save set** target an explicitly selected named set, showing its destination and captured scope. Do not infer the destination from the last Apply. Saving defaults should also be explicit: changing defaults changes what every future reset means. Preserve authored entries and comments; retain fingerprint checks, atomic replacement, and the imported-file ownership rules.

5. **Keep captures and restart data distinct.** Save as should identify initial versus current/ending values and capture every value in each chosen category, so a saved computed state survives later default edits. An instantaneous vector is not a periodic orbit or delay-system restart. Validate every set at load, apply transactionally, and preserve the earlier contract that parameter-only application does not silently reevaluate initial expressions. Syntax simplification must not discard those behavioral guarantees.

## Round 2: Claude

Claude's reply, round 2.

1. Accepted: one set, and your better argument (one container serves both independent and joint
   reuse; separate sets when independence is wanted). Accepted: the UI shows what each set holds,
   and a mixed set stays visibly mixed.

2. Accepted: settings only under `options:` inside a set, model names flat; conversion resolves
   XPPAUT's keys first (model names before options, as load_eqn.cpp and XPPAUT 8.0 do) and only
   then nests. The groups are the `options` block's own (numerics, plot, ...), so a set's
   `options:` reads exactly like the block. I will tell the maintainer this is XPPAUT's
   structure, not its literal text.

3. You convinced me on the apply rule, and the maintainer's own words were "it's like dict
   update", which is your patch over the current values, not my reset-by-kind. My only worry was
   W309's reason for overlays from the defaults: a figure made after snic then hopf is a mix
   nobody wrote down. But complete captures (Save as) record every value, so reproducibility
   lives in the capture, not in the apply rule. One addition: XPPAUT already has the restore
   action per panel (the parameter window's and the initial-conditions window's Default
   buttons, back to the file's values). So: Restore defaults per kind (parameters, initial
   values, each settings group?) or one action for everything? I lean per panel, as XPPAUT, plus
   nothing global. Push back if you think a global restore is needed for scripts.

4. Accepted: Save set writes to a set the user picks explicitly, showing the target and its
   scope; never inferred from the last Apply; saving the defaults (the par/init lines) is its
   own explicit action. Is "Save defaults" even needed, given Export and editing the file? I
   would keep it: a user tuning parameters by hand wants "make these the model's values"
   without leaving the program.

5. Accepted: Save as names initial versus current values, captures every value of each chosen
   kind, validation at load, transactional apply, and a parameter-only set never re-evaluates
   initial expressions.

If you agree, state the final rule set in a few lines (syntax, apply, restore, save) and what
the maintainer must decide. If not, say where. At most about 400 words.

## Round 2: Astra

Agreed, with two qualifications: a capture is reproducible only for the scope it includes, and scripts need an explicit way to start from the complete model baseline.

1. **Syntax:** ordinary `par`/`init` lines and the model’s `options` block establish defaults. One `set NAME { ... }` holds flat parameter/state assignments plus settings under `options: { numerics: {...}, plot: {...}, ... }`. No typed named groups, named defaults, or bundles. Conversion resolves XPPAUT keys using its original precedence before nesting settings.

2. **Apply:** validate the complete set, then patch only its listed entries over current values in one transaction. Unlisted values remain unchanged. Parameter-only application preserves initial values. Failure preserves the session, including RNG state.

   Complete captures remove dependence on previous values **within their captured scope**. A parameters-only capture does not reproduce a figure whose initial values and integration settings were omitted. Scripts must record their effective inputs too.

3. **Restore:** keep per-panel defaults for parameters and initial values, and per-group defaults for settings. Settings restore to built-in defaults overlaid by the model’s options. Parameter restore leaves initial values alone. Initial restore is an explicit initialization action; document evaluation of file expressions against the effective parameters.

   I would also provide **Restore all model defaults** through the same core operation. It gives users a clear fresh start after applying mixed sets and gives scripts a baseline without enumerating today’s panels. A script may instead start each independent run from a fresh model session. Global restore need not occupy every panel.

4. **Save:** keep **Save defaults**: tuning the model through its controls is a legitimate authoring workflow. Make its target categories explicit. **Save set** targets a user-selected set and updates its existing entries; expanding its scope requires an explicit choice. **Save as** creates a complete capture of chosen categories, distinguishing initial from current/ending values. **Export** writes the chosen content separately. Preserve comments, detect disk conflicts, and replace atomically.

The maintainer must approve the nested settings syntax, the patch rule replacing the recorded reset-by-kind decision, whether to expose Restore all in the GUI, and the scope controls for Save defaults and Save set.
