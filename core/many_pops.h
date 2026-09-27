#ifndef _many_pops_h
#define _many_pops_h
#ifdef __cplusplus
extern "C" {
#endif


int select_table(void);
#ifdef __cplusplus
}
#endif
#include "grobs.h"
#ifdef __cplusplus
extern "C" {
#endif

/* The plot windows: every window's graph settings, the active one, and
   the windows a command draws on (all open ones under Simulplot). */
typedef struct {
    GRAPH graph[MAXPOP];   /* graph[i].Use: window i is open */
    GRAPH *current;        /* &graph[active] */
    int active;            /* the active window's index */
    int count;             /* how many are open */
    int open[MAXPOP];      /* the open windows' indices, count of them */
    int simul;             /* Simulplot: draw on every open window */
    XppWinId draw_win;     /* graph[active].w, the window drawn into */
} XppPlotWindows;
extern XppPlotWindows plot_windows;


#ifdef __cplusplus
}
#endif
#endif
