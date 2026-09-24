#ifndef _many_pops_h
#define _many_pops_h
#ifdef __cplusplus
extern "C" {
#endif


int select_table(void);
void get_intern_set(void);
void title_text(char *string);
void restore_off(void);
void restore_on(void);
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
void destroy_a_pop(void);
void init_grafs(int x, int y, int w, int h);
void ps_restore(void);
void svg_restore(void);
void resize_all_pops(int wid, int hgt);
void kill_all_pops(void);
void create_a_pop(void);
void GrCol(void);
void BaseCol(void);
void SmallGr(void);
void SmallBase(void);
void make_active(int i,int flag);
void set_gr_fore(void);
void set_gr_back(void);
void canvas_xy(char *buf);
void set_active_windows();


#ifdef __cplusplus
}
#endif
#endif
