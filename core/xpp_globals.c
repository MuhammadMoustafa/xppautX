/* Definitions for xpp_globals.h. Values are the ones main.c, many_pops.c,
   color.c and graf_par.c used to initialise them with. */
#include "xpp_globals.h"

int Xup = 0;
int use_ani_file = 0;
char anifile[XPP_MAX_NAME];
char *xpp_auto_dir = NULL;
float xppvermaj, xppvermin;
/* Set this to 1 if you want the tutorial to come up at start-up as default
   behavior */
int DoTutorial = 0;
OptionsSet notAlreadySet;

FILE *logfile;
int XPPVERBOSE = 1;
int OVERRIDE_QUIET = 0;
int OVERRIDE_LOGFILE = 0;
int AutoFreezeFlag = 0; /* freeze the curve after every integration */
CURVE frz[MAXFRZ];  /* frozen curves of every plot window */
XppWinId draw_win;  /* the plot window being drawn into (an X11 Window) */
int ks_ncycle = 1, ks_speed = 50; /* kinescope autoplay */
int help_menu; /* which main-window menu keys go to: MAIN_MENU, FILE_MENU, NUM_MENU */

char PlotFormat[100];
int PS_Color = 1;

char SLIDER1VAR[XPP_NAME_MAX+1];
char SLIDER2VAR[XPP_NAME_MAX+1];
char SLIDER3VAR[XPP_NAME_MAX+1];
double SLIDER1LO = 0.0;
double SLIDER2LO = 0.0;
double SLIDER3LO = 0.0;
double SLIDER1HI = 1.0;
double SLIDER2HI = 1.0;
double SLIDER3HI = 1.0;

int color_min, color_total, COLOR, color_max;

GRAPH graph[MAXPOP];
GRAPH *MyGraph;
int SimulPlotFlag = 0;
int current_pop;
int num_pops;
int ActiveWinList[MAXPOP];

int animation_on_the_fly = 0;

int DCURYs, DCURXs;
int DCURY, DCURX;

int AutoRedrawFlag = 1;
int mark_flag = 0;
int mark_ibrs, mark_ibre;
int mark_ipts, mark_ipte;
int mark_ixs, mark_ixe, mark_iys, mark_iye;

int aplot_range;

int AxisVarLabels = 0;
