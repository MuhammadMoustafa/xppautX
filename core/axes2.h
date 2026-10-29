
#pragma once

#include <string>

namespace xpp {
struct Session; /* session.h */

/* axes2.cpp: the axes and tick labels of s's active plot window, and what it plots as text ("y vs x") */
std::string plot_title(const Session &s);
void do_axes(Session &s);
void Box_axis(Session &s, double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy,
              int flag);

} // namespace xpp
