# C Files

For some types of problems that have lengthy and complicated right-hand sides, you would like to use directly compiled C code rather than using the built in interpreter. I have found that one generally does not get as much speed up as merits the extra work of recompilation for each problem. Nevertheless, I have included an option for doing that in XPP. If you want to do this for a particular ODE file, simply append the call to XPP with the option `-m` which means to make a C file. XPP goes on as before but when you exit the program, there will be two files called `my_cfile.c` and ` my_hfile.h.` These files should help you in your creation of your own file. Basically, you should replace the call to `my_rhs()` in `my_rhs.c` with the contents of `my_cfile.c.` At this time, this is incompatible with functional equations, so dont use it if you are solving them. Nor is it compatible with tabulated stuff.

For example, the Morris-Lecar model

    2
    variables v,w
    fixed m
    params v1=-.01,v2=0.15,v3=0.1,v4=0.145,gna=1.33,phi=.333
    params vk=-.7,vl=-.5,iapp=.075,gk=2.0,gl=.5,om=1
    user minf 1 .5*(1+tanh((arg1-v1)/v2))
    user ninf 1 .5*(1+tanh((arg1-v3)/v4))
    user lamn 1 phi*cosh((arg1-v3)/(2*v4))
    odev  (iapp+gl*(vl-v)+gk*w*(vk-v)+gna*m*(1-v))*om
    odew (lamn(v)*(ninf(v)-w))*om
    odem minf(v)
    b v-v'
    b w-w'
    done

produces the header file:


    #define v y__y[0]
    #define w y__y[1]
    double m;
    #define v1 constants[1]
    #define v2 constants[2]
    #define v3 constants[3]
    #define v4 constants[4]
    #define gna constants[5]
    #define phi constants[6]
    #define vk constants[7]
    #define vl constants[8]
    #define iapp constants[9]
    #define gk constants[10]
    #define gl constants[11]
    #define om constants[12]
    double minf();
    double ninf();
    double lamn();

and the C file


    #include "my_hfile.h"
    extern double constants[];
    double minf(arg1)
    double arg1;
    { 
     return(.5*(1+tanh((arg1-v1)/v2))
    );
    }

    double ninf(arg1)
    double arg1;
    { 
     return(.5*(1+tanh((arg1-v3)/v4))
    );
    }

    double lamn(arg1)
    double arg1;
    { 
     return(phi*cosh((arg1-v3)/(2*v4))
    );
    }

    my_rhs(t,y__y,ydot,neq)
     double t,*y__y,*ydot;
    int neq;
     { 

     do_fix();
    ydot[0]= (iapp+gl*(vl-v)+gk*w*(vk-v)+gna*m*(1-v))*om;
    ydot[1]=(lamn(v)*(ninf(v)-w))*om;

    }
    do_fix()
    {
    m=minf(v);

    }

Copy the file, `my_rhs.c` to some backup. Delete the function called `my_rhs(...)` and substitute the contents of `my_cfile.c` putting the top declarations of ` my_cfile.c` at the beginning of `my_rhs.c`. Then, remake XPP. Invoke it as before by typing `xpp` followed by the file name.

## Warnings

This tool is only a guide to the production of C code. There are several things to watch out for:

1.  C is case sensitive and XPP is not so make sure you are consistent.

2.  If your parameters are variables are the same as any of the local variables or key words in the C file `my_rhs.c` then there will be compilation errors. For example, don’t use `i.` When in doubt, capitalize all your parameters and variables in the ODE file keeping the system functions such as `exp` in lower case.

3.  Some XPP functions like `heav` and `max` are not known to C so you must define them. I will probably fix this later.

4.  The expression `y**x` or `yx̂` makes no sense in C and should be replaced by `pow(y,x)`.

## Improved ODE parser

The latest version of XPP/XPPAUT (1.6 and above) uses a somewhat different syntax for ODE files. No longer do you have to worry about the order in which things are described, nor do you have to worry about some names being defined before others. The right-hand sides, initial data, functions, etc are now much easier to input (in the sense that they are written almost as you would in a paper.) Many of the same commands used in the old style format remain unchanged, but the handling of fixed, auxiliary, and right-hand sides is much more intuitive. Finally, standard line continuation is allowed with the usual UNIX character, $`\backslash`$. As this new stuff has not been tested thoroughly, let me know if you encounter problems. Also, the old style format still works so when in doubt use it.

The newstyle format for the parser has the following format:

The big difference is that the name of the variable, auxiliary quantity, fixed quantity, and Markov variable are kept with their right-hand sides. Thus there is no `o,i,r` stuff necessary anymore and the `fixed,variable,kernel` declarations are gone. Auxiliary variables always have user defined names and functions are put in as you would write them. (No longer is it necessary to use ` arg1,arg2, ` etc as the names of the arguments.) For variables that will satisfy differential equations, maps, or integral equations, you write the equation in the most obvious fashion. The order of statements is unimportant (except for fixed variables which are evaluated in the order they are defined). Finally the standard UNIX line continuation character $`\backslash`$ is recognized so you can put statements on multiple lines. The exception to this is the Markov transition matrix. Each row of the matrix must be put on the same line.
