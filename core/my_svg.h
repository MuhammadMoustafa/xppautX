
#ifndef _my_svg_h_
#define _my_svg_h_
#ifdef __cplusplus
extern "C" {
#endif


/* my_svg.cpp */
void svg_write(const char *str);
/* the y axis' label, rotated along the axis and level at (x, y) */
void svg_y_axis_label(int x, int y, const char *label);


#ifdef __cplusplus
}

#include "xpp_error.h"
#include "my_ps.h"

/* The SVG picture of the session s (image_format.h's row): svg_init opens
   filename, or the error when it cannot be written; the primitives
   graphics.cpp calls while it is drawn; svg_end closes it */
xpp::Result<> svg_init(xpp::Session &s, const char *filename, int color);
void svg_end(xpp::Session &s);
void svg_do_color(const PlotFileState &pf, int color);
void svg_bead(xpp::Session &s, int x, int y);
void svg_frect(xpp::Session &s, int x, int y, int w, int h);
void svg_line(xpp::Session &s, int xp1, int yp1, int xp2, int yp2);
void svg_linetype(xpp::Session &s, int linetype);
void svg_point(xpp::Session &s, int x, int y);
void special_put_text_svg(xpp::Session &s, int x, int y, const char *str, int size);
void svg_text(xpp::Session &s, int x, int y, const char *str);
#endif
#endif
