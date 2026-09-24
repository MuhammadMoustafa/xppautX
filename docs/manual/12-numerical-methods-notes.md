# Some comments on the numerical methods

Most are standard.

1.  The BVP solve works by shooting and using Newton’s method. All Jacobi matrices are computed numerically.

2.  The nullclines are found by dividing the window into a grid and evaluating the vector field at each point. Zero contours are found and plotted.

3.  Equilibria are found with Newton’s method and eigenvalues are found by the QR algorithm. Invariant sets are found by using initial data along an eigenvector for the corresponding eigenvalue. The eigenvector is computed by inverse iteration.

4.  The Gear algorithm is out of Gear’s text on numerical methods.

5.  The two adaptive algorithms qualrk4 and stiff are from Numerical Recipes.

6.  CVODE is based on a C-version of LSODE. It was written by Scott D. Cohen and Alan C. Hindmarsh, Numerical Mathematics Group, Center for Computational Sciences and Engineering, L-316, Lawrence Livermore National Lab, Livermore, CA 94551. email: alanh@llnl.gov. You can get full documentation for this powerful package `http://netlib.bell-labs.com/netlib/ode/index.html`.

7.  Dormand/Prince are from their book

8.  Rosenbrock is based on a Matlab version of the two step Rosenbrock algorithm (see Numerical Recipes again)

9.  Delay equations are solved by storing previous data and quadratically interpolating from this data.

10. Stability of delay equations is computed by a method suggested by Tatyana Luzyanina. The linearized stability for a delay equation results in solving a transcendental equation $`f(z)=0.`$ The idea is to use the argument principle and compute the total change in the argument of $`f(z)`$ as $`z`$ goes around a a contour $`C.`$ The number of times divided by $`2\pi`$ tells us the number of roots of $`f`$ inside the contour. Thus, XPP simply adds values of the argument of $`f(z)`$ at discrete points on a large contour defined by the user and which encloses a big chunk of the right-half plane. Obviously the best it can do is give sufficient conditions for instability as there could always be roots outside the contour. But it seems to work pretty well with modest contours except near changes in stability. In addition, XPP tries to find a specific eigenvalue by using Newton’s method on the characteristic equation. Since there are infinitely many possible roots to these transcendental equations, the root found can be arbitrary. However, suppose there is a single pair of roots in the right-half plane. Then guessing a positive root will often land you on the desired root. Using the Singular Point Range option will follow this particular root as a parameter varies. This can often lead to a discovery of the value of the parameter for which there is a Hopf bifurcation.

11. Adjoints are computed for stable periodic orbits by integrating the negative transpose of the numerically computed variational equation backwards in time using backward Euler.

12. The Volterra solver uses essentially an integrator (second order) based on the implicit product scheme described in Peter Linz’s book on Volterra equations (SIAM,1985). For ODEs implicit schemes take considerably more time than explicit ones, but since most of the compute time for Volterra equations is in approximating the integral, this time penalty is minimal. Performance is gained primarily by taking advantage of convolution type equations.

13. Normally distributed noise is computed by the Box-Muller transformation of uniform noise.

14. The curve-fitting is done by using a heavily customized version of the Marquardt-Levenberg algorithm taken from Numerical Recipes in C.

15. The FFT is through the usual means

16. The algorithms in the bifurcation package are described in the AUTO manual available from Eusebius Doedel. I just wrote the interface.

17. DAEs of the form $`F(X',X,W,t)=0`$ are solved by fixing $`X`$ and using Newton’s method to compute $`(X',W)`$. The value of $`X'`$ is sent to the integrator to update $`X.`$ This apparently is not how DASSL does it. It is essentially the method used by Rheinboldt et al in their package MANPAK. (I must confess that I invented my own way to solve them out of the inability to get DASSL to work.)
