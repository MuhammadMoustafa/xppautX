# W863 Every label type has its own mark (#945)

Pictures of the autoX diagram of examples/ode/mu26.odex (tests/autox/ode_mu26_control.json's three runs: the steady state, its
cycles from the Hopf point, and the cycles to their SNIC), with the key under the plot, taken by web2check's `labelmarks`
section (`XPP_SHOTS=docs/studies/w863 node tools/web2check.mjs --only labelmarks`) on the page of this commit.

| Picture | Shows |
|---|---|
| `mu26-diagram-key-1280-light.jpg`, `mu26-diagram-key-1280-dark.jpg` | The desktop width: the plot with LP 10 (filled triangle), LPC 16 (hollow triangle), SNIC 21 (bar), EP 3/4/17 (bars) and AH 2 (filled circle), and the key with each type's mark and name. |
| `mu26-diagram-key-768-light.jpg`, `mu26-diagram-key-768-dark.jpg` | The tablet width: the same plot and key, the key wrapped over three rows. |
| `mu26-diagram-key-375-light.jpg`, `mu26-diagram-key-375-dark.jpg` | The phone width: the key and the top of the plot. The plot's height is about 50 px at this width (the page's layout, not this card's change). |

Each mark follows the approved picture (`docs/studies/ui-review-2026-11-23/w863_label_marks.jpg`): the fill says
equilibrium (filled) or cycle (hollow), the shape the kind of point, the code is drawn beside every special point.

Open at these pictures (not fixed by this card):
- At 375 px the plot is too short to read its marks, and the codes EP 17 and LPC 16 touch at the top of the plot.
- At every width EP 1 and AH 2 sit on one spot, so their codes run together ("EP 1AH 2"). Their points are the same
  as before this card: the names are placed as they were, only special points now always carry their code.

The tests and checks: `web2/test/labelMarks.test.ts` (the mark rules over the table), web2check's `labelmarks` section
(LP, LPC and SNIC drawn with three different marks, every special point in view named).
