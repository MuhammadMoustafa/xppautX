
#ifndef _my_svg_h_
#define _my_svg_h_

#include "xpp_error.h"
#include "my_ps.h"

namespace xpp {
struct Session; /* session.h */

/* my_svg.cpp */
void svg_write(PlotFileState &pf, const char *str);
/* the y axis' label, rotated along the axis and level at (x, y) */
void svg_y_axis_label(PlotFileState &pf, int x, int y, const char *label);

/* The SVG picture of the session s (image_format.h's row): svg_init opens
   filename, or the error when it cannot be written; the primitives
   graphics.cpp calls while it is drawn; svg_end closes it */
Result<> svg_init(Session &s, const char *filename, int color);
void svg_end(Session &s);
void svg_do_color(PlotFileState &pf, int color);
void svg_bead(Session &s, int x, int y);
void svg_frect(Session &s, int x, int y, int w, int h);
void svg_line(Session &s, int xp1, int yp1, int xp2, int yp2);
void svg_linetype(Session &s, int linetype);
void svg_point(Session &s, int x, int y);
void special_put_text_svg(Session &s, int x, int y, const char *str, int size);
void svg_text(Session &s, int x, int y, const char *str);

} // namespace xpp
#endif
