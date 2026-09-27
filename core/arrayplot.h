#ifndef _arrayplot_h_
#define _arrayplot_h_

#include "xpplim.h"
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

void close_aplot_files(void);
void optimize_aplot(int *plist);
void set_up_aplot_range(void);
void fit_aplot(void);
void init_my_aplot(void);
void dump_aplot(FILE *fp, int f);


#ifdef __cplusplus
}

#include <string>
#include <string_view>

/* the array plot's settings */
struct APLOT {
  int index0,alive,nacross,ndown,plotdef;
  int height,width,ploth,plotw;
  int nstart,nskip,ncskip;
  std::string name; /* its first column */
  double zmin,zmax;
  std::string xtitle,ytitle,filename,bottom; /* Print's */
  int type;
};

void scale_aplot(APLOT *ap, double *zmax, double *zmin);
int editaplot(APLOT *ap);
void print_aplot(APLOT *ap);
/* splits an array plot's first column name at its trailing digits: "u10"
   gives "u" and 10, a name with no digits itself and 0 */
std::string get_root(std::string_view s, int *num);

/* the array plot (arrayplot.cpp), a Session's (session.h): its settings;
   range: the next range integration saves the array plot at each step
   (the array plot's range saving), integrate.cpp clears it when the range
   ends; the range saving's settings (Array range saving: range_count,
   still, tag, the base name range_stem) and its open GIF file
   (json_windows.cpp writes it); Autoplot: redraw after each integration
   (auto_redraw) */
struct ArrayPlotState {
  APLOT plot{};
  int range = 0, range_count = 0, still = 1, tag = 0, auto_redraw = 0;
  FILE *fp = nullptr;
  std::string range_stem = "rangearray";
};
#endif
#endif
