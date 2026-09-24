# Introduction

XPP (XPPAUT is another name; I will use the two interchangeably) is a tool for solving differential equations, difference equations, delay equations, functional equations, boundary value problems, and stochastic equations. It evolved from a chapter written by John Rinzel and myself on the qualitative theory of nerve membranes and eventually became a commercial product for MSDOS computers called PHASEPLANE. It is now available as a program running under X11 and UNIX.

The code brings together a number of useful algorithms and is extremely portable. Upstream XPPAUT's graphics and interface were written completely in Xlib, which explains the somewhat idiosyncratic and primitive widgets interface; this fork (xppautX) replaced that X11 front end with a modern browser page (**web2**, see [Using the interface](04-using-the-interface.md)) that keeps the same menus, hotkeys and numerics.

XPP contains the code for the popular bifurcation program, AUTO. Thus, you can switch back and forth between XPP and AUTO, using the values of one program in the other and vice-versa. I have put a “friendly” face on AUTO as well. You do not need to know much about it to play around with it.

XPP has the capabilities for handling up to 5000 differential equations. There are solvers for delay and stiff differential equations a well as some code for boundary value problems. Difference equations are also handled. Up to 10 graphics windows can be visible at once and a variety of color combinations is supported. PostScript/SVG output is supported. Post processing is easy and includes the ability to make histograms, FFTs and applying functions to columns of your data. Equilibria and linear stability as well as one-dimensional invariant sets can be computed. Nullclines and flow fields aid in the qualitative understanding of two-dimensional models. Poincare maps and equations on cylinders and tori are also supported. Some useful averaging theory tricks and various methods for dealing with coupled oscillators are included primarily because that is what I do for a living. Equations with Dirac delta functions are allowable.

There is an animation package that allows you to create animated versions of your simulations, such as a little pendulum moving back and forth or lamprey swimming. The animation view is opened by invoking the `(V)iew axes` `(T)oon` menu item. See [Creating Animations](10-animations.md) for complete info.

I will assume that you are well versed in the theory of ordinary differential equations although you need not be to use the program. There are a number of useful features designed for people who use dynamical systems to *model* their experiments. There is a curve-fitter based on the Marquardt-Levenberg algorithm which lets you fit data points to the solutions to dynamical systems. Gnuplot-like graphics and support for some graphics objects such as text, arrows, and pointers are part of the package. You can also import bifurcation curves as part of your graphs. It is possible to automatically generate “movies” of three-dimensional views of attractors or parametric changes in the attractor as some parameters vary. I have also included a small preprocessing utility that allows one to create files for large systems of coupled equations.

There are a number of other such programs available, but they all seem to require that your problems be compiled before using them. XPP does not; I have devised a simple and fairly fast formula compiler that is based on the idea of the inner interpretor used in the language FORTH (which remains my first love as far as language is concerned) Fear not, the differential equations and boundary conditions and other formulae are written in usual algebraic notation. However, in order to run big problems very quickly, I have written the code so that it is possible to create a library that can be linked to your problem and thus create a binary with the right-hand sides compiled. This can run much faster than the parsed code. See [Creating C-files for faster simulations](11-dll-libraries.md) and [C Files](15-generated-c-files.md).

XPP has been compiled on most UNIX machines and works fine now on Windows, Macs, and Linux. Building XPP requires only the standard C compiler, and Xlib. Look at the any README files that come with the distribution for solutions to common compilation problems.

The basic unit for XPP is a single ASCII file (hereafter called an ODE file) that has the equations, parameters, variables, boundary conditions, and functions for your model. You can also include numerical parameters such as time step size and method of integration although these can also be changed within the program. The graphics and postprocessing are all done within the program using the mouse (or touch) and various menus and buttons. The impatient user should look at some sample `.ode` files instead of actually reading the documentation. There are many command line arguments and options that can be added to set up numerics ([Quick reference](16-quick-reference.md)). There is also a resource file `.xpprc` that you can add to your home directory (below). The documentation here is pretty incomplete, but covers much of the basics. There is a book from SIAM available, and `docs/upstream/tree.pdf` (the original manual's historical companion) gives a description of *every* command.

## Notes on the Interface

Upstream XPPAUT's X11 text fields had no cut and paste, and BackSpace and
Delete behaved differently across systems; this no longer applies in
web2's browser text fields, which behave like any other web form (full
cut/paste, Home/End, arrow keys, selection). See
[Using the interface](04-using-the-interface.md) for what's the same as
X11 (the menus, the single-letter hotkeys) and what's different. Almost
every command has a keyboard shortcut; these are given below.

## Disclaimer

XPP is distributed as is. The author makes no claims as to the performance of the program. Anyone is allowed to modify and distribute XPP as long as the original code is also made available. See the LICENSE file for the full caveats.

## Acknowledgements

Artie Sherman, John Rinzel for many suggestions. Daniel Dougherty and Robert McDougal for contribution actual code! Also Sebius Doedel for making AUTO available. I want to also thank Bart Oldmann for some pointers on the porting of the most recent AUTO version.

Please let me know of any bugs or other stuff that you’d like to see incorporated into XPP. I will usually fix them quickly.

My EMAIL address is bard@pitt.edu.

## Note

The easiest way to get a thorough understanding of the program as well as a short tutorial in dynamical systems is to use the World Wide Web tutorial which can be accessed from my home page at http://www.pitt.edu/$`\equiv`$phase. This tutorial is geared toward computational neuroscientists (in the choice of problems) but provides a fairly detailed introduction to the program.

## Environment variables.

While you can make a file in your home directory called `.xpprc` which contains a list of commonly used options, XPP also uses some environment variables. Note, on Windows computers the default home directory is taken to be a user’s Desktop folder.

The environment variables which XPP uses are:

- **XPPBROWSER**: Web browser to view documentation (e.g. /usr/bin/firefox)
- **XPPEDITOR**: Text editor to view/edit documentation (e.g. /usr/bin/gedit)
- **XPPHELP**: Path to the XPPAUT documentation file $`<`$xpphelp.html$`>`$ (e.g. /usr/share/doc/xppaut/html/xpphelp.html)
- **XPPSTART**: File browser will open to the specified path. This may be useful in an instructional setting to point to a mapped drive containing course materials or an NFS file share.

On Mac/Linux/Unix systems, these environment variables are typically set within a user’s .bashrc file using export commands. For example:

            export XPPHELP=/usr/share/doc/xppaut/html/xpphelp.html
            export XPPBROWSER=/usr/bin/firefox
            export XPPEDITOR=/usr/bin/nedit
            export XPPSTART=/usr/share/doc/xppaut/examples/ode

When I run XPP on Windows, I run the following bat file (xppaut.bat) which serves to set various environment variables (the `DISPLAY` line is an X11 leftover; xppautX needs no X server and ignores it):

    :: Specify location of where you want your .xpprc to be.
    :: For most Windows users their Desktop is probably a safe bet.
    set HOME=%HOMEDRIVE%%HOMEPATH%\Desktop
    :: Change path to your favorite browser
    set XPPBROWSER=c:/Program Files/Netscape/Communicator/Program/netscape.exe
    set XPPHELP=c:/xppall/help/xpphelp.html
    set DISPLAY=127.0.0.1:0.0

    :: Might want to change this to JEdit or whatever text editor you use.
    :: set XPPEDITOR=c:/Windows/notepad
    set XPPEDITOR=notepad++

    :: You may want to set XPPSTART to your research or course directory
    set XPPSTART=c:/xppall/ode

    set argC=0
    for %%x in (%*) do Set /A argC+=1
    IF %argC%==0 (c:/xppall/xppaut) ELSE (c:/xppall/xppaut %1 %2 %3)
    pause

Here is my `.xpprc` file:

    # xpprc file
    @ but=quit:fq
    @ maxstor=50000,bell=0
    @ meth=qualrk,tol=1e-6,atol=1e-6
    # thats it
