#pragma once

#include "xpp_error.h"
#include "xpp_io.h"
#include <stdio.h>

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
};

namespace xpp {
/* One array picture passed to its registered encoder. Data remains in storage. */
struct ArrayPicture {
  const char *filename, *xtitle, *ytitle, *bottom;
  int nacross, ndown, col0, row0, nskip, ncskip, maxrow, maxcol;
  float **data;
  double zmin, zmax, tlo, thi;
};
struct Session; /* session.h */
}

/* the array plot of the loading Session s at its defaults (a load) */
void init_my_aplot(xpp::Session &s);

/* The array plot of the session s: its range saving's movie closed,
   Plotvars' columns shown, Array range saving's settings, Fit, the range
   of ap's values, Edit, Print, and its settings in a .set file (f: 1 read,
   else write) */
void close_aplot_files(xpp::Session &s, bool complete);
void optimize_aplot(xpp::Session &s, int *plist);
void set_up_aplot_range(xpp::Session &s);
void fit_aplot(xpp::Session &s);
void scale_aplot(const xpp::Session &s, APLOT *ap, double *zmax, double *zmin);
/* the Edit and Print keys: their forms asked, then array_plot.configure's or write_aplot_pdf's work */
void editaplot(xpp::Session &s, APLOT *ap);
void print_aplot(xpp::Session &s);
/* the array plot ready to be written: open, configured and (needs_data) with data to show, else why not (`where` names the command) */
xpp::Result<> aplot_ready(const xpp::Session &s, const std::string &where, bool needs_data = true);
/* the array plot as a PDF picture into `file` (its labels are the plot's xtitle, ytitle and bottom): false when
   replacing an existing file was declined */
xpp::Result<bool> write_aplot_pdf(xpp::Session &s, std::string_view file);
/* the next range sweep saves the array plot at each run: as one GIF movie (still false) or a GIF per run,
   `tag`ged with the swept value, from the file `file` names */
void begin_array_range(xpp::Session &s, bool still, bool tag, std::string_view file);
/* the time of the first and the last row the array plot shows (the page's range label, Print's ranges) */
struct AplotTimes { double first, last; };
AplotTimes aplot_times(const xpp::Session &s, const APLOT &ap);
/* splits an array plot's first column name at its trailing digits: "u10"
   gives "u" and 10, a name with no digits itself and 0 */
std::string get_root(std::string_view s, int *num);

/* the array plot (arrayplot.cpp), a Session's (session.h): its settings;
   range: the next range integration saves the array plot at each step
   (the array plot's range saving), integrate.cpp clears it when the range
   ends; the range saving's settings (Array range saving: range_count,
   still, tag, the first destination range_stem) and its open GIF file
   (json_windows.cpp writes it); Autoplot: redraw after each integration
   (auto_redraw) */
struct ArrayPlotState {
  APLOT plot{};
  int range = 0, range_count = 0, still = 1, tag = 0, auto_redraw = 0;
  xpp::Writer movie;
  bool save_cancelled = false; /* stop later frame saves after a declined/failed frame */
  std::string range_stem;
};
