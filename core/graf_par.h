#ifndef _graf_par_h_
#define _graf_par_h_

#include "xpplim.h"
#include "xpp_types.h"
#include "struct.h" /* CURVE, MAXFRZ */
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {
struct Session; /* session.h */

#define RUBBOX 0

#define SCRNFMT 0
#define PSFMT 1
#define SVGFMT 2

#define REAL_SMALL 1.e-6

#define MAXBIFCRV 100
#define lmax(a,b) ((a) > (b) ? (a) : (b))

/* graf_par.cpp: the colours' names and each one's palette index */
extern const char *const color_names[];
extern const int colorline[];

/* an imported bifurcation diagram's curve (Freeze > Bif.Diag) */
struct BifCurve {
  std::vector<float> x,y;
  int color=0;
};

/* The frozen curves of every plot window (Graphic stuff > Freeze) */
typedef struct {
    CURVE curve[MAXFRZ]; /* .use: the slot holds one; .w: its window */
    int auto_freeze;     /* freeze the curve after every integration */
    /* their points: curve[i].xv/yv/zv point into points[i][0..2] (CURVE,
       struct.h, holds plain pointers) */
    std::array<std::array<std::vector<float>,3>,MAXFRZ> points;
    /* the key (Freeze > Key): where, and whether it is drawn */
    double key_x, key_y;
    int key_flag;
    /* an imported bifurcation diagram's curves (at most MAXBIFCRV) and
       its window */
    struct {
      std::vector<BifCurve> curves;
      XppWinId w;
    } bif_diagram;
} XppFrozenCurves;

/* 3D Params' movie */
struct Mov3d {
  std::string angle="theta"; /* theta or phi, at most 19 characters */
  std::string yes="N";       /* at most 2 */
  double start=45;
  double incr=45;
  int nclip=7;
};

void check_val(double *x1, double *x2, double *xb, double *xd);
void pretty(double *x1, double *x2);
void change_cmap_com(Session &s, int i);

void dump_ps(Session &s, int i);

struct DataTable; /* data_formats.h */
/* The plot windows' commands and views, on the session s: its active
   plot window (plot_windows.current) and the window drawn in (draw_win) */
void change_view_com(Session &s, int com);
void check_flags(Session &s);
void get_2d_view(Session &s, int ind);
void axes_opts(Session &s);
void get_3d_view(Session &s, int ind);
void corner_cube(Session &s, double *xlo, double *xhi, double *ylo, double *yhi);
void fit_window(Session &s);
void user_window(Session &s);
void xi_vs_t(Session &s);
void movie_rot(Session &s, double start, double increment, int nclip, int angle);
void get_3d_par_com(Session &s);
void update_view(Session &s, float xlo, float xhi, float ylo, float yhi);
void window_zoom_com(Session &s, int c);
void zoom_in(Session &s, int i1, int j1, int i2, int j2);
void zoom_out(Session &s, int i1, int j1, int i2, int j2);
void graph_all(Session &s, int *list, int n, int type);
int alter_curve(Session &s, std::string_view title, int in_it, int n);
void edit_curve(Session &s);
void new_curve(Session &s);
/* the main plot window's picture export, in image_format.h's
   IMAGE_FORMAT_PS or IMAGE_FORMAT_SVG (W53, issue #101) */
void export_plot_picture(Session &s, int fmt);
void default_window(Session &s);
void add_a_curve_com(Session &s, int c);
void export_graf_data(Session &s);
/* the frozen curves of s (Freeze) and their key */
void freeze_com(Session &s, int c);
void set_key(Session &s, int x, int y);
void draw_freeze_key(Session &s);
void key_frz_com(Session &s, int c);
void delete_frz_crv(Session &s, int i);
void kill_frz(Session &s);
int freeze_crv(Session &s, int ind);
/* a run's curve frozen when s freezes each run (Freeze > On freeze) */
void auto_freeze_it(Session &s);
int create_crv(Session &s, int ind);
void edit_frz_crv(Session &s, int i);
void draw_freeze(Session &s, XppWinId w);
int get_frz_index(Session &s, XppWinId w);
/* What the current plot window shows, for Save data (browse_data.cpp): its
   curves (numbered 1.. in order) and then its frozen curves, one row per
   point, as the columns curve, x, y and, in 3D, z */
DataTable plot_curves_table(const Session &s);

/* the plot file of a batch run (dump_ps): the model's file name, the
   internal set's (_name), then the run's number i (_0007) unless i < 0,
   and the plot format's extension (@ plotfmt=) */
std::string batch_plot_name(const Session &s, int i);
/* frozen curve slot i as a session file restores it (xpp_session.cpp,
   W57): in window w, of grtype type (z's points only when type>0: 3D),
   its colour, key and name; false when the slot is in use or the points'
   counts differ */
bool restore_frozen_curve(Session &s, int i, XppWinId w, int type, int color, std::string key, std::string name,
                          std::vector<float> x, std::vector<float> y, std::vector<float> z);

/* How plots are written to files (PostScript, SVG) */
struct XppPlotExport {
    std::string format; /* a batch run's plot files: "ps" or "svg" (@ plotfmt=) */
    int color = 1;      /* in colour (1) or black and white */
};

} // namespace xpp
#endif
