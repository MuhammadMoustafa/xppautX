#ifndef XPP_GROBS_H
#define XPP_GROBS_H

#include "xpp_types.h"
#include "xpplim.h"
#include "struct.h"
#include <string>
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

/* What a text label looks like: its text and size (fancy_text_abs's 0 to
   LABEL_SIZE_MAX), its style and its colour (a curve colour, 0 the
   foreground). The page draws style and colour; the PostScript and SVG
   output draws text as XPPAUT's did. */
constexpr int LABEL_SIZE_MAX = 4;
constexpr int LABEL_SIZE_DEFAULT = 2; /* XPPAUT's Text dialog offered 2 */
constexpr int LABEL_STYLE_NORMAL = 0, LABEL_STYLE_BOLD = 1, LABEL_STYLE_ITALIC = 2, LABEL_STYLE_BOLD_ITALIC = 3;
constexpr int LABEL_STYLE_COUNT = 4;
constexpr int LABEL_FONT_COUNT = 2; /* LABEL::font: 0 the text font, 1 the symbol font */
constexpr std::size_t LABEL_TEXT_MAX = 1000; /* a session file may come from anyone: its text is bounded */
struct LabelLook {
    std::string text;
    int size = LABEL_SIZE_DEFAULT, style = LABEL_STYLE_NORMAL, color = 0;
};

/* what is wrong with a text, a size, a style or a colour, or an empty
   string; label_problem is the first of the four's. The one check of the
   command, the ask and the session file (which checks each at its line). */
std::string label_text_problem(std::string_view text);
std::string label_size_problem(int size);
std::string label_style_problem(int style);
std::string label_color_problem(int color);
std::string label_problem(const LabelLook &look);

/* The labels and graphic objects of the session s (Text,etc). */
/* a label with look at (x, y) in data coordinates, in plot window w;
   its slot in s.labels, -1 when full. A text starting with % is replaced
   once by its \{expr} filled in, as XPPAUT's Text dialog did. */
int add_label(Session &s, LabelLook look, XppWinId w, float x, float y);
/* label id changed: look, and where it is when move; `\{expr}` as add_label */
void change_label(Session &s, int id, LabelLook look, bool move, float x, float y);
void delete_label(Session &s, int id);
/* the text of slot id as drawn: its \{expr} filled in (once) */
std::string shown_label_text(Session &s, int id);
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
