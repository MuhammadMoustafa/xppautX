#ifndef _load_eqn_h_
#define _load_eqn_h_

#include <stdio.h>
#include "xpplim.h" /* XPP_NAME_MAX */

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

/* 1 while an option may still be set: the command line and .xpprc set
   theirs first, and the ODE file's may not override them */
extern OptionsSet notAlreadySet;

void dump_torus(FILE *fp, int f);
void load_eqn(void);
void set_all_vals(void);
void read_defaults(FILE *fp);
void fil_flt(FILE *fpt, double *val);
void fil_int(FILE *fpt, int *val);

void add_intern_set(const char *name, const char *does);
void extract_action(const char *ptr);
void extract_internset(int j);
void do_intern_set(const char *name1, const char *value);
int msc(const char *s1, const char *s2);
void set_internopts(OptionsSet *mask);
void set_internopts_xpprc_and_comline(void);
void check_for_xpprc(void);
void stor_internopts(const char *s1);
/* an @ line of the model (form_ode.cpp's parser) kept in xpp::Model's
   options, which set_internopts applies; -1 (logged, not kept) when it
   sets dll_lib or dll_fun */
int add_model_option(const char *s1);
void set_option(const char *name, const char *s2, int force, OptionsSet *mask);

#define XPP_NSLIDERS 3

/* load_eqn.cpp's settings that have no other owner yet: the initial data
   the last run started from, the plot's axes and view, the boundary
   value solver's and the backward Euler's settings, and the command
   line's switches */
extern int IX_PLT[10],IY_PLT[10],IZ_PLT[10],NPltV,MultiWin,START_LINE_TYPE;
extern double X_LO[10],Y_LO[10],X_HI[10],Y_HI[10];
extern double x_3d[2],y_3d[2],z_3d[2];
extern int IXPLT,IYPLT,IZPLT,AXES,TIMPLOT;
extern double MY_XLO,MY_YLO,MY_XHI,MY_YHI;
extern double BVP_EPS,BVP_TOL,EulTol;
extern int BVP_MAXIT,BVP_FLAG,MaxEulIter;
extern int SHOOT,PAR_FOL;
extern int RunImmediately,got_file;
/* the integration's settings (the nUmerics menu, @ options) and the run's
   state: the number of equations, the time span and step, tolerances,
   the Poincare section, the torus, the flags the integrator keeps */
extern int PLOT_3D,INFLAG,STORFLAG,FOREVER,ENDSING,PAUSER,NULL_HERE;
extern int METHOD,NJMP,EVEC_ITER,NMESH,FFT,HIST;
extern double HMIN,HMAX,TOLER,ATOLER,BOUND,DELAY;
extern double EVEC_ERR,NEWT_ERR;
extern double TEND,DELTA_T,T0,TRANS;
extern int TORUS;
extern double TOR_PERIOD;
extern int POIMAP,POISGN,POIEXT,SOS,POIVAR;
extern double POIPLN;

#ifdef __cplusplus
}

#include <array>
#include <string>
/* The parameter sliders the ODE file sets up (@ s1=name, slo1=, shi1=,
   likewise 2 and 3); notAlreadySet.SLIDERn is 0 once slider n was set. */
struct XppSlider {
    std::string var;           /* the parameter it moves */
    double lo = 0.0, hi = 1.0; /* its range */
};
extern std::array<XppSlider,XPP_NSLIDERS> sliders;
/* the options file (the model's "options" line, else default.opt) that
   set_all_vals reads */
extern std::string options_file;
#endif
#endif
