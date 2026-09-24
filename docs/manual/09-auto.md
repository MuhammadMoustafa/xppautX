# Auto interface

AUTO is a program that was written several years ago by Eusebius Doedel. It has the ability to track bifurcation curves for steady-state and periodic systems. The program is very powerful particularly for following periodic orbits. A full FORTRAN implementation of it is available along with documentation from Doedel. His Email is doedel@cs.concordia.edu.

The version supported in XPP is a subset of AUTO but allows you to do most of the things you would nornally want to for autonomous ODEs and with BVPS. In particular, you can track fixed points, find turning points and Hopf bifurcation points, compute two-parameter curves of turning points and Hopf points, compute branches of periodic solutions emanating from a Hopf point, track period-doubling bifurcations, torus bifurcations, and two-parameter curves of fixed period orbits. Points can be imported into XPP as well as complete orbits. The bifurcation diagrams are dynamically produced and you can move around them using the arrow keys. Curves can be saved and reloaded for later use. Diagrams can be saved and imported into the main XPP window.

Click on the `File Auto` menu item to bring up AUTO.

## The AUTO view

**In web2**, AUTO opens as its own view (`ui/AutoView.tsx`), anchored to
the right of the plot on screens from 48 rem wide and a full-screen sheet
below that; both stop above the status bar. It has:

- the **diagram**, drawn from the `diagram` data event: a curve per branch
  and stability run (stable solid, unstable dashed, periodic branches as
  their maximum and minimum), labelled points as crosses, and the segment
  from a Hopf point to the first point of the periodic branch it started
  (`from`, which XPP itself leaves blank); it zooms (wheel), pans
  (Shift+drag or the middle button) and undoes (`Ctrl+Z`) in the client,
  and a tooltip or the readout names the point under the mouse or cursor;
- a **stability circle** (`ui/AutoInfo.tsx`) below it, and a **status
  strip** (`ui/AutoStatus.tsx`) along the window's bottom edge, where the
  main window has its status bar, from the `autoinfo` event: branch, point,
  type, label, parameters, norm, the plotted variable, the period, and
  the eigenvalues/multipliers, in place of the small square and info
  windows of the X11 AUTO window;
- an **axis dialog** (`ui/AutoAxes.tsx`), opened by clicking an axis name
  next to the diagram, for Axes' choices (Hi, Norm, Hi-lo, Period, Two
  par, Frequency, Average and their ranges) below — usable during a run,
  applied when idle;
- once a run ends, the status strip says **why it stopped**, in words:
  "Stopped: parameter iapp reached Par Max (0.5)" (see "Why a branch
  stopped" below), and AUTO's Output gets the same as a line for every
  branch that ends ("Branch 1 stopped at point 49: ...");
- the diagram's **key** lists the label types it has, spelled out (EP End
  point, HB Hopf, ...), each with its meaning as a tooltip, and hovering a
  labelled point names its type; the point where the last run ended also
  says why;
- an **Output** panel with AUTO's printed table (`xpp_log_auto()`, always
  written in browser mode);
- Numerics as a dialog with Save/Load to a settings file, in place of the
  X11 Numerics window: every field has a plain name with AUTO's short name
  kept ("Max points (NMX)"), its help as a tooltip, and a message beside it
  when its value is not one AUTO takes (OK waits until every value is);
- **Grab** as a mode of the diagram: arrows, `[` `]`, Tab to the labelled
  points, Enter takes the point, Escape cancels, a click or tap takes the
  nearest point (see "Points and labels" below for what grabbing does);
- **Clear** hides the branches computed so far behind a key entry
  ("Earlier branches") instead of blanking the window until a redraw (AUTO
  has no reDraw button any more: the diagram is always current); and
  **Mark values** for user points (the `Usr Period` dialog below).

There is no Abort button in the view (A10 in docs/ui-v2.md): the status
bar's Stop stops a running continuation, as it stops any long job; the
view's own × or Close stops a run first, then closes.

Everything below this section (parameters, axes, numerics, points and
labels, the algorithm itself) is AUTO's own vocabulary and unchanged; only
where you click or type differs, as above.

## Preparation

Before you can use AUTO, you must prepare your system for it. You must start your bifurcation analysis from either a fixed point of your model, a periodic orbit, or a solution to a boundary value problem. AUTO seems to work best when you start from a steady state, but I have had success starting at periodic orbits. If you want to start at a steady state, find one and integrate so that the system is at rest. If you want to start at a periodic orbit, then find one and make sure that the total integration time is the “period” of your orbit. This is what the AUTO interface uses as an approximate starting period. There are several ways to do this; the best is to use the boundary value solver of XPP but just plain old integration often works fine. To solve a boundary value problem, it is necessary to find an initial set of parameters for which you can solve the problem within XPP. You should arrange the “length” of the interval to always be 1. That is you must scale the problem so that the domain interval of interest is $`[0,1].`$ You must then compute a solution using XPP before calling AUTO.

For discrete dynamical systems, I have added the capability of continuation of $`n-`$periodic orbits by having AUTO find fixed points of $`F^n(x).`$ To do this, just set the parameter **nOut** in the XPP numerics menu to the desired period. The example `del_log.ode` has set this up for a period 7 orbit.

For the example file `lecar.ode` the parameter of interest is `iapp` and this has been set at a negative value so that the system has a stable rest state. The variables have been initialized to their rest states as well. Once you have prepared the problem as such, you are ready to run.

Click on the “File” item and choose “Auto” to open the AUTO view (see
above).

## Choosing parameters

The first thing you should do is tell AUTO which parameters you might use in the bifurcation analysis. Up to 5 are allowed. Click on “Parameter” and a list of 5 parameters will appear. Type in the names of the parameters you want to use. For `lecar.ode` use ` iapp,phi,gk,vk,gna` The default is the first 5 or fewer parameters in your `ode` file. If you have fewer than 5 parameters, only the available ones will appear.

## Diagram axes

Next, you should tell AUTO the axes and the main bifurcation parameters. Click on “Axes” and 6 choices appear:

### (H)i

This plots the maximum of the chosen variable.

### (N)orm

This plots the $`L_2`$ norm of the solution.

### h(I)-lo

This plots both the max and min of the chosen variable (convenient for periodic orbits.)

### (P)eriod

Plot the period versus a parameter

### (T)wo par

Plot the second parameter versus the primary parameter for two-parameter continuations.

### (Z)oom

Use the mous to zoom in on a region.

### last (1) par

Use the plot parameters from the last 1-parameter plot.

### last (2) par

Use plot parameters from last 2-parameter plot.

### (F)requency

Plot Frequency vesus parameter.

### (A)verage

Plot the average of a variable versus the parameter.

After clicking, a new window pops up with the following items:

### Y-axis

This is the variable for the y-axis of the plot. For two-parameter and period plots, its contents is ignored.

### Main Parm

This is the principal bifurcation parameter. It must be one of those you specified in the parameter window. The default is the first parameter in the parameter list.

### 2nd Parm

This is the other parameter for two-parameter continuations.

### Xmin ... Ymax

The plotting dimensions of the diagram.

Once you press `OK` the axes will be redrawn and labeled. For the present model, set `Xmin=-.5, Ymin=-1.5, Xmax=.5, Ymax=1.0.`

## Numerical parameters

Next, set the NUMERICAL parameters. **In web2** the Numerics dialog names
each field in plain words with AUTO's short name after it, in three
groups; hovering a field shows its help. The page checks each value as you
type (a message beside the field, OK disabled until all are good) and the
core checks them again when they arrive:

| Field | What it does | Valid values |
|---|---|---|
| Mesh intervals (NTST) | mesh intervals of a periodic orbit or boundary value solution; raise it when a periodic branch looks wrong or does not converge (doubled when following a period doubling) | whole number, at least 1 |
| Max points (NMX) | the most points a branch may have; it ends (EP) at this many | whole number, at least 1 |
| Label every (NPR) | label and save the whole solution every NPR points, besides the special points | whole number, at least 1 |
| Collocation points (NCOL) | collocation points per mesh interval | whole number, 2 to 7 (4 is usual) |
| First step (DS) | the first step; its sign is the direction the main parameter goes | not 0, from DSMIN to DSMAX in size |
| Smallest step (DSMIN) | the smallest step: a point that does not converge is retried with half the step until it would go below this, then the branch ends (MX) | above 0, at most DSMAX |
| Largest step (DSMAX) | the largest step; too large jumps over folds and Hopf points | above 0 |
| Par Min (RL0), Par Max (RL1) | the main parameter's range: a branch that leaves it ends (EP) | Par Min below Par Max |
| Norm Min (A0), Norm Max (A1) | the norm's range: a branch whose norm leaves it ends (EP) | Norm Min below Norm Max |
| Parameter tolerance (EPSL), Solution tolerance (EPSU) | Newton's relative convergence tolerances for the parameters and the solution | above 0, often 1e-4 to 1e-7 |
| Special point tolerance (EPSS) | how closely special points are located; usually 100 to 1000 times EPSL and EPSU | above 0 |
| Adapt mesh every (IAD) | adapt a periodic orbit's mesh every IAD steps; 0 keeps it fixed | whole number, 0 or more (3 is usual) |
| Branch switches (MXBF) | steady states: at how many branch points AUTO follows the other branch by itself; negative: in one direction only; 0: none | whole number |
| Output detail (IID) | how much AUTO writes to its diagnostics (.d file): 0 almost nothing, 1 a little, 2 the usual, 3 also the Jacobian and residuals of the start, 4 and 5 very much (debugging) | 0 to 5 |
| Locate iterations (ITMX) | the most iterations locating a special point | whole number, at least 1 |
| Newton iterations (ITNW) | the most Newton iterations for a point, before the step is halved (IADS above 0) or the branch ends | whole number, at least 1 |
| Full Newton steps (NWTN) | after this many iterations the Jacobian is kept (chord method) | whole number, at least 1 |
| Adapt step every (IADS) | adapt the step every IADS steps; 0 keeps it at DS, and a point that does not converge then ends the branch (MX) | whole number, 0 or more (1 is usual) |
| Skip branch points (SuppBP) | 1: do not look for branch points (and for periodic orbits no Floquet multipliers, period doublings or tori); 0: look for them | 0 or 1 |

The settings file (Save/Load settings) keeps the core's form labels
(`Nmax`, `Ntst`, ...). The original window had the following items:

### Ntst

This is the number of mesh intervals for discretization of periodic orbits. If you are getting apparently bad results or not converging, it helps to increase this. For following period doubling bifurcations, it is automatically doubled so you should reset it later.

### Nmax

The maximum number of steps taken along any branch. If you max out, make this bigger.

### Npr

Give complete info every `Npr` steps.

### Ds

This is the initial step size for the bifurcation calculation. *The sign of `Ds` tells AUTO the direction to change the parameter.* Since stepsize is adaptive, `Ds` is just a “suggestion.”

### Dsmin

The minimum stepsize (positive).

### Dsmax

The maximum step size. If this is too big, AUTO will sometimes miss important points.

### Par Min

This is the left-hand limit of the diagram for the principle parameter. The calculation will stop if the parameter is less than this.

### Par Max

This is the right-hand limit of the diagram for the principle parameter. The calculation will stop if the parameter is greater than this.

### Norm Min

The lower bound for the $`L_2`$ norm of the solution. If it is less than this the calculation will stop.

### Norm Max

The upper bound for the $`L_2`$ norm of the solution. If it is greater than this the calculation will stop.

For the present model, you should set `Dsmax` to be 0.05, `Par Min` to -0.45 and `Par Max` to 0.45.

## User functions

Suppose you want to get plots at specific values of parameters or at fixed periods of a limit cycle. Then you can click on “User” which produces a menu 0-9 asking you how many points you want to keep. Click on 0 for none or some other number. A new window will appear with slots for 9 items. You can type in anything of the form:

        <parameter>=<value>

or

        T=<value>

AUTO will mark and save complete information for any point that satisfies either of these criteria. The second is used to indicate that you want to keep a point with a particular period, e.g., ` T=25` will save the any periodic orbit with period 25.

## Running

At this point, you are probably ready to run. But before doing a run, here is a hint. You can “save” the diagram at this point (see below under “File”). Although it is an “empty” diagram, all parameters axes, and numerics are saved. You can then reload them later on.

Click on “Run” to run the bifurcation. Depending on the situation, a number of menus can come up. For initial exploration, there are three choices, starting at a new steady state, periodic, or boundary value solution. If you are running the example, click on the steady-state option and a nice diagram will show up and a bunch of points will move around in the stability circle. These indicate stability: for fixed points, they represent exponentials of the eigenvalues; for periodics, the Floquet multipliers. Thus those in the circle are stable and those out of the circle are unstable. Bifurcations occur on the circle. The outer ones are “clipped” so that they will always lie in the square, thus you can keep count of them.

The diagram,itself, has two different lines and two different circles. Stable fixed points are thick lines, stable periodics are solid circles, unstable fixed points are thin lines, and unstable periodics are open circles. Additionally, there are crosses occasionally dispersed with numbers associated with them. These represent “special” points that AUTO wants to keep. There are several of them (web2 spells each out in the key, the readout and the info strip, with its meaning as the key's tooltip):

### EP End point

Where a branch starts or ends normally: a limit (Par Min/Max, Norm
Min/Max), Max points, Stop or a Mark value set to stop. The status strip
and Output say which (see "Why a branch stopped").

### MX No convergence

AUTO could not compute the next point, even at the smallest step, and
ended the branch there (failure to converge).

### LP Fold (limit point)

Limit point or turning point of a branch: two solutions meet and
disappear.

### HB Hopf

Hopf bifurcation: a pair of eigenvalues crosses the imaginary axis, and a
branch of periodic orbits starts here.

### BP Branch point

Bifurcation or branch point: another branch crosses this one.

### PD Period doubling

Period doubling bifurcation: a Floquet multiplier crosses -1.

### TR Torus

Torus bifurcation from a periodic orbit: a pair of Floquet multipliers
crosses the unit circle.

### UZ Marked value

A point where a parameter, or the period T, reaches one of the Mark values
(user functions).

- : Output every $`Npr^{th}`$ point.

## Why a branch stopped

AUTO labels the end of a branch EP or MX but does not say why. xppautX
records it where AUTO decides it (autlib1.c's stplae and stplbv, and the
"No convergence" notes of its fort.9) and says it in plain words: in the
AUTO view's status strip once the run ends ("Stopped: ..."), as a line in
Output for every branch that ends ("Branch 1 stopped at point 49: ..."),
next to the end point's label when you hover it, and as `autoinfo`'s
`stop` for a client (docs/protocol.md). The reasons:

| Stopped: | Why | What to change to go further |
|---|---|---|
| parameter *p* reached Par Min / Par Max (*value*) | the main parameter left the range in Numerics | Par Min / Par Max (RL0 / RL1) |
| the norm reached Norm Min / Norm Max (*value*) | the solution's norm left its range | Norm Min / Norm Max (A0 / A1) |
| the branch reached Max points (NMX *n*) | the branch has as many points as Max points allows | Max points (NMX), or a larger Largest step (DSMAX) |
| by the user (Stop) | Stop (or the view's Close) during the run | Run again from the end point |
| parameter *p* reached a Mark value set to stop | a user point marked as an end (AUTO's UZR endpoint) | the Mark values |
| no convergence even at the smallest step (Dsmin *value*) | the solver failed and halving the step reached Smallest step (DSMIN): a sharp turn, a jump, or a singular point (MX) | a smaller DSMIN or DSMAX, a larger NTST, looser tolerances |
| no convergence with a fixed step size (IADS 0) | the solver failed and Adapt step every (IADS) is 0, so the step is not reduced (MX) | IADS 1 |
| no convergence switching branches, ... | the same while starting a bifurcating branch at a branch point | as above |

Recording the reason changes nothing AUTO computes: the points, labels and
saved diagrams are the same as before.

## Grabbing

You can use these special points to continue calculations with AUTO. The “Grab” item lets you peruse the diagram at a leisurely pace and to grab special points or regular points for importing into XPP or continuing a bifurcation calculation. Click on “Grab” and stuff appears in the info window and a cross appears on the diagram. Use the left and right arrow keys to cruise through the diagram. The right key goes forward and the left backward. At the bottom, information about the branch, the point number, the type of point, the AUTO label, the parameters, and the period are given. The points marked by crosses have lables and types associated with them. The type is one of the above. The label corresponds to the number on the diagram. If point is positive, it is an unstable solution and if it is negative it is stable. As you traverse the diagram, stability is shown in the circle. In web2,
grabbing is a mode of the diagram itself (arrows, `[` `]`, Tab to the
labelled points, Enter takes the point, Escape cancels, a click or tap
takes the nearest point); there is no display-dependent drawing bug to
work around.

You can traverse the diagram very quickly by tapping the `Tab` key which takes you the special points only. Type `Esc` to exit with no action or type `Return` to grab the point. If it is a regular point (i.e., not special) then the parameters and the variables will be set to the values for that point within XPP. You can then integrate the equations or look at nullclines, etc. If you grab a special point, then you can use this as a restart point for more AUTO calculations, such as fixed period, two-parameter studies, and continuations. Then, you can run AUTO again. Bifurcation diagrams are cumulative unless you reset them in the “File” menu. That is, new stuff is continually appended to the old. The only limit is machine memory.

If you grab a special point and click on “Run” several possibilities arise depending on the point:

### Regular Point

Reset the diagram and begin anew. You will be asked first if you want to do this.

### Hopf Point

- **Periodic**: Compute the branch of periodics emanating from the Hopf point
- **Extend**: Continue the branch of steady states through this point.
- **New Point**: Restart whole calculation using this as a starting point
- **Two Param**: Compute a two parameter diagram of Hopf points.

### Period doubling

- **Doubling**: Compute the branch of period 2 solutions.
- **Two-param**: Compute two-parameter curve of period doubling points.

### Limit point

Compute two parameter family of limit points (fixed points or periodic.)

### Periodic point

The point is periodic so
- **Extend**: Extend the branch
- **Fixed Period**: Two parameter branch of fixed period points.

### Torus point

Compute two-parameter family of torus bifurcations or extend the branch or compute two-parameter fixed period.

*Before running, after a point is grabbed, be sure to set up the correct axes and ranges for the parameters.*

## Aborting

Any calculation can be gracefully stopped by clicking on the “Abort” key. This produces a new end point from which you can continue. Note that if there are many branches, you may have to press “Abort” several times.

`Clear` just erases the screen and `reDraw` redraws it.

## Saving diagrams

`File` allows you to do several things:

### Import orbit

If the grabbed point is a special one and is a periodic orbit, this loads the orbit into XPP for plotting. This is useful for unstable orbits that cant be computed by integrating initial data.

### Save diagram

Writes a file for the complete diagram which you can use later.

### Load Diagram

Loads a previously saved one.

### Postscript

This makes a hard copy of the bifurcation diagram

### Reset diagram

This clears the whole thing.

### Write pts

This writes a file specified by the user which has 5 columns and describes the currently visible bifurcation diagram. The first column has the coordinates of the x-axis, the second and third columns hold the contents of the y-axis, (e.g. max and min of the orbit). The fourth column is one of 1-4 meaning stable fixed point, unstable fixed point, stable periodic, unstable periodic, respectively. The fifth column is the branch number. The main window of XPP can import files in this format and plot them

## Homoclinics and heteroclinics

A recent version of AUTO includes a library of routines called HOMCONT which allow the user to track homoclinic and heteroclinic orbits. XPP incorporates some aspects of this package. The hardest part of computing a branch of homoclinics is finding a starting point. Consider a differential equation:
``` math
x'=f(x,\alpha)
```
where $`\alpha`$ is a free parameter. Homoclinics are codimension one trajectories; that is, they are expected to occur only at a particular value of a parameter, say, $`\alpha=0.`$ We suppose that we have computed an approximate homoclinic to the fixed point $`\bar{x}`$ which has an $`n_s-`$dimensional stable manifold and an $`n_u-`$dimensional unstable manifold. We assume $`n_s+n_u=n`$ where $`n`$ is the dimension of the system. The remaining discussion is based on Sandstede et al. The way that a homoclinic is computed is to approximate it on a finite interval; say $`[0,P].`$ We rescale time by $`t=Ps.`$ We double the dimension of the system so that we can simultaneously solve for the equilibrium point as the parameters vary. We want to start along the unstable manifold and end on the stable manifold. Let $`L_u`$ be the projection onto the unstable subspace of the linearization of $`f`$ about the fixed point and let $`L_s`$ be the projection onto the stable space. Then we want to solve the following system:
``` math
\begin{eqnarray*}
\frac{dx}{ds} &=& P f(x,\alpha) \\
\frac{dx_e}{ds}&=& 0 \\
f(x_e(0)) &=& 0 \\
L_s (x(0)-x_e(0)) &=& 0 \\
L_u (x(1)-x_e(1)) &=& 0
\end{eqnarray*}
```
Note that there are $`2n`$ differential equations and $`2n`$ boundary conditions; $`n`$ for the equilibrium, $`n_s`$ at $`s=0`$ and $`n_u`$ at $`s=1.`$ There is one more condition required. Clearly one solution to this boundary value problem is $`x(s)\equiv x_e(s)\equiv \bar{x}`$ which is pretty useless. However, any translation in time of the homoclinic is also a homoclinic so we have to somehow define a phase of the homoclinic. Suppose that we have computed a homoclinic, $`\hat{x}(s).`$ Then we want to minimize the least-squares difference between the new solution and the old solution to set the phase. This leads to the following integral condition:
``` math
\int_0^1 \hat{x}'(s)(\hat{x}(s)-x(s))\ ds = 0.
```
This is *one* more condition which accounts for the need for an additional free parameter.

XPP allows you to specify the projection boundary conditions and by setting a particular flag on in AUTO, you can implement the integral condition. Since the XPP version of AUTO does not allow you to have more conditions than there are differential equations, you should pick one parameter which will be slaved to all the other ones you vary and let this satisfy a trivial differential equation,
``` math
\alpha'=0.
```

Here is the first example of continuing a homoclinic in two-dimensions.
``` math
x'=y \quad y'=x(1-x)-ax+\sigma xy
```
When $`(a,\sigma)=(0,0)`$ there is a homoclinic orbit (Prove this by integrating the equations; this is a conservative dynamical system.) For small $`a`$ it is possible to prove that there is a homoclinic orbit for a particular choice of $`\sigma(a)`$ using Melnikov methods (see Holmes and Guckenheimer). We now write the equations as a 5-dimensional system using $`\sigma`$ as the slaved parameter and introducing a parameter, $`P`$ for the period:
``` math
\begin{eqnarray*}
x' &=& P f(x,y) \\
y' &=& P g(x,y)  \\
x_e' &=& 0 \\
y_e' &=& 0   \\
\sigma' &=& 0
\end{eqnarray*}
```
where $`f(x,y)=y`$, $`g(x,y)=x(1-x)-ax+\sigma xy`$ and the following boundary conditions
``` math
\begin{eqnarray*}
0 &=& f(x_e,y_e) \\
0 &=& g(x_e,y_e) \\
0 &=& L_s (x(0)-x_e,y(0)-y_e)      \\
0 &=& L_u (x(1)-x_e,y(1)-y_e)
\end{eqnarray*}
```
and the integral condition. XPP has a defined function for the projection boundary conditions called `hom_bcs(k)` where ` k=0,1,...,n-1` corresponding to the total number required. You do not need to be concerned with ordering etc as long as you get them all and you give XPP the required information. Here is the ODE file:

    # tsthomi.ode
    f(x,y)=y
    g(x,y)=x*(1-x)-a*y+sig*x*y
    x'=f(x,y)*per
    y'=g(x,y)*per
    # auxiliary ODE for fixed point
    xe'=0
    ye'=0
    # free parameter
    sig'=0
    # boundary conditions
    b f(xe,ye)
    b g(xe,ye)
    # project off the fixed point from unstable manifold
    b hom_bcs(0)
    # project onto the stable manifold
    b hom_bcs(1)
    par per=8.1,a=0
    init x=.1,y=.1
    @ total=1.01,meth=8,dt=.001
    @ xlo=-.2,xhi=1.6,ylo=-1,yhi=1,xp=x,yp=y
    done

The only new feature is the projection conditions. *XPP’s boundary value solver will not work here since there are more equations than conditions and it doesn’t know about the integral condition.* I have set the total integration time to 1 and have added the additional parameter `per` corresponding to the parameter $`P`$ in the differential equation. I use the Dormand-Prince order 8 integrator as it is pretty accurate. I have also set the view to be the $`(x,y)-`$plane. Note that this is a pretty rough approximation of the true homoclinic. We will use AUTO to improve this before continuing in the parameter $`a`$. Run XPP with this ODE file and integrate the equations. You will get a rough homoclinic pretty far from the fixed point. Click on File Auto to the the AUTO window. Now click on Axes Hi. Choose `xmin=0,xmax=50,ymin=-6,ymax=6` and also select `sig` as the variable in the y-axis. Click on OK and bring up the Auto Numerics dialog. Change `Ntst=35, Dsmin=1e-4,Dsmax=5`, `Par Max=50,EPSL=EPSU=EPSS=1e-7` and click OK. Now, before you run the program, click on Usr Period and choose 3 for the number. We want AUTO to output at particular values of the parameter `per` corresponding to $`P`$. When the dialog comes up, fill the first three entries in as `per=20,per=35,per=50` respectively and click OK. This forces AUTO to output when $`P`$ reaches these three values. Now, click on Run and choose Homoclinic. A little dialog box appears. Fill it in as follows: `Left Eq: Xe` `Right Eq: Xe` `NUnstable: 1` `NStable: 1`. You must tell AUTO the dimension of the stable and unstable manifolds as well as the fixed point to which the orbit is homoclinic. (Note that if you ever fill this in wrong or need to change it, you can access it from the main XPP menu under Bndry Value Homoclinic.) Once you click on OK, you should see a straight line across the screen as the homoclinic approximation gets better. Click on Grab and grab the second point corresponding to the point `Per=35`. For fun, in the XPP window, click on Initial Conds Go and you will see a much better homoclinic orbit.

Now that we have a much improved homoclinic orbit, we will continue in the parameter $`a`$ as desired. First, lets make sure we get the orbits when $`a=-6,-4,-2,2,4,6`$ so we will click on Usr Period and choose 6. Type in `a=-6,a=-4, etc` for the first 6 entries and then click OK. Click on Axes Hi to change the axes and the continuation parameter. Change the `Main Parm` to `a`, `Xmin=-7,Xmax=7` and click OK. Click on Numerics and change `Par Min=-6, Par Max=6` and then click OK. Now click on Run and you will see a line that is almost diagonal. When done, click on Grab again, and watch the bottom of the AUTO window until you see Per=35 and click Enter. In the Numerics menu, change `Ds=-.02` to change directions, and click Ok. Now click Run and there will be another diagonal line that is in the opposite direction. Click on Grab and grab point number 7 corresponding to `a=6`. In the XPP window, click on Init Conds go and you will see a distorted homoclinic. It is not that great and could be improved probably by continuing with `Per` some more. Grab the point labeled 11 (`a=-6`) and in XPP try to integrate it. It doesn’t look even close. This is because the homoclinic orbit is unstable and shooting (which is what we are doing when we integrate the equation) is extremely sensitive to the stability of the orbits. In the AUTO window, click on File Import Orbit to get the orbit that AUTO computed using collocation. In the XPP main window, click on Restore and you will see a much better version of the homoclinic orbit. This is because collocation methods are not sensitive to the stability of orbits! In fact, you can verify that the fixed point (0,0) is a saddle-point with a positive eigenvalue, $`\lambda_u`$ and a negative one of $`\lambda_s`$ whose sum is the trace of the linearized matrix, $`-a`$. The sum of the eigenvalues is called the saddle-quantity and if it is positive (for us, $`a<0`$), then the homoclinic is unstable.

We now describe how to find heteroclinic orbits. The methods are the same except that we must track two *different* fixed points. Thus, we need an additional $`n`$ equations for the other fixed point. As with homoclinic orbits, we go from the unstable manifold to the stable manifold. In this case, the “left” fixed point is the one emerging from the unstable manifold and the “right” fixed point is the one going into the stable manifold. Thus, the dynamical system is :
``` math
\begin{eqnarray*}
\frac{dx}{ds} &=& P f(x,\alpha) \\
\frac{dx_{left}}{ds}&=& 0 \\
\frac{dx_{right}}{ds}&=& 0 \\
f(x_{left}(0)) &=& 0 \\
f(x_{right}(1)) &=& 0 \\
L_s (x(0)-x_{left}(0)) &=& 0 \\
L_u (x(1)-x_{right}(1)) &=& 0.
\end{eqnarray*}
```
The only difference is that we have the additional $`n`$ equations for the right fixed point and the $`n`$ additional boundaty conditions. It is important that you give good values for the initial conditions for the two fixed points since they are different and you need to converge to them. The classic bistable reactrion-diffusion equation provides a nice example of a heteroclinic. The equations are:
``` math
-cu'=u''+u(1-u)(u-a)
```
which we rewrite as a system:
``` math
\begin{eqnarray*}
u' &=& u_p \equiv f(u,u_p) \\
u_p' &=& -cu_p - u(1-u)(u-a) \equiv g(u,u_p)
\end{eqnarray*}
```
The fixed point $`(1,0)`$ has a one-dimensional unstable manifold and $`(0,0)`$ as a one-dimensional stable manifold. We seek a solution from $`(1,0)`$ to $`(0,0).`$ For $`a=0.5`$ and $`c=0`$, there is an exact solution joining the two saddle points. (Prove this by showing that
``` math
u_p^2 +u^4/2-2u^3/3+u^4/2
```
is constant along solutions when $`a=0.5,c=0.`$) We will use this as a starting point in our calculation. Here is the ODE file:

    # tstheti.ode
    # a heteroclinic orbit
    # unstable at u=1, stable at u=0
    f(u,up)=up
    g(u,up)=-c*up-u*(1-u)*(u-a)
    # the dynamics
    u'=up*per
    up'=(-c*up-u*(1-u)*(u-a))*per
    # dummy equations for the fixed points
    uleft'=0
    upleft'=0
    uright'=0
    upright'=0
    # the velocity parameter
    c'=0
    # fixed points
    b f(uleft,upleft)
    b g(uleft,upleft)
    b f(uright,upright)
    b g(uright,upright)
    # projection conditions 
    b hom_bcs(0)
    b hom_bcs(1)
    # parameters
    par per=6.67,a=.5
    # initial data
    init u=.918,up=-.0577,c=0
    # initial fixed points
    init uleft=1,upleft=0,uright=0,upright=0
    @ total=1.01,dt=.01
    @ xp=u,yp=up,xlo=-.25,xhi=1.25,ylo=-.75,yhi=.25
    # some AUTO parameters
    @ epss=1e-7,epsu=1e-7,epsl=1e-7,parmax=60,dsmax=5,dsmin=1e-4,ntst=35
    done

I have added a few AUTO numerical settings so that I don’t have to set them later. Run XPP with this ODE file and integrate the equations. We will now continue this approximate heteroclinic in the parameter `per`. Fire up AUTO (File Auto) and click on Axes Hi. Put `c` on the y-axis and make ` Xmin=0,Xmax=60,Ymin=-2,Ymax=2`. Then click OK. As above, we will also keep solutions at particular values of `per` by clicking on ` Usr period` `3`, choosing `per=20,per=40,per=60`, and then OK. Now we are ready to run. Click on Run Homoclinic. When the dialog box comes up set the following: `Left Eq: ULEFT, Right Eq: URIGHT, NUnstable:1, NStable:1` and then click OK. You should see a nice straight line go across the screen. Grab the point labeled ` per=40` and then click on File Import Orbit. Look at it in the XPP main window and freeze it. Now in the Auto window, click on Axes Hi and change the Main Parm to `a` and `Xmax=1`. Click OK and then click on Numerics to get the Auto Numerics dialog. Change ` Dsmax=0.1, Par Max=1` and click OK. Now click on Usr Period 4 and make the four user functions `a=.75,a=.9,a=.25,a=.1` and click OK. Now click on Run and watch a line drawn across the screen. This is the velocity, $`c`$ as a function of the threshold, $`a`$. Click on Grab and looking at the bottom of the screen, wait until you see `per=40` and then click on Enter. Now, open the Numerics dialog box and change `Ds=-.02` to go the other direction. Click on Run and you should see the rest of the line drawn across the screen. Click on Grab and move to the point labeled `a=0.25` Click on File Import Orbit and plot this in the main XPP window.
