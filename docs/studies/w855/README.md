# W855 The autoX Run dialog (#936): screenshots of the real page

`run-dialog-<width>-<light|dark>.jpg`: autoX > Run on lecar.odex with its Hopf label (AH 2) grabbed, at 375, 768 and 1280 px wide.
Drawn from `tools/web2check.mjs --only autox` with `XPP_SHOTS=docs/studies/w855` (pictures only, never a check).

Against the approved picture (../ui-review-2026-11-23/w855_run_dialog.jpg): five steps in the picture's order; the start list names the
label with its mark, type, parameter value and branch ("AH 2 · Andronov–Hopf · iapp = 0.2625 · branch 1"); the kinds are the core's for a
Hopf point, a two-parameter run named "Hopf curve"; Increase / Decrease / Both; Stop outside as one row; numerics one group, open at 1280.
Since the review: the kind a point starts by its type is first and picked (a Hopf point: Periodic orbits; three cards across at 768, one column at 375
and 1280); under the start list, a line names the branch's direction ("Along branch 1, iapp rises from this point."), so a long label never cuts it; the numerics' summary line shows the key values in force and "N changed"
after it; the numerics are compact label-left rows with small fields, their sections side by side at 1280 and one column below; the checkbox rows have the
box before the full-width text; Stop outside's two inputs are styled as the other fields.
Differences that remain: the numerics' sections are of unequal heights (the picture's are even rows of Step | Limits, Mesh | Newton, with the first
section here the unnamed one holding the branch switches); the numerics' help is a tooltip on the field, not a "?"; the dialog keeps no Cancel button (x and Run only: no dialog of the app has one), which the picture shows.
