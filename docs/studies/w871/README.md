# W871 Two diagram labels at one point have their codes run together (#953)

Pictures of the autoX diagram of examples/ode/mu26.odex (tests/autox/ode_mu26_control.json's three runs, the key under the
plot), taken by web2check's `labelmarks` section on the page of this card
(`XPP_SHOTS=docs/studies/w871 node tools/web2check.mjs --only labelmarks`).

| Picture | Shows |
|---|---|
| `mu26-diagram-key-1280-light.jpg`, `mu26-diagram-key-1280-dark.jpg` | The desktop width: AH 2 is written above EP 1 (both on one spot, no longer "EP 1AH 2"), and EP 4 and EP 3 keep a clear gap. |
| `mu26-diagram-key-768-light.jpg`, `mu26-diagram-key-768-dark.jpg` | The tablet width: the same placement, EP 1 a line below AH 2. |
| `mu26-diagram-key-375-light.jpg`, `mu26-diagram-key-375-dark.jpg` | The phone width: EP 17 and LPC 16 are written on separate lines. The plot is only about 50 px tall at this width (the page's layout, not this card's change), so its top codes sit at the plot's edge. |

The rule (web2/src/plot/labelPlace.ts `placeLabels`, used by web2/src/plot/diagramChart.ts): a name keeps 2 CSS px clear
of every name already written, on both axes; one that would not is moved a line (its height plus the gap) down, then up.
The check (web2check `labelmarks`, at each of the three widths) reads the drawn name boxes from `__xpp.diagram().nameBoxes`
and fails if any two come closer than 2 px. The unit test is web2/test/autoxView.test.ts "two names side by side that would touch".
