# Modern navigation design

Approved direction: 2026-10-04. The visible interface is organized by what
people want to do. The existing one-letter shortcuts remain an alternate
input method and no longer determine the layout or capitalization.

## Navigation

The command sidebar stays available and stable while legacy File/Numerics
shortcut states change. It groups actions into **Files**, **Run**,
**Analysis**, **Plot**, and **Tools**, with Run first. Commands use the existing stable
menu identifiers, independent of whichever legacy menu is active. A search
field filters commands by label, description, and existing command identifier. Plain labels
and a separate shortcut hint replace mixed capitalization.

Files contains Open model, Open session, Save session as, Reload model,
Import XPPAUT settings, Export simulation information, and Quit. Native
desktop menus expose the same basic file operations and use the existing
OS dialogs. Ctrl/Cmd+O opens a model; Ctrl/Cmd+S saves a session through
the existing save-as dialog. The label explicitly says Save session as:
there is no remembered destination or silent overwrite in this change.
Recent files and save-in-place require separate persistent document state
and are not invented here.

Run contains integration, continuation, initial conditions, and parameters.
Analysis contains equilibria/stability, nullclines, direction fields/flow,
AUTO continuation, boundary-value problems, stochastic/Poincare/adjoint
tools. Plot contains axes, curves/export, phase space, labels, windows,
redraw, 3D and captured frames. Tools contains calculator, model source,
named parameter sets, preferences, tutorial, recording and playback.

## Workspace

The header identifies the model by filename, with its full path available
as a tooltip. Integrate, Save session as, Data, Model and Help remain
immediate actions. Recording, array plots and animation remain discoverable
in the grouped navigation rather than taking permanent header space.
The plot title remains with the plot itself. Values remain beside the plot
on wide screens and use the existing sheet on narrow screens.

F6 cycles between Commands, Plot, Values and any open Data/Model panel;
Shift+F6 reverses it. Existing skip-to-plot and letter shortcuts remain.
Arrow keys move between command results, and Ctrl/Cmd+K focuses command
search. Dialogs retain focus trapping and Escape cancellation.

## Scientific settings and recovery

The Numerics section identifies the selected solver. Controls that this
solver does not use live in a folded section with a persistent explanation
that they apply only after switching to a method that uses them. Parameters
and initial conditions explain when edits commit and which run uses them.

A working-value checkpoint captures the current parameter and initial
condition values for exploration. Restore returns those values through
existing validated set commands. It does not change the model defaults,
solver settings, data or plots, and is cleared when another model/session
is loaded. Reset continues to mean model defaults. This is a named recovery
action, not a general undo system. Recovery now starts folded.

The right panel prioritizes States, then Parameters. A compact two-column
parameter grid and folded Numerics keep the reference model's two states
and twelve parameters visible together on a desktop. Inspection uses ten
significant digits, with scientific notation for extreme values; focused
edits retain full precision. Final current states come from the core's
double values. Live trajectory samples during integration are float32,
so display digits do not imply ten-digit numerical accuracy.

Each state shows its maximum sampled |Δstate/Δt| over the last ten stored
intervals. This indicates recent motion, not evaluated derivatives or
certified convergence. Missing/invalid samples show a dash. Sampling can
miss changes and float32 quantization can make a small rate zero. Extend
the run and examine the trajectory; interpretation depends on state/time
units. Larger models and narrow screens still require scrolling.

## Visual and implementation constraints

Keep the current palette, type scale, target sizes, responsive layout,
contextual help and numerical engines. Export labels explain what data is
saved rather than how internal delivery works. Menu metadata belongs to
the existing menu owner; command-kind enforcement and recording use the
existing protocol path. Native hooks may request normal application
commands but must not gain arbitrary filesystem authority.

## Task cards

| Card | Work | Dependencies | Acceptance |
|---|---|---|---|
| W186 | Stable menu-item dispatch and plain command labels | None | Actions target their identified menu regardless of current shortcut state; unknown IDs fail; kinds, recording and legacy keys preserved |
| W187 | Grouped searchable navigation and workspace hierarchy | W186 | Every existing command reachable; no sidebar replacement; Files/Run/Analysis/Plot/Tools; primary header emphasizes model/run/save |
| W188 | Native session file actions and conventional shortcuts | W186 | Open/save sessions visible in native menus; OS picker reused; cancel/overwrite semantics preserved; Ctrl/Cmd+O/S |
| W189 | Fast keyboard pane and command navigation | W187 | F6/Shift+F6 cycles visible work areas; Ctrl/Cmd+K searches; arrow navigation; modal focus unaffected |
| W190 | Solver clarity and working-value recovery | W187 | Unused settings explicitly explained; checkpoint captures/restores par/IC values and cannot cross model loads |
| W191 | Validation, export wording, docs and visual review | W187–W190 | Targeted workflow regression gates, native build, numerical consistency, responsive inspection, documented limits |

The UI/UX review in `ui-ux-review-2026-10-04.md` explains the evidence behind
these changes. Task status and issue links live in `roadmap.md`.
