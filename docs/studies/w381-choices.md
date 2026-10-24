# W381: Named Choices Instead of Numbers or Signs

Stage 1 study for card [W381 / #443](https://github.com/MuhammadMoustafa/xppautX/issues/443) (2026-10-15):
"A choice is named, never a number or a sign. Using numbers to indicate options is a bad behavior and we should fix that."

## 1. Scope and Strategy

XPPAUT historically encoded categorical choices, toggle states, algorithm selectors, and special execution modes using small integers, negative numbers, or special character signs.

This study inventories every option, setting, dialog field, protocol field, and session file key (.odex, .snapx, .recx) where a number or a sign stands for a choice.
For each item:
- **Location**: exact `file:line` in the codebase.
- **Semantic role**: what property or branch it controls.
- **Today's spelling**: numeric value or sign.
- **Proposed named form**: boolean for yes/no toggles; lowercase named string identifier for categorical selections.
- **Stage 2 Owner Card**: the architectural owner responsible for converting the subsystem in Stage 2.
- **W352 Operation**: the corresponding dotted command from [docs/studies/w308-inventory.md](w308-inventory.md).

---

## 2. Inventory of Numbered and Signed Choices

### 2.1 Group 1: Numerics and Integration (Stage 2: `card-w381-numerics`)

| Location (`file:line`) | Target Member / Setting | What It Chooses | Today's Spelling | Proposed Named Form | Stage 2 Owner | W352 Operation |
|---|---|---|---|---|---|---|
| [core/model_options.cpp:166](../../core/model_options.cpp#L166), [core/numerics_settings.cpp:63, 166](../../core/numerics_settings.cpp#L63) | `TOTAL_TIME`, `s.numerics.total_time`, `s.numerics.forever` | Run duration vs run indefinitely until stopped | Negative number (e.g. `-20`) or sign `< 0` | `stop_condition: choice("duration","forever")`, or separate boolean `forever: true/false` with positive `duration: real` | `numerics_settings` | `run.continue`, `session.configure_numerics` |
| [core/model_options.cpp:213](../../core/model_options.cpp#L213), [core/numerics_settings.cpp:42](../../core/numerics_settings.cpp#L42), [core/menus.cpp:377, 416](../../core/menus.cpp#L377) | `METHOD`, `s.numerics.method` | Numerical solver algorithm | Integers 0..14 (0=Discrete, 1=Euler, 2=Mod. Euler, 3=Runge-Kutta, 4=Adams, 5=Gear, 6=Volterra, 7=BackEul, 8=Qualst.RK4, 9=Stiff, 10=CVode, 11=DoPri5, 12=DoPri83, 13=Rosen23, 14=Symplectic) | `choice("discrete","euler","modified_euler","runge_kutta","adams","gear","volterra","backward_euler","quality_rk4","stiff","cvode","dopri5","dopri83","rosenbrock23","symplectic")` | `solver` / `numerics_settings` | `session.configure_numerics` |
| [core/model_options.cpp:330-345](../../core/model_options.cpp#L330-L345), [core/numerics.cpp:319](../../core/numerics.cpp#L319), [core/menus.cpp:372, 405](../../core/menus.cpp#L372) | `POINCARE_MAP`, `s.numerics.poincare_map` | Type of Poincaré map intersection condition | Integers 0..3 (0=None, 1=Section, 2=Max/min, 3=Period) | `choice("none","section","max_min","period")` | `numerics` / `model_options` | `analysis.poincare_map` |
| [core/model_options.cpp:353-356](../../core/model_options.cpp#L353-L356), [core/numerics.cpp:328](../../core/numerics.cpp#L328) | `POINCARE_SIGN`, `s.numerics.poincare_sign` | Direction of trajectory crossing through Poincaré plane | Integers 1 (upwards), -1 (downwards), 0 (both) | `choice("positive","negative","both")` (or `upwards`, `downwards`, `both`) | `numerics` / `model_options` | `analysis.poincare_map` |
| [core/model_options.cpp:357-360](../../core/model_options.cpp#L357-L360), [core/numerics.cpp:329](../../core/numerics.cpp#L329) | `POINCARE_STOP`, `s.numerics.poincare_stop` | Stop integration upon reaching section plane | Integers 0 (don't stop), 1 (stop) | boolean `stop_on_cross: true/false` | `numerics` / `model_options` | `analysis.poincare_map` |
| [core/integrate.h:139](../../core/integrate.h#L139), [core/integrate.cpp:205, 234-235](../../core/integrate.cpp#L205), [core/model_options.cpp:655-662](../../core/model_options.cpp#L655-L662) | `RANGERESET`, `s.integrator.range.reset` | Clear storage buffer between successive range runs | Integers 0/1 (`"N"`/`"Y"`) | boolean `reset_storage: true/false` | `integrate` / `model_options` | `run.range`, `run.double_range` |
| [core/integrate.h:139](../../core/integrate.h#L139), [core/integrate.cpp:206, 236-237](../../core/integrate.cpp#L206), [core/model_options.cpp:663-670](../../core/model_options.cpp#L663-L670) | `RANGEOLDIC`, `s.integrator.range.oldic` | Reset ICs to base initial values for each sweep step vs starting from previous endpoint | Integers 0/1 (`"N"`/`"Y"`) | boolean `use_old_ic: true/false` (or `choice("base_initial_values","previous_endpoint")`) | `integrate` / `model_options` | `run.range`, `run.double_range` |
| [core/integrate.h:139](../../core/integrate.h#L139), [core/integrate.cpp:206, 238-239](../../core/integrate.cpp#L206) | `s.integrator.range.cycle` | Cycle plot color across sweep runs | Integers 0/1 (`"N"`/`"Y"`) | boolean `cycle_color: true/false` | `integrate` | `run.range`, `run.double_range` |
| [core/integrate.h:139](../../core/integrate.h#L139), [core/integrate.cpp:206, 240-241](../../core/integrate.cpp#L206) | `s.integrator.range.movie` | Capture kinescope screenshots during range sweep | Integers 0/1 (`"N"`/`"Y"`) | boolean `capture_movie: true/false` | `integrate` | `run.range`, `run.double_range` |
| [core/integrate.h:141](../../core/integrate.h#L141), [core/integrate.cpp:254, 272-275, 784](../../core/integrate.cpp#L254) | `s.integrator.range.rtype`, "Crv(1) Array(2)" dialog field | Output structure for double parameter sweep | Integers 0 (1-param), 1 (curve), 2 (raster array); dialog field `"1"` or `"2"` | `choice("curve","array")` | `integrate` | `run.double_range` |
| [core/integrate.h:139](../../core/integrate.h#L139), [core/integrate.cpp:94-95, 177, 186](../../core/integrate.cpp#L94-L95) | `s.integrator.range.type`, `s.integrator.range.type2` | Swept entity type (parameter vs initial state) | Integers 1 (`PARAM`), 2 (`IC`) | `choice("parameter","variable")` | `integrate` | `run.range`, `run.double_range` |
| [core/model_options.cpp:671-672](../../core/model_options.cpp#L671-L672), [core/xpp_batch.h:20](../../core/xpp_batch.h#L20) | `RANGE`, `s.batch_range` | Output a file per reset run vs one combined file | Small integer 0 or 1 (`@ range=1`) | boolean `rangefiles: true/false` (converted in review W349) | `xpp_batch` / `model_options` | `session.configure_output` |
| [core/integrate.h:81, 175](../../core/integrate.h#L81), [core/integrate.cpp:149, 330](../../core/integrate.cpp#L149), [core/ui_json.cpp:532](../../core/ui_json.cpp#L532) | `s.integrator.eq_range.shoot`, `write_equilibrium(..., int shoot)` | Compute and integrate saddle invariant manifolds | Integers 0 or 1 ("Shoot (1/0)") | boolean `shoot_manifolds: true/false` | `integrate` / `pp_shoot` | `equilibrium.find`, `equilibrium.search`, `equilibrium.range`, `equilibrium.export` |
| [core/integrate.h:175](../../core/integrate.h#L175), [core/integrate.cpp:150-151, 165-167](../../core/integrate.cpp#L150-L151) | `s.integrator.eq_range.col`, `s.integrator.eq_range.movie` | Cycle color and capture movie during equilibrium sweep | Integers 0 or 1 | boolean `cycle_color: true/false`, `capture_movie: true/false` | `integrate` | `equilibrium.range` |
| [core/adj2.cpp:492](../../core/adj2.cpp#L492) | `s.adjoint.liap_flag` | Adjoint parameter sweep | Integer 0 or 1 ("Range over parameters?(0/1)") | boolean `range_over_parameters: true/false` | `adj2` | `analysis.adjoint_range` |
| [core/model_options.cpp:315-328](../../core/model_options.cpp#L315-L328), [core/numerics.cpp:152](../../core/numerics.cpp#L152) | `s.numerics.cv_bandflag` | Use banded solver matrix for CVODE | Integer 0 or 1 ("Banded system(0/1)?") | boolean `banded: true/false` | `numerics` / `solver` | `session.configure_numerics` |
| [core/model_options.cpp:377-383](../../core/model_options.cpp#L377-L383), [core/numerics.cpp:146](../../core/numerics.cpp#L146), [core/tabular.cpp:162](../../core/tabular.cpp#L162) | `AUTOEVAL`, `s.tables[i].autoeval` | Re-evaluate lookup tables upon parameter changes | Integer bitmask / integer 0/1 ("AutoEval(1=yes) :", "Auto-evaluate? (1/0)") | boolean `auto_evaluate: true/false` | `tabular` / `numerics` | `table.edit` |
| [core/pp_shoot.cpp:188, 322](../../core/pp_shoot.cpp#L188), [core/menus.cpp:368](../../core/menus.cpp#L368) | `ishow` in BVP shooting | Intermediate display mode for BVP shooting | Integers 0 (No show), 1 (Show) | `choice("final","steps")` (or boolean `show_steps: true/false`) | `pp_shoot` | `bvp.set_show_mode` |

---

### 2.2 Group 2: Graphics, Plotting, and Views (Stage 2: `card-w381-graphics`)

| Location (`file:line`) | Target Member / Setting | What It Chooses | Today's Spelling | Proposed Named Form | Stage 2 Owner | W352 Operation |
|---|---|---|---|---|---|---|
| [core/model_options.cpp:463-477](../../core/model_options.cpp#L463-L477), [core/graf_par.cpp:468](../../core/graf_par.cpp#L468) | `AXES`, `s.plot_settings.axes` | Projection geometry: 2D plane vs 3D cube | String `"2"`/`"3"` mapping internally to integers `0` and `5` | `choice("2d","3d")` | `graf_par` / `model_options` | `plot.set_2d`, `plot.set_3d` |
| [core/model_options.cpp:551-560](../../core/model_options.cpp#L551-L560), [core/graf_par.cpp:732](../../core/graf_par.cpp#L732) | `LT`, `s.plot_settings.start_line_type`, `c.type` | Curve stroke rendering style | Integers -5..1 (-5: dotted, -4: dot-dash, -3: short dash, -2: medium dash, -1: long dash, 0: points, 1: solid) | `choice("dotted","dot_dash","short_dash","medium_dash","long_dash","points","solid")` | `graphics` / `model_options` | `plot.add_curve`, `plot.edit_curve` |
| [core/model_options.cpp:85-92, 561-580](../../core/model_options.cpp#L85-L92), [core/colormap.cpp](../../core/colormap.cpp), [core/xpp_session.cpp:245, 256, 274, 348](../../core/xpp_session.cpp#L245) | `XNC`, `YNC`, `SMC`, `UMC`, `SEC`, `UEC`, `SPC`, `UPC`, `c.color`, `g.color`, `l.color` | Curve and annotation stroke colors | Integers 0..10 | Hex string `#rrggbb` or `"foreground"` (W308 decision) | `colormap` / `graphics` / `xpp_session` | `plot.*`, `annotation.*`, `nullcline.*`, `auto.*` |
| [core/model_options.cpp:581-588](../../core/model_options.cpp#L581-L588), [core/menus.cpp:357-358, 395](../../core/menus.cpp#L357-L358) | `COLORMAP`, `s.colormap` | Colormap gradient ramp | Integers 0..6 (0=Normal, 1=Periodic, 2=Hot, 3=Cool, 4=Blue-red, 5=Gray, 6=Cubehelix) | `choice("jet","periodic","hot","cool","blue_red","gray","cubehelix")` | `colormap` / `model_options` | `plot.set_colormap`, `array_plot.configure` |
| [core/model_options.cpp:606-607](../../core/model_options.cpp#L606-L607), [core/nullcline.cpp](../../core/nullcline.cpp) | `DFDRAW`, `s.nullclines.df_batch` | Direction field automatic drawing on start | Integers 0 or 1 | boolean `auto_draw: true/false` | `nullcline` / `model_options` | `dirfield.configure` |
| [core/model_options.cpp:608-609](../../core/model_options.cpp#L608-L609), [core/nullcline.cpp](../../core/nullcline.cpp) | `NCDRAW`, `s.nullclines.nc_batch` | Nullcline drawing mode vs export | Integers 0 (no), 1 (draw), 2 (save to nullclines.dat) | `choice("none","draw")` (value 2 dropped per maintainer #413; export is command `nullcline.export`) | `nullcline` / `model_options` | `nullcline.configure`, `nullcline.export` |
| [core/model_options.cpp:612-613](../../core/model_options.cpp#L612-L613), [core/nullcline.cpp](../../core/nullcline.cpp) | `COLORIZE`, `s.nullclines.colorize_flag` | Colorize phaseplane / curves by velocity or variable | Integers 0 or 1 | boolean `colorize: true/false` | `nullcline` / `model_options` | `analysis.color_coding` |
| [core/model_options.cpp:453-457](../../core/model_options.cpp#L453-L457), [core/menus.cpp:360-362, 428](../../core/menus.cpp#L360-L362) | `SIMPLOT`, `s.plot_windows.simul` | Simultaneous plotting across windows during run | Integers 0 (SimPlot Off) or 1 (SimPlot On) | boolean `simultaneous_plot: true/false` | `graphics` / `model_options` | `window.set_simultaneous_plot` |
| [core/model_options.cpp:458-462](../../core/model_options.cpp#L458-L462) | `MULTIWIN`, `s.plot_settings.multi_win` | Separate window layout mode | Integers 0 or 1 | boolean `multi_window: true/false` | `graphics` / `model_options` | `window.configure` |
| [core/model_options.cpp:597-603](../../core/model_options.cpp#L597-L603), [core/my_ps.cpp:385](../../core/my_ps.cpp#L385) | `PS_COLOR`, `s.plot_file.ps_color_flag` | Monochrome vs color PostScript hardcopy | Integers 0 (B&W), 1 (Color) | boolean `color: true/false` (or `choice("monochrome","color")`) | `my_ps` / `model_options` | `plot.export_postscript` |
| [core/grobs.h](../../core/grobs.h), [core/grobs.cpp:269, 291](../../core/grobs.cpp#L269), [core/xpp_session.cpp:255](../../core/xpp_session.cpp#L255) | `g.type` in `s.grobs` | Graphic annotation object classification | Small integers (1=arrow, 2=pointer, 3=marker) | `choice("arrow","pointer","marker")` | `grobs` / `xpp_session` | `annotation.add_arrow`, `annotation.add_pointer`, `annotation.add_marker` |
| [core/grobs.cpp:421](../../core/grobs.cpp#L421), [core/xpp_session.cpp:242](../../core/xpp_session.cpp#L242) | `l.size` in `s.labels` | Annotation font size category | Integers 0..4 ("Size 0-4 :") | `choice("small","medium","large","extra_large")` or integer point size | `grobs` / `xpp_session` | `annotation.add_text`, `annotation.edit` |
| [core/commands.cpp:450](../../core/commands.cpp#L450), [core/xpp_session.cpp:265](../../core/xpp_session.cpp#L265) | `s.frozen_curves.auto_freeze` | Auto-freeze curve on run completion | Integers 0 or 1 | boolean `auto_freeze: true/false` | `graphics` / `xpp_session` | `plot.set_auto_freeze` |

---

### 2.3 Group 3: AUTO Bifurcation Continuation (Stage 2: `card-w381-auto`)

| Location (`file:line`) | Target Member / Setting | What It Chooses | Today's Spelling | Proposed Named Form | Stage 2 Owner | W352 Operation |
|---|---|---|---|---|---|---|
| [core/auto_settings.h:75](../../core/auto_settings.h#L75), [core/menus.cpp:493-495](../../core/menus.cpp#L493-L495), [core/xpp_session_auto.cpp:54, 91](../../core/xpp_session_auto.cpp#L54) | `plot`, `s.auto_state.options.plot` | Diagram projection type in AUTO | Integers 0..14 (0=Hi, 1=Norm, 2=hI-lo, 3=Period, 4=Two par, 5=Zoom in, 6=Zoom out, 7=last 1 par, 8=last 2 par, 9=Fit, 10=fRequency, 11=Average, 12=Default, 13=Scroll, 14=new View) | `choice("max","norm","max_min","period","two_parameter","frequency","average")` | `auto_settings` / `auto_nox` / `xpp_session_auto` | `auto.configure_plot` |
| [core/xpp_session_auto.cpp:43](../../core/xpp_session_auto.cpp#L43), [core/diagram.h](../../core/diagram.h) | `itp` in `lead_columns`, `s.auto_state.bifur.itp` | Bifurcation point classification | Integers: 1=BP, 2=LP, 3=HB, 4=PD, 5=TR, 6=EP, 7=UZ, -1=MX | `choice("branch_point","limit_point","hopf","period_doubling","torus","endpoint","user_point","diverged")` | `auto_state` / `xpp_session_auto` | `auto.diagram` |
| [core/auto_settings.cpp:72](../../core/auto_settings.cpp#L72), [core/auto_nox.cpp:1538](../../core/auto_nox.cpp#L1538) | `suppbp`, `s.auto_state.suppress_bp` | Suppress detection and labeling of branch points | Integer 0 or 1 | boolean `skip_branch_points: true/false` | `auto_settings` / `auto_nox` | `auto.configure_numerics` |
| [core/auto_settings.cpp:67](../../core/auto_settings.cpp#L67) | `iid`, `s.auto_state.options.iid` | Diagnostic print output detail level | Integers 0..5 | `choice("silent","summary","standard","verbose","debug")` | `auto_settings` | `auto_configure_numerics` |
| [core/menus.cpp:497-498](../../core/menus.cpp#L497-L498), [core/auto_nox.cpp:1827](../../core/auto_nox.cpp#L1827) | `auto_start_items`, `s.auto_state.bifur.ips` | Initial continuation problem type | Single-character keys `"s"`, `"p"`, `"b"`, `"h"`, `"e"` | `choice("steady_state","periodic","boundary_value","homoclinic","heteroclinic")` | `auto_nox` | `auto.run` |

---

### 2.4 Group 4: Analysis, Data Browser, and Files (Stage 2: `card-w381-files`)

| Location (`file:line`) | Target Member / Setting | What It Chooses | Today's Spelling | Proposed Named Form | Stage 2 Owner | W352 Operation |
|---|---|---|---|---|---|---|
| [core/model_options.cpp:742-743](../../core/model_options.cpp#L742-L743), [core/histogram.h](../../core/histogram.h) | `POSTPROCESS`, `s.histogram.post_process` | Automated post-processing analysis on batch completion | Small integers 0, 1, 2, ... | `choice("none","histogram","histogram_2d","fourier","power_spectrum")` | `histogram` / `model_options` | `analysis.configure_postprocess` |
| [core/model_options.cpp:778-779](../../core/model_options.cpp#L778-L779), [core/histogram.h](../../core/histogram.h) | `SPECWIN`, `s.histogram.spec_win` | Windowing filter function for Fourier power spectrum | Small integers 0..3 | `choice("rectangular","hamming","hann","bartlett")` | `histogram` / `model_options` | `analysis.spectral_density` |
| [core/model_options.cpp:782-787](../../core/model_options.cpp#L782-L787), [core/xpp_log.h](../../core/xpp_log.h) | `QUIET`, `log_settings.verbose` | Console output verbosity | Text `"0"` or `"1"` | boolean `quiet: true/false` | `model_options` / `xpp_log` | `session.configure_output` |
| [core/xpp_session.cpp:98, 112, 141](../../core/xpp_session.cpp#L98), [core/snapx.h](../../core/snapx.h) | `windows.setx`: `number`, `active` | Window identifier | Integers 0, 1, 2, ... | Window name string e.g. `"window1"`, `"phase_plane"` | `xpp_session` | `window.*` |
| [core/xpp_session.cpp:115-116](../../core/xpp_session.cpp#L115-L116) | `windows.setx`: `auto_view` `earlier` | Earlier AUTO view reference | Integer index | View name string | `xpp_session` | `auto.select_view` |
| [core/xpp_session_auto.cpp:135, 150](../../core/xpp_session_auto.cpp#L135) | `auto/views.setx`: `active` | Active AUTO view reference | Integer index | View name string | `xpp_session_auto` | `auto.select_view` |
| [core/xpp_session.cpp:239, 254, 272](../../core/xpp_session.cpp#L239) | `marks.setx`: `window` | Window containing label or mark | Integer index | Window name string | `xpp_session` | `annotation.*`, `plot.*` |

---

## 3. Options That Are Missing or Make No Sense

XPPAUT's historical option set should not limit the modernization. Below is the list of options that are illogical, overloaded, or missing, with recommended modern replacements:

### 3.1 Illogical or Overloaded Options Today
1. **`total_time` negative sign for "forever"**:
   - *Problem*: Passing `TOTAL=-50` in a model file or typing `-50` in the Numerics menu means "integrate until manually stopped" (storing points along the way). The negative sign is unintuitive and breaks numerical parsing rules.
   - *Recommendation*: Introduce an explicit boolean setting `forever: true/false` (or `stop_condition: choice("duration","forever")`). When `forever` is true, integration runs until aborted; `duration` remains a strictly positive real number.
2. **`ncdraw=2` saving to `nullclines.dat`**:
   - *Problem*: In XPPAUT, `@ ncdraw=2` causes the nullcline solver to write ASCII points to `nullclines.dat` instead of drawing them to screen. This couples visualization configuration with ad-hoc legacy file output.
   - *Recommendation*: Restrict `ncdraw` to `choice("none","draw")`. Outputting nullcline data should be performed by the explicit command `nullcline.export --file <name> --format <fmt>`.
3. **`range=1` for multiple batch files**:
   - *Problem*: `@ range=1` indicates that a batch sweep should write separate files `output.dat.1`, `output.dat.2`, etc., rather than appending to `output.dat`. The name `range` collides with the concept of range integration itself.
   - *Recommendation*: Renamed in review W349 to `rangefiles: boolean`. This is clear and unambiguous.
4. **`transient_time` vs `store_from`**:
   - *Problem*: In XPPAUT, `TRANS` integrates for a duration before points are recorded into storage. Users frequently confuse `T0` (start time of simulation clock) with `TRANS` (transient discard duration).
   - *Recommendation*: Retain `transient_time: real` for compatibility, but introduce the descriptive alias `discard_transient: real`.
5. **Overloaded `TOL` across solvers**:
   - *Problem*: In RK45 (adaptive Runge-Kutta), `TOL` is used as a relative error bound. In CVODE and Gear, `TOL` serves as relative tolerance while `ATOL` serves as absolute tolerance. In fixed-step solvers, `TOL` is ignored.
   - *Recommendation*: Clearly separate solver tolerances into `relative_tolerance` and `absolute_tolerance`, marking them as `unused` when a fixed-step method is selected.

### 3.2 Missing Options That Users Expect
1. **Simulation output format (`output_format`)**:
   - *Missing*: Currently, only `plotfmt` exists for PostScript/SVG graphics. Batch simulation data defaults to uncompressed whitespace-delimited ASCII `.dat`.
   - *Recommendation*: Add `.odex` option `output_format: choice("csv","tsv","parquet")` and compression toggle `output_compression: choice("none","zstd","gzip")`.
2. **Explicit overflow bound toggle (`bounds_check`)**:
   - *Missing*: Today, `bound=100` silently terminates integration if any variable exceeds 100. Many biological and physical models have variables (concentrations, firing counts) that legitimately exceed 100, causing mysterious run aborts.
   - *Recommendation*: Add `bounds_check: boolean` (defaulting to `false` or `true` with clear warning), and `bound_limit: real`.
3. **Adaptive step size ceiling and floor (`min_step`, `max_step`)**:
   - *Problem*: Previously named `DTMIN` and `DTMAX`, they are now in `.odex` as `min_step` and `max_step`. However, fixed-step RK4 and Euler do not show these. The UI should expose them dynamically only for adaptive methods.
4. **Initial condition evaluation at T0 (`evaluate_ics`)**:
   - *Missing*: Formulas for initial conditions often depend on parameters. When parameters change, initial conditions defined by formulas are not automatically re-evaluated unless reloaded.
   - *Recommendation*: Add option `recompute_ics_on_parameter_change: boolean`.

---

## 4. AUTO's Own Constants (AUTO-07p Vocabulary) and Front-End Naming

AUTO-07p (and AUTO2000 before it) uses compact Fortran acronyms for continuation parameters. The maintainer decided in [issue #367](https://github.com/MuhammadMoustafa/xppautX/issues/367):
- **AUTO's internal and foreign files** keep their historical acronyms (`IPS`, `ILP`, `ISW`, `ISP`, etc.).
- **The front-end UI and dotted protocol** use descriptive names (`mesh_intervals`, `collocation_points`, `detect_folds`, etc.).
- **Help and documentation** cross-reference the Fortran acronyms so experienced AUTO users can locate them instantly (`help NTST` maps to `mesh_intervals`).

The table below defines the bidirectional mapping:

| AUTO-07p Acronym | AUTO File Spelling | Front-End & Dotted Command Name | Meaning and Valid Values |
|---|---|---|---|
| `IPS` | `ips <int>` | `problem_type` | Continuation problem classification: `choice("equilibrium","periodic","boundary_value","homoclinic","heteroclinic")` (Fortran 1, 2, 4, 9) |
| `ILP` | `ilp <0\|1>` | `detect_folds` | Detection and location of limit points (folds): boolean `true`/`false` |
| `ISW` | `isw <int>` | `branch_mode` | Branch continuation & switching: `choice("regular","switch_branch","two_parameter")` (Fortran 1, -1, 2) |
| `ISP` | `isp <0..3>` | `bifurcation_detection` | Bifurcation detection rigor: `choice("none","all","branch_points_only")` (Fortran 0, 1, 2) |
| `NICP` | `nicp <int>` | (Implied by `parameters`) | Number of continuation parameters (length of `parameters` list) |
| `ICP` | `icp <list>` | `parameters` | Active continuation parameters: `list[name]` (e.g. `["iapp"]` or `["iapp", "v_rev"]`) |
| `NTST` | `ntst <int>` | `mesh_intervals` | Number of mesh intervals for collocation discretization: integer (1..10000) |
| `NCOL` | `ncol <int>` | `collocation_points` | Number of Gauss collocation points per mesh interval: integer (2..7) |
| `NMX` | `nmx <int>` | `max_points` | Maximum number of continuation steps along branch: integer (1..100000) |
| `NPR` | `npr <int>` | `print_every` | Step interval for diagnostic logging and diagram point labelling: integer (>0) |
| `DS` | `ds <real>` | `step_size` | Initial pseudo-arclength continuation step size (sign determines continuation direction): real (!=0) |
| `DSMIN` | `dsmin <real>` | `step_min` | Minimum allowed continuation step size: real (>0) |
| `DSMAX` | `dsmax <real>` | `step_max` | Maximum allowed continuation step size: real (>0) |
| `RL0` | `rl0 <real>` | `parameter_min` | Lower bound on principal continuation parameter: real |
| `RL1` | `rl1 <real>` | `parameter_max` | Upper bound on principal continuation parameter: real |
| `A0` | `a0 <real>` | `norm_min` | Lower bound on solution L2 norm: real |
| `A1` | `a1 <real>` | `norm_max` | Upper bound on solution L2 norm: real |
| `EPSL` | `epsl <real>` | `tolerance_parameters` | Newton convergence tolerance for continuation parameters: real (>0) |
| `EPSU` | `epsu <real>` | `tolerance_solution` | Newton convergence tolerance for state and boundary conditions: real (>0) |
| `EPSS` | `epss <real>` | `tolerance_special` | Bisection and Mueller location tolerance for special bifurcation points: real (>0) |
| `IAD` | `iad <int>` | `adapt_mesh_every` | Frequency of mesh adaptation: integer (0 = fixed mesh, >0 = adapt every N steps) |
| `IADS` | `iads <int>` | `adapt_step_every` | Frequency of step size adaptation: integer (0 = fixed step, >0 = adapt every N steps) |
| `MXBF` | `mxbf <int>` | `branch_switch_limit` | Maximum branch points to track before stopping continuation: integer |
| `IID` | `iid <int>` | `output_detail` | Diagnostic output detail level: `choice("silent","summary","standard","verbose","debug")` (0..5) |
| `ITMX` | `itmx <int>` | `locate_iterations` | Maximum iterations for locating special points: integer (>0) |
| `ITNW` | `itnw <int>` | `newton_iterations` | Maximum Newton-Raphson correction iterations: integer (>0) |
| `NWTN` | `nwtn <int>` | `full_newton_iterations` | Number of chord iterations before full Newton Jacobian updates: integer (>0) |
| `SUPPBP` | `suppbp <0\|1>` | `skip_branch_points` | Suppress branch point detection: boolean `true`/`false` |
| `THL` | `thl <real>` | `parameter_weight` | Weight assigned to parameters in pseudo-arclength distance metric: real |
| `THU` | `thu <real>` | `solution_weight` | Weight assigned to solution states in pseudo-arclength distance metric: real |

---

## 5. Transition Path to Stage 2 Cards

The conversion from numbered/signed options to named choices should be structured into four focused Stage 2 cards:
1. **`card-w381-numerics`**:
   - Convert `TOTAL_TIME` negative sign to `forever: boolean`.
   - Convert `METHOD` integers to solver keys.
   - Convert `POINCARE_MAP`, `POINCARE_SIGN`, `POINCARE_STOP`.
   - Convert `RangeVars` (`reset`, `oldic`, `cycle`, `movie`, `rtype`, `type`, `type2`, "Crv(1) Array(2)").
   - Convert `ishow` in BVP and `shoot` in Equilibria.
2. **`card-w381-graphics`**:
   - Convert `AXES` from 0/5 to `"2d"`/`"3d"`.
   - Convert line types `LT` (-5..1) to named stroke styles.
   - Convert palette color indices (0..10) to hex strings `#rrggbb` and `"foreground"`.
   - Convert `COLORMAP` (0..6) to named gradient schemes.
   - Convert `SIMPLOT`, `MULTIWIN`, `PS_COLOR`, `auto_freeze`.
   - Convert `g.type` and label styles.
3. **`card-w381-auto`**:
   - Convert AUTO diagram plot types (0..14) to named projections.
   - Convert special point types `itp` to named bifurcation categories.
   - Implement front-end descriptive names for AUTO constants while preserving legacy keys in foreign `.auto` files.
   - Convert `suppbp` to boolean `skip_branch_points`.
4. **`card-w381-files`**:
   - Convert window identifiers in `windows.setx`, `marks.setx`, `auto/views.setx` from integer numbers to window names.
   - Update `.odex` reader/writer to accept only named forms for modern files, relegating numeric choices to the `.ode` import converter.
   - Bump format version to 4 for W308/W381.

## Review (coordinator, 2026-10-15)

- Superseded while this study ran: the negative `TOTAL_TIME` rows (2.1's first row, 99-101, the stage 2 list) are settled by W347 A full storage is one decision in every mode ([#408](https://github.com/MuhammadMoustafa/xppautX/issues/408)): "Run until stopped" was dropped (no `forever` setting), Total is always positive, a negative Total is refused in our files and read as |Total| on a .ode/.set import, where XPPAUT also runs |Total|. The `RANGE`/`batch_range` row is done by W349 A range run with reset writes its runs' files by the run's setting, not the mode ([#410](https://github.com/MuhammadMoustafa/xppautX/issues/410)): `sweep.rangefiles`, a yes/no.
- The BVP shooting row's `ishow` is `set_up_periodic`'s "show" field, core/pp_shoot.cpp:176-206, not line 188.
- The proposals (named values, the missing and illogical options, AUTO's names on the page) are for the maintainer to settle before stage 2's cards are written.

## Decisions (maintainer, 2026-10-24)

All thirteen questions settled; they replace this study's proposals where they differ. Discussion: https://github.com/MuhammadMoustafa/xppautX/discussions/544.

1. **The rule.** Every option, setting, dialog field, protocol field and file key of ours that picks one of several things takes a name (`method: cvode`, `axes: 3d`); a yes/no is `true`/`false`. XPPAUT's numbers are read on import only (.ode, .set, .par, .ic) and `--convert` writes the names. A bare number in a file of ours is an error at its line. The format version rises (W308 question 9: 4).
2. **Colours (W478, extending W308's "hex everywhere").** `#rrggbb`, `#rrggbbaa` (alpha, written only when not opaque), the CSS colour names (read, written back as hex) and `foreground`. Alpha is drawn by the page, SVG and PDF; PostScript is removed (W479).
3. **Nullclines (`ncdraw`).** Two independent options: `nullclines.draw: true/false` and `nullclines.file: "nc.csv"` (left out: not saved); the same save is a command. `.ode`'s 0/1/2 map to their combinations, 2 with `file: "nullclines.dat"`.
4. **The transient.** `transient_time` becomes `store_from`: a time, not a duration. Stored once t has passed it in the run's direction (forward t ≥ store_from, backward t ≤ store_from), replacing XPPAUT's |t| comparison (an XPPAUT bug for runs not starting at 0: docs/xppaut-findings.md). A `store_from` at or past the run's end in its direction is an error before the run (nothing would be stored); below the start is allowed (store everything). `--check` reports it, so the VS Code extension shows it with no code of its own.
5. **Tolerances.** `relative_tolerance` and `absolute_tolerance`; the dialog shows only what the chosen method reads (the solver table's trait); a value is kept when the method does not read it.
6. **Output files.** `--silent`'s output goes through W52's data-format registry, the format from the file name (`.dat`, `.csv`, `.csv.gz`, `.npz`); the default stays `output.dat`. No Parquet or zstd.
7. **`bound`.** Split by meaning: `stop_at_limit: true/false` (default true) and `state_limit` (a positive number, default 100, XPPAUT's); the equilibria search gets its own `search_limit`; the DAE solver's cap is a named constant. Reaching the limit always says why (W480). In a sweep: one log line per stopped run (run, parameter value, variable, t, the setting and its value) and one message shown at the end grouping the causes ("12 of 50 runs stopped early: ..."); nothing extra when every run ends normally.
8. **Step limits.** `min_step`/`max_step` shown only for methods that read them; values kept across method changes; an impossible pair is an error before the run; reaching `min_step` stops with t and, where known, the variable.
9. **Fixed values.** `par`, `init` and `const` take one kind of value, read by one function: numbers, arithmetic, `pi`, consts (const arrays included), built-in functions of those and an array's index; `init` also `ran()`, drawn once at load. A parameter, state, auxiliary or fixed variable or user function is an error at its line (a quantity `d = expr` is the tool for a value worked out from parameters). `par tau = 1/30` is now allowed and saved as written.
10. **Line styles.** `draw: line | points | both`, `dash: solid | dashed | dotted | dash_dot`, `width`, `marker: circle | square | triangle | diamond | cross | plus | star`, `marker_size`; the same fields on the page, in SVG and PDF; XPPAUT's `LT` −5..1 read on import only.
11. **Colormaps.** XPPAUT's seven (`jet`, `periodic`, `hot`, `cool`, `blue_red`, `gray`, `cubehelix`), the perceptually uniform `viridis` (default for new models), `plasma`, `inferno`, `magma`, `cividis`, the diverging `coolwarm`, `rdbu`, `seismic`, the cyclic `twilight`, `hsv`, the rainbow `turbo`, and the single hues `greys`, `blues`, `reds`, `greens`, `oranges`, `purples`; licences checked before data is copied. An imported `.ode` keeps its own.
12. **AUTO's projections.** One enum: `maximum`, `norm`, `maximum_and_minimum`, `period`, `two_parameter`, `frequency`, `average`. **A choice's label is its name**, underscores as spaces and a capital first letter, derived, never written apart (for every choice: methods, colormaps, styles).
13. **Windows** (W308 question 6, settled 2026-10-14; the default changed 2026-10-24). Every graph and AUTO view has a name and is never numbered; the defaults are `graph_1`, `graph_2`, ... and `diagram_1`, ...; a name is the user's text, shown and referenced exactly as written, spaces allowed (not empty, no leading or trailing space, unique).

**User text is always quoted, a choice never.** In every file of ours and in `--server`'s text form, anything the user names or writes is a double-quoted string even without spaces (window, curve and key names, labels, titles, paths, set names: `set "hopf" { ... }`); choices and keywords stay bare one-word tokens.

**Every stop says why** (W480): a computation that ends before what was asked reports what stopped, the setting and its value, and the model line.
