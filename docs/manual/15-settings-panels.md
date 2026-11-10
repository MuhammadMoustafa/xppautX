# The settings panels

Every group of settings has one panel, titled by the group's name, and its rows are in the order of the option table
(the option table), the one source of a setting's key, label, choices, default, limits and help line. A dotted key
(`step.min`) is a titled section inside the panel. The same keys are the `options` block of a `.odex`, the `help` of the program and
the manual below. Each panel opens from the menu entry of its task: Integration from Run > Settings, Poincare from Run > Poincare map,
Sweep from Run > Over a range > One parameter (beside its Run button), Equilibria from Analysis > Equilibria, Analysis inside the
histogram and spectrum commands' forms, Continuation from autoX > Numerics, Diagram from autoX > Axes (it changes the open view; the model's
values only start a new one), Plot from Plot > Axes > Options (it changes the active graph), Export inside the picture export
commands' forms and Output from File > Output, where each path has a Choose file button. The thread count and the log are File > Preferences. Values > Numerics shows the Integration rows beside the plot, each change applying at once.
Apply sets the changed rows together, all or nothing, as one undo step. The values of a panel come from the core when the running command
ends; the interface group (sliders, buttons, animation) has its own editors.


## Integration

- **Total time** (`total_time`): How long a run integrates, in model time. From an XPPAUT .ode: TOTAL.
- **Start time** (`start_time`): The time a run starts at. From an XPPAUT .ode: T0.
- **Dt** (`dt`): The time step of a fixed-step method, and the first step of an adaptive one. From an XPPAUT .ode: DT.
- **Store every** (`store_every`): Keep one step of every this many in the data table. From an XPPAUT .ode: NOUT, NJMP.
- **Store from** (`store_from`): Keep no data before this time; the run before it is a transient. From an XPPAUT .ode: TRANS.
- **Storage precision** (`storage_precision`): What the data table keeps each value as: a single-precision float as XPPAUT does, or the integrator's double.
- **State limit** (`state_limit`): A state beyond plus or minus this value stops the run. From an XPPAUT .ode: BOUND (which also sets equilibria.search_limit): the one value is both limits.
- **Stop at limit** (`stop_at_limit`): Whether a state beyond the state limit stops the run, or only marks it.
- **Method** (`method`): The integration method. From an XPPAUT .ode: METH, METHOD.
- **Subsystem split** (`subsystem_split`): What splits the model into independent subsystems: its equations alone, or also the run (a Poincare section or stop).
- **Compile** (`compile`): Whether the equations run as compiled code or in the interpreter; a change applies when the model next loads.
- **Seed** (`seed`): The seed of the random numbers a run draws; the same seed gives the same run. From an XPPAUT .ode: SEED.

### Step

- **Relative tolerance** (`step.relative_tolerance`): The relative error an adaptive method accepts in one step. From an XPPAUT .ode: TOL.
- **Absolute tolerance** (`step.absolute_tolerance`): The absolute error an adaptive method accepts in one step. From an XPPAUT .ode: ATOL.
- **Min** (`step.min`): The smallest step an adaptive method takes before it gives up. From an XPPAUT .ode: DTMIN.
- **Max** (`step.max`): The largest step an adaptive method takes. From an XPPAUT .ode: DTMAX.

### Jacobian

- **Kind** (`jacobian.kind`): Whether an implicit method gets the Jacobian by differences or from the exact derivatives.
- **Band upper** (`jacobian.band_upper`): The diagonals above the main one that a banded Jacobian keeps. From an XPPAUT .ode: BANDUP.
- **Band lower** (`jacobian.band_lower`): The diagonals below the main one that a banded Jacobian keeps. From an XPPAUT .ode: BANDLO.

### Delay

- **Max delay** (`delay.max_delay`): The longest delay a model uses; it sets how much history is kept. From an XPPAUT .ode: DELAY.
- **History points** (`delay.history_points`): How many past points of the history the Volterra method keeps for an integral equation. From an XPPAUT .ode: VMAXPTS.
- **Real guess** (`delay.real_guess`): The largest real part the search for a delay equilibrium's eigenvalues looks for.
- **Imaginary guess** (`delay.imaginary_guess`): The largest imaginary part the search for a delay equilibrium's eigenvalues looks for.
- **Grid** (`delay.grid`): How many points a side the grid of a delay equilibrium's eigenvalue search has.

### Euler

- **Tolerance** (`euler.tolerance`): The Newton tolerance of the implicit Euler method.
- **Iterations** (`euler.iterations`): The most Newton iterations the implicit Euler method makes in a step.

### Boundary value

- **Max iterates** (`boundary_value.max_iterates`): The most Newton iterates a boundary value solve makes.
- **Tolerance** (`boundary_value.tolerance`): The tolerance a boundary value solve stops at.
- **Epsilon** (`boundary_value.epsilon`): The step that differences the boundary value problem's Jacobian.
- **Range parameter** (`boundary_value.range_parameter`): The parameter Range shoot varies.
- **Range steps** (`boundary_value.range_steps`): How many steps Range shoot makes between its ends.
- **Range low** (`boundary_value.range_low`): The value Range shoot starts at.
- **Range high** (`boundary_value.range_high`): The value Range shoot ends at.
- **Range side** (`boundary_value.range_side`): Which end of the interval Range shoot stores the solution's values at.
- **Range cycle color** (`boundary_value.range_cycle_color`): Whether each solution of Range shoot is drawn in the next color.

### Torus

- **Period** (`torus.period`): The period of the variables that live on a circle or torus. From an XPPAUT .ode: TOR_PER.
- **Fold** (`torus.fold`): A variable that is taken modulo the torus period; each names one. From an XPPAUT .ode: FOLD.
- **On** (`torus.on`): Whether the variables folded onto the torus are taken modulo its period.

## Poincare

- **Map** (`map`): What a run records at a Poincare section: nothing, the section, the maxima or the period. From an XPPAUT .ode: POIMAP.
- **Variable** (`variable`): The variable the Poincare section is taken on. From an XPPAUT .ode: POIVAR.
- **Sign** (`sign`): Which crossings of the section count: upward, downward or both. From an XPPAUT .ode: POISGN.
- **Plane** (`plane`): The value of the variable at the section. From an XPPAUT .ode: POIPLN.
- **Stop** (`stop`): Whether a crossing of the section stops the run. From an XPPAUT .ode: POISTOP.

## Equilibria

- **Max iterations** (`max_iterations`): The most Newton iterations the search for an equilibrium makes. From an XPPAUT .ode: NEWT_ITER.
- **Newton tolerance** (`newton_tolerance`): The tolerance the search for an equilibrium stops at. From an XPPAUT .ode: NEWT_TOL.
- **Jacobian step** (`jacobian_step`): The step that differences the Jacobian of the equilibrium search. From an XPPAUT .ode: JAC_EPS.
- **Search limit** (`search_limit`): A state beyond plus or minus this value ends the equilibrium search. From an XPPAUT .ode: BOUND (which also sets integration.state_limit): the one value is both limits.
- **Print eigenvalues** (`print_eigenvalues`): Whether the eigenvalues of an equilibrium are printed.
- **Draw invariant sets** (`draw_invariant_sets`): Whether the stable and unstable sets of a saddle are drawn.
- **Draw strong sets** (`draw_strong_sets`): Whether the strong stable and unstable sets are drawn.
- **Range parameter** (`range_parameter`): The parameter Range equilibria varies.
- **Range steps** (`range_steps`): How many steps Range equilibria makes between its ends.
- **Range low** (`range_low`): The value Range equilibria starts at.
- **Range high** (`range_high`): The value Range equilibria ends at.
- **Range shoot** (`range_shoot`): Whether Range equilibria finds each equilibrium by shooting from the last one.
- **Range stability column** (`range_stability_column`): The data column Range equilibria stores each equilibrium's stability in.

## Sweep

- **Variable** (`variable`): The parameter or variable a sweep varies. From an XPPAUT .ode: RANGEOVER.
- **Steps** (`steps`): How many steps the sweep makes between its ends. From an XPPAUT .ode: RANGESTEP.
- **Low** (`low`): The value the sweep starts at. From an XPPAUT .ode: RANGELOW.
- **High** (`high`): The value the sweep ends at. From an XPPAUT .ode: RANGEHIGH.
- **Reset** (`reset`): Whether each run of the sweep starts from the initial conditions. From an XPPAUT .ode: RANGERESET.
- **Old ic** (`old_ic`): Whether each run of the sweep starts where the one before ended. From an XPPAUT .ode: RANGEOLDIC.
- **Write files** (`write_files`): Whether each run of the sweep is written to a file of its own. From an XPPAUT .ode: RANGE.
- **Second variable** (`second_variable`): The parameter or variable the second sweep of a two-variable range varies.
- **Second low** (`second_low`): The value the second sweep starts at.
- **Second high** (`second_high`): The value the second sweep ends at.
- **Cycle color** (`cycle_color`): Whether each run of the sweep is drawn in the next color.

## Continuation

- **Mesh intervals** (`mesh_intervals`): How many mesh intervals a periodic orbit or boundary value solution has. From an XPPAUT .ode: NTST.
- **Max steps** (`max_steps`): The most continuation steps a run takes before it stops. From an XPPAUT .ode: NMAX.
- **Output every** (`output_every`): A diagram point is kept every this many steps. From an XPPAUT .ode: NPR.
- **Collocation points** (`collocation_points`): The collocation points in each mesh interval. From an XPPAUT .ode: NCOL.
- **Min step size** (`min_step_size`): The smallest continuation step. From an XPPAUT .ode: DSMIN.
- **Max step size** (`max_step_size`): The largest continuation step. From an XPPAUT .ode: DSMAX.
- **Step size** (`step_size`): The first continuation step. From an XPPAUT .ode: DS.
- **Parameter min** (`parameter_min`): A continuation run stops when the parameter falls below this. From an XPPAUT .ode: PARMIN.
- **Parameter max** (`parameter_max`): A continuation run stops when the parameter rises above this. From an XPPAUT .ode: PARMAX.
- **Norm min** (`norm_min`): A continuation run stops when the solution's norm falls below this. From an XPPAUT .ode: NORMMIN.
- **Norm max** (`norm_max`): A continuation run stops when the solution's norm rises above this. From an XPPAUT .ode: NORMMAX.
- **Parameter tolerance** (`parameter_tolerance`): The tolerance the parameter converges to in the Newton iteration. From an XPPAUT .ode: EPSL.
- **State tolerance** (`state_tolerance`): The tolerance the state converges to in the Newton iteration. From an XPPAUT .ode: EPSU.
- **Special point tolerance** (`special_point_tolerance`): The tolerance the special points of a branch are located to. From an XPPAUT .ode: EPSS.
- **Mesh adapt every** (`mesh_adapt_every`): The mesh is adapted every this many steps; 0 keeps it as it is.
- **Max branch switches** (`max_branch_switches`): The most times a run switches to another branch.
- **Switch one direction** (`switch_one_direction`): Whether a branch switch continues in one direction only.
- **Output detail** (`output_detail`): How much of each point the run writes to the Output.
- **Max location iterations** (`max_location_iterations`): The most iterations that locate a special point.
- **Max newton iterations** (`max_newton_iterations`): The most Newton iterations in one continuation step.
- **Full newton after** (`full_newton_after`): A full Newton iteration is taken after this many steps; before it the Jacobian is kept.
- **Step adapt every** (`step_adapt_every`): The step size is adapted every this many steps.
- **Suppress branch points** (`suppress_branch_points`): Whether branch points are left out of the diagram.
- **Follow all branches** (`follow_all_branches`): Whether a run follows every branch it finds, not only the one it started on.
- **Special points** (`special_points`): The codimension-two points a run detects: a comma-separated list of their names, empty for none.

## Diagram

- **Variable** (`variable`): The variable the continuation diagram plots up. From an XPPAUT .ode: AUTOVAR.
- **X min** (`x_min`): The left edge of the continuation diagram. From an XPPAUT .ode: AUTOXMIN.
- **X max** (`x_max`): The right edge of the continuation diagram. From an XPPAUT .ode: AUTOXMAX.
- **Y min** (`y_min`): The bottom edge of the continuation diagram. From an XPPAUT .ode: AUTOYMIN.
- **Y max** (`y_max`): The top edge of the continuation diagram. From an XPPAUT .ode: AUTOYMAX.

## Plot

- **Colormap** (`colormap`): The colors a color-coded curve or an array plot runs through. From an XPPAUT .ode: COLORMAP.
- **Curves** (`curves`): The curves of the starting graph, each with the variables on its axes; the first holds the axes' variables. From an XPPAUT .ode: CURVES.

### Axes

- **Kind** (`axes.kind`): A two- or three-dimensional plot. From an XPPAUT .ode: AXES.
- **X min** (`axes.x_min`): The low end of x: the window's left edge in a two-dimensional plot, the box's in a three-dimensional one. From an XPPAUT .ode: XMIN, XLO: wins over the key's other name when both are given; a 2D plot only.
- **X max** (`axes.x_max`): The high end of x: the window's right edge in a two-dimensional plot, the box's in a three-dimensional one. From an XPPAUT .ode: XMAX, XHI: wins over the key's other name when both are given; a 2D plot only.
- **Y min** (`axes.y_min`): The low end of y: the window's bottom edge in a two-dimensional plot, the box's in a three-dimensional one. From an XPPAUT .ode: YMIN, YLO: wins over the key's other name when both are given; a 2D plot only.
- **Y max** (`axes.y_max`): The high end of y: the window's top edge in a two-dimensional plot, the box's in a three-dimensional one. From an XPPAUT .ode: YMAX, YHI: wins over the key's other name when both are given; a 2D plot only.
- **Z min** (`axes.z_min`): The low end of the three-dimensional box in z. From an XPPAUT .ode: ZMIN.
- **Z max** (`axes.z_max`): The high end of the three-dimensional box in z. From an XPPAUT .ode: ZMAX.
- **Phi** (`axes.phi`): The azimuth the three-dimensional plot is viewed from, in degrees. From an XPPAUT .ode: PHI.
- **Theta** (`axes.theta`): The elevation the three-dimensional plot is viewed from, in degrees. From an XPPAUT .ode: THETA.

### Line

- **Draw** (`line.draw`): Whether a new curve is drawn as a line, as points or as both. From an XPPAUT .ode: LT (which also sets plot.line.marker, plot.line.marker_size): 1 draws a line; 0 or -n draws points marked with a circle (n its size).
- **Dash** (`line.dash`): The dash pattern of a new curve's line.
- **Width** (`line.width`): The width of a new curve's line.
- **Marker** (`line.marker`): The marker drawn at the points of a new curve. From an XPPAUT .ode: LT (which also sets plot.line.draw, plot.line.marker_size): 1 draws a line; 0 or -n draws points marked with a circle (n its size).
- **Marker size** (`line.marker_size`): The size of the markers of a new curve. From an XPPAUT .ode: LT (which also sets plot.line.draw, plot.line.marker): 1 draws a line; 0 or -n draws points marked with a circle (n its size).

### Nullclines

- **Mesh** (`nullclines.mesh`): How many points a side the nullclines are found on. From an XPPAUT .ode: NMESH.
- **Draw** (`nullclines.draw`): Whether the nullclines are drawn after a run. From an XPPAUT .ode: NCDRAW (which also sets output.nullclines_file): 0 is false, 1 true; 2 is false and also saves the nullclines to nullclines.dat.

### Direction field

- **Grid** (`direction_field.grid`): How many arrows a side the direction field has. From an XPPAUT .ode: DFGRID.
- **Draw** (`direction_field.draw`): What a run draws of the direction field, and whether it also saves it. From an XPPAUT .ode: DFDRAW.

### Color by

- **Enabled** (`color_by.enabled`): Whether a curve is colored by a quantity along it. From an XPPAUT .ode: COLORIZE.
- **Via** (`color_by.via`): The variable a color-coded curve takes its color from. From an XPPAUT .ode: COLORVIA.
- **Low** (`color_by.low`): The value of the coloring variable that gets the first color. From an XPPAUT .ode: COLORLO.
- **High** (`color_by.high`): The value of the coloring variable that gets the last color. From an XPPAUT .ode: COLORHI.

### Array plot

- **First column** (`array_plot.first_column`): The first data column the array plot shows.
- **Columns** (`array_plot.columns`): How many columns the array plot shows.
- **First row** (`array_plot.first_row`): The stored row the array plot starts at, counted from 0.
- **Rows** (`array_plot.rows`): How many rows the array plot shows.
- **Row skip** (`array_plot.row_skip`): The array plot shows every this many rows of the stored data.
- **Z min** (`array_plot.z_min`): The value the array plot gives the first color.
- **Z max** (`array_plot.z_max`): The value the array plot gives the last color.

## Analysis

- **Coupling** (`coupling`): The coupling of the H function, one `variable = formula` for each differential equation.

### Histogram

- **Low** (`histogram.low`): The low end of the histogram's range. From an XPPAUT .ode: HISTLO.
- **High** (`histogram.high`): The high end of the histogram's range. From an XPPAUT .ode: HISTHI.
- **Bins** (`histogram.bins`): How many bins the histogram has. From an XPPAUT .ode: HISTBINS.
- **Column** (`histogram.column`): The column of the data the histogram counts. From an XPPAUT .ode: HISTCOL.

### Histogram2

- **Low** (`histogram2.low`): The low end of the second axis of the two-dimensional histogram. From an XPPAUT .ode: HISTLO2.
- **High** (`histogram2.high`): The high end of the second axis of the two-dimensional histogram. From an XPPAUT .ode: HISTHI2.
- **Bins** (`histogram2.bins`): How many bins the second axis of the two-dimensional histogram has. From an XPPAUT .ode: HISTBINS2.
- **Column** (`histogram2.column`): The column on the second axis of the two-dimensional histogram. From an XPPAUT .ode: HISTCOL2.

### Spectrum

- **Column** (`spectrum.column`): The column of the data the spectrum is taken of. From an XPPAUT .ode: SPECCOL.
- **Column2** (`spectrum.column2`): The second column of a cross spectrum or a coherence. From an XPPAUT .ode: SPECCOL2.
- **Width** (`spectrum.width`): How many points the spectrum's windows are long. From an XPPAUT .ode: SPECWIDTH.
- **Window** (`spectrum.window`): The window the spectrum's segments are weighted with. From an XPPAUT .ode: SPECWIN.

### Transpose

- **First column** (`transpose.first_column`): The first data column Transpose takes.
- **Columns** (`transpose.columns`): How many data columns Transpose takes.
- **Column skip** (`transpose.column_skip`): Transpose takes every this many columns of the stored data.
- **First row** (`transpose.first_row`): The stored row Transpose starts at, counted from 0.
- **Rows** (`transpose.rows`): How many stored rows Transpose takes.
- **Row skip** (`transpose.row_skip`): Transpose takes every this many rows of the stored data.

## Interface

- **Sliders** (`sliders`): The sliders the window offers: a parameter or variable each, with its range.
- **Buttons** (`buttons`): The buttons of the window: a label and the command each runs.

## Output

- **Data file** (`data_file`): The file a run writes its data to, named from the model's folder. From an XPPAUT .ode: OUTPUT.
- **Picture file** (`picture_file`): The picture a batch run writes of the graph (PDF or SVG by its extension); empty for none. From an XPPAUT .ode: PLOTFMT.
- **Run set** (`run_set`): The sets a batch run runs, one run each in the model's order; with none, one run with no set.
- **Nullclines file** (`nullclines_file`): The file the nullclines are saved to after a run. From an XPPAUT .ode: NCDRAW (which also sets plot.nullclines.draw): 0 is false, 1 true; 2 is false and also saves the nullclines to nullclines.dat.
- **After run** (`after_run`): What a batch run makes of its data once it has run: an analysis, or the mean or variance of its runs. From an XPPAUT .ode: POSTPROCESS, STOCH: 1 is mean_of_runs, 2 variance_of_runs, 0 none.
- **After run file** (`after_run_file`): The file a batch run writes its after-run analysis to.
