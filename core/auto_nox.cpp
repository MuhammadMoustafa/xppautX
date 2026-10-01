#include "model.h"
#include "session.h"
#include "autox.h"
#include "snapx.h"
#include "xpp_session.h"
#include "model_switch.h"
#include "integrate.h"
#include "storage.h"
#include "form_ode.h"
#include "xpp_log.h"
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <string_view>
#include <limits>
#include "numerics.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include <string.h>
#include "autevd.h"
#include "auto_stop.h"
#include "auto_stability.h"
#include "csv_export.h"
#include "image_format.h"
#include <libgen.h>
#include "graf_par.h"


#include "pp_shoot.h"

#include "xpp_files.h"
#include "load_eqn.h"


#include "expr.h"

#include "diagram.h"
#include "browse.h"

#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "axes2.h"
#include "graphics.h"

#include "xpp_job.h"
#include "auto_data.h"
#include "derived.h"   /* evaluate_derived() */
#include "tabular.h"   /* redo_all_fun_tables() */
#include "getvar.h"    /* setvar() */
#include "my_rhs.h"    /* extra() */
#include "menus.h"
#include "my_ps.h"
#include "solver.h"

#define PARAM_BOX 1

#define RUBBOX 0

#define ESC 27

#define UPT 6
#define SPT 7

#define OPEN_3 1
#define NO_OPEN_3 0
#define OVERWRITE 0
#define APPEND 1

/* calculation types */

#define LPE2 1
#define LPP2 2
#define HB2 3
#define TR2 4
#define BR2 5
#define PD2 6
#define FP2 7
#define BV1 8
#define EQ1 9
#define PE1 10
#define DI1 11
#define HO2 12

#define HI_P 0  /* uhi vs par */
#define NR_P 1  /* norm vs par */
#define HL_P 2  /* Hi and Lo vs par  periodic only */
#define PE_P 3  /* period vs par   */
#define P_P  4  /* param vs param  */

#define FR_P 10  /* freq vs par   */
#define AV_P 11 /* ubar vs par */
#define SPER 3
#define UPER 4
#define CSEQ 1
#define CUEQ 2

/* the label the running continuation started from (Auto.irs), for its
   first point: do_auto sets it, addbif takes it (auto_run_from_take) */
static int run_from;


namespace {
/* the diagram's marked stretch (the S and E keys in the Grab loop): the
   branch and point numbers of its start and end, and where they are drawn */
struct DiagramMark {
    int state = 0;        /* 0 nothing, 1 start marked, 2 start and end */
    int start_branch = 0, end_branch = 0;
    int start_point = 0, end_point = 0;
};
DiagramMark diagram_mark;
int auto_redraw = 1; /* AUTO's File menu Redraw toggle: only reported */
} // namespace
/*  two parameter colors  need to do this
    LP is 20 (red)
    HB  is  28 blue
    TR  is  26 (green) 
    PD  is 24  (orange)
    BR  is  27 (turquoise)
    FP  is 25  (olive)
*/

constexpr int LPP_color=0;
constexpr int LPE_color=20;
constexpr int HB_color=28;
constexpr int TR_color=26;
constexpr int PD_color=23;
constexpr int BR_color=27;
constexpr int FP_color=25;


static int load_all_labeled_orbits=0;


int go_go_auto(xpp::Session &s); /* gogoauto.cpp: AUTO's run, in no header */

static GRABPT grabpt;



static std::string this_auto_file;
/* AUTO's unit files under its folder (open_auto): fort.3 the restart
   data, fort.7 the branches, fort.8 the solutions, fort.9 the diagnostics */
namespace {
std::string fort3,fort7,fort8,fort9;
}

static unsigned int DONT_XORCross=0;

static double XfromAuto,YfromAuto;
static int FromAutoFlag=0;

/* AUTO's continuation parameters back into the model (find_point, the
   Grab loop's Return): a diverged run's stored point can hold a
   non-finite value (QA SCI-001, -nan in the "state" event's pars). Skip
   it and keep the model's previous value instead, once per parameter. */
static void auto_set_pars_from(xpp::Session &s, const double *par)
{
    for (int i = 0; i < s.auto_state.npar; i++) {
        const int idx = s.auto_state.par_index[i];
        if (std::isfinite(par[i])) s.parser.constants[idx] = par[i];
        else
            xpp::log(XPP_LOG_WARN, "AUTO: {} from the diagram is not finite; keeping {:.16g}",
                     s.model().upar_names[s.auto_state.par[i]], s.parser.constants[idx]);
    }
}

/* do_auto's cleanup after go_go_auto(): a run that diverges can leave
   AUTO's own working copy of a continuation parameter (autpp.cpp's func,
   synced into constants[] every RHS call) not finite. Restore what the
   run started from instead of leaving -nan in the model (QA SCI-001). */
static void auto_restore_finite_pars(xpp::Session &s, const double *before)
{
    for (int i = 0; i < s.auto_state.npar; i++) {
        const int idx = s.auto_state.par_index[i];
        if (!std::isfinite(s.parser.constants[idx])) {
            xpp::log(XPP_LOG_WARN, "AUTO: the run left {} not finite; keeping {:.16g}",
                     s.model().upar_names[s.auto_state.par[i]], before[i]);
            s.parser.constants[idx] = before[i];
        }
    }
}

static double HOMO_SHIFT=0.0;



static AUTOAX Old1p;
static AUTOAX Old2p;

/* color plot stuff */
void colset(xpp::Session &s, int type )
{
  switch(type) {
  case CSEQ:
    autocol(s.auto_state.stable_eq_color);
    break;
 case CUEQ:
    autocol(s.auto_state.unstable_eq_color);
    break;
 case SPER:
    autocol(s.auto_state.stable_po_color);
    break;
 case UPER:
    autocol(s.auto_state.unstable_po_color);
    break;
  }
  
}

void pscolset2(xpp::Session &s, int flag2)
{
   switch(flag2){
  case LPE2:
    set_linestyle(s,LPE_color-19);
    break;
  case LPP2:
    set_linestyle(s,LPP_color);
    break;
  case HB2:
    set_linestyle(s,HB_color-19);
    break;
  case TR2:
    set_linestyle(s,TR_color-19);
    break;
  case BR2:
    set_linestyle(s,BR_color-19);
    break;
  case PD2:
    set_linestyle(s,PD_color-19);
    break;
  case FP2:
     set_linestyle(s,FP_color-19);
    break;
  default:
    set_linestyle(s,0);
  }

}
void colset2(int flag2)
{
  LineWidth(2);
  switch(flag2){
  case LPE2:
    autocol(LPE_color);
    break;
  case LPP2:
    autocol(LPP_color);
    break;
  case HB2:
    autocol(HB_color);
    break;
  case TR2:
    autocol(TR_color);
    break;
  case BR2:
    autocol(BR_color);
    break;
  case PD2:
    autocol(PD_color);
    break;
  case FP2:
     autocol(FP_color);
    break;
  default:
    autocol(0);
  }

}

void storeautopoint(xpp::Session &s, double x,double y)
{
  if(s.auto_state.axes().plot==P_P){
    XfromAuto=x;
    YfromAuto=y;
    FromAutoFlag=1;
  }
}
void setautopoint(xpp::Session &s)
{
  if(FromAutoFlag)
    {
      FromAutoFlag=0;
      xpp::set_val(s,s.model().upar_names[s.auto_state.par[s.auto_state.axes().icp1]],XfromAuto);
      xpp::set_val(s,s.model().upar_names[s.auto_state.par[s.auto_state.axes().icp2]],YfromAuto);
      xpp::evaluate_derived(s);
      xpp::ok_or_show(xpp::redo_all_fun_tables(s));
      redraw_params();
    }
}
      
namespace {
/* AUTO's parameter k's name, or "" (auto_par_name) */
std::string par_label(const xpp::Session &s, int k)
{
  const char *p=auto_par_name(s, k);
  return p?p:"";
}

/* the diagram's axis labels: its first parameter, and what Auto.plot
   shows up the side */
struct AxisLabels {
  std::string x,y;
};
AxisLabels axis_labels(const xpp::Session &s, const AUTOAX &ax)
{
  AxisLabels l;
  l.x=par_label(s, ax.icp1);
  switch(ax.plot){
  case HI_P:
  case HL_P:
    l.y=s.model().uvar_names[ax.var];
    break;
  case NR_P:
    l.y="Norm";
    break;
  case PE_P:
    l.y="Period";
    break;
  case FR_P:
    l.y="Frequency";
    break;
  case P_P:
    l.y=par_label(s, ax.icp2);
    break;
  case AV_P:
    l.y=s.model().uvar_names[ax.var]+"_bar";
    break;
  }
  return l;
}
} // namespace

void get_auto_str(const xpp::Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel)
{
  AxisLabels l=axis_labels(s, ax);
  xlabel=std::move(l.x);
  ylabel=std::move(l.y);
}

/* the diagram's axes in a PostScript or SVG export (diagram.cpp
   export_diagram), whichever ps_init/svg_init began */
void draw_export_axes(xpp::Session &s)
{
 set_scale(s,s.auto_state.axes().xmin,s.auto_state.axes().ymin,s.auto_state.axes().xmax,s.auto_state.axes().ymax);
 const AxisLabels l=axis_labels(s, s.auto_state.axes());
 Box_axis(s,s.auto_state.axes().xmin,s.auto_state.axes().xmax,s.auto_state.axes().ymin,s.auto_state.axes().ymax,l.x.c_str(),l.y.c_str(),0);
}

void draw_bif_axes(xpp::Session &s)
{
 int x0=s.auto_state.bifur.x0,y0=s.auto_state.bifur.y0,ii,i0;
 int x1=x0+s.auto_state.bifur.wid,y1=y0+s.auto_state.bifur.hgt;
 std::string junk;
 clear_auto_plot();
 ALINE(x0,y0,x1,y0);
 ALINE(x1,y0,x1,y1);
 ALINE(x1,y1,x0,y1);
 ALINE(x0,y1,x0,y0);
 junk=xpp::format("{:g}",s.auto_state.axes().xmin);
 ATEXT(x0,y1+text_metrics.small_height+2,junk.c_str());
 junk=xpp::format("{:g}",s.auto_state.axes().xmax);
 ii=static_cast<int>(junk.size())*text_metrics.small_width;
 ATEXT(x1-ii,y1+text_metrics.small_height+2,junk.c_str());
 junk=xpp::format("{:g}",s.auto_state.axes().ymin);
 ii=static_cast<int>(junk.size());
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y1,junk.c_str());
 junk=xpp::format("{:g}",s.auto_state.axes().ymax);
 ii=static_cast<int>(junk.size());
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y0+text_metrics.small_height,junk.c_str());
 const AxisLabels l=axis_labels(s, s.auto_state.axes());
 ATEXT((x0+x1)/2,y1+text_metrics.small_height+2,l.x.c_str());
 ATEXT(10*text_metrics.small_width,text_metrics.small_height,l.y.c_str());
 /* the data of the diagram starts again too, in every view */
 for(int v=0;v<static_cast<int>(s.auto_state.views.size());v++)auto_diagram(s,v,NULL);
 refreshdisplay();
}

int IXVal(const xpp::Session &s, double x)
{
  double temp=static_cast<double>(s.auto_state.bifur.wid)*(x-s.auto_state.axes().xmin)/(s.auto_state.axes().xmax-s.auto_state.axes().xmin);
  return (static_cast<int>(temp)+s.auto_state.bifur.x0);
}

int IYVal(const xpp::Session &s, double y)
{
  double temp=static_cast<double>(s.auto_state.bifur.hgt)*(y-s.auto_state.axes().ymin)/(s.auto_state.axes().ymax-s.auto_state.axes().ymin);
  return(s.auto_state.bifur.hgt-static_cast<int>(temp)+s.auto_state.bifur.y0);
}

int chk_auto_bnds(const xpp::Session &s, int ix,int iy)
{
  int x1=s.auto_state.bifur.x0,x2=s.auto_state.bifur.x0+s.auto_state.bifur.wid;
  int y1=s.auto_state.bifur.y0,y2=s.auto_state.bifur.y0+s.auto_state.bifur.hgt;
  if((ix>=x1)&&(ix<x2)&&(iy>=y1)&&(iy<y2))return 1;
  return 0;
}
void close_auto(xpp::Session &s, int flg) /* labels compatible with A2K  */
{
  /* Close fp8 before the renames below: Windows refuses rename()/remove()
     on a file that is still open (see xpp::files::move), which left
     fort.8 behind next to <model>.s with the handle leaked. Linux allows
     renaming/removing an open file, which is likely why this was never
     turned on upstream -- it was dead code there, not a deliberate
     no-op. */
  s.auto_lib.fp8.reset();
  if(flg==0) {/*Overwrite*/
    xpp::files::move(fort7.c_str(),(this_auto_file+".b").c_str());
    xpp::files::move(fort9.c_str(),(this_auto_file+".d").c_str());
    xpp::files::move(fort8.c_str(),(this_auto_file+".s").c_str());
  }
  else {/*APPEND*/
    xpp::files::prepend(fort7.c_str(),(this_auto_file+".b").c_str());
    xpp::files::prepend(fort9.c_str(),(this_auto_file+".d").c_str());
    xpp::files::prepend(fort8.c_str(),(this_auto_file+".s").c_str());
  }

    xpp::files::remove(fort8.c_str());

    xpp::files::remove(fort7.c_str());
    xpp::files::remove(fort9.c_str());
    xpp::files::remove(fort3.c_str());

}

/* AUTO writes fort.3/7/8/9 under HOME. A HOME that is set but unusable
   (missing, not writable) must fall back to the model's directory like an
   unset one, or the opens fail deep inside autlib1.c. */
static const char *auto_home_dir(xpp::Session &s, char *dname)
{
  const char *home;

  /* xppautX gives each session its own directory (xpp_globals.h) */
  if (!s.auto_state.dir.empty())
    return s.auto_state.dir.c_str();

  home = getenv("HOME");
  if (home == NULL || !xpp::files::dir_writable(home))
    home = dname;
  return home;
}

void create_auto_file_name(xpp::Session &s)
{
  /* basename()/dirname() may write into their argument or return a
     pointer into it, so each needs its own writable, NUL-terminated
     copy of s.model().this_file (std::string::data() is both since C++17) */
  std::string basec = s.model().this_file, dirc = s.model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* HOME = auto_home_dir(s, dname);

  this_auto_file=xpp::format("{}/{}",HOME,bname);
}

void open_auto(xpp::Session &s, int flg) /* compatible with new auto */
{
  std::string basec = s.model().this_file, dirc = s.model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* HOME = auto_home_dir(s, dname);

  this_auto_file=xpp::format("{}/{}",HOME,bname);
  fort3=xpp::format("{}/fort.3",HOME);
  fort7=xpp::format("{}/fort.7",HOME);
  fort8=xpp::format("{}/fort.8",HOME);
  fort9=xpp::format("{}/fort.9",HOME);

  if(flg==1){
    xpp::files::copy((this_auto_file+".s").c_str(),fort3.c_str());
  }

}

const char *auto_fort_path(int unit)
{
  switch(unit){
  case 3: return fort3.c_str();
  case 7: return fort7.c_str();
  case 8: return fort8.c_str();
  case 9: return fort9.c_str();
  default: return "";
  }
}

/* what a run continues, for auto_stability.h */
static int run_stability_kind(xpp::Session &s)
{
  if(s.auto_state.two_param!=0||s.auto_state.bifur.isw==2)return AUTO_STABILITY_OTHER;
  if(s.auto_state.bifur.ips==2)return AUTO_STABILITY_PERIODIC;
  if(s.auto_state.bifur.ips==1||s.auto_state.bifur.ips==-1)return AUTO_STABILITY_STEADY;
  return AUTO_STABILITY_OTHER;
}

/* what a stored point is, for auto_stability.h: one-parameter periodic
   orbits have a negative branch (autlib1.cpp stplbv) */
static int point_stability_kind(const DIAGRAM *d)
{
  if(d->flag2!=0)return AUTO_STABILITY_OTHER;
  return d->ibr<0?AUTO_STABILITY_PERIODIC:AUTO_STABILITY_STEADY;
}

/* a run starts from Auto.irs's label, or from initial data: its first
   point takes the label's stored stability when it is the label's own
   solution, else it is not computed (auto_stability.h) */
static void stability_run_start(xpp::Session &s)
{
  const DIAGRAM *d=s.auto_state.bifur.irs>0?diagram_of_label(s,s.auto_state.bifur.irs):NULL;
  if(d==NULL){
    auto_stability_run_start(run_stability_kind(s),s.auto_state.bifur.isw,AUTO_STABILITY_NONE,0,0,NULL,NULL);
    return;
  }
  auto_stability_run_start(run_stability_kind(s),s.auto_state.bifur.isw,point_stability_kind(d),d->itp,s.model().node,d->evr,d->evi);
}

/* MAIN Running routine  Assumes that Auto structure is set up */
namespace {
int auto_depth; /* do_auto's own follow-up runs (RestartLabel) are one run */
/* one do_auto call's depth, however it ends */
struct AutoDepth {
  AutoDepth() { if(auto_depth++==0)auto_stop_clear(); } /* xppautX: T23: why this run's branches end */
  ~AutoDepth() { auto_depth--; }
  AutoDepth(const AutoDepth &)=delete;
  AutoDepth &operator=(const AutoDepth &)=delete;
};
}

void do_auto(xpp::Session &s, int iold, int isave, int itp)
{
      redraw_auto_menus();
      
    set_auto(s); /* this sets up all the continuation initialization 
                   it is equivalent to reading in auto parameters
                   and running init in auto 
		*/
 
    open_auto(s, iold); /* this copies the relevant files .s  to fort.3 */
    const AutoDepth depth;
    std::string failed; /* why the run failed (xpp::AutoFailed), empty if it did not */
    {
      const xpp::Job job; /* Abort cancels it (xpp_job.h) */
      run_from=s.auto_state.bifur.irs>0?s.auto_state.bifur.irs:0; /* the diagram's data say where the run started */
      stability_run_start(s); /* what its first point's stability is (auto_stability.h) */
      {
          xpp::Computation computing; /* what Abort stops (xpp_job.h) */
          std::array<double, 8> before{}; /* AutoPar's size */
          for (int i = 0; i < s.auto_state.npar; i++) before[i] = s.parser.constants[s.auto_state.par_index[i]];
          try {
              go_go_auto(s); /* this complets the initialization and calls the
                                main routines
                             */
          } catch (const xpp::AutoFailed &e) {
              /* W63a: what the numerics used to exit() on ends the run
                 instead, as a cancel does (auto_state.h) */
              failed = e.what;
          }
          auto_restore_finite_pars(s, before.data()); /* leave no NaN parameter behind (QA SCI-001) */
      }
      run_from=0;
      if(xpp_job_cancelled()||!failed.empty())s.auto_state.restart_label=0; /* xppautX: cancel: no follow-up run */
      if(!failed.empty())s.auto_lib.restart_flag=0;
    }
    /*     run_aut(Auto.nfpar,itp); THIS WILL CHANGE TO gogoauto stuff */ 
    close_auto(s, isave); /* this copies fort.8 to the .s file and other 
                          irrelevant stuff 
		       */
    if(!failed.empty()){
      xpp::log_auto("AUTO stopped: {}\n",failed); /* the AUTO window's Output */
      err_msg(("AUTO stopped: "+failed).c_str());
    }
    
    if(s.auto_state.restart_label!=0){
      xpp::log_auto_printf("RestartLabel=%d itp=%d ips=%d nfpar=%d ilp=%d isw=%d isp=%d A2p=%d \n",s.auto_state.restart_label,s.auto_state.bifur.itp, s.auto_state.bifur.ips,s.auto_state.bifur.nfpar,s.auto_state.bifur.ilp,s.auto_state.bifur.isw,s.auto_state.bifur.isp,s.auto_state.two_param);
      s.auto_state.bifur.irs=s.auto_state.restart_label;
      s.auto_state.restart_label=0;
      do_auto(s, iold,isave, s.auto_state.bifur.itp);
      
    }
     ping();
      redraw_params();
}

void set_auto(xpp::Session &s) /* Caution - need to include NICP here */
{
  s.auto_state.nuzr=s.auto_state.bifur.nper;
  init_auto(s.auto_state,s.model().node,s.auto_state.bifur.nfpar,s.auto_state.bifur.nbc,s.auto_state.bifur.ips,s.auto_state.bifur.irs,s.auto_state.bifur.ilp,s.auto_state.bifur.ntst,s.auto_state.bifur.isp,
	    s.auto_state.bifur.isw,s.auto_state.bifur.nmx,s.auto_state.bifur.npr,s.auto_state.bifur.ds,s.auto_state.bifur.dsmin,
	    s.auto_state.bifur.dsmax,s.auto_state.bifur.rl0,s.auto_state.bifur.rl1,s.auto_state.bifur.a0,s.auto_state.bifur.a1,s.auto_state.axes().icp1,
	    s.auto_state.axes().icp2,s.auto_state.bifur.icp3,s.auto_state.bifur.icp4,s.auto_state.bifur.icp5,s.auto_state.bifur.nper,s.auto_state.bifur.epsl,s.auto_state.bifur.epsu,s.auto_state.bifur.epss,s.auto_state.bifur.ncol);
  
}
int auto_name_to_index(const xpp::Session &s, std::string_view name)
{
  int i,in;
  find_variable(s,name,&in);
  if(in==0)return(10);
  in=xpp::find_user_name(s.model(),PARAM_BOX,name);
  for(i=0;i<s.auto_state.npar;i++)
    if(s.auto_state.par[i]==in)return(i);
  return(-1);
}
const char *auto_par_name(const xpp::Session &s, int k)
{
  return k>=0&&k<s.auto_state.npar&&s.auto_state.par[k]>=0&&s.auto_state.par[k]<s.model().nupar?s.model().upar_names[s.auto_state.par[k]].c_str():NULL;
}

namespace {
/* AUTO's parameter index's name, T for the period; empty for no such
   parameter */
std::string par_or_period_name(const xpp::Session &s, long index)
{
  if(index==AUTO_PERIOD_INDEX)return "T";
  return index>=0&&index<s.auto_state.npar?par_label(s, static_cast<int>(index)):std::string();
}

/* fscanf "%ld" at s (blanks, a sign, digits): true with v */
bool read_long(const char *s,long &v)
{
  char *end;
  v=strtol(s,&end,10);
  return end!=s;
}

/* AUTO heads its printed columns PAR(n) and U(n); XPP knows what the user
   called them, and already uses those names for the diagram's axes. This
   rewrites one 14-character column heading for the screen only: fort.7 and
   fort.9 keep AUTO's own format, which its restart path and other people's
   scripts read. PAR(10) and friends are the period and such, not the user's
   parameters, and par_or_period_name leaves them alone. A name that does not
   fit (a name may be any length) is shortened with a '~' (short_name) and
   still leaves a blank between it and the next heading: the column stays
   14 wide so the numbers below stay under it. */
std::string col_centre(const std::string &s)
{
  const std::string t=xpp::short_name(s,AUTO_COL_W-1);
  const int n=static_cast<int>(t.size());
  const int l=(AUTO_COL_W-n)/2;
  return std::string(static_cast<size_t>(l),' ')+t+std::string(static_cast<size_t>(AUTO_COL_W-n-l),' ');
}

std::string screen_col(const xpp::Session &s, const char *col)
{
  long p;
  const char *c=col;
  while(isspace(static_cast<unsigned char>(*c)))c++;
  if(strncmp(c,"PAR(",4)==0&&read_long(c+4,p)){
    const std::string name=par_or_period_name(s, p);
    if(!name.empty())return col_centre(name);
  }
  /* U(n), and the MAX(n) / MIN(n) a periodic branch prints, where AUTO has
     overwritten the U itself */
  const char *q=strchr(col,'(');
  if(q!=NULL&&strstr(col,"PAR")==NULL&&read_long(q+1,p)&&p>=1&&p<=s.model().node){
    size_t n=static_cast<size_t>(q-col);
    if(n>0&&col[n-1]=='U')n--; /* the name replaces the U */
    /* keep what stands in front of it: MAX, MIN, L2-NORM, INTEGRAL */
    std::string pre(col,std::min<size_t>(n,AUTO_COL_W));
    const size_t a=pre.find_first_not_of(' '),b=pre.find_last_not_of(' ');
    pre=a==std::string::npos?std::string():pre.substr(a,b-a+1);
    return col_centre(pre+(pre.empty()?"":" ")+s.model().uvar_names[p-1]);
  }
  return std::string(std::string_view(col).substr(0,AUTO_COL_W));
}
} // namespace

std::string auto_screen_col(const xpp::Session &s, const std::string &col)
{
  return screen_col(s, col.c_str());
}

void auto_per_par(xpp::Session &s)
{
  
  const char *const key=menu_auto_mark.keys;
  std::array<std::string, 9> values;
  static const char *n[]={"Uzr1","Uzr2","Uzr3","Uzr4","Uzr5",
		      "Uzr6","Uzr7","Uzr8","Uzr9"};
  int status,i,in;
  char ch;
  /* "Mark values" (T21): AUTO labels (UZ) the points where a parameter or
     the period reaches one of these values */
  ch=static_cast<char>(menu_choose(&menu_auto_mark,s.auto_state.bifur.nper));
  for(i=0;i<10;i++)
    if(ch==key[i])s.auto_state.bifur.nper=i;
  s.auto_state.nuzr=s.auto_state.bifur.nper;
  if(s.auto_state.bifur.nper>0){
    for(i=0;i<9;i++){
      values[i] = xpp::format("{}={:g}", par_or_period_name(s, s.auto_state.bifur.uzrpar[i]), s.auto_state.bifur.period[i]);
    }
    status=do_string_box(5,2,"Mark values (UZ): parameter=value or per=value",n,values);
    if(status!=0)
      for(i=0;i<9;i++){
	xpp::Tokens tokens(values[i]);
	in=auto_name_to_index(s, tokens.next("=").value_or(std::string_view()));
	if(in>=0){
	  s.auto_state.bifur.uzrpar[i]=in;
	  s.auto_state.bifur.period[i]=atof(tokens.text("@").c_str());
	}
      }
  }
  for(i=0;i<9;i++){
    s.auto_state.uzr_period[i]=s.auto_state.bifur.period[i];
    s.auto_state.uzr_par[i]=s.auto_state.bifur.uzrpar[i];
  }
  
}

/* auto parameters are 1-8 (0-7) and since there are only 8, need to associate them
   with real xpp parameters for which there may be many 
*/
void auto_params(xpp::Session &s)
{
  static const char *n[]={"*2Par1","*2Par2","*2Par3","*2Par4","*2Par5","*2Par6","*2Par7","*2Par8"};
  int status,i,in;
  std::array<std::string, 8> values;
  for(i=0;i<8;i++){
    if(i<s.auto_state.npar)  values[i] = s.model().upar_names[s.auto_state.par[i]];
    else values[i].clear();
  }
  static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),
                            XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2)};
  status=do_string_box_of(8,1,"Parameters",n,values,kinds);
  if(status!=0){
    for(i=0;i<8;i++){
      if(i<s.auto_state.npar){
	in=xpp::find_user_name(s.model(),PARAM_BOX,values[i].c_str());
	if(in>=0){
	  s.auto_state.par[i]=in;
	  in=xpp::get_param_index(s,values[i].c_str());
	  s.auto_state.par_index[i]=in;
	}
      }
    }
  }
}

void auto_num_par(xpp::Session &s)
{
  /* grouped by what they do, which upstream's order was not: the box is 7
     rows by 4 columns, so a column is a group. Mesh and step size, then the
     ranges and tolerances, then the solver's integer knobs. */
  static const char *n[]={"Ntst","Nmax","NPr","Ncol","Ds","Dsmin","Dsmax",
		    "Par Min","Par Max","Norm Min","Norm Max","EPSL","EPSU","EPSS",
                    "IAD","MXBF","IID","ITMX","ITNW","NWTN","IADS","SuppBP"};
  int status;
  std::array<std::string, 22> values;
  values[0] = xpp::format("{:d}", s.auto_state.bifur.ntst);
  values[1] = xpp::format("{:d}", s.auto_state.bifur.nmx);
  values[2] = xpp::format("{:d}", s.auto_state.bifur.npr);
  values[3] = xpp::format("{:d}", s.auto_state.bifur.ncol);
  values[4] = xpp::format("{:g}", s.auto_state.bifur.ds);
  values[5] = xpp::format("{:g}", s.auto_state.bifur.dsmin);
  values[6] = xpp::format("{:g}", s.auto_state.bifur.dsmax);
  values[7] = xpp::format("{:g}", s.auto_state.bifur.rl0);
  values[8] = xpp::format("{:g}", s.auto_state.bifur.rl1);
  values[9] = xpp::format("{:g}", s.auto_state.bifur.a0);
  values[10] = xpp::format("{:g}", s.auto_state.bifur.a1);
  values[11] = xpp::format("{:g}", s.auto_state.bifur.epsl);
  values[12] = xpp::format("{:g}", s.auto_state.bifur.epsu);
  values[13] = xpp::format("{:g}", s.auto_state.bifur.epss);
  values[14] = xpp::format("{:d}", s.auto_state.advanced.iad);
  values[15] = xpp::format("{:d}", s.auto_state.advanced.mxbf);
  values[16] = xpp::format("{:d}", s.auto_state.advanced.iid);
  values[17] = xpp::format("{:d}", s.auto_state.advanced.itmx);
  values[18] = xpp::format("{:d}", s.auto_state.advanced.itnw);
  values[19] = xpp::format("{:d}", s.auto_state.advanced.nwtn);
  values[20] = xpp::format("{:d}", s.auto_state.advanced.iads);
  values[21] = xpp::format("{:d}", s.auto_state.suppress_bp); 

  static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
  status=do_string_box_of(7,4,"AutoNum",n,values,kinds);
  if(status!=0){
    s.auto_state.bifur.ntst=atoi(values[0].c_str());
    s.auto_state.bifur.nmx=atoi(values[1].c_str());
    s.auto_state.bifur.npr=atoi(values[2].c_str());
    s.auto_state.bifur.ncol=atoi(values[3].c_str());
    s.auto_state.bifur.ds=atof(values[4].c_str());
    s.auto_state.bifur.dsmin=atof(values[5].c_str());
    s.auto_state.bifur.dsmax=atof(values[6].c_str());
    s.auto_state.bifur.rl0=atof(values[7].c_str());
    s.auto_state.bifur.rl1=atof(values[8].c_str());
    s.auto_state.bifur.a0=atof(values[9].c_str());
    s.auto_state.bifur.a1=atof(values[10].c_str());
    s.auto_state.bifur.epsl=atof(values[11].c_str());
    s.auto_state.bifur.epsu=atof(values[12].c_str());
    s.auto_state.bifur.epss=atof(values[13].c_str());
    s.auto_state.advanced.iad=atoi(values[14].c_str());
    s.auto_state.advanced.mxbf=atoi(values[15].c_str());
    s.auto_state.advanced.iid=atoi(values[16].c_str());
    s.auto_state.advanced.itmx=atoi(values[17].c_str());
    s.auto_state.advanced.itnw=atoi(values[18].c_str());
    s.auto_state.advanced.nwtn=atoi(values[19].c_str());
    s.auto_state.advanced.iads=atoi(values[20].c_str());
    s.auto_state.suppress_bp=atoi(values[21].c_str());

  }

}    

void auto_plot_par(xpp::Session &s)
{

  const char *const key=menu_auto_plot_type.keys;
  char ch;

  static const char *n[]={"*1Y-axis","*2Main Parm", "*2Secnd Parm", "Xmin", "Ymin",
		   "Xmax", "Ymax"};
  std::array<std::string, 7> values;
  int  status,i;
  int ii1,ii2,ji1,ji2;
  int i1=s.auto_state.axes().var+1;
  ch=static_cast<char>(menu_choose(&menu_auto_plot_type,s.auto_state.axes().plot));
  if(ch==ESC) 
    return;
  for(i=0;i<5;i++){
    if(ch==key[i])s.auto_state.axes().plot=i;
  }
  if(ch==key[10])s.auto_state.axes().plot=10;
  if(ch==key[11])s.auto_state.axes().plot=11;
  if(ch==key[5]){
    if(auto_rubber(s,&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_in(s, ii1,ji1,ii2,ji2);
      redraw_diagram(s);
    }
    return;
  }
  
  if(ch==key[6]){
    if(auto_rubber(s,&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_out(s, ii1,ji1,ii2,ji2);
     
      redraw_diagram(s);
    }
    return;
  }

  /* a new plot type or axes: the diagram drawn again in its quantities
     (it used to wait for reDraw, which a client drawing from data lacks) */
  if(ch==key[7]){
    load_last_plot(s, 1);
    redraw_diagram(s);
    return;
  }

  if(ch==key[8]){
    load_last_plot(s, 2);
    redraw_diagram(s);
    return;
  }

  if(ch==key[9]){
    auto_fit(s);
    redraw_diagram(s);
    return;
  }
  if(ch==key[12]){
    auto_default(s);
    redraw_diagram(s);
    return;
  }
  if(ch==key[13]){
    auto_scroll_window(s);
    redraw_diagram(s);
    return;
  }
  if(ch==key[14]){
    auto_new_view(s);
    return;
  }
  values[0] = xpp::ind_to_sym(s,i1);
  values[1] = s.model().upar_names[s.auto_state.par[s.auto_state.axes().icp1]];
  values[2] = s.model().upar_names[s.auto_state.par[s.auto_state.axes().icp2]];
  values[3] = xpp::format("{:g}", s.auto_state.axes().xmin);
  values[4] = xpp::format("{:g}", s.auto_state.axes().ymin);
  values[5] = xpp::format("{:g}", s.auto_state.axes().xmax);
  values[6] = xpp::format("{:g}", s.auto_state.axes().ymax);
  static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
  status=do_string_box_of(7,1,"AutoPlot",n,values,kinds);
  if(status!=0){
    /*  get variable names  */
    find_variable(s,values[0].c_str(),&i);
    if(i>0)
      s.auto_state.axes().var=i-1;
    /*  Now check the parameters  */
    i1=xpp::find_user_name(s.model(),PARAM_BOX,values[1].c_str());
    if(i1>=0){
      for(i=0;i<s.auto_state.npar;i++){
	if(i1==s.auto_state.par[i]){
	  s.auto_state.axes().icp1=i;

	}
      }
    }
     i1=xpp::find_user_name(s.model(),PARAM_BOX,values[2].c_str());
    if(i1>=0){
      for(i=0;i<s.auto_state.npar;i++){
	if(i1==s.auto_state.par[i]){
	  s.auto_state.axes().icp2=i;
	}
      }
    }

    s.auto_state.axes().xmin=atof(values[3].c_str());
    s.auto_state.axes().ymin=atof(values[4].c_str());
    s.auto_state.axes().xmax=atof(values[5].c_str());
    s.auto_state.axes().ymax=atof(values[6].c_str());
    if(s.auto_state.axes().plot<4)keep_last_plot(s, 1);
    if(s.auto_state.axes().plot==4)keep_last_plot(s, 2);
    redraw_diagram(s);

}
}

void auto_default(xpp::Session &s)
{
  s.auto_state.axes().xmin=s.auto_state.options.xmin;
  s.auto_state.axes().xmax=s.auto_state.options.xmax;
  s.auto_state.axes().ymin=s.auto_state.options.ymin;
  s.auto_state.axes().ymax=s.auto_state.options.ymax;
}

void auto_fit(xpp::Session &s)
{
  double xlo=s.auto_state.axes().xmin,xhi=s.auto_state.axes().xmax,ylo=s.auto_state.axes().ymin,yhi=s.auto_state.axes().ymax;
  bound_diagram(s, &xlo,&xhi,&ylo,&yhi);
  /* a flat quantity (a steady branch's period, all 0) is widened as the
     plot window's Fit does: an empty range divides by zero in IXVal/IYVal */
  double mid,span;
  check_val(&xlo,&xhi,&mid,&span);
  check_val(&ylo,&yhi,&mid,&span);
  s.auto_state.axes().xmin=xlo;
  s.auto_state.axes().xmax=xhi;
  s.auto_state.axes().ymin=ylo;
  s.auto_state.axes().ymax=yhi;
}
  
void auto_zoom_in(xpp::Session &s, int i1, int j1, int i2, int j2)
{
   double x1,y1,x2,y2;
   int temp;
   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   double dx = (s.auto_state.axes().xmax-s.auto_state.axes().xmin);
   double dy = (s.auto_state.axes().ymax-s.auto_state.axes().ymin);
   x1 = s.auto_state.axes().xmin+static_cast<double>((i1-s.auto_state.bifur.x0))*(dx)/static_cast<double>(s.auto_state.bifur.wid);
   x2 = s.auto_state.axes().xmin+static_cast<double>((i2-s.auto_state.bifur.x0))*(dx)/static_cast<double>(s.auto_state.bifur.wid);
   y1 = s.auto_state.axes().ymin+static_cast<double>((s.auto_state.bifur.hgt+s.auto_state.bifur.y0-j1))*(dy)/static_cast<double>(s.auto_state.bifur.hgt);
   y2 = s.auto_state.axes().ymin+static_cast<double>((s.auto_state.bifur.hgt+s.auto_state.bifur.y0-j2))*(dy)/static_cast<double>(s.auto_state.bifur.hgt);
 
   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx/2;
	  dy = dy/2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  s.auto_state.axes().xmin=x1-dx/2;
	  s.auto_state.axes().xmax=x1+dx/2;
	  s.auto_state.axes().ymin=y1-dy/2;
	  s.auto_state.axes().ymax=y1+dy/2;
  }
  else
  {           
	  s.auto_state.axes().xmin=x1;
	  s.auto_state.axes().ymin=y1;
	  s.auto_state.axes().xmax=x2;
	  s.auto_state.axes().ymax=y2;     
  }
  	
}

void auto_zoom_out(xpp::Session &s, int i1, int j1, int i2, int j2)
{
   double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
   int temp;
   double dx = (s.auto_state.axes().xmax-s.auto_state.axes().xmin);
   double dy = (s.auto_state.axes().ymax-s.auto_state.axes().ymin);
   double a1,a2,b1,b2;

   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   a1=static_cast<double>((i1-s.auto_state.bifur.x0))/static_cast<double>(s.auto_state.bifur.wid);
      a2=static_cast<double>((i2-s.auto_state.bifur.x0))/static_cast<double>(s.auto_state.bifur.wid);
      b1=static_cast<double>((s.auto_state.bifur.hgt+s.auto_state.bifur.y0-j1))/static_cast<double>(s.auto_state.bifur.hgt);
      b2=static_cast<double>((s.auto_state.bifur.hgt+s.auto_state.bifur.y0-j2))/static_cast<double>(s.auto_state.bifur.hgt);

   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx*2;
	  dy = dy*2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  s.auto_state.axes().xmin=x1-dx/2;
	  s.auto_state.axes().xmax=x1+dx/2;
	  s.auto_state.axes().ymin=y1-dy/2;
	  s.auto_state.axes().ymax=y1+dy/2;
  }
  else
  {           
    x1=(a1*s.auto_state.axes().xmax-a2*s.auto_state.axes().xmin)/(a1-a2);
    x2=(s.auto_state.axes().xmin-s.auto_state.axes().xmax+a1*s.auto_state.axes().xmax-a2*s.auto_state.axes().xmin)/(a1-a2);
    y1=(b1*s.auto_state.axes().ymax-b2*s.auto_state.axes().ymin)/(b1-b2);
    y2=(s.auto_state.axes().ymin-s.auto_state.axes().ymax+b1*s.auto_state.axes().ymax-b2*s.auto_state.axes().ymin)/(b1-b2);
	  s.auto_state.axes().xmin=x1;
	  s.auto_state.axes().ymin=y1;
	  s.auto_state.axes().xmax=x2;
	  s.auto_state.axes().ymax=y2;
  }

} 

void auto_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, double *uhigh, double *ulow, double *ubar, double a)
{
 /* a plot type none of the cases know leaves the point at (par1, 0) */
 *x=par1;
 *y1=*y2=0.0;
 switch(ax->plot){
  case HI_P:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=*y1;
    break;
  case NR_P:
    *x=par1;
    *y1=a;
    *y2=*y1;
    break;
  case HL_P:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=ulow[ax->var];
    break;
  case AV_P:
    *x=par1;
    *y1=ubar[ax->var];
    *y2=*y1;
    break;
  case PE_P:
    *x=par1;
    *y1=per;
    *y2=*y1;
    break;
  case FR_P:
    *x=par1;
    if(per>0)*y1=1./per;
    else *y1=0.0;
    *y2=*y1;
    break;
  case P_P:
    *x=par1;
    *y1=par2;
    *y2=*y1;
    break;
  }
}

void add_ps_point(xpp::Session &s, double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
		  int type, int flg, int lab, int npar, int icp1, int icp2, int flag2,
		  double *evr, double *evi)
{
  double x,y1,y2,par1,par2=0;
  int type1=type;
  par1=par[icp1];
  if(icp2<s.auto_state.npar)par2=par[icp2];
  auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    s.auto_state.bifur.lastx=x;
    s.auto_state.bifur.lasty=y1;
  }
  if(flag2==0&&s.auto_state.axes().plot==P_P)
    {
  
       return;
     }
  if(flag2>0&&s.auto_state.axes().plot!=P_P){
  
    return;
  }

  if((flag2>0)&&(s.auto_state.axes().plot==P_P))
   type1=CSEQ;
  switch(type1){
 
  case CSEQ:
    if(s.auto_state.axes().plot==PE_P||s.auto_state.axes().plot==FR_P)break;
    if(icp1!=s.auto_state.axes().icp1)break;
    if(flag2>0&&s.auto_state.axes().icp2!=icp2)break;

    if(s.plot_export.color){
      set_linestyle(s,1);
      if(flag2>0)pscolset2(s, flag2);
    }
    else 
      set_linestyle(s,8);
    line_abs(s,static_cast<float>(x),static_cast<float>(y1),static_cast<float>(s.auto_state.bifur.lastx),static_cast<float>(s.auto_state.bifur.lasty));
    break;
  case CUEQ:
    if(s.auto_state.axes().plot==PE_P||s.auto_state.axes().plot==FR_P)break;
    if(icp1!=s.auto_state.axes().icp1)break;
    if(flag2>0&&s.auto_state.axes().icp2!=icp2)break;
    if(s.auto_state.axes().plot!=P_P)
      {if(s.plot_export.color) set_linestyle(s,0);else set_linestyle(s,4);}
    else
      {
	pscolset2(s, flag2);
      
      }
    line_abs(s,static_cast<float>(x),static_cast<float>(y1),static_cast<float>(s.auto_state.bifur.lastx),static_cast<float>(s.auto_state.bifur.lasty));
    break;
  case UPER:
    if(s.plot_export.color) 
      set_linestyle(s,9); 
    else 
      set_linestyle(s,0);
    if(icp1!=s.auto_state.axes().icp1)break;
    if(flag2>0&&s.auto_state.axes().icp2!=icp2)break;
    s.drawing.point_type=UPT;
    point_abs(s,static_cast<float>(x),static_cast<float>(y1));
    point_abs(s,static_cast<float>(x),static_cast<float>(y2));
    break;
  case SPER:
    if(s.plot_export.color)
      set_linestyle(s,7);
    else
      set_linestyle(s,0);
    if(icp1!=s.auto_state.axes().icp1)break;
    if(flag2>0&&s.auto_state.axes().icp2!=icp2)break;
    s.drawing.point_type=SPT;
    point_abs(s,static_cast<float>(x),static_cast<float>(y1));
    point_abs(s,static_cast<float>(x),static_cast<float>(y2)); 
    break;
  }

  s.auto_state.bifur.lastx=x;
  s.auto_state.bifur.lasty=y1;
}

void auto_line(xpp::Session &s, double x1i, double y1i, double x2i, double y2i)
{
  double xmin,ymin,xmax,ymax;
  float x1=x1i,x2=x2i,y1=y1i,y2=y2i;
  double x1d,x2d,y1d,y2d;
  float x1_out,y1_out,x2_out,y2_out;

  get_scale(s,&xmin,&ymin,&xmax,&ymax);
  set_scale(s,s.auto_state.axes().xmin,s.auto_state.axes().ymin,s.auto_state.axes().xmax,s.auto_state.axes().ymax);
  if(clip(s.drawing,x1,x2,y1,y2,&x1_out,&y1_out,&x2_out,&y2_out)){
    x1d=x1_out;
    x2d=x2_out;
    y1d=y1_out;
    y2d=y2_out;
    DLINE(s, x1d,y1d,x2d,y2d);
  }
 
  set_scale(s,xmin,ymin,xmax,ymax);
}
/* The point add_point() is given next, for the diagram's data
   (auto_diagram): its caller knows the branch and point, add_point does not. */
static int dpt_ibr,dpt_ntot,dpt_itp,dpt_node,dpt_from;
void auto_point_id(int ibr,int ntot,int itp,int node,int from)
{
  dpt_ibr=ibr;
  dpt_ntot=ntot;
  dpt_itp=itp;
  dpt_node=node;
  dpt_from=from;
}

int auto_run_from_take(void)
{
  int f=run_from;
  run_from=0;
  return f;
}

/* the stability circle as data (auto_data.h): a stored point's values
   (auto_stability.h), whether AUTO is computing it or a grab is on it */
static void show_stab(xpp::Session &s, const double *evr,const double *evi,int n,int periodic)
{
  auto_data_stab(evr,evi,n,periodic);
}

/* the colour colset() and colset2() give a point */
static int auto_point_color(xpp::Session &s, int type,int flag2)
{
  switch(flag2){
  case 0: break;
  case LPE2: return LPE_color;
  case LPP2: return LPP_color;
  case HB2: return HB_color;
  case TR2: return TR_color;
  case BR2: return BR_color;
  case PD2: return PD_color;
  case FP2: return FP_color;
  default: return 0;
  }
  switch(type){
  case CSEQ: return s.auto_state.stable_eq_color;
  case CUEQ: return s.auto_state.unstable_eq_color;
  case SPER: return s.auto_state.stable_po_color;
  case UPER: return s.auto_state.unstable_po_color;
  }
  return 0;
}

/* this bit of code is for writing points - it only saves what is
   in the current view

*/
int check_plot_type(const xpp::Session &s, int flag2,int icp1, int icp2)
{
  if(flag2==0 && s.auto_state.axes().plot==P_P)
    return 0;
  if(flag2>0  && s.auto_state.axes().plot!=P_P)
    return 0; 
  if(icp1!=s.auto_state.axes().icp1)
    return 0;
  if(flag2>0 && icp2!=s.auto_state.axes().icp2)
    return 0;
  return 1;

} 
namespace {

/* the point add_point() is given, in view v (axes ax): its data
   (auto_diagram) and, in the active view, its drawing. A point the view
   does not plot (a one-parameter point in a two-parameter view, a
   two-parameter point in any other) goes to the data too, with no
   coordinates, so every view holds one entry per point of the diagram in
   the same order: a point's index is the same in every view (the grab,
   Clear's earlier branches). */
void view_point(xpp::Session &s, int v, const AUTOAX &ax, bool active, double *par, double per, double *uhigh, double *ulow,
                double *ubar, double a, int type, int flg, int lab, int icp1, int icp2, int flag2)
{
  double x,y1,y2,par1,par2=0;
  int ix=0,iy1=0,iy2=0,type1=type;
  XppDiagPoint dp;
  par1=par[icp1];
  if(icp2<s.auto_state.npar)par2=par[icp2];
  auto_xy_plot(&ax,&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a); /* figure out who sits on axes */
  memset(&dp,0,sizeof dp);
  dp.ibr=dpt_ibr;
  dp.pt=dpt_ntot;
  dp.itp=dpt_itp;
  dp.node=dpt_node;
  dp.from=dpt_from;
  dp.type=type;
  dp.flag2=flag2;
  dp.newseg=(flg==0);
  dp.color=auto_point_color(s, type,flag2);
  dp.lw=(type==CSEQ||flag2>0)?2:1;
  dp.x=x;
  dp.y1=y1;
  dp.y2=y2;
  if(active){
    if(flg==0){
      s.auto_state.bifur.lastx=x;
      s.auto_state.bifur.lasty=y1;
    }
    ix=IXVal(s, x);
    iy1=IYVal(s, y1);
    iy2=IYVal(s, y2);
    autobw();
  }
  if((flag2==0&&ax.plot==P_P)||(flag2>0&&ax.plot!=P_P)){
    dp.x=dp.y1=dp.y2=std::numeric_limits<double>::quiet_NaN();
    auto_diagram(s,v,&dp);
    return;
  }
  if(flag2>0) /* a two-parameter point in a two-parameter view */
    type1=CSEQ;
  switch(type1){
  case CSEQ:
  case CUEQ:
    if(ax.plot==PE_P||ax.plot==FR_P)break;
    if(icp1!=ax.icp1)break;
    if(flag2>0&&ax.icp2!=icp2)break;
    dp.draw=1;
    if(active){
      LineWidth(type1==CSEQ?2:1);
      colset(s, type);
      if(flag2>0)colset2(flag2);
      auto_line(s, x,y1,s.auto_state.bifur.lastx,s.auto_state.bifur.lasty);
      autobw();
    }
    break;
  case UPER:
  case SPER:
    if(icp1!=ax.icp1)break;
    if(flag2>0&&ax.icp2!=icp2)break;
    dp.draw=type1==SPER?2:3;
    if(active){
      LineWidth(1);
      colset(s, type);
      if(flag2>0)colset2(flag2);
      if(type1==SPER){
        if(chk_auto_bnds(s, ix,iy1))FillCircle(ix,iy1,3);
        if(chk_auto_bnds(s, ix,iy2))FillCircle(ix,iy2,3);
      }
      else{
        if(chk_auto_bnds(s, ix,iy1))Circle(ix,iy1,3);
        if(chk_auto_bnds(s, ix,iy2))Circle(ix,iy2,3);
      }
      autobw();
    }
    break;
  }
  if(lab!=0&&icp1==ax.icp1&&(flag2==0||ax.icp2==icp2)){
    dp.lab=lab;
    if(active){
      const std::string bob=xpp::format("{}",lab);
      LineWidth(1);
      if(chk_auto_bnds(s, ix,iy1)){
        ALINE(ix-4,iy1,ix+4,iy1);
        ALINE(ix,iy1-4,ix,iy1+4);
      }
      if(chk_auto_bnds(s, ix,iy2)){
        ALINE(ix-4,iy2,ix+4,iy2);
        ALINE(ix,iy2-4,ix,iy2+4);
      }
      if(chk_auto_bnds(s, ix,iy1))ATEXT(ix+8,iy1+8,bob.c_str());
    }
  }
  if(active){
    s.auto_state.bifur.lastx=x;
    s.auto_state.bifur.lasty=y1;
  }
  auto_diagram(s,v,&dp);
}

} // namespace

/* main plotting code: the point in every view */
void add_point(xpp::Session &s, double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
	       int type, int flg, int lab, int npar, int icp1, int icp2, int icp3, int icp4, int flag2,
	       double *evr, double *evi)
{
  for(int v=0;v<static_cast<int>(s.auto_state.views.size());v++)
    view_point(s, v,s.auto_state.views[static_cast<std::size_t>(v)].axes,v==s.auto_state.active_view,par,per,uhigh,
               ulow,ubar,a,type,flg,lab,icp1,icp2,flag2);
  show_stab(s, evr,evi,s.model().node,type==SPER||type==UPER);
  refreshdisplay();
}

const char *auto_bif_sym(int itp)
{
  switch(itp%10){
  case 1:
  case 6:
    return "BP";
  case 2:
  case 5:
    return "LP";
  case 3:
    return "HB";
  case -4:
    return "UZ";
  case 7:
    return "PD";
  case 8:
    return "TR";
  case 9:
    return "EP";
  case -9:
    return "MX";
  default:
    return "  ";
  }
}

void info_header(xpp::Session &s, int flag2, int icp1, int icp2)
{
  /* the names head 10-wide columns of new_info's numbers */
  auto short10=[](std::string_view name){ return xpp::short_name(name,10); };
  const std::string p1name=short10(s.model().upar_names[s.auto_state.par[icp1]]);
  const std::string p2name=icp2<s.auto_state.npar?short10(s.model().upar_names[s.auto_state.par[icp2]]):std::string("   ");
  const std::string vname=short10(s.model().uvar_names[s.auto_state.axes().var]);
  SmallBase();
  std::string bob=xpp::format("  Br  Pt Ty  Lab {:>10} {:>10}       norm {:>10}     period",
	  p1name,
	  p2name,
	  vname);
  draw_auto_info(bob.c_str(),10,text_metrics.small_height+1);

}

void new_info(xpp::Session &s, int ibr, int pt, const char *ty, int lab, double *par, double norm, double u0, double per, int flag2, int icp1, int icp2)
{
  double p1,p2=0.0;
  clear_auto_info();
  info_header(s, flag2,icp1,icp2);
  p1=par[icp1];
  if(icp2<s.auto_state.npar)p2=par[icp2];
  std::string bob=xpp::format("{:4} {:4} {:>2} {:4} {:10.4g} {:10.4g} {:10.4g} {:10.4g} {:10.4g}",
	  ibr,pt,ty,lab,p1,p2,norm,u0,per);
  draw_auto_info(bob.c_str(),10,2*text_metrics.small_height+2);
  refreshdisplay();
}

void traverse_out(xpp::Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw)
{
  double norm,per,*par,par1,par2=0,*evr,*evi;
  int pt,itp,ibr,lab,icp1,icp2,flag2;
  double x,y1,y2;
  const char *symb;
  if (d==NULL)
  {
	return;
  }
  norm=d->norm;
  par=d->par;

  per=d->per;
  lab=d->lab;
  itp=d->itp;
  ibr=d->ibr;
  icp1=d->icp1;
  icp2=d->icp2;
  flag2=d->flag2;
  pt=d->ntot;
  
  evr=d->evr;
  evi=d->evi;
 
  symb=auto_bif_sym(itp);
 par1=par[icp1];
  if(icp2<s.auto_state.npar)par2=par[icp2];  
    auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,per,d->uhi,d->ulo,d->ubar,norm);
  
    *ix=IXVal(s, x);
    *iy=IYVal(s, y1);
    if (dodraw==1)
    {
      AutoDataInfo ai;
    	XORCross(*ix,*iy);
  	new_info(s, ibr,pt,symb,lab,par,norm,d->u0[s.auto_state.axes().var],per,flag2,icp1,icp2);
      /* what the strip shows, as data */
      ai.ibr=ibr;
      ai.pt=pt;
      ai.itp=itp;
      ai.lab=lab;
      ai.type=get_bif_type(ibr,pt,lab);
      ai.flag2=flag2;
      ai.node=d->index;
      ai.sym=symb;
      ai.p1name=s.model().upar_names[s.auto_state.par[icp1]].c_str();
      ai.p1=par1;
      ai.p2name=icp2<s.auto_state.npar?s.model().upar_names[s.auto_state.par[icp2]].c_str():NULL;
      ai.p2=par2;
      ai.norm=norm;
      ai.vname=s.model().uvar_names[s.auto_state.axes().var].c_str();
      ai.u=d->u0[s.auto_state.axes().var];
      ai.per=per;
      ai.x=x;
      ai.y=y1;
      ai.y2=y2;
      /* the point and its circle together, after new_info (whose drawing
         may flush the autoinfo event): a flush between them sent the new
         circle with the old point when a step took over 0.1 s (autocheck
         under valgrind, W21) */
      auto_data_info(&ai);
      show_stab(s, evr,evi,s.model().node,ibr<0);
    }
    if(lab>0 && load_all_labeled_orbits>0)
      xpp::ok_or_show(load_auto_orbitx(s,ibr,1,lab,per));

}

void do_auto_win(xpp::Session &s)
{
  if(s.auto_state.bifur.exist==0){
    if(s.model().node>NAUTO){
      err_msg(xpp::format("Auto restricted to less than {} variables",NAUTO).c_str());
      return;
    }
    make_auto(s,"It's AUTO man!","AUTO");
    s.auto_state.bifur.exist=1;
    
  }

}

void load_last_plot(xpp::Session &s, int flg)
{
 if(flg==1) {/* one parameter */
  s.auto_state.axes().xmin=Old1p.xmin;
  s.auto_state.axes().xmax=Old1p.xmax;
  s.auto_state.axes().ymin=Old1p.ymin;
  s.auto_state.axes().ymax=Old1p.ymax;
  s.auto_state.axes().icp1=Old1p.icp1;
  s.auto_state.axes().icp2=Old1p.icp2;
  s.auto_state.axes().plot=Old1p.plot;
 s.auto_state.axes().var=Old1p.var;
}
if(flg==2) {/* two parameter */
  s.auto_state.axes().xmin=Old2p.xmin;
  s.auto_state.axes().xmax=Old2p.xmax;
  s.auto_state.axes().ymin=Old2p.ymin;
  s.auto_state.axes().ymax=Old2p.ymax;
  s.auto_state.axes().icp1=Old2p.icp1;
  s.auto_state.axes().icp2=Old2p.icp2;
  s.auto_state.axes().plot=Old2p.plot;
 s.auto_state.axes().var=Old2p.var;
}

}
void keep_last_plot(xpp::Session &s, int flg)
{
  if(flg==1){ /* one parameter */
    Old1p.xmin=s.auto_state.axes().xmin;
    Old1p.xmax=s.auto_state.axes().xmax;
    Old1p.ymin=s.auto_state.axes().ymin;
    Old1p.ymax=s.auto_state.axes().ymax;
    Old1p.icp1=s.auto_state.axes().icp1;
    Old1p.icp2=s.auto_state.axes().icp2;
    Old1p.plot=s.auto_state.axes().plot;
    Old1p.var=s.auto_state.axes().var;
  }
  if(flg==2){
    Old2p.xmin=s.auto_state.axes().xmin;
    Old2p.xmax=s.auto_state.axes().xmax;
    Old2p.ymin=s.auto_state.axes().ymin;
    Old2p.ymax=s.auto_state.axes().ymax;
    Old2p.icp1=s.auto_state.axes().icp1;
    Old2p.icp2=s.auto_state.axes().icp2;
    Old2p.plot=P_P;
    Old2p.var=s.auto_state.axes().var;
  }
}

void auto_new_view(xpp::Session &s)
{
  AutoDiagramView v;
  v.axes=s.auto_state.axes();
  s.auto_state.views.push_back(v);
  s.auto_state.active_view=static_cast<int>(s.auto_state.views.size())-1;
  if(s.auto_state.bifur.exist)redraw_diagram(s);
}

int auto_close_view(xpp::Session &s, int k)
{
  const int n=static_cast<int>(s.auto_state.views.size());
  if(k<0||k>=n||n<2)return 0;
  s.auto_state.views.erase(s.auto_state.views.begin()+k);
  /* the active one keeps its view; closed, the one in its place (or the
     last) takes over */
  if(s.auto_state.active_view>k)s.auto_state.active_view--;
  else if(s.auto_state.active_view==k)s.auto_state.active_view=std::min(k,n-2);
  if(s.auto_state.bifur.exist)redraw_diagram(s);
  return 1;
}

int auto_activate_view(xpp::Session &s, int k)
{
  if(k<0||k>=static_cast<int>(s.auto_state.views.size()))return 0;
  s.auto_state.active_view=k;
  return 1;
}

void init_auto_win(xpp::Session &s)
{
  int i;
  if(s.model().node>NAUTO)return;
  start_diagram(s, s.model().node); 
  for(i=0;i<10;i++){
    s.auto_state.bifur.period[i]=11.+3.*i;
    s.auto_state.bifur.uzrpar[i]=10;
    s.auto_state.uzr_period[i]=s.auto_state.bifur.period[i];
    s.auto_state.uzr_par[i]=10;
  }
  s.auto_state.npar=8;
  if(s.model().nupar<8)s.auto_state.npar=s.model().nupar;
  for(i=0;i<s.auto_state.npar;i++)s.auto_state.par[i]=i;
  for(i=0;i<s.auto_state.npar;i++){
    s.auto_state.par_index[i]=xpp::get_param_index(s,s.model().upar_names[s.auto_state.par[i]]);
  }
  s.auto_state.bifur.nper=0;
  grabpt.flag=0;  /*  no point in buffer  */
  s.auto_state.bifur.exist=0;
 s.auto_state.blrtn.torper=s.numerics.tor_period;
 create_auto_file_name(s);
 
/*  Control -- done automatically   */
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.ips=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.nbc=s.model().node;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.homo_flag=0;
/*  User controls this      */
  s.auto_state.bifur.ncol=s.auto_state.options.ncol;
  s.auto_state.bifur.ntst=s.auto_state.options.ntst;
  s.auto_state.bifur.nmx=s.auto_state.options.nmx;
  s.auto_state.bifur.npr=s.auto_state.options.npr;
  s.auto_state.bifur.ds=s.auto_state.options.ds;
  s.auto_state.bifur.dsmax=s.auto_state.options.dsmax;
  s.auto_state.bifur.dsmin=s.auto_state.options.dsmin;
  s.auto_state.bifur.rl0=s.auto_state.options.rl0;
  s.auto_state.bifur.rl1=s.auto_state.options.rl1;
  s.auto_state.bifur.a0=s.auto_state.options.a0;
  s.auto_state.bifur.a1=s.auto_state.options.a1;
  
  s.auto_state.bifur.epsl=s.auto_state.options.epsl;
    s.auto_state.bifur.epsu=s.auto_state.options.epsu;
  s.auto_state.bifur.epss=s.auto_state.options.epss;

/* The diagram plotting stuff: one view (W50)    */
  s.auto_state.views.assign(1,AutoDiagramView{});
  s.auto_state.active_view=0;
  s.auto_state.axes().xmax=s.auto_state.options.xmax;
  s.auto_state.axes().xmin=s.auto_state.options.xmin;
  s.auto_state.axes().ymax=s.auto_state.options.ymax;
  s.auto_state.axes().ymin=s.auto_state.options.ymin;
  s.auto_state.axes().plot=HL_P;
  s.auto_state.axes().var=s.auto_state.options.var;

/* xpp parameters    */
  
  s.auto_state.axes().icp1=0;
  s.auto_state.axes().icp2=1;
   s.auto_state.bifur.icp3=1;
  s.auto_state.bifur.icp4=1;
  s.auto_state.bifur.icp5=1;
  keep_last_plot(s, 1);
  keep_last_plot(s, 2);
  s.auto_state.advanced.iad=3;
  s.auto_state.advanced.mxbf=5;
  s.auto_state.advanced.iid=2;
  s.auto_state.advanced.itmx=8;
  s.auto_state.advanced.itnw=7;
  s.auto_state.advanced.nwtn=3;
  s.auto_state.advanced.iads=1;
  s.auto_state.run.nunstab=1;
  s.auto_state.run.nstab=s.model().node-1;
}

int yes_reset_auto(xpp::Session &s)
{
  if(diagram_count(s.diagram)<=1)return(0);
 kill_diagrams(s);
 FromAutoFlag=0;
    grabpt.flag=0;
    xpp::files::remove((this_auto_file+".b").c_str());
    xpp::files::remove((this_auto_file+".d").c_str());
    xpp::files::remove((this_auto_file+".s").c_str());
    diagram_mark.state=0;
    return 1;
}
int reset_auto(xpp::Session &s)
{
  char ch;
    if(diagram_count(s.diagram)<=1)return(0);
    ch=static_cast<char>(TwoChoice("YES","NO","Destroy AUTO diagram & files","yn"));
    if(ch!='y')return(0);
   
  return(yes_reset_auto(s));
}

void auto_grab(xpp::Session &s)
{
  traverse_diagram(s);
} 

void get_start_period(xpp::Session &s, double *p)
{
 *p=s.data_store.col[0][s.data_store.rows-1];
}
void find_best_homo_shift(xpp::Session &s, int n)
/* this code looks for the best value
    of the shift to be close as possible to the saddle 
    point of the homoclinic when starting from a 
    long periodic orbit
*/
{
  int i,j;
  double dmin=10000.0;
  double d;
  double tshift=0.0;
  for(i=0;i<s.data_store.rows;i++){
    d=0.0;
    for(j=0;j<n;j++){
      d+=fabs(s.data_store.col[j+1][i]-s.auto_state.homo_l[j]);
    }
    if(d<dmin){
      dmin=d;
      tshift=s.data_store.col[0][i];
    }
  }
  HOMO_SHIFT=tshift;
  xpp::log_auto_printf("shifting %g\n",HOMO_SHIFT);
}
void get_shifted_orbit(xpp::Session &s, double *u, double t, double p, int n)
{
  double ts;
  int i,i1,i2,ip,j;
  double lam;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  ts=fmod(t*p+HOMO_SHIFT,p);
  for(i=0;i<s.data_store.rows;i++){
    ip=(i+1)%s.data_store.rows;
    if((ts>=s.data_store.col[0][i])&&(ts<s.data_store.col[0][ip])){
      i1=i;
      i2=ip;
      lam=ts-s.data_store.col[0][i];
      for(j=0;j<n;j++)
	u[j]=(1.0-lam)*s.data_store.col[j+1][i1]+lam*s.data_store.col[j+1][i2];
      break;
    }
  }
}
void get_start_orbit(xpp::Session &s, double *u, double t, double p, int n)
{
  double tnorm,lam;
  int i1,i2,j;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  tnorm=t*(s.data_store.rows-1);
  i1=static_cast<int>(tnorm);
  i2=i1+1;
  if(i2>=s.data_store.rows)i2-=s.data_store.rows;
  lam=(tnorm-static_cast<double>(i1));

   for(j=0;j<n;j++)
    u[j]=(1.0-lam)*s.data_store.col[j+1][i1]+lam*s.data_store.col[j+1][i2];
}
  
void auto_start_choice(xpp::Session &s)
{
  char ch;
  s.auto_state.homo_flag=0;
  if(s.numerics.method==xpp::method::DISCRETE){
    auto_new_discrete(s);
    return;
  }
  ch=static_cast<char>(menu_choose(&menu_auto_start,0));
   if(ch=='s'){
    auto_new_ss(s);
    return;
  }
  /* the other starts take the orbit last integrated (autpp.cpp stpnt:
     get_start_period, get_start_orbit) */
  if(strchr("pbhe",ch)!=NULL&&ch!=0&&s.data_store.rows<2){
    err_msg("Integrate first: this start takes its orbit from the last integration");
    return;
  }
  if(ch=='p'){
  auto_start_at_per(s);
    return;
  }
 if(ch=='b'){
   s.auto_state.bifur.nbc=s.model().node;
   auto_start_at_bvp(s);
   return;
 }
 if(ch=='h'){
   s.auto_state.homo_flag=1;
   auto_start_at_homoclinic(s);
     return;
   }
     
 if(ch=='e'){
   s.auto_state.homo_flag=2;
   auto_start_at_homoclinic(s);
   return;
 }

  redraw_auto_menus();
}

void torus_choice(xpp::Session &s)
{
  char ch;
  ch=static_cast<char>(menu_choose(&menu_auto_torus,0));
   if(ch=='e'){
    auto_new_per(s);
    return;
  }
  if(ch=='f'){
      auto_2p_fixper(s);
    return;
  }
  if(ch=='t'){
    auto_torus(s);
    return;
    } 
  redraw_auto_menus();
}
 
void per_doub_choice(xpp::Session &s)
{
  char ch;
  ch=static_cast<char>(menu_choose(&menu_auto_per_doub,0));
  if(ch=='d'){
    auto_period_double(s);
    return;
  }
   if(ch=='e'){
    auto_new_per(s);
    return;
  }
  if(ch=='f'){
      auto_2p_fixper(s);
    return;
  }
  if(ch=='t'){
    auto_twopar_double(s);
    return;
  }
  redraw_auto_menus();
}
  
void periodic_choice(xpp::Session &s)
{
  char ch;
  ch=static_cast<char>(menu_choose(&menu_auto_periodic,0));
  if(ch=='e'){
    auto_new_per(s);
    return;
  }
  if(ch=='f'){
    auto_2p_fixper(s);
    return;
  }

  redraw_auto_menus();
}

void hopf_choice(xpp::Session &s)
{
  if(s.numerics.method==xpp::method::DISCRETE){
    auto_2p_hopf(s);
    return;
  }

  char ch;
  ch=static_cast<char>(menu_choose(&menu_auto_hopf,0));

  if(ch=='p'){
    auto_new_per(s);
    return;
  }
  if(ch=='e'){
    auto_extend_ss(s);
    return;
  }
  if(ch=='n'){
    auto_new_ss(s);
    return;
  }
  if(ch=='t'){
    auto_2p_hopf(s);
    return;
  }
  redraw_auto_menus();
}

void auto_run(xpp::Session &s)
{
  int itp1,itp2,itp,ips;
  char ch;
  if(grabpt.flag==0){   /* the first call to AUTO   */
    auto_start_choice(s);
    ping();return;
  }
  if(grabpt.lab==0){
    ch=static_cast<char>(TwoChoice("YES","NO","Not Labeled Pt: New Start?","y"));
    if(ch=='y')auto_start_diff_ss(s);
    ping();return;
  }
    
  itp=grabpt.itp;
  itp1=itp%10;
  itp2=itp/10;
  ips=s.auto_state.bifur.ips;
  if(itp1==3||itp2==3){  /* its a HOPF Point  */
    hopf_choice(s);
    ping();return;
  }
  if(itp1==7||itp2==7){ /* period doubling */
    per_doub_choice(s);
    ping();return;
  }
  if(ips==9){
    auto_homo_choice(s, itp);
    ping(); return;
  }
  if(itp1==2||itp2==2){ /* limit point */
     s.auto_state.bifur.ips=1;
     auto_2p_limit(s, s.auto_state.bifur.ips);
    ping();return;
  }
  if(itp1==5||itp2==5){ /* limit pt of periodic or BVP */
    if(s.auto_state.bifur.ips!=4)
      s.auto_state.bifur.ips=2;  /* this is a bit dangerous - the idea is that
                      if you are doing BVPs, then that is all you are
                      doing  
		   */
    auto_2p_limit(s, s.auto_state.bifur.ips);
    ping(); return;
  }
  if(itp1==6||itp2==6||itp1==1||itp2==1){ /* branch point  */ 

  auto_branch_choice(s, grabpt.ibr,ips);
    ping();
    return;
  }
  if(itp1==8||itp2==8){ /* Torus 2 parameter */
    torus_choice(s);
    ping();
    return;
  }
  if(grabpt.ibr<0) { /* its a periodic -- just extend it  */
    periodic_choice(s);
    ping();return;
  }
  if(grabpt.ibr>0&&ips!=4){ /*  old steady state -- just extend it  */
    auto_extend_ss(s);
    ping();return;
  }
  if(grabpt.ibr>0&&ips==4){
    auto_extend_bvp(s);
    ping();
    return;
  }
}

void auto_homo_choice(xpp::Session &s, int itp)
{
  if(itp!=5)
    auto_extend_homoclinic(s);
  
}
void auto_branch_choice(xpp::Session &s, int ibr, int ips)
{

  char ch;
  int ipsuse;
  ch=static_cast<char>(menu_choose(&menu_auto_branch,0));

  if(ch=='s'){
       if(ibr<0&&ips==2)
      auto_switch_per(s);
    else 
      if(ips==4)
	auto_switch_bvp(s);
      else
	auto_switch_ss(s);
    return;
  }
  if(ch=='e'){
    auto_extend_ss(s);
    return;
  }
  if(ch=='n'){
    auto_new_ss(s);
    return;
  }
  if(ch=='t'){
 
    ipsuse=1;
    if(ips==4)
      ipsuse=4;
    if(ibr<0)
      ipsuse=2;
    auto_2p_branch(s, ipsuse);
    return;
  }
  redraw_auto_menus();
}

/*  RUN AUTO HERE */
/*  these are for setting the parameters to run for different choices    */

/*  Just a short recall of the AUTO parameters
   NBC = 0 unless it really is a BVP problem (not periodics or heteroclinics)
   NICP = 1 for 1 parameter and 2 fro 2 parameter and the rest will 
            be taken care of in AUTO  and I think 2 for hetero?
   ILP = 1 (0) detection (no) of folds usually 1
   ISP = 2  detect all special points! but I think maybe set to 0, 1 for 
            BVP I think 
            for 2 parameter continuation ?
   ISW = -1 branch switching 1 is for normal 2 for two parameter of folds, tori,HB, PD!!

   IPS   1 - std for steady states of ODEs
         -1 maps
         2 periodic orbits
         4 BVP  (set NBC=NODE)
         9 Homoclinic

for example   2 P continuation of HB
              IPS=1 ILP=1 NICP=2 ISP=0 ISW=2 
BVP problem   IPS=4, NICP=1 NBC=NODE ISP=1 ISW=1 ILP=1

discrete dynamical system with two par of Hopf
first IPS=-1 ISP=ISW=1  then 
NICP=2, ISW=2 at Hopf

*/   

/* Start a new point for bifurcation diagram   */

void auto_start_diff_ss(xpp::Session &s)
{
  s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.ips=1;
  if(s.numerics.method==xpp::method::DISCRETE)s.auto_state.bifur.ips=-1;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=0;
  do_auto(s, NO_OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_start_at_bvp(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
 xpp::compile_bvp(s);
  if(s.numerics.bvp_flag==0)
    return; 
  s.auto_state.type_of_calc=BV1;
 s.auto_state.bifur.ips=4;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
  if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
    
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=0;
  s.auto_state.new_period_flag=2;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_start_at_per(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.ips=2;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=0;
  s.auto_state.new_period_flag=1;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_new_ss(xpp::Session &s)
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=0;

  if(diagram_count(s.diagram)>1){
    ans=reset_auto(s);
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.ips=1;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
      if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=0;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_new_discrete(xpp::Session &s)
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=0;
  if(diagram_count(s.diagram)>1){
    ans=reset_auto(s);
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
  s.auto_state.type_of_calc=DI1;
  s.auto_state.bifur.ips=-1;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=0; 
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}
 
void auto_extend_ss(xpp::Session &s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg("Can't continue infinite period Hopf!");
  	return;
  } 
  
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.ips=1;
  if(s.numerics.method==xpp::method::DISCRETE)
    s.auto_state.bifur.ips=-1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
    
  s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

int get_homo_info(xpp::Session &s, int flg,int *nun,int *nst,double *ul, double *ur)
{
  std::array<std::string, 100> v;
  int n=2+2*s.model().node;
  int i;
  int flag=0;
  /* do_string_box_of's names are read-only (const char *const *): plain
     std::strings own the text, names just points at them for the call */
  std::vector<std::string> labels(n);
  labels[0]="dim unstable";
  v[0] = xpp::format("{:d}", *nun);
  labels[s.model().node+1]="dim stable";
  v[s.model().node+1] = xpp::format("{:d}", *nst);
  for(i=0;i<s.model().node;i++){
    labels[i+1]=s.model().uvar_names[i]+"_L";
    v[i+1] = xpp::format("{:g}", ul[i]);
    labels[i+2+s.model().node]=s.model().uvar_names[i]+"_R";
    v[i+2+s.model().node] = xpp::format("{:g}", ur[i]);
  }
  std::vector<const char*> names(n);
  for(i=0;i<n;i++) names[i]=labels[i].c_str();

  {
    std::vector<int> kinds(n, XPP_FIELD_NUMBER);
    kinds[0]=XPP_FIELD_INTEGER;
    kinds[s.model().node+1]=XPP_FIELD_INTEGER;
    flag=do_string_box_of(n/2,2,"Homoclinic info",names.data(),v,kinds.data());
  }
  if(flag!=0){
    *nun=atoi(v[0].c_str());
    *nst=atoi(v[s.model().node+1].c_str());
    for(i=0;i<s.model().node;i++){
      ul[i]=atof(v[i+1].c_str());
      if(s.auto_state.homo_flag==2)
	ur[i]=atof(v[i+2+s.model().node].c_str());
    }
  }
  return flag;
}

void auto_extend_homoclinic(xpp::Session &s)
{
   s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;

      s.auto_state.type_of_calc=HO2;
  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=1;
  s.auto_state.bifur.ips=9;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==1)
    s.auto_state.run.iequib=1;
  if(s.auto_state.homo_flag==2)
    s.auto_state.run.iequib=-2;

  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);

}

void auto_start_at_homoclinic(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  int flag;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=0;
    s.auto_state.type_of_calc=HO2;

  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=1;
  s.auto_state.bifur.ips=9;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==1){
    s.auto_state.run.iequib=1;
    find_best_homo_shift(s, s.model().node);
  }
  if(s.auto_state.homo_flag==2)
    s.auto_state.run.iequib=-2;
  flag=get_homo_info(s, s.auto_state.homo_flag,&s.auto_state.run.nunstab,&s.auto_state.run.nstab,s.auto_state.homo_l.data(),s.auto_state.homo_r.data());
  if(flag)do_auto(s, opn,cls,s.auto_state.bifur.itp);

}
    
void auto_new_per(xpp::Session &s) /* same for extending periodic  */
{
  s.auto_state.blrtn.torper=grabpt.torper;
  
  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg("Can't continue infinite period Hopf.");
  	return;
  } 	
      s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1; /* -1 */
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=2;
    s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_extend_bvp(xpp::Session &s) /* extending bvp */
{
      s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=4;
    s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_per(xpp::Session &s)
{
      s.auto_state.type_of_calc=PE1;
  s.auto_state.blrtn.torper=grabpt.torper;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=1; /*grabpt.nfpar;*/
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=2;
  s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_bvp(xpp::Session &s)
{
     s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=4;
  s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_ss(xpp::Session &s)
{

      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=1;
  if(s.numerics.method==xpp::method::DISCRETE)
    s.auto_state.bifur.ips=-1;
  s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_2p_limit(xpp::Session &s, int ips)
{
  int ipsuse=1;
  int itp1,itp2;
  s.auto_state.blrtn.torper=grabpt.torper;
  s.auto_state.bifur.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0; /* was 2 */
  /* fix ips now */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==5)||(itp2==5))
      ipsuse=2;
  }

  s.auto_state.bifur.ips=ipsuse;
  s.auto_state.two_param=LPP2;
  if(ipsuse==1){
    s.auto_state.type_of_calc=LPE2;
    s.auto_state.two_param=LPE2;
  }
  else{
    s.auto_state.type_of_calc=LPP2;
    s.auto_state.two_param=LPP2;
  }
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

namespace {
/* continue a grabbed period doubling (PD2) or torus bifurcation (TR2) in
   two parameters: the same periodic restart, told apart by its kind */
void auto_2p_periodic(xpp::Session &s, int kind)
{
  s.auto_state.blrtn.torper=grabpt.torper;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.two_param=kind;
  s.auto_state.type_of_calc=kind;
  s.auto_state.bifur.ips=2;
  s.auto_state.bifur.ilp=0;
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}
} // namespace

void auto_twopar_double(xpp::Session &s)
{
  auto_2p_periodic(s, PD2);
}

void auto_torus(xpp::Session &s)
{
  auto_2p_periodic(s, TR2);
}

void auto_2p_branch(xpp::Session &s, int ips)
{
 int ipsuse=1;
  int itp1,itp2; 
 s.auto_state.blrtn.torper=grabpt.torper;
  s.auto_state.bifur.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0; /* was 2 */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==6)||(itp2==6))
      ipsuse=2;
  }

  s.auto_state.bifur.ips=ipsuse;
  if(s.numerics.method==xpp::method::DISCRETE)
    s.auto_state.bifur.ips=-1;
  s.auto_state.two_param=BR2;
      s.auto_state.type_of_calc=BR2;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_2p_fixper(xpp::Session &s)
{
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* was1 */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=2;
  s.auto_state.two_param=FP2;
  s.auto_state.type_of_calc=FP2;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_2p_hopf(xpp::Session &s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg("Can't continue infinite period Hopf.");
  	return;
  } 
  
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=1;
  if(s.numerics.method==xpp::method::DISCRETE)
    s.auto_state.bifur.ips=-1;
  s.auto_state.two_param=HB2;
    s.auto_state.type_of_calc=HB2;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_period_double(xpp::Session &s)
{

 s.auto_state.blrtn.torper=grabpt.torper;
  s.auto_state.bifur.ntst=2*s.auto_state.bifur.ntst;
  s.auto_state.bifur.irs=grabpt.lab;
  s.auto_state.bifur.nfpar=1; /* grabpt.nfpar; */

  s.auto_state.bifur.itp=grabpt.itp;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp==1) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=2;
  s.auto_state.two_param=0;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

/**********   END RUN AUTO *********************/

void load_auto_orbit(xpp::Session &s)
{
  xpp::ok_or_show(load_auto_orbitx(s,grabpt.ibr,grabpt.flag,grabpt.lab,grabpt.per));
}
xpp::Result<> load_auto_orbitx(xpp::Session &s, int ibr,int flag, int lab, double per)
{
  double *x;
  int i,j,nstor;
  double u[NAUTO],t;
  double period;
  std::string string;
  int nrow,ndim,label,flg;

  if((ibr>0&&(s.auto_state.bifur.ips!=4)&&(s.auto_state.bifur.ips!=3)&&(s.auto_state.bifur.ips!=9))||flag==0)return {};
   /* either nothing grabbed or just a fixed point and that is already loaded */
  string=this_auto_file+".s";
  xpp::UniqueFile fp=xpp::open_read(string.c_str());
  if(!fp){
    return xpp::fail("AUTO","No such file");
  }
  label=lab;
  period=per;
  flg=move_to_label(label,&nrow,&ndim,fp.get());
  nstor=ndim;
  if(ndim>s.model().node)nstor=s.model().node;
  if(flg==0){
    xpp::log_auto_printf("Could not find label %d in file %s \n",label,string.c_str());
    return xpp::fail("AUTO","Cant find labeled pt");
  }
  x=&s.data_store.current[0];
  for(i=0;i<nrow;i++){
    get_a_row(u,&t,ndim,fp.get());
    if(s.auto_state.bifur.ips!=4) 
      s.data_store.col[0][i]=t*period;
    else
      s.data_store.col[0][i]=t;

    for(j=0;j<nstor;j++){
      s.data_store.col[j+1][i]=u[j];
      x[j]=u[j];
    }
    extra(s,x,static_cast<double>(s.data_store.col[0][i]),nstor,s.model().neq);
    for(j=nstor;j<s.model().neq;j++)
      s.data_store.col[j+1][i]=static_cast<float>(x[j]);
  }
  s.data_store.rows=nrow;
  refresh_browser(s,nrow);
  /* insert auxiliary stuff here */
  if(load_all_labeled_orbits==2)clr_all_scrns(s);
  drw_all_scrns(s);
  return {};
}

void save_auto(xpp::Session &s)
{
  std::string filename=xpp_session_file_name(s.model(),xpp::autox::extension);
  if(!file_selector("Save diagram",filename,"*.autox"))return;
  if(xpp::snapx::has_extension(filename,".auto"))filename+='x'; /* the old name, the new file */
  filename=xpp::snapx::with_extension(filename,xpp::autox::extension);
  if(diagram_count(s.diagram)<=1){
    /* leave no file without a diagram (nor replace one with it) */
    err_msg("Empty diagram -- nothing to save");
    return;
  }
  std::optional<std::string> bytes=xpp::autox::file_bytes(s);
  if(!bytes)return; /* an error message said why */
  /* written beside filename and renamed over it once whole */
  xpp::Writer w=open_writer_asking(filename.c_str(),true);
  if(!w)return;
  if(!w.write(*bytes)){
    w.abort();
    err_msg(xpp::format("Cannot write {}",filename).c_str());
    return;
  }
  w.commit();
}

void load_auto_numerics(xpp::Session &s, FILE *fp)
{
 int i,in;
 /* The fscanf formats this replaces ended in whitespace, which fscanf
    skipped; the token reader leaves it in the stream, and every read that
    follows (load_auto_graph, load_diagram) skips it first. */
 xpp::TokenReader tr=xpp::TokenReader::attach(fp);
 if (!tr.read(s.auto_state.npar)) return;
 for(i=0;i<s.auto_state.npar;i++){
   if (!tr.read(s.auto_state.par[i])) return;
   in=xpp::get_param_index(s,s.model().upar_names[s.auto_state.par[i]]);
   s.auto_state.par_index[i]=in;
 }
 if (!tr.read(s.auto_state.nuzr)) return;
  for(i=0;i<9;i++){
    s.auto_state.bifur.nper=s.auto_state.nuzr;
    if (!tr.read(s.auto_state.uzr_period[i]) || !tr.read(s.auto_state.uzr_par[i])) return;
    s.auto_state.bifur.period[i]=s.auto_state.uzr_period[i];
    s.auto_state.bifur.uzrpar[i]=s.auto_state.uzr_par[i];
  }

 if (!tr.read(s.auto_state.bifur.ntst) || !tr.read(s.auto_state.bifur.nmx) || !tr.read(s.auto_state.bifur.npr)) return;
 if (!tr.read(s.auto_state.bifur.ds) || !tr.read(s.auto_state.bifur.dsmin) || !tr.read(s.auto_state.bifur.dsmax)) return;
 if (!tr.read(s.auto_state.bifur.rl0) || !tr.read(s.auto_state.bifur.rl1) || !tr.read(s.auto_state.bifur.a0) || !tr.read(s.auto_state.bifur.a1)) return;
 if (!tr.read(s.auto_state.advanced.iad) || !tr.read(s.auto_state.advanced.mxbf) || !tr.read(s.auto_state.advanced.iid) || !tr.read(s.auto_state.advanced.itmx)
     || !tr.read(s.auto_state.advanced.itnw) || !tr.read(s.auto_state.advanced.nwtn) || !tr.read(s.auto_state.advanced.iads)) return;
}

void load_auto_graph(xpp::Session &s, FILE *fp)
{
  xpp::TokenReader tr=xpp::TokenReader::attach(fp);
  if (!tr.read(s.auto_state.axes().xmin) || !tr.read(s.auto_state.axes().ymin) || !tr.read(s.auto_state.axes().xmax) || !tr.read(s.auto_state.axes().ymax)
      || !tr.read(s.auto_state.axes().var) || !tr.read(s.auto_state.axes().plot)) return;
}
  
namespace {
/* a blank line, which make_q_file leaves out */
bool noinfo(std::string_view s)
{
  for(char c:s)
    if(!isspace(static_cast<unsigned char>(c)))return false;
  return true;
}
} // namespace

xpp::Result<> make_q_file(FILE *fp)
{
  std::string string=this_auto_file+".s";
  /* written beside the .s and renamed over it once whole */
  xpp::Writer w(string.c_str());
  if(!w){
    return xpp::fail("AUTO","Couldnt open s-file");
  }

  /* the rest of fp, the .auto's copy of the .s, without its blank lines */
  xpp::LineReader lr=xpp::LineReader::attach(fp);
  while(auto line=lr.next()){
    if(!noinfo(*line))
      w.print("{}\n",*line);
  }
  w.commit();
  return {};
}

void load_auto(xpp::Session &s)
{
  std::string filename=xpp_session_file_name(s.model(),xpp::autox::extension);
  if(!file_selector("Load diagram",filename,"*.autox *.auto"))return;
  /* an .autox carries its model: opened as File > Open model opens it,
     its diagram into that model (the same one: only the diagram, in
     place of the one there) */
  if(xpp_saved_file_name(filename)){
    xpp_model_open(s, filename.c_str());
    return;
  }
  if(diagram_count(s.diagram)>1&&reset_auto(s)==0)return;
  xpp::autox::import_file(s,filename);
}

std::string auto_solutions_file()
{
  return this_auto_file+".s";
}

/* an XPPAUT .auto file, at fp, imported: its settings, diagram and
   solutions (xpp_session.cpp's older files too): 1 loaded, -1 an empty
   diagram */
int import_auto_file(xpp::Session &s, FILE *fp)
{
  int status;
  load_auto_numerics(s, fp);
  load_auto_graph(s, fp);
  auto_data_forget(); /* the strip described the diagram this one replaces */
  status=load_diagram(s, fp,s.model().node);
  if(status!=1)return status;
  xpp::ok_or_show(make_q_file(fp));
  return 1;
}

int move_to_label(int mylab, int *nrow, int *ndim, FILE *fp)
{
  /* a solution's label line: ibr ntot itp lab nfpr isw ntpl nar nrowpr
     (and more), AUTO's "%5ld" columns that may touch, which read(long&)
     reads apart; then its nrowpr rows. The rows after the label line
     found are get_a_row()'s. */
  enum {IBR,NTOT,ITP,LAB,NFPAR,ISW,NTPL,NAR,NSKIP,NFIELDS};
  std::array<long,NFIELDS> f{};
  xpp::TokenReader tr=xpp::TokenReader::attach(fp);
  while(true){
    for(long &v:f)
      if(!tr.read(v))return(0);
    tr.skip_line();
    if(mylab==f[LAB]){
      *nrow=static_cast<int>(f[NTPL]);
      *ndim=static_cast<int>(f[NAR])-1;
      return(1);
    }
    for(long i=0;i<f[NSKIP];i++)
      if(!tr.skip_line())return(0);
  }
}

void get_a_row(double *u, double *t, int n, FILE *fp)
 {
   int i;
   xpp::TokenReader tr=xpp::TokenReader::attach(fp);
   if (!tr.read(*t)) return;
   for(i=0;i<n;i++)
     if (!tr.read(u[i])) return;
 }

/* the diagram and its eigenvalues/multipliers as CSV (csv_export.h),
   pandas.read_csv/MATLAB readtable read with no options; one file dialog
   answer names both files (csv_export_diagram_pair derives the second) */
void export_auto_csv(xpp::Session &s)
{
  std::string filename="diagram.csv";
  if(!file_selector("Export CSV",filename,"*.csv"))return;
  const xpp::Result<bool> written=csv_export_diagram_pair(s,filename.c_str());
  if(!written)xpp::show_error(written.error());
  else if(!*written)err_msg("Nothing to export: run or load a diagram first");
}

void auto_file(xpp::Session &s)
{

  char ch;
  ch=static_cast<char>(menu_choose(&menu_auto_file,0));
  if(ch=='i'){
    load_auto_orbit(s);
    return;
  }
  if(ch=='s'){
    save_auto(s);
    return;
  }
  if(ch=='l'){
    load_auto(s);
    redraw_diagram(s); /* the loaded diagram, at once */
    return;
  }
  if(ch=='r'){
    reset_auto(s);
    redraw_diagram(s); /* now empty */
  }
  if(ch=='c'){
    grabpt.flag=0;
  }
  if(ch=='p'){
    s.plot_file.no_break_line=1;
    export_auto_picture(s, xpp::IMAGE_FORMAT_PS);
    s.plot_file.no_break_line=0;
  }
  if(ch=='v'){
    s.plot_file.no_break_line=1;
    export_auto_picture(s, xpp::IMAGE_FORMAT_SVG);
    s.plot_file.no_break_line=0;
  }
  if(ch=='w'){
    write_pts(s);
  }
  if(ch=='a'){
    write_info_out(s);
  }
  if(ch=='d'){
    write_init_data_file(s);
  }
  if(ch=='t'){
    auto_redraw=1-auto_redraw;
    if(auto_redraw==1)err_msg("Redraw is ON");
    else err_msg("Redraw is OFF");
  }
  if(ch=='o'){
    if(diagram_mark.state<2)
      err_msg("Mark a branch first using S and E");
    else
      load_browser_with_branch(s, diagram_mark.start_branch,diagram_mark.start_point,diagram_mark.end_point);
	}
  if(ch=='x'){
    export_auto_csv(s);
  }
  if(ch=='n'){
    if(diagram_mark.state<2) 
      err_msg("Mark a branch first using S and E");
    else
      do_auto_range(s);
  }
  if(ch=='e'){
    if(s.auto_state.axes().plot!=P_P){
      err_msg("Must be in 2 parameter plot");
      return;
    }
    setautopoint(s);

  }
  if(ch=='b'){
    if(load_all_labeled_orbits==0){
      load_all_labeled_orbits=1;
      err_msg("Draw orbits - no erase");
      return;
    }
     if(load_all_labeled_orbits==1){
      load_all_labeled_orbits=2;
      err_msg("Draw orbits - erase first");
      return;
    }
      if(load_all_labeled_orbits==2){
      load_all_labeled_orbits=0;
      err_msg("Draw orbits off");
      return;
    }
  }

}

void auto_get_info(xpp::Session &s, int *n, std::string &pname)
{
  int i1,i2,ibr;
  DIAGRAM *d,*dnew;

  if(diagram_mark.state==2){
    i1=abs(diagram_mark.start_point);
    ibr=diagram_mark.start_branch;
    i2=abs(diagram_mark.end_point);
    *n=abs(i2-i1);
    d=diagram_first(s.diagram);
    while(1){
      if(d->ibr==ibr && ((d->ntot==i1)||(d->ntot==(-i1))))
	{
	  pname=s.model().upar_names[s.auto_state.par[d->icp1]];
	  break;
	}
       dnew=diagram_next(s.diagram,d);
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
    }
  }
  
}

void auto_set_mark(xpp::Session &s, int i)
{
  int pt,ibr;
  if(diagram_mark.state==2){
    ibr=diagram_mark.start_branch;
    if(abs(diagram_mark.start_point)<abs(diagram_mark.end_point))
      pt=abs(diagram_mark.start_point)+i;
    else
      pt=abs(diagram_mark.end_point)+i;
    find_point(s, ibr,pt);
  }
}

void find_point(xpp::Session &s, int ibr, int pt)
{
  int i;
  DIAGRAM *d,*dnew;
   if(diagram_count(s.diagram)<2)return;
   d=diagram_first(s.diagram);
   while(1)
     {
       if(d->ibr==ibr && ((d->ntot==pt)||(d->ntot==(-pt))))
	 {  /* need to look at both signs to ignore stability */
	   /* now we use this info to set parameters and init data */
	   for(i=0;i<s.model().node;i++)
	     setvar(s,i+1,d->u0[i]);
	   xpp::get_ic(s,0,d->u0);
	   auto_set_pars_from(s, d->par);
	   xpp::evaluate_derived(s);
	   xpp::ok_or_show(xpp::redo_all_fun_tables(s));
	   redraw_params();
	   redraw_ics();
           if((d->per)>0)
	     xpp::set_total(s,d->per);		       
	   break;
	 }
       dnew=diagram_next(s.diagram,d);
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
     }
}

void do_auto_range(xpp::Session &s)
{
  double t=s.numerics.tend;
  
  if(diagram_mark.state==2)
    xpp::do_auto_range_go(s);
  s.numerics.tend=t;
}

void DLINE(xpp::Session &s, double a,double b,double c,double d)
{
  ALINE(IXVal(s, a),IYVal(s, b),IXVal(s, c),IYVal(s, d));
}

/* ---- grabbing a point on the bifurcation diagram, marking a branch and
   the hint line (logic from auto_x11.c) ---- */
/* auto_c.h's LEFT/RIGHT (1/2, unused below) are for AUTO's own
   continuation direction; undef them so mykeydef.h's key codes (6/2,
   used by the switch below) don't warn about redefining a different
   value. */
#undef LEFT
#undef RIGHT
#include "mykeydef.h"
static DIAGRAM *CUR_DIAGRAM;
static void grab_diagram_point(xpp::Session &s, const DIAGRAM *d);
static void finish_grab(xpp::Session &s);

const char *query_special(const char *title)
{
	XppMenu menu=menu_auto_special;
	menu.title=title;
	int ch=static_cast<char>(menu_choose(&menu,1));
	redraw_auto_menus();
	int i=ch!=0?xpp_menu_index(&menu,ch):-1;
	return i>=0?menu.items[i]:NULL;
}

void traverse_diagram(xpp::Session &s)
{
  DIAGRAM *d,*dnew,*dold;
  int done=0;
  int ix,iy;
  int lalo;
  int kp;
  int xm,ym;
  diagram_mark.state=0;
  if(diagram_count(s.diagram)<2)return;
  
  d=diagram_first(s.diagram); 
  DONT_XORCross=0;
  traverse_out(s, d,&ix,&iy,1);
  
  while(done==0){
    kp=xpp_ui.auto_grab_event(s,&xm,&ym);
    if(kp==XPP_AUTO_NODE)
    {
      /* a point of the diagram by its entry: the cursor goes there */
      dnew=diagram_point(s.diagram,xm);
      if(dnew!=NULL){
        clear_msg(s);
        XORCross(ix,iy);
        d=dnew;
        CUR_DIAGRAM=d;
        traverse_out(s, d,&ix,&iy,1);
      }
    }
    else if(kp==XPP_AUTO_CLICK)
    {
	{
       		clear_msg(s);
	        /*
		GO HOME
		*/
		XORCross(ix,iy);
		DONT_XORCross = 1;
		d=diagram_first(s.diagram);
		CUR_DIAGRAM=d;
		traverse_out(s, d,&ix,&iy,0);
                /*
		END GO HOME
		*/
       
       		/*
		GO END
		*/
		int mindex=0;
		double dist;
		double ndist = s.auto_state.bifur.wid*s.auto_state.bifur.hgt;
		XORCross(ix,iy);
                lalo=load_all_labeled_orbits;
		load_all_labeled_orbits=0;
		while (1)
		{
			dist = sqrt((static_cast<double>((xm-ix)))*(static_cast<double>((xm-ix))) + (static_cast<double>((ym-iy)))*(static_cast<double>((ym-iy)))); 
			if (dist<ndist)
			{
				ndist = dist;
				mindex=d->index;
			}
			dnew=diagram_next(s.diagram,d);
			if(dnew==NULL){dnew=d;break;}
			d=dnew;
			traverse_out(s, d,&ix,&iy,0);/*Need this each time to update the distance calc*/
	        }
		d=dnew;
       		CUR_DIAGRAM=d;
		load_all_labeled_orbits=lalo;
       		traverse_out(s, d,&ix,&iy,0);
		/*
		END GO END
		*/

		/*
		GO HOME
		*/
		XORCross(ix,iy);
		while (1){
		        if (d->index == mindex){dnew=d;break;}
        		dnew=diagram_prev(s.diagram,d);
        		if(dnew==NULL){dnew=d;break;}
        		d=dnew;
		}
		d=dnew;
		CUR_DIAGRAM=d;
		DONT_XORCross = 0;
		traverse_out(s, d,&ix,&iy,1);
                /*
		END GO HOME
		*/
		
	}
    }
    else {
        clear_msg(s);
	const char *nsymb;
        
	int found=0;

      switch(kp){
      case RIGHT:
	dnew=diagram_next(s.diagram,d);
	if(dnew==NULL)dnew=diagram_first(s.diagram);
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(s, d,&ix,&iy,1);
	break;
	
      case LEFT:
	dnew=diagram_prev(s.diagram,d);
	if(dnew==NULL)dnew=diagram_first(s.diagram);
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(s, d,&ix,&iy,1);
	break;
      case UP:
       if ((nsymb=query_special("Next..."))==NULL){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=diagram_next(s.diagram,d);
	 if(dnew==NULL){dnew=d;break;} 
 	 if(strcmp(auto_bif_sym(dnew->itp),nsymb)==0){d=dnew;found=1;break;} 
         d=dnew;
       }
       if (found)
       {
         d=dnew;
       }
       else
       {
         s.auto_state.bifur.hinttxt=xpp::format("  Higher {} not found",nsymb);
	 xpp_ui.auto_show_hint(s);
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
      case DOWN:
       if ((nsymb=query_special("Previous..."))==NULL){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=diagram_prev(s.diagram,d);
	 if(dnew==NULL){dnew=d;break;} 
 	 if(strcmp(auto_bif_sym(dnew->itp),nsymb)==0){d=dnew;found=1;break;} 
         d=dnew;
       }
       if (found)
       {
         d=dnew;
       }
       else
       {
         s.auto_state.bifur.hinttxt=xpp::format("  Lower {} not found",nsymb);
	 xpp_ui.auto_show_hint(s);
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break; 
      case TAB:
       XORCross(ix,iy);
       while(1){
         dnew=diagram_next(s.diagram,d);
         if(dnew==NULL){dnew=diagram_first(s.diagram);break;} /*TAB wraps*/
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
	/* New code */
      case 's': /* mark the start of a branch */
	if(diagram_mark.state==0) {
	  MarkAuto(ix,iy);
	  diagram_mark.start_branch=d->ibr;
	  diagram_mark.start_point=d->ntot;
	  diagram_mark.state=1;

	}
	break;
      case 'e': /* mark end of branch */
	if(diagram_mark.state==1){
	  MarkAuto(ix,iy);
	  diagram_mark.end_branch=d->ibr;
	  diagram_mark.end_point=d->ntot;
	  diagram_mark.state=2;

	}
	break;
       case END:/*All the way to end*/
       XORCross(ix,iy);
       d=last_diagram(s.diagram);
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
       case HOME:/*All the way to beginning*/
       XORCross(ix,iy);
       d=diagram_first(s.diagram);
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
       case PGUP: /*Same as TAB except we don't wrap*/
       XORCross(ix,iy);
       while(1){
         dnew=diagram_next(s.diagram,d);
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
       case PGDN: /*REVERSE TAB*/
       XORCross(ix,iy);
       while(1){
         dnew=diagram_prev(s.diagram,d);
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(s, d,&ix,&iy,1);
       break;
      
      case FINE:
	done=1;
	XORCross(ix,iy);
	/*Cross should be erased now that we have made our selection.*/
	/*Seems XORing it with new draw can tend to bring it back randomly
	depending on the order of window expose events.  Best not
	to do the XORCross function at all.*/
	DONT_XORCross = 1;
	xpp_ui.auto_grab_end(1);
	break;
      case ESC:
	done=-1;
	xpp_ui.auto_grab_end(-1);
	break;
      }
    }
    
  }
  /* check mark_flag branch similarity */
  if(diagram_mark.state==2){
    if(diagram_mark.start_branch!=diagram_mark.end_branch)
      diagram_mark.state=0;
  }
  if(done==1) grab_diagram_point(s, d);
  finish_grab(s);
}

/* takes diagram point d exactly as traverse_diagram's Return does: grabpt,
   the parameters, the initial condition. The caller then calls
   finish_grab(), which traverse_diagram does whether or not a point was
   taken. */
static void grab_diagram_point(xpp::Session &s, const DIAGRAM *d)
{
  int i;
  grabpt.ibr=d->ibr;
  grabpt.lab=d->lab;
  for(i=0;i<8;i++)
    grabpt.par[i]=d->par[i];
  grabpt.per=d->per;
  grabpt.torper=d->torper;
  for(i=0;i<s.model().node;i++){
    grabpt.uhi[i]=d->uhi[i];
    grabpt.ulo[i]=d->ulo[i];
    grabpt.u0[i]=d->u0[i];
    grabpt.ubar[i]=d->ubar[i];
    setvar(s,i+1,grabpt.u0[i]);
  }
  xpp::get_ic(s,0,grabpt.u0);
  grabpt.flag=1;
  grabpt.itp=d->itp;
  grabpt.nfpar=d->nfpar;
  auto_set_pars_from(s, grabpt.par);
}

/* what follows a grab: derived values, tables and the shown values */
static void finish_grab(xpp::Session &s)
{
  xpp::evaluate_derived(s);
  xpp::ok_or_show(xpp::redo_all_fun_tables(s));
  redraw_params();
  redraw_ics();
}

/* the scriptable grabs' common end: 0 and nothing changed without a point */
static int grab_and_finish(xpp::Session &s, const DIAGRAM *d)
{
  if(d==NULL) return 0;
  grab_diagram_point(s, d);
  finish_grab(s);
  return 1;
}

/* grabs the diagram point labelled lab exactly as an interactive grab
   ending with Return on it would (docs/protocol.md "Grab by point"): same
   grabpt, parameters, info strip and stability circle, and what a
   following Run starts from. Returns 0 and changes nothing for a label no
   stored point has. */
int auto_grab_label(xpp::Session &s, int lab)
{
  return grab_and_finish(s, diagram_of_label(s,lab));
}

/* grabs the index'th (1-based) stored point of AUTO's type `type`
   (auto_bif_sym's BP/EP/HB/LP/MX/PD/TR/UZ), in stored order: "the 2nd HB".
   Returns 0 and changes nothing when there is no such point. */
int auto_grab_type_index(xpp::Session &s, const char *type, int index)
{
  const DIAGRAM *d;
  int count=0;
  if(index<1 || diagram_count(s.diagram)<2) return 0;
  for(d=diagram_first(s.diagram);d!=NULL;d=diagram_next(s.diagram,d)){
    if(d->lab!=0 && strcmp(auto_bif_sym(d->itp),type)==0){
      count++;
      if(count==index) break;
    }
  }
  return grab_and_finish(s, d);
}

void MarkAuto(int x, int y)
{

  LineWidth(2);
  ALINE(x-8,y-8,x+8,y+8);
  ALINE(x+8,y-8,x-8,y+8);
  LineWidth(1);

}

void clear_msg(xpp::Session &s)
{
  s.auto_state.bifur.hinttxt.clear();
  xpp_ui.auto_show_hint(s);
}

void auto_update_view(xpp::Session &s, float xlo,float xhi, float ylo, float yhi)
{
              s.auto_state.axes().xmin=xlo;
	      s.auto_state.axes().ymin=ylo;
	      s.auto_state.axes().xmax=xhi;
	      s.auto_state.axes().ymax=yhi;
	      redraw_diagram(s);

}

/* the pointer moved to pixel (i,j) of the diagram: show its coordinates */
void auto_motion_xy(xpp::Session &s, int i,int j)
{
  double x,y;
    x=s.auto_state.axes().xmin+static_cast<double>((i-s.auto_state.bifur.x0))*(s.auto_state.axes().xmax-s.auto_state.axes().xmin)/static_cast<double>(s.auto_state.bifur.wid);
    y=s.auto_state.axes().ymin+static_cast<double>((s.auto_state.bifur.y0-j+s.auto_state.bifur.hgt))*(s.auto_state.axes().ymax-s.auto_state.axes().ymin)/static_cast<double>(s.auto_state.bifur.hgt);
    auto_point_xy(s, x,y);
}

void auto_point_xy(xpp::Session &s, double x,double y)
{
    s.auto_state.bifur.hinttxt=xpp::format("x={:g},y={:g}",x,y);
    storeautopoint(s, x,y);
    xpp_ui.auto_show_hint(s);
}
