# The settings panels

Every group of settings has one panel, titled by the group's name, and its rows are in the order of the option table
(`core/model_options.cpp`), the one source of a setting's key, label, choices, default, limits and help line. A dotted key
(`step.min`) is a titled section inside the panel. The same keys are the `options` block of a `.odex`, the `help` of the program and
the manual below. Each panel opens from the menu entry of its task: Integration from Run > Run settings, Poincare from Run > Poincare map,
Sweep from Run > Over a range > Run over a range (beside its Run button), Equilibria from Analysis > Equilibria, Analysis inside the
histogram and spectrum commands' forms, Continuation from autoX > Numerics, Diagram from autoX > Axes (it changes the open view; the model's
values only start a new one), Plot from Plot > Axes > Axes options (it changes the active graph), Export inside the picture export
commands' forms and Output from File > Output, where each path has a Choose file button. The thread count and the log are File > Preferences. Values > Numerics shows the Integration rows beside the plot, each change applying at once.
Apply sets the changed rows together, all or nothing, as one undo step. The values of a panel come from the core when the running command
ends; the interface group (sliders, buttons, animation) has its own editors.


## Integration

- **Total time** (`total_time`): How long a run integrates, in model time.
- **Start time** (`start_time`): The time a run starts at.
- **Dt** (`dt`): The time step of a fixed-step method, and the first step of an adaptive one.
- **Store every** (`store_every`): Keep one step of every this many in the data table.
- **Store from** (`store_from`): Keep no data before this time; the run before it is a transient.
- **Storage precision** (`storage_precision`): What the data table keeps each value as: a single-precision float as XPPAUT does, or the integrator's double.
- **State limit** (`state_limit`): A state beyond plus or minus this value stops the run.
- **Stop at limit** (`stop_at_limit`): Whether a state beyond the state limit stops the run, or only marks it.
- **Method** (`method`): The integration method.
- **Subsystem split** (`subsystem_split`): What splits the model into independent subsystems: its equations alone, or also the run (a Poincare section or stop).
- **Compile** (`compile`): Whether the equations run as compiled code or in the interpreter; a change applies when the model next loads.
- **Seed** (`seed`): The seed of the random numbers a run draws; the same seed gives the same run.

### Step

- **Relative tolerance** (`step.relative_tolerance`): The relative error an adaptive method accepts in one step.
- **Absolute tolerance** (`step.absolute_tolerance`): The absolute error an adaptive method accepts in one step.
- **Min** (`step.min`): The smallest step an adaptive method takes before it gives up.
- **Max** (`step.max`): The largest step an adaptive method takes.

### Jacobian

- **Kind** (`jacobian.kind`): Whether an implicit method gets the Jacobian by differences or from the exact derivatives.
- **Band upper** (`jacobian.band_upper`): The diagonals above the main one that a banded Jacobian keeps.
- **Band lower** (`jacobian.band_lower`): The diagonals below the main one that a banded Jacobian keeps.

### Delay

- **Max delay** (`delay.max_delay`): The longest delay a model uses; it sets how much history is kept.
- **History points** (`delay.history_points`): How many past points of the history a delay model keeps.
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

### Torus

- **Period** (`torus.period`): The period of the variables that live on a circle or torus.
- **Fold** (`torus.fold`): A variable that is taken modulo the torus period; each names one.

## Poincare

- **Map** (`map`): What a run records at a Poincare section: nothing, the section, the maxima or the period.
- **Variable** (`variable`): The variable the Poincare section is taken on.
- **Sign** (`sign`): Which crossings of the section count: upward, downward or both.
- **Plane** (`plane`): The value of the variable at the section.
- **Stop** (`stop`): Whether a crossing of the section stops the run.

## Equilibria

- **Max iterations** (`max_iterations`): The most Newton iterations the search for an equilibrium makes.
- **Newton tolerance** (`newton_tolerance`): The tolerance the search for an equilibrium stops at.
- **Jacobian step** (`jacobian_step`): The step that differences the Jacobian of the equilibrium search.
- **Search limit** (`search_limit`): A state beyond plus or minus this value ends the equilibrium search.
- **Print eigenvalues** (`print_eigenvalues`): Whether the eigenvalues of an equilibrium are printed.
- **Draw invariant sets** (`draw_invariant_sets`): Whether the stable and unstable sets of a saddle are drawn.
- **Draw strong sets** (`draw_strong_sets`): Whether the strong stable and unstable sets are drawn.

## Sweep

- **Variable** (`variable`): The parameter or variable a sweep varies.
- **Steps** (`steps`): How many steps the sweep makes between its ends.
- **Low** (`low`): The value the sweep starts at.
- **High** (`high`): The value the sweep ends at.
- **Reset** (`reset`): Whether each run of the sweep starts from the initial conditions.
- **Old ic** (`old_ic`): Whether each run of the sweep starts where the one before ended.
- **Write files** (`write_files`): Whether each run of the sweep is written to a file of its own.

## Continuation

- **Mesh intervals** (`mesh_intervals`): How many mesh intervals a periodic orbit or boundary value solution has.
- **Max steps** (`max_steps`): The most continuation steps a run takes before it stops.
- **Output every** (`output_every`): A diagram point is kept every this many steps.
- **Collocation points** (`collocation_points`): The collocation points in each mesh interval.
- **Min step size** (`min_step_size`): The smallest continuation step.
- **Max step size** (`max_step_size`): The largest continuation step.
- **Step size** (`step_size`): The first continuation step.
- **Parameter min** (`parameter_min`): A continuation run stops when the parameter falls below this.
- **Parameter max** (`parameter_max`): A continuation run stops when the parameter rises above this.
- **Norm min** (`norm_min`): A continuation run stops when the solution's norm falls below this.
- **Norm max** (`norm_max`): A continuation run stops when the solution's norm rises above this.
- **Parameter tolerance** (`parameter_tolerance`): The tolerance the parameter converges to in the Newton iteration.
- **State tolerance** (`state_tolerance`): The tolerance the state converges to in the Newton iteration.
- **Special point tolerance** (`special_point_tolerance`): The tolerance the special points of a branch are located to.
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

- **Variable** (`variable`): The variable the continuation diagram plots up.
- **X min** (`x_min`): The left edge of the continuation diagram.
- **X max** (`x_max`): The right edge of the continuation diagram.
- **Y min** (`y_min`): The bottom edge of the continuation diagram.
- **Y max** (`y_max`): The top edge of the continuation diagram.

### Colours

- **Stable equilibrium** (`colors.stable_equilibrium`): The colour of the stable equilibria in the diagram.
- **Unstable equilibrium** (`colors.unstable_equilibrium`): The colour of the unstable equilibria in the diagram.
- **Stable periodic** (`colors.stable_periodic`): The colour of the stable periodic orbits in the diagram.
- **Unstable periodic** (`colors.unstable_periodic`): The colour of the unstable periodic orbits in the diagram.

## Plot

- **Colourmap** (`colormap`): The colours a colour-coded curve or an array plot runs through.
- **Curves** (`curves`): The curves of the starting graph, each with the variables on its axes; the first holds the axes' variables.

### Axes

- **Kind** (`axes.kind`): A two- or three-dimensional plot.
- **X min** (`axes.x_min`): The low end of x: the window's left edge in a two-dimensional plot, the box's in a three-dimensional one.
- **X max** (`axes.x_max`): The high end of x: the window's right edge in a two-dimensional plot, the box's in a three-dimensional one.
- **Y min** (`axes.y_min`): The low end of y: the window's bottom edge in a two-dimensional plot, the box's in a three-dimensional one.
- **Y max** (`axes.y_max`): The high end of y: the window's top edge in a two-dimensional plot, the box's in a three-dimensional one.
- **Z min** (`axes.z_min`): The low end of the three-dimensional box in z.
- **Z max** (`axes.z_max`): The high end of the three-dimensional box in z.
- **Phi** (`axes.phi`): The azimuth the three-dimensional plot is viewed from, in degrees.
- **Theta** (`axes.theta`): The elevation the three-dimensional plot is viewed from, in degrees.

### Line

- **Draw** (`line.draw`): Whether a new curve is drawn as a line, as points or as both.
- **Dash** (`line.dash`): The dash pattern of a new curve's line.
- **Width** (`line.width`): The width of a new curve's line.
- **Marker** (`line.marker`): The marker drawn at the points of a new curve.
- **Marker size** (`line.marker_size`): The size of the markers of a new curve.

### Nullclines

- **Mesh** (`nullclines.mesh`): How many points a side the nullclines are found on.
- **Draw** (`nullclines.draw`): Whether the nullclines are drawn after a run.
- **X colour** (`nullclines.x_color`): The colour of the x nullcline.
- **Y colour** (`nullclines.y_color`): The colour of the y nullcline.

### Direction field

- **Grid** (`direction_field.grid`): How many arrows a side the direction field has.
- **Draw** (`direction_field.draw`): What a run draws of the direction field, and whether it also saves it.

### Manifolds

- **Stable colour** (`manifolds.stable_color`): The colour of the stable manifold of a saddle.
- **Unstable colour** (`manifolds.unstable_color`): The colour of the unstable manifold of a saddle.

### Colour by

- **Enabled** (`color_by.enabled`): Whether a curve is coloured by a quantity along it.
- **Via** (`color_by.via`): The variable a colour-coded curve takes its colour from.
- **Low** (`color_by.low`): The value of the colouring variable that gets the first colour.
- **High** (`color_by.high`): The value of the colouring variable that gets the last colour.

## Analysis


### Histogram

- **Low** (`histogram.low`): The low end of the histogram's range.
- **High** (`histogram.high`): The high end of the histogram's range.
- **Bins** (`histogram.bins`): How many bins the histogram has.
- **Column** (`histogram.column`): The column of the data the histogram counts.

### Histogram2

- **Low** (`histogram2.low`): The low end of the second axis of the two-dimensional histogram.
- **High** (`histogram2.high`): The high end of the second axis of the two-dimensional histogram.
- **Bins** (`histogram2.bins`): How many bins the second axis of the two-dimensional histogram has.
- **Column** (`histogram2.column`): The column on the second axis of the two-dimensional histogram.

### Spectrum

- **Column** (`spectrum.column`): The column of the data the spectrum is taken of.
- **Column2** (`spectrum.column2`): The second column of a cross spectrum or a coherence.
- **Width** (`spectrum.width`): How many points the spectrum's windows are long.
- **Window** (`spectrum.window`): The window the spectrum's segments are weighted with.

## Interface

- **Sliders** (`sliders`): The sliders the window offers: a parameter or variable each, with its range.
- **Buttons** (`buttons`): The buttons of the window, each running a command or a set.

## Output

- **Data file** (`data_file`): The file a run writes its data to, named from the model's folder.
- **Picture file** (`picture_file`): The picture a batch run writes of the graph (PDF or SVG by its extension); empty for none.
- **Run set** (`run_set`): A set a batch run runs, by its name; repeat the option for each set. With none, the model runs once with no set.
- **Nullclines file** (`nullclines_file`): The file the nullclines are saved to after a run.
- **After run** (`after_run`): What a batch run makes of its data once it has run: an analysis, or the mean or variance of its runs.
- **After run file** (`after_run_file`): The file the after_run analysis is written to, so the data file keeps the trajectory.
