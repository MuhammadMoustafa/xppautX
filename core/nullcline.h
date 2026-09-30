#ifndef _nullcline_h_
#define _nullcline_h_

#include "xpplim.h"

#ifdef __cplusplus
extern "C" {
#endif


/* the nullclines (current, or frozen number who) as segments: 4 floats each */
int get_nullcline_floats(float **v, int *n, int who, int type);

#ifdef __cplusplus
}

#include <string>
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

namespace xpp {
struct Session; /* session.h */
}

/* The nullclines and the direction field of the session s's active plot
   window: its commands, and redrawn with it */
void create_new_cline(xpp::Session &s);
void froz_cline_stuff_com(xpp::Session &s, int i);
void redraw_dfield(xpp::Session &s);
void direct_field_com(xpp::Session &s, int c);
void restore_nullclines(xpp::Session &s);
void new_clines_com(xpp::Session &s, int c);
void do_batch_nclines(xpp::Session &s);
void do_batch_dfield(xpp::Session &s);
/* the direction field the current window shows, one arrow a line (x y
   and the arrow's end), in PostScript's frame (-silent's dirfields.dat,
   the protocol's `dfield` `write`); an error message when it shows none */
void write_dfield(xpp::Session &s, const char *name);
/* the orbits' colouring (@ colorize=, colorvia=) of s in use */
void set_colorization_stuff(xpp::Session &s);
#endif
#endif
