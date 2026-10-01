#ifndef _load_eqn_h_
#define _load_eqn_h_

#include <stdio.h>
#include "xpplim.h"

/*
The acutual max filename length is determined by the 
FILENAME_MAX (see <stdio.h>), and usually 4096 -- but
this is huge and usually overkill.  On the otherhand 
the old Xpp default string buffer size of 100 is a bit
restricitive for lengths of filenames. You could also 
set this define in the Makefile or at compile time to 
override the below definition.
*/

#ifndef XPP_MAX_NAME
#define XPP_MAX_NAME 300
#if (XPP_MAX_NAME > FILENAME_MAX)
	#undef XPP_MAX_NAME
	#define XPP_MAX_NAME FILENAME_MAX
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif

/*
Options are set accroding to an order of precedence

command line < mfile < .xpprc < default.opt

Add any options here that you might want to track.
*/
typedef struct {
   int BIG_FONT_NAME;
   int SMALL_FONT_NAME;
   int IXPLT;
   int IYPLT;
   int IZPLT;
   int AXES;
   int NMESH;
   int METHOD;
   int TIMEPLOT;
   int MAXSTOR;
   int TEND;
   int DT;
   int T0;
   int TRANS;
   int BOUND;
   int TOLER;
   int DELAY;
   int XLO;
   int XHI;
   int YLO;
   int YHI;  
   int UserBlack;
   int UserWhite;
   int UserMainWinColor;
   int UserDrawWinColor;
   int UserGradients;
   int UserBGBitmap;
   int UserMinWidth;
   int UserMinHeight;
   int YNullColor;
   int XNullColor;
   int StableManifoldColor;
   int UnstableManifoldColor;
   int START_LINE_TYPE;
   int RandSeed;
   int PaperWhite;
   int COLORMAP;
   int NPLOT;
   int XP;
   int YP;
   int ZP;
   int NOUT;
   int VMAXPTS;
   int TOR_PER;
   int JAC_EPS;
   int NEWT_TOL;
   int NEWT_ITER;
   int FOLD;
   int DTMIN;
   int DTMAX;
   int BANDUP;
   int BANDLO;
   int PHI;
   int THETA;
   int XMIN;
   int XMAX;
   int YMIN;
   int YMAX;
   int ZMIN;
   int ZMAX;
   int POIVAR;
   int OUTPUT;
   int POISGN;  
   int POIEXT;
   int POISTOP;
   int STOCH;
   int POIPLN;
   int POIMAP;
   int RANGEOVER;
   int RANGESTEP;
   int RANGELOW;
   int RANGEHIGH;
   int RANGERESET;
   int RANGEOLDIC;
   int RANGE;
   int NTST;
   int NMAX;
   int NPR;
   int NCOL;
   int DSMIN;
   int DSMAX;
   int DS;
   int PARMAX;
   int NORMMIN;
   int NORMMAX;
   int EPSL;
   int EPSU;
   int EPSS;
   int RUNNOW;
   int SEC;
   int UEC;
   int SPC;
   int UPC;
   int AUTOEVAL;
   int AUTOXMAX;
   int AUTOYMAX;
   int AUTOXMIN;
   int AUTOYMIN;
   int AUTOVAR;
   int PS_FONT;
   int PS_LW;   
   int PS_FSIZE;
   int PS_COLOR;
   int FOREVER;
   int BVP_TOL;
   int BVP_EPS;
   int BVP_MAXIT;
   int BVP_FLAG;
   int SOS;
   int FFT;
   int HIST;
   int PltFmtFlag;
   int ATOLER;
   int MaxEulIter;
   int EulTol;
   int EVEC_ITER;
   int EVEC_ERR;
   int NEWT_ERR;
   int NULL_HERE;
   int TUTORIAL;
   int SLIDER1;
   int SLIDER2;
   int SLIDER3;
   int SLIDER1LO;
   int SLIDER2LO;
   int SLIDER3LO;
   int SLIDER1HI;
   int SLIDER2HI;
   int SLIDER3HI;
  int POSTPROCESS;
  int HISTCOL;
  int HISTLO;
  int HISTHI;
  int HISTBINS;
  int SPECCOL;
  int SPECCOL2;
  int SPECWIDTH;
  int SPECWIN;
  int PLOTFORMAT;
  int DFGRID;
  int DFBATCH;
  int NCBATCH;
  int COLORVIA;
  int COLORIZE;
  int COLORLO;
  int COLORHI;
  int HISTCOL2;
  int HISTLO2;
  int HISTHI2;
  int HISTBINS2;

  } OptionsSet;

void fil_flt(FILE *fpt, double *val);
void fil_int(FILE *fpt, int *val);

int msc(const char *s1, const char *s2);
void check_for_xpprc(void);
void stor_internopts(const char *s1);

#define XPP_NSLIDERS 3

#ifdef __cplusplus
}

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
/* The parameter sliders the ODE file sets up (@ s1=name, slo1=, shi1=,
   likewise 2 and 3); notAlreadySet.SLIDERn is 0 once slider n was set. */
struct XppSlider {
    std::string var;           /* the parameter it moves */
    double lo = 0.0, hi = 1.0; /* its range */
};

/* The numerics settings in use (the nUmerics menu, the model's @ options,
   the command line), a Session's (session.h) */
struct NumericsSettings {
  /* the time span: from t0 for tend, step delta_t, every njmp-th step
     stored once |t| is past the transient trans */
  double t0 = 0, tend = 0, delta_t = 0, trans = 0;
  int njmp = 0;
  /* the method (numerics.cpp's numbers) and its step bounds, tolerances,
     the bound on the variables and the delays' maximum */
  int method = 0;
  double hmin = 0, hmax = 0, toler = 0, atoler = 0, bound = 0, delay = 0;
  /* the Volterra equations' memory, in points */
  int max_points = 0;
  /* equilibria: the Newton and eigenvector tolerances and iterations */
  double evec_err = 0, newt_err = 0;
  int evec_iter = 0;
  /* the boundary value solver */
  double bvp_eps = 0, bvp_tol = 0;
  int bvp_maxit = 0, bvp_flag = 0;
  /* the backward Euler's Newton solve */
  double eul_tol = 0;
  int max_eul_iter = 0;
  /* the nullclines' mesh */
  int nmesh = 0;
  /* the Poincare map: its kind, the section's variable, sign and plane,
     extrema, stop on section */
  int poimap = 0, poivar = 0, poisgn = 0, poiext = 0, sos = 0;
  double poipln = 0;
  /* the torus and its period (each variable's flag is Session::itor) */
  int torus = 0;
  double tor_period = 6.2831853071795864770;
  /* the random numbers' seed: rand_seed is the NEXT run's (shown/set by
     "@ seed=", Stochastic > New seed, -newseed; W71's "a seed per run"),
     applied and logged when that run starts (xpp_math.cpp nsrand48); the
     seed a run actually used is last_seed, empty before any run, for the
     protocol's state and a saved data file's header/metadata. Once used,
     rand_seed is redrawn from a separate seed stream seeded by it
     (xpp::next_seed), so an untouched field still gives fresh noise next
     time. */
  int rand_seed = 12345678;
  std::optional<int> last_seed;
  /* CVODE's banded Jacobian (the nUmerics menu's Stiff settings) */
  int cv_bandflag = 0, cv_bandupper = 1, cv_bandlower = 1;
  /* the global flags' crossing tolerance */
  double stol = 1.e-10;
  /* recompute the tables and kernels when a parameter changes
     (Numerics' AutoEval, @ autoeval=) */
  int auto_evaluate = 0;
  /* a run's state: data were stored (inflag), storing is on (storflag),
     integrate without end (forever), a range stops (endsing), pauses
     between runs (pauser), shoots (shoot) or follows its parameter
     (par_fol); the browser shows a Fourier transform (fft) or a
     histogram (hist); nullclines were computed (null_here) */
  int inflag = 0, storflag = 0, forever = 0, endsing = 0, pauser = 0;
  int shoot = 0, par_fol = 0, fft = 0, hist = 0, null_here = 0;
};

/* The main plot's settings in use (the model's @ options, the Viewaxes
   dialogs), a Session's (session.h): the plotted variables, the axes'
   kind, the 2D window and the 3D box, and the extra curves (@ xp2=,
   ..., npltv of them) with the windows @ nplot opens (multi_win) */
struct PlotSettings {
  int ixplt = 0, iyplt = 0, izplt = 0;
  int axes = 0, timplot = 0, plot_3d = 0;
  double my_xlo = 0, my_ylo = 0, my_xhi = 0, my_yhi = 0;
  std::array<double,2> x_3d{}, y_3d{}, z_3d{};
  std::array<int,10> ix_plt{}, iy_plt{}, iz_plt{};
  int npltv = 0, multi_win = 0;
  std::array<double,10> x_lo{}, y_lo{}, x_hi{}, y_hi{};
  int start_line_type = 1;
};

/* the name=value items of an @ line as set_internopts reads them, or
   (set) of an internal set's "$ ..." as extract_action does: each
   item without a name or a value left out (@ total = 1 sets nothing) */
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set);

namespace xpp {
struct Model;   /* model.h */
struct Session; /* session.h */
}

/* The load (xpp_batch.cpp) hands the Model and Session it builds to what
   reads the model and its options (W47d3): */
/* the model's file (Model::this_file) read into s: an .ode, an .odex, or
   one typed in when there is none */
void load_eqn(xpp::Session &s);
/* the settings the model and its options file leave unset, then the
   storage, the solver and the initial values the model gives */
void set_all_vals(xpp::Session &s);
/* the options file's settings, those still unset */
void read_defaults(xpp::Session &s, FILE *fp);
/* an @ line of the model (form_ode.cpp's parser) kept in m's options,
   which set_internopts applies; -1 (logged, not kept) when it sets
   dll_lib or dll_fun */
int add_model_option(xpp::Model &m, const char *s1);
/* the model's options not yet applied, then .xpprc's and the command
   line's; option name set to s2 (force: even when set already, mask:
   which may be set again) */
void set_internopts(xpp::Session &s, OptionsSet *mask);
void set_internopts_xpprc_and_comline(xpp::Session &s);
void set_option(xpp::Session &s, const char *name, const char *s2, int force, OptionsSet *mask);
/* the torus settings read from or written to a set file (lunch-new.cpp) */
void dump_torus(xpp::Session &s, FILE *fp, int f);
/* the model's internal sets: one added (set name {does}), one's
   "name=value ..." settings applied to s (extract_action for any such
   text, a comment's action), one setting */
void add_intern_set(xpp::Model &m, const char *name, const char *does);
void extract_action(xpp::Session &s, const char *ptr);
void extract_internset(xpp::Session &s, int j);
void do_intern_set(xpp::Session &s, const char *name1, const char *value);
/* File/cOpy set line (W67): the first of set1, set2, ... not yet a set of
   the model */
std::string intern_set_default_name(const xpp::Model &m);
/* why name cannot name a new set (not a name the parser reads, or already
   a set), "" when it can */
std::string intern_set_name_problem(const xpp::Model &m, std::string_view name);
/* `set name {p=v,...,x=v,...}`: every parameter and initial condition as
   it is now, numbers that read back exactly */
std::string intern_set_line(const xpp::Session &s, std::string_view name);
#endif
#endif
