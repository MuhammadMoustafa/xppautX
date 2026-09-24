# Numerical parameters

When you click on the `nUmerics` command in the main menu, a new list appears. This is the numerics menu and allows you to set all of the numerical parameters as well as some post-processing. Some of these may not yet be implemented. Press `Esc` or click on the ` exit` to get the main menu back.

The items on this menu are:

### (T)otal

This is the amount of time to integrate. It is called `TEND` in the documentation. If it is negative then it will be made positive and no data will be stored. Thus you can integrate for very long periods of time without being told that the storage is full.

### (S)tart time

This is the initial time `T0` For autonomous systems it is usually irrelevant.

### tRansient

The program will integrate silently for this amount of time before plotting output. It is used to get rid of transients.

### (D)t

This is the step size used by the fixed step integrators and the output step for Gear,CVODE, Quality RK, Rosen,and Stiff algorithms. It is positive or negative depending on the direction of integration.

### n(C)line ctrl

will prompt you for the grid size for computing nullclines.

### s(I)ng pt ctrl

This prompts you for errors and epsilons for eigenvalues and equilibria calculations as well as the maximum iterates. If you have global flags, then you will be asked for `smin` as well.

### n(O)ut

This sets the number of integration steps to perform between output to storage. Thus, if you output every 10 steps with a `Dt` of .05, XPP will yield output at times that are $`10*.05=.5`$ timesteps apart. The advantage of this is that lengthier records of data can be made without losing accuracy of the integrator. This parameter is also used in the continuation of fixed point for maps in AUTO. If this parameter is $`n>1`$, then in AUTO, the fixed point of $`F^(n)(x)`$ is found where $`F`$ is the right-hand side of the equations. This allows AUTO to continue periodic points of maps.

### (B)ounds

This sets a global bound on the integrator. If any variable exceeds this value in magnitude, a message appears and the integration stops.

### (M)ethod

allows you to choose the methods of integration. DoPri5, DoPri83, Gear, CVODE, Quality RK, Rosen,and Stiff are adaptive. The first two are the Dormand-Prince integrators and are the most modern of the group. Gear,CVODE, Rosen, and Stiff are the best to use for stiff problems. If you choose the adaptive methods, you will be asked for error tolerance, minimum step, and maximum step size. The discrete method should be used for difference equations. If you choose CVODE/Rosen, you can choose to use the banded version of it. You should set the upper and lower bandwidths. Note that this is recommended for stiff PDEs only and should be used in conjunction with the block arrays. You can get huge speed up in the integration. I have gotten 100 fold on some problems. If the method is adaptive, the output `NOUT` is set to 1. Also the adaptive methods generate several possible error messages that you may have to respond to. These suggest how to fix the error. Markov processes are ignored by GEAR. Backward Euler prompts you for a tolerance and the maximum number of iterates for each step. The Volterra method, described below, prompts for a tolerance, maximum iterates, and a “memory size.” You will also be asked if you want the convolution kernels to be re-evaluated after any parameter is changed in either the range integration or through a manual change in parameters. If this flag is 1 then the kernels will be re-computed. The default is to not recompute them. Memory size determines how far back to save the results for the integrator. If this is small, there is a big speed-up in the integration, but you could be neglecting important terms. The symplectic integrator should only be used for systems of the form: ``` math \begin{eqnarray*} x_1' &=& v_1 \ v_1' &=& F_1(x_1,\ldots,x_n) \ x_2' &=& v_2 \ v_2' &=& F_2(x_1,\ldots,x_n) \ \vdots &=& \vdots \ x_n' &=& v_n \ v_n' &=& F_n(x_1,\ldots,x_n) \end{eqnarray*} ``` where $`F_j = \partial_{x_j} V(x_1,\ldots,x_n).`$ That is, it is for frictionless mechanical problems. The equations must be written in the above order as well or it won’t work. Symplectic integrators preserve a discrete analogue of the energy so that unlike other integrators, they preserve the invariants of the flow.

### d(E)lay

This sets the upper bound for the maximum delay and three other parameters that have to do with stability of delay equations. If in your delay equations, the delay exceeds this, a message will appear and the integration will stop. Each time this is changed all previous delay data is destroyed and you must begin your integration anew. Thus, it should be the first thing you set when solving a delay equation. Since the storage depends on the size of `Dt` when you change this, then the delay storage will also be destroyed. Delay equations require data for $`t<t_0`$ so that you should edit the `Delay ICs` to achieve this. Use the fixed step integrators for this, althought, adaptive sometimes will work. In addition to the maximal delay, you will be asked for `real part ` and `im part`. When the program looks at stability of delay equations, it considers a rectangular region defined by the four points in the complex plane. It also attempts to find one root (usually the one with the most positive real part. These two quatities provide a “guess” for the root. Once a root is found, these quantities are replaced by that root. So, if you want to find a root that is different, then change the initial guess.(See the section on numerical methods.) Finally, you will be asked for `DelayGrid` which tells the program how many steps to take on each side of the contour to determine stability. Basically, the larger this parameter is the more accurate the stability determination.

### (C)olor code

If you have a color system, XPP can code the output according to the magnitude of the velocity or the magnitude of another variable. A choice pops up for these two options or for turning off the color. This overrides any color on any other curves in your picture. Once you choose to color code, you are asked to either choose max and min values or have XPP do it for you via optimize. The latter will compute the max and min and set the scales accordingly.

### (P)oincare map

This sets up parameters for Poincare sections. A choice of four items will appear: the `Max/Min` option the `Poincare section` option, the `Periodic` option, and the option to turn `off` all maps. A parameter box will pop up and you should type in the parameters. They are the variable to check and the section and the direction. That is, a point will be recorded if the variable crosses the section such that it is either positive going to negative or vice versa according to the direction parameter. If the direction is set to zero, the, the point will be recorded from either direction. The flag `Stop on Section` instructs XPP to halt when the section is crossed. Note that automatic interpolation is done. If the section variable is `Time, T` and the section is say `T1`, then each time `T=0 modulo T1` the point is recorded. This is useful for periodically driven systems. If you have opted for the `Max/Min` option, then the section is irrelevant and the point will be recorded when a local maximum (if the direction is 1) minimum (direction=-1) or both (direction=0) of the variable is encountered. The `Periodic` option does the following. When ever the specified variable hits the section, the time of the hit is recorded and the previous time is subtracted from the current time and recorded in the time column. This leads to a list of intervals between hits. If you create an auxiliary variable that is a complicated function of the other variables, you can use this for the section thus allowing you to have sections which are not the coordinate axes.

### R(U)elle plot

This allows you to retard any of the axes by an integral number of steps. This is useful for chaotic orbits and delayed systems. Choosing a number for any of the axes will result in the variable associated with that axis being delayed by the number of steps inputted. Thus if you plot X vs X then of course you will get a diagonal line, but if you make the Y-axis delayed by say 50 and the output is every .1 timesteps, then the plot will be X(t-5) vs X(t). This does not appear during integration of the equations and is available only after a computation. You set it up and then click Restore from the main menu.

### stoc(H)astic

This brings up a series of items that allow you to compute many trajectories and find their mean and variance. It also contains commands for post-simulation data analysis. It is most useful when used with systems that are either Markovian or have noise added to the right-hand sides. The items are:
- **New seed**: Use this to reseed the random number generator. If you use the same seed then the results will not change from run to run.
- **Compute**: This will put up the same dialog box as the “Integrate” “Range” choice. Two new data sets will be created that will compute the mean and the variance of the point by point values of the trajectories over the number of trial runs you choose. If the system is completely deterministic and the parameters and initial conditions are identical for each run, then this is superfluous. Otherwise, the mean and variance are computed. You can then access these new arrays as described below. If you fire up the sample Markov problem, choose the “Compute” option, and set keep the initial data constant over say 20 runs, then you can look at the mean trajectory and its variance for each of your variables.
- **Data**: This puts the results of the most recent run into the data browser and enables plotting of them.
- **Mean**: This puts the results of the mean value of the most recently computed set of trials.
- **Variance**: This does the same for the variance.
- **Histogram**: This computes a histogram for a chosen variable and additional conditions and replaces the “t” column and the first variable column with the bin values and the number per bin respectively. You will be prompted for the number of bins, a maximum and minimum value and the variable on which to perform the histogram. Finally, you will be asked for additional conditions that involve the other stored variables (not the fixed ones though.) For example, suppose you have run an ODE/Markov system and you want the distribution of a continuous variable when the Markov variable is in state 1. Then the additional condition would be `z==1` where ` z` is the Markov variable. Multiple conditions are made by using the `&` and `| ` expressions. Note that `==` is the logical equal and is not the same as the algebraic one.
- **Old Hist**: brings back the most recently computed histogram.
- **Fourier**: This prompts you for a data column. It then computes a Fourier transform (FFT). The results are in the Browser. The first column (labeled “T”) is the mode. The second, the cosine component and the third, the sine component.
- **Power**: computes the power spectrum and the phase using the FFT.
- **Spec.dens**: this uses welch windowing to compute a smoother power spectrum than the straight-up power method. You divide your data into windows of a certain length and then premultiply by a window (square, parabolic,cosine or triangular) before taking the power over that window. The windows are then normalized and the result is normalized so that the sum is 1.
- **fIt curve**: This is a routine based on Marquardt-Levenberg algorithm for nonlinear least squares fitting. A description of the method can be found in *Numerical Recipes in C.* In this implementation, one can choose parameters and initial data to vary in an attempt to minimize the least-squares difference between solutions to a dynamical system and data. The data must be in a file in which the first column contains the independent values in increasing order. The remaining columns contain data which are to be fitted to solutions to a DE. Not all the columns need be used. When you choose this option, a window pops up with 10 entries describing the fit parameters. The items are:
  - **File**: This is the name of the data file. The first column must contain the times at which the data was taken.
  - **NCols**: This contains the total number of columns in the data file. This includes all columns in the file, even those that you will not use.
  - **Fitvar**: This is a list separated by commas or spaces of variables that you want to fit to the data. These must not be Markov variables or Auxiliary variables. They are restricted to the items that you define as “Variables” in the ODE file. Due to laziness, you can only have as many variables as you can type in 25 or fewer characters.
  - **To Col**: This should contain a list of column numbers in the data file associated with each of the variables you want to fit. Thus, for example, if you want to fit “x” to column 5 and “y” to column 2, you would type “x y” in the “Fitvar” entry and “5 2” in the “To Col” entry. The number of columns in this must equal the number of variables to be fit.
  - **Params**: These items (there are 2 of them in case you have lots of parameters you need to vary) contain the names of parameters and variables. If the name is a variable, then the initial data for that variable will be adjusted. If it is a parameter, then the parameter will be adjusted. On the initial call, the current initial data and parameter values are used. The lists of parameters and initial data can be separated by spaces or commas.
  - **Tolerance**: This is just a small number that tells the algorithm when the least square error is not changing enough to be significant. That is if either the difference is less than “TOL” or the ratio of the difference with the least square is less than “TOL” then the program will halt successfully. On should not make this too small as such differences are insignificant and a waste of CPU time. The default of .001 seems to work well.
  - **Npts**: This is the number of points in the data file that you want to fit to.
  - **Epsilon**: This is used for numerical differentiation. 1e-5 is a good value since we really don’t need precise derivatives.
  - **Max iter**: This is the maximum number of iterates you should use before giving up.
- On return, the program will put the best set of parameters that it has found. It currently is quite verbose and prints a lot of stuff to the console. This is mainly info about the current values of the parameters and the least square.
- **Liapunov exponent**: attempts to compute the maximum Liapunov exponent of the current simulation. The method is pretty simplistic but works on the examples I have fed it. Given a solution $`x(t)`$ at a series of points $`t_1,\ldots,t_n`$ I perturb the solution at each time point, integrate the equation to the next time point, and compute the logarithm of the rate of growth. This is averaged over the whole time series to give an approximation. The size of the perturbation is determined by the numerical parameter `JAC_EPS` which can be set from the numerics menu under Sing pt ctl. You will be prompted as to whether you want to compute the exponent over a range of parameters. If you choose a range, then the range dialog box will appear; fill it in as usual.
- is spike time auto-correlation. I needed this once for a class so I put it into XPP. Basically, if the data is a list of spike-times then this will make a histogram of the differences between these over the data set. That is it makes a histogram of $`x_i-x_j`$ over all values of $`i,j.`$
- is a cross correlation between two time series - more properly, it is the covariance. It returns: ``` math C_j = \frac{1}{M}\sum_{i=0}^{N-1} (x_i-bar{x})(y_{i+j}-\bar{y}) ``` where $`M`$ is the total entries used. That is, because of zero padding, calculations with low $`j`$ have more points than those with high $`j`$; this divides by the number of counts. I don’t know if this is proper, but it seems a fairer comparison.
- **loo(K)up** : This allows you to change the definitions of tabulated functions by reading in a different file or changing the formula. Thus, if you have many experimental sets of data, you can read them in one by one and integrate the equations. You are prompted for the name of a tabulated function. Then you give the filename to read in. You will continue to be prompted and can type a few carriage returns to get out. If the table was defined as a function instead of a file, then you will be prompted for the number of points, the limits of the range (` Xhi,Xlo`) ad finally, the formula of for the function defining the table. Note that it must be a function of `t`. Note that if the function contains parameters and these are changed, it will be automatically recomputed unless the AUTOEVALUATE flag is set to 0. By default, it is set to 1. You would likely set it to zero if you want to create a random table in order to implement “frozen” noise.

### bndry(V)al

prompts you for the maximum iterates, the error tolerance, and the deviation for the numerical Jacobian for the shooting method for solving BVPs.

### (A)veraging

This allows you to compute the adjoint and do averaging for weakly coupled oscillators. To use this option, you must successfully compute a periodic orbit. This does not work well with stiff systems so good luck. The menu that pops up is:
- **(N)ew adj**: This makes the adjoint from the computed periodic data. Success or failure will be noted. Separate storage is maintained for the adjoint. The program automatically puts the data from the adjoint into the browser so it can be viewed and plotted or saved.
- **(A)djoint**: This will place the adjoint data in the browser,
- **(O)rbit**: This places the periodic orbit in the browser.
- **(M)ake H**: This will prompt you for the coupling function and the result will be averaged and placed in 2 columns of storage, the first is the time, then the H function. If there are enough columns, the odd and even parts of the averaged function will be retained. As with the adjoint, this list is automatically placed in the browser. The user will be prompted for the coupling functions and should type in the formulas. The coupling is between two identical units. Say you want to couple two oscillators via a variable called `V` Then, you `V` refers to the oscillator getting the input and `V’` refers to the oscillator providing it. For example, suppose you want to study the behavior of two weakly diffusively coupled Fitzhugh-Nagumo equations: ``` math \begin{eqnarray}     dv_1/dt &=& f(v_1,w_1)+\epsilon (v_2-v_1) \     dw_1/dt &=& g(v_1,w_1) \     dv_2/dt &=& f(v_2,w_2)+\epsilon (v_1-v_2) \     dw_2/dt &=& g(v_2,w_2) \end{eqnarray} ``` Then for the coupling you would input
- v'-v
- 0
- for the required coupling.
- **(H) function**: This places the computed H function in the browser.
- Anytime you integrate, etc, the data will be placed back into the storage area.
