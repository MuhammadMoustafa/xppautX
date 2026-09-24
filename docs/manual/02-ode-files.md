# ODE Files

**NOTE.** *Pre 1992, XPP used a different form for ODE files. I no longer document them* A command line option lets you convert old-style to new style format.

ODE files are ASCII readable files that the XPP parser reads to create machine usable code. Lines can be continued with the standard backslash character, however, the total length of any line cannot exceed 1000 characters.

**Example.** I will start with a very simple example to get you up and running. The model is the periodically driven Fitzhugh-Nagumo equation:
``` math
\begin{eqnarray}
dv/dt &=& f(v)-w+s(t)+I_0 \\
dw/dt &=& \epsilon(v-\gamma w) \\
f(v) &=& v(1-v)(v-a) \\
s(t) &=& \alpha \sin \omega t
\end{eqnarray}
```

Here is the ODE file:

    # Forced Fitzhugh-Nagumo fhn.ode 
    dv/dt = f(v)-w+s(t)+I_0
    dw/dt = eps*(v-gamma*w)
    f(v)=v*(1-v)*(v-a)
    s(t)=al*sin(omega*t)
    param a=.25,eps=.05,gamma=1,I_0=.25
    param al=0,omega=2
    @ total=100,dt=.2,xhi=100
    done

The file is pretty self-explanatory. The first line cannot contain a number as its first character (this makes the parser think that the format of the ODE file is the old style.) The last line should be the word “done.” The names of all parameters must be declared with optional values (the default sets them to 0.) There can be as many as you can fit on each line (up to 2000 parameters) but they must be separated by commas or spaces and the “=” sign must have *no* spaces on either side of it.

You could optionally include initial data by adding either of the following sets of lines to the file:

    init v=.25,w=.3

or

    v(0)=.25
    w(0)=.3

As you have probably guessed, comments have the form:

    # This is a comment

The “@” sign tells XPP that you want to preset some of the internal parameters for numerical integration and graphing, AUTO, etc. Please put a space after the “@” on each line or it wont work. In this case, we have told XPP to integrate the equations until t=100 with a timestep of 0.25 and to set the high value for the x-axis to 100. You can, of course, change all these internal options from within XPP; this provides an easy way “set” up the problem for “one-button” operation.

## Quick exploration

Once you have written an ODE file, you can run XPP by typing

    xpp ffhn.ode

where `ffhn.ode` is the filename you created (`xppautX` for this fork's
binary; see [Using the interface](04-using-the-interface.md) for how it
starts and opens the front end in your browser).

The program loads the file. If there are errors, it reports them and exits
(`-silent`) or keeps serving the page so you can read what it printed
(the browser front end). Almost every command has a keyboard shortcut,
either the first letter or the letter in parentheses/capitalized, shown on
the menu next to the item; use the mouse to click the command or the
keyboard to type its key. To solve the differential equation with the
current parameters, click `Initialconds` and then `Go` (or type `I G`).
You will see the variable $`V(t)`$ plotted across the screen as a function
of time. Click `Xivst`. When the prompt comes up backspace over `V`, and
type in `w` and `Enter.` The variable $`w`$ will be plotted versus time.
Note that the vertical axis of the window is automatically adjusted. Click
`Viewaxes` to choose the view and fill in the form as follows:

- X-axis: V
- Y-axis: W
- Xmin: -.5
- Ymin: 0
- Xmax: 1.5
- Ymax: 1
- Xlabel: V
- Ylabel: w

and then click on `Ok.` The phase-plane will be drawn showing a limit cycle. Click `Nullclines` and then `New` to draw the nullclines. Click `Text,etc` then `Text` and type in “V-nullcline” followed by `Enter` at the prompt. Accept the defaults for text size and font by typing `Enter` twice. Move the mouse pointer to the cubic-like curve and click the button. The text should appear on the screen. Repeat this but type in “w-nullcline” for the text. Click `Graphic stuff` and then `Postscript`. Accept the defaults and a hardcopy postscript file will be produced which you can view or printout on an appropriate printer. Click ` File` and then `Quit` and answer `Yes` to exit XPP.

A much more extensive tutorial is available on the World Wide Web (see above). This document is mainly a reference to all the features (bugs :) of XPP.

## ODE File format

ODE files consist of ascii readable text which XPP uses to describe the program it wants to solve. The line length is limited to 256 characters total. Individual lines can be continued with the UNIX backslash character, $`\backslash.`$ ODE files have any combination of the following lines. The order is not too important but can matter (see below).

    # comment line - name of file, etc   
    ...
    #include <filename>
    ... 
    options <filename>
    ...
    d<name>/dt=<formula>
    <name>'=<formula>
    ...
    <name>(t)=<formula>
    ...
    volt <name>=<formula>
    ...
    <name>(t+1)=<formula>
    ...
    markov <name> <nstates>
    {t01} {t02} ... {t0k-1}
    {t10} ...
    ...
    {tk-1,0} ...
    ...
    aux <name>=<formula>
    ...
    <name>=<formula>
    ...
    parameter <name1>=<value1>,<name2>=<value2>, ...
    ...
    !<name>=<formula>
    ...
    wiener <name1>, <name2>, ...
    ...
    number <name1>=<value1>,<name2>=<value2>, ...
    ...
    <name>(<x1>,<x2>,...,<xn>)=<formula>
    ...
    table <name> <filename>
    ...
    table <name> % <npts> <xlo> <xhi> <function(t)>
    ...
    global sign {condition} {name1=form1;...}
    ...
    init <name>=<value>,...
    ...
    <name>(0)=<value> or <expr>
    ...
    bdry <expression>
    ...
    %[i1 .. i2]
    ...
    %
    command[i1..i2] ...
    ...
    name[i1..i2] ...
    ...
    0= <expression>
    ...
    solv <name>=<expression>
    ...
    special <name>=conv(type,npts,ncon,wgt,rootname)
               fconv(type,npts,ncon,wgt,rootname,root2,function)
               sparse(npts,ncon,wgt,index,rootname)
                   fsparse(npts,ncon,wgt,index,rootname,root2,function)
                   mmult(n,m,w,root)
                   fmmult(n,m,w,root1,root2,function)
               gill(0,rxnlist)
                   findext(type,n,skip,root)
    ...
    only <name2>,<name2>,...
    ... 
    # comments
    ...
    @ <name>=<value>, ...
    ...
    set <name> {x1=z1,x2=z2,...,}
    ..
    " More comments
    "  {name=val,...,name=val} active comments
    done

The typical ODE file contains some or all of the above types of lines. Continuous variables, auxiliary quantities, and Markov variables are all plottable quantities in XPP. That is, once you have solved your equation, you can plot or view any of the continuous and Markov variables or the auxiliary quantities.

XPP uses a bunch of defaults when it is started up by looking for a file called “default.opt.” If it cannot find it, it uses internal options. Alternatively, you can tell XPP the name of the options file you want to use. The description of these files is below. The format for such a statement is:

    option <filename>

which loads the options file specified in `<filename>`. You will probably not want to use this very much as you can now specify all of the parameters in the options file within your ODE file by using the “@” symbol.

XPP lets you include files in the ODE file so that for example, you can create a library of functions which your XPP ode file can call. Here is an example of two files, the first is called `test.ode` and the second is called `test.inc`:

    # test.ode
    #include test.inc
    x'=f(x)
    par a=.25
    done

    # test.inc
    f(x)=x*(1-x)*(x-a)
    #done

The contents of `test.inc` will be included into the ODE file as if you had written

    # test.ode
    f(x)=x*(1-x)*(x-a)
    x'=f(x)
    par a=.25
    done

**NOTES:** (1) At the end of every include file you have to have the statement `#done` (2) include files can include other files.

Variables are the quantities you wish to integrate in time. There are two types of variables: (i) continuous and (ii) Markov. I will first describe continuous variables. Variable names (as can all names in XPP) can have up to 64 characters each (up to 9 in XPPAUT 8 and earlier). XPP is case insensitive. Any combination of letters and numbers is valid as is the underscore, “\_”. There are 5 ways that you can tell XPP the names of the continuous variables and their right-hand sides. The following three are equivalent:

    d<name>/dt=<formula>
    <name>'=<formula>
    <name>(t)=<formula>

Here `<name>` is the name of the variable. The last version is for notational convenience only sincd the `dx/dt` notation makes no sense for discrete dynamical systems. The `<formula> ` is exactly that, the formula for the right-hand sides of the equations. These equations can appear anywhere in your file and in any order. However, the order in which they are written determines the order in which they appear in the Data Browser (see below.)

The fourth way of defining a continuous variable is:

    <name>(t) = <formula>

which tells XPP that this defines a Volterra integral equation. (It is distinguished from a definition of some function of the dummy variable `t` by the presence of an integral operator (`int{` or ` int[` ) in the right-hand side. (see below). For example, the convolution equation:
``` math
v(t) = \exp(-t) + \int_0^t e^{-(t-s)^2}v(s) ds
```
would be written as:

    v(t) = exp(-t) + int{exp(-t^2)#v}

Integro-differential equations use the `dv/dt` etc notation so that
``` math
\frac{dv(t)}{dt} = -v(t)+ \int_0^t e^{-(t-s)^2}v(s) ds
```
becomes

    dv/dt= -v+int{exp(-t^2)#v}

In the event that the right-hand side does not contain any integral operator (as would be the case, for example, if there was a fixed or hidden variable definition) then you can force the parser into making the equation a Volterra integral equation by typing

    volt v= exp(-t)+int{exp(-t^2)#v}

**NOTE.** In this format you do not write `v(t)=...` but just `v=...`

XPP gives you the option of defining many variables at once using an array-like declaration which XPP expands into a set of declarations. For example, you could declare 10 equations with the following command:

    x[1..10]'=-x[j]

and XPP would internally expand this is

    dX1/dt=-X1
    dX2/dt=-X2
    dX3/dt=-X3
    dX4/dt=-X4
    dX5/dt=-X5
    dX6/dt=-X6
    dX7/dt=-X7
    dX8/dt=-X8
    dX9/dt=-X9
    dX10/dt=-X10

Thus, you can make networks and discretizations of PDE’s compactly. Note the appearance of the expression `[j] `. This is expanded by XPP to the value of the index. Similarly, the following are allowable indices:

    [j+n]
    [j-n]
    [j*n]

where `n ` is any integer. The `[1..10] ` notation tells XPP to start `j` at 1 and go to 10. You can start with any nonnegative integer and end with any. The first number can be less than or greater than the second. XPP does not treat arrays in any efficient manner, it is as if you defined 10 or whatever variables. The names of the variables are the root name with the index appended.

Related to pseudo arrays are array blocks that have (for example) the form

    %[1..3]
    x[j]'=-y[j]
    y[j]'=x[j]
    init x[j]=1
    %

This will be expanded as follows:

    x1'=-y1
    y1'=x1
    init x1=1
    x2'=-y2
    y2'=x2
    init x2=1
    x3'=-y3
    y3'=x3
    init x3=1

which groups the “arrays” along their index rather than along the variable name. This has many disadvantages particularly if you want to put in initial data within the program. However, if you are attempting to solve a discretized version of a partial differential equation that is very stiff, then this has the advantage that the Jacobi matrix that arises from the linearization (required for stiff systems) is banded rather than dense. If you choose CVODE as the integration method (recommended for stiff systems) then there is an option to use the banded version of CVODE. For large systems (say 200 spatial points) this can result in a speed up of the order of 500- to 1000-fold! That is, a problem that would take an hour to integrate, instead takes 4 or 5 seconds. Note that Markov variables cannot be defined in one of these blocks. It will screwup!

There are two ways to define initial data in the ODE file. Either use the method of typing `init x=1.23,y=423.6 ...` or ` x(0)=1.23`. In the latter case, you can also initialize variables that involve delayed arguments. For example, `x(0)=sin(t)` will initialize `x` to be `sin(t)` for `-DELAY < t < 0` where `DELAY` is the maximum delay. WARNING: this has a few bugs in it; the formula `x(0)=t+1` will initialize `x(0)=0` but ` x(0)=1+t` will initialze `x(0)=1`. The values for $`t<0`$ will be properly evaluated but $`t=0`$ will not be.

Markov variables are finite state quantities that randomly flip from one integer state to another according to the transition probability table that is given in the ODE file. They are treated like variables in that they have initial conditions and are accessible to the user. Markov variables are declared as

    markov <name> <nstates>
    {t01} {t02} ... {t0k-1}
    {t10} ...
    ...
    {tk-1,0} ...
    ...

Each Markov variable has its own line which must begin with the letter “m”. (All other letters are ignored, but for readability, it is best to write it out.) The name of the variable, `<name>`, and the number of states, `<nstates>`, are included on the first line. The possible values are 0,1, and so on up to $`k-1`$ where $`k`$ is the number of states.

A finite state Markov variable with $`k`$ states must have associated with it a $`k\times k`$ matrix, $`T_{ij}`$ which contains the probability of going from state $`i`$ to state $`j`$ per unit of time. Thus the effective transition probability is `DeltaT` times $`T_{ij}`$; the larger is `DeltaT` the higher the probability. Since the probabilities must add to 1 in any row, the diagonal terms are automatic and ignored by XPP. If there are $`k`$ states to the variable, then there must be $`k`$ rows following the declaration of the transition matrix. Each row contains $`k`$ entries delimited by curly brackets and separated by spaces. For example, suppose $`z`$ is a two-state variable with transition probabilities, $`P_{ij}`$ then it would be defined by:

    markov z 2
    {0} {P01}
    {P10} {0}

where `P01, P10` are any *algebraic expression or number involving the parameters and variables* of the XPP file. Note that this implies that the state transitions can be dependent on any other quantities.

At each output time step, the probabilities are computed, multiplied by the timestep, and a random number is chosen. If it is in the appropriate range, then the transition will be made. Transitions of several such variables are made in parallel and then each is updated.

In many cases, you might one to keep track of some combination of your variables. For example, you might want to track the potential energy of a damped pendulum as it swings. They are declared as:

    aux <name>=formula

where `<name>` is the name of the quantity and `<formula>` is the formula for it. *Note that a formula cannot refer to an auxiliary named quantity; use fixed or hidden variables for this.* An example using the auxiliary quantity is the damped pendulum:
``` math
ml\frac{d^2x}{dt^2} = -mg\sin x -\mu\frac{dx}{dt}
```
with potential energy:
``` math
P.E. = mg(1-\cos x)
```
and kinetic energy
``` math
K.E.= \frac{1}{2}ml(\frac{dx}{dt})^2.
```
Since XPP solves systems of first order equations, this is first converted and results in the ODE file:

    # damped pendulum 
    dx/dt = xp
    dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
    aux P.E.=m*g*(1-cos(x))
    aux K.E.=.5*m*l*xp^2
    param m=10,mu=.1,g=9.8,l=1
    done

where I have also given some values to the parameters.

As with differential equations, you can also define many auxialiar variables at once with a statement like:

    aux r[1..10]=sqrt(x[j]^2+y[j]^2)

which will be expanded in the obvious fashion.

XPP allows you to define intermediate quantities that can be used in the right-hand sides of the equations. They are kept internally by XPP and here, the order in which they are declared matters. They are evaluated in the order in which they are defined, so earlier defined ones should not refer to later defined ones. The format is:

    <name> = <formula>

They are most useful if you want to use a complicated quantity in several right-hand sides. The `<name>` is kept internal to XPP and their values are not stored (unlike variables). *Note that they are different from functions which can take arguments and are not hidden from the user.*

For example, in the pendulum model above, you might also want the total energy:
``` math
T.E. = K.E. + P.E.
```
Now, as I remarked above, you cannot just add another auxiliary variable using `P.E.` and `K.E.` since they are not known to XPP. But why compute them twice. Here is how to use fixed variables in this example.

    # damped pendulum pend.ode
    dx/dt = xp
    dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
    pe=m*g*(1-cos(x))
    ke=.5*m*l*xp^2
    aux P.E.=pe
    aux K.E.=ke
    aux T.E=pe+ke
    param m=10,mu=.1,g=9.8,l=1
    done

Both energies are only computed once. (For this example, the performance difference for computing the additional quantity is negligible, but for more complex formulae, fixed quantities are useful.)

Hidden variables can also be declared in groups like ODEs:

    ica[1..10]=gca*minf(v[j])*(v[j]-eca)

this is expanded into 10 declarations:

    ica1=gca*minf(v1)*(v1-eca)
    ica2=gca*minf(v2)*(v2-eca)
    ...

DAEs can be solved with XPP by combining the `0= ` statement with the `solv ` statement. A general DAE has the form $`F(X,X',W,t)=0`$ where $`X,W`$ are vector quantities and $`X'`$ is the derivative of $`X.`$ XPP treats these in generality but currently cannot integrate past singularities that better integrators such as those found in MANPAK will traverse. I plan to add the MANPAK integrator DAEN1 shortly. In any case, the integrator still handles alot of different problems. The syntax is pretty simple. Algebraic constraints are written using the `0= ` command and the algebraic quantities (i.e. those that don’t involve derivatives) are defined using the `solv ` command. For example:
``` math
\begin{eqnarray*}
x' &=& -x \\
0 &=& x+y-1
\end{eqnarray*}
```
with $`(x(0)=1,y(0)=0)`$ would be written as:

    # dae_ex1.ode
    x'=-x
    0= x+y-1 
    x(0)=1
    solv y=0
    aux yy=y
    done

The `solv` statement tells XPP that $`y`$ is an algebraic quantity and its initial value is 0. The `aux` statement will let you also plot the value of $`y`$ since it is “hidden” from the user. Here is a more complicated equation which could not be solved by XPP without the DAE stuff:
``` math
x'+exp(x')+x=0
```
with $`x(0)=-(1+e),x'(0)=1.`$ Note that the function $`x+exp(x)`$ has no closed inverse so that we cannot write this in terms of $`x'`$. Here is the ode file:

    # dae_ex2.ode
    x'=xp
    0= xp+exp(xp)+x
    x(0)=-3.7182
    solv xp=1
    aux xdot=xp
    done

Note that we create a dummy algebraic variable called `xp` which is the derivative of $`x.`$ This is because XPP treats the derivatives in a special manner so we have to accomodate its idiosyncrasies by adding an additional algebraic variable. This last example exploits numerical errors to get the DAE solver to go beyond where it should go legally! It is a relaxation oscillator:
``` math
\begin{eqnarray*}
w' &=& v \\
0 &=& v(1-v^2)-w
\end{eqnarray*}
```
with $`w(0)=0,v(0)=1.`$ Note that the algebraic equation has multiple roots for some values of $`w`$ and thus as $`w`$ groes it must “jump” to a new branch. This cannot happen in a true DAE and in fact, one has to set tolerances low to get the numerical errors to let it work. Here is the next DAE example:

    #dae_ex3.ode
    w'=v_
    0= v_*(1-v_*v_)-w
    solv v_=1
    aux v=v_
    @ NEWT_ITER=1000,NEWT_TOL=1e-3,JAC_EPS=1e-5,METH=qualrk
    done

The important numerical parameters for the DAEs are the maximum iterates, the tolerance for Newton’s method, and the epsilon value for computing the Jacobian. These are found in the numerics menu under the menu item SingPt Control.

The DAE algebraic variables are initialized in the ODE file as formulae or constants. However, once integrated, the DAEs retain their current values, not their initial values. To change the initial DAE values, you use the Initialconds menu under the DAE sub menu.

Parameters are named quantities that represent constants in your ODE and which you can change from within the program. The format is:

    parameter <name1>=<value1>, <name2>=<value2>,...

There can be many declarations on each line. It is very important that there be *no spaces* between the `<name>` the `=` sign, and the `value` of the parameter. Without an `=` sign and a value, the parameter is set to zero by default.

Numbers are just like parameters but they are “hidden” from the user; they do not appear in the parameters windows once ypu run the program. Their only advantage is that in a problem with many defined constants, of which only a few can be freely chosen, the parameter window is not cluttered by dozens of parameters.

Derived parameters are also “hidden” from the user and allow you to define constants in terms of other parameters through formulas. Each time you change a parameter, these derived parameters are updated. They differ from “fixed” quantities in that they are not updated at every integration step. As an example, suppose you want to define area in terms of radius and length:

    par length=50,diam=10
    !area=pi*length*diam

will create a quantity called `area` that will be altered whenever you change the parameters `length,diam`. Note that the **!** in front of the name tells XPP that this is not a fixed variable and should only be updated when parameters are changed. Their values can be examined using the calculator by just inputting their names.

Wiener parameters are more properly “functions” that return scaled white noise. They are held fixed for $`t`$ to $`t+dt`$ during an integration. At each time step, they are then changed and their value is a normally distributed random number with zero mean and unit variance. The program scales them by the appropriate time step as well. Their purpose is so that one can use methods other than Euler for solving noisy problems. In particular, large steps can be taken using backward Euler without loss of stability.

The declarations:

    par a[1..5]=.25
    wiener w[1..5]
    number z[1..5]=.123456

behave in the obvious fashion. Note that this expression will lead to an error:

    par a[1..2]=.5, c=.1234

as XPP will expand it into 2 lines:

    par a1=.5, c=.1234
    par a2=.5, c=.1234

which will give an “duplicate name” error. On the other hand, this expression will work:

    par a[1..2]=.25,b[j]=.3

and is the same as:

    par a1=.25,b1=.3
    par a2=.25,b2=.3

User defined functions have the following form:

    <name>(x1,x2,...)=<expression>

where `<name>` is the name of the function and `x1,x2,...` are the dummy arguments and `<expression>` is a formula defining the function. There can be at most 9 arguments.

Tables are another type of function but (at least as of now) are only of one argument. The “table” declaration takes one of two forms: (i) file based and (ii) function-based. The file based version has the form:

    table <name> <filename>

allows you to declare a function called ` <name>` that reads in values from the file, `<filename>` or as a function of one variable over some interval and then is used in your program as a function of 1 variable interpolated from the tabulated values. The values of this table are assumed to be equally spaced and the file is an ASCII file with the format:

    <number of values>
    <xlo>
    <xhi>
    y1
    y2
    .
    .
    .
    yn

Thus, $`f(xlo)=y1`$ and $`f(xhi)=yn.`$ (If the number of points in the file description of the table starts with the ASCII character ‘i’ , (e.g. i50 instead of 50) then the interpolation will be piecewise constant; otherwise it is linear.) These tables can also be read in from within XPP but a valid table must be given to start the program. This table can be arbitrarily long (as memory permits) and thus you can use experimental data as inputs to differential equations or even sketch curves and use that as your nonlinearity.

You can directly input tabulated functions as well to speed up computations with complicated functions. In this case, after the table name put a parenthesis symbol followed by the number of points, the minimum argument and the maximum argument and then the function. This should be written as a function of “t”. Thus, the statement

    table f % 501 -10 10 tanh(t)

will produce a table of the hyperbolic tangent function from -10 to 10 consisting of 501 points.

Using the data browser, you can create tabulated data from a simulation to use later in a different simulation as a function or as input or whatever.

The “global” declaration allows you to set some conditions and then if these conditions hold reset dynamic variables according to the conditions. The form of “global” declarations is:

    global sign {condition} {name1=form1;...}

The `condition` is any combination of variables such that if it is zero, then the desired event has occurred. `sign` is either 1,-1, or 0. A sign of 1 means that if the condition goes from less than zero to greater than zero, the event has occurred. A sign of -1 means that if the condition *decreases* through zero that event has occurred. Finally, a sign of 0, means that any crossing of zero signals an event. Each time the condition is met, events occur. There can be up to 20 events per condition and they are delimited by braces and separated by semicolons. Events are always of the form: `variable=formula` where `variable` is one of the differential equation variables or parameters and formula is some formula involving the variables. All formulae are first evaluated and then the variables are updated. Some examples are shown below. **WARNING!** The global flags are *ignored* by the adams integrator. Use GEAR, EULER, RUNGE-KUTTA, BACKWARD EULER, MODIFIED EULER, STIFF, QUALITY-RK, DORMAND-PRINCE, ROSENBROCK or CVODE. There is one special event that you can put into the list of events: `out_put=val` if `val>0` then the current value of all variables etc is stored. This allows you to, for example, get faster more accurate Poincare maps.

*Note.* You will sometimes get the rather obscure message that the program is “Working too hard.” This is a diagnostic that one of two things is occurring. First, due to round-off, sometimes the extrapolation to zero for the condition is actually not zero but is instead some very small number. Thus XPP checks for the definition of this which is called `s ` and if it is less than a user defined value of `smin` (changed in the numerics menu under “sIng pt ctrl”) then it is treated as zero. The console reports this number when the error message occurs so that you can change `smin` to be larger, say 1e-13, to avoid this message.

The second situation that causes this to arise is that the time step `DT` is too large and the same condition is occuring twice in that step. Since the interpolation is linear, this means you should try to take smaller steps; otherwise numerical errors will accumulate.

Global flags can also be defined in groups, for example:

    global 1 x[1..5]-1 {x[j]=0}

The default for initial conditions of Markov and continuous variables is zero. Initial data can also be set in the ODE file (and of course easily set from within XPP) in one of two ways:

    <name>(0)=value

will set the variable `<name>` to the specified value. Alternatively, you can initialize many variables on one line by typing

    init <name1>=value1, <name2>=value2, ... 

Boundary conditions can be placed anywhere in the file or ignored altogether if you don’t plan on solving boundary value problems. They have the form:

    bndry <expression>

where stands for boundary condition. The expression is one involving your variables and which will be set to zero. In order to distinguish left and right boundary conditions, the following notation is used. For the values of the variables at the left end of the interval, use the symbol for the variable. For the values at the right end of the interval, use the symbol for the variable appended by a single quote, ’. Thus to specify the boundary conditions $`x(0)=1`$, $`y(1)=2`$, the following is used:

    bndry x-1
    bndry y'-2

Periodic boundary conditions would be written as:

    bndry x-x'
    bndry y-y'

Note that it is not necessary to specify the BCs at this point. They can be specified within the program. Also note that the notation I have used allows the specification of mixed boundary conditions such as periodic BCs. (XPP has some additional special commands for periodic boundary conditions that enable the user to specify fewer equations than usual.) The number of boundary conditions must match the number of variables in your problem. Do not use the boundary value solver with Volterra, delay, stochastic, or discrete equations.

Initial and boundary conditions can also be defined *en masse* via:

    bdry x[1..2]-2
    x[1..2](0)=.345

and this will be expanded in the expected fashion.

This is relevant only if you are using XPP in silent model. The `only` declaration will save to the output only the variables specified by this command.

XPP has many many internal parameters that you can set from within the program and four parameters that can only be set before it is run. Most of these internal parameters can be set from the “Options” files described above and whose format is at the end of this document. However, it is often useful to put the options right into the ODE file. *NOTE: Any options defined in the ODE file override all others such as the onres in the OPTIONS file.* In addition, there are several options not available in the options file. These options are used by the “silent” integrator to produce a file for output when running without X.

The format for changing the options is:

    @ name1=value1, name2=value2, ...

where `name` is one of the following and `value` is either an integer, floating point, or string. (All names can be upper or lower case). The first four options *can only be set outside the program.* They are:

- MAXSTOR=`integer` sets the total number of time steps that will be kept in memory. The default is 5000. If you want to perform very long integrations change this to some large number.

- BACK= `{Black,White}` sets the background to black or white.

- SMALL=`fontname` where `fontname` is some font available to your X-server. This sets the “small” font which is used in the Data Browser and in some other windows.

- BIG=`fontname` sets the font for all the menus and popups.

- SMC={0,...,10} sets the stable manifold color

- UMC={0,...,10} sets the unstable manifold color

- XNC={0,...,10} sets the X-nullcline color

- YNC={0,...,10} sets the Y-nullcline color

- BUT=s1:s2 defines a user button. You can use this many times in the same ODE file. The first string is the name of the button.The second is the set of key stroke short cuts. For example: ` BUT=mouse:im` will put a button on the main window labeled `mouse` and when you press it, it will act as though you had clicked ` InitConds Mouse`.

The remaining options can be set from within the program. They are

- LT=`int` sets the linetype. It should be less than 2 and greater than -6.

- SEED=`int` sets the random number generator seed.

- XP=name sets the name of the variable to plot on the x-axis. The default is `T`, the time-variable.

- YP=name sets the name of the variable on the y-axis.

- ZP=name sets the name of the variable on the z-axis (if the plot is 3D.)

- NPLOT=`int` tells XPP how many plots will be in the opening screen.

- XP2=name,YP2=name,ZP2=name tells XPP the variables on the axes of the second curve; XP8 etc are for the 8th plot. Up to 8 total plots can be specified on opening. They will be given different colors.

- AXES=`{2,3}` determine whether a 2D or 3D plot will be displayed.

- TOTAL=value sets the total amount of time to integrate the equations (default is 20).

- DT=value sets the time step for the integrator (default is 0.05).

- NJMP=`integer`, NOUT=`integer` tell XPP how frequently to output the solution to the ODE. The default is 1, which means at each integration step. It is also used to specify a the period for maps in the continuation package AUTO.

- T0=value sets the starting time (default is 0).

- TRANS=value tells XPP to integrate until `T=TRANS` and then start plotting solutions (default is 0.)

- NMESH=`integer` sets the mesh size for computing nullclines (default is 40).

- {BANDUP=int, BANDLO=int} sets the upper and lower limits for banded systems which use the banded version of the CVODE integrator.

- METH=`{ discrete,euler,modeuler,rungekutta,adams,gear,volterra, backeul, qualrk,stiff,cvode,5dp,83dp,2rb, ymp}` sets the integration method (see below; default is Runge-Kutta.) The latter four are the two Dormand-Prince integrators, the Rosenbrock, and the symplectic integrators.

- DTMIN=value sets the minimum allowable timestep for the Gear integrator.

- DTMAX=value sets the maximum allowable timestep for the Gear integrator

- VMAXPTS=value sets the number of points maintained in for the Volterra integral solver. The default is 4000.

- { JAC_EPS=value, NEWT_TOL=value, NEWT_ITER=value} set parameters for the root finders.

- ATOLER=value sets the absolute tolerance for several of the integrators.

- TOLER=value sets the error tolerance for the Gear, adaptive RK, and stiff integrators. It is the relative tolerance for CVODE and the Dormand-Prince integrators.

- BOUND=value sets the maximum bound any plotted variable can reach in magnitude. If any plottable quantity exceeds this, the integrator will halt with a warning. The program will not stop however (default is 100.)

- DELAY=value sets the maximum delay allowed in the integration (default is 0.)

- PHI=value,THETA=value set the angles for the three-dimensional plots.

- XLO=value,YLO=value,XHI=value,YHI=value set the limits for two-dimensional plots (defaults are 0,-2,20,2 respectively.) Note that for three-dimensional plots, the plot is scaled to a cube with vertices that are $`\pm1`$ and this cube is rotated and projected onto the plane so setting these to $`\pm2`$ works well for 3D plots.

- XMAX=value, XMIN=value, YMAX=value, YMIN=value, ZMAX=value, ZMIN=value set the scaling for three-d plots.

- OUTPUT=filename sets the filename to which you want to write for “silent” integration. The default is “output.dat”.

- POIMAP=`{ section,maxmin} ` sets up a Poincare map for either sections of a variable or the extrema.

- POIVAR=name sets the variable name whose section you are interested in finding.

- POIPLN=value is the value of the section; it is a floating point.

- POISGN=`{ 1, -1, 0 }` determines the direction of the section.

- POISTOP=1 means to stop the integration when the section is reached.

- RANGE=1 means that you want to run a range integration (in batch mode).

- RANGEOVER=name, RANGESTEP, RANGELOW, RANGEHIGH, RANGERESET=` Yes,No`, RANGEOLDIC=`Yes,No` all correspond to the entries in the range integration option (see below).

- TOR_PER=value, defined the period for a toroidal phasespace and tellx XPP that there will be some variables on the circle.

- FOLD=name, tells XPP that the variable \<name\> is to be considered modulo the period. You can repeat this for many variables.

- STOCH=`1,2` is useful in batch mode and allows one to use the RANGE parameters to compute a mean (1) trajectory or its variance (2). That is, suppose you want to average a simulation over 100 trials. Then set RANGESTEP=500,RANGELOW=0,RANGEHIGH=0, STOCH=1.

- AUTOEVAL=`{0,1}` tells XPP whether or not to automatically re-evaluate tables everytime a parameter is changed. The default is to do this. However for random tables, you may want this off. Each table can be flagged individually within XPP.

- Postscript options. `PS_COLOR,PS_FONT,PS_LW,PS_FSIZE` represent respectively, the color flag (0/1), the name of the font (default is Times-Roman), the linewidth in tenths of a point (default 5), the fontsize (in points, default 14).

- AUTO-stuff. The following AUTO-specific variables can also be set: `NTST, NMAX, NPR, EPSU, EPSS, EPSL, DSMIN, DSMAX, DS, PARMIN, PARMAX, NORMMIN, NORMMAX, AUTOXMIN, AUTOXMAX, AUTOYMIN, AUTOYMAX, AUTOVAR`. The last is the variable to plot on the y-axis. The x-axis variable is always the first parameter in the ODE file unless you change it within AUTO.

Sometimes, you wnat to prepare a bunch of simulations that use different initial data or parameter values or numerical methods, etc. You can, of course, change these within the program by choosing the desired option and changing it. Or, you can do the simulation and save the results in a “.set” file (see below). Another way to do this is by adding a bunch of parameter sets to the ode file. The format for this is:

    set name {item1=value1, item2=value2, ..., }

Then, you tell XPP to use the named set and it will do all of the things inside the brackets. The items are any parameter name, any variable name, or any of the internal options named above. The named sets are accessed through the `Get par set` menu item in the ` File` menu. Here is an example:

    # test
    x'=-a*x+c
    par a=1,c=1
    set set1 {a=1,c=1,x=0,dt=.25}
    set set2 {a=.25,c=0,x=1,dt=.1}
    done

If you load “set1” then the parameters, initial conditions, and `DeltaT` will be set to the values in the brackets. Choosing “set2” sets them differently. The names can be anything you like.

The `special ` directive allows you to create dense coupled systems of ODEs and is much faster than using the more general summation operator `sum`. There are two types of convolutions and 2 types of “sparse” coupling functions. The synatx is

    special zip=conv(type,npts,ncon,wgt,root)

This will produce an array, `zip` of `npts` is length defined as:
``` math
\hbox{zip}[i] =\sum_{j=-\hbox{ncon}}^{\hbox{ncon}}\hbox{wgt}[j+ncon]
\hbox{root}[i+j]
```
for $`i=0,\ldots,npts-1.`$ `root` is the name of a variable and thus, there must be at least `npts-1` variables defined after ` root.` The array `wgt` is defined as a table using the ` tabular` command and must be of length `2 ncon + 1.` The ` type` determines the nature of the convolution at the edges. Type ` even` reflects the boundaries, `periodic` makes them periodic, and `0` does not include them in the sum. The object `zip` behaves as a function of ne variable with domain 0 to `npts-1`. Here is an example

    # neural network
    tabular wgt % 25 -12 12 1/25
    f(u)=1/(1+exp(-beta*u))
    special k=conv(even,51,12,wgt,u0)
    u[0..50]'=-u[j]+f(a*k([j])-thr)
    par a=4,beta=10,thr=1
    done

The `sparse` network has the syntax:

    special zip=sparse(npts,ncon,wgt,index,root)

where `wgt` and `index` are tables with at least `npts * ncon` entries. The array `index` returns the indices of the offsets to with which to connect and the array `wgt` is the coupling strength. The return is

    zip[i] = sum(j=0;j<ncon) w[i*ncon+j]*root[k]
    k = index[i*ncon+j] 

Thus one can make complicated types of couplings. The following is a randomly coupled network with 5 random connections of random strength:

    # junk2.ode
    table w % 255 0 255 .4*ran(1)
    table ind % 255 0 255 flr(51*ran(1))
    special bob=sparse(51,5,w,ind,v0)
    v[0..50]'=-v[j]+f(k*bob([j])-thr-c*delay(v[j],tau))
    par k=3,thr=1,beta=1,c=2.5,tau=5
    f(u)=1/(1+exp(-beta*u))
    done

The other two types of networks allow more complicated interactions:

    special zip=fconv(type,npts,ncon,wgt,root1,root2,f)

evaluates as

    zip[i]=sum(j=-ncon;j=ncon) wgt[ncon+j]*f(root1[i+j],root2[i])

and

    special zip=fsparse(npts,ncon,wgt,index,root1,root2,f)

evaluates as

    zip[i]=sum(j=0;j<ncon) wgt[ncon*i+j]*f(root1[k],root2[i])
    k = index[i*ncon+j] 

They are useful for coupled phase oscillator models.

There are two more such functions which are essentially just matric multiplications.

    special k=mmult(n,m,w,u)

returns a vector `k` of length `m ` defined as
``` math
k(j)=\sum_{i=0}^{n-1} w(i+nj)u(i)
```
The associated functional operator:

    special k=fmmult(n,m,w,u,v,f)

returns
``` math
k(j)=\sum_{i=0}^{n-1} w(i+nj)f(u(i),v(j)).
```

The command

    special z=gill(0,rxnlist)

sets up a list of reactions to implement the gillespie method. (The 0 is superfluous at this point and serves as a place-holder for implementing faster approximations) The reactions are a set of fixed variable that you include. The vector `z` returns the following information: $`z(0)`$ returns the time to the next reaction; $`z(1,\ldots,m)`$ returns a 0 or 1 according as to whether that reaction took place. Here is an example from Gillespie

    # gillesp_bruss.ode
    # gillespie algorithm for brusselator
    #
    # x1  -> y1 (c1)
    # x2+y1 -> y2+Z (c2)
    # 2 y1 + y2 -> 3 y1 (c3)
    # y1 -> Z2 (c4)
    par c1x1=5000,c2x2=50,c3=.00005,c4=5
    init y1=1000,y2=2000
    #  compute the reaction rates
    r1=c1x1
    r2=c2x2*y1
    r3=c3*y1*y2*(y1-1)/2
    r4=c4*y1
    special z=gill(0,r{1-4})
    tr'=tr+z(0)
    y1'=y1+z(1)-z(2)+z(3)-z(4)
    y2'=y2+z(2)-z(3)
    @ bound=100000000,meth=discrete,total=1000000,njmp=1000
    @ xp=y1,yp=y2
    @ xlo=0,ylo=0,xhi=10000,yhi=10000
    done

Note the form of the reaction list. Other reactions can be added separated by commas. This appears to be easier to code that the way that I used in the XPP book before i implemented this new algorithm.

A line that starts with a quote mark `" ` is treated differently from the normal comments and is included in a special buffer. This is to separate out comments that are descriptive of the general file as opposed to line by line comments which when separated from the ODE file have no context. **Furthermore** you can add “actions” associated with these comments. That is you can set XPP parameters like integration method, etc and also parameters and initial data. These comments have the form:

    " {gca=1.2,gk=0} Set the potassium to zero and turn on the calcium

Clicking on `File Prt info` brings up a window with the ODE source code. Clicking on `Action` in this window brings up the active comments. The user does not see `{gca=1.2,gk=0} ` but instead sees:

    * Set the potassium to zero and turn on the calcium

with an asterisk to indicate there is n action associated with the line. Clicking on the will set `gca=1.2` and `gk=0`. Thus, you can make nice little tutorials within the ODE file. These are limited to 500 lines.

The last line in the file should be “done” telling the ODE reader that the file is over.

All of the declarations, ` markov, parameter, wiener, table, aux, init, bndry, global, done, ` can be abbreviated by their first letter.

## Reserved words

You should be aware of the following keywords that should not be used in your ODE files for anything other than their meaning here.

    sin cos tan atan atan2 sinh cosh tanh
    exp delay ln log log10 t pi if then else mod
    asin acos heav sign  flr ran abs del\_shft
    max min normal besselj bessely erf erfc poisson
    arg1 ... arg9  @ $ + - / * ^ ** shift
    | > < == >= <= != not \# int sum of i'

These are mainly self-explanatory. The nonobvious ones are:

- **`atan2(x,y)`**: is the argument of the complex number $`x+iy.`$

- **`heav(arg1)`**: the step function, zero if `arg1<0` and 1 otherwise.

- **`sign(arg)`**: which is the sign of the argument (zero has sign 0)

- **`ran(arg)`**: produces a uniformly distributed random number between 0 and `arg.`

- **`besselj, bessely `**: take two arguments, $`n,x`$ and return respectively, $`J_n(x)`$ and $`Y_n(x),`$ the Bessel functions.

- **`erf(x), erfc(x)`**: are the error function and the complementary function.

- **`normal(arg1,arg2)`**: produces a normally distributed random number with mean `arg1` and variance `arg2`.

- **`poisson(arg1)`**: produces an integer which is the expected number of events for the value of `arg` from a Poisson process.

- **`max(arg1,arg2)`**: produces the maximum of the two arguments and `min` is the minimum of them.

- **`mod(arg1,arg2)`**: is `arg1` modulo `arg2`.

- **`if(<exp1>)then(<exp2>)else(<exp3>)`**: evaluates ` ` If it is nonzero it evaluates to otherwise it is . E.g. `if(x>1)then(ln(x))else(x-1)` will lead to `ln(2)` if `x=2` and `-1 if x=0.`

- **`delay(<var>,<exp>)`**: returns variable `<var>` delayed by the result of evaluating `<exp>`. In order to use the delay you must inform the program of the maximal possible delay so it can allocate storage. (See the section on the NUMERICS menus.)

- **`flr(arg)`**: is the integer part of`<arg>` returning the largest integer less than `<arg>`.

- **`t `**: is the current time in the integration of the differential equation.

- **`pi`**: is $`\pi.`$

- **`arg1, ..., arg9`**: are the formal arguments for functions

- **`int, #`**: concern Volterra equations.

- **`shift(<var>,<exp>)`**: This operator evaluates the expression `<exp>` converts it to an integer and then uses this to indirectly address a variable whose address is that of `<var>` plus the integer value of the expression. This is a way to imitate arrays in XPP. For example if you defined the sequence of 5 variables, ` u0,u1,u2,u3,u4` one right after another, then `shift(u0,2)` would return the value of `u2.`

- **`del_shft(<var>,<shft>,<delay>).`**: This operator combines the `delay` and the `shift` operators and returns the value of the variable `<var>` shifted by `<shft>` at the delayed time given by `<delay>`. It is of limited utility as far as I know, but I needed it for a problem, so here it is.

- **`sum(<ex1>,<ex2>)of(<ex3>)`**: is a way of summing up things. The expressions `,<ex1>` are evaluated and their integer parts are used as the lower and upper limits of the sum. The index of the sum is `i’` so that you cannot have double sums since there is only one index. is the expression to be summed and will generally involve `i’.` For example `sum(1,10)of(i’)` will be evaluated to 55. Another example combines the sum with the shift operator. `sum(0,4)of(shift(u0,i’))` will sum up `u0` and the next four variables that were defined after it. An example below shows how this can be used to solve large systems of equations that are densely coupled.

The parser in this distribution is a great improvement over the old style parser. The parser distinguishes between the old style and the new style by whether or not the first line of the file contains a number. If the first line is a number, then the old style parser is used. Otherwise, the new style is used.
