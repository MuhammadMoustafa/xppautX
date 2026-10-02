#ifndef _nullcline_h_
#define _nullcline_h_

#include "xpplim.h"
#include "xpp_io.h"
#include <string>
#include <string_view>
#include <vector>

namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */

/* the nullclines (current, or frozen number who) as segments: 4 floats each */
int get_nullcline_floats(Session &s, float **v, int *n, int who, int type);

/* nullcline.cpp's settings, a Session's (session.h): a batch run's
   nullclines and direction field (@ ncdraw=, dfdraw=), the nullclines'
   colours, the direction field's grid and kind (df_flag), set while it
   is drawn (for the SVG classes), and what colours an orbit
   (@ colorvia=, colorlo=, colorhi=, colorize=) */
struct NullclineSettings {
  int nc_batch = 0, df_batch = 0;
  int x_null_color = 2, y_null_color = 7;
  int df_grid = 16, df_flag = 0;
  int doing_dfield = 0;
  std::string color_via = "speed";
  double color_via_lo = 0, color_via_hi = 1;
  int colorize_flag = 0;
};

/* A frozen pair of nullclines: 4 floats (a segment) per point. */
struct FrozenCline {
  std::vector<float> xn,yn;
  int nmx=0,nmy=0;
  int n_ix=-5,n_iy=-5;
};

/* what nullcline.cpp computed and keeps, a Session's (session.h) */
struct NullclineState {
  /* Range clines' dialog values */
  struct {
    std::string rv;
    int nstep=0;
    double xlo=0,xhi=0;
  } range;
  /* the current nullclines, as segments (4 floats each), the variables
     on their axes, and which one is being computed (1 x, 2 y) */
  std::vector<float> x_null,y_null;
  int num_x_n=0,num_y_n=0;
  int null_ix=0,null_iy=0,which_crv=0;
  /* the contour's two rows of the grid, and where new segments go */
  std::vector<float> n_top,n_bot;
  std::vector<float> *saver=nullptr;
  int num_index=0;
  /* the frozen nullclines; started is Freeze's first use (start_ncline) */
  std::vector<FrozenCline> frozen;
  bool frozen_started=false;
  /* the direction field drawn last: its variables, and its arrows (1
     Direct Field's, all one length; 0 Scaled Dir.Fld's) */
  int df_ix=-1,df_iy=-1;
  int dfield_type=0;
};

/* The nullclines and the direction field of the session s's active plot
   window: its commands, and redrawn with it */
/* Saved segments are parsed whole before the session applies any member. */
std::string nullclines_text(const Session &s);
NullclineState read_nullclines(const Model &m, Lines &l);
void create_new_cline(Session &s);
void froz_cline_stuff_com(Session &s, int i);
void redraw_dfield(Session &s);
void direct_field_com(Session &s, int c);
void restore_nullclines(Session &s);
void new_clines_com(Session &s, int c);
void do_batch_nclines(Session &s);
void do_batch_dfield(Session &s);
/* the direction field the current window shows, one arrow a line (x y
   and the arrow's end), in PostScript's frame (--silent's dirfields.dat,
   the protocol's `dfield` `write`); an error message when it shows none */
void write_dfield(Session &s, std::string_view name);
/* the orbits' colouring (@ colorize=, colorvia=) of s in use */
void set_colorization_stuff(Session &s);

} // namespace xpp
#endif
