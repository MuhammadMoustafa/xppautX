# The main commands

Results such as a successful curve fit, a mean and standard deviation, a
Liapunov exponent, a saved boundary-value point, and AUTO toggle states
appear in the status bar. They do not open an error dialog. If the
kinescope fills, the command reports one error; frames already captured
remain available. A script or `--silent` run exits 1 if any error occurred,
including an output file it could not write; a successful fit exits 0
(W133).

All commands can be invoked by typing the hot key for that command (capitalized on the menu) or clicking on the menu with the mouse. Usually most commands can be aborted by pressing the `Esc` key. Once one of these is chosen, the program begins to calculate and draw the trajectories. If you want to stop prematurely, press the `Esc` key and the integration will stop.

**In web2**, the main menu and every one of these commands and hotkeys
are unchanged; see [Using the interface](04-using-the-interface.md) for
the menu panel, the values panel (parameters, ICs, sliders), where dialogs
and prompts appear, and how Stop (the status bar) replaces the X11
window-specific Abort/Esc behaviour for a long-running command described
below.

### (I)nitial conds

This invokes a list of options for integrating the differential equations. The choices are:
- **(R)ange**: This lets you integrate multiple times with the results shown in the graphics window. Pressing this option produces a new window with several boxes to fill in. First choose the quantity you want to range over. It can be a parameter or a variable. The integrator will be called and this quantity will be changed at the beginning of each integration. Then choose the starting and ending value and the number of steps. The option Reset storage only stores the last integration. If you choose not to reset, each integration is appended to storage. Most likely, storage will be exceeded and the integration will overwrite or stop. The option to use last initial conditions will automatically use the final result of the previous integration as initial dat for the next integration. Otherwise, the current ICs will be used at each step (except of course for the variable through which you are ranging.) If you choose `Yes` in the `Movie` item, then after each integration, XPP will take a snapshot of the picture. You can then replay this series of snapshots back using the Kinescope. When you are happy with the parameters, simply press the OK button. Otherwise, press the Cancel button to abort. Assuming that you have accepted, the program will compute the trajectories and plot them storing none of them or all of them. If you press `Esc` it will abort the current trajectory and move on to the next. Pressing the `/ ` key will abort the whole process.
- **(2)par range**: is similar to range integration but allows you to range over two items. The `Crv(1) Array(2)` item determines how the range is done. If you choose `Crv` then the two paramaters are varied in concert, $`[a(i),b(i)]`$ for $`i=0,\ldots,N`$. The more useful `Array` varies them independently as $`[a(i),b(j)]`$ for $`i=0,\ldots,N`$ and $`j=0,\ldots,M.`$
- **(L)ast**: This uses the end result of the most recent integration as the starting point of the curret integration.
- **(O)ld**: This uses the most recent initial data as the current initial data. It is essentially the same as Go.
- **(G)o**: which uses the initial data in the IC window and the current numerics parameters to solve the equation. The output is drawn in the current selected graphics window and the data are saved for later use. The solution continues until either the user aborts by pressing `Esc`, the integration is complete, or storage runs out.
- **(M)ouse**: allows you to specify the values with the mouse. Click at the desired spot.
- **(S)hift**: This is like Last except that the stating time is shifted to the current integration time. This is irrelevant for autonomous systems but is useful for nonautonomous ODEs.
- **(N)ew**: This prompts you at the command line for each initial condition. Press `Return` to accept the value presented.
- **s(H)oot**: allows you to use initial data that was produced when you last searched for an equilibrium. When a rest state has a single positive or negative eigenvalue, then XPP will ask if you want to approximate the invariant manifold. If you choose `yes` to this, then the initial data that were used to compute the trajectories are remembered. Thus, when you choose this option, you will be asked for a number 1-4. This number is the order in which the invariant trajectories were computed. Note if the invariant set is a stable manifold, then you should integrate backwards in time.
- **(F)ile**: prompts you for a file name which has the initial data as a sequence of numerical values.
- **form(U)la**: allows you to set all the initial data as a formula. This is good for systems that represent chains of many ODEs. When prompted for the variable, type in `u[2..10]` for example to set the variables `u2,u3, ..., u10` and then put in a formula using the index `[j]`. Note you must use `[j]` and not `j` by itself. For example `sin([j]*2*pi/10)`. Repeat this for different variables hitting enter twice to begin the integration.
- **M(I)ce**: allows you to choose multiple points with the mouse. Click Esc when done.
- **(D)AE guess**: lets you choose a guess for the algebraic variables of the DAE.
- **(B)ackward**: is the same as “Go” but the integration is run backwards in time.

### (C)ontinue

This allows you to continue integrating appending the data to the current curve. Type in the new ending time.

### (N)ullclines

This option allows you to draw the nullclines of systems. They are most useful for two-dimensional models, but XPP lets you draw them for any model. The constraints are the same as in the direction fields option above. The menu has 4 items.
- **(N)ew**: draws a new set of nullclines.
- **(R)estore**: restores the most recently computed set.
- **(A)uto**: turns on a flag that makes XPP redraw them every time it is necessary because some other window obscured them.
- **(M)anual**: turns this flag off so that you must restore them manually. The X-axis nullcline is blue and the Y-axis nullcline is red.
- **(F)reeze**: allows you to freeze and play back multiple nullclines
  - **(F)reeze**: freezes the current nullclines
  - **(D)elete all**: deletes all frozen nullclines
  - **(R)ange**: lets you vary a parameter through some range, computes the nullclines and stores them
  - **(A)nimate**: redraws all the frozen nullclines erasing the screen between each one. The user specifies a delay between drawing.
- **(S)ave**: saves the nullclines into a file. They can then be plotted using other software. **NOTE:** The way that XPP computes nullclines (by computing zero contours of a two-variable function) means that the nullclines are composed of a series of small line segments. This means that if you try to plot them as a continuous curve, your plotting program will produce garbage. Thus, you should plot them as points and not draw line segments between them. The data file produced has 4 columns. The first two are the “x” nullcline and the second two are the “y” nullcline. Data is as follows:
- xnx1 xny1 ynx1 yny1     xnx2 xny2 ynx2 yny2     ...
- There are an even number of entries. The “x” nullcline consists of segments `(xnx1,xny1),(xnx2,xny2)` between pairs of points. Similarly for the “y” nullcline.

### (D)irection Field/Flow

This option is best used for two-dimensional systems however, it can be applied to any system. The current graphics view must be a two-d plot in which both variables are different and neither is the time variable, T. There are five items.
- **(D)irection fields**: Choosing the direction field option will prompt you for a grid size. The two dimensional plane is broken into a grid of the size specified and lines are drawn at each point specifying the direction of the flow at that point. The length of the line gives the magnitude. If the system is more than two-dimensional, the other variables will be held at the values in the initial conditions window.
- **(F)low**: Choosing the flow, you will be prompted for a grid size and trajectories started at each point on the grid will be integrated according to the numerical parameters. Any given trajectory can be aborted by pressing `Esc` and the whole process stopped by pressing `/`. The remaining variables if in more than two-dimensions are initialized with the values in the IC window.
- **(N)o Dir Field**: turns off redrawing of direction fields when you click on (Redraw). Erasing the screen automatically turns this off.
- **(C)olorize**: This draws a grid of filled rectangles on the screen whose color is coded by the velocity or some other quantity. (See the Numerics Colorize menu item).
- **(S)caled Dir. Fld**: is the same as (D)irection field but the lengths are all scaled to 1 so only directional information is given.

### (W)indow

allows you to rewindow the current graph. Pressing this presents another menu with the choices:
- **(W)indow**: A parameter box pops up prompting you for the values. Press OK or CANCEL when done.
- **(Z)oom in**: Use the mouse to expand a region by clicking, dragging and releasing. The view in the rectangle will be expanded to the whole window.
- **Zoom (O)ut**: As above but the whole window will be shrunk into the rectangle.
- **(F)it**: The most common command will automatically fit the window so the entire curve is contained within it. For three-D stuff the window data will be scaled to fit into a cube and the cube scaled to fit in the window. Use this often.

### ph(A)se space

XPP allows for periodic domains so that you can solve equations on a torus or cylinder. You will be prompted to make (A)ll variables periodic, (N)o variables periodic or (C)hoose the ones you want. You will be asked for the period which is the same for all periodic variables (if they must be different, rescale them) Choose them by clicking the appropriate names from the list presented to you. An `X` will appear next to the selected ones. Clicking toggles the `X`. Type `Esc` when done or CANCEL or DONE. XPP mods your variables by this period and is smart enough when plotting to not join the two ends.

### (K)inescope

This allows you to capture the active window and play it back. Another menu pops up with the choices:
- **(C)apture**: which takes a snapshot of the currently active window
- **(R)eset**: which deletes all the snapshots
- **(P)layback**: which cycles thru the pictures each time you click the left mouse button and stops if you click the middle.
- **(A)utoplay**: continuously plays back snapshots. You tell it how many cycles and how much time between frames in milliseconds.
- **(S)ave**: Save each frame as a GIF, with a file dialog for each destination; Cancel writes no new frame
- **(M)ake anigif**: Create an animated gif from the frames. A file dialog offers `<model>.gif`.

**In web2** a snapshot is data (the series, marks and viewport of the
active plot window, docs/ui-v2.md T15), not a bitmap: reloading the
page loses captured frames (they live in the client). Make anigif asks for its destination, then the core
writes the GIF itself, asking the page for each frame's
pixels, so the picture is only ever as good as what's on screen, not a
copy of an X11 pixmap.

### (G)raphic stuff

This induces a popup menu with several choices.
- **(A)dd curve**: This lets you add another curve to the picture. A parameter box will appear aking you for the variables on each axis, a color, and line type (1 is solid, 0 is a point, and negative integers are small circles of increasing radii.) All subsequent integrations and restorations will include the new graph. Up to 10 per window are allowed.
- **(D)elete last**: Will remove most recent curve from the added list.
- **(R)emove all**: Deletes all curves but the first.
- **(E)dit curve**: You will be asked for th curve to edit. The first is 0, the second 1, etc. You will get a parameter box like the add curve option.
- **(P)ostscript**: This will ask you for a file name and write a postscript representation of the current window. Nullclines, text, and all graphs will be plotted. You will be asked for Black and White or Color. Color tries to match the color on the screen. Black and white will use a variety of dashed curves for the plots. The Land/Port option lets you draw either in Landscape (default) or Portrait style. Note that Portrait is rather distorted and is created in for those who cannot rotate their postscript plots. Font size sets the size of the fonts on the axes.
- **(F)reeze**: This will create a permanent curve in the window. Usually, when you reintegrate the equations or load in some new data, the current curve will be replace by the new data. Freeze prevents this. Up to 26 curves can be frozen.
  - **(F)reeze** : This freezes the current curve 0 for the current plotting window. It will not be plotted in other windows. If you change the axes from 2 to 3 dimensions and it was frozen as a 2D curve (and *vice versa* ) then it will also not be plotted. It is better to create another window to work in 3 dimensions so this is avoided. A parameter box pops up that asks you for the color (linetype) as well as the key name and the curve name. The curve name is for easy reference and should be a few characters. The key name is what will be printed on the graph if a key is present.
  - **(D)elete**: This gives you a choice of available curves to delete.
  - **(E)dit**: This lets you edit a named curve; the key, name, and linetype can be altered.
  - **(R)emove all**: This gets rid of all of the frozen curves in the current window.
  - **(K)ey**: This turns the key on or off. If you turn it on, then you can position it with the mouse on the graph. The key consists of a line followed by some text describing the line. Only about 15 characters are permitted.
  - **(B)if.diag**: will prompt you for a filename and then using the current view, draw the diagram. The file must be of the same format as is produced by the `Write pts` option in the AUTO menus. (see AUTO below.) The diagram is colored according to whether the points are stable/unstable fixed points or periodics. The diagram is “frozen” and there can only be one diagram at a time.
  - **(C)lr. BD**: clears out the current bifurcation diagram.
  - **(O)n freeze**: Toggles a flag that automatically freezes the curves as you integrate them.
- **a(X)es opts**: This puts up a window which allows you to tell XPP where you want the axes to be drawn, whether you want them, and what fontsize to make the PostScript axes labels.
- **exp(O)rt**: saves what the plot shows: this is the data browser's Save data with *What the plot shows* already chosen (see [(W)rite: Save data](07-data-browser.md#write-save-data)). It asks the format (XPP's `.dat`, CSV, compressed CSV or NumPy's `.npz`) and the file, and writes the window's curves and then its frozen curves as one table with the columns `curve`, `x`, `y` (and `z` in 3D), one row per point (in `.npz`, one array per curve). It replaces XPP's old XY export (the first curve's x followed by every curve's y on each row, in `%g`): a table with a curve column holds every curve in full, frozen curves and 3D plots included.
- **(C)olormap**: This lets you choose a different color map from the default. There are a bunch of them; try them all and pick your favorite.

### n(U)merics

This is so important that a section is devoted to it. See below.

### (F)ile

This brings up a menu with several options. Type `Esc` to abort.
- **(P)rt src**: Brings up a window with the source code for the ODE file. If you click on `Action` it brings up the active comments so you can make little tutorials.
- **(R) Import XPPAUT set**: This imports a `.set` file that XPPAUT wrote (xppautX no longer writes one: **sa(V)e session** holds everything a set file did and more). The file is very tightly connected to the ODE file it was written for, so you should not import one from a different problem. The file must end with the equations XPPAUT writes after its last value (`RHS etc ...`, not read); one without them, such as a session's `model.set`, is refused. A set file is read whole and checked before anything is taken from it: every named value must have the open model's expected name (including variables, parameters and torus entries), and every setting its expected label. The first mismatch or bad value is an error naming the file, line and line as written, and nothing changes. On success the imported values are immediately saved as a session, `<name>.snapx` beside the `.set` (`name` is its base name), and that session is now open; one message names it. If saving fails, the error says so and the valid imported values stay applied.
- **(A)uto**: This brings up the AUTO window. See below for a description of this.
- **(C)alculator**: This pops up a little window. Type formulae in the command line involving your variables and the results are displayed in the popup. Click on Quit or type `Esc` to exit.
- **(S)ave info**: This is like `(P)rt info` but saves the info to a file. It is human readable.
- **(H)elp**: Opens this manual, at this chapter.
- **(Q)uit**: Asks "Quit xppautX? Save this session first?", the one
  question every way of leaving a session asks (the desktop window's
  File > Quit and its close box too; **open (M)odel** and **r(E)load**
  ask it in their own words): **Save session** (`S`) writes a session file
  first, as **sa(V)e session** does, then quits; **Don't save** (`D`)
  quits; **Cancel** (`Esc`) keeps working. A recording in progress is saved
  with the session (its name is asked as **Stop** asks it). The desktop
  window's close box asks while something computes without stopping it
  (Cancel leaves the run going; Save session stops it, then saves). In
  the browser,
  closing the tab does not end xppautX (the browser asks whether to leave
  the page): File/Quit is the way to end it there.
- **(T)ranspose** : This is not a very good place to put this but I stuck it here just to get it into the program. The point of this routine is to allow one to transpose chunks of the output. For example, if you are solving the discretization of some spatial problem and find a steady state, there is no way to plot the steady state as a function of the index of the discrete system. This routine lets you do that. The idea is to take something that looks like:
- t1  x11  x21  x31 ... xm1      t2  x12  x22  x32 ... xm2     ...     tn  x1n  x2n  x3n ... xmn
- and transpose some subset of it. You are prompted for 6 items. They are the name of the first column you want to index, the number of columns (`ncols` and amount you want to skip across columns, ` colskip` (so that `colskip = 2` would be every other column. You must also provide the starting row `j1`, the number of rows, ` nrows` and the row skip, `rowskip.` the The storage array is temporarily replaced by a new array that has `M=ncols` rows and `nrows+1` columns (since the data is transposed, the rows and columns are as well; confusing ain’t it). The form of the array is:
- 1  x(i1,j1) x(i1,j2) x(i1,j3) ...     2  x(i2,j1) x(i2,j2) x(i2,j3) ...     ...     M  x(iM,j1) x(iM,j2) x(iM,j3) ...
- where `i2=i1+colskip, i3=i1+2*colskip, ...` and `i1` is the index corresponding to the name of the first column you provide. Similarly, `j2=j1+rowskip, ...`. As a brief example, suppose that you solve a system of equations of the form: ``` math x_j' = f(x_{j-1},x_j,x_{j+1},I_j) ``` where $`j=1,\dots,20.`$ Click on transpose and choose `x1` as the first column, `colskip=1, ncols=20` and say `row1=350, nrows=1,rowskip=1` then a new array will be produced. The first column is the index from 1 to 20 and the second is `xj(350)` where 350 is the index and not the actual value of time. By plotting the second column versus the first you get a “spatial profile.”
- **(G)et par set**: This loads one of the parameter sets that you have defined in the ODE file. Every item is checked first: an item whose value is not a number, or an option that does not take it, is an error at the set's line, and nothing of the set is applied.
- **c(O)py set line**: Asks for a name for the set (`set1`, `set2`, ... is
  suggested; the name must be a valid name and not already a set of the
  model), shows the line and, on Copy, puts it on the clipboard:
  `set name {a=1,b=2,...,x=0.5,...}` with every parameter and initial
  condition as they are now, numbers exactly as they read back. Paste it
  into your `.ode` (XPP never writes it for you), reload the model
  (**r(E)load** below), and
  the regime is a named set in **(G)et par set**, keeping its meaning
  whatever else you change in the file. If the browser refuses the
  clipboard, the line stays on screen to copy by hand.
- **c(L)one**: Asks for a file name and writes a new ODE file next to it that
  reproduces the current model: the source lines, with the parameters and
  boundary conditions replaced by their current (possibly since-edited)
  values as comments for you to fold back in.
- **.(X)pprc**: Opens your `~/.xpprc` (`%USERPROFILE%\.xpprc` on Windows) in
  the editor named by the `XPPEDITOR` environment variable; an error if it
  is not set.
- **t(U)torial**: Steps through a series of short tips ("Did you know you
  can...") one at a time; Next for another, Done to stop.
- **open (M)odel**: A `.ode` converts and saves as `.odex` beside it,
  which becomes the open model (see [.odex](02-ode-files.md#odex)).
  Asks for a `.ode` or `.odex` file (or a `.snapx`
  session file, which opens that session: **opeN session** below; or a `.recx`
  recording, which opens in the player), then
  whether to save this session first (**Save session** writes a session
  file, **Don't save**; Escape keeps the current model), and loads it in
  place of the current model: its data,
  diagram and windows go, and the new model starts as a double-click would
  start it, from its own folder. A file that cannot be loaded changes
  nothing: an error says so and the current model goes on (see
  [Opening another model](01-introduction.md#starting-it)).
- **r(E)load**: Reads the model's file again (edit it in your editor, then
  Reload), after asking whether to save this session first, as **open
  (M)odel** does (its data and diagram go). Parameters, initial data, numerics and AUTO settings keep their values by name;
  what the file adds comes with the file's values, and what it drops is
  left out. A file that no longer loads changes nothing.
- **sa(V)e session**: Asks for a file name and writes one session file,
  `name.snapx`, to continue later exactly where you are: the model itself
  (its `.odex` and every included file it read), the values and numerics (the set format), every
  plot window with its axes, variables and zoom, the text, arrows and
  frozen curves, AUTO's diagram and settings (saved under `auto/`) and views, and the data table (NumPy's `.npz`). The earlier runs a
  window keeps until Erase are left out. A data table above 50 MB asks
  whether to leave it out (**Leave it out**: Go computes it again).
- **ope(N) session**: Asks for a `.snapx` file, then whether to save this
  session first (as **open (M)odel**), loads the model saved in it, from the file alone however
  the `.ode` has changed since, and restores the session as it was saved.
  A session file without its model (saved before this version), or with
  a part missing or damaged, is refused with an error naming the part and
  its line, and the current session stays as it was. See
  [session files](01-introduction.md#starting-it).
- **recor(D)**: Starts recording what you do, step by step; press it
  again (or **Stop** on the red recording bar the page shows, or
  **Record** in the title bar to start) to stop, and it asks for a file
  name and writes one plain text file, `name.recx`, next to the model: the
  session as it was when you pressed Record (what **sa(V)e session**
  writes, without the data table: the values, numerics, windows and AUTO's
  diagram, so a recording begun in the middle of your work plays back
  from exactly there), the model itself and every file the session read while recording (a set, a
  parameter file, a table, an animation; an AUTO file or a session file
  you opened, written as base64 text), then the steps. A step is everything
  one command does until XPP is ready again: **Initialconds** then **Go**
  is one step, with the keys you pressed, a dialog's answers are part of
  the step that asked, a menu you left with `Esc` is a step too, and a run
  you stopped keeps where it stopped. Each zoom or pan is a step of its
  own; the time you spent between steps is not recorded. While recording,
  the bar's **Note for the next step** box holds a note that goes with the
  next step you take (a caption for whoever plays it back); notes can also
  be written afterwards in any text editor, as `#` lines just above a
  step. The file ends with a fingerprint of the embedded files and the
  steps (not the notes): editing a note keeps it, changing a step or a file
  does not. A recording holds no data: playing it back computes everything
  again. A key a running command reads itself (`/` ending a range, `Esc`
  stopping the animation's **Go**) is recorded with where it came.
- **pla(Y) recording**: Asks for a `.recx` file (or use **Play a
  recording** in the title bar, or **open (M)odel** on a `.recx`), then
  whether to save this session first, and loads the session the
  recording began from (its model, values, windows and diagram, from the
  recording itself; a file without it is refused with an error). Its steps then play back exactly as they were
  taken, nothing fixed or skipped: before each step the note written for
  it shows as a large caption above the plot, the keys it pressed light up
  one by one in a box at the top of the plot (with the menu item lit in
  the menu panel, a button lit when the step was a click, a dialog's
  answers filled in before its **OK**), then the step runs. Only
  computing takes time: after each step the player waits a moment to
  let you see it (less for a zoom or a pan), then goes on. The controls
  below the plot: **Play**/**Pause**, **Step** (one step, then pause),
  **Restart**, and the speed (0.25x, 0.5x, 1x, 2x, 4x, 8x; the core gives the bounds); the progress shows a
  segment per step (a half one for a view step). The step list at the
  right shows every step with its note; click one to write or change its
  note and **Save note** (it goes into the `.recx`; the fingerprint stays
  valid, as notes are not part of it) or **Play from here** (the steps
  before it run at once, then it plays). The files a step read come from
  the recording, never from the disk, and the replay writes its output in
  a scratch folder of its own, so it never touches your files and plays
  the same every time. A recording changed after it was made (a step or a
  file that no longer matches the fingerprint) shows the banner "This
  recording was changed after it was made" and still plays. A step that
  goes where the recording does not (it asks a question the recording
  does not answer) stops the player with an error; the question is then
  yours to answer.

### (P)arameters

Type the name of a parameter to change and enter its value. Repeat for more parameters. Hit `Enter` a few times to exit. Type `default` to get back the values when you started XPP. Use this if you don’t want to mess with the mouse.

### (E)rase

erases the contents of the active window, redraws the axes, deletes all text in the window, and sets the redraw flag to Manual.

### (M)ake window

The option allows you to create and destroy graphics windows. There are several choices.
- **(C)reate**: makes a copy of the currently active window and makes itself active. You can change the graphs in this window without affecting the other windows.
- **(K)ill all**: Removes all but the main graphics window.
- **(D)estroy**: This destroys the currently active window. The main window cannot be destroyed.
- **(B)ottom**: puts the active window on the bottom.
- **(A)uto**: turns on a flag so that the window will automatically be redrawn when needed.
- **(M)anual**: turns off the flag and the user must restore the picture manually.
- **(S)imPlot on/off**: lets you plot the solution in all active windows while the simulation is running. This slows you down quite a bit.
- After a window is created, you can use the mouse to find the coordinates by pressing and moving in the window. The coordinates are given near the top of the window.

### (T)ext, etc

allows you to write text to the display in a variety of sizes and in two different fonts. You can also add other symbols to your graph.
- **Text**: This prompts you for the text you want to add. Then you are asked for the size; there are five choices (0-5): 0-8pt, 1-12pt, 2-14pt, 3-18pt, 4-24pt. Text also has several escape sequences:
  - $`\backslash`$<!-- -->1 – switches to Greek font
  - $`\backslash`$<!-- -->0 – switches to Roman font
  - $`\backslash`$s – subscript
  - $`\backslash`$S – superscript
  - $`\backslash`$n – neither sub nor superscript
  - $`\backslash`${expr} – evaluate the expression in the braces before rendering.
- Note that not all X-servers will have these fonts, but the postscript file will still draw them. Finally, place the text with the mouse.
- **Arrow**: This lets you draw an arrow-head to indicate a direction on a trajectory. You will be prompted for the size, which should be some positive number, usually less than 1. Then you must move the the mouse and select a direction and starting point. Click on the starting point and holding the mouse button down, drag the mouse to indicate the direction of the arrow-head. Then release the mouse-button and the arrow will be drawn.
- **Pointer**: This is like an arrow, but draws the stem as well as the arrow head. It can be used to point to important features of your graph. The prompts are like those for `Arrow.`
- **Marker**: This lets you draw little markers, such as triangles, squares, etc on the picture. When prompted to position the marker with the mouse, you can over-ride the mouse and manually type in coordinates if you hit the (Tab) key.
- **Edit**: This lets you edit the text, arrows, and pointers in one of three ways:
  - **Move**: lets you move the object to another location without changing any of its properties. Choose the object with the mouse by clicking near it. You will then be prompted as to whether you want to move the item that XPP selected. If you answer `yes` use the mouse to reposition it.
  - **Change**: lets you change the properties: for text, the text itself, size, and font can be change; for arrows and pointers, only the size of the arrow head can be changed. As above, select the object with the mouse and then edit the properties.
  - **Delete**: deletes the object that you select with the mouse.
- **(D)elete All**: Deletes all the objects in the current window.
- **marker(S)**: This is similar to the Marker command, but allows you to automatically mark a number of points along a computed trajectory. You use the data browser to move the desired starting point of the list to the top line of the browser. Then click on the (Text) (markerS) command and choose a size and color. Then tell XPP how many markers and how many browser lines to jump between markers. (Thus, 10 would put a marker at every 10th data point)..

### (S)ing pts

This allows you to calculate equilibria for a discrete or continuous system. The program also attempts to determine stability for delay-differential equations (see below in the numerics section.) There are three options.
- **(G)o**: begins the calculation using the values in the initial data box as a first guess. Newton’s method is applied. If a value is found XPP tries to find the eigenvalues and asks you if you want them printed out. If so, they are written to the console. Then if there is a single real positive or real negative eigenvalue, the program asks you if you want the unstable or stable manifolds to be plotted. Answer yes if so and they will be approximated. The calculation will continue until either a variable goes out of bounds or you press `Esc`. If `Esc` is pressed, the other branch is computed. (Unstable manifolds are yellow and computed first followed by the stable manifolds in color turquoise.) The program continues to find any other invariant sets until it has gotten them all. These are not stored, however, the initial data needed to create them are and can be accessed with the `Initial Conds` ` sHoot` command. Once an equilibrium is computed a window appears with info on the value of the point and its stability. The top of the window tells you the number of complex eigenvalues with positive,`(c+)`, negative `(c-)`, zero `(im)` real parts and the number of real positive `(r+)` and real negative `(r-)` eigenvalues. If the equation is a difference equation, then the symbols correspond the numbers of real or complex eigenvalues outside `(+)` the unit circle or inside `(-)` This window remains and can be iconified.
- **(M)ouse**: This is as above but you can specify the initial guess by clicking the mouse. Only the two variables in the two-D window will reflect the mouse values. This is most useful for 2D systems.
- **(R)ange**: This allows you to find a set of equilibria over a range of parameters. A parameter box will prompt you for the parameter, starting and ending values, number of steps. Additionally, two other items are requested. Column for stability will record the stability of the equilibrium point in the specified column (Use column number greater than 1). If you elect to shoot at each, the invariant manifolds will be drawn for each equilibrium computed. The stability can be read as a decimal number of the form `u.s ` where `s` is the number of stable and `u` the number of unstable eigenvalues. So `2.03` means 3 eigenvalues with negative real parts (or in the unit circle) and 2 with positive real parts (outside the unit circle.) For delay equations, if a root is found, its real part is included in this column rather than the stability summary since there are infinitely many possible eigenvalues. The result of a range calculation is saved in the data array and replaces what ever was there. The value of the parameter is in the time column, the equilibria in the remaining columns and the stability info in whatever column you have specified. `Esc` aborts one step and `/` aborts the whole procedure. As with the initial data/range option, you can also make a movie. This is useful mainly for systems where invariant sets are to be computed.

### (V)iew axes

This selects one of different types of graphs: a 2D box or a 3D Box; brings up a threed window; or lets you create animations of your simulation. If you select the 2D curve, you will be asked for limits as in the window command as well as the variables to place on the axes and the labels for the axes. 3D is more complicated. You will be asked for the 3 variables for the 3 axes, their max and min values and 4 more numbers, `XLO`, etc. XPP first scales the data to fit into a cube with corners (-1,-1,-1) and (1,1,1). Rotation of this cube is performed and then projected into the two-D window. `XLO`, etc define the scales of this projection and are thus unrelated to the values of your data. You are also asked for labels of the axes. Use the `(F)it ` option if you don’t know whats going on.
- (A)rray plots introduce a new window that lets the user plot many variables at once as a function of time with color coded values. The point is to let one plot, e.g., an array of voltages, `V1, V2, ..., VN` across the horizontal as time varies in the vertical dimension. For example, suppose you have discretized some PDE and want to see the evolution in space and time of the variables. Then use this plotting option. You will be prompted for the first column name of the “array”, then number of columns, the first row, then number of rows, and the number of rows to skip. For example, if the discretized PDE variables are `u0,u1, ... ,u50`, then type in ` u0` for the first element and `51` for the number of columns. If you want rows 200 through 800 only every 4th time unit, you would put 200 for the first row, 201 as the number of rows, and 4 as the skip value. You can also set the column skip as well. This is useful if you have defined a series of variables with the array blocks. If the array block is for a two-component system, then plot every 2, so you would put 2 in this entry. The `Print` button asks you for a file name and top and bottom labels and a render style. The render styles are:
  - **-1**: Grey scale
  - **0**: Blue-red
  - **1**: Red-Yellow-Green-Blue-Violet
  - **2**: Like 1 but periodic
- Render must be a whole number from -1 through 2. Malformed or out-of-range text reports an error before the filename ask; no picture is written (W177, #229).
- Array plot `Range` writes still GIFs or one movie, according to `Still(1/0)`. A movie replaces its destination only after the range finishes; Stop, a failed integration or a cancelled picture leaves an existing movie untouched.
- The `Style` button does nothing yet. The `Edit` button lets you change ranges and arrays to plot. The `Redraw` button is obvious.
- Since the animation option requires learning lots of new stuff, see [Creating Animations](10-animations.md) for a description of the animation language and what you can do with it.

The plot's Graphic stuff > Freeze > Import diagram reads a six-column `.dat` or `.csv` table: x, low, high, type, branch and two-parameter flag. A CSV header is skipped. The richer AUTO Export CSV has a different layout.

### (X)i vs t

This chooses a certain 2D view and prompts you for the variable name. The window is automatically fitted and the data plotted. It is a shortcut to choosing a view and windowing it.

### (R)estore

redraws the most recent data in the browser in accordance with the graphics parameters of the active window.

### (3)d params

This lets you choose rotations of the axes and perspective planes. Play with this to see. You must be have a 3D view in the active graph to use this.
- There is another selection: `Movie`. If you choose ` Yes` for this, then after you click `Ok`, you will be prompted for some additional parameters. There are two angles you can vary, ` theta` and `phi`. Choose one, give an initial value, an increment, and the number of rotations you want to perform. XPP will then use the `Kinescope` to take successive snapshots of the screen after performing each rotation. You can then play these back from the Kinescope or save them as an animated gif.

### (B)ndry val

This solves boundary value problems by numerical shooting. There are 4 choices.
- **(S)how**: This shows the successive results of the shooting and erases the screen at the end and redraws the last solution. The program uses the currently selected numerical integration method, the current starting point, `T0` as the left end time and `T0+TEND` as the right end. Thus, if the interval of interest is `(2.5,6)` then set `T0=2.5` and ` TEND=3.5` in the numerics menu.
- **(N)o show**: This is as above but will not show successive solutions.
- **(R)ange**: This allows you to range over a parameter keeping starting or ending values of each of the variables. A window will appear asking you for the parameter, the start, end, and steps. You will also be asked if you want to cycle color which means that the results of each successful solution to the BVP will appear in different colors. Finally the box labeled`side` tells the program whether to save the initial `(0)` or final`(1)` values of the solution. As the program progresses, you will see the current parameter in the info window under the main screen. You can abort the current step by pressing `Esc` and the whole process by pressing `/`. As in the `Initialconds Range` option, you can also choose `Movie`. Then, as before, after each solution is computed, a snapshot is take. Thus, you can playback the solutions as a function of the range parameter.
- **(P)eriodic**: Periodic boundary conditions can be solved thru the usual methods, but one then must write an addition equation for the frequency parameter. This option eliminates that need so that a 2-D autonomous system need not be suspended into a 3D one. You will be asked for the name of the adjustable parameter for frequency. You will also be asked for the section variable and section. This is an additional condition that must be satisfied, namely, $`x(0)=x_0`$ where $`x`$ is the section variable and $`x_0`$ is the section. Type `yes` if you want the progress shown.
