/* Definitions for xpp_globals.h. Values are the ones main.c, many_pops.c,
   color.c and graf_par.c used to initialise them with. */
#include "xpp_globals.h"

int Xup = 0, TipsFlag = 1;
int XPPBatch = 0, batch_range = 0, BatchEquil = -1;
char batchout[256];
char UserOUTFILE[256];
int allwinvis = 0;
int use_intern_sets = 1;
int use_ani_file = 0;
char anifile[XPP_MAX_NAME];
float xppvermaj, xppvermin;
/* Set this to 1 if you want the tutorial to come up at start-up as default
   behavior */
int DoTutorial = 0;
OptionsSet notAlreadySet;

FILE *logfile;
int XPPVERBOSE = 1;
int OVERRIDE_QUIET = 0;
int OVERRIDE_LOGFILE = 0;
int tfBell;

char big_font_name[100], small_font_name[100];
char PlotFormat[100];
int PaperWhite = -1;
char UserBlack[8];
char UserWhite[8];
char UserMainWinColor[8];
char UserDrawWinColor[8];
char UserBGBitmap[XPP_MAX_NAME];
int UserGradients = -1;
int UserMinWidth = 0, UserMinHeight = 0;
int PS_Color = 1;

int SLIDER1 = -1;
int SLIDER2 = -1;
int SLIDER3 = -1;
char SLIDER1VAR[20];
char SLIDER2VAR[20];
char SLIDER3VAR[20];
double SLIDER1LO = 0.0;
double SLIDER2LO = 0.0;
double SLIDER3LO = 0.0;
double SLIDER1HI = 1.0;
double SLIDER2HI = 1.0;
double SLIDER3HI = 1.0;
double SLIDER1INIT = 0.5;
double SLIDER2INIT = 0.5;
double SLIDER3INIT = 0.5;

int color_mode = 1, color_min, color_total, COLOR, color_max;

GRAPH graph[MAXPOP];
GRAPH *MyGraph;
int SimulPlotFlag = 0;
int current_pop;
int num_pops;
int ActiveWinList[MAXPOP];

int animation_on_the_fly = 0;

int DCURYb, DCURXb, CURY_OFFb;
int DCURYs, DCURXs, CURY_OFFs;
int DCURY, DCURX, CURY_OFF;
int MSStyle = 0;

int AutoRedrawFlag = 1;
int mark_flag = 0;
int mark_ibrs, mark_ibre;
int mark_ipts, mark_ipte;
int mark_ixs, mark_ixe, mark_iys, mark_iye;

int aplot_range;
