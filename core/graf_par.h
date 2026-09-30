#ifndef _graf_par_h_
#define _graf_par_h_


#include "xpplim.h"
#define RUBBOX 0

#define SCRNFMT 0
#define PSFMT 1
#define SVGFMT 2


#define REAL_SMALL 1.e-6

#define MAXBIFCRV 100
#define lmax(a,b) ((a) > (b) ? (a) : (b))

#include "xpp_types.h"
#include "struct.h" /* CURVE, MAXFRZ */
#ifdef __cplusplus
extern "C" {
#endif

/* graf_par.cpp: the colours' names and each one's palette index */
extern const char *const color_names[];
extern const int colorline[];

/* The frozen curves of every plot window (Graphic stuff > Freeze) */
typedef struct {
    CURVE curve[MAXFRZ]; /* .use: the slot holds one; .w: its window */
    int auto_freeze;     /* freeze the curve after every integration */
} XppFrozenCurves;


void check_val(double *x1, double *x2, double *xb, double *xd);
void pretty(double *x1, double *x2);
void change_cmap_com(int i);
void init_bd(void);

#ifdef __cplusplus
}

#include <string>

void dump_ps(xpp::Session &s, int i);

namespace xpp { struct DataTable; /* data_formats.h */ struct Session; /* session.h */ }
/* The plot windows' commands and views, on the session s: its active
   plot window (plot_windows.current) and the window drawn in (draw_win) */
void change_view_com(xpp::Session &s, int com);
void check_flags(xpp::Session &s);
void get_2d_view(xpp::Session &s, int ind);
void axes_opts(xpp::Session &s);
void get_3d_view(xpp::Session &s, int ind);
void corner_cube(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi);
void fit_window(xpp::Session &s);
void user_window(xpp::Session &s);
void xi_vs_t(xpp::Session &s);
void movie_rot(xpp::Session &s, double start, double increment, int nclip, int angle);
void get_3d_par_com(xpp::Session &s);
void update_view(xpp::Session &s, float xlo, float xhi, float ylo, float yhi);
void window_zoom_com(xpp::Session &s, int c);
void zoom_in(xpp::Session &s, int i1, int j1, int i2, int j2);
void zoom_out(xpp::Session &s, int i1, int j1, int i2, int j2);
void graph_all(xpp::Session &s, int *list, int n, int type);
int alter_curve(xpp::Session &s, const char *title, int in_it, int n);
void edit_curve(xpp::Session &s);
void new_curve(xpp::Session &s);
/* the main plot window's picture export, in image_format.h's
   IMAGE_FORMAT_PS or IMAGE_FORMAT_SVG (W53, issue #101) */
void export_plot_picture(xpp::Session &s, int fmt);
void default_window(xpp::Session &s);
void add_a_curve_com(xpp::Session &s, int c);
void export_graf_data(xpp::Session &s);
/* the frozen curves of s (Freeze) and their key */
void freeze_com(xpp::Session &s, int c);
void set_key(xpp::Session &s, int x, int y);
void draw_freeze_key(xpp::Session &s);
void key_frz_com(xpp::Session &s, int c);
void delete_frz_crv(xpp::Session &s, int i);
void kill_frz(xpp::Session &s);
int freeze_crv(xpp::Session &s, int ind);
/* a run's curve frozen when s freezes each run (Freeze > On freeze) */
void auto_freeze_it(xpp::Session &s);
int create_crv(xpp::Session &s, int ind);
void edit_frz_crv(xpp::Session &s, int i);
void draw_freeze(xpp::Session &s, XppWinId w);
int get_frz_index(xpp::Session &s, XppWinId w);
/* What the current plot window shows, for Save data (browse_data.cpp): its
   curves (numbered 1.. in order) and then its frozen curves, one row per
   point, as the columns curve, x, y and, in 3D, z */
xpp::DataTable plot_curves_table(const xpp::Session &s);

/* the plot file of a batch run (dump_ps): the model's file name, the
   internal set's (_name), then the run's number i (_0007) unless i < 0,
   and the plot format's extension (@ plotfmt=) */
std::string batch_plot_name(const xpp::Session &s, int i);
#include <vector>
/* frozen curve slot i as a session file restores it (xpp_session.cpp,
   W57): in window w, of grtype type (z's points only when type>0: 3D),
   its colour, key and name; false when the slot is in use or the points'
   counts differ */
bool restore_frozen_curve(xpp::Session &s, int i, XppWinId w, int type, int color, std::string key, std::string name,
                          std::vector<float> x, std::vector<float> y, std::vector<float> z);

/* How plots are written to files (PostScript, SVG) */
struct XppPlotExport {
    std::string format; /* a batch run's plot files: "ps" or "svg" (@ plotfmt=) */
    int color = 1;      /* in colour (1) or black and white */
};
#endif
#endif
