#ifndef _arrayplot_h_
#define _arrayplot_h_

#include "xpplim.h"
#include <stdio.h>


#include "xpp_types.h"
#ifdef __cplusplus
extern "C" {
#endif

void close_aplot_files(void);
void optimize_aplot(int *plist);
void set_up_aplot_range(void);
void fit_aplot(void);
void init_my_aplot(void);
/* splits an array plot's first column name at its trailing digits into
   sroot (100 chars) and the number */
void get_root(const char *s, char *sroot, int *num);
void dump_aplot(FILE *fp, int f);

/* 1: the next range integration saves the array plot at each step (the
   array plot's range saving); integrate.c clears it when the range ends */
extern int aplot_range;

/* the range saving's settings (Array range saving) and its open GIF file
   (json_windows.cpp writes it); Autoplot: redraw after each integration */
extern int aplot_range_count, aplot_still, aplot_tag, plot3d_auto_redraw;
extern FILE *ap_fp;

#ifdef __cplusplus
}

#include <string>
#include <string_view>

/* the array plot's settings */
struct APLOT {
  XppWinId base,wclose,wedit,wprint,wstyle,wscale,wmax,wmin,wplot,wredraw,wtime,wgif,wrange,wfit;
  int index0,indexn,alive,nacross,ndown,plotdef;
  int height,width,ploth,plotw;
  int nstart,nskip,ncskip;
  char name[XPP_NAME_MAX+1]; /* its first column (lunch-new.cpp's io_string reads it by its size) */
  double tstart,tend,zmin,zmax,dt;
  std::string xtitle,ytitle,filename,bottom; /* Print's */
  int type;
};

extern APLOT aplot;
void scale_aplot(APLOT *ap, double *zmax, double *zmin);
int editaplot(APLOT *ap);
void print_aplot(APLOT *ap);
std::string get_root(std::string_view s, int *num);
extern std::string aplot_range_stem; /* Array range saving's base name */
#endif
#endif
