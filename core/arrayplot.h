#ifndef _arrayplot_h_
#define _arrayplot_h_

#include <stdio.h>


#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
typedef struct {
  Window base,wclose,wedit,wprint,wstyle,wscale,wmax,wmin,wplot,wredraw,wtime,wgif,wrange,wfit;
  int index0,indexn,alive,nacross,ndown,plotdef;
  int height,width,ploth,plotw;
  int nstart,nskip,ncskip;
  char name[20];
  double tstart,tend,zmin,zmax,dt;
  char xtitle[256],ytitle[256],filename[256],bottom[256];
  int type;
} APLOT;
#endif /* Xlib.h */
void set_acolor(int);
void tag_aplot(char *);
void close_aplot_files(void);
void draw_one_array_plot(char *);
void gif_aplot_all(char *,int);
void optimize_aplot(int *plist);
void make_my_aplot(char *name);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void scale_aplot(APLOT *ap, double *zmax, double *zmin);
void init_arrayplot(APLOT *ap);
void expose_aplot(Window w);
void do_array_plot_events(XEvent ev);
void wborder(Window w, int i, APLOT ap);
#endif /* Xlib.h */
void destroy_aplot(void);
void init_my_aplot(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void create_arrayplot(APLOT *ap, char *wname, char *iname);
void print_aplot(APLOT *ap);
void apbutton(Window w);
void draw_scale(APLOT ap);
void draw_aplot(APLOT ap);
#endif /* Xlib.h */
void edit_aplot(void);
void get_root(char *s, char *sroot, int *num);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void reset_aplot_axes(APLOT ap);
#endif /* Xlib.h */
void dump_aplot(FILE *fp, int f);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
int editaplot(APLOT *ap);
#endif /* Xlib.h */
void gif_aplot(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void grab_aplot_screen(APLOT ap);
void redraw_aplot(APLOT ap);
void display_aplot(Window w, APLOT ap);

#endif /* Xlib.h */
#endif
