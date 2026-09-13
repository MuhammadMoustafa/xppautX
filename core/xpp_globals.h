#ifndef XPP_GLOBALS_H
#define XPP_GLOBALS_H

/* Program-wide state that the numerics need and that used to be defined in
   main.c / many_pops.c / color.c / graf_par.c (all X11 files). Defined in
   xpp_globals.c, which has no UI dependency. */

#include <stdio.h>
#include "xpplim.h"
#include "struct.h"
#include "load_eqn.h" /* OptionsSet */

/* run mode */
extern int Xup, TipsFlag;
extern int XPPBatch, batch_range, BatchEquil;
extern char batchout[256];
extern char UserOUTFILE[256];
extern int allwinvis;
extern int use_intern_sets;
extern int use_ani_file;
extern char anifile[XPP_MAX_NAME];
extern float xppvermaj, xppvermin;
extern int DoTutorial;
extern OptionsSet notAlreadySet;

/* logging */
extern FILE *logfile;
extern int XPPVERBOSE;
extern int OVERRIDE_QUIET;
extern int OVERRIDE_LOGFILE;
extern int tfBell;

/* appearance options parsed from the ODE file / command line */
extern char big_font_name[100], small_font_name[100];
extern char PlotFormat[100];
extern int PaperWhite;
extern char UserBlack[8];
extern char UserWhite[8];
extern char UserMainWinColor[8];
extern char UserDrawWinColor[8];
extern char UserBGBitmap[XPP_MAX_NAME];
extern int UserGradients;
extern int UserMinWidth, UserMinHeight;
extern int PS_Color;

/* sliders */
extern int SLIDER1, SLIDER2, SLIDER3;
extern char SLIDER1VAR[20], SLIDER2VAR[20], SLIDER3VAR[20];
extern double SLIDER1LO, SLIDER2LO, SLIDER3LO;
extern double SLIDER1HI, SLIDER2HI, SLIDER3HI;
extern double SLIDER1INIT, SLIDER2INIT, SLIDER3INIT;

/* colour table bookkeeping (the X colormap itself stays in color.c) */
extern int color_mode, color_min, color_total, COLOR, color_max;

/* plot windows: the graph array and which one is active */
extern GRAPH graph[MAXPOP];
extern GRAPH *MyGraph;
extern int SimulPlotFlag;
extern int current_pop;
extern int num_pops;
extern int ActiveWinList[MAXPOP];

/* animation */
extern int animation_on_the_fly;

/* font metrics of the front end (0 when headless) */
extern int DCURYb, DCURXb, CURY_OFFb;
extern int DCURYs, DCURXs, CURY_OFFs;
extern int DCURY, DCURX, CURY_OFF;
extern int MSStyle;

/* AUTO diagram window state read by auto_nox.c */
extern int AutoRedrawFlag;
extern int mark_flag;
extern int mark_ibrs, mark_ibre;
extern int mark_ipts, mark_ipte;
extern int mark_ixs, mark_ixe, mark_iys, mark_iye;

/* array plot */
extern int aplot_range;

#endif
