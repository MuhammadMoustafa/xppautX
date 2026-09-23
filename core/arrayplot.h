#ifndef _arrayplot_h_
#define _arrayplot_h_

#include "xpplim.h"
#include <stdio.h>


#include "xpp_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  XppWinId base,wclose,wedit,wprint,wstyle,wscale,wmax,wmin,wplot,wredraw,wtime,wgif,wrange,wfit;
  int index0,indexn,alive,nacross,ndown,plotdef;
  int height,width,ploth,plotw;
  int nstart,nskip,ncskip;
  char name[XPP_NAME_MAX+1];
  double tstart,tend,zmin,zmax,dt;
  char xtitle[256],ytitle[256],filename[256],bottom[256];
  int type;
} APLOT;

extern APLOT aplot;
void set_acolor(int);
void tag_aplot(char *);
void close_aplot_files(void);
void draw_one_array_plot(char *);
void gif_aplot_all(char *,int);
void optimize_aplot(int *plist);
void make_my_aplot(char *name);
void scale_aplot(APLOT *ap, double *zmax, double *zmin);
void init_arrayplot(APLOT *ap);
void set_up_aplot_range(void);
void fit_aplot(void);
int editaplot(APLOT *ap);
void print_aplot(APLOT *ap);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void expose_aplot(Window w);
void do_array_plot_events(XEvent ev);
void wborder(Window w, int i, APLOT ap);
#endif /* Xlib.h */
void destroy_aplot(void);
void init_my_aplot(void);
void create_arrayplot(APLOT *ap, char *wname, char *iname);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void apbutton(Window w);
void draw_scale(APLOT ap);
void draw_aplot(APLOT ap);
#endif /* Xlib.h */
void edit_aplot(void);
void get_root(char *s, char *sroot, int *num);
void reset_aplot_axes(APLOT ap);
void dump_aplot(FILE *fp, int f);
void gif_aplot(void);
void grab_aplot_screen(APLOT ap);
void redraw_aplot(APLOT ap);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void display_aplot(Window w, APLOT ap);

#endif /* Xlib.h */

#ifdef __cplusplus
}
#endif
#endif
