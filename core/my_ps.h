#ifndef _my_ps_h_
#define _my_ps_h_
#ifdef __cplusplus
extern "C" {
#endif



void ps_stroke(void);
void ps_write(const char *str);
void ps_abs(int x, int y);
void ps_rel(int x, int y);


#ifdef __cplusplus
}

#include <stdio.h>
#include <string>

#include "xpp_error.h"

namespace xpp {
struct Session; /* session.h */
}

/* a picture file's state (my_ps.cpp, my_svg.cpp, shared with
   graphics.cpp), a Session's (session.h): which format is being written
   (plt_fmt_flag), in colour, the line count, the last point drawn and
   whether a line goes on from it, the PostScript font, its size and the
   line width (@ ps_font=, ps_fsize=, ps_lw=), and the SVG file open */
struct PlotFileState {
  int plt_fmt_flag = 0, ps_color_flag = 1, ps_lines = 0;
  int last_ps_x = 0, last_ps_y = 0, no_break_line = 0;
  int ps_font_size = 14;
  double ps_lw = 5;
  std::string ps_font = "Times-Roman";
  FILE *svgfile = nullptr;
};

/* The PostScript picture of the session s (image_format.h's row): ps_init
   opens filename (color: in colour), or the error when it cannot be
   written; the primitives graphics.cpp calls while it is drawn; ps_end
   closes it */
xpp::Result<> ps_init(xpp::Session &s, const char *filename, int color);
void ps_end(xpp::Session &s);
void ps_do_color(const PlotFileState &pf, int color);
void ps_bead(xpp::Session &s, int x, int y);
void ps_frect(xpp::Session &s, int x, int y, int w, int h);
void ps_line(xpp::Session &s, int xp1, int yp1, int xp2, int yp2);
void chk_ps_lines(xpp::Session &s);
void ps_linetype(xpp::Session &s, int linetype);
void ps_point(xpp::Session &s, int x, int y);
void ps_fnt(const xpp::Session &s, int cf, int scale);
void ps_show(xpp::Session &s, const char *str, int type);
void special_put_text_ps(xpp::Session &s, int x, int y, const char *str, int size);
void ps_text(xpp::Session &s, int x, int y, const char *str);
/* PostScript's own parameter dialog (BW/colour, portrait, axes font
   size, font, linewidth), asked before the file selector in
   graf_par.cpp's export_plot_picture (image_format.h's ask_params, W53,
   issue #101); 0 if the user cancelled */
int ps_ask_params(xpp::Session &s);
#endif
#endif
