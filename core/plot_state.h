#pragma once
/* The state of the plot windows that a Session holds (session.h), a model's options give and a run
   reads (W733, #812): pure data, with no function of drawing. Each struct is the data part of the
   header of the plot module that works on it (many_pops.h, graf_par.h, graphics.h, nullcline.h,
   arrayplot.h, grobs.h, phase_data.h, marks_data.h), which includes this one: the Session, the
   loading of a model and the numerics below the plot need what a window holds, never what the plot
   does with it. */

#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "colormap.h"
#include "struct.h"
#include "xpp_types.h"

namespace xpp {

/* ---- many_pops.h's: the plot windows ---- */
/* The split view (W720; many_pops.h): the main area's graphs and autoX views in one group or two side by side (right) or one above the other (down). */
enum class SplitDirection { none, right, down };
/* the first group's share of the main area when a split starts: half and half */
inline constexpr double SPLIT_SHARE_DEFAULT = 0.5;
struct WindowSplit {
    SplitDirection direction = SplitDirection::none;
    double share = SPLIT_SHARE_DEFAULT;     /* the first group's share, 0..1 */
    std::array<std::string, 2> shown;       /* each group's shown window (split_upkeep keeps them true) */
};

/* The plot windows: every window's graph settings, the active one, and
   the windows a command draws on (all open ones under Simulplot). */
struct XppPlotWindows {
    std::vector<GRAPH> graph{1};   /* graph[i].Use: window i is open */
    GRAPH *current;        /* &graph[active] */
    int active;            /* the active window's index */
    int count;             /* how many are open */
    std::vector<int> open{0};      /* the open windows' indices in the tabs' order (move_window), count of them; the main graph's, 0, first */
    int simul;             /* Simulplot: draw on every open window */
    XppWinId draw_win;     /* graph[active].w, the window drawn into */
    WindowSplit split;     /* the split view's direction, share and shown windows */
};

/* whether each window that is neither a graph nor an autoX view (many_pops.h's DATA_WINDOW, ANIMATION_WINDOW and ARRAY_PLOT_WINDOW) is
   hidden (a window hidden keeps everything it holds) */
struct ToolWindowsHidden {
    bool data = false, animation = false, array_plot = false;
};


/* ---- graf_par.h's: the frozen curves and the 3D movie ---- */
/* an imported bifurcation diagram's curve (Freeze > Bif.Diag) */
struct BifCurve {
  std::vector<float> x,y;
  Color color;
};

/* The frozen curves of every plot window (Graphic stuff > Freeze) */
struct XppFrozenCurves {
    std::vector<CURVE> curve; /* .use: the slot holds one; .w: its window */
    int autox_freeze;     /* freeze the curve after every integration */
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
};

/* 3D Params' movie */
struct Mov3d {
  std::string angle="theta"; /* theta or phi, at most 19 characters */
  std::string yes="N";       /* at most 2 */
  double start=45;
  double incr=45;
  int nclip=7;
};

/* ---- graphics.h's: the drawing ---- */
/* the drawing state (graphics.cpp, axes2.cpp), a Session's (session.h):
   the drawing area in device units, its tick and character sizes, its
   world coordinates, the text's justification and the 3D view's angles;
   label unlabelled 2D axes with the plotted variables (front ends that ask) */
struct DrawingState {
  int d_left = 0, d_right = 0, d_top = 0, d_bottom = 0;
  int v_tic = 0, h_tic = 0, v_char = 0, h_char = 0;
  double x_min = 0, y_min = 0, x_max = 0, y_max = 0;
  int text_justify = 0;
  double theta0 = 45, phi0 = 45;
  int axis_var_labels = 0;
};

/* ---- nullcline.h's: the nullclines and the direction field ---- */
/* nullcline.cpp's settings, a Session's (session.h): a run's requested
   nullclines (nullclines.draw, nullclines.file) and direction field (@ dfdraw=), the nullclines'
   colors, the direction field's grid and kind (df_flag), set while it
   is drawn (for the SVG classes), and what colors an orbit
   (@ colorvia=, colorlo=, colorhi=, colorize=) */
struct NullclineSettings {
  bool draw = false;     /* compute them after a run */
  std::string file;      /* and save them there; empty: not saved */
  int df_batch = 0;
  int df_grid = 16, df_flag = 0;
  std::string color_via = "speed";
  double color_via_lo = 0, color_via_hi = 1;
  bool colorize_flag = false;
};

/* A frozen pair of nullclines: 4 floats (a segment) per point. */
inline constexpr std::size_t max_frozen_clines=10000; /* the most frozen pairs a session holds */

struct FrozenCline {
  std::vector<float> xn,yn;
  int nmx=0,nmy=0;
  int n_ix=-5,n_iy=-5;
};

/* what nullcline.cpp computed and keeps, a Session's (session.h) */
struct NullclineState {
  /* Range clines' dialog values */
  struct {
    std::string rv;
    int nstep=0;
    double xlo=0,xhi=0;
  } range;
  /* the current nullclines, as segments (4 floats each), the variables
     on their axes, and which one is being computed (1 x, 2 y) */
  std::vector<float> x_null,y_null;
  int num_x_n=0,num_y_n=0;
  int null_ix=0,null_iy=0,which_crv=0;
  /* fnull's state and rates, one per variable, kept between its many calls */
  std::vector<double> point,rates;
  /* the contour's two rows of the grid, and where new segments go */
  std::vector<float> n_top,n_bot;
  std::vector<float> *saver=nullptr;
  int num_index=0;
  /* the frozen nullclines; started is Freeze's first use (start_ncline) */
  std::vector<FrozenCline> frozen;
  bool frozen_started=false;
  int animation_delay_ms=200; /* pacing when browsing frozen nullcline frames */
  /* the direction field drawn last: its variables, and its arrows (1
     Direct Field's, all one length; 0 Scaled Dir.Fld's) */
  int df_ix=-1,df_iy=-1;
  int dfield_type=0;
};

/* ---- grobs.h's: the marker settings ---- */
/* Text,etc's marker settings: the last ones the Marker and Markers
   dialogs gave (type, color, size; the Markers' number, first row and
   skip) */
constexpr double MARKER_SIZE_DEFAULT = 1.0; /* XPPAUT's dialogs offered this, and grobs.h's ARROW_SIZE_DEFAULT */
struct MarkInfo {
    int type = 2;
    Color color;
    int number = 1, start = 0, skip = 1;
    double size = MARKER_SIZE_DEFAULT;
};

/* a graphic object (grobs.h: a pointer, an arrow or a marker) of plot window w */
struct GROB {
  float xs,ys,xe,ye;
  double size;
  int id;
  XppWinId w;
  int type;
  Color color;
};

/* ---- phase_data.h's and marks_data.h's: what each window shows ---- */
/* What each plot window shows of these, recorded since it was last blanked:
   a Session's (Session::phase_shown), which phase_data.cpp fills; what the
   client got of it is phase_data.cpp's own, the client's. */
struct PhaseShown {
    /* nullcline segments, 4 values each */
    struct Clines {
        std::vector<float> x, y;
    };
    struct Nullclines {
        int ix = 0, iy = 0;
        Color xcolor, ycolor;
        Clines now;
        std::vector<Clines> frozen;
    };
    /* one curve's trajectories of Flow */
    struct FlowCurve {
        Color color;
        std::vector<float> x, y; /* the trajectories one after the other, NaN between two */
        /* thinning: the last point kept, and the last one seen when it was not kept */
        float kx = 0, ky = 0, px = 0, py = 0;
        bool pending = false;
    };
    struct Field {
        int n = 0, scaled = 0;
        Color color;
        double du = 0, dv = 0;
        std::vector<float> grid;  /* x, y, ux, uy per arrow */
        std::vector<float> speed; /* one per arrow */
        std::vector<FlowCurve> flows; /* one per curve of the window */
    };
    struct Window {
        Nullclines nc;
        Field df;
        /* every change of nc, of df, bumps its generation: an update looks
           at a record again only when its generation moved */
        unsigned long nc_generation = 0, df_generation = 0;
        unsigned long trajectory = 0; /* the flow trajectory its last point was from */
    };
    std::vector<Window> windows{1};
    /* Flow is drawing (phase_data_flow_start to _stop), and the number of
       the trajectory it draws */
    bool flowing = false;
    unsigned long trajectory = 0;
};

/* What each plot window was drawn since it was last blanked: a Session's
   (Session::marks_shown), which marks_data.cpp fills; what the client got
   of it is marks_data.cpp's own, the client's. */
struct MarksShown {
    struct Equilibrium {
        double x, y;
        int symbol; /* eq_symb's: 0 box, 1 triangle, 3 circle */
        bool operator==(const Equilibrium &o) const
        {
            return std::memcmp(&x, &o.x, sizeof x) == 0 && std::memcmp(&y, &o.y, sizeof y) == 0 && symbol == o.symbol;
        }
    };
    struct Record {
        std::vector<Equilibrium> eqs;
        std::map<int, std::string> labels;    /* a label's id -> the text drawn */
        std::set<int> grobs;                  /* the ids of the graphic objects drawn */
        std::map<int, unsigned long> frozen;  /* frozen_curves.curve[] slot -> its generation */
    };
    std::vector<Record> windows{1};
    /* each frozen_curves.curve[] slot's generation, bumped when a curve is
       made in it (marks_data_frozen_new): a record keeps the generation it
       drew, so a new curve in the slot is not the one drawn */
    std::vector<unsigned long> generation;
    unsigned long generations = 0;
};

} // namespace xpp
