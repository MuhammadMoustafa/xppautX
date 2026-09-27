#ifndef _nullcline_h_
#define _nullcline_h_

#include "xpplim.h"

#ifdef __cplusplus
extern "C" {
#endif


void create_new_cline(void);
void froz_cline_stuff_com(int i);
/* the nullclines (current, or frozen number who) as segments: 4 floats each */
int get_nullcline_floats(float **v, int *n, int who, int type);
void redraw_dfield(void);
void direct_field_com(int c);
void restore_nullclines(void);
void new_clines_com(int c);
void do_batch_nclines(void);
void do_batch_dfield(void);
/* the -silent run's nullclines.dat and dirfields.dat */
void silent_nullclines(void);
void silent_dfields(void);

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

/* C++ linkage: xppautx_main.cpp declares it so itself */
void set_colorization_stuff(void);
#endif
#endif
