#ifndef XPP_GROBS_H
#define XPP_GROBS_H

#include "xpp_types.h"
#include "xpplim.h"
#include "struct.h"
#include <string_view>

namespace xpp {
struct Session; /* session.h */

/* text labels and graphic objects (arrows, pointers, markers) on the
   plot windows; see grobs.cpp */

#define MAXLAB 50
#define MAXGROB 400

/* a graphic object's type: a pointer, an arrow, or a marker, MARKER plus
   its shape (the Markers menu's: box, diamond, triangle, plus, X, circle) */
constexpr int POINTER = 0, ARROW = 1, MARKER = 2;
constexpr int MARKER_SHAPE_COUNT = 6;

/* Text,etc's marker settings: the last ones the Marker and Markers
   dialogs gave (type, colour, size; the Markers' number, first row and
   skip) */
struct MarkInfo {
    int type = 2, color = 0;
    int number = 1, start = 0, skip = 1;
    double size = 1.0;
};

typedef struct {
  float xs,ys,xe,ye;
  double size;
  short use;
  XppWinId w;
  int type, color;
} GROB;

int select_marker_type(int *type);
int get_marker_info(Session &s);
int get_markers_info(Session &s);

/* The labels and graphic objects of the session s (Text,etc). */
/* a label at pixel (x, y) of the current window; its slot in s.labels, -1
   when full */
int add_label(Session &s, std::string_view text, int x, int y, int size, int font);
void draw_marker(Session &s, double x, double y, double size, int type);
void draw_grob(Session &s, int i);
void arrow_head(Session &s, double xs, double ys, double xe, double ye, double size);
/* forgets the text labels and graphic objects (Text,etc) of window w */
void destroy_labels_and_grobs(Session &s, XppWinId w);
void draw_label(Session &s, XppWinId w);
void add_grob(Session &s, double xs, double ys, double xe, double ye, double size, int type, int color);
void add_marker(Session &s);
void add_markers(Session &s);
void add_pntarr(Session &s, int type);
void edit_object_com(Session &s, int com);
void do_gr_objs_com(Session &s, int com);
void do_windows_com(Session &s, int c);
void set_restore(Session &s, int flag);

} // namespace xpp
#endif
