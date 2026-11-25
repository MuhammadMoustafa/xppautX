# W854 Opening a .autox opens the autoX window with its script (#935)

Pictures of the page right after `ab_runs.autox` (an `.autox` script of `examples/tstauto/ab.odex`'s two runs, `steady` and `back`)
opens, from the web2check autoX section's script shots (`node tools/web2check.mjs --only shots`, XPP_SHOTS set): the whole page at
375, 768 and 1280 wide, light and dark. The autoX tab is the active one at once, its Script panel lists the two runs, nothing has run.

- `open-1280-light.jpg`, `open-1280-dark.jpg`: the desktop width.
- `open-768-light.jpg`, `open-768-dark.jpg`: the tablet width.
- `open-375-light.jpg`, `open-375-dark.jpg`: the phone width. The empty plot's message ("No branches yet ...") wraps inside the plot
  here; before W854's `.plot-empty` change (web2/src/css/plot.css) it ran past the plot's left edge.

Decision (W854): the load itself opens the autoX window when it opens a script (core/xpp_batch.cpp, `load_and_set_up`), so every
way of opening one (the command line, File > Open model, Explorer, Finder, `--server`) reaches the page the same way: `session.state`
carries `autox`, and the page makes the autoX tab active. No command is sent for it.
