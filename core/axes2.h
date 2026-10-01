
#ifndef _axes2_h_
#define _axes2_h_

namespace xpp {
struct Session; /* session.h */

/* axes2.cpp: the axes, tick labels and title of s's active plot window */
void re_title(Session &s);
void do_axes(Session &s);
void Box_axis(Session &s, double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy,
              int flag);

} // namespace xpp
#endif
