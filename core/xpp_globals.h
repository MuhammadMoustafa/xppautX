#ifndef XPP_GLOBALS_H
#define XPP_GLOBALS_H

/* Program-wide state that the numerics need and that used to be defined in
   main.c / many_pops.c / color.c / graf_par.c (all X11 files). Defined in
   xpp_globals.c, which has no UI dependency. */

#include <stdio.h>
#include "xpplim.h"
#include "struct.h"
#include "load_eqn.h" /* OptionsSet */
#ifdef __cplusplus
extern "C" {
#endif

/* run mode */
extern int Xup;
extern int use_ani_file;
/* AUTO's scratch directory (fort.3/7/8/9, <model>.ode.b/.d/.s). NULL: HOME,
   as upstream (X11, -silent); xppautX sets a private one per session so
   concurrent sessions never share AUTO files (xppautx_main.c). */
extern char *xpp_auto_dir;
extern char anifile[XPP_MAX_NAME];
extern float xppvermaj, xppvermin;
extern int DoTutorial;
extern OptionsSet notAlreadySet;

extern int help_menu;
extern int ks_ncycle, ks_speed;
extern CURVE frz[MAXFRZ];
extern XppWinId draw_win;
extern int AutoFreezeFlag;

/* appearance options parsed from the ODE file / command line */
extern char PlotFormat[100];
extern int PS_Color;

/* colour table bookkeeping (the X colormap itself stays in color.c) */
extern int color_min, color_total, COLOR, color_max;

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
extern int DCURYs, DCURXs;
extern int DCURY, DCURX;

/* AUTO diagram window state read by auto_nox.c */
extern int AutoRedrawFlag;
extern int mark_flag;
extern int mark_ibrs, mark_ibre;
extern int mark_ipts, mark_ipte;
extern int mark_ixs, mark_ixe, mark_iys, mark_iye;

/* array plot */
extern int aplot_range;

/* label unlabelled 2D axes with the plotted variables (front ends that ask) */
extern int AxisVarLabels;

#ifdef __cplusplus
}
#endif
#endif
