# W859: the autoX view after a run (#940)

Real page, lecar.odex (set hopf, Find an equilibrium, Initial conditions from it, File/autoX, then one autoX run of
an equilibrium (Branch 1) and one periodic run from the first Hopf point with output every 5 points (Branch 2)).
Captured with tools/cdp.mjs from a scratch script (not in the repo), at 375x812, 768x1024 and 1280x800, light and
dark. The pointer is on Branch 2's row at 768 and 1280 (its accent bar, tint, and its branch drawn thicker).
Design: docs/studies/ui-review-2026-11-23/w859_autox_view.jpg.

| File | Shows |
|---|---|
| autox-view-1280-{light,dark}.jpg | Branch list beside the plot (from 80 rem): its header (how many branches), a row per branch with eye, swatch, name, kind (periodic: "from AH 0.262453") and range, chips (mark, code, value) and the two-line stop text. The toolbar is Run, Grab, Script at the left, Fit and the ⋯ menu at the right; the key is one line; the fit note with Undo is inside the plot's top-left corner; the status is one line with Output (17) at its end. |
| autox-view-768-{light,dark}.jpg | The branch list under the plot (plot 500 px of 1024). |
| autox-view-375-{light,dark}.jpg | The phone: toolbar, a key of two short lines (marks and codes; their names are in the tips), the plot first (about 380 px of 812), the branch list under it, the view scrolling with the status strip kept at the bottom. |

## Differences from the picture

- Run names: the picture's rows are "h up", "h down", "cycles" (a script's run names). The protocol does not say which branches a run
  made (autox.script_runs has run names and Done marks only), so rows are "Branch N". Not done: naming rows from the Script panel.
- Script is off in these captures (no .autox script open); with one open it shows or hides the script's runs, and the branch list's header
  says "N, from the script's M runs" and "Script done".
- The key's mark names are the core's labels ("Andronov-Hopf", "Fold of cycles", "End of branch"), not the picture's shortened ones
  ("Hopf", "fold", "end"); a steady or periodic box is the stable curve's color, while an unstable curve has its own color on the plot
  (black dashed steady, blue dashed periodic, the diagram's palette), which the picture draws in the stable color.
- The status strip's text is the page's own ("Stopped: ... · steady states · branch 1, point 57 · ..."), one line with the whole in its tip,
  not the picture's "Script: 3 runs, 3 branches, 1921 points. ..." summary.
- The plot keeps its corner Fit (the page's T30 control) beside the toolbar's Fit; the picture does not draw it.
- The dark theme's blue (unstable periodic) curve and swatch are the diagram's palette and are faint on the dark background (not changed here).
