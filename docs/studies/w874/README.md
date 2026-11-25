# W874: the page lightens a color too dark for the dark theme (#956)

Pictures of the autoX view after a run with unstable periodic orbits (examples/ode/lecar.odex: its equilibrium run,
then the periodic branch from the Hopf point, so the diagram holds unstable periodic curves drawn in the core's
`#0000ff`). Each whole page at 375, 768 and 1280 px wide, light and dark. Taken by
`XPP_SHOTS=docs/studies/w874 node tools/web2check.mjs --only w874shots` (JPEG, from the real page).

- `autox-periodic-<width>-light.jpg`: the light theme, unchanged: the unstable periodic branch (branch 2's swatch
  and the blue dashed-then-solid curves at about iapp 0.25 and 0.47) is the core's pure blue `#0000ff`.
- `autox-periodic-<width>-dark.jpg`: the dark theme: the same curves drawn lighter blue (`#4d4dff`, the hue and
  saturation kept, the lightness raised to a contrast of 3:1 against the plots' `--surface`, #1b1b1b). Pure blue reads as blue.

Decision (card W874, maintainer 2026-11-25): the core's colors stay as they are; the page lightens a color that is
too dark to read on the dark theme (web2/src/plot/colors.ts `cssColor`). The light theme is unchanged.
