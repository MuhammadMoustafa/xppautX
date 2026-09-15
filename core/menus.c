/* Menu labels and hint strings. Generated from upstream menus.h; the
   MENUDEF widget struct lives in menus.h. */
#include "menus.h"
#include "menudrive.h"



#define MAIN_MENU 0
#define FILE_MENU 1
#define NUM_MENU 2
#define MAIN_ENTRIES 20
/* CLONE */
#define FILE_ENTRIES 16
#define NUM_ENTRIES 18
char *main_menu[]={
 "XPP","Initialconds","Continue","Nullcline",
 "Dir.field/flow","Window/zoom","phAsespace",
 "Kinescope","Graphic stuff","nUmerics","File",
 "Parameters","Erase","Makewindow","Text,etc",
 "Sing pts","Viewaxes","Xi vs t","Restore","3d-params",
 "Bndryval"};

char *num_menu[]={"NUMERICS","Total","Start time","tRansient",
"Dt","Ncline ctrl","sIng pt ctrl","nOutput","Bounds","Method",
"dElay","Color code","stocHast","Poincare map","rUelle plot",
"looKup","bndVal","Averaging","[Esc]-exit"};
/* CLONE change */  

char *fileon_menu[]={
"FILE","Prt src","Write set","Read set",
"Auto","Calculator","Edit","Save info",
"Bell off","Help","Quit","Transpose","tIps","Get par set","cLone",".Xpprc","tUtorial"}; 

char *fileoff_menu[]={
"FILE","Prt src","Write set","Read set",
"Auto","Calculator","Edit","Save info",
"Bell on","Help","Quit","Transpose","tIps","Get par set","cLone",".Xpprc","tUtorial"}; 

/* end CLONE change */
/* hints for the main menus */
char *main_hint[]=
{ "Integrate the equations",
  "Continue integration for specified time",
  "Draw nullclines",
  "Direction fields and flows of the phaseplane",
  "Change the size of two-dimensional view",
  "Set up periodic/torus phase space",
  "Take snapshots of the screen",
  "Adding graphs,hard copy, etc",
  "Numerics options",
  "Quit, save stuff, etc",
  "Change problem parameters",
  "Clear screen",
  "Create other windows",
  "Add fancy text and lines,arrows",
  "Find fixed points and stability",
  "Change 2 or 3d views",
  "Plot variable vs time",
  "Redraw the graph ",
  "Set parameters for 3D view",
  "Run boundary value solver" };


char *file_hint[]={
"Display source and active comments",
"Save information for restart",
"Read information for restart",
"Run AUTO, the bifurcation package",
"A little calculator -- press ESC to exit",
"Edit right-hand sides or functions or auxiliaries",
"Save info about simulation in human readable format",
"Turn bell on/off",
"Browser help",
"Duh!",
"Transpose storage",
"Turn off these silly tips",
"Set predefined parameters",
"Clone the ode file",
"Edit your .xpprc preferences file",
"Run a quick tutorial on XPPAUT"
};


char *num_hint[]={
"Total time to integrate eqns",
"Starting time -- T0",
"Time to integrate before storing",
"Time step to use",
"Mesh for nullclines",
"Numerical parameters for fixed points",
"Number of steps per plotted point",
"Maximum allowed size of any variable",
"Integration method",
"Maximum delay and delay related stuff",
"Color trajectories according to velocity,etc",
"Curve fitting, FFT, mean, variance, seed, etc",
"Define Poincare map parameters",
"Define shifted plots",
"Modify lookup tables",
"Numerical setup for boundary value solver",
"Compute adjoint and averaged functions",
"Return to main menu"
};

/* other hints  */

char *null_hint[]={
"Compute new nullclines",
"Redraw last nullclines",
"Set automatic redraw -- X redraws when needed",
"Only redraw when asked (Redraw)",
"Freeze multiple nullclines",
"Save nullcline values to a file"
};

char *null_freeze[]={
  "Freeze current clines",
  "Delete all frozen clines",
  "Range freeze a bunch of clines",
  "Animate nullclines"
};
char *ic_hint[]={
"Integrate over a range of parameters, init data, etc",
"Integrate over range of 2 parameters,init data, etc",
"Pick up from last step of previous solution",
"Use current initial data",
"Use current initial data",
"Specify initial data with mouse",
"Pick up from last step and shift T to latest time",
"Input new initial data",
"Use the initial data from last shooting from fixed pt",
"Read init data from file",
"Type in function of 't' for t^th equation",
"Repeated ICs using mouse ","Guess new values for DAE",
"Integrate backwards"
};

char *wind_hint[]={
"Manually choose 2D view",
"Zoom into with mouse",
"Zoom out with mouse",
"Let XPP automatically choose window",
"Reset to default view",
"Scroll around the view"
};

char *flow_hint[]={
"Draw vector field for 2D section",
"Draw regular series of trajectories",
" ",
"Color the PP on a grid",
"Draw only directions"
};

char *phas_hint[]={
"Each variable is on a circle",
"No variable on circle",
"Choose circle variables"
};

char *kin_hint[]={
"Grab a screen shot",
"Clear all screen shots",
"Manually cycle thru screenshots",
"Continuously cycle through screen shots",
"Dump the screen shots to disk",
"Make animated gif file from screenshots"
};

char *graf_hint[]={
"Add another curve to the current plot",
"Delete last added plot",
"Remove all the added plots except the main one",
"Edit parameters for a plot",
"Create a postscript file of current plot",
"Create a styleable svg file of current plot",
"Options for permanently saving curve",
"Axes label sizes and zero axes for postscript",
"Export the numbers used in the graphs on the screen",
"Change colormap"
};

char *cmap_hint[]={
 " blue-green-red",
 "red-...-violet-red",
 "black-red-yellow-white",
 "pale cyan->pale yellow",
 "blue-violet-red",
 "black-white",
 "helical luminence corrected"
};
char *frz_hint[]={
"Permanently keep main curve -- even after reintegrating",
"Delete specified frozen curve",
"Edit specified frozen curve",
"Remove all frozen curves",
"Toggle key on/off",
"Import bifurcation data",
"Clear imported bifurcation curve",
"Automatically freeze after each integration",
};

char *stoch_hint[]={
"Seed random number generator",
"Run many simulations to get average trajectory",
"Get data from last simulation",
"Load average trajectory from many runs",
"Load variance of from many runs",
"Compute histogram",
"Reload last histogram",
"Fourier series of trajectory",
"Power spectrum/phase",
"Fit data from file to trajectory",
"Get mean/variance of single trajectory",
"Compute maximal Liapunov exponent",
"Compute spike-time autocorrel",
"Compute correlations - subtracting mean",
"Compute windowed spectral density",
"Compute two-variable histograms"
};

char *bvp_hint[]={
"Solve BVP over range of parameters",
"Don't show any but final step",
"Show each step of iteration",
"Solve BVP with periodic conditions",
"Set up special homoclinic stuff"
}; 

char *adj_hint[]={
"Compute a new adjoint function",
"Compute averaging interaction function",
"Load computed adjoint",
"Load computed orbit",
"Load computed interaction function",
"Adjoint numerical parameters",
"Range over stuff to computte many adjoints"
};

char *map_hint[]={
"Turn off Poincare map",
"Define section for Poincare map",
"Compute Poincare map on maximum/minimum of variable",
"Compute map based on period between events",
};


char *view_hint[]={
  "Two-dimensional view settings",
  "Three-dimensional view settings",
  "Plot array ",
  "Animation window"
};

char *half_hint[]={
"Create new window",
"Delete all but main window",
"Delete last window",
"Place current window at bottom",
"Automatically redraw",
"Redraw only when requested",
"Plot all graphs simultaneously -- slows you down"};

char *text_hint[]={
"Create text labels in different fonts ",
"Add arrows to trajectories",
"Create lines with arrowheads",
"Add squares, circles, etc to plot",
"Change properties, delete, or move existing add-on",
"Delete all text, markers, arrows",
"Create many markers based on browser data"
};

char *edit_hint[]={
"Move the selected item",
"Change properties of selected item",
"Delete selected item"
};

char *sing_hint[]={
"Find fixed points over range of parameter",
" ",
"Use mouse to guess fixed point",
"Monte carlo search for fixed points"};

char *meth_hint[]={
"Discrete time -- difference equations",
"Euler method",
"Heun method -- 2nd order Euler",
"4th order Runge-Kutta",
"4th order predictor-corrector",
"Gear's method -- for stiff systems",
"Integrator for Volterra equations",
"Implicit Euler scheme",
"4th order Runge-Kutta with adaptive steps <- RECOMMENDED",
"Another stiff method if Gear fails",
"Stiff - bad for discontinuous systems",
"Dormand-Prince5",
"Dormand-Prince8(3)",
"Rosenbrock(2,3) - good with discontinuties",
"Symplectic - x''=F(x)"};

char *color_hint[]={
" ",
"Color according to magnitude of derivative",
"Color according to height of Z-axis"
};

char *tab_hint[]={"Edit the lookup tables","View a table in the data browser"};

char *edrh_hint[]={
"Edit right-hand sides and auxiliaries",
"Edit function definitions",
"Save current file with new defs",
"Load external C right-hand sides"
};

char *auto_hint[]={
"Tell AUTO the parameters you may vary",
"What will be plotted on the axes and what parameter(s)",
"Tell AUTO range, direction, and tolerance",
"Run the continuation",
"Grab a point to continue from",
"Tell AUTO to save at values of period or parameter",
"Erase the screen",
"Redraw the diagram",
"Save and output options"};

char *no_hint[]={ 
" "," "," "," "," "," "," "," "," "," ", " "," "," "," "};

char *aaxes_hint[]={
"Plot maximum of variable vs parameter",
"Plot norm of solution vs parameter",
"Plot max/min of variable vs parameter",
"Plot period of orbit vs parameter",
"Set up two-parameter bifurcation",
"Zoom in with mouse",
"Zoom out with mouse",
"Recall last 1 parameter plot",
"Recall last 2 parameter plot",
"Let XPP determine the bounds of the plot",
"Plot frequency vs parameter",
"Plot average of orbit vs parameter",
"Return to default bounds of the plot",
"Scroll around the plot"
};

char *afile_hint[]={
"Load a computed orbit into XPP",
"Write diagram info to file for reuse",
"Load previously saved file for restart",
"Create postscript file of picture",
"Create SVG file of picture",
"Delete all points of diagram and associated files",
"Clear grab point to allow start from new point",
"Write the x-y values of the current diagram to file",
"Write all the info for the whole diagram!",
"Save initial data for whole diagram",
"Toggle automatic redraw",
"Range over a marked branch",
"Select a point in 2 parameter diagram",
"Draw orbits of labeled points automatically",
"Put all data from branch into browser",
};

char *aspecial_hint[]={
"Bifurcation or branch point",
"Endpoint of a branch",
"Hopf bifurcation point",
"Limit point or turning point of a branch",
"Failure to converge",
"Period doubling bifurcation",
"Torus bifurcation from a periodic",
"User defined function",
};

char *arun_hint[]={
  "Start at fixed point",
  "Start at periodic orbit",
  "Start at solution to boundary value problem",
  "Start at homoclinics",
  "Start at a heteroclinic",
}; 

char *browse_hint[]={
 "Find closest data point to given value",
 "Scroll up",
 "Scroll down",
 "Scroll up a page",
 "Scroll down a page",
 "Scroll left",
 "Scroll right",
 "First plotted point",
 "Last plotted point",
 "Mark first point for plotting",
 "Mark last point for plotting",
 "Redraw data",
 "Write data to ascii file",
 "Load first line of BROWSER to initial data",
 "Replace column by formula",
 "Unreplace last replacement",
 "Write a column of data in tabular format",
 "Load data from a file into BROWSER",
 " ",
 "Add a new column to BROWSER",
 "Delete a column from BROWSER"
};

/* keys of the main-window menus; the numerics menu ends with Esc */
char main_menu_keys[]="icndwakgufpemtsvxr3b";
char num_menu_keys[]="tsrdniobmechpukva\033";
char file_menu_keys[]="pwracesbhqtiglxu";

/* pop-up menus, formerly static arrays inside the menudrive.c handlers */
static char *ic_items[]={"(R)ange","(2)par range","(L)ast","(O)ld","(G)o",
  "(M)ouse","(S)hift","(N)ew","s(H)oot","(F)ile","form(U)la","m(I)ce",
  "DAE guess","(B)ackward"};
static char *null_items[]={"(N)ew","(R)estore","(A)uto","(M)anual",
  "(F)reeze","(S)ave"};
static char *frzcline_items[]={"(F)reeze","(D)elete all","(R)ange","(A)nimate"};
static char *dfield_items[]={"(D)irect Field","(F)low","(N)o dir. fld.",
  "(C)olorize","(S)caled Dir.Fld"};
static char *window_items[]={"(W)indow","(Z)oom In","Zoom (O)ut","(F)it",
  "(D)efault","(S)croll"};
static char *torus_items[]={"(A)ll","(N)one","(C)hoose"};
/* (X)tra is defined but not shown: the menu has 6 entries */
static char *kin_items[]={"(C)apture","(R)eset","(P)layback","(A)utoplay",
  "(S)ave","(M)ake AniGif","(X)tra"};
static char *curve_items[]={"(A)dd curve","(D)elete last","(R)emove all",
  "(E)dit curve","(P)ostscript","S(V)G","(F)reeze","a(X)es opts",
  "exp(O)rt data","(C)olormap"};
static char *freeze_items[]={"(F)reeze","(D)elete","(E)dit","(R)emove all",
  "(K)ey","(B)if.Diag","(C)lr. BD","(O)n freeze"};
static char *freeze_off_items[]={"(F)reeze","(D)elete","(E)dit","(R)emove all",
  "(K)ey","(B)if.Diag","(C)lr. BD","(O)ff freeze"};
static char *key_items[]={"(N)o key","(K)ey"};
static char *cmap_items[]={"(N)ormal","(P)eriodic","(H)ot","(C)ool",
  "(B)lue-red","(G)ray","c(U)behelix"};
static char *windows_items[]={"(C)reate","(K)ill all","(D)estroy","(B)ottom",
  "(A)uto","(M)anual","(S)imPlot On"};
static char *windows_simoff_items[]={"(C)reate","(K)ill all","(D)estroy",
  "(B)ottom","(A)uto","(M)anual","(S)imPlot Off"};
static char *text_items[]={"(T)ext","(A)rrow","(P)ointer","(M)arker",
  "(E)dit","(D)elete all","marker(S)"};
static char *text_edit_items[]={"(M)ove","(C)hange","(D)elete"};
static char *sing_items[]={"(G)o","(M)ouse","(R)ange","monte(C)ar"};
static char *view_items[]={"2D","3D","Array","Toon"};
static char *bvp_items[]={"(R)ange","(N)o show","(S)how","(P)eriodic"};
static char *stoch_items[]={"New seed","Compute","Data","Mean","Variance",
  "Histogram","Old hist","Fourier","Power","fIt data","Stat","Liapunov",
  "stAutocor","Xcorrel etc","spEc.dns","2D-hist"};
static char *map_items[]={"(N)one","(S)ection","(M)ax/min","(P)eriod"};
static char *color_items[]={"(N)o color","(V)elocity","(A)nother quantity"};
static char *adj_items[]={"(N)ew adj","(M)ake H","(A)djoint","(O)rbit",
  "(H)fun","(P)arameters","(R)ange"};
static char *tab_items[]={"(E)dit","(V)iew"};
static char *meth_items[]={"(D)iscrete","(E)uler","(M)od. Euler",
  "(R)unge-Kutta","(A)dams","(G)ear","(V)olterra","(B)ackEul",
  "(Q)ualst.RK4","(S)tiff","(C)Vode","DoPri(5)","DoPri(8)3",
  "Rosen(2)3","sYmplectic"};
static char *edrh_items[]={"RHS's","Functions","Save as","Load DLL"};

/*                                 name  title  n  items  keys  hints  first_cmd  width  row */
const XppMenu menu_integrate={"integrate","Integrate",14,ic_items,"r2logmsnhfuidb",ic_hint,M_IR,13,3};
const XppMenu menu_nullclines={"nullclines","Nullclines",6,null_items,"nramfs",null_hint,M_NN,10,6};
const XppMenu menu_freeze_cline={"freeze_cline","Freeze cline",4,frzcline_items,"fdra",null_freeze,M_NFF,10,6};
const XppMenu menu_dirfield={"dirfield","Two-D Fun",5,dfield_items,"dfncs",flow_hint,M_DD,18,6};
const XppMenu menu_window={"window","Window",6,window_items,"wzofds",wind_hint,M_WW,13,13};
const XppMenu menu_torus={"torus","Torus",3,torus_items,"anc",phas_hint,M_AA,9,4};
const XppMenu menu_kinescope={"kinescope","Kinescope",6,kin_items,"crpasmx",kin_hint,M_KC,11,8};
/* (F)reeze and (C)olormap open the next two menus instead of a command */
const XppMenu menu_curves={"curves","Curves",10,curve_items,"adrepvfxoc",graf_hint,M_GA,15,8};
const XppMenu menu_freeze={"freeze","Freeze",8,freeze_items,"fderkbco",frz_hint,M_GFF,15,8};
const XppMenu menu_freeze_off={"freeze_off","Freeze",8,freeze_off_items,"fderkbco",frz_hint,M_GFF,15,8};
const XppMenu menu_freeze_key={"freeze_key","Key",2,key_items,"nk",no_hint,M_GFKN,9,8};
const XppMenu menu_colormap={"colormap","Colormap",7,cmap_items,"nphcbgu",cmap_hint,M_GCN,15,8};
const XppMenu menu_windows={"windows","Make window",7,windows_items,"ckdbams",half_hint,M_MC,11,14};
const XppMenu menu_windows_simoff={"windows_simoff","Make window",7,windows_simoff_items,"ckdbams",half_hint,M_MC,11,14};
/* (E)dit opens menu_text_edit */
const XppMenu menu_text={"text","Text,etc",7,text_items,"tapmeds",text_hint,M_TT,10,10};
const XppMenu menu_text_edit={"text_edit","Edit",3,text_edit_items,"mcd",edit_hint,M_TEM,9,10};
const XppMenu menu_equilibria={"equilibria","Equilibria",4,sing_items,"gmrc",sing_hint,M_SG,12,6};
const XppMenu menu_view={"view","Axes",4,view_items,"23at",view_hint,M_V2,5,13};
const XppMenu menu_bvp={"bvp","Bndry Value Prob",4,bvp_items,"rnsp",bvp_hint,M_BR,16,6};
const XppMenu menu_stochastic={"stochastic","Stochastic",16,stoch_items,"ncdmvhofpislaxe2",stoch_hint,M_UHN,10,2};
const XppMenu menu_poincare={"poincare","Poincare map",4,map_items,"nsmp",map_hint,M_UPN,13,6};
const XppMenu menu_color_code={"color_code","Color code",3,color_items,"nva",color_hint,M_UCN,11,12};
const XppMenu menu_adjoint={"adjoint","Adjoint",7,adj_items,"nmaohpr",adj_hint,M_UAN,10,11};
const XppMenu menu_lookup={"lookup","Tables",2,tab_items,"ev",tab_hint,M_UKE,12,11};
/* sets METHOD directly */
const XppMenu menu_method={"method","Method",15,meth_items,"demragvbqsc582y",meth_hint,-1,15,1};
const XppMenu menu_edit_rhs={"edit_rhs","Edit Stuff",4,edrh_items,"rfsl",edrh_hint,M_FER,11,13};

const XppMenu *const xpp_menus[]={
  &menu_integrate,&menu_nullclines,&menu_freeze_cline,&menu_dirfield,
  &menu_window,&menu_torus,&menu_kinescope,&menu_curves,&menu_freeze,
  &menu_freeze_off,&menu_freeze_key,&menu_colormap,&menu_windows,
  &menu_windows_simoff,&menu_text,&menu_text_edit,&menu_equilibria,
  &menu_view,&menu_bvp,&menu_stochastic,&menu_poincare,&menu_color_code,
  &menu_adjoint,&menu_lookup,&menu_method,&menu_edit_rhs};
const int xpp_menu_count=sizeof(xpp_menus)/sizeof(xpp_menus[0]);
