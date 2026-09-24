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

int AutoFreezeFlag = 0; /* freeze the curve after every integration */
CURVE frz[MAXFRZ];  /* frozen curves of every plot window */
int ks_ncycle = 1, ks_speed = 50; /* kinescope autoplay */
int help_menu; /* which main-window menu keys go to: MAIN_MENU, FILE_MENU, NUM_MENU */

char PlotFormat[100];
int PS_Color = 1;

int animation_on_the_fly = 0;

int aplot_range;

int AxisVarLabels = 0;
