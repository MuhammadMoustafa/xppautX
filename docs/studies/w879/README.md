# W879: the autoX script panel at 375 px

Pictures of the Script panel open on the autoX view (ab.odex with a two-run script, nothing run yet), the page as the web2check `shots` section takes them (`XPP_SHOTS=folder node tools/web2check.mjs --only shots`, `autox-script-open-<width>-<theme>`), at the page's scroll top.

- `autox-script-open-375-light.jpg`, `autox-script-open-375-dark.jpg`: 375 px. Each run's Run sits on its row beside the run's name and its start; the run list scrolls inside its own height (4rem at phone width, 12rem wider) and ends above the status bar. Run all is below the list: at scroll top the sticky status bar still covers its top edge, and it is reached by scrolling the page (checked by autoxScriptFitCheck, section autox: after the page scrolls the panel, Run all clears the status bar).
- `autox-script-open-768-light.jpg`, `autox-script-open-768-dark.jpg`: 768 px, the same panel.
- `autox-script-open-1280-light.jpg`, `autox-script-open-1280-dark.jpg`: 1280 px, the panel beside the branch list, Run and Run all on their lines.

Open item: at 375 x 812 the plot, the branch list and the Point disclosure take about 690 px above the panel, so Run all cannot sit fully above the status bar at scroll top without a shorter plot at phone width (outside this card).
