# W309 design review: Claude and Codex (Astra)

The design in [design.md](design.md) was reviewed by Codex (gpt-6-astra, high effort, read-only on
this repository) on 2026-10-11, at the maintainer's request: "be a thinker, push back if something
doesn't feel right". Three rounds; Astra closed with "settled". The full exchange follows the
summary, verbatim.

## Outcome

### What changed in the design

- **Captures vs overlays.** A group saved from a computed state (after a run, or a grabbed AUTO
  point) is written complete, with a provenance comment (model, time, source). Overlays are for
  authored regimes. AUTO restarts keep referring to stored solutions (W306), never flattened into `init`.
- **`preset` bundles (new).** `preset vvst { par: vvst, options: vvst }`: flat, at most one
  reference per kind, no values, no nesting, validated whole before anything applies, applied from
  its own Presets menu. The converter writes one for every mixed XPPAUT `set`. "Save state with its
  parameters" writes `par rest`, `init rest` and `preset rest` in one transaction.
- **Selection semantics.** Choosing a `par` or `init` group starts from the defaults (so "hopf"
  always means the same values); XPPAUT's patching (cool then hopf keeps cool's `iapp`) is dropped
  and documented as a difference. Choosing an `options` group resets only the sections it names
  (option-table defaults, then the model's default options group, then the group); lists replace.
- **Declarations.** The default `par` group declares the parameters. A non-default group assigns:
  importing it (`from "f.odex" import bursting`) brings no declaration, and its keys must be
  parameters the consuming model declares. A file of non-default groups only is a valid import.
- **Imported models.** `import "cell.odex" as c1`: `c1.` groups whose defaults compose inside c1's
  namespace; selecting `c1.bursting` resets only c1's parameters. Duplicate group identity and two
  defaults owning one parameter are two different errors.
- **Initial values.** Default and selected `init` maps merged before evaluation, settings (seed)
  first, each expression evaluated once in order. Selecting a parameter group keeps the current
  initial numbers; only selecting an initial group (or Initialize) evaluates, and selecting the
  same one again re-evaluates (new random draws). Histories stay outside groups.
- **Validation.** Every group checked at load, all or nothing, with kind, group, key, file, line,
  column and source in errors; W296's limits; a path in an options group is asked about (W286)
  only when the group is selected.
- **Saving.** Save checks the file's fingerprint and refuses a conflict, validates the whole new
  text, replaces it atomically, then updates the live groups without resetting the session. Save
  writes only into the open model file (an imported group: Save as or Export); in a `.snapx`, into
  the session's embedded model. Unchanged entries keep their text (`v: f(a)` stays). Export writes
  complete values and can add the import in one step.
- **`.setx` stays** as the session-settings transfer format: an XPPAUT `.set` holds graphics,
  delays, boundary conditions and run position that have no group, so it keeps converting to a
  `.setx` (W284), not into groups.
- **Arrays** in groups reuse the range syntax and limits; the grammar extension (expression values,
  per-entry ranges) is stated in the spec. Existing converted `.odex` examples with mixed sets
  (r3b's `period`, the coaster models) join the conversion checks.
- **Window.** "bursting — modified", inherited vs explicit values, initial vs current vs ending
  shown; actions labelled Save group / Save group as... / Export group....

### For the maintainer to decide

1. **Approve the named-set break**: groups are chosen from the defaults, not patched over the
   current values as XPPAUT does.
2. **`.set` keeps converting to `.setx`**, reversing the earlier "three files" decision (a `.set`
   holds things no group can take).
3. **The `preset` bundle**, added by the review: approve it.
4. **Migration of our own older `.odex` (and those inside `.snapx`/`.recx`)**: a one-shot
   translator (an explicit exception to "no code for older files of ours"), or a manual guide plus
   an error that names the fix. Astra favours the translator; Claude the guide.
5. **Save keeps authored text** for entries the user did not change.
6. Astra's view, relayed: a plain `par a = 1` line is not error-prone in itself, the ambiguity came
   from undocumented precedence. The named-default design stands unless you reopen it.

## The exchange

### Round 1: Claude to Astra (the brief)

[W309 design review, round 1]

You are reviewing a design for xppautX (a modernised fork of XPPAUT, a tool for simulating and analysing differential equations; its users are mostly computational scientists). Read docs/studies/w309-values-design/design.md in this repository first: it records a design settled between the maintainer and Claude (the coordinating agent), with the options that were rejected and why. You may read anything else in the repository for evidence (docs/odex.md, docs/protocol.md, AGENTS.md's "Code quality", core/model_options.cpp, core/lunch-new.cpp, examples/), but you change nothing: this is a read-only review.

Your role: be a thinker, not an approver. Push back wherever something does not feel right; do not agree with a point just because it was decided. For each point you raise, say why, with evidence from the repository or from how comparable tools (XPPAUT, AUTO-07p, PyDSTool, MATLAB's MatCont, SBML/COPASI, Julia's ModelingToolkit) handle it, and what you would do instead. Agree only where you genuinely do, and say briefly why.

Cover at least:
1. The final design (section 2.5, item 7, and 2.6-2.7): named typed groups inside .odex, one complete `default` per kind, overlays, Save / Save as / Export. Is it sound for real users? What will go wrong?
2. Each of the ten open points in section 3: your answer to each.
3. Whether any rejected option (section 2.5, items 1-6) was rejected for a wrong reason, or deserves to come back.
4. Anything missing: use cases not covered, interactions with .snapx sessions, .autox scripts (W306), the protocol, conversion from .ode, security (the model file may come from someone else), all-or-nothing loading, error messages naming file and line.

Answer as a numbered list, most important first; each item: the point, your position (agree / disagree / modify), the reason and evidence, your concrete proposal. End with a short list of the questions you think only the maintainer can decide. Keep it under about 1500 words.

### Round 1: Astra

1. **A saved result must survive changes to defaults — modify (§2.5.7).** Typed groups and independent selection are sound, but overlays and captured results have different meanings. Saving only differences makes yesterday’s “steady state” change when tomorrow’s default changes. Keep overlays for authored regimes; save computed states and their associated parameters as complete numeric groups, with model identity and provenance. An initial vector is also insufficient for periodic-orbit continuation: [AUTO-07p distinguishes points from orbits](https://raw.githubusercontent.com/auto-07p/auto-07p/master/doc/auto.tex), and [`.snapx` already stores restart orbits](../../../docs/protocol.md#L1635). W306 should retain references to those richer results rather than flattening everything into `init`.

2. **Conversion changes named-set behaviour — disagree with automatic equivalence.** Existing sets patch current values; [the implementation changes only listed entries](../../../core/load_eqn.cpp#L307). In [lecar.ode](../../../examples/ode/lecar.ode#L33), selecting `cool` then `hopf` retains `cool`’s `iapp`; a default-based overlay resets it. Splitting mixed sets also loses their single atomic action. Preserve converted sets through an explicit “apply these changes” operation, separate from selecting a default-based preset. If that operation is rejected, document an intentional compatibility break. The 183 default-run checksums alone cannot establish equivalence.

3. **Save needs ownership and conflict semantics — modify (open point 4).** Replacing source spans can preserve surrounding text, but cannot prevent overwriting an editor’s newer version. Imported groups may belong to shared libraries; archive models have virtual sources. [Session restoration deliberately ignores subsequent disk edits](../../../docs/protocol.md#L1655). Save should check the source fingerprint, refuse a conflict, validate the proposed complete file, then replace atomically. Show the destination; default imported groups to a local copy. Inside `.snapx`, update the session’s embedded model and persist through Save session. Never redirect that operation to the original disk model.

4. **Imported defaults require scoped composition — modify (open point 7).** Two instances of `cell.odex` necessarily bring two parameter defaults. Treating both as competing global defaults breaks a documented use case. Conversely, [imported options currently do not apply](../../../docs/odex.md#L341). Prefix group names and their targets; compose parameter/initial defaults within each instance’s namespace. Keep imported settings available only through explicit selection. Reject overlapping default ownership, naming both sources. Selective group imports must distinguish names they *target* from declarations they *depend on*, so importing a regime does not accidentally import another model.

5. **Initial expressions need an explicit evaluation lifecycle — modify (open point 5).** Agree with evaluating against selected parameters, but merge the default and selected initial-expression maps **before** evaluation. Then apply effective settings, including seed, and evaluate each surviving expression once in a documented order. Otherwise overridden random expressions consume draws unnecessarily; [setting the seed currently reseeds the generator](../../../core/model_options.cpp#L337). Parameter edits should mark an expression-derived initialization stale, not silently overwrite manually edited ICs. Offer explicit reinitialization. Snapshot restoration must restore saved numbers and RNG state without reevaluating formulas. Delay histories need separate treatment: `init` does not define the prehistory.

6. **Scripts should describe reproducible runs — agree with overrides, modify their contract (open point 9).** [AUTO-07p supports solution objects plus overriding options](https://raw.githubusercontent.com/auto-07p/auto-07p/master/doc/auto.tex); that is a useful precedent. Define precedence explicitly: model defaults → selected groups → inline overrides, with a separate rule when starting from a stored continuation solution. Omitted inputs must not inherit an unrelated previous run. Following [W306/W308](../../../docs/roadmap.md#L951), expose structured list, inspect, select, capture and save operations through one core owner. Return resolved inputs, provenance and result IDs. Validate the entire script before execution; distinguish invalid scripts from numerical failures after earlier runs have completed.

7. **“Offered but inactive” must still mean validated — modify the loading contract.** [The repository requires whole-file validation](../../../docs/protocol.md#L209). Check every group’s syntax, names, types and expressions before accepting the model; evaluate context-dependent expressions transactionally when selected. A failed selection must preserve values, settings and RNG state. Diagnostics should identify group kind/name, offending value, source file/line/column and source text, with the importing or script call site as context. Reuse existing token, expansion, allocation and path limits. Listing foreign groups must not open their logfiles or execute buttons. Pure parsing still needs aggregate budgets against many groups and imports.

8. **One settings selector can undermine independent reuse — modify (§2.1, §2.7).** Agree with task-oriented names, but selecting continuation settings should not unexpectedly reset plot axes, integration tolerances or output paths. “Complete options default” is also undefined: the example specifies only `dt`, while [the option table supplies other defaults](../../../core/model_options.cpp#L1019). Define completeness as the resolved settings, not mandatory boilerplate. Let a preset declare its affected settings sections, resetting within those sections only. Specify list semantics explicitly—sliders and buttons should replace their list, not accumulate on repeated selection.

9. **Declarations should be independent of values — modify (open point 1).** Prefer `par a, b` declarations plus a complete named parameter default. Repeating names is a modest cost for an independently checkable model interface. If the default group declares parameters, “missing parameter” is circular unless another declaration establishes what was expected. States remain declared by equations or their appropriate state declarations. Completeness should cover actual independent states, including Markov states, not auxiliaries. W313’s warning-and-zero rule means initial defaults are *completed with diagnostics*, not strictly complete. Specify empty-kind behaviour for parameter-free models.

10. **Same names across kinds are useful, but do not establish a pair — modify (open point 8).** Allow `(kind, qualified name)` identities, so `par rest` and `init rest` coexist. Require typed selectors where an import would otherwise be ambiguous. “Save state with its parameters” should capture both in one transaction and record their relationship explicitly; matching spelling alone is insufficient. Rejected option 3 used a false implication: independent reuse does **not** require separate files. Typed groups can coexist in one file and still be selected independently. A small bundle referencing groups would preserve convenient joint application without merging their meanings.

11. **Keep `.setx` for its distinct job — modify (open point 3).** [Its documented contents](../../../docs/odex.md#L265) include run position, ending values, delays, boundary conditions and graphics—not merely model options. Keep it as an explicitly labelled session-settings transfer format and archive representation, using shared validators. Use `.odex` groups for authored presets. Publish a field mapping before converting XPPAUT `.set`: splitting it into parameters, initials and options must not silently discard fields that have no destination.

12. **Breaking `.odex` requires more than regenerating examples — modify (open point 2).** Some files were authored directly; `.snapx` and `.recx` embed `.odex` text. Regenerating from `.ode` cannot recover those edits. Use a deterministic default name such as `base`, resolving collisions per kind. Ship a separate, explicit migration tool, outside the runtime loader, preserving authored content and embedded archives. This respects the no-runtime-fallback rule. Verify ordered parameter declarations, initial-expression evaluation, named-set sequences and archive replay as well as numerical baselines.

13. **Arrays belong inside groups — agree, with constraints (open point 6).** Reuse [existing constant-bound range semantics](../../../docs/odex.md#L362), expansion limits and diagnostics identifying the expanded index. Reject overlaps such as an explicit `x3` plus a range that assigns `x3`, rather than making textual order decide. However, “no new grammar” is inaccurate: expression-valued entries and per-entry ranges extend the options block grammar. Specify those extensions explicitly and use one parser.

14. **Export/import helps scale, but manual wiring is insufficient — modify (open point 10).** Support searchable groups with source labels, optional descriptions, and an “Export and add import” operation. Exports must either materialize complete values or explicitly preserve their base/dependencies; a timestamp comment does not make a partial overlay portable. Rejected options 1 and 4 deserve to return as supported workflows—one typed group per `.odex` file—without extra extensions or mandatory fragmentation. I agree with rejecting option 5’s compulsory external files: it burdens small models unnecessarily.

15. **The window needs visible working-state semantics — modify; reconsider rejection 6’s rationale.** Show `bursting — modified`, inherited versus explicit values, and whether displayed state means initial, current or ending values; [the session format already distinguishes these](../../../docs/protocol.md#L1634). Label actions “Save group”, “Save group as…” and “Export group…”. Preview affected fields before applying mixed bundles. Named defaults are reasonable, but ordinary `par a=1` declarations are not inherently error-prone; ambiguity comes from undocumented precedence. An explicitly marked inline-default form deserves reconsideration if declaration duplication proves cumbersome.

Only the maintainer can decide:

- Whether preserving existing named-set behaviour outweighs having only default-based selection.
- Whether separate parameter declarations justify their authoring overhead.
- Whether settings presets may affect independently selected sections.
- Whether shared imported files are writable through ordinary Save.
- Whether migration of existing user models and archives is a release requirement.

### Round 2: Claude

[W309 design review, round 2: Claude's reply to your round 1]

Thank you: most of this lands. My position on each of your points, numbered as yours. Push back again wherever I am wrong; where we agree, say so in one line and move on.

1. Captures vs overlays: AGREE. A group saved from a computed state (after a run, or a grabbed AUTO point) is written complete (every state; with "its parameters", every parameter) with a provenance comment (model, time, source such as "AUTO run hb1, label 4"). Overlays are for authored regimes. Periodic orbits: agree, an init group is a state only; AUTO restarts keep referring to stored solutions (W306 already starts from "a stored point by run + type + index", and .snapx keeps the orbits). Nothing is flattened into init.

2. Patch vs default-based selection: PARTLY DISAGREE. For par and init I want default-based: "hopf" must mean the same values whatever was clicked before. XPPAUT's patch makes the result depend on click order (cool then hopf keeps cool's iapp), which is the kind of hidden history our findings file records as an XPPAUT flaw, not a behaviour to preserve. The break is documented (docs/xppautx-vs-xppaut.md, docs/odex-quirks.md). Evidence of impact: only lecar.ode among the examples uses `set` at all (grep '^set' examples/ode/*.ode), and two of its six sets (vvst, pp) are mostly plot presets. Your atomicity point is right though: a mixed XPPAUT set (vvst sets the plot, total and iapp) must stay one action. I take your bundle idea from point 10 for that: `preset vvst { par: vvst, options: vvst }`, an explicit list of group references (no values), written by the converter for every mixed set. No separate "apply these changes" operation.

3. Save ownership: AGREE. Save checks the file's fingerprint and refuses a conflict ("changed on disk since it was loaded: reload, or Save as elsewhere"), builds the whole new text, parses and validates it, then replaces it through the temp-then-rename Writer. Save writes only into the open model file: a group from an import cannot be saved in place (Save as copies it into the model, or Export). In a .snapx, Save changes the session's embedded model, persisted by Save session, never the original disk model.

4. Scoped imported defaults: AGREE. `import "cell.odex" as c1` brings `c1.`-prefixed groups whose defaults compose inside c1's namespace; an unprefixed import whose default collides with the importer's is an error naming both lines; imported options groups only on explicit selection.

5. Evaluation lifecycle: AGREE on merging the default and selected init maps before evaluating, settings (seed) applied first, each surviving expression evaluated once in declaration order, snapshot restore reading numbers and RNG state without re-evaluating. DISAGREE on marking ICs stale after a parameter edit and offering reinitialisation: that is a UI behaviour the window does not have today and W309 does not need; it can be its own card if users ask. Delay histories stay outside groups (`history` is a model statement).

6. Scripts: AGREE, and it belongs in W306's card: precedence model defaults -> selected groups -> inline overrides; a run's inputs are resolved from the script, never inherited from an earlier run or the window; the whole .autox is validated before anything runs (already decided: all or nothing), and a numerical failure after earlier runs is reported as such.

7. Offered means validated: AGREE fully. Every group is parsed and checked at load (names, types, ranges, expressions' symbols), all or nothing; evaluation errors at selection leave values, settings and RNG untouched; diagnostics name kind, group, key, file, line, column and source; W296's limits apply; an options group naming a path (output, logfile) goes through W286's outside-path question only when selected, never when listed.

8. Settings selection: AGREE. The options default is not "complete": unspecified keys keep the option table's defaults. Selecting an options group resets only the top-level sections it names (integration, continuation, plot, ...) to the default group's values and then applies its entries; other sections stay as they are. Lists (sliders, buttons) are replaced, never appended.

9. Declarations: DISAGREE. The default par group IS the declaration: its keys are the model's parameters, in order; a non-default group naming a name the default does not declare is an error with file and line. That is checkable and not circular: "missing parameter" then simply cannot happen for par (only for init, whose states are declared by their equations, where W313's warning applies). Writing every parameter name twice (`par a, b` plus the default group) is a cost every model pays for a check we already get. A model with no parameters has no par group; a par group there is an error.

10. Same names across kinds: AGREE on (kind, qualified name) identity and typed selectors where ambiguous. The explicit link is the `preset` bundle from point 2: "Save state with its parameters" writes `par rest`, `init rest` and `preset rest { par: rest, init: rest }` in one transaction. Your remark on rejected option 3 is fair: independent reuse does not need separate files, which is exactly why groups in one file won.

11. .setx: AGREE, and this reverses one of the maintainer's decisions, which I will flag: an XPPAUT .set holds graphics, delays, boundary conditions and run position that have no group to go into, so a .set keeps converting to a .setx (as today, W284), the session-settings transfer format; .odex groups are the authored presets.

12. Migration: DISAGREE on a migration tool. AGENTS.md's rule (maintainer's): "No fallbacks: our own files ... load only what they hold ... no code for older files of ours", and we are in beta. The repository's own .odex, .snapx and .recx files (examples, tests) are regenerated or edited by hand in the implementing card; a user's older .odex fails with an error at the line that names the fix ("an unnamed `par` line: write it as a named group, e.g. `par base default { ... }`"). The converter names its defaults `base`. If you think shipping beta users a break with no tool is wrong, argue it: the maintainer decides.

13. Arrays: AGREE, including that the options grammar is extended (expression values, per-entry ranges): stated explicitly in the spec, one parser.

14. Export: AGREE: Export writes complete values (a capture, portable), and offers "Export and add the import" as one action. One typed group per .odex file stays possible as a workflow, no new extension.

15. Window: AGREE: "bursting - modified", inherited vs explicit values, initial vs current vs ending made visible, actions labelled Save group / Save group as... / Export group.... The unnamed-default form was the maintainer's call; I will relay your view.

Your round 2: answer only where you still disagree or where my reply introduced a new problem (the `preset` bundle especially, and the declaration rule in 9). Then list what we now agree on and what remains for the maintainer. Under about 900 words.

### Round 2: Astra

1. **`preset` is the right addition, but its transaction needs defining.** Make it a flat bundle with at most one reference per kind, no nested presets, and no values. Resolve every reference and validate the combined configuration before applying anything; then apply parameters/settings and evaluate initials in the agreed order. Cancellation or failure must preserve values, settings and RNG state. Imported presets must retain their original bindings after qualification, rather than resolving against coincidentally matching names in the importer. Put presets in a visible “Apply preset” control; selecting one through a parameters menu would conceal its effects on other sections.

2. **Default-as-declaration is acceptable; exported value groups expose the remaining ambiguity.** I withdraw the demand for separate declarations: your rule is coherent for a complete model. However, an exported `par rest { ... }` must assign existing parameters when imported, while `par base default { ... }` declares them. The [current importer follows declarations and their dependencies](../../../docs/odex.md#L331), so this distinction must be explicit in the grammar and import rules. Define value-only libraries as allowed to lack defaults, with their targets validated against the consuming model. Importing a capture must never pull in a declaring default and produce duplicate parameters. A non-default parameter group in a complete model without any declaring default should be an error.

3. **Namespaced defaults also need namespaced application scope.** Agree that `c1` and `c2` defaults compose. But selecting `c1.bursting` must reset only `c1`’s parameters to its default, leaving `c2` and the parent untouched. Specify that scope independently of which keys the overlay contains. Likewise, distinguish two errors: duplicate group identity and overlapping default ownership. Disjoint parameter libraries should be composable without being rejected merely because both contain a default. This follows from the [documented multiple-instance import workflow](../../../docs/odex.md#L341).

4. **The compatibility break is defensible; the evidence understates its reach.** I accept deterministic parameter/initial selection and withdraw the patch-operation requirement if the maintainer approves the documented difference. Your grep establishes that only the *remaining `.ode` files* include `lecar` sets. Existing converted models also matter: [r3b.odex](../../../examples/ode/r3b.odex#L13) has a mixed `period` set containing initials, integration settings and plot settings; coaster models have parameter sets. Include those in conversion/migration checks. For settings sections, define the reset baseline as **option-table defaults overlaid by model defaults**, then the selected group. Otherwise unspecified model-default keys could retain history. Document that section-scoped settings selection remains compositional, unlike whole-kind parameter selection.

5. **Deferring a stale-IC indicator is reasonable; deferring the lifecycle contract is not.** I accept keeping that UI feature outside W309. The spec must nevertheless say what parameter-only selection does to already evaluated initials: preserve their numbers until an explicit initial-group selection or initialization operation. Define whether selecting the already selected initial group reevaluates it, including random draws. That supplies W306 and every front end with a usable contract. Fixed `history` statements can remain outside groups, but parameter-dependent histories must use the effective run parameters; a captured instantaneous vector must not be presented as a complete delay-system restart.

6. **Save must update the live group registry as well as the file.** Your ownership and fingerprint rules resolve my main objection. After successful replacement, update group definitions, source locations, provenance and the remembered fingerprint without resetting the working session. Otherwise immediately selecting the saved group could use its old definition, or a second Save could report a false conflict. Also settle whether ordinary Save of an authored expression/overlay preserves that representation or replaces it with complete numeric values. I favour preserving authored structure for ordinary Save and making numeric capture explicit; silent replacement of `v: f(a)` with today’s number loses useful intent.

7. **Migration remains a maintainer tradeoff, not a rule-derived inevitability.** My earlier statement that a separate migration tool automatically respects AGENTS.md was too strong: “no code for older files of ours” may prohibit that tool too. It requires an explicit exception. I still favour a one-shot translator, especially for embedded `.recx`/`.snapx` models. If beta policy rejects it, publish a concrete manual migration guide covering archives and recordings as well as plain declarations, and make the unsupported-format error actionable. “Beta” explains the policy; it does not reduce users’ recovery work.

We now agree on:

- Complete numeric captures; overlays for authored regimes; preserved AUTO restart solutions.
- Save ownership, conflict detection and atomic replacement.
- Typed identities, explicit bundles and transactional validation.
- Settings-before-initial evaluation, snapshot restoration and replacement-list semantics.
- Retaining `.setx`, shared grammar/limits, portable exports and the clarified window labels.
- Deterministic scripted inputs and explicit reporting of numerical failures.

Remaining for the maintainer:

- Approve the named-set compatibility break.
- Choose migration assistance or a documented manual break.
- Decide whether ordinary Save preserves authored expressions and overlays.
- Confirm scoped defaults and the parameter-only initialization contract.

### Round 3: Claude

[W309 design review, round 3: Claude's reply to your round 2]

1. preset: AGREE as you define it: flat, at most one reference per kind, no values, no nesting; all references resolved and the combination validated before anything applies; failure leaves values, settings and RNG untouched; imported presets keep their own bindings after qualification; applied from its own visible "Presets" menu, never hidden inside the parameters menu.

2. Value-only groups: AGREE. A non-default group assigns, a default group declares; `from "f.odex" import bursting` of a non-default group brings no declaration, and its keys must be parameters the consuming model declares (error with both places otherwise). A file holding only non-default groups is a valid import and has no default to require. A non-default par group in a model with no declaring default is an error.

3. Scope: AGREE. Selecting c1.bursting resets c1's parameters to c1's default and applies the overlay; c2 and the parent are untouched. Two errors kept apart: a duplicate group identity, and two defaults owning the same parameter. Disjoint libraries with their own defaults compose.

4. AGREE, and thank you for r3b.odex: existing .odex examples with mixed sets (r3b's `period`, the coaster models) join the conversion checks. Settings baseline: option-table defaults, then the model's default options group, then the selected group, per section.

5. AGREE on the contract: selecting a parameter group leaves the current initial values as numbers; only selecting an initial group (or Initialize) evaluates init expressions, and selecting the same initial group again re-evaluates it (new random draws: it is an explicit action). Parameter-dependent histories use the run's effective parameters; an init capture is never presented as a delay system's full restart.

6. AGREE: a successful Save updates the live groups, their places, provenance and the remembered fingerprint, without resetting the session. Save preserves authored structure: entries the user did not change keep their text (`v: f(a)` stays), entries the user changed become the value set; a numeric capture is the explicit Save as from a computed state. That is my proposal; the maintainer confirms.

7. Migration: agreed it is the maintainer's trade-off; I will put both options to him: a one-shot translator (needs an explicit exception to "no code for older files of ours"), or a manual guide plus actionable errors.

Unless you still disagree with something, reply with one line, "settled", and nothing else is needed. If you disagree, say so in under 200 words.

### Round 3: Astra

settled
