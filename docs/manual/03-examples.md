# Examples

Nothing helps one understand how to use a program better than lots of examples.

## Morris-Lecar Equations

The Morris-Lecar equations arise as a simplification of a model for barnacle muscle oscillations. They have the form:
``` math
\begin{eqnarray*}
C\frac{dV}{dt} &=&
g_L(V_L-V)+g_Kw(V_k-V)+g_{Ca}m_\infty(V)(V_{Ca}-V)+I \\
\frac{dw}{dt} &=& \phi \lambda_w(V)(w_\infty(V)-w)
\end{eqnarray*}
```
where
``` math
\begin{eqnarray*}
m_\infty(V) &=& .5(1+\tanh((V-V_1)/V_2)) \\
w_\infty(V) &=& .5(1+\tanh((V-V_3)/V_4)) \\
\lambda_w &=& \cosh((V-V3)/(2V_4))
\end{eqnarray*}
```
This is a very straightforward model and so the equation file is pretty simple:

    # The Morris-Lecar equations 

    # Declare the parameters
    p gl=.5,gca=1,gk=2
    p vk=-.7,vl=-.5,vca=1
    p v1=.01,v2=.145,v3=.1,v4=.15
    p i=.2,phi=.333   
      
    # Define some functions
    minf(v)=.5*(1+tanh((v-v1)/v2))
    winf(v)= .5*(1+tanh((v-v3)/v4))
    lamw(v)= cosh((v-v3)/(2*v4))

    # define the right-hand sides
    v'= gl*(vl-v)+gk*w*(vk-v)+gca*minf(v)*(vca-v)+i
    w'= phi*lamw(v)*(winf(V)-w)

    # some initial conditions -- not necessary but for completeness
    v(0)=.05
    w(0)=0

    # Done!!
    d

Note that some errors are now caught by the parser. For example, duplicate names and illegal syntax are found.

Suppose you want to keep track of the calcium current as an auxiliary variable. Then, the following file will work

    # The Morris-Lecar equations

    # Declare the parameters
    p gl=.5,gca=1,gk=2
    p vk=-.7,vl=-.5,vca=1
    p v1=.01,v2=.145,v3=.1,v4=.15
    p i=.2,phi=.333   
      
    # Define some functions
    minf(v)=.5*(1+tanh((v-v1)/v2))
    winf(v)= .5*(1+tanh((v-v3)/v4))
    lamw(v)= cosh((v-v3)/(2*v4))

    # define the right-hand sides
    v'= gl*(vl-v)+gk*w*(vk-v)-gca*minf(v)*(v-vca)+i
    w'= phi*lamw(v)*(winf(v)-w)
    #
    aux ica=gca*minf(v)*(v-vca)

    # some initial conditions -- not necessary but for completeness
    v(0)=.05
    w(0)=0

    # Done!!
    d

Note that we are wasting computational time since we compute $`I_{Ca}`$ twice; once as a contribution to the potential change and once as an auxiliary variable. In a FORTRAN or C program, one would compute it as a local variable and use it in both instances. This is the purpose of fixed variables. (For the present problem, the computational overhead is trivial, but for coupled arrays and other things, this can be quite substantial.) The last program uses a fixed variable to reduce the computation:

    # The Morris-Lecar equations ml1.ode

    # Declare the parameters
    p gl=.5,gca=1,gk=2
    p vk=-.7,vl=-.5,vca=1
    p v1=.01,v2=.145,v3=.1,v4=.15
    p i=.2,phi=.333   
      
    # Define some functions
    minf(v)=.5*(1+tanh((v-v1)/v2))
    winf(v)= .5*(1+tanh((v-v3)/v4))
    lamw(v)= cosh((v-v3)/(2*v4))

    # define the right-hand sides
    v'= gl*(vl-v)+gk*w*(vk-v)-icaf+i
    w'= phi*lamw(v)*(winf(v)-w)

    # where
    icaf=gca*minf(v)*(v-vca)

    # and
    aux ica=icaf

    # some initial conditions -- not necessary but for completeness
    v(0)=.05
    w(0)=0

    # Done!!
    d

The calcium current is computed once instead of twice. This is why “fixed” variables are useful.

**NOTE:** Since fixed quantities are not “visible” to the user and auxiliary quantities are not “visible” to the internal formula compiler, we can use the same name for the fixed as the auxiliary variable; the `aux ` declaration essentially makes it visible to the user with almost no computational overhead. However, it does generate an error message (not fatal) so it is best to make all names unique.

## A linear cable equation with boundary conditions

In studying a dendrite, one is often interested in the steady-state voltage distribution. Consider a case where the dendrite is held at $`V=V_0`$ at $`x=0`$ and has a leaky boundary condition at $`x=1.`$ Then the equations are:
``` math
\begin{eqnarray*}
\lambda^2\frac{d^2V}{dx^2} &=& V(x) \\
V(0)&=& V0 \\
a\frac{dV(1)}{dx} + b V(1) &=& 0
\end{eqnarray*}
```
When $`a=0,b\ne0`$ the voltage at $`x=1`$ is held at 0. When $`a\ne0,b=0`$ there is no leak from the cable and the conditions are for sealed end. Since this is a second order equation and XPP can only handle first order, we write it as a system of two first order equations in the file which is:

    # Linear Cable Model cable.ode

    # define the 4 parameters, a,b,v0,lambda^2
    p a=1,b=0,v0=1,lam2=1
    # now do the right-hand sides
    v'=vx
    vx'=v/lam2

    # The initial data
    v(0)=1
    vx(0)=0

    # and finally, boundary conditions
    # First we want V(0)-V0=0
    b v-v0
    #
    # We also want aV(1)+bVX(1)=0
    b a*v'+b*vx'
    # Note that the primes tell XPP to evaluate at the right endpoint
    d

Note that I have initialized $`V`$ to be 1 which is the appropriate value to agree with the first boundary condition.

## The delayed inhibitory feedback net

A simple way to get oscillatory behavior is to introduce delayed inhibition into a neural network. The equation is:
``` math
\frac{dx(t)}{dt} = -x(t) + f(ax(t)-bx(t-\tau)+P)
```
where $`f(u)=1/(1+\exp(-u))`$ and $`a,b,\tau`$ are nonnegative parameters. Here $`p`$ is an input. The XPP file is:

    # delayed feedback

    # declare all the parameters, initializing the delay to 3
    p tau=3,b=4.8,a=4,p=-.8
    # define the nonlinearity
    f(x)=1/(1+exp(-x))
    # define the right-hand sides; delaying x by tau
    dx/dt = -x + f(a*x-b*delay(x,tau)+p)
    x(0)=1
    # done
    d

Try this example after first going into the numerics menu and changing the maximal delay from 0 to say, 10. Glass and Mackey describe a few other delay systems some of which have extremely complex behavior. Try them out.

## A population problem with random mutation

This example illustrates the use of Markov variables and is due to Tom Kepler. There are two variables, $`x_1,x_2`$ and a Markov state variable, $`z.`$ The equations are:
``` math
\begin{eqnarray*}
x_1' &=& x_1(1-x_1-x_2) \\
x_2' &=& z(ax_2(1-x_1-x_2)+\epsilon x_1)
\end{eqnarray*}
```
and $`z`$ switches from 0 to 1 proportionally to $`x_1.`$ The transition matrix is 0 everywhere except in the 0 to 1 switch where it is $`\epsilon x_1.`$ So z=1 is an absorbing state.

Initial conditions should be $`x_1(0)=1.e-4`$ or so, $`x_2(0)`$ the same (this is just for convenience; we really want $`x_2(0)=0`$ and then have it jump discontinuously to $`1.e-4`$ or so when $`z`$ makes its transition, but this shouldn’t matter that much)

This models the population dynamics of two populations $`x_1,x_2`$ in competition with each other. $`x_2`$, initially absent, is a mutant of $`x_1.`$ The mutation rate $`\epsilon`$ should be smaller than one. The relative advantage, $`a`$, should be larger than one.

One expects that $`x_1`$ grows for a while, eventually $`z`$ makes its transition, $`x_2`$ begins to grow and eventually overtakes $`x_1.`$

The equation file for this is

    # Kepler model kepler.ode

    init x1=1.e-4,x2=1.e-4,z=0
    p eps=.1,a=1
    x1' = x1*(1-x1-x2)
    x2'= z*(a*x2*(1-x1-x2)+eps*x1)
    # the markov variable and its transition matrix
    markov z 2
    {0} {eps*x1}
    {0} {0}
    d

## Some equations with flags

Tyson describes a model which involves cell growth:
``` math
\begin{eqnarray*}
\frac{du}{dt} &=& k_4(v-u)(a+u^2)-k_6u \\
\frac{dv}{dt} &=& k_1m-k_6u \\
\frac{dm}{dt} &=& bm
\end{eqnarray*}
```

This is a normal looking model with exponential growth of the mass. However, if $`u`$ decreases through 0.2, then the “cell” divides in half. That is the mass is set to half of its value. Thus, we want to flag the event $`u=.2.`$ The file in the *new format* (no sense using the old format since global variables did not appear in earlier versions of XPP)

    # tyson.ode 
    i u=.0075,v=.48,m=1
    p k1=.015,k4=200,k6=2,a=.0001,b=.005
    u'=  k4*(v-u)*(a+u^2) - k6*u
    v'= k1*m - k6*u
    m'= b*m
    global -1 {u-.2} {m=.5*m} 
    d

Everything is fairly straightforward. When $`u-.2`$ decreases through zero tha is, $`u`$ is greater than .2 and then less than .2, the mass is cut in half. Integration using any of the integrators *except ADAMS!* yields a regular limit cycle oscillation.

Another example with discontinuities arises in the study of coupled oscillators. Two phase oscillators $`x,y`$ travel uniformly around the circle. If $`x`$ hits $`2\pi`$ it is reset to 0 and adds an amount $`r(y)`$ to the phase of $`y.`$ The XPP file is

    # delta coupled oscillators delta.ode
    x'=1
    y'=w
    global 1 {x-2*pi} {x=0;y=b*r(y)+y}
    global 1 {y-2*pi} {y=0;x=b*r(x)+x}
    r(x)=sin(x+phi)-sin(phi)
    par b=-.25,phi=0,w=1.0
    done

Note that there are 2 conditions and each creates 2 events.

## Large coupled systems

Many times you want to solve large coupled systems of differential equations. Writing the ODE files for these can be a tedious exercise. The array declaration described above simplifies the creation of ODE file for this. For example, suppose that you want to solve:
``` math
\frac{du_j}{dt} = -u_j + f(a\sum_{i=0}^{n-1}\cos\beta(i-j) u_i)
```
which is a discrete convolution. Suppose that `n=20` so that this is 20 equations. Its not very convenient to type these equations so instead you can create a simple file in which the equations are typed just once:

    # chain of 20 neurons
    param a=.25,beta=.31415926
    u[0..19]'=-u[j]+f(a*sum(0,19)of(cos(beta*([j]-i'))*shift(u0,i')))
    f(x)=tanh(x)
    done

The `u[0..19]` line with an index spanning `0` to `19` tells XPP to repeat this from 0 to 19. Thereafter, the string ` [j]` or some simple arithmetic expressions of `j` are evaluated and substituted verbatim. Note how I have used the shift operator on `u0` to make it act like an array. (Recall that `shift(x,n)` gives the value of the variable that is defined `n` after the variable `x` is defined.

Here is one more example of nearest neighbor coupling in an excitable medium. This is a discretization of a PDE. I will use the analogue of “no flux” boundaries so that the end ODEs will be defined separately.

    # pulse wave
    param a=.1,d=.2,eps=.05,gamma=0,i=0
    v0(0)=1
    v1(0)=1
    f(v)=-v+heav(v-a)
    #
    v0'=i+f(v0)-w0+d*(v1-v0)
    v[1..19]'=i+f(v[j])-w[j]+d*(v[j-1]-2*v[j]+v[j+1])
    v20'=i+f(v20)-w20+d*(v19-v20)
    #
    w[0..20]'=eps*(v[j]-gamma*w[j])
    @ meth=modeuler,dt=.1,total=100,xhi=100
    done

I only need to define `v0,v20` separately since `w` does not diffuse. Note how much typing I saved.

XPP comes with some other examples that I urge you to look at. A Volterra example is shown below.
