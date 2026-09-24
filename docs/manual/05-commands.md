# The main commands

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
(In web2, Zoom/Zoom out/Fit are client-side plot modes drawn on the canvas — see [Using the interface](04-using-the-interface.md#plots-and-axes) — so there is no `-xorfix`-style rubber-band drawing bug to work around.)

### ph(A)se space

XPP allows for periodic domains so that you can solve equations on a torus or cylinder. You will be prompted to make (A)ll variables periodic, (N)o variables periodic or (C)hoose the ones you want. You will be asked for the period which is the same for all periodic variables (if they must be different, rescale them) Choose them by clicking the appropriate names from the list presented to you. An `X` will appear next to the selected ones. Clicking toggles the `X`. Type `Esc` when done or CANCEL or DONE. XPP mods your variables by this period and is smart enough when plotting to not join the two ends.

### (K)inescope

This allows you to capture the active window and play it back. Another menu pops up with the choices:
- **(C)apture**: which takes a snapshot of the currently active window
- **(R)eset**: which deletes all the snapshots
- **(P)layback**: which cycles thru the pictures each time you click the left mouse button and stops if you click the middle.
- **(A)utoplay**: continuously plays back snapshots. You tell it how many cycles and how much time between frames in milliseconds.
- **(S)ave**: Save the frames in either ppm or gif format
- **(M)ake anigif**: Create an animated gif from the frames. The file is always called `anim.gif`.

**In web2** a snapshot is data (the series, marks and viewport of the
active plot window, docs/ui-v2.md T15), not a bitmap: reloading the
page loses captured frames (they live in the client), and Save/Make
anigif render the GIF or PNG in the page itself from that data, so the
picture is only ever as good as what's on screen, not a copy of an X11
pixmap.

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
- **exp(O)rt**: This lets you save the points that are currently plotted on the screen in XY format. Thus if you have a phaseplane on the screen, only the X and Y values are saved. This makes it compatible with porgrams like XMGR which assume X Y1 Y2 ... data. If you have several traces on the screen at once, it saves the X values of the first trace and the Y values of the first and all subsequent traces.
- **(C)olormap**: This lets you choose a different color map from the default. There are a bunch of them; try them all and pick your favorite.

### n(U)merics

This is so important that a section is devoted to it. See below.

### (F)ile

This brings up a menu with several options. Type `Esc` to abort.
- **(P)rt info**: Brings up a window with the source code for the ODE file. If you click on `Action` it brings up the active comments so you can make little tutorials.
- **(W)rite set**: This creates a file with all of the info about the current numerics, etc as well as all of the currently highlighted graphics window. It is readable by the user. It in some sense saves the current state of XPP and can be read in later.
- **(R)ead set**: This reads a set that you have previously written. The files are very tightly connected to the current ODE file so you should not load a saved file from one equation for a different problem.
- **(A)uto**: This brings up the AUTO window if you have installed AUTO. See below for a description of this.
- **(C)alculator**: This pops up a little window. Type formulae in the command line involving your variables and the results are displayed in the popup. Click on Quit or type `Esc` to exit.
- **(E)dit**: You can edit the equations from within XPP. *Note that XPP is capable of understanding right-hand sides of up to 256 characters. However, the RHS editor will not accept anything longer than about 72 characters.* This menu item presents a list of four options:
  - **(R)HS**: Edit the right-hand sides of the ODEs IDEs, and auxiliary variables. If you are happy with the editing, then type `TAB` or click on `OK.` The program will parse the new equations and if they are syntactically correct, alter the corresponding equation. If there is an error, the you will be told of the offending right-hand-side and that will not be changed.
  - **(F)unctions**: This lets you alter any user-defined functions. It is otherwise the same as the above.
  - **(S)ave as**: This creates an “ODE” file based on the current parameter values, functions, and right-hand sides. You will be asked for a filename.
  - **(L)oad DLL**: invokes the dynamic linker. You can load in complicated RHS’s that would be awkward to create using XPP’s simple language.
- **(S)ave info**: This is like `(P)rt info` but saves the info to a file. It is human readable.
- **(H)elp**: Opens this manual, at this chapter.
- **(Q)uit**: This exits XPP first asking if you are sure.
- **(T)ranspose** : This is not a very good place to put this but I stuck it here just to get it into the program. The point of this routine is to allow one to transpose chunks of the output. For example, if you are solving the discretization of some spatial problem and find a steady state, there is no way to plot the steady state as a function of the index of the discrete system. This routine lets you do that. The idea is to take something that looks like:
- t1  x11  x21  x31 ... xm1      t2  x12  x22  x32 ... xm2     ...     tn  x1n  x2n  x3n ... xmn
- and transpose some subset of it. You are prompted for 6 items. They are the name of the first column you want to index, the number of columns (`ncols` and amount you want to skip across columns, ` colskip` (so that `colskip = 2` would be every other column. You must also provide the starting row `j1`, the number of rows, ` nrows` and the row skip, `rowskip.` the The storage array is temporarily replaced by a new array that has `M=ncols` rows and `nrows+1` columns (since the data is transposed, the rows and columns are as well; confusing ain’t it). The form of the array is:
- 1  x(i1,j1) x(i1,j2) x(i1,j3) ...     2  x(i2,j1) x(i2,j2) x(i2,j3) ...     ...     M  x(iM,j1) x(iM,j2) x(iM,j3) ...
- where `i2=i1+colskip, i3=i1+2*colskip, ...` and `i1` is the index corresponding to the name of the first column you provide. Similarly, `j2=j1+rowskip, ...`. As a brief example, suppose that you solve a system of equations of the form: ``` math x_j' = f(x_{j-1},x_j,x_{j+1},I_j) ``` where $`j=1,\dots,20.`$ Click on transpose and choose `x1` as the first column, `colskip=1, ncols=20` and say `row1=350, nrows=1,rowskip=1` then a new array will be produced. The first column is the index from 1 to 20 and the second is `xj(350)` where 350 is the index and not the actual value of time. By plotting the second column versus the first you get a “spatial profile.”
- **(G)et par set**: This loads one of the parameter sets that you have defined in the ODE file.

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
- The `Style` button does nothing yet. The `Edit` button lets you change ranges and arrays to plot. The `Redraw` button is obvious.
- Since the animation option requires learning lots of new stuff, see [Creating Animations](10-animations.md) for a description of the animation language and what you can do with it.

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
