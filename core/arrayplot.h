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
void close_aplot_files(void);
void optimize_aplot(int *plist);
void scale_aplot(APLOT *ap, double *zmax, double *zmin);
void init_arrayplot(APLOT *ap);
void set_up_aplot_range(void);
void fit_aplot(void);
int editaplot(APLOT *ap);
void print_aplot(APLOT *ap);
void init_my_aplot(void);
void edit_aplot(void);
void get_root(const char *s, char *sroot, int *num);
void dump_aplot(FILE *fp, int f);

/* 1: the next range integration saves the array plot at each step (the
   array plot's range saving); integrate.c clears it when the range ends */
extern int aplot_range;

/* the range saving's settings (Array range saving) and its open GIF file
   (json_windows.cpp writes it); Autoplot: redraw after each integration */
extern int aplot_range_count, aplot_still, aplot_tag, plot3d_auto_redraw;
extern char aplot_range_stem[256];
extern FILE *ap_fp;

#ifdef __cplusplus
}
#endif
#endif
