> XPP Commands\
> Bard Ermentrout — Dec 2012

## ODE File Format

    # comment line - name of file, etc
    #include filename   
    d<name>/dt=<formula>
    <name>'=<formula>
    <name>(t)=<formula>
    volt <name>=<formula>
    <name>(t+1)=<formula>
    x[n1..n2]' = ...[j] [j-1] ... <--  Arrays
    %[i1..i2]
    u[j]'=...
    v[j]'=...
    % 
    markov <name> <nstates> <init>
      {t01} {t02} ... {t0k-1}
      {t10} ...
      ...
      {tk-1,0} ... {tk-1 k-1}

    aux <name>=<formula>
    !<name>=<formula> <--  parameters defined as formulae
    <name>=<formula>
    parameter <name1>=<value1>,<name2>=<value2>, ...
    wiener <name1>, <name2>, ...
    number <name1>=<value1>,<name2>=<value2>, ...
    <name>(<x1>,<x2>,...,<xn>)=<formula>
    table <name> <filename>
    table <name> % <npts> <xlo> <xhi> <function(t)>
    global sign {condition} {name1=form1;...}
    init <name>=<value>,...
    <name>(0)=<value> or <expr> <--  delay initial conditions
    bdry <expression>
    0= <expression>    <---  For DAEs
    solv <name>=<expression> <------ For DAEs
    special <name>=conv(type,npts,ncon,wgt,rootname)
               fconv(type,npts,ncon,wgt,rootname,root2,function)
               sparse(npts,ncon,wgt,index,rootname)
               fsparse(npts,ncon,wgt,index,rootname,root2,function)
                   fftcon(type,npts,wgt,rootname)
                   mmult(ncol,nrow,matrix,rootname)
                   fmmult(ncol,nrow,matrix,root1,root2,f)
                   findext(type,n,skip,root)
                   gill(meth,rxn_list)
                   delmmult(n,m,w,tau,root)
                   delsparse(m,nc,w,index,tau,root)
              
    export {x1,x2,....} {x1p,x2p,..}     
    # comments
    @ <name>=<value>, ...
    set <name> {x1=z1,x2=z2,...}
    only <name1>,<name2>,...
    options <filename>
    " {z=3,b=3,...} Some nice text   <---  Active comments 
    done

**Remarks**

- ODEs are put in as `name'=expression` or `dname/dt=expression`. Note you can use this notation for maps as well or use `name(t+1)=expression`.
- `name=expression` allows you to use `name` in many locations and to build up more complex expressions. The order in which they are written is the order in which they are evaluated, so don't use an expression using a name that hasn't yet been defined.
- Array expressions such as `x[1..10]'=...[j]...` are expanded as `x1'=...1..` etc. The `[j]` is expanded into a number. You can use some minimal arithmetic as well such as `[j+1],[j-1],[j*2]` etc.
- `x[0..99](0)=sin(2*pi*[j]/100)` is a way to initialize an array ODE.
- `markov <name> <n> <init>` sets up a continuous Markov process with `n` states and whose $`n\times n`$ transition matrix follows with entries delimited with the braces `{, }`. The diagonal entries should just be 0 as they are ignored. The starting state is given by `<init>`.
- `aux` quantities are extra stuff you might want to plot.
- `parameters` are named quantities that you can change within the program.
- `numbers` are named quantities that are invisible to the user and cannot be changed.
- `!name=...` defines a named quantity whose value depends on other numbers and parameters, but not variables, as these named quantities are only computed when you change parameters.
- `wiener` defines a set of Wiener processes.
- `table` reads in a function in the form of a table or you can define it within the ODE file; the file version of a table starts with three numbers, the number of values, the low, and the high, followed by the y-values. The x values are equally spaced from low to high. Tables are treated as functions of one variable with a linear interpolation as the default. If you want a cubic spline then the initial number in the file should have an `s` in front of it and if you want piecewise constant then put an `i` in front of the number. The function form of the table uses `%` followed by three numbers as with the file format and then a formula using the independent variable `t`. So `table s % 51 -25 25 exp(-abs(t))/2` will produce a tabular function such that `s(5)` would return $`\exp(-5)/2.`$ Other values are interpolated. Tables are very useful for networks.
- While rather clumsy in notation, you can initialize a delay equation via `x(0)=f(t)` to set the values of $`x`$ for $`-\tau\le t<0.`$ Delay initial data is zero by default.
- The `global` declaration includes two special quantities. Inside the braces, if you type `arret=value`, the integration will stop if the `value` is not zero. This way you can stop an integration if a particular event happens. The other declaration is `out_put=value` which will override the `transient` (see the numerics menu) and allow you to plot (when `value` is nonzero), but only when the events occur.
- In the `global` declaration the `sign` is `{0,1,-1}`. 0 means the condition must be met exactly at the point it was checked. It is a good way to set initial conditions as expressions, e.g. `global 0 t {x=sin(1)}`.
- `bdry <expression>` is a way to set boundary conditions for the boundary value solver. The BVP solver tries to zero the expression. Use the names of your variables for the left end conditions and primed versions for the right ends (see `gberg.ode`).
- The pair `0= <expression>` and `solve <name>=<expression>` set up differential algebraic equations. The lines starting with `0=` will solve for the variables in the `solv` lines to make the expressions zero. See `huygens.ode` for an example where three accelerations are solved for to get the dynamics of two pendulums on a cart.
- `export` is used to communicate with a dynamically loaded library compiled from some C code. See `tstdll.ode`.
- `only` is useful for silent mode (no GUI) as this puts out a bunch of data in a file. `only` restricts the output to a specified set of variables.
- `set` creates a bunch of settings for numerics, variables, parameters that you can call by name within XPP.
- Active comments allow you to create a little tutorial where you take care of changing parameters, numerics, etc. You invoke this in XPP with the File Printsrc command. See `lecar.ode` for an example.

**Order of evaluation.**

1. Fixed variables
2. `specials`
3. DAEs
4. External C code
5. RHSs

The general integral equation
``` math
u(t)=f(t)+\int_0^t K(t,s,u(s))ds
```
becomes

    u = f(t) + int{K(t,t',u)}

The convolution equation:
``` math
v(t) = \exp(-t) + \int_0^t e^{-(t-s)^2}v(s) ds
```
would be written as:

    v(t) = exp(-t) + int{exp(-t^2)#v}

If one wants to solve, say,
``` math
u(t) = exp(-t) + \int^t_0 (t-t')^{-mu} K(t,t',u(t'))dt'
```
the form is:

    u(t)= exp(-t) + int[mu]{K(t,t',u}

and for convolutions, use the form:

    u(t)= exp(-t) + int[mu]{w(t)#u}

**NETWORKS**

    special zip=conv(type,npts,ncon,wgt,root)

where `root` is the name of a variable and `wgt` is a table, produces an array `zip` with `npts`:
``` math
\hbox{zip}[i] =\sum_{j=-\hbox{ncon}}^{\hbox{ncon}}\hbox{wgt}[j+ncon]
\hbox{root}[i+j]
```

    special bob=fftconv(type,npts,wgt,root)

is similar to the `conv` operation, but uses the FFT to do it. The `type` should be `odd` or `periodic`. The size of the `wgt` table should be either `npts` or `2 npts`. The `sparse` network has the syntax:

    special zip=sparse(npts,ncon,wgt,index,root)

where `wgt` and `index` are tables with at least `npts * ncon` entries. The array `index` returns the indices of the offsets to with which to connect and the array `wgt` is the coupling strength. The return is

    zip[i] = sum(j=0;j<ncon) w[i*ncon+j]*root[k]
    k = index[i*ncon+j] 

The other two types of networks allow more complicated interactions:

    special zip=fconv(type,npts,ncon,wgt,root1,root2,f)

evaluates as

    zip[i]=sum(j=-ncon;j=ncon) wgt[ncon+j]*f(root1[i+j],root2[i])

and

    special zip=fsparse(npts,ncon,wgt,index,root1,root2,f)

evaluates as

    zip[i]=sum(j=0;j<ncon) wgt[ncon*i+j]*f(root1[k],root2[i])
    k = index[i*ncon+j] 

Matrix multiplication is also possible:

    special k=mmult(n,m,w,u)

returns a vector `k` of length `m ` defined as

    k(j)=sum(i=0;i<n)w(i+nj)u(i)

while

    special k=fmmult(n,m,w,u,v,f)

returns

    k(j)=sum(i=0;i<n)w(i+nj)f(u(i),v(j))

    special z=findext(type,n,skip,root)

finds the extreme values of a list of `n` variables starting at `root` and skipping every `skip` one. `type=1,-1,0` according as to whether you want the max, min, or both. In any case, `z(0), z(2)` are the max and min and `z(1),z(3)` are the index of the max and min. See `kohonen.ode` for an example of this.

    special k=delmmult(n,m,w,tau,u0)

returns the $`m`$ values
``` math
k(i)=\sum_{j=0}^{m-1} \mbox{w}[i m + j] u[j](t-\mbox{tau}[i m + j])
```

    special k=delsparse(m,n,w,l,tau,root)

returns the $`m`$ values
``` math
k(i)=\sum_{j=0}^{n-1} \mbox{w}[i n  + j] u[l(i n +j)](t-\mbox{tau}[i m + j])
```

    special ydot=import(soname,sofun,nret,root,w1,w2,...wm)

This is a conveneient way to load a large number of right-hand sides that have been coded in C. See the examples in the cuda folder.

## The options list

The format for changing the options, either as `@` lines in an ODE file
or in an [options file](14-options-file.md), is:

    @ name1=value1, name2=value2, ...

where `name` is one of the following and `value` is either an integer, floating point, or string. (All names can be upper or lower case).

- QUIET=`0,1` dont print out stuff

- LOGFILE=`filename` store prontouts in logfile

- MAXSTOR=`integer` sets the total number of time steps that will be kept in memory. The default is 5000. If you want to perform very long integrations change this to some large number.

- FORECOLOR=`rrggbb` sets the hexidecimal frame color, and text menu color.

- BACKCOLOR=`rrggbb` sets the background color on the menus, sliders, dialogs, and buttons

- MWCOLOR=`rrggbb` sets the background color for the main frame in the popups.

- DWCOLOR=`rrggbb` sets the color of the drawing window and the browser numerical displays

- BACKIMAGE=`file.xbm` sets the back image. The format is the not generally readily found X11-bitmap format. On the mac, you can use some unix tools like gimp, xv, or ImageMagick.

- SMALLFONT=`fontname` where `fontname` is some font available to your X-server. This sets the “small” font which is used in the Data Browser and in some other windows.

- BIGFONT=`fontname` sets the font for all the menus and popups.

- SMC={0,...,10} sets the stable manifold color. These colors correspond to the colors available when plotting in XPP and are roughly, `“black”,red,redorange,orange,yelloworange,yello,yellowgreen,green,bluegreen,blue,purple` Here “black” means the FORECOLOR.

- UMC={0,...,10} sets the unstable manifold color

- XNC={0,...,10} sets the X-nullcline color

- YNC={0,...,10} sets the Y-nullcline color

- SEC,UEC,SPC,UPC=color set the colors for the screen on AUTO for stable eq, unstable eq, stable per, unstable per.

- OUTPUT=filename sets the filename to which you want to write for “silent” integration. The default is “output.dat”.

- GRADS={0,1} turn off or on gradients in the XPP buttons \</LI\>

- HEIGHT=pixels,WIDTH=pixels, sets the height and width of the main window intially

- RUNNOW=1 run the simulation as soon as the windows are up (Don’t wait for Init conds Go, e.g.

- BUT=name:kbs defines button on the top of the main window. You can define up to 20 such buttons. They will appear across the top when you press them, they will execute an XPP keyboard shortcut. For example, **BUT=Fit:wf** will create a button labeled **Fit** and when you press it, it will be as if you had clicked **Window/zoom Fit**. Most of the menu items are available.

The remaining options can be set from within the program. They are

- LT=`int` sets the linetype. It should be less than 2 and greater than -6.

- SEED=`int` sets the random number generator seed.

- XP=name sets the name of the variable to plot on the x-axis. The default is `T`, the time-variable.

- YP=name sets the name of the variable on the y-axis.

- ZP=name sets the name of the variable on the z-axis (if the plot is 3D.)

- COLORMAP=`0,1,2,3,4,5` sets the colormap

- NPLOT=`int` tells XPP how many plots will be in the opening screen.

- XP2=name,YP2=name,ZP2=name tells XPP the variables on the axes of the second curve; XP8 etc are for the 8th plot. Up to 8 total plots can be specified on opening. They will be given different colors.

- MULTIWIN=`0,1`, puts multiple windows up and one each of the NPLOT curves in them.

- SIMPLOT=`0,1` turns on the simultaneous plot flag for multiple windows

- XHI2=value,YHI2=value,XLO2=value,YLO2=values dimensions the extra windows up to 8.

- AXES=`{2,3}` determine whether a 2D or 3D plot will be displayed.

- COLORIZE=1,COLORVIA=`name`, COLORLO=`value`, COLORHI=`value` all set the colorization of trajectories and in the `Numerics colorcode` command. If `name` is `speed`, then colorcoding will be via velocity.

- TOTAL=value sets the total amount of time to integrate the equations (default is 20).

- DT=value sets the time step for the integrator (default is 0.05).

- NJMP=`integer` tells XPP how frequently to output the solution to the ODE. The default is 1, which means at each integration step.

- T0=value sets the starting time (default is 0).

- TRANS=value tells XPP to integrate until `T=TRANS` and then start plotting solutions (default is 0.)

- NMESH=`integer` sets the mesh size for computing nullclines (default is 40).

- DFGRID=`integer` sets the grid size for direction fields, flows etc (default is 10);

- METH=`{ discrete,euler,modeuler,rungekutta,adams,gear,volterra, backeul, qualrk,stiff,cvode,5dp,83dp,2rb,ymp}` sets the integration method (default is Runge-Kutta.)

- DTMIN=value sets the minimum allowable timestep for the Gear integrator.

- DTMAX=value sets the maximum allowable timestep for the Gear integrator

- VMAXPTS=value sets the number of points maintained in for the Volterra integral solver. The default is 4000.

- { JAC_EPS=value, NEWT_TOL=value, NEWT_ITER=value} set parameters for the root finders.

- ATOLER=value sets the absolute tolerance for CVODE.

- TOLER=value sets the error tolerance for the Gear, adaptive RK, and stiff integrators. It is the relative tolerance for CVODE.

- BOUND=value sets the maximum bound any plotted variable can reach in magnitude. If any plottable quantity exceeds this, the integrator will halt with a warning. The program will not stop however (default is 100.)

- DELAY=value sets the maximum delay allowed in the integration (default is 0.)

- BANDUP=`int`, BANDLO=`int` bandwidths for the Jacobian computation.

- PHI=value,THETA=value set the angles for the three-dimensional plots.

- XLO=value,YLO=value,XHI=value,YHI=value set the limits for two-dimensional plots (defaults are 0,-2,20,2 respectively.) Note that for three-dimensional plots, the plot is scaled to a cube with vertices that are $`\pm1`$ and this cube is rotated and projected onto the plane so setting these to $`\pm2`$ works well for 3D plots.

- XMAX=value, XMIN=value, YMAX=value, YMIN=value, ZMAX=value, ZMIN=value set the scaling for three-d plots.

- POIMAP=`{ section,maxmin, period} ` sets up a Poincare map for either sections of a variable, the extrema, or period.

- POIVAR=name sets the variable name whose section you are interested in finding.

- POIPLN=value is the value of the section; it is a floating point.

- POISGN=`{ 1, -1, 0 }` determines the direction of the section.

- POISTOP=1 means to stop the integration when the section is reached.

- RANGE=1 means that you want to run a range integration (in batch mode).

- RANGEOVER=name, RANGESTEP=number, RANGELOW=number, RANGEHIGH=number, RANGERESET=`Yes,No`, RANGEOLDIC=`Yes,No` all correspond to the entries in the range integration option.

- TOR_PER=value, defined the period for a toroidal phasespace and tellx XPP that there will be some variables on the circle.

- FOLD=name, tells XPP that the variable \<name\> is to be considered modulo the period. You can repeat this for many variables.

- PS_Font=`fontname`,PS_LW=`linewidth`,PS_FSIZE=`fontsize`,PS_COLOR=`0,1` sets up postscript options.

- S1=`name`, SLO1=`number`,SHI1=`number` sets the variables, parameters associated with a slider and their low and high values. Use S2,S3 for the other sliders (This is different for the iPad/iPhone.)

- STOCH=`1,2` set the stochastic flag. Set up a range and use this to compute the mean etc of an ensemble of trajectories for batch mode integration. 1 returns the mean and 2 the variance.

- POSTPROCESS=`1,2,3,4,5,6` is for batch integration and allows you to process your data as follows: 1-histogram, 2-Fourier,3-Power,4-Power spectral density, 5-cross spectrum, 6-coherence.

- HISTHI=`number`,HISTLO=`number`,HISTBINS=`number`, HISTCOL=`variable name`, sets up the relevant quantitues for a batch histogram; used in conjunction with POSTPROCESS.

- SPECCOL=`name`, SPECCOL2=`name`, SPECWIDTH=`number`, SPECWIN=`0,1,2,3,4` (corresponding to square,parabolic,hamming,bartlett, or hanning windows), sets up relevant spectral stuff for use in conjunction with postprocessing.

- AUTO-stuff. The following AUTO-specific variables can also be set: `NTST, NMAX, NPR, DSMIN, DSMAX, DS, PARMIN, PARMAX, NORMMIN, NORMMAX, AUTOXMIN, AUTOXMAX, AUTOYMIN, AUTOYMAX, AUTOVAR`. The last is the variable to plot on the y-axis. The x-axis variable is always the first parameter in the ODE file unless you change it within AUTO.

- DLL_LIB=`file` Dynamically linked library

- DLL_FUN=`name` Dynamically linked function.

- DFDRAW =`1, 2, or 3` will force the drawing of direction fields on batch plots; 1 is unscaled, 2 is scaled, and 3 is colorized.

- NCDRAW=`1` forces the drawing of nullclines in batch plots

0-Foreground color; 1-Red; 2-Red Orange; 3-Orange; 4-Yellow Orange; 5-Yellow; 6-Yellow Green; 7-Green; 8-Blue Green; 9-Blue; 10-Purple.

You should be aware of the following keywords that should not be used in your ODE files for anything other than their meaning here.

    sin cos tan atan atan2 sinh cosh tanh
    exp delay ln log log10 t pi if then else
    asin acos heav sign mod flr ran abs del\_shft 
    max min normal besselj bessely besseli erf erfc poisson
    lgamma arg1 ... arg9  @ $ + - / * ^ ** shift
    | > < == >= <= != not \# int sum of i'

These are mainly self-explanatory. The nonobvious ones are:

- **`heav(arg1)`**: the step function, zero if `arg1<0` and 1 otherwise.

- **`sign(arg)`**: which is the sign of the argument (zero has sign 0)

- **`ran(arg)`**: produces a uniformly distributed random number between 0 and `arg.`

- **`besselj, bessely, besseli `**: take two arguments, $`n,x`$ and return $`J_n(x)`$ $`Y_n(x),I_n(x)`$ the Bessel functions.

- **`erf(x), erfc(x)`**: are the error function and the complementary function.

- **`lgamma(x)`**: is the log of the gamma function.

- **`normal(arg1,arg2)`**: produces a normally distributed random number with mean `arg1` and variance `arg2`.

- **`poisson(arg)`**: produces the number of events from a Poisson process with parameter `arg`. It is a random number.

- **`max(arg1,arg2)`**: produces the maximum of the two arguments and `min` is the minimum of them.

- **`if(<exp1>)then(<exp2>)else(<exp3>)`**: evaluates ` ` If it is nonzero it evaluates to otherwise it is . E.g. `if(x>1)then(ln(x))else(x-1)` will lead to `ln(2)` if `x=2` and `-1 if x=0.`

- **`delay(<var>,<exp>)`**: returns variable `<var>` delayed by the result of evaluating `<exp>`. In order to use the delay you must inform the program of the maximal possible delay so it can allocate storage.

- **`del_shft(<var>,<shft>,<delay>).`**: This operator combines the `delay` and the `shift` operators and returns the value of the variable `<var>` shifted by `<shft>` at the delayed time given by `<delay>`. (See `sine-circle.ode` for an example.)

- **`mod(arg1,arg2)`**: is `arg1` modulo `arg2`

- **`flr(arg)`**: is the integer part of`<arg>` returning the largest integer less than `<arg>`.

- **`t `**: is the current time in the integration of the differential equation.

- **`pi`**: is $`\pi.`$

- **`arg1, ..., arg9`**: are the formal arguments for functions. You never will actually see them, but they are used internally in the parser.

- **`int, #`**: concern Volterra equations.

- **`shift(<var>,<exp>)`**: This operator evaluates the expression `<exp>` converts it to an integer and then uses this to indirectly address a variable whose address is that of `<var>` plus the integer value of the expression. This is a way to imitate arrays in XPP. For example if you defined the sequence of 5 variables, ` u0,u1,u2,u3,u4` one right after another, then `shift(u0,2)` would return the value of `u2.`

- **`set(<var>,int,value)`**: is a weird little function that will assign the variable `<var> ` shifted by `int ` the value `value`. It would be used for example to reset some random variable in a `global` statement. Since *all* XPP functions return values, this needs an assignment and will retun `value`. For example:

      global 1 t-t0 {u=set(x0,50*ran(1),3.14159}

  will pick a random index `int` and assign `shift(x0,int)` the value 3.14159. Weird, but I needed it for a problem, so there it is.

- **`sum(<ex1>,<ex2>)of(<ex3>)`**: is a way of summing up things. The expressions `,<ex1>` are evaluated and their integer parts are used as the lower and upper limits of the sum. The index of the sum is `i’` so that you cannot have double sums since there is only one index. is the expression to be summed and will generally involve `i’.` For example `sum(1,10)of(i’)` will be evaluated to 55. Another example combines the sum with the shift operator. `sum(0,4)of(shift(u0,i’))` will sum up `u0` and the next four variables that were defined after it.

Add a `.xpprc` file to set your favorite options, e.g

    @ bell=0,grads=0,dwcolor=eeddff
    @ bigfont=lucidasanstypewriter-bold-14

## Command line arguments

xppautX adds its own front-end flags ahead of xpp's original ones (see
[Using the interface](04-using-the-interface.md#starting-xppautx)):

    xppautX [--browser|--server|--script FILE] [--port N] [--no-open]
            [--verbose|--debug] [--version|--help] file.ode [xpp options]

xpp's own options still apply after the file name (or anywhere, for the
ones below that predate this ordering). Many of them provide an API:
other programs or scripts can interact with `xppautX -silent` in a batch
mode, useful for processing many files or runs.

**Appearance flags with no effect any more.** These sized, coloured or
fonted the X11 windows; web2 has its own light/dark/system theme
(docs/ui-v2.md section 6) and a responsive layout instead, so xppautX
accepts and ignores them: `-xorfix`, `-allwin`, `-white`, `-bigfont
*font*`, `-smallfont *font*`, `-forecolor *color*`, `-backcolor *color*`,
`-mwcolor *color*`, `-dwcolor *color*`, `-backimage *filename*`, `-grads
*B*`, `-width *N*`, `-height *N*`, `-bell *B*`, `-ee`.

**Flags that still work**, because they are about the model, the batch
run or the files, not the X11 windows:

- **-silent**: Runs XPP's integrators without opening the front end. The result of the integration is saved to a file called `output.dat` (see `-outfile`) but this can be changed. The length of integration, methods, Poincare sections, etc, are all specified in either the [options file](14-options-file.md) or in the internal options. When you run a range integration in silent mode, if the parameter `RANGERESET` is `yes` (the default) then a new output file will be opened for each integration: ranging over 50 values gives 50 output files named `output.dat.0`, `output.dat.1`, etc. If you have set `RANGERESET=no`, then only one file is produced.
- **-convert**: Converts the old-style parser format to the new style, writing `<file>.new`.
- **-setfile *filename***: loads the named `.set` file after loading up the ODE file.
- **-newseed**: uses the machine time to re-seed the random number generator.
- **-runnow**: runs the ODE file immediately on startup (implied by `-silent`).
- **-parfile *filename***: loads parameters from the named file. The first line gives the number of parameters, then one value per line followed by its name (must match a parameter name in the ODE file), e.g. (`lecar.par`):

  ```
  12 Number params
  0.0    iapp
  .333   phi
  -.01   v1
  0.15   v2
  0.1    v3
  0.145  v4
  1.33   gca
  -.7    vk
  -.5    vl
  2.0    gk
  .5     gl
  1      om
  ```
- **-outfile *filename***: sends output to this file (default `output.dat`). The first column is time, the rest are the ordered variables, e.g. (`lecar.out`, variables `V`, `W`):

  | t | V | W |
  |---|---|---|
  | 0 | -0.36059999 | 0.0911 |
  | 0.050000001 | -0.36620989 | 0.087350026 |
  | 0.1 | -0.3715646 | 0.083690271 |
  | ⋮ | ⋮ | ⋮ |
- **-icfile *filename***: loads initial conditions from the named file, one value per line mapped to the variables in file order (variables with a differential equation, Wiener and Markov variables), e.g. (`lecar.ic`, for `V`, `W`): `-0.3606` then `0.0911`.
- **-internset *B***: run (`1`) or not (`0`) internal sets during a batch run.
- **-uset *setname***: names an internal set to run during a batch run (repeatable).
- **-rset *setname***: names an internal set *not* to run during a batch run (repeatable).
- **-include *filename***: names a file to include along with the selected file (the `#include` directive, from the command line).
- **-qsets / -qpars / -qics**: query the names of internal sets, the parameters, or the initial conditions and save the results to `-outfile`; lets an external program or script inspect an ODE model.
- **-quiet *B***: verbose log messages will (`B=0`) or will not (`B=1`) be written; xppautX's own `--verbose`/`--debug` raise the level further (core/xpp_log.h).
- **-logfile *filename***: the file to which log messages are written (also xppautX's `-logfile`).
- **-anifile *filename***: loads an animation (`.ani`) from the named file at start-up; useful for teaching demonstrations.
- **-mkplot**: with `-silent`, writes a plot named after the ODE file (or numbered, for several); SVG or PS (`-plotfmt`).
- **-noout**: does not write an output data file in batch mode; combine with `-mkplot` to suppress the data.
- **-plotfmt *ps\|svg***: sets the plot format for `-mkplot`.
- **-version**: prints xpp's version (`xppautX --version` prints this fork's).
- **-ncdraw *k***: for a figure with nullclines: `k=1` draws them on the plot; `k=2` (with `-silent`) dumps them to `nullclines.dat` instead, X-nullcline first then Y (format for gnuplot). For example, to compute the nullclines, dump them to a file and not save the run's output:

  ```
  xppautX lecar.ode -noout -silent -ncdraw 2
  ```
  or to make a plot:
  ```
  xppautX lecar.ode -noout -silent -ncdraw 1 -dfdraw 1 -mkplot
  ```
- **-dfdraw *k***: draws direction fields the same way: `k=1,2,3` unscaled, scaled or colorized on the plot; `k=4,5` dumps the scaled/unscaled coordinates to `dirfields.dat`.
- **-readset *filename***: loads anything an internal set file can hold (options, parameters, initial conditions) from one line (up to 1024 characters), e.g. a file `tst.opt` containing `iapp=0.1;phi=.05;total=500`, run as `xppautX lecar.ode -readset tst.opt` (works with `-silent` and `-mkplot` too).
- **-with *string***: the same as `-readset`, with the options inline in a quoted string (no spaces), e.g. `xppautX lecar.ode -with "iapp=0.1;phi=0.05;total=1000" -runnow`.

The only other thing on the command line should be the file name. Thus,

    xppautX test.ode -convert

will convert `test.ode` to the new format and run it.
