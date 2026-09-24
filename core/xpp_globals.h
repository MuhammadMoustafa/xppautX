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

/* appearance options parsed from the ODE file / command line */
extern char PlotFormat[100];
extern int PS_Color;

/* animation */
extern int animation_on_the_fly;

/* array plot */
extern int aplot_range;

/* label unlabelled 2D axes with the plotted variables (front ends that ask) */
extern int AxisVarLabels;

#ifdef __cplusplus
}
#endif
#endif
