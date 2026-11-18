# W808: which options are advanced (#889)

Rows read from core/model_options.cpp in `option_groups` order (159 .odex options; import-only rows have no key and no panel place). Label = the key's last segment, underscores shown as spaces; a dotted key is a titled section of its panel.

**The rule.** An option is *advanced* when it tunes how a method works inside (a tolerance, an iteration cap, a step floor or ceiling, a Jacobian increment or band width, a buffer size, a mesh or grid resolution of an internal search): its default is right for almost every model and changing it only tunes speed or convergence. It is *basic* when it decides what is computed or shown (the method, the time span, dt, what is stored, a range swept, a variable, a plot, a file). One boolean per row (`advanced`) in the option table; a panel folds its advanced rows under "More" at its bottom, keeping the group and the order; the .odex and the protocol are unchanged.

**New row (W804).** The event crossing's smallest step (today `NumericsSettings::stol`, model_numerics.h) is advanced: it is a step floor of the event locator (integration group, section `step`, next to step.min). It is not in the table yet, so it is not counted below.

"doubtful" marks a row the rule classifies, but whose users often change it, or whose class depends on how one reads the rule; the reason follows.


## integration

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `total_time` | Total time | basic | Decides what is computed, shown or written: how long a run integrates, in model time. |
| `start_time` | Start time | basic | Decides what is computed, shown or written: the time a run starts at. |
| `dt` | Dt | basic | Decides what is computed, shown or written: the time step of a fixed-step method, and the first step of an adaptive one. |
| `store_every` | Store every | basic | Decides what is computed, shown or written: keep one step of every this many in the data table. **Doubtful:** Output thinning, basic like output; not tuning. |
| `store_from` | Store from | basic | Decides what is computed, shown or written: keep no data before this time; the run before it is a transient. |
| `storage_precision` | Storage precision | advanced | Only changes how many bytes a stored value takes; the numbers computed are the same. **Doubtful:** The XPPAUT-compat choice (float vs double) changes the digits a user reads from the table, and users compare against XPPAUT. |
| `state_limit` | State limit | basic | Decides what is computed, shown or written: a state beyond plus or minus this value stops the run. **Doubtful:** A guard (stop when the run blows up), like search_limit; but the run it stops is a result the user sees, so basic. |
| `stop_at_limit` | Stop at limit | basic | Decides what is computed, shown or written: whether a state beyond the state limit stops the run, or only marks it. **Doubtful:** Same as state_limit. |
| `method` | Method | basic | Decides what is computed, shown or written: the integration method. |
| `subsystem_split` | Subsystem split | advanced | Decides how the integrator partitions work inside; the results do not change. |
| `compile` | Compile | advanced | Chooses compiled code or the interpreter: speed only. |
| `seed` | Seed | basic | Decides what is computed, shown or written: the seed of the random numbers a run draws; the same seed gives the same run. **Doubtful:** Looks like internal tuning, but it decides which random run a user gets: basic by the rule. |
| `step.relative_tolerance` | Relative tolerance | advanced | An accuracy knob of the method; the default suits nearly every model. **Doubtful:** Users of adaptive methods change it often to check a result (the first thing to try on a stiff or sensitive run). |
| `step.absolute_tolerance` | Absolute tolerance | advanced | Same as the relative tolerance: tunes the step controller. **Doubtful:** Changed together with the relative tolerance, so they should fold together. |
| `step.min` | Min | advanced | Step floor of the controller: it only decides when the method gives up. |
| `step.max` | Max | advanced | Step ceiling of the controller: bounds speed and smoothness, not what is computed. **Doubtful:** A model with a fast forcing (a pulse train) needs it to not step over events; a user who meets that changes it. |
| `jacobian.kind` | Kind | advanced | How an implicit method gets its Jacobian (differences or exact): speed and convergence. |
| `jacobian.band_upper` | Band upper | advanced | Band width of the Jacobian storage: internal. |
| `jacobian.band_lower` | Band lower | advanced | Band width of the Jacobian storage: internal. |
| `delay.max_delay` | Max delay | advanced | Sizes the delay history buffer. **Doubtful:** It is a property of the model (its longest delay), and a too-small value is an error a user must fix; the rule says buffer size, the effect is correctness. |
| `delay.history_points` | History points | advanced | Size of the Volterra history buffer. |
| `delay.real_guess` | Real guess | advanced | Search window of the delay equilibrium eigenvalue search: tunes a search. |
| `delay.imaginary_guess` | Imaginary guess | advanced | Search window of the delay eigenvalue search: tunes a search. |
| `delay.grid` | Grid | advanced | Grid of the delay eigenvalue search: tunes a search. |
| `euler.tolerance` | Tolerance | advanced | Newton tolerance of the implicit Euler step: iteration tuning. |
| `euler.iterations` | Iterations | advanced | Newton iteration cap of the implicit Euler step. |
| `boundary_value.max_iterates` | Max iterates | advanced | Newton iteration cap of a shooting solve. |
| `boundary_value.tolerance` | Tolerance | advanced | Stopping tolerance of the shooting solve. |
| `boundary_value.epsilon` | Epsilon | advanced | Difference step of the shooting Jacobian. |
| `boundary_value.range_parameter` | Range parameter | basic | Decides what is computed, shown or written: the parameter Range shoot varies. |
| `boundary_value.range_steps` | Range steps | basic | Decides what is computed, shown or written: how many steps Range shoot makes between its ends. |
| `boundary_value.range_low` | Range low | basic | Decides what is computed, shown or written: the value Range shoot starts at. |
| `boundary_value.range_high` | Range high | basic | Decides what is computed, shown or written: the value Range shoot ends at. |
| `boundary_value.range_side` | Range side | basic | Decides what is computed, shown or written: which end of the interval Range shoot stores the solution's values at. |
| `boundary_value.range_cycle_color` | Range cycle color | basic | Decides what is computed, shown or written: whether each solution of Range shoot is drawn in the next color. |
| `torus.period` | Period | basic | Decides what is computed, shown or written: the period of the variables that live on a circle or torus. |
| `torus.fold` | Fold | basic | Decides what is computed, shown or written: a variable that is taken modulo the torus period; each names one. |
| `torus.on` | On | basic | Decides what is computed, shown or written: whether the variables folded onto the torus are taken modulo its period. |

## poincare

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `map` | Map | basic | Decides what is computed, shown or written: what a run records at a Poincare section: nothing, the section, the maxima or the period. |
| `variable` | Variable | basic | Decides what is computed, shown or written: the variable the Poincare section is taken on. |
| `sign` | Sign | basic | Decides what is computed, shown or written: which crossings of the section count: upward, downward or both. |
| `plane` | Plane | basic | Decides what is computed, shown or written: the value of the variable at the section. |
| `stop` | Stop | basic | Decides what is computed, shown or written: whether a crossing of the section stops the run. |

## equilibria

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `max_iterations` | Max iterations | advanced | Newton iteration cap. |
| `newton_tolerance` | Newton tolerance | advanced | Newton tolerance. **Doubtful:** A user who finds no equilibrium may loosen it. |
| `jacobian_step` | Jacobian step | advanced | Difference step of the Jacobian. |
| `search_limit` | Search limit | advanced | Bounds where the search wanders; a guard, not a question asked. **Doubtful:** XPPAUT sets it together with the integration state limit (BOUND), which is basic: the two would sit in different folds. |
| `print_eigenvalues` | Print eigenvalues | basic | Decides what is computed, shown or written: whether the eigenvalues of an equilibrium are printed. |
| `draw_invariant_sets` | Draw invariant sets | basic | Decides what is computed, shown or written: whether the stable and unstable sets of a saddle are drawn. |
| `draw_strong_sets` | Draw strong sets | basic | Decides what is computed, shown or written: whether the strong stable and unstable sets are drawn. |
| `range_parameter` | Range parameter | basic | Decides what is computed, shown or written: the parameter Range equilibria varies. |
| `range_steps` | Range steps | basic | Decides what is computed, shown or written: how many steps Range equilibria makes between its ends. |
| `range_low` | Range low | basic | Decides what is computed, shown or written: the value Range equilibria starts at. |
| `range_high` | Range high | basic | Decides what is computed, shown or written: the value Range equilibria ends at. |
| `range_shoot` | Range shoot | basic | Decides what is computed, shown or written: whether Range equilibria finds each equilibrium by shooting from the last one. |
| `range_stability_column` | Range stability column | basic | Decides what is computed, shown or written: the data column Range equilibria stores each equilibrium's stability in. |

## sweep

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `variable` | Variable | basic | Decides what is computed, shown or written: the parameter or variable a sweep varies. |
| `steps` | Steps | basic | Decides what is computed, shown or written: how many steps the sweep makes between its ends. |
| `low` | Low | basic | Decides what is computed, shown or written: the value the sweep starts at. |
| `high` | High | basic | Decides what is computed, shown or written: the value the sweep ends at. |
| `reset` | Reset | basic | Decides what is computed, shown or written: whether each run of the sweep starts from the initial conditions. |
| `old_ic` | Old ic | basic | Decides what is computed, shown or written: whether each run of the sweep starts where the one before ended. |
| `write_files` | Write files | basic | Decides what is computed, shown or written: whether each run of the sweep is written to a file of its own. |
| `second_variable` | Second variable | basic | Decides what is computed, shown or written: the parameter or variable the second sweep of a two-variable range varies. |
| `second_low` | Second low | basic | Decides what is computed, shown or written: the value the second sweep starts at. |
| `second_high` | Second high | basic | Decides what is computed, shown or written: the value the second sweep ends at. |
| `cycle_color` | Cycle color | basic | Decides what is computed, shown or written: whether each run of the sweep is drawn in the next color. |

## continuation

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `mesh_intervals` | Mesh intervals | advanced | Resolution of the periodic orbit mesh. **Doubtful:** A user whose orbit is not resolved (a relaxation oscillation) raises it: the commonest autoX change after the parameter range. |
| `max_steps` | Max steps | basic | Decides what is computed, shown or written: the most continuation steps a run takes before it stops. |
| `output_every` | Output every | basic | Decides what is computed, shown or written: a diagram point is kept every this many steps. **Doubtful:** Keeps one point in N; like store_every (basic) it decides what is kept. |
| `collocation_points` | Collocation points | advanced | Discretisation inside autoX; the default is right for almost every model. |
| `min_step_size` | Min step size | advanced | Step floor of the continuation controller. **Doubtful:** Often lowered when a run stops at "step too small"; the stop message names it. |
| `max_step_size` | Max step size | advanced | Step ceiling of the continuation controller. **Doubtful:** Often lowered to get more points on a branch. |
| `step_size` | Step size | advanced | First step of the continuation. **Doubtful:** Sign and size choose where the branch starts going; users set it when they reverse direction. |
| `parameter_min` | Parameter min | basic | Decides what is computed, shown or written: a continuation run stops when the parameter falls below this. |
| `parameter_max` | Parameter max | basic | Decides what is computed, shown or written: a continuation run stops when the parameter rises above this. |
| `norm_min` | Norm min | basic | Decides what is computed, shown or written: a continuation run stops when the solution's norm falls below this. |
| `norm_max` | Norm max | basic | Decides what is computed, shown or written: a continuation run stops when the solution's norm rises above this. |
| `parameter_tolerance` | Parameter tolerance | advanced | Newton convergence tolerance of the parameter. |
| `state_tolerance` | State tolerance | advanced | Newton convergence tolerance of the state. |
| `special_point_tolerance` | Special point tolerance | advanced | Locating tolerance of special points. |
| `mesh_adapt_every` | Mesh adapt every | advanced | How often the mesh is adapted: internal. |
| `max_branch_switches` | Max branch switches | advanced | Cap on branch switches of one run. **Doubtful:** It limits what is found, not how: a run follows fewer branches. |
| `switch_one_direction` | Switch one direction | basic | Decides what is computed, shown or written: whether a branch switch continues in one direction only. **Doubtful:** Chooses which branch is followed: basic. |
| `output_detail` | Output detail | advanced | How much each point writes to the Output: only what is printed. |
| `max_location_iterations` | Max location iterations | advanced | Iteration cap of locating a special point. |
| `max_newton_iterations` | Max newton iterations | advanced | Newton iteration cap of a step. |
| `full_newton_after` | Full newton after | advanced | Jacobian reuse policy of the Newton iteration: speed. |
| `step_adapt_every` | Step adapt every | advanced | How often the step is adapted: internal. |
| `suppress_branch_points` | Suppress branch points | basic | Decides what is computed, shown or written: whether branch points are left out of the diagram. |
| `follow_all_branches` | Follow all branches | basic | Decides what is computed, shown or written: whether a run follows every branch it finds, not only the one it started on. |
| `special_points` | Special points | basic | Decides what is computed, shown or written: the codimension-two points a run detects: a comma-separated list of their names, empty for none. |

## diagram

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `variable` | Variable | basic | Decides what is computed, shown or written: the variable the continuation diagram plots up. |
| `x_min` | X min | basic | Decides what is computed, shown or written: the left edge of the continuation diagram. |
| `x_max` | X max | basic | Decides what is computed, shown or written: the right edge of the continuation diagram. |
| `y_min` | Y min | basic | Decides what is computed, shown or written: the bottom edge of the continuation diagram. |
| `y_max` | Y max | basic | Decides what is computed, shown or written: the top edge of the continuation diagram. |

## plot

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `colormap` | Colormap | basic | Decides what is computed, shown or written: the colors a color-coded curve or an array plot runs through. |
| `curves` | Curves | basic | Decides what is computed, shown or written: the curves of the starting graph, each with the variables on its axes; the first holds the axes' variables. |
| `axes.kind` | Kind | basic | Decides what is computed, shown or written: a two- or three-dimensional plot. |
| `axes.x_min` | X min | basic | Decides what is computed, shown or written: the low end of x: the window's left edge in a two-dimensional plot, the box's in a three-dimensional one. |
| `axes.x_max` | X max | basic | Decides what is computed, shown or written: the high end of x: the window's right edge in a two-dimensional plot, the box's in a three-dimensional one. |
| `axes.y_min` | Y min | basic | Decides what is computed, shown or written: the low end of y: the window's bottom edge in a two-dimensional plot, the box's in a three-dimensional one. |
| `axes.y_max` | Y max | basic | Decides what is computed, shown or written: the high end of y: the window's top edge in a two-dimensional plot, the box's in a three-dimensional one. |
| `axes.z_min` | Z min | basic | Decides what is computed, shown or written: the low end of the three-dimensional box in z. |
| `axes.z_max` | Z max | basic | Decides what is computed, shown or written: the high end of the three-dimensional box in z. |
| `axes.phi` | Phi | basic | Decides what is computed, shown or written: the azimuth the three-dimensional plot is viewed from, in degrees. |
| `axes.theta` | Theta | basic | Decides what is computed, shown or written: the elevation the three-dimensional plot is viewed from, in degrees. |
| `line.draw` | Draw | basic | Decides what is computed, shown or written: whether a new curve is drawn as a line, as points or as both. |
| `line.dash` | Dash | basic | Decides what is computed, shown or written: the dash pattern of a new curve's line. |
| `line.width` | Width | basic | Decides what is computed, shown or written: the width of a new curve's line. |
| `line.marker` | Marker | basic | Decides what is computed, shown or written: the marker drawn at the points of a new curve. |
| `line.marker_size` | Marker size | basic | Decides what is computed, shown or written: the size of the markers of a new curve. |
| `nullclines.mesh` | Mesh | advanced | Resolution of the nullcline search. **Doubtful:** A user with a thin nullcline raises it, and the picture changes with it. |
| `nullclines.draw` | Draw | basic | Decides what is computed, shown or written: whether the nullclines are drawn after a run. |
| `direction_field.grid` | Grid | advanced | Number of arrows a side: resolution of a drawing. **Doubtful:** It changes only what is drawn, which makes it display, not computing; basic by the rule would be right if drawings counted as "what is shown". |
| `direction_field.draw` | Draw | basic | Decides what is computed, shown or written: what a run draws of the direction field, and whether it also saves it. |
| `color_by.enabled` | Enabled | basic | Decides what is computed, shown or written: whether a curve is colored by a quantity along it. |
| `color_by.via` | Via | basic | Decides what is computed, shown or written: the variable a color-coded curve takes its color from. |
| `color_by.low` | Low | basic | Decides what is computed, shown or written: the value of the coloring variable that gets the first color. |
| `color_by.high` | High | basic | Decides what is computed, shown or written: the value of the coloring variable that gets the last color. |
| `array_plot.first_column` | First column | basic | Decides what is computed, shown or written: the first data column the array plot shows. |
| `array_plot.columns` | Columns | basic | Decides what is computed, shown or written: how many columns the array plot shows. |
| `array_plot.first_row` | First row | basic | Decides what is computed, shown or written: the stored row the array plot starts at, counted from 0. |
| `array_plot.rows` | Rows | basic | Decides what is computed, shown or written: how many rows the array plot shows. |
| `array_plot.row_skip` | Row skip | basic | Decides what is computed, shown or written: the array plot shows every this many rows of the stored data. |
| `array_plot.z_min` | Z min | basic | Decides what is computed, shown or written: the value the array plot gives the first color. |
| `array_plot.z_max` | Z max | basic | Decides what is computed, shown or written: the value the array plot gives the last color. |
| `array_plot.top_label` | Top label | basic | Decides what is computed, shown or written: the text above the array plot's PDF picture. |
| `array_plot.side_label` | Side label | basic | Decides what is computed, shown or written: the text beside the array plot's PDF picture, running up its side. |
| `array_plot.bottom_label` | Bottom label | basic | Decides what is computed, shown or written: the text under the array plot's PDF picture. |

## analysis

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `coupling` | Coupling | basic | Decides what is computed, shown or written: the coupling of the H function, one `variable = formula` for each differential equation. |
| `histogram.low` | Low | basic | Decides what is computed, shown or written: the low end of the histogram's range. |
| `histogram.high` | High | basic | Decides what is computed, shown or written: the high end of the histogram's range. |
| `histogram.bins` | Bins | basic | Decides what is computed, shown or written: how many bins the histogram has. |
| `histogram.column` | Column | basic | Decides what is computed, shown or written: the column of the data the histogram counts. |
| `histogram2.low` | Low | basic | Decides what is computed, shown or written: the low end of the second axis of the two-dimensional histogram. |
| `histogram2.high` | High | basic | Decides what is computed, shown or written: the high end of the second axis of the two-dimensional histogram. |
| `histogram2.bins` | Bins | basic | Decides what is computed, shown or written: how many bins the second axis of the two-dimensional histogram has. |
| `histogram2.column` | Column | basic | Decides what is computed, shown or written: the column on the second axis of the two-dimensional histogram. |
| `spectrum.column` | Column | basic | Decides what is computed, shown or written: the column of the data the spectrum is taken of. |
| `spectrum.column2` | Column2 | basic | Decides what is computed, shown or written: the second column of a cross spectrum or a coherence. |
| `spectrum.width` | Width | advanced | Length of the spectrum windows: an estimator tuning. **Doubtful:** It decides the frequency resolution a user reads off; a user who needs a finer spectrum changes it. |
| `spectrum.window` | Window | basic | Decides what is computed, shown or written: the window the spectrum's segments are weighted with. **Doubtful:** A window function is a method choice, but its default is right for most data; kept with the spectrum rows. |
| `transpose.first_column` | First column | basic | Decides what is computed, shown or written: the first data column Transpose takes. |
| `transpose.columns` | Columns | basic | Decides what is computed, shown or written: how many data columns Transpose takes. |
| `transpose.column_skip` | Column skip | basic | Decides what is computed, shown or written: transpose takes every this many columns of the stored data. |
| `transpose.first_row` | First row | basic | Decides what is computed, shown or written: the stored row Transpose starts at, counted from 0. |
| `transpose.rows` | Rows | basic | Decides what is computed, shown or written: how many stored rows Transpose takes. |
| `transpose.row_skip` | Row skip | basic | Decides what is computed, shown or written: transpose takes every this many rows of the stored data. |

## interface

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `sliders` | Sliders | basic | Decides what is computed, shown or written: the sliders the window offers: a parameter or variable each, with its range. |
| `buttons` | Buttons | basic | Decides what is computed, shown or written: the buttons of the window: a label and the command each runs. |
| `animation` | Animation | basic | Decides what is computed, shown or written: the animation file the window plays beside a run. |

## output

| Key | Label | Class | Why (under the rule) |
|---|---|---|---|
| `data_file` | Data file | basic | Decides what is computed, shown or written: the file a run writes its data to, named from the model's folder. |
| `picture_file` | Picture file | basic | Decides what is computed, shown or written: the picture a batch run writes of the graph (PDF or SVG by its extension); empty for none. |
| `run_set` | Run set | basic | Decides what is computed, shown or written: the sets a batch run runs, one run each in the model's order; with none, one run with no set. |
| `nullclines_file` | Nullclines file | basic | Decides what is computed, shown or written: the file the nullclines are saved to after a run. |
| `after_run` | After run | basic | Decides what is computed, shown or written: what a batch run makes of its data once it has run: an analysis, or the mean or variance of its runs. |
| `after_run_file` | After run file | basic | Decides what is computed, shown or written: the file a batch run writes its after-run analysis to. |

## Counts

| Group | Rows | Basic | Advanced | Doubtful |
|---|---|---|---|---|
| integration | 38 | 18 | 20 | 9 |
| poincare | 5 | 5 | 0 | 0 |
| equilibria | 13 | 9 | 4 | 2 |
| sweep | 11 | 11 | 0 | 0 |
| continuation | 25 | 10 | 15 | 7 |
| diagram | 5 | 5 | 0 | 0 |
| plot | 34 | 32 | 2 | 2 |
| analysis | 19 | 18 | 1 | 2 |
| interface | 3 | 3 | 0 | 0 |
| output | 6 | 6 | 0 | 0 |
| **all** | 159 | 117 | 42 | 22 |

## Groups that would be all basic or all advanced

- All basic (no More control at all): poincare, sweep, diagram, interface, output. Their panels stay as they are; the rule finds nothing to fold in a run range, a Poincare section, axes, the buttons or the output files.
- All advanced: none. The largest advanced share is integration (20 of 38) and continuation (15 of 25); in both the basic rows stay on top, so More never hides what a run needs.

Other notes: integration's Step, Jacobian, Delay, Euler and Boundary value sections would be entirely advanced except `boundary_value.range_*` (a Range shoot sweep, basic) and `step.*` stays one section split in two by the fold, so a folded row keeps its section title inside More. Sections that fall wholly into More (Jacobian, Euler, Delay) show their title only there. A method-specific section the current method does not use is already under "Options not used by this method or model"; More is a second, different fold (by user, not by method), and a row that is both (euler.* with RK4) should sit in the first, as today. The two folds should read as one pattern: same disclosure, one inside the other when needed.

## Folding (decided 2026-11-17, replaces the single "More" fold)

The single More fold split a section in two (basic Euler beside advanced Euler) and is dropped. Every section (fieldset, a dotted key's first segment) of a panel is collapsible on its own, its title the toggle (a chevron, the title and its option count stay readable when folded). A section starts folded when every option in it is advanced, or when none of its options apply to the current method; a mixed section starts open; any section opens with a click. Rows outside any section (a panel's top rows) are always shown. This one pattern replaces the "Options not used by this method or model" disclosure: an unused section is a folded section.

Which sections start folded, by the rule (this table, with the classes above):

| Panel | Sections | Start folded | Start open (mixed or basic) |
|---|---|---|---|
| integration | step, jacobian, delay, euler, boundary_value, torus | step (4 advanced), jacobian (3), delay (4), euler (2): all advanced | boundary_value (3 advanced, 6 basic Range shoot rows), torus (2 basic) |
| plot | axes, line, nullclines, direction_field, color_by, array_plot | none | all: nullclines and direction_field hold one advanced row beside a basic one |
| analysis | histogram, histogram2, spectrum, transpose | none | all: spectrum holds one advanced row (width) |
| equilibria, continuation, poincare, sweep, diagram, interface, output | none (no dotted keys) | nothing to fold | the rows are all top rows, always shown; the advanced ones stay in the list in the table's order |

Method rule: a section no option of which the current method uses also starts folded. In the mockup (lecar, runge_kutta) euler and jacobian count as unused; the page's own used/unused answer is not wired in this model, so it was simulated. Under backward_euler both are used, but both are all advanced, so by the literal rule they still start folded (see the mockup `integration-backward_euler-*`): a section that is all advanced and used by the chosen method stays folded; if a method's own section should open when chosen, the rule needs a third clause.

Doubtful classes matter here: moving step.relative_tolerance or step.max to basic would make Step a mixed section that starts open. Equilibria and continuation have no sections, so the rule folds nothing in their panels (continuation, 15 of 25 advanced, would want its own sections to profit).

## Proposed sections (W809)

The maintainer asked to see sections for the two unsectioned panels (mockups `continuation-*` and `equilibria-sections-*`, injected in the page; the keys would gain the dotted section, labels drop the section word as Integration's do).

| Panel | Top rows (always shown) | Section | Rows | Starts |
|---|---|---|---|---|
| continuation | max_steps, parameter_min, parameter_max, norm_min, norm_max, max_branch_switches, switch_one_direction, suppress_branch_points, follow_all_branches, special_points | step | step_size, min_step_size, max_step_size, step_adapt_every | folded (all advanced) |
| | | mesh | mesh_intervals, collocation_points, mesh_adapt_every | folded |
| | | newton | parameter_tolerance, state_tolerance, special_point_tolerance, max_newton_iterations, max_location_iterations, full_newton_after | folded |
| | | output | output_every, output_detail | open (mixed: output_every basic) |
| equilibria | print_eigenvalues, draw_invariant_sets, draw_strong_sets | range | the six range_* rows | open (basic) |
| | | newton | max_iterations, newton_tolerance, jacobian_step, search_limit | folded |

Seen in the shots: the top block of Continuation is long (10 rows) but is what a run needs; three of four sections start folded so the panel is one screen at 768 and 1440. Dropping the section word makes "Full newton after" read "Full after" and "Max newton iterations" "Max iterations" (the section title says Newton): the first wants a better label ("Full after" alone is vague). Dark and light, 390/768/1440: no cut text, overlap or horizontal scroll.

## Layout v3: one help column and paired rows (mockups `*-v3-*`)

1. The help "?" is one column at the end of each row, after the input (on a phone, under the label at the input's right end): the shots' ? buttons share one x at each width (checked by script in every shot: 361, 642, 978 px right edges at 390, 768, 1440).
2. Paired rows by naming rule, over each section's rows (same group and section): `<stem>_min`/`<stem>_max`, `min_<stem>`/`max_<stem>`, `<stem>_low`/`<stem>_high`, or `min`/`max`, `low`/`high` with an empty stem. One label (the stem, else the section's name, else the group's), two inputs side by side, each with its own placeholder and a small "min"/"max" caption, the first row's help button. Every pair the rule makes in the option table:

| Group | Keys | Label |
|---|---|---|
| integration | `step.min` / `step.max` | Step (section name) |
| integration | `boundary_value.range_low` / `boundary_value.range_high` | Range |
| equilibria | `range_low` / `range_high` | Range |
| sweep | `low` / `high` | Sweep (group name) |
| sweep | `second_low` / `second_high` | Second |
| continuation | `min_step_size` / `max_step_size` | Step size |
| continuation | `parameter_min` / `parameter_max` | Parameter |
| continuation | `norm_min` / `norm_max` | Norm |
| diagram | `x_min` / `x_max`, `y_min` / `y_max` | X, Y |
| plot | `axes.x_min` / `axes.x_max`, `y`, `z` | X, Y, Z |
| plot | `color_by.low` / `color_by.high` | Color by |
| plot | `array_plot.z_min` / `array_plot.z_max` | Z |
| analysis | `histogram.low` / `histogram.high`, `histogram2.low` / `histogram2.high` | Histogram, Histogram2 |

Where the rule gets it wrong or needs care:
- `step.min`/`step.max` is labelled "Step" inside the Step section, which says nothing: the section name as a stem is a poor label; "Size" or "Min and max" reads better for these (a label rule, not a pairing one).
- Continuation's step section then shows "Size" (step_size), "Step size" (the pair) and "Adapt every": "Step size" next to "Size" is confusing; the first should be "First", or the pair "Min and max size".
- `second_low`/`second_high` gets "Second" (the sweep's second variable): the stem names a variable, not a quantity. `sweep.low`/`high` gets "Sweep".
- Pairs of different type or unit would pair wrongly only if they shared a stem; none in the table does. Missed by design: `jacobian.band_upper`/`band_lower`, `start_time`/`total_time`, `store_from`, `real_guess`/`imaginary_guess` (no min/max/low/high word).
- Phone: two inputs at 390 px leave no room for the inline validation text ("above 0" cut the value to "1e-0"); the mockup hides it inside pairs below 48rem (a violated value still shows its error under the row). Wider, each input keeps its hint.
- The pair shares one help button (the first row's); the second option's help line is not reachable from it, so a pair's help should list both lines.

## Switch rows (mockups `*-v4a-*`, `*-v4b-*`, on top of v3)

- **A, one line each** (`v4a`): label, switch (right edge of the input column, left of the ?), then the ? in its column. On a phone the label stays beside the switch and a long label wraps beside it, never above it. The ? column keeps its x. Continuation (Switch one direction, Suppress branch points, Follow all branches), Equilibria (Print eigenvalues, Draw invariant sets, Draw strong sets) and Integration (Stop at limit, Compile). Costs nothing to the layout rule; the switch sits at the right so the eye travels from the label across a gap, which is the one weakness (a wide panel at 1440 leaves a long gap in the A form: see its shots).
- **B, shared row** (`v4b`): consecutive switch rows of a section (no other row between) become one wrapping line of items, each a small switch, its label after it and its own ? right after the label (each ? opens its own help, none is lost). At 390 each takes its own line when two do not fit (Continuation), at 768 and 1440 they share lines. Integration has no consecutive switches (Stop at limit and Compile are separated by Method and Subsystem split), so B changes nothing there and has no Integration shots. The group has no ? column: its ?s are not at the common x, a deliberate break of v3 rule 1 for switches.
- Leaning (a design view, not a decision): B saves the most height where switches come in runs (Equilibria's top block shrinks from three rows to two lines at 768); A keeps the one ? column and every row's place predictable. A mixed rule (A, and B only for runs of three or more) was not mocked.
