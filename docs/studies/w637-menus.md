# W637: the menus grouped by task

Decided 2026-11-08 by the maintainer from two rounds of mockups of the real page (each menu today and proposed, every submenu open; 1024 and 1280 px light, 1440 px dark, the phone drawer at 390 px). Round 1 was too long in places ("Don't you think some menus are too long?") and kept the header's Tools menu, a second menu beside the menu bar ("are we duplicating ourselves here?"); round 2: "Accepted". Issue #709.

## Rules

- One menu: the menu bar, shown as a drawer on a phone. The header's Tools menu goes: everything in it is in File › Recording or Windows. The header's Values, Data table, Model and Help buttons stay until W644 makes those panels the dock's tabs.
- A command whose home is a view (the graph's row and Kinescope bar, the Table view, the Array plot, Animation and player bars, the autoX window, Values' Sets rows, slider cards, each window's submenu in Windows, the shortcuts editor) leaves the menu bar. Every command stays in Search commands.
- No level holds more than 12 rows.
- Submenus fly out to the side at 48rem and wider, one open at a time, a › at the right of the title; the menu keeps its own height (no max-height or scroll is needed any more). The phone's drawer keeps unfolding them inside.
- A line separates a menu's commands from its submenus, wherever one follows the other; it is derived from the tree, not stored in the rows.
- A home marked NEW below has no control for the command in its view today: W637 adds one there.

## Menu places, in display order

| Command | Label | Today | Decided |
|---|---|---|---|
| `session.open_model` | Open model | File | File |
| `session.reload_model` | Reload model | File | File |
| `session.open` | Open session | File | File |
| `session.save` | Save session | File | File |
| `session.save_as` | Save session as | File | File |
| `session.save_copy` | Save a copy of the session | File | File |
| `session.export_info` | Export simulation information | File | File |
| `set.rename` | Rename set | File/Sets | File/Sets |
| `set.delete` | Delete set | File/Sets | File/Sets |
| `set.load_file` | Load a named part of a file | File/Sets | File/Sets |
| `session.import_xppaut_state` | Import XPPAUT session state | File/Sets | File/Sets |
| `recording.start` | Start recording | File | File/Recording |
| `recording.stop` | Stop recording | File | File/Recording |
| `recording.open` | Play recording | File | File/Recording |
| `plot.export_pdf` | Export graph PDF | File/Curves | File/Export |
| `plot.export_svg` | Export graph SVG | File/Curves | File/Export |
| `plot.export_data` | Export graph data | File/Curves | File/Export |
| `browser.export` | Export data | File | File/Export |
| `animation.export_frames` | Export animation frames | File/Animation | File/Export |
| `session.quit` | Quit | File | File |
| `run.initial` | Run from initial | Run | Run |
| `run.last` | Run from last state | Run | Run |
| `run.old` | Run from stored initial values | Run | Run |
| `run.continue` | Continue | Run | Run |
| `run.backward` | Run backward | Run | Run |
| `run.steady` | Run to steady state | Run | Run |
| `run.abort` | Stop | Run | Run |
| `run.configure` | Run settings | Run | Run |
| `analysis.poincare_map` | Poincare map | Analysis/Numerics | Run |
| `ic.set` | Set initial conditions | Run/Initial conditions | Run/Initial conditions |
| `ic.formula` | Initial value by formula | Run/Initial conditions | Run/Initial conditions |
| `ic.array` | Initial values by array formula | Run/Initial conditions | Run/Initial conditions |
| `ic.load` | Load initial conditions | Run/Initial conditions | Run/Initial conditions |
| `ic.from_data_row` | Initial conditions from data row | Run/Data | Run/Initial conditions |
| `ic.at` | Run from a point | Run/Initial conditions | Run/Initial conditions |
| `ic.mouse_stream` | Run from every point | Run/Initial conditions | Run/Initial conditions |
| `run.dae_guess` | DAE guess | Run/Initial conditions | Run/Initial conditions |
| `run.shoot` | Shoot | Run/Initial conditions | Run/Initial conditions |
| `run.range` | Run over a range | Run/Initial conditions | Run/Over a range |
| `run.double_range` | Run over two parameters | Run/Initial conditions | Run/Over a range |
| `run.shift` | Shift run | Run/Initial conditions | Run/Over a range |
| `bvp.solve` | Solve boundary values | Run/Boundary values | Run/Boundary values |
| `bvp.solve_periodic` | Solve periodic orbit | Run/Boundary values | Run/Boundary values |
| `bvp.solve_range` | Solve over a range | Run/Boundary values | Run/Boundary values |
| `bvp.configure` | Boundary value settings | Run/Numerics | Run/Boundary values |
| `equilibrium.find` | Find equilibrium | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.guess` | Equilibrium from a guess | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.range` | Equilibria over a range | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.search` | Monte Carlo search | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.import_as_ic` | Import as initial conditions | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.export` | Export equilibria | Analysis/Equilibria | Analysis/Equilibria |
| `equilibrium.configure` | Equilibrium settings | Analysis/Equilibria | Analysis/Equilibria |
| `nullcline.compute` | Compute nullclines | Analysis/Nullclines | Analysis/Nullclines |
| `nullcline.redraw` | Restore nullclines | Analysis/Nullclines | Analysis/Nullclines |
| `nullcline.auto_mode` | Nullcline redraw mode | Analysis/Nullclines | Analysis/Nullclines |
| `nullcline.set_mesh` | Nullcline mesh | Analysis/Numerics | Analysis/Nullclines |
| `nullcline.export` | Export nullclines | Analysis/Nullclines | Analysis/Nullclines |
| `nullcline.freeze` | Freeze nullclines | Analysis/Nullclines/Freeze | Analysis/Nullclines/Freeze |
| `nullcline.clear_frozen` | Delete frozen nullclines | Analysis/Nullclines/Freeze | Analysis/Nullclines/Freeze |
| `nullcline.range_freeze` | Freeze nullclines over a range | Analysis/Nullclines/Freeze | Analysis/Nullclines/Freeze |
| `nullcline.animate` | Animate frozen nullclines | Analysis/Nullclines/Freeze | Analysis/Nullclines/Freeze |
| `dirfield.compute` | Compute direction field | Analysis/Direction field | Analysis/Direction field |
| `dirfield.flow` | Flow | Analysis/Direction field | Analysis/Direction field |
| `dirfield.colorize` | Colorize | Analysis/Direction field | Analysis/Direction field |
| `dirfield.clear` | Clear direction field | Analysis/Direction field | Analysis/Direction field |
| `dirfield.export` | Export direction field | Analysis/Direction field | Analysis/Direction field |
| `analysis.histogram` | Histogram | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.histogram_2d` | 2D histogram | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.reload_histogram` | Show last histogram | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.statistics` | Statistics | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.fit_curve` | Fit data | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.lyapunov_exponent` | Lyapunov exponent | Analysis/Numerics/Stochastic | Analysis/Statistics |
| `analysis.fourier_series` | Fourier series | Analysis/Numerics/Stochastic | Analysis/Spectra |
| `analysis.power_spectrum` | Power spectrum | Analysis/Numerics/Stochastic | Analysis/Spectra |
| `analysis.spectral_density` | Spectral density | Analysis/Numerics/Stochastic | Analysis/Spectra |
| `analysis.autocorrelation` | Autocorrelation | Analysis/Numerics/Stochastic | Analysis/Spectra |
| `analysis.cross_correlation` | Cross-correlation | Analysis/Numerics/Stochastic | Analysis/Spectra |
| `analysis.stochastic_integrate` | Compute stochastic runs | Analysis/Numerics/Stochastic | Analysis/Stochastic |
| `analysis.load_stochastic_run` | Show last run data | Analysis/Numerics/Stochastic | Analysis/Stochastic |
| `analysis.load_stochastic_mean` | Show mean | Analysis/Numerics/Stochastic | Analysis/Stochastic |
| `analysis.load_stochastic_variance` | Show variance | Analysis/Numerics/Stochastic | Analysis/Stochastic |
| `analysis.seed_random` | New seed | Analysis/Numerics/Stochastic | Analysis/Stochastic |
| `analysis.adjoint_compute` | Compute adjoint | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_interaction` | Make H function | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_range` | Adjoint over a range | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_load` | Show adjoint | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_load_orbit` | Show orbit | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_load_interaction` | Show H function | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `analysis.adjoint_configure` | Adjoint settings | Analysis/Numerics/Adjoint | Analysis/Adjoint |
| `table.edit` | Edit table | Analysis/Numerics/Tables | Analysis/Tables |
| `table.view_in_browser` | View table | Analysis/Numerics/Tables | Analysis/Tables |
| `analysis.phase_space_torus` | Phase space torus | Analysis/Phase space | Analysis |
| `tool.calculator` | Calculator | Analysis | Analysis |
| `plot.set_variables` | Set variables | Plot | Plot |
| `plot.add_data_markers` | Add data markers | Plot/Annotations | Plot |
| `annotation.delete_all` | Delete all annotations | Plot/Annotations | Plot |
| `plot.variable_vs_time` | Variable vs time | Plot | Plot |
| `plot.set_colormap` | Colormap | Plot/Curves | Plot |
| `analysis.color_coding` | Color by value | Analysis/Numerics | Plot |
| `plot.set_2d` | Set 2D plot | Plot/Axes | Plot/Axes |
| `plot.set_3d` | Set 3D plot | Plot/Axes | Plot/Axes |
| `plot.set_3d_view` | Set 3D view | Plot | Plot/Axes |
| `plot.set_axes_origin` | Axes options | Plot/Curves | Plot/Axes |
| `plot.set_shifted_axes` | Ruelle plot | Plot/Numerics | Plot/Axes |
| `plot.add_curve` | Add curve | Plot/Curves | Plot/Curves |
| `plot.edit_curve` | Edit curve | Plot/Curves | Plot/Curves |
| `plot.delete_curve` | Delete last curve | Plot/Curves | Plot/Curves |
| `plot.delete_all_curves` | Remove all curves | Plot/Curves | Plot/Curves |
| `plot.freeze_curve` | Freeze curve | Plot/Freeze | Plot/Freeze |
| `plot.edit_frozen` | Edit frozen curve | Plot/Freeze | Plot/Freeze |
| `plot.delete_frozen` | Delete frozen curve | Plot/Freeze | Plot/Freeze |
| `plot.delete_all_frozen` | Remove all frozen curves | Plot/Freeze | Plot/Freeze |
| `plot.set_frozen_legend` | Frozen curves key | Plot/Freeze | Plot/Freeze |
| `plot.set_autox_freeze` | Freeze after every run | Plot/Freeze | Plot/Freeze |
| `plot.import_bifurcation` | Import bifurcation diagram | Plot/Freeze | Plot/Freeze |
| `plot.clear_bifurcation` | Clear bifurcation diagram | Plot/Freeze | Plot/Freeze |
| `window.set_bounds` | Set view bounds | Plot/View | Plot/View |
| `window.reset_default` | Default view | Plot/View | Plot/View |
| `window.zoom` | Zoom in | Plot/View | Plot/View |
| `window.zoom_out` | Zoom out | Plot/View | Plot/View |
| `window.scroll` | Scroll view | Plot/View | Plot/View |
| `browser.load` | Load data | File | Data |
| `value.load_file` | Load values file | File/Values | Data |
| `value.set_parameters` | Set parameters | Data | Data |
| `value.set_by_formula` | Set parameter by formula | Data | Data |
| `value.undo` | Undo value edit | Data | Data |
| `value.redo` | Redo value edit | Data | Data |
| `autox.open` | Open autoX | autoX | autoX |
| `autox.run` | Run | autoX | autoX |
| `autox.grab` | Grab | autoX | autoX |
| `autox.configure_numerics` | Numerics | autoX | autoX |
| `autox.configure_plot` | Axes | autoX | autoX |
| `autox.configure_marks` | Mark values | autoX | autoX |
| `session.set_threads` | Threads | autoX | autoX |
| `autox.save_session` | Save diagram | File/autoX | autoX/File |
| `autox.open_session` | Open diagram | File/autoX | autoX/File |
| `autox.import_diagram` | Import XPPAUT diagram | autoX/File | autoX/File |
| `autox.reset_all` | Reset diagram | autoX/File | autoX/File |
| `autox.export_pdf` | Export diagram PDF | File/autoX | autoX/Export |
| `autox.export_svg` | Export diagram SVG | File/autoX | autoX/Export |
| `autox.export_csv` | Export CSV | File/autoX | autoX/Export |
| `autox.export_points` | Write points | File/autoX | autoX/Export |
| `autox.export_all_info` | Write all info | File/autoX | autoX/Export |
| `autox.export_initial_data` | Write initial data | File/autoX | autoX/Export |
| `autox.load_orbit_into_browser` | Load orbit into browser | autoX/File | autoX/Branch |
| `autox.load_branch_into_browser` | Load branch into browser | autoX/File | autoX/Branch |
| `autox.range_branch` | Range over a branch | autoX/File | autoX/Branch |
| `autox.draw_labeled_orbits` | Draw labeled orbits | autoX/File | autoX/Branch |
| `autox.select_2parameter_point` | Select two-parameter point | autoX/File | autoX/Branch |
| `autox.clear_grab` | Clear grab | autoX/File | autoX/Branch |
| `autox.set_auto_redraw` | Automatic diagram redraw | autoX/File | autoX/View |
| `autox.use_last_plot` | Use last plot | autoX/View | autoX/View |
| `autox.default_view` | Default diagram view | autoX/View | autoX/View |
| `autox.zoom` | Zoom diagram in | autoX/View | autoX/View |
| `autox.zoom_out` | Zoom diagram out | autoX/View | autoX/View |
| `autox.scroll` | Scroll diagram | autoX/View | autoX/View |
| `tool.values` | Values | Data | Windows |
| `tool.browser` | Data table | Data | Windows |
| `session.get_source` | Model source | File | Windows |
| `window.create` | New graph | Windows | Windows |
| `autox.new_view` | New autoX view | Windows | Windows |
| `animation.open` | Open animation | Windows | Windows |
| `array_plot.open` | Open array plot | Windows | Windows |
| `tool.command_palette` | Command palette | Help | Help |
| `tool.keyboard_shortcuts` | Keyboard shortcuts | Help | Help |
| `tool.manual` | Manual | Help | Help |

## Leaving the menu bar, by their home

| Home | Command | Label | Today |
|---|---|---|---|
| graph row | `window.redraw` | Redraw plot | Plot |
| graph row | `window.clear` | Clear plot | Plot |
| graph row | `window.destroy_all` | Close extra graphs | Windows/Window |
| graph row | `window.send_to_bottom` | Send to bottom | Windows/Window |
| graph row | `window.set_redraw_mode` | Redraw mode | Plot |
| graph row | `window.set_simultaneous_plot` | Simultaneous plot | Plot |
| graph row | `annotation.add_text` | Add text | Plot/Annotations |
| graph row | `annotation.add_arrow` | Add arrow | Plot/Annotations |
| graph row | `annotation.add_pointer` | Add pointer | Plot/Annotations |
| graph row | `annotation.add_marker` | Add marker | Plot/Annotations |
| graph row (Kinescope bar) | `kinescope.capture` | Capture frame | File/Kinescope |
| graph row (Kinescope bar) | `kinescope.play` | Play kinescope | File/Kinescope |
| graph row (Kinescope bar) | `kinescope.reset` | Reset kinescope | File/Kinescope |
| graph row (Kinescope bar) | `kinescope.export_gif` | Make kinescope GIF | File/Kinescope |
| NEW control in its view (none today): Kinescope bar | `kinescope.autoplay` | Autoplay kinescope | File/Kinescope |
| NEW control in its view (none today): Kinescope bar | `kinescope.export_frames` | Save kinescope frames | File/Kinescope |
| the graph itself (Fit, drag to zoom, axis dialog) | `window.fit` | Fit to data | Plot/View |
| the graph itself (Fit, drag to zoom, axis dialog) | `window.set_display` | Set display | Plot/View |
| the text or mark on the graph (double-click), Annotations panel | `annotation.edit` | Change annotation | Plot/Annotations/Edit |
| the text or mark on the graph (double-click), Annotations panel | `annotation.move` | Move annotation | Plot/Annotations/Edit |
| the text or mark on the graph (double-click), Annotations panel | `annotation.delete` | Delete annotation | Plot/Annotations/Edit |
| the text or mark on the graph (double-click), Annotations panel | `annotation.list` | List annotations | Plot/Annotations |
| Table view toolbar | `browser.find` | Find | Data |
| Table view toolbar | `browser.replace_column` | Replace | Data |
| Table view toolbar | `browser.undo_replace` | Unreplace | Data |
| Table view toolbar | `browser.tabulate_column` | Tabulate column | Data |
| Table view toolbar | `browser.set_slice_start` | First row | Data |
| Table view toolbar | `browser.set_slice_end` | Last row | Data |
| Table view toolbar | `browser.restore_full_slice` | Restore rows | Data |
| Table view toolbar | `browser.add_column` | Add column | Data |
| Table view toolbar | `browser.delete_column` | Delete column | Data |
| Table view toolbar | `browser.transpose` | Transpose data | Data |
| Table view toolbar | `browser.page` | Browser page | Data |
| Array plot toolbar | `array_plot.redraw` | Redraw array plot | Windows/Array plot |
| Array plot toolbar | `array_plot.configure` | Edit array plot | Windows/Array plot |
| Array plot toolbar | `array_plot.fit` | Fit array plot | Windows/Array plot |
| Array plot toolbar | `array_plot.range` | Array plot range | Windows/Array plot |
| Array plot toolbar | `array_plot.export_pdf` | Export array plot PDF | File/Array plot |
| Array plot toolbar | `array_plot.export_gif` | Export array plot GIF | File/Array plot |
| Array plot toolbar | `array_plot.scroll` | Scroll array plot | Windows/Array plot |
| Animation view | `animation.load` | Load animation | Windows/Animation |
| Animation view | `animation.play` | Play animation | Windows/Animation |
| Animation view | `animation.pause` | Pause animation | Windows/Animation |
| Animation view | `animation.step` | Step animation | Windows/Animation |
| Animation view | `animation.seek` | Seek animation | Windows/Animation |
| Animation view | `animation.set_speed` | Set animation speed | Windows/Animation |
| Animation view | `animation.reset` | Reset animation | Windows/Animation |
| Animation view | `animation.grab_point` | Grab point | Windows/Animation |
| NEW control in its view (none today): Animation view | `animation.faster` | Speed up animation | Windows/Animation |
| NEW control in its view (none today): Animation view | `animation.slower` | Slow down animation | Windows/Animation |
| NEW control in its view (none today): Animation view | `animation.set_frame_skip` | Set frame skip | Windows/Animation |
| NEW control in its view (none today): Animation view | `animation.set_live_draw` | Draw animation live | Windows/Animation |
| Recording player bar | `recording.play` | Play | File/Recording |
| Recording player bar | `recording.pause` | Pause | File/Recording |
| Recording player bar | `recording.step` | Step | File/Recording |
| Recording player bar | `recording.seek` | Seek | File/Recording |
| Recording player bar | `recording.close` | Close recording | File/Recording |
| Recording player bar | `recording.set_step_note` | Set step note | File/Recording |
| Recording player bar | `recording.add_note` | Add note | File/Recording |
| NEW control in its view (none today): Player bar | `recording.set_speed` | Set speed | File/Recording |
| autoX window (toolbar, click on the diagram) | `autox.hide_earlier` | Clear | autoX |
| autoX window (toolbar, click on the diagram) | `autox.redraw` | Redraw diagram | autoX |
| autoX window (toolbar, click on the diagram) | `autox.pick` | Pick point | autoX |
| autoX window (toolbar, click on the diagram) | `autox.set_display` | Set diagram display | autoX/View |
| autoX window (toolbar, click on the diagram) | `autox.fit` | Fit diagram | autoX/View |
| Values panel (each group's Sets row) | `set.apply` | Apply set | File/Sets |
| Values panel (each group's Sets row) | `set.save_new` | Save as new set | File/Sets |
| Values panel (each group's Sets row) | `set.save_into` | Replace in set | File/Sets |
| Values panel (each group's Sets row) | `set.save_defaults` | Save as defaults | File/Sets |
| Values panel (each group's Sets row) | `set.restore_defaults` | Restore defaults | File/Sets |
| Values panel (each group's Sets row) | `set.file_contents` | Load from file | File/Sets |
| Values panel (each group's Sets row) | `set.export` | Export set | File/Sets |
| slider card under the plot | `value.slide` | Slide value | Data/Sliders |
| slider card under the plot | `value.configure_slider` | Configure slider | Data/Sliders |
| slider card under the plot | `value.remove_slider` | Remove slider | Data/Sliders |
| each window's own submenu in Windows | `window.select` | Select window | Windows/Window |
| each window's own submenu in Windows | `window.rename` | Rename window | Windows/Window |
| each window's own submenu in Windows | `window.hide` | Hide window | Windows/Window |
| each window's own submenu in Windows | `window.show` | Show window | Windows/Window |
| each window's own submenu in Windows | `window.destroy` | Destroy window | Windows/Window |
| Keyboard shortcuts editor | `keymap.get` | Get keymap | Help/Keymap |
| Keyboard shortcuts editor | `keymap.set` | Set keymap | Help/Keymap |
| Keyboard shortcuts editor | `keymap.reset` | Reset keymap | Help/Keymap |
| protocol and palette only | `help` | Command reference | Help |
| protocol and palette only | `events.subscribe` | Subscribe to events | Help/Events |
| protocol and palette only | `events.unsubscribe` | Unsubscribe from events | Help/Events |
| protocol and palette only | `session.get_state` | Get session state | File/Session |
| protocol and palette only | `window.list` | List windows | Windows/Window |
| protocol and palette only | `tool.run_source_action` | Run source action | Help/Tool |
| protocol and palette only | `tool.run_user_button` | Run user button | Help/Tool |
