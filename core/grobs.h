#ifndef XPP_GROBS_H
#define XPP_GROBS_H

/* text labels and graphic objects (arrows, pointers, markers) on the
   plot windows; see grobs.cpp */
#include "xpp_types.h"
#include "xpplim.h"
#include "struct.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAXLAB 50
#define MAXGROB 400

typedef struct {
  float xs,ys,xe,ye;
  double size;
  short use;
  XppWinId w;
  int type, color;
} GROB;


int select_marker_type(int *type);
int get_marker_info(void);
int get_markers_info(void);

#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}

/* The labels and graphic objects of the session s (Text,etc). */
/* a label at pixel (x, y) of the current window; its slot in s.labels, -1
   when full */
int add_label(xpp::Session &s, const char *text, int x, int y, int size, int font);
void draw_marker(xpp::Session &s, double x, double y, double size, int type);
void draw_grob(xpp::Session &s, int i);
void arrow_head(xpp::Session &s, double xs, double ys, double xe, double ye, double size);
/* forgets the text labels and graphic objects (Text,etc) of window w */
void destroy_labels_and_grobs(xpp::Session &s, XppWinId w);
void draw_label(xpp::Session &s, XppWinId w);
void add_grob(xpp::Session &s, double xs, double ys, double xe, double ye, double size, int type, int color);
void add_marker(xpp::Session &s);
void add_markers(xpp::Session &s);
void add_pntarr(xpp::Session &s, int type);
void edit_object_com(xpp::Session &s, int com);
void do_gr_objs_com(xpp::Session &s, int com);
void do_windows_com(xpp::Session &s, int c);
void set_restore(xpp::Session &s, int flag);
#endif
#endif
