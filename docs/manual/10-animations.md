# Creating Animations

Many years ago, as a teenager, I used to make animated movies using various objects like Kraft caramels (“Caramel Knowledge”) and vegetables (“The Call of the Wild Vegetables”). After I got my first computer, I wanted to develop a language to automate computer animation. As usual, things like jobs, family, etc got in the way and besides many far better programmers have created computer assisted animation programs. Thus, I abandoned this idea until I recently was simulating a simple toy as a project with an undergraduate. I thought it would be really cool if there were a way to pipe the output of a solution to the differential equation into some little cartoon of the toy. This would certainly make the visualization of the object much more intuitive. There were immediately many scientific reasons that would make such visualization useful as well. Watching the gaits of an animal or the waving of cilia or the synchronization of many oscillators could be done much better with animation than two and three dimensional plots of the state variables.

With this in mind, I have developed a simple scripting language that allows the user to make little cartoons and show them in a dedicated window. The following steps are required

- Run the numerical simulation for however long you need it.

- Using a text editor create a description of the animation using the little scripting language described below.

- Click on the `(V)iew axes` `(T)oon` menu item from the main XPP window.

- Click on the `File` item in the animation window that pops up and give it the name of your script file.

- If the file is OK, click on `Go` item and the animation will begin.

## The animation view

**In web2**, `(V)iew axes` `(T)oon` opens the **Animation** tab
(`ui/AniView.tsx`) instead of a separate resizeable window; there is no
pixmap-memory limit to run out of. The core computes each frame's
primitives in the `.ani` file's own unit coordinates (the `ani` `frame`
data event, `core/ani_data.cpp`) and the page draws them on a canvas
scaled to the tab's own size, so the picture fits any viewport from a
phone to a wide desktop. The controls are the same ideas as the X11
window's six buttons, as player controls:

- **File**: loads a new animation file (usual extension `filename.ani`,
  as below), through the browser's file dialog like any other Open.
- **Go / Pause**: plays and pauses; a step after Pause continues from the
  frame shown, unlike the X11 window's slow round trip through Pause.
- **Reset**: moves the animation back to the beginning.
- **the seek slider, and the frame step buttons** (`>>>>`/`<<<<` in
  X11): move to any frame or step one at a time; the delay between frames
  sets the speed (Fast/Slow in X11 are this delay).
- **Skip**: sets the number of frames to skip while playing.
- Saving frames: kinescope-style export (GIF/PNG made in the page from
  the frames it already has) replaces `Mpeg` and the `anigif` option; see
  "Saving pictures and files" in
  [Using the interface](04-using-the-interface.md).

At a phone width the tab is a full-screen sheet, with 44 px touch targets
on every control, and no sideways scroll.

## DASL: Dynamical Animation Scripting Language

In order to use the animation components of XPP, you must first describe the animation that you want to do. This is done by creating a script with a text editor that describes the animation. You describe the coordinates and colors of a number of simple geometric objects. The coordinates and colors of these objects can depend on the values of the variables (both regular and fixed, but not auxiliary) at a given time. The animation then runs through the output of the numerical solution and draws the objects based on that output. I will first list all the commands and then give some examples.

Basically, there are two different types of objects: (i) transient and (ii) permanent. Transient objects have coordinates that are recomputed at every time as they are changing with the output of the simulation. Permanent objects are computed once and are fixed through the duration of the simulation. The objects themselves are simple geometric figures and text that can be put together to form the animation.

Each line in the script file consists of a command or object followed by a list of coordinates that must be separated by semicolons. For some objects, there are other descriptors which can be optional. Since color is important in visualization, there are two ways a color can be described. Either as a formula which is computed to yield a number between 0 and 1 or as an actual color name started with the dollar sign symbol, \$. The `ani` file consists of a lines of commands and objects which are loaded into XPP and played back on the animation window. Here are the commands:

- ****dimension****: xlo;ylo;xhi;yho
- ****speed****: delay
- ****transient****:
- ****permanent****:
- ****line****: x1;y1;x2;y2;color;thickness
- ****rline****: x1;y1;color;thickness
- ****rect****: x1;y1;x2;y2;color;thickness
- ****frect****: x1;y1;x2;y2;color
- ****circ****: x1;y1;rad;color;thickness
- ****fcirc****: x1;y1;rad;color
- ****ellip****: x1;y1;rx;ry;color;thickness
- ****fellip****: x1;y1;rx;ry;color
- ****comet****: x1;y1;type;n;color
- ****text****: x1;y1;s
- ****vtext****: x1;y1;s;z
- ****settext****: size;font;color
- ****xnull****: x1;y1;x2;y2;color;id
- ****ynull****: x1;y1;x2;y2;color;id
- ****end****:

All commands can be abbreviated to their first three letters and case is ignored. At startup the dimension of the animation window in user coordinates is (0,0) at the bottom left and (1,1) at the top right. Thus the point (0.5,0.5) is the center no matter what the actual size of the window on the screen. **Color** is described by either a floating point number between 0 and 1 with 0 corresponding to red and 1 to violet. When described as a floating point number, it can be a formula that depends on the variables. In all the commands, the color is optional *except* **settext.** The other way of describing color is to use names which all start with the \$ symbol. The names are: **\$WHITE, \$RED, \$REDORANGE, \$ORANGE, \$YELLOWORANGE, \$YELLOW, \$YELLOWGREEN, \$GREEN, \$BLUEGREEN, \$BLUE,\$PURPLE, \$BLACK**.

The **transient** and **permanent** declarations tell the animator whether the coordinates have to be evaluated at every time or if they are fixed for all time. The default when the file is loaded is **transient.** Thus, these are just toggles between the two different types of objects.

The number following the **speed** declaration must be a nonnegative integer. It tells tha animator how many milliseconds to wait between pictures.

The **dimension** command requires 4 numbers following it. They are the coordinates of the lower left corner and the upper right. The defaults are (0,0) and (1,1).

The **settext** command tells the animator what size and color to make the next text output. The size must be an integer, **{ 0,1,2,3,4 }** with 0 the smallest and 4 the biggest. The font is either **roman** or **symbol.** The color must be a named color and not one that is evaluated.

The remaining ten commands all put something on the screen.

- ****line x1;y1;x2;y2;color;thickness**** : draws a line from **(x1,y1)** to **(x2,y2)** in user coordinates. These four numbers can be any expression that involves variables and fixed variables from your simulation. They are evaluated at each time step (unless the line is **permanent**) and this is scaled to be drawn in the window. The **color** is optional and can either be a named color or an expression that is to be evaluated. The **thickness** is also optional but if you want to include this, you must include the **color** as well. **thickness** is any nonnegative integer and will result in a thicker line.

- ****rline x1;y1;color;thickness**** : is similar to the **line** command, but a line is drawn from the endpoints of the last line drawn to **(xold+x1,yold+y1)** which becomes then new last point. All other options are the same. This is thus a “relative” line.

- ****rect x1;y1;x2;y2;color;thickness**** : draws a rectangle with lower corner **(x1,y1)** to upper corner **(x2,y2)** with optional color and thickness.

- ****frect x1;y1;x2;y2;color**** : draws a filled rectangle with lower corner **(x1,y1)** to upper corner **(x2,y2)** with optional color.

- ****circ x1;y1;rad;color;thick**** : draws a circle with radius **rad** centered at **(x1,y1)** with optional color and thickness.

- ****fcirc x1;y1;rad;color**** : draws a filled circle with radius **rad** centered at **(x1,y1)** with optional color.

- ****ellip x1;y1;rx;ry;color**** : draws an ellipse with radii **rx,ry** centered at **(x1,y1)** with optional color and thickness.

- ****fellip x1;y1;rx;ry;color**** : draws a filled ellipse with radii **rx,ry** centered at **(x1,y1)** with optional color.

- ****comet x1;y1;type;n;color****: keeps a history of the last ` n` points drawn and renders them in the optional `color`. If ` type` is non-negative, then the last n points are drawn as a line with thickness in pixels of the magnitude of `type`. If `type` is negative, filled circles are drawn with a radius of `-thick` in pixels.

- ****text x1;y1;s****: draws a string **s** at position **(x1,y1)** with the current color and text properties. Only the coordinates can depend on the current values.

- ****vtext x1;y1;s;z****: draws a string **s** followed by the floating point value **z** at position **(x1,y1)** with the current color and text properties. Thus, you can print out the current time or value of a variable at any given time.

- ****xnull x1;y1;x2;y2;color;id****: uses the nullclines that you have already computed in your animation. You can use the static nullclines by just choosing **-1** for the **id** parameter. To use dynamic nullclines, you must compute a range of nullclines using the Nullcline Freeze Range command. The parameter **id** runs from 0 to N where N is the number of nullclines that you have computed in the range dialog. The animator converts **id** to an integer and tests whether it is in the range and then loads the appropriate nullcline. There is an example shown below. The **ynull** command is identical. The parameters **x1,y1,x2,y2** tell the animator the window in which the nullclines are defined. These should be the lower-left and upper right corners of the phaseplane where the nullclines were computed.

*REMARK.* As with lines in ODE files, it is possible to create arrays of commands using the cobination of the **\[i1..i2\]** construction. Some examples are shown below.

## Examples

I will start out with a simple pendulum example and then a bunch of more interesting examples. Here is the old pendulum again:

    # damped pendulum pend.ode
    dx/dt = xp
    dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
    pe=m*g*(1-cos(x))
    ke=.5*m*l*xp^2
    aux P.E.=pe
    aux K.E.=ke
    aux T.E=pe+ke
    x(0)=2
    param m=10,mu=1,g=9.8,l=1
    param scale=0.008333
    @ bounds=1000
    done

I have added an initial condition and another parameter used to scale the magnitude of the kinetic energy for a later animation. Fire up XPP and run this simulation. Now we will create a very simple animation:

    # pend.ani
    # simple animation file for pendulum
    line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
    fcircle .5+.4*sin(x);.5-.4*cos(x);.05;$RED
    end

Notice that comments are allowed. This file is included with the distribution as is the ODE file so you don’t have to type it in. There are only two lines of code. The first tells the animator to draw a line from the center of the screen at **(.5,.5)** to a point **( 5+.4\*sin(x), .5-.4\*cos(x) )**, where **x** is the variable in the ODE file for the pendulum. The line has thickness 3 and is black. The next line of code says to draw a filled circle centered at the same point as the line was with radius **0.05**. This will be colored red. Finally, we tell the interpreter that this is the end of the commands. In case you haven’t already done it, click on (Initialconds) (Go) to solve the ODE. You should see a damped oscillation. Now click on (Viewaxes) (Toon). A new window will appear with the “test pattern” on the screen. In this window click on (File) and type in **pend.ani** at the prompt. XPP will tell you that two lines were loaded successfully. Comments are ignored. Click on (Go) in the animation window. You will see a pendulum appear and rock back and forth. It may be somewhat jerky depending on the server and graphics properties of your computer and graphics. You can stop it by clicking on (Pause), speed it up by clicking on (Fast) and slow it down by clicking on (Slow). The reaction to mouse clicks is terribly slow, so to test animation, I would integrate just for a short time at first until you are satisfied. You can edit the file and reload it with the (File) command.

Now we consider a much more complicated animation file:

    # pend2.ani
    # fancy animation file for pendulum
    PERMANENT
    settext 3;rom;$PURPLE
    text .25;.9;Pendulum
    line 0;.5;1;.5;$BLUE;4
    SPEED 10
    TRANSIENT
    line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
    fcircle .5+.4*sin(x);.5-.4*cos(x);.05;1.-scale*ke
    settext 1;rom;$BLACK
    vtext .05;.1;t=;t
    settext 1;sym;$BLACK
    vtext .05;.05;q=;x
    end

The first noncomment tells the animator that what follows will be on every frame exactly as intially defined. The text is made fairly large and purple in Times-Roman font. The **text** command puts the text a quarter away across the screen near the top and write “Pendulum.” Next a thick blue line is drawn across the middle of the screen to act as a “tether” for the pendulum. We set the delay between frame to be 10 milliseconds with the **SPEED** command. Then all the remaining objects are to be **TRANSIENT.** The first line drawn is the arm of the pendulum. At the end, we place a filled circle, but the color of the circle is proportional to the kinetic energy. **NOTE:** We have used the **fixed variable** version of the kinetic energy, **ke** and not the **auxiliary variable, K.E.** since the latter is not “known” to the internal formula compiler but the former is. Next we set the text color and font stuff to small black roman letters and use the **vtext** command to tell the user the current time. Finally, we set the text to symbol and plot the value of the angle, $`\theta`$ which is the same key as the letter “q.”

Click on the (File) button in the animator. Load the file called **pend2.ani** and run it.

The next example is of a large dynamical system that represents a set of coupled excitable cells. The ODE file is called `wave.ode` and is included in the distribution. Here it is

    # wave.ode
    # pulse wave with diffusional coupling 
    param a=.1,d=.2,eps=.05,gamma=0,i=0
    vv0(0)=1
    vv1(0)=1
    f(v)=-v+heav(v-a)
    #
    vv0'=i+f(vv0)-w0+d*(vv1-vv0)
    vv[1..19]'=i+f(vv[j])-w[j]+d*(vv[j-1]-2*vv[j]+vv[j+1])
    vv20'=i+f(vv20)-w20+d*(vv19-vv20)
    #
    w[0..20]'=eps*(vv[j]-gamma*w[j])
    @ meth=qualrk,dt=.25,total=150,xhi=150
    done

Now we will create an animation file that plots the values of the voltages, **v0, ..., v20** as beads along the vertical axis whose horizontal height is proportional to their voltage and whose color is proportional to the value of the recovery variables, **w0, ..., w20**. Since I have run the simulation, I know that the recovery variables are between 0 and 1 and that the voltages are between -1 and 1. Here is the one-line animation file for this effect. It is called `wave.ani` and is included with the distribution:

    # wave.ani
    # animated wave 
    fcircle .05+.04*[0..20];.5*(vv[j]+1);.02;1-w[j]
    end

This file uses the “array” capabilities of the XPP parser to expand the single line into 21 lines from 0 to 20. I just draw a filled circle at scaled vertical coordinate and horizontal coordinate with a small radius and colored according to the recovery variable. The horizontal coordinate is expanded to be `.05 + .04*0`, `.05 + .04*1`, etc; the vertical is `.5*(vv0+1)`, `.5*(vv1+1)`, etc; and the color is `1-w0`, 1-w1, etc. Thus this is interpreted as a 21 line animation file. Try it to see what it looks like. It is a simple matter to add a vertical scale and time ticker.

This next example illustrates the use of the relative line command. Here, the model is a chain of 20 oscillators representing the phases of spinal motoneurons which control the muscles of the lamprey, an eel-like animal. We will also compute a cumulative “bend” angle which is dependent on the phase of each oscillation. Here is the ODE file, called `lamprey.ode`

    # example of a chain of coupled oscillators
    par grad=0,phi=.1
    h(u)=sin(u+phi)
    par af=1,ar=1
    par bend=.1
    x1'=1+grad+af*h(x2-x1)
    x[2..19]'=1+grad*[j]+ar*h(x[j-1]-x[j])+af*h(x[j+1]-x[j])
    x20'=1+20*grad+ar*h(x9-x10)
    # here is cumulative bend
    an1=bend*sin(x1)
    an[2..20]=bend*sin(x[j])+an[j-1]
    @ bound=1000
    @ total=50
    done

Fire up XPP and run this. (Note that this should be run on a torus phase-space, but since we are only looking at the animation, it is not important. ) Now get the animation window and load in the file ` fish.ani` which looks like:

    # fish.ani
    # lamprey swimmer -- oscchain.ode
    line 0;.5;.04*cos(an1);.5+.04*sin(an1);$BLACK;3
    rline .04*cos(an[2..20]);.04*sin(an[j]);$BLACK;3
    END

Click on (Go) to watch it swim! I draw a series of short line segments relative to the previous one and at an angle that is determined by the **bend** parameter in the ODE file and on the phase of the controlling oscillator. For fun, rerun the simulation with the parameter **grad** set to 0.1.

The penultimate example revisits the Lorenz equations. These equations can be derived by looking at a simple water wheel which consists of a circle of leaky cups with water dripping into them. (See Strogatz for a nice derivation of the equations.) The angle of one of the cups with respect to the viewer is found by integrating the angular velocity which is proportional to the **x** variable in the equations. Thus, as a final example, I present an animation with 8 cups of the Lorenz water wheel. I have created another ODE file that has additional info that I will use called `lorenz2.ode`. Here it is:

    # the famous Lorenz equation set up for animated waterwheel and
    # some delayed coordinates as well
    init x=-7.5  y=-3.6  z=30
    par r=27  s=10  b=2.66666
    par c=.2  del=.1
    x'=s*(-x+y)
    y'=r*x-y-x*z
    z'=-b*z+x*y
    # x is proportional to the angular velocity so integral is angle
    theta'=c*x
    th[0..7]=theta+2*pi*[j]/8
    # approximate the velocity vector in the butterfly coords
    z1=z-del*(-b*z+x*y)
    x1=x-del*(s*(-x+y))
    @ dt=.025, total=40, xplot=x,yplot=y,zplot=z,axes=3d
    @ xmin=-20,xmax=20,ymin=-30,ymax=30,zmin=0,zmax=50
    @ xlo=-1.5,ylo=-2,xhi=1.5,yhi=2,bound=10000
    done

Fire it up with XPP and integrate it. Then load the animation file called `lorenz.ani` and run it. Here is the file:

    # shows the waterwheel using the integrated ang velocity
    # see Strogatz book.  Use lorenz2.ode
    PERMANENT 
    circ .515;.515;.46;$BLACK;2
    TRANSIENT
    SPEED 20
    frect .5+.45*sin(th[0..7]);.5+.45*cos(th[j]);.55+.45*sin(th[j]);.55+.45*cos(th[j]);$BLACK
    # plotting the butterfly and a lagged version of it !!
    fcirc .5+x/40;z/50;.02;$GREEN
    fcirc .5+x1/40;z1/50;.02;$RED
    end

The waterwheel can be seen when running the animation. In the center of the screen are two colored dots that represent the $`(x,z)`$ coordinates (green) of the attractor and the approximate velocity of these two variables in red.

The last example shows the use of dynamic nullclines. Here is the ODE file:

    init u=.0426,v=.0843
    u'=-u+f(aee*u-aie*v-te+stim(t))
    v'=(-v+f(aei*u-aii*v-ti))/tau
    par aee=15,aie=9,te=3
    par aei=20,aii=3,ti=3,tau=5
    stim(t)=s0+s1*if(t<tdone)then(t/tdone)else(0)
    par s0=0,tdone=5,s1=1.2
    f(u)=1/(1+exp(-u))
    @ xp=u,yp=v,xlo=-.1,ylo=-.1,xhi=1.1,yhi=1.1,total=50
    done

and here is the animation file

    xnull -.1;-.1;1.1;1.1;$RED;10*stim(t)
    ynull -.1;-.1;1.1;1.1;$GREEN;10*stim(t)
    fcircle (u+.1)/1.2;(v+.1)/1.2;.025;$BLACK
    end

The stimulus goes from 0 to 1.2. Since nullclines are computed with $`t=0`$, the stimulus is essentially zero as far as XPP is concerned when the nullclines are computed. However, I have added a dummy parameter `s0` which we will vary between 0 and 1.2 to get a family of nullclines. Thus, click on Nullcline Freeze Range and use `s0` as the range parameter, 12 steps, with a Low value of 0 and a High value of 1.2. You will see 13 nullclines drawn. Think of them as nullclines $`0,1,2,\ldots,12`$. They correspond to `s0`=$`0,
0.1,\ldots, 1.2.`$ Integrate the ODEs. Click on Viewaxes Toon and load the above animation file. Click on Go and watch it animate the nullclines as well as show the solution. The first 4 terms in the nullcline directive scale the nullclines to fit on the animator. They are the same values as the phaseplane window. The next term is just the color. The final **id** parameter is coded as 10 times the stimulus. Thus, when the stimulus is at .85, this evaluates to 8.5 and XPP truncates this to the integer, 8, and draws nullcline “8” which is the nullcline for a stimulus of strength 0.8. If you draw enough of them, then it wont appear to jump much.

## Saving a movie

X11 XPP's MPEG button wrote a `.ppm` file per frame and left you to run
the external `mpeg_encode` program on them yourself (a workflow this
section used to walk through step by step, disk space and all). **In
web2** there is no PPM-writing step and no external encoder: capture
frames with the kinescope (they are data — series, marks and the
viewport — not pixels) and export them as an animated GIF or PNG frames
from the page itself (docs/ui-v2.md T15). See "Saving pictures and files"
in [Using the interface](04-using-the-interface.md).
