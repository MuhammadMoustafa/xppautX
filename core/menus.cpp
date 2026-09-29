/* Menu labels and hint strings. Generated from upstream menus.h; the
   MENUDEF widget struct lives in menus.h. */
#include "menus.h"
#include "menudrive.h"



const char *const main_menu[]={
 "XPP","Initialconds","Continue","Nullcline",
 "Dir.field/flow","Window/zoom","phAsespace",
 "Kinescope","Graphic stuff","nUmerics","File",
 "Parameters","Erase","Makewindow","Text,etc",
 "Sing pts","Viewaxes","Xi vs t","Restore","3d-params",
 "Bndryval"};

const char *const num_menu[]={"NUMERICS","Total","Start time","tRansient",
"Dt","Ncline ctrl","sIng pt ctrl","nOutput","Bounds","Method",
"dElay","Color code","stocHast","Poincare map","rUelle plot",
"looKup","bndVal","Averaging","[Esc]-exit"};
const char *const file_menu[]={
"FILE","Prt src","Write set","Read set",
"Auto","Calculator","Save info",
"Help","Quit","Transpose","Get par set","cLone",".Xpprc","tUtorial",
"cOpy set line","open Model","rEload"};

/* hints for the main menus */
const char *const main_hint[]=
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


const char *const file_hint[]={
"Display source and active comments",
"Save information for restart",
"Read information for restart",
"Run AUTO, the bifurcation package",
"A little calculator -- press ESC to exit",
"Save info about simulation in human readable format",
"Browser help",
"Duh!",
"Transpose storage",
"Set predefined parameters",
"Clone the ode file",
"Edit your .xpprc preferences file",
"Run a quick tutorial on XPPAUT",
"Copy the current values as a named set line for the .ode",
"Load another model in place of this one",
"Read the model's file again, keeping the values"
};


const char *const num_hint[]={
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

const char *const null_hint[]={
"Compute new nullclines",
"Redraw last nullclines",
"Set automatic redraw -- X redraws when needed",
"Only redraw when asked (Redraw)",
"Freeze multiple nullclines",
"Save nullcline values to a file"
};

const char *const null_freeze[]={
  "Freeze current clines",
  "Delete all frozen clines",
  "Range freeze a bunch of clines",
  "Animate nullclines"
};
const char *const ic_hint[]={
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

const char *const wind_hint[]={
"Manually choose 2D view",
"Zoom into with mouse",
"Zoom out with mouse",
"Let XPP automatically choose window",
"Reset to default view",
"Scroll around the view"
};

const char *const flow_hint[]={
"Draw vector field for 2D section",
"Draw regular series of trajectories",
" ",
"Color the PP on a grid",
"Draw only directions"
};

const char *const phas_hint[]={
"Each variable is on a circle",
"No variable on circle",
"Choose circle variables"
};

const char *const kin_hint[]={
"Grab a screen shot",
"Clear all screen shots",
"Manually cycle thru screenshots",
"Continuously cycle through screen shots",
"Dump the screen shots to disk",
"Make animated gif file from screenshots"
};

const char *const graf_hint[]={
"Add another curve to the current plot",
"Delete last added plot",
"Remove all the added plots except the main one",
"Edit parameters for a plot",
"Create a postscript file of current plot",
"Create a styleable svg file of current plot",
"Options for permanently saving curve",
"Axes label sizes and zero axes for postscript",
"Save what the plot shows (its curves and frozen curves) as data",
"Change colormap"
};

const char *const cmap_hint[]={
 " blue-green-red",
 "red-...-violet-red",
 "black-red-yellow-white",
 "pale cyan->pale yellow",
 "blue-violet-red",
 "black-white",
 "helical luminence corrected"
};
const char *const frz_hint[]={
"Permanently keep main curve -- even after reintegrating",
"Delete specified frozen curve",
"Edit specified frozen curve",
"Remove all frozen curves",
"Toggle key on/off",
"Import bifurcation data",
"Clear imported bifurcation curve",
"Automatically freeze after each integration",
};

const char *const stoch_hint[]={
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

const char *const bvp_hint[]={
"Solve BVP over range of parameters",
"Don't show any but final step",
"Show each step of iteration",
"Solve BVP with periodic conditions",
"Set up special homoclinic stuff"
}; 

const char *const adj_hint[]={
"Compute a new adjoint function",
"Compute averaging interaction function",
"Load computed adjoint",
"Load computed orbit",
"Load computed interaction function",
"Adjoint numerical parameters",
"Range over stuff to computte many adjoints"
};

const char *const map_hint[]={
"Turn off Poincare map",
"Define section for Poincare map",
"Compute Poincare map on maximum/minimum of variable",
"Compute map based on period between events",
};


const char *const view_hint[]={
  "Two-dimensional view settings",
  "Three-dimensional view settings",
  "Plot array ",
  "Animation window"
};

const char *const half_hint[]={
"Create new window",
"Delete all but main window",
"Delete last window",
"Place current window at bottom",
"Automatically redraw",
"Redraw only when requested",
"Plot all graphs simultaneously -- slows you down"};

const char *const text_hint[]={
"Create text labels in different fonts ",
"Add arrows to trajectories",
"Create lines with arrowheads",
"Add squares, circles, etc to plot",
"Change properties, delete, or move existing add-on",
"Delete all text, markers, arrows",
"Create many markers based on browser data"
};

const char *const edit_hint[]={
"Move the selected item",
"Change properties of selected item",
"Delete selected item"
};

const char *const sing_hint[]={
"Find fixed points over range of parameter",
" ",
"Use mouse to guess fixed point",
"Monte carlo search for fixed points"};

const char *const meth_hint[]={
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

const char *const color_hint[]={
" ",
"Color according to magnitude of derivative",
"Color according to height of Z-axis"
};

const char *const tab_hint[]={"Edit the lookup tables","View a table in the data browser"};


const char *const auto_hint[]={
"Tell AUTO the parameters you may vary",
"What will be plotted on the axes and what parameter(s)",
"Tell AUTO range, direction, and tolerance",
"Run the continuation",
"Grab a point to continue from",
"Tell AUTO to save at values of period or parameter",
"Erase the screen",
"Redraw the diagram",
"Save and output options"};

const char *const no_hint[]={ 
" "," "," "," "," "," "," "," "," "," ", " "," "," "," "};

const char *const aaxes_hint[]={
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

const char *const afile_hint[]={
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

const char *const aspecial_hint[]={
"Bifurcation or branch point",
"Endpoint of a branch",
"Hopf bifurcation point",
"Limit point or turning point of a branch",
"Failure to converge",
"Period doubling bifurcation",
"Torus bifurcation from a periodic",
"User defined function",
};

const char *const arun_hint[]={
  "Start at fixed point",
  "Start at periodic orbit",
  "Start at solution to boundary value problem",
  "Start at homoclinics",
  "Start at a heteroclinic",
}; 

/* keys of the main-window menus; the numerics menu ends with Esc */
const char *const main_menu_keys="icndwakgufpemtsvxr3b";
const char *const num_menu_keys="tsrdniobmechpukva\033";
const char *const file_menu_keys="pwracshqtglxuome";

/* pop-up menus, formerly static arrays inside the menudrive.c handlers */
static const char *ic_items[]={"(R)ange","(2)par range","(L)ast","(O)ld","(G)o",
  "(M)ouse","(S)hift","(N)ew","s(H)oot","(F)ile","form(U)la","m(I)ce",
  "DAE guess","(B)ackward"};
static const char *null_items[]={"(N)ew","(R)estore","(A)uto","(M)anual",
  "(F)reeze","(S)ave"};
static const char *frzcline_items[]={"(F)reeze","(D)elete all","(R)ange","(A)nimate"};
static const char *dfield_items[]={"(D)irect Field","(F)low","(N)o dir. fld.",
  "(C)olorize","(S)caled Dir.Fld"};
static const char *window_items[]={"(W)indow","(Z)oom In","Zoom (O)ut","(F)it",
  "(D)efault","(S)croll"};
static const char *torus_items[]={"(A)ll","(N)one","(C)hoose"};
/* (X)tra is defined but not shown: the menu has 6 entries */
static const char *kin_items[]={"(C)apture","(R)eset","(P)layback","(A)utoplay",
  "(S)ave","(M)ake AniGif"};
static const char *curve_items[]={"(A)dd curve","(D)elete last","(R)emove all",
  "(E)dit curve","(P)ostscript","S(V)G","(F)reeze","a(X)es opts",
  "exp(O)rt data","(C)olormap"};
static const char *freeze_items[]={"(F)reeze","(D)elete","(E)dit","(R)emove all",
  "(K)ey","(B)if.Diag","(C)lr. BD","(O)n freeze"};
static const char *freeze_off_items[]={"(F)reeze","(D)elete","(E)dit","(R)emove all",
  "(K)ey","(B)if.Diag","(C)lr. BD","(O)ff freeze"};
static const char *key_items[]={"(N)o key","(K)ey"};
static const char *cmap_items[]={"(N)ormal","(P)eriodic","(H)ot","(C)ool",
  "(B)lue-red","(G)ray","c(U)behelix"};
static const char *windows_items[]={"(C)reate","(K)ill all","(D)estroy","(B)ottom",
  "(A)uto","(M)anual","(S)imPlot On"};
static const char *windows_simoff_items[]={"(C)reate","(K)ill all","(D)estroy",
  "(B)ottom","(A)uto","(M)anual","(S)imPlot Off"};
static const char *text_items[]={"(T)ext","(A)rrow","(P)ointer","(M)arker",
  "(E)dit","(D)elete all","marker(S)"};
static const char *text_edit_items[]={"(M)ove","(C)hange","(D)elete"};
static const char *sing_items[]={"(G)o","(M)ouse","(R)ange","monte(C)ar"};
static const char *view_items[]={"2D","3D","Array","Toon"};
static const char *bvp_items[]={"(R)ange","(N)o show","(S)how","(P)eriodic"};
static const char *stoch_items[]={"New seed","Compute","Data","Mean","Variance",
  "Histogram","Old hist","Fourier","Power","fIt data","Stat","Liapunov",
  "stAutocor","Xcorrel etc","spEc.dns","2D-hist"};
static const char *map_items[]={"(N)one","(S)ection","(M)ax/min","(P)eriod"};
static const char *color_items[]={"(N)o color","(V)elocity","(A)nother quantity"};
static const char *adj_items[]={"(N)ew adj","(M)ake H","(A)djoint","(O)rbit",
  "(H)fun","(P)arameters","(R)ange"};
static const char *tab_items[]={"(E)dit","(V)iew"};
static const char *meth_items[]={"(D)iscrete","(E)uler","(M)od. Euler",
  "(R)unge-Kutta","(A)dams","(G)ear","(V)olterra","(B)ackEul",
  "(Q)ualst.RK4","(S)tiff","(C)Vode","DoPri(5)","DoPri(8)3",
  "Rosen(2)3","sYmplectic"};

/*                                 name  title  n  items  keys  hints  first_cmd */
const XppMenu menu_integrate={"integrate","Integrate",14,ic_items,"r2logmsnhfuidb",ic_hint,M_IR};
const XppMenu menu_nullclines={"nullclines","Nullclines",6,null_items,"nramfs",null_hint,M_NN};
const XppMenu menu_freeze_cline={"freeze_cline","Freeze cline",4,frzcline_items,"fdra",null_freeze,M_NFF};
const XppMenu menu_dirfield={"dirfield","Two-D Fun",5,dfield_items,"dfncs",flow_hint,M_DD};
const XppMenu menu_window={"window","Window",6,window_items,"wzofds",wind_hint,M_WW};
const XppMenu menu_torus={"torus","Torus",3,torus_items,"anc",phas_hint,M_AA};
const XppMenu menu_kinescope={"kinescope","Kinescope",6,kin_items,"crpasm",kin_hint,M_KC};
/* (F)reeze and (C)olormap open the next two menus instead of a command */
const XppMenu menu_curves={"curves","Curves",10,curve_items,"adrepvfxoc",graf_hint,M_GA};
const XppMenu menu_freeze={"freeze","Freeze",8,freeze_items,"fderkbco",frz_hint,M_GFF};
const XppMenu menu_freeze_off={"freeze_off","Freeze",8,freeze_off_items,"fderkbco",frz_hint,M_GFF};
const XppMenu menu_freeze_key={"freeze_key","Key",2,key_items,"nk",no_hint,M_GFKN};
const XppMenu menu_colormap={"colormap","Colormap",7,cmap_items,"nphcbgu",cmap_hint,M_GCN};
const XppMenu menu_windows={"windows","Make window",7,windows_items,"ckdbams",half_hint,M_MC};
const XppMenu menu_windows_simoff={"windows_simoff","Make window",7,windows_simoff_items,"ckdbams",half_hint,M_MC};
/* (E)dit opens menu_text_edit */
const XppMenu menu_text={"text","Text,etc",7,text_items,"tapmeds",text_hint,M_TT};
const XppMenu menu_text_edit={"text_edit","Edit",3,text_edit_items,"mcd",edit_hint,M_TEM};
const XppMenu menu_equilibria={"equilibria","Equilibria",4,sing_items,"gmrc",sing_hint,M_SG};
const XppMenu menu_view={"view","Axes",4,view_items,"23at",view_hint,M_V2};
const XppMenu menu_bvp={"bvp","Bndry Value Prob",4,bvp_items,"rnsp",bvp_hint,M_BR};
const XppMenu menu_stochastic={"stochastic","Stochastic",16,stoch_items,"ncdmvhofpislaxe2",stoch_hint,M_UHN};
const XppMenu menu_poincare={"poincare","Poincare map",4,map_items,"nsmp",map_hint,M_UPN};
const XppMenu menu_color_code={"color_code","Color code",3,color_items,"nva",color_hint,M_UCN};
const XppMenu menu_adjoint={"adjoint","Adjoint",7,adj_items,"nmaohpr",adj_hint,M_UAN};
const XppMenu menu_lookup={"lookup","Tables",2,tab_items,"ev",tab_hint,M_UKE};
/* Save data (browse_data.cpp data_write): what it writes; the format
   comes next, from the data formats' registry (data_formats.h) */
static const char *const save_what_items[]={"The data table","What the plot shows"};
static const char *const save_what_hint[]={"The stored rows from First to Last, every column",
  "The current plot's curves and frozen curves, one row per point: curve,x,y (and z in 3D)"};
const XppMenu menu_save_what={"save_what","Save data",2,save_what_items,"tp",save_what_hint,-1};
/* sets METHOD directly */
const XppMenu menu_method={"method","Method",15,meth_items,"demragvbqsc582y",meth_hint,-1};


/* ---- the windows' key layers (menus.h) ---- */

int xpp_menu_index(const XppMenu *m, int ch)
{
  for (int i = 0; i < m->n; i++)
    if (ch == m->keys[i])
      return i;
  return -1;
}

static const char *auto_window_items[]={"(P)arameter","(A)xes","(N)umerics","(R)un","(G)rab",
  "(U)sr period","(C)lear","re(D)raw","(F)ile"};
const XppMenu menu_auto_window={"auto_window","AUTO",9,auto_window_items,"panrgucdf",auto_hint,-1};

static const char *browser_window_items[]={"(F)ind","(G)et","(R)eplace","(U)nreplace","(T)able","(H)ome: first",
  "(E)nd: last","re(S)tore","(A)dd column","(D)elete column","(L)oad","(W)rite"};
static const char *const browser_window_hint[]={"Find a value in a column","Make the selected row the initial conditions",
  "Replace a column's values","Undo the last replace","View a table","The first row to keep","The last row to keep",
  "Keep every row again","Add a column from a formula","Delete the last added column","Load data from a file",
  "Save the data or the plot to a file"};
const XppMenu menu_browser_window={"browser_window","Data browser",12,browser_window_items,"fgruthesadlw",
  browser_window_hint,-1};

static const char *ani_window_items[]={"(F)ile","(G)o","(R)eset","(S)kip","(M)peg","(O)n the fly","gr(A)b"};
static const char *const ani_window_hint[]={"Load an animation file","Play the animation","Back to the first frame",
  "Frames skipped between two shown","Save the frames as files","Toggle drawing while integrating",
  "Grab a point of the picture"};
const XppMenu menu_ani_window={"ani_window","Animation",7,ani_window_items,"fgrsmoa",ani_window_hint,-1};

static const char *aplot_window_items[]={"re(D)raw","(E)dit","(F)it","(R)ange","(P)rint","(G)IF"};
static const char *const aplot_window_hint[]={"Redraw the array plot","Edit its settings","Fit the range to the data",
  "Draw a range of parameter values","Write PostScript","Write a GIF"};
const XppMenu menu_aplot_window={"aplot_window","Array plot",6,aplot_window_items,"defrpg",aplot_window_hint,-1};

static const char *equilibrium_window_items[]={"(I)mport"};
static const char *const equilibrium_window_hint[]={"Make the equilibrium the initial conditions"};
const XppMenu menu_equilibrium_window={"equilibrium_window","Equilibrium",1,equilibrium_window_items,"i",
  equilibrium_window_hint,-1};
