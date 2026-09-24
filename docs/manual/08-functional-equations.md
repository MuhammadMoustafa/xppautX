# Functional equations

In the course of some research problems, I was pursuing, I ran into some Volterra equations of the form:
``` math
u(t)=f(t)+\int_0^t K(t,s,u(s))ds
```
that I could not convert to ODES. (If $`K`$ is a convolution with a sum of powers and exponentials, then it can be converted to an ODE. Since XPP is much more efficient with ODEs and has been thoroughly debugged with respect to them, you should always attempt this conversion first.) Thus, I have added the capability to solve equations with this type of term in them. This has necessitated the addition of two new commands for the ODE file and a solver for such problems. The solver is described below in the numerical section. Since the equation above requires “memory” all the way back to $`t=0`$ and one often is interested in long time behavior, XPP truncates this to:
``` math
u(t)=f(t)+\int_{\max(t-T,0)}^t K(t,s,u(s))ds
```
where `T=DT*MaxPoints`, the latter being a parameter that you set in the numerics menu. The default is 4000. Thus, one assumes that the kernel function decays for large $`t`$ and so the tail will be small. As `MaxPoints` is a parameter, you can always make it larger at the price of taking longer to evaluate right-hand sides. Let us consider the following equation:
``` math
u(t) = \sin(t)+\frac{1}{2}(\cos(t)-\exp(t)-t\exp(t))+\int_0^t
(t-s)\exp(-(t-s))u(s)ds
```
whose solution is $`u(t)=\sin(t).`$ The following ODE file will create this model and also add an auxiliary variable with the solution for purposes of comparison.

    #  voltex1.ode
    u(t)=sin(t)+.5*cos(t)-.5*t*exp(-t)-.5*exp(-t)+int{(t-t')*exp(t'-t)*u}
    aux utrue=sin(t)
    done

The `int{K(u,t,t’)}` construction tells XPP that this is a Volterra integral. If your problem can be cast as a convolution problem, considerable speedup can be obtained since lookup tables are created. The present example is in fact a convolution problem, so that instead of the full declaration, one could instead write:

    #  voltex2.ode
    u(t)=sin(t)+.5*cos(t)-.5*t*exp(-t)-.5*exp(-t)+int{t*exp(-t)#u}
    aux utrue=sin(t)
    done

which convolves the first expression (of $`t`$ **only**) with $`u.`$ For this example, using a time step of .05 and integrating to 40, there is a 3-fold speed-up using the convolution. For more complicated kernels, it will be more.

If one wants to solve, say,
``` math
u(t) = exp(-t) + \int^t_0 (t-t')^{-mu} K(t,t',u(t'))dt'
```
the form is:

    u(t)= exp(-t) + int[mu]{K(t,t',u}

Note that the “mu” must be a number between 0 and 1. If “mu” is greater than or equal to 1, the integral is singular at 0.

## Warning

If you have parameters in your definition of the kernel and you change them, then you will have to go into the numerics menu and recompute the kernels by calling the Method command which automatically recomputes the kernels. Alternatively, turn on the AutoEval flag and it will be done automatically.

I close this section with an example of a pair of coupled oscillators in an infinite bath which has diffusion and passive decay. The equations are:
``` math
\begin{eqnarray*}
u(t) &=&\int_0^t k(t-s)F(u(s),v(s))ds + \int_0^t
k_d(t-s)F(u_1(s),v_1(s))ds \\
v(t) &=&\int_0^t k(t-s)G(u(s),v(s))ds + \int_0^t
k_d(t-s)G(u_1(s),v_1(s))ds \\
u_1(t) &=&\int_0^t k(t-s)F(u_1(s),v_1(s))ds + \int_0^t
k_d(t-s)F(u(s),v(s))ds \\
v_1(t) &=&\int_0^t k(t-s)G(u_1(s),v_1(s))ds + \int_0^t
k_d(t-s)G(u(s),v(s))ds
\end{eqnarray*}
```
where
``` math
\begin{eqnarray*}
k(t) &=& \exp(-t)/\sqrt(\pi t) \\
k_d(t) &=& \exp(-t)\exp(-d/t)/\sqrt(\pi t)
\end{eqnarray*}
```
and
``` math
\begin{eqnarray*}
F(u,v) &=& \lambda u -v - (u+qv)(u^2+v^2) \\
G(u,v) &=& \lambda v + u - (v-qu)(u^2+v^2).
\end{eqnarray*}
```
Note that the kernel $`k`$ is weakly singular and thus $`\mu=.5.`$

Note that in addition there is a singularity at $`t=0`$ for the diffusive kernel (division by zero); this can be rectified by adding a small amount to the denominator. The XPP file is as follows

    # lamvolt.ode 
    # the four variables:   
    init u=0  v=0  u1=0  v1=0  
    par lam=1.5  q=0.8  d=1  u0=1  u10=0.95  
    # 1/sqrt(pi)=
    number spi=0.56419  
    # the integral equations; since (0,0,0,0) is a rest point, I
    # add a small quickly decaying transient
    u(t)=u0*exp(-5*t)+spi*(int[.5]{exp(-t)#f}+int[.5]{exp(-t-d/(t+.0001))#f1})
    v(t)=spi*(int[.5]{exp(-t)#g}+int[.5]{exp(-t-d/(t+.0001))#g1})
    u1(t0)=u10*exp(-5*t)+spi*(int[.5]{exp(-t)#f1}+int[.5]{exp(-t-d/(t+.0001))#f})
    v1(t)=spi*(int[.5]{exp(-t)#g1}+int[.5]{exp(-t-d/(t+.0001))#g})
    # the four functions f,g,f1,g1
    f=lam*u-v-(u*u+v*v)*(u+q*v)
    g=lam*v+u-(u*u+v*v)*(v-q*u)
    f1=lam*u1-v1-(u1*u1+v1*v1)*(u1+q*v1)
    g1=lam*v1+u1-(u1*u1+v1*v1)*(v1-q*u1)
    done

Try it.
