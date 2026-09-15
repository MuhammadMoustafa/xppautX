#ifndef XPP_GROBS_H
#define XPP_GROBS_H

/* text labels and graphic objects (arrows, pointers, markers) on the
   plot windows; see grobs.c */
#include "xpp_types.h"
#include "xpplim.h"
#include "struct.h"

#define MAXLAB 50
#define MAXGROB 400

typedef struct {
  float xs,ys,xe,ye;
  double size;
  short use;
  XppWinId w;
  int type, color;
} GROB;

extern LABEL lb[MAXLAB];
extern GROB grob[MAXGROB];

void add_label(char *s, int x, int y, int size, int font);
void draw_marker(double x, double y, double size, int type);
void draw_grob(int i);
void arrow_head(double xs, double ys, double xe, double ye, double size);
void destroy_grob(XppWinId w);
void destroy_label(XppWinId w);
void draw_label(XppWinId w);
void add_grob(double xs, double ys, double xe, double ye, double size, int type, int color);
int select_marker_type(int *type);
int man_xy(float *xe, float *ye);
int get_marker_info(void);
int get_markers_info(void);
void add_marker(void);
void add_marker_old(void);
void add_markers(void);
void add_markers_old(void);
void add_pntarr(int type);
void edit_object_com(int com);
void do_gr_objs_com(int com);
void do_windows_com(int c);
void set_restore(int flag);
int is_col_plotted(int nc);

#endif
