# Creating C-files for faster simulations

## Dynamically linked libraries

If your OS supports dynamically linked libraries, then it is easy to hook a right-hand side defined in C or even partially defined in C. You will first have to edit the Makefile so that it will use a dynamically linked library. I do not distribute it with this option turned on since it is seldom used. In the `CFLAGS` definition, add `-DHAVEDLL` and in the `LIBS` definition, add `-ldl`. Recompile XPP with these options.

Now you will be able to load libraries that contain one or more sets of right-hand sides. I will now present an elementary example. Consider the following ODE file `tstdll.ode`

    # test of dll
    x'=xp
    y'=yp
    xp=0
    yp=0
    export {x,y,a,b,c,d,t} {xp,yp}
    par a=1,b=1,c=1,d=1
    done

The code beginning with `export` tells XPP we will call an external function routine, passing the 7 items `x,y,a,b,c,d,t` and returning the two items `xp,yp`. Note that they are fixed variables and set to zero at initialization, but XPP will override this when it is run if a library is loaded. They are just dummy place holders for the true right-hand sides. Now lets define the right-hand sides.

Here is the C-file called `funexample.c`

    #include <math.h>
    /*  
     some example functions
    */

    lv(double *in,double *out,int nin,int nout,double *v,double *cn)
    {
      double x=in[0],y=in[1];
      double a=in[2],b=in[3],c=in[4],d=in[5];
       double t=in[6];
      out[0]=a*x*(b-y);
      out[1]=c*y*(-d+x);
    }

    vdp(double *in,double *out,int nin,int nout,double *v,double *cn)
    {
      double x=in[0],y=in[1];
      double a=in[2],b=in[3],c=in[4],d=in[5];
       double t=in[6];
      out[0]=y;
      out[1]=-x+a*y*(1-x*x);
    }

    duff(double *in,double *out,int nin,int nout,double *v,double *cn)
    {
      double x=in[0],y=in[1];
      double a=in[2],b=in[3],c=in[4],d=in[5];
     double t=in[6];
      out[0]=y;
      out[1]=x*(1-x*x)+a*sin(b*t)-c*y;
    }

This defines 3 different models, the Lotka-Volterra equation, the van der Pol equation, and the Duffing equation. Note that in addition to the parameters and variables that are passed by the user, XPP also passes all the variable and parameter information. The order is generally as follows. The array `v` contains `t`, the variables in the order they are defined followed by the fixed variables in the order defined. The array `cn` contains the parameters define in your model. `cn[2]` contains the first parameter and the others are defined in the order created. Thus, for the ode file `tstdll.ode`, we have the following identifications:

    cn[2]=a,cn[3]=b,cn[4]=c,cn[5]=d
    v[0]=t,v[1]=x,v[2]=y,v[3]=xp,v[4]=yp

Edit and save the C file and type `make -f Makefile.lib` which will compile the module and then create a shared library. Run XPP using the ode file `tstdll.ode`. Now click on `File` `Edit` and choose the load dynamic library option. For the library name, use the full path, unless you have put the library in `usr/lib`. Or better yet, before you run XPP, type `export LD_LIBRARY_PATH=.` or ` setenv LD_LIBRARY_PATH=.`, depending on your shell and then XPP will look in the current directory for the library. Then for the function, choose one of `lv, vdp, duff` which are the three right-hand sides define above. The main advantage of dynamically linked libraries is that if you have a right-hand side that is three pages of computer output, then you can still use XPP with no problems.

## Completely defining the right-hand sides in C

This is probably not something you want to do very often. It is better to use the above approach. This method forces XPP to be a stand-alone problem for a single ODE. For very complicated models such as discretizations of a PDE you may want to compile the right-hand sides and run a dedicated program for that particular model. This is not as hard as it seems. You must first create a library, which is done by typing `make lib` after you have successfully compiled all of XPP: this builds `build/obj/libxppcore.a` (upstream's `make xpplib`/`libxpp.a`, before the numerics library and the X11 front end were split apart; there is no `-lX11` to link against any more, since xppautX has no X11 front end, so a program linked this way gets the headless core only — no browser page, no `-silent` batch entry point of its own). You only need to make this library once. You can then move it to where you usually keep libraries if you want. Now all you do is create a C file for the right-hand sides (I will show you below), say, it is called `lorenzrhs.c` and then type:

    gcc lorenzrhs.c -o lorenz build/obj/libxppcore.a -lm

and if all goes well, you will have an executable called “lorenz”. If the ODE file was called “lorenz.ode” then run this as follows:

    lorenz lorenz.ode

as XPP needs the information contained in the ODE file to tell it the names of variables and parameters, etc.

Since the names of the parameters are interpreted by XPP as references to a certain array, you must communicate their values to the C functions for your right-hand sides. XPP provides a little utility for this. In the (Files) submenu. click on (c-Hints) and a bunch of defines will be pronted to the console. You can use these in your program. All of your c-files must have the following skeleton:

    #include <math.h>

    extern double constants[]; 
    main(argc,argv)
     char **argv; 
     int argc;
    {
     do_main(argc,argv);
     }



    my_rhs(t,y,ydot,neq)
     double t,*y,*ydot; 
     int neq;
    {

    }

    extra(y,t,nod,neq)
     double t,*y; 
     int nod,neq;
    {

    }

In addition, you must define the parameters for the problem. Clicking on the (c-Hints) will essentially write this skeleton along with the defines for the problem. Lets take the lorenz equation as an example. Here is the ODE file:

    # the famous Lorenz equation set up for 3d view
    init x=-7.5  y=-3.6  z=30
    par r=27  s=10  b=2.66666  
    x'=s*(-x+y)
    y'=r*x-y-x*z
    z'=-b*z+x*y
    @ dt=.025, total=40, xplot=x,yplot=y,zplot=z,axes=3d
    @ xmin=-20,xmax=20,ymin=-30,ymax=30,zmin=0,zmax=50
    @ xlo=-1.5,ylo=-2,xhi=1.5,yhi=2
    done

Run XPP with this file as the input file and click on the (File) (c-Hints) and the following will be written to the console:

    #include <math.h>

     extern double constants[]; 
    main(argc,argv)
     char **argv; 
     int argc;
    {
     do_main(argc,argv);
     }
    /* defines for lorenz.ode  */ 
    #define r constants[2]
    #define s constants[3]
    #define b constants[4]
    #define X y[0]
    #define XDOT ydot[0]
    #define Y y[1]
    #define YDOT ydot[1]
    #define Z y[2]
    #define ZDOT ydot[2]
    my_rhs(t,y,ydot,neq)
     double t,*y,*ydot; 
     int neq;
    {
      }
    extra(y,t,nod,neq)
     double t,*y; 
     int nod,neq;
    {
      }

You just fill in the blanks as follows to produce the required C file:

    #include <math.h>

    extern double constants[];

    main(argc,argv)
         char **argv;
         int argc;
    {
      do_main(argc,argv);
    }

    /* defines for lorenz.ode  */ 
    #define r constants[2]
    #define s constants[3]
    #define b constants[4]
    #define X y[0]
    #define XDOT ydot[0]
    #define Y y[1]
    #define YDOT ydot[1]
    #define Z y[2]
    #define ZDOT ydot[2]

    extra(y, t,nod,neq)
         double *y,t;
         int nod,neq;
    {
     return; 
    }

    my_rhs( t,y,ydot,neq)
     double t,*y,*ydot;
     int neq;
    {
     XDOT=s*(-X+Y);
    YDOT=r*X-Y-X*Z;
    ZDOT=-b*Z+X*Y;

    }

Now just compile this and link it with the XPP library and run it with “lorenz.ode” as the input and it will be a dedicated solver of the lorenz equations.

Here is a final example that shows you how to include user-defined functions and auxiliary variables. Here is the ODE file:

    # The Morris-Lecar model as in our chapter in Koch & Segev
    #  A simple membrane oscillator.  
    #
    params v1=-.01,v2=0.15,v3=0.1,v4=0.145,gca=1.33,phi=.333
    params vk=-.7,vl=-.5,iapp=.08,gk=2.0,gl=.5,om=1
    minf(v)=.5*(1+tanh((v-v1)/v2))
    ninf(v)=.5*(1+tanh((v-v3)/v4))
    lamn(v)= phi*cosh((v-v3)/(2*v4))
    ica=gca*minf(v)*(v-1)
    v'=  (iapp+gl*(vl-v)+gk*w*(vk-v)-ica)*om
    w'= (lamn(v)*(ninf(v)-w))*om
    aux I_ca=ica
    b v-v'
    b w-w'
    @ TOTAL=30,DT=.05,xlo=-.6,xhi=.5,ylo=-.25,yhi=.75
    @ xplot=v,yplot=w
    set vvst {xplot=t,yplot=v,xlo=0,xhi=100,ylo=-.6,yhi=.5,total=100 \
        dt=.5,meth=qualrk}
    done

and here is the C-file you need:

    #include <math.h>

    extern double constants[];

    main(argc,argv)
         char **argv;
         int argc;
    {
      do_main(argc,argv);
    }


    /* defines for lecar.ode  */ 
    #define v1 constants[2]
    #define v2 constants[3]
    #define v3 constants[4]
    #define v4 constants[5]
    #define gca constants[6]
    #define phi constants[7]
    #define vk constants[8]
    #define vl constants[9]
    #define iapp constants[10]
    #define gk constants[11]
    #define gl constants[12]
    #define om constants[13]
    #define V y[0]
    #define VDOT ydot[0]
    #define W y[1]
    #define WDOT ydot[1]
    #define I_CA y[2]



    double minf(v)
         double v;
    {
      return .5*(1+tanh((v-v1)/v2));
    }

    double ninf(v)
         double v;
    {
      return .5*(1+tanh((v-v3)/v4));
    }

    double lamn(v)
         double v;
    {
      return phi*cosh((v-v3)/(2*v4));
    }


    extra(y, t,nod,neq)
         double *y,t;
         int nod,neq;
    {
      I_CA=gca*minf(V)*(V-1.0);
    }

    my_rhs( t,y,ydot,neq)
     double t,*y,*ydot;
     int neq;
    {
    VDOT=om*(iapp+gl*(vl-V)+gk*W*(vk-V)+gca*minf(V)*(1-V));
    WDOT=om*lamn(V)*(ninf(V)-W);
    }

Note that the user-defined functions appear as C functions and the auxiliary variables are almost like regular ones and are defined in the “extra” function.
