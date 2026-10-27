#pragma once

#include "xpp_types.h"
#include "struct.h"
#include <algorithm>
#include <array>
#include <span>
#include <vector>
#include <string>
#include <string_view>

namespace xpp {
struct Session; /* session.h */

/* text labels and graphic objects (arrows, pointers, markers) on the
   plot windows; see grobs.cpp */

/* Every annotation (a text, an arrow, a pointer, a marker) has an id, one
   counter for all of them (Session::next_annotation_id): it stays the
   annotation's own for the session's life and its file, whatever is deleted
   around it. The lists keep their annotations in id order. */

/* the most texts, and the most arrows, pointers and markers, a session
   holds: a session file or a client may not make the lists grow without
   end (a trust boundary), and 100000 of either is far beyond a readable plot */
constexpr std::size_t ANNOTATION_LIST_MAX = 100000;

/* a graphic object's type: a pointer, an arrow, or a marker, MARKER plus
   its shape (the Markers menu's: box, diamond, triangle, plus, X, circle) */
constexpr int POINTER = 0, ARROW = 1, MARKER = 2;
constexpr int MARKER_SHAPE_COUNT = 6;
inline constexpr std::array<std::string_view, MARKER_SHAPE_COUNT> marker_shape_names{"box", "diamond", "triangle", "plus", "cross", "circle"};

/* Text,etc's marker settings: the last ones the Marker and Markers
   dialogs gave (type, colour, size; the Markers' number, first row and
   skip) */
constexpr double MARKER_SIZE_DEFAULT = 1.0, ARROW_SIZE_DEFAULT = .1; /* XPPAUT's dialogs offered these */
struct MarkInfo {
    int type = 2;
    Color color;
    int number = 1, start = 0, skip = 1;
    double size = MARKER_SIZE_DEFAULT;
};

typedef struct {
  float xs,ys,xe,ye;
  double size;
  int id;
  XppWinId w;
  int type;
  Color color;
} GROB;

/* the glyph of marker shape 0..MARKER_SHAPE_COUNT-1 (marker_shape_names): its
   polylines in glyph units, y up, centred on the marker's position, a unit
   being 1/MARKER_HALF_UNITS of the half size at size 1. The pictures draw
   them in data units, the page in pixels (hello's annotation_glyphs). */
constexpr double MARKER_HALF_UNITS = 6;
std::span<const std::span<const std::array<double, 2>>> marker_strokes(int shape);
/* an arrow head's two strokes are as wide, either side, as this part of
   their length (arrow_head) */
constexpr double ARROW_HEAD_SPREAD = .5;
/* hello's `annotation_glyphs`: both of the above, for the page to draw */
std::string annotation_glyphs_json();

/* the annotation with this id in a list kept in id order, or null */
template <typename List>
auto find_annotation(List &list, int id) -> decltype(list.data())
{
    const auto at = std::lower_bound(list.begin(), list.end(), id, [](const auto &a, int want) { return a.id < want; });
    return at != list.end() && at->id == id ? &*at : nullptr;
}

int select_marker_type(int *type);
int get_marker_info(Session &s);
int get_markers_info(Session &s);

/* What a text label looks like: its text and size (fancy_text_abs's 0 to
   LABEL_SIZE_MAX), its style and its colour. The page draws style and
   colour; the PDF and SVG output draw the size and colour (not the style). */
constexpr int LABEL_SIZE_MAX = 4;
constexpr int LABEL_SIZE_DEFAULT = 2; /* XPPAUT's Text dialog offered 2 */
constexpr int LABEL_STYLE_NORMAL = 0, LABEL_STYLE_BOLD = 1, LABEL_STYLE_ITALIC = 2, LABEL_STYLE_BOLD_ITALIC = 3;
constexpr int LABEL_STYLE_COUNT = 4;
constexpr int LABEL_FONT_COUNT = 2; /* LABEL::font: 0 the text font, 1 the symbol font */
constexpr std::size_t LABEL_TEXT_MAX = 1000; /* a session file may come from anyone: its text is bounded */
struct LabelLook {
    std::string text;
    int size = LABEL_SIZE_DEFAULT, style = LABEL_STYLE_NORMAL;
    Color color = foreground_color;
};

/* what is wrong with a text, a size or a style, or an empty
   string; label_problem is the first of the three's. The one check of the
   command, the ask and the session file (which checks each at its line). */
std::string label_text_problem(std::string_view text);
std::string label_size_problem(int size);
std::string label_style_problem(int style);
std::string label_problem(const LabelLook &look);

/* The labels and graphic objects of the session s (Text,etc). */
/* a label with look at (x, y) in data coordinates, in plot window w; its
   id, -1 when the list is full (ANNOTATION_LIST_MAX). A text starting with
   % is replaced once by its \{expr} filled in, as XPPAUT's Text dialog did. */
int add_label(Session &s, LabelLook look, XppWinId w, float x, float y);
/* a graphic object in plot window w (type POINTER, ARROW or MARKER plus a
   shape; a marker has no end); its id, -1 when the list is full */
int add_grob(Session &s, XppWinId w, double xs, double ys, double xe, double ye, double size, int type, const Color &color);
/* what Edit/Change gives a graphic object: its size and colour, and for a
   marker its shape */
struct GrobLook {
    int type;
    double size;
    Color color;
};
/* the label or object with this id changed to look (`\{expr}` as add_label),
   moved so its position (an object's start, its end following) is (x, y), or
   deleted; false when no such annotation, or not of that kind, exists */
bool edit_label(Session &s, int id, LabelLook look);
bool edit_grob(Session &s, int id, const GrobLook &look);
bool move_annotation(Session &s, int id, double x, double y);
bool delete_annotation(Session &s, int id);
/* the text of l as drawn: its \{expr} filled in (once) */
std::string shown_label_text(Session &s, const LABEL &l);
void draw_marker(Session &s, double x, double y, double size, int type);
void draw_grob(Session &s, const GROB &g);
void arrow_head(Session &s, double xs, double ys, double xe, double ye, double size);
/* forgets the text labels and graphic objects (Text,etc) of window w */
void destroy_labels_and_grobs(Session &s, XppWinId w);
void draw_label(Session &s, XppWinId w);
void add_marker(Session &s);
void add_markers(Session &s);
void add_pntarr(Session &s, int type);
void edit_object_com(Session &s, int com);
void do_gr_objs_com(Session &s, int com);
void do_windows_com(Session &s, int c);
void set_restore(Session &s, int flag);

} // namespace xpp
