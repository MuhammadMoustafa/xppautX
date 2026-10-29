#pragma once

#include "xpp_error.h"
#include "xpp_types.h"
#include "struct.h" /* CURVE */
#include <array>
#include <optional>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {
struct Session; /* session.h */
struct XppPlotExport;

#define RUBBOX 0

#define REAL_SMALL 1.e-6

#define MAXBIFCRV 100
#define lmax(a,b) ((a) > (b) ? (a) : (b))

/* an imported bifurcation diagram's curve (Freeze > Bif.Diag) */
struct BifCurve {
  std::vector<float> x,y;
  Color color;
};

/* The frozen curves of every plot window (Graphic stuff > Freeze) */
typedef struct {
    std::vector<CURVE> curve; /* .use: the slot holds one; .w: its window */
    int auto_freeze;     /* freeze the curve after every integration */
    /* their points: curve[i].xv/yv/zv point into points[i][0..2] (CURVE,
       struct.h, holds plain pointers) */
    std::vector<std::array<std::vector<float>,plot_axes>> points;
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
/* the session's colour scale: the array plot and the direction field show it at once */
void set_colormap(Session &s, Colormap colormap);

void dump_ps(Session &s, int i);

struct DataTable; /* data_formats.h */
/* The plot windows' commands and views, on the session s: its active
   plot window (plot_windows.current) and the window drawn in (draw_win) */
void change_view_com(Session &s, int com);
/* The flags a graph's axes and grtype imply (ThreeDFlag, TimeFlag): the one
   rule, for the active window and for each extra window of a model */
void check_flags(GRAPH &g);
/* a graph of two different variables, neither time, in 2D: its nullclines
   and direction field can be computed */
bool phase_plane(const GRAPH &g);
/* The axes of the active plot window as the 2D and 3D view forms and plot.set_2d and set_3d have them: per axis
   its data column (find_variable's), limits and label, and the 3D view's window on the screen (xlo, xhi, ylo,
   yhi); the first two axes only when the plot is 2D */
struct AxesView {
  std::array<int,3> column;
  std::array<double,3> lo, hi;
  std::array<std::string,3> label;
  std::array<double,4> window;
};
/* curve ind's axes now */
AxesView axes_view(const Session &s, int ind);
/* the active window made a 2D or a 3D plot of these axes, checked whole first (each limit below its upper one,
   the screen window ordered): all or nothing; redrawn */
Result<> set_axes_view(Session &s, int ind, bool three, const AxesView &view);
/* A 2D guess sets two differential states; time and auxiliary axes cannot seed an equilibrium. */
bool phase_plane_states(const Session &s, const GRAPH &g);
/* The axes dialog of the active plot window: its curve ind's variables, limits and labels in
   columns (X, Y, and Z when three, with the 3D view's window and parameters), read whole and
   applied all or nothing; false when cancelled or when a field is refused (shown) */
bool axes_dialog(Session &s, int ind, bool three);
/* the 3D view's parameters, in 3D params and in the axes dialog of a 3D plot: the perspective flag, the
   z plane and view, the angles, and the movie (made when `movie`: `yes` is what the form's Movie answer
   was, `vary` theta or phi, the angle it starts at and steps by, and how many steps) */
struct ViewParams {
  int persp = 0;
  double zplane = 0, zview = 0, theta = 0, phi = 0;
  bool movie = false;
  std::string yes, vary;
  double start = 0, incr = 0;
  int nclip = 0;
};
/* the active window's and the session's movie parameters now (no movie asked for) */
ViewParams view_params(const Session &s);
/* the parameters applied to the active window: set, the movie if asked, the rotation made, redrawn; a movie
   varies theta or phi, nothing else */
Result<> set_view_params(Session &s, const ViewParams &params);
/* where the axes cross, whether each axis is drawn through there, and the font of the axes' text (a session
   setting, like a picture's) */
struct AxesOrigin {
  std::array<double,3> origin;
  std::array<bool,3> shown;
  double font_size;
};
AxesOrigin axes_origin(const Session &s);
/* the active window's axes as these (the font size from axes_font_size_min to _max), redrawn */
Result<> set_axes_origin(Session &s, const AxesOrigin &axes);
void axes_opts(Session &s);
void corner_cube(Session &s, double *xlo, double *xhi, double *ylo, double *yhi);
void fit_window(Session &s);
void user_window(Session &s);
void xi_vs_t(Session &s);
/* the active window plots the data column `name` against time, its view fitted to the data (or the run's
   time span before there is any) */
Result<> variable_vs_time(Session &s, std::string_view name);
void movie_rot(Session &s, double start, double increment, int nclip, int angle);
void get_3d_par_com(Session &s);
/* the active window's view set to these bounds (and its curves' ranges, for a 2D plot), redrawn */
void update_view(Session &s, double xlo, double xhi, double ylo, double yhi);
void window_zoom_com(Session &s, int c);
/* Window/zoom's In and Out on the active window, in plot coordinates: the view becomes the box x1..x2, y1..y2
   (x1 below x2, y1 below y2); Out makes the current view fill that box; Out by makes it `factor` times
   as wide and high about the point (cx, cy) */
void zoom_in(Session &s, double x1, double y1, double x2, double y2);
void zoom_out(Session &s, double x1, double y1, double x2, double y2);
void zoom_out_by(Session &s, double cx, double cy, double factor);
/* what a click with no drag does to Zoom Out: the view doubles about the point */
inline constexpr double ZOOM_OUT_CLICK_FACTOR = 2;
void graph_all(Session &s, int *list, int n, int type);
/* a width above 0, a marker size not below 0; the error is `command`'s */
Result<> check_line_style(std::string_view command, const LineStyle &style);
/* a curve's data columns and look: all the New and Edit curve forms and plot.add_curve and edit_curve set */
struct CurveSettings {
  int x = 0, y = 0, z = 0;
  Color color;
  LineStyle style;
};
/* curve `curve` of the active window now */
CurveSettings curve_settings(const Session &s, int curve);
/* the settings checked, set on curve `curve` of the active window and redrawn */
Result<> set_curve(Session &s, int curve, const CurveSettings &settings);
/* a curve added with these settings: its index */
Result<int> add_curve(Session &s, const CurveSettings &settings);
/* curve `curve` removed; the first one is the window's own and stays */
Result<> delete_curve(Session &s, int curve);
/* every curve but the first removed */
void delete_all_curves(Session &s);
/* the main plot window's picture in image_format.h's IMAGE_FORMAT_PDF or IMAGE_FORMAT_SVG (W53, issue
   #101) written to `filename`: false when the user declined to replace the file */
Result<bool> write_plot_picture(Session &s, int fmt, std::string_view filename, const XppPlotExport *given = nullptr);
void default_window(Session &s);
void add_a_curve_com(Session &s, int c);
void export_graf_data(Session &s);
/* the frozen curves of s (Freeze) and their key */
void freeze_com(Session &s, int c);
/* the key of the frozen curves (Freeze > Key): drawn or not, at the plot coordinates `at` when given; redrawn */
void set_frozen_legend(Session &s, bool visible, std::optional<std::pair<double,double>> at);
void draw_freeze_key(Session &s);
void key_frz_com(Session &s, int c);
void delete_frz_crv(Session &s, int i);
void kill_frz(Session &s);
/* a frozen curve's colour, line style, legend text and name (the Edit Freeze form's), and the most bytes the
   legend text and the name hold (Edit Freeze's field widths in XPPAUT) */
struct FrozenSettings {
  Color color;
  LineStyle style;
  std::string key, name;
};
inline constexpr std::size_t frozen_key_max = 19, frozen_name_max = 9;
FrozenSettings frozen_settings(const Session &s, int slot);
/* the style a width above 0 and a marker size not below 0, the key and the name not over their limits */
Result<> check_frozen_settings(const FrozenSettings &settings);
/* the settings checked, then set on the frozen curve in `slot`, redrawn */
Result<> set_frozen_curve(Session &s, int slot, const FrozenSettings &settings);
/* the error unless `slot` holds a frozen curve of window `w` */
Result<> frozen_slot_in_window(const Session &s, XppWinId w, int slot, std::string_view command);
/* the frozen curve in `slot` of window `w` deleted, redrawn */
Result<> delete_frozen(Session &s, XppWinId w, int slot);
/* the imported bifurcation diagram cleared, redrawn */
void clear_bifurcation(Session &s);
/* AUTO's Write pts file `filename` (six columns, checked whole first) shown in the active window as curves
   by kind: how many curves it made */
Result<int> import_bifurcation(Session &s, std::string_view filename);
/* a run's curve frozen when s freezes each run (Freeze > On freeze) */
void auto_freeze_it(Session &s);
/* curve `ind` of the active window frozen in its window: the slot it took */
Result<int> freeze_curve(Session &s, int ind);
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
bool restore_frozen_curve(Session &s, int i, XppWinId w, int type, const Color &color, const LineStyle &style, std::string key, std::string name,
                          std::vector<float> x, std::vector<float> y, std::vector<float> z);

/* How plots are written to files (PDF, SVG) */
struct XppPlotExport {
    std::string format = "pdf"; /* a batch run's plot files: "pdf" or "svg" (@ plotfmt=) */
    bool color = true;          /* in colour or black and white */
    double font_size = 12;      /* the axes' text, points */
};
/* the axes' font size, from the smallest to the largest the picture and Axes options forms and commands take */
inline constexpr double axes_font_size_min = 1, axes_font_size_max = 100;

} // namespace xpp
