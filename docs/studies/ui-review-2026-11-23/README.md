# UI review, 2026-11-23: autoX mockups

The maintainer found these problems while using xppautX on a lab model (the pvn_slowfast_hcn script, build 21125d3). The cards
W853-W863 came from that session. Three of them are design-first (AGENTS.md: a UI decision is made by looking at it). These are their
mockups, at docs/ui-v2.md's widths (390, 768 and 1280 px), light and dark, drawn with the page's own tokens
(web2/src/css/tokens.css). They are proposals: none is settled yet.

| Picture | Proposal | Card |
|---|---|---|
| `w855_run_dialog.jpg` | autoX > Run as five steps: where to start (a labelled point or the current state), what to compute (only the kinds that start allows), the parameter with its direction and the range it stops outside, branches (follow what the run finds, clear the diagram first), and numerics as one collapsed group prefilled with the values in force. A full-screen sheet on a phone; two columns at 1280 px, with the numerics shown open. | W855 (#936) |
| `w859_autox_view.jpg` | The view after a run. The plot keeps most of the height, and the run's end fits the axes to the new branches, with Undo. The legend becomes a key line plus a branch list: one row per run with its range, chips for what it found (click one to grab that point) and why it stopped. Output points are faint dots with no number. | W859 (#940) |
| `w863_label_marks.jpg` | One rule for the marks. The shape gives the kind of point. Filled means an equilibrium label and hollow a cycle label (LP ▲, LPC △). A small square is a two-parameter point, and a bar is where a branch ended. The code is drawn beside every special point. The table shows each of the 27 types against today's mark and the types it currently shares it with. | W863 (#945) |

The diagram in the pictures is a sketch of pvn_slowfast_hcn's shape: the fold at hfix = 0.2701, the second fold at -2.731,
the Hopf point at 1.63, the fold of cycles at 9.06 and the SNIC at 0.27. It is not a plot of the computed points.

`mockup-sources/` holds what made them. Each `.dc.html` is one artboard of the claude.ai design canvas the maintainer reviews
(its `canvas.json` is that canvas's index, with Main.dc.html renamed RunDialog.dc.html here). `view.html` draws one without the
canvas, `view.html?f=AutoxView.dc.html`, from a local server or from Chrome started with `--allow-file-access-from-files`. The
pictures were shot that way, in headless Chrome at a 2662 px wide window, then cropped. These files are not code to copy: each
card builds its part in its owner (web2/src/ui, web2/src/plot).
