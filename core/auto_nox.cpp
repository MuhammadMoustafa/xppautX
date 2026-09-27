#include "model.h"
#include "integrate.h"
#include "storage.h"
#include "form_ode.h"
#include "xpp_log.h"
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <string_view>
#include "numerics.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include <string.h>
#include "autevd.h"
#include "auto_stop.h"
#include "auto_stability.h"
#include "csv_export.h"
#include <libgen.h>
#include "graf_par.h"


#include "pp_shoot.h"

#include "xpp_files.h"
#include "load_eqn.h"


#include "parserslow.h"

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
#include "my_rhs.h"    /* extra() */
#include "menus.h"
#include "my_ps.h"

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

int TypeOfCalc=0;
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

#define DISCRETE 0
/* the label the running continuation started from (Auto.irs), for its
   first point: do_auto sets it, addbif takes it (auto_run_from_take) */
static int run_from;

extern int fp8_is_open;

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
int SEc=20;
int UEc=0;
int SPc=26;
int UPc=28;
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

int RestartLabel=0;
int auto_ntst=15,auto_nmx=200,auto_npr=50,auto_ncol=4;
double auto_ds=.02,  auto_dsmax=.5,  auto_dsmin=.001;
double auto_rl0=0.0,auto_rl1=2,auto_a0=0.0,auto_a1=1000.;
double  auto_xmax=2.5,  auto_xmin=-.5,auto_ymax=3.0,auto_ymin=-3.0;
double auto_epsl=1e-4,auto_epsu=1e-4,auto_epss=1e-4;
int auto_var=0;

static int load_all_labeled_orbits=0;

int SuppressBP=0;
ROTCHK blrtn;

/* gogoauto.c and diagram.cpp declare these in no header */
extern "C" int go_go_auto(void);
extern "C" void load_browser_with_branch(int ibr, int pts, int pte);

static GRABPT grabpt;

int AutoTwoParam=0;
int NAutoPar=8;
int Auto_index_to_array[8];
int AutoPar[8];

double outperiod[20];
integer UzrPar[20];
int NAutoUzr;

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
static void auto_set_pars_from(const double *par)
{
    for (int i = 0; i < NAutoPar; i++) {
        const int idx = Auto_index_to_array[i];
        if (std::isfinite(par[i])) constants[idx] = par[i];
        else
            xpp::log(XPP_LOG_WARN, "AUTO: {} from the diagram is not finite; keeping {:.16g}",
                     xpp::model().upar_names[AutoPar[i]], constants[idx]);
    }
}

/* do_auto's cleanup after go_go_auto(): a run that diverges can leave
   AUTO's own working copy of a continuation parameter (autpp.cpp's func,
   synced into constants[] every RHS call) not finite. Restore what the
   run started from instead of leaving -nan in the model (QA SCI-001). */
static void auto_restore_finite_pars(const double *before)
{
    for (int i = 0; i < NAutoPar; i++) {
        const int idx = Auto_index_to_array[i];
        if (!std::isfinite(constants[idx])) {
            xpp::log(XPP_LOG_WARN, "AUTO: the run left {} not finite; keeping {:.16g}",
                     xpp::model().upar_names[AutoPar[i]], before[i]);
            constants[idx] = before[i];
        }
    }
}

int HomoFlag=0;
double homo_l[100],homo_r[100];
static double HOMO_SHIFT=0.0;

BIFUR Auto;
ADVAUTO aauto;

int NewPeriodFlag;

static AUTOAX Old1p;
static AUTOAX Old2p;

/* color plot stuff */
void colset(int type )
{
  switch(type) {
  case CSEQ:
    autocol(SEc);
    break;
 case CUEQ:
    autocol(UEc);
    break;
 case SPER:
    autocol(SPc);
    break;
 case UPER:
    autocol(UPc);
    break;
  }
  
}

void pscolset2(int flag2)
{
   switch(flag2){
  case LPE2:
    set_linestyle(LPE_color-19);
    break;
  case LPP2:
    set_linestyle(LPP_color);
    break;
  case HB2:
    set_linestyle(HB_color-19);
    break;
  case TR2:
    set_linestyle(TR_color-19);
    break;
  case BR2:
    set_linestyle(BR_color-19);
    break;
  case PD2:
    set_linestyle(PD_color-19);
    break;
  case FP2:
     set_linestyle(FP_color-19);
    break;
  default:
    set_linestyle(0);
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

void storeautopoint(double x,double y)
{
  if(Auto.plot==P_P){
    XfromAuto=x;
    YfromAuto=y;
    FromAutoFlag=1;
  }
}
void setautopoint()
{
  if(FromAutoFlag)
    {
      FromAutoFlag=0;
      set_val(xpp::model().upar_names[AutoPar[Auto.icp1]],XfromAuto);
      set_val(xpp::model().upar_names[AutoPar[Auto.icp2]],YfromAuto);
      evaluate_derived();
      redo_all_fun_tables();
      redraw_params();
    }
}
      
namespace {
/* AUTO's parameter k's name, or "" (auto_par_name) */
std::string par_label(int k)
{
  const char *p=auto_par_name(k);
  return p?p:"";
}

/* the diagram's axis labels: its first parameter, and what Auto.plot
   shows up the side */
struct AxisLabels {
  std::string x,y;
};
AxisLabels axis_labels()
{
  AxisLabels l;
  l.x=par_label(Auto.icp1);
  switch(Auto.plot){
  case HI_P:
  case HL_P:
    l.y=xpp::model().uvar_names[Auto.var];
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
    l.y=par_label(Auto.icp2);
    break;
  case AV_P:
    l.y=xpp::model().uvar_names[Auto.var]+"_bar";
    break;
  }
  return l;
}
} // namespace

void get_auto_str(std::string &xlabel, std::string &ylabel)
{
  AxisLabels l=axis_labels();
  xlabel=std::move(l.x);
  ylabel=std::move(l.y);
}

/* the diagram's axes in a PostScript or SVG export (diagram.cpp
   export_diagram), whichever ps_init/svg_init began */
void draw_export_axes()
{
 set_scale(Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax);
 const AxisLabels l=axis_labels();
 Box_axis(Auto.xmin,Auto.xmax,Auto.ymin,Auto.ymax,l.x.c_str(),l.y.c_str(),0);
}

void draw_bif_axes()
{
 int x0=Auto.x0,y0=Auto.y0,ii,i0;
 int x1=x0+Auto.wid,y1=y0+Auto.hgt;
 std::string junk;
 clear_auto_plot();
 ALINE(x0,y0,x1,y0);
 ALINE(x1,y0,x1,y1);
 ALINE(x1,y1,x0,y1);
 ALINE(x0,y1,x0,y0);
 junk=xpp::format("{:g}",Auto.xmin);
 ATEXT(x0,y1+text_metrics.small_height+2,junk.c_str());
 junk=xpp::format("{:g}",Auto.xmax);
 ii=static_cast<int>(junk.size())*text_metrics.small_width;
 ATEXT(x1-ii,y1+text_metrics.small_height+2,junk.c_str());
 junk=xpp::format("{:g}",Auto.ymin);
 ii=static_cast<int>(junk.size());
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y1,junk.c_str());
 junk=xpp::format("{:g}",Auto.ymax);
 ii=static_cast<int>(junk.size());
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y0+text_metrics.small_height,junk.c_str());
 const AxisLabels l=axis_labels();
 ATEXT((x0+x1)/2,y1+text_metrics.small_height+2,l.x.c_str());
 ATEXT(10*text_metrics.small_width,text_metrics.small_height,l.y.c_str());
 auto_diagram(NULL); /* the data of the diagram starts again too */
 refreshdisplay();
}

int IXVal(double x)
{
  double temp=static_cast<double>(Auto.wid)*(x-Auto.xmin)/(Auto.xmax-Auto.xmin);
  return (static_cast<int>(temp)+Auto.x0);
}

int IYVal(double y)
{
  double temp=static_cast<double>(Auto.hgt)*(y-Auto.ymin)/(Auto.ymax-Auto.ymin);
  return(Auto.hgt-static_cast<int>(temp)+Auto.y0);
}

int chk_auto_bnds(int ix,int iy)
{
  int x1=Auto.x0,x2=Auto.x0+Auto.wid;
  int y1=Auto.y0,y2=Auto.y0+Auto.hgt;
  if((ix>=x1)&&(ix<x2)&&(iy>=y1)&&(iy<y2))return 1;
  return 0;
}
void close_auto(int flg) /* labels compatible with A2K  */
{
  /* Close fp8 before the renames below: Windows refuses rename()/remove()
     on a file that is still open (see xpp_files_move), which left
     fort.8 behind next to <model>.s with the handle leaked. Linux allows
     renaming/removing an open file, which is likely why this was never
     turned on upstream -- it was dead code there, not a deliberate
     no-op. */
  if(fp8_is_open){
      xpp::UniqueFile{fp8}.reset(); /* autlib1.cpp's xpp_files_open_stream, closed */
      fp8=NULL;
      fp8_is_open=0;
  }
  if(flg==0) {/*Overwrite*/
    xpp_files_move(fort7.c_str(),(this_auto_file+".b").c_str());
    xpp_files_move(fort9.c_str(),(this_auto_file+".d").c_str());
    xpp_files_move(fort8.c_str(),(this_auto_file+".s").c_str());
  }
  else {/*APPEND*/
    xpp_files_prepend(fort7.c_str(),(this_auto_file+".b").c_str());
    xpp_files_prepend(fort9.c_str(),(this_auto_file+".d").c_str());
    xpp_files_prepend(fort8.c_str(),(this_auto_file+".s").c_str());
  }

    xpp_files_remove(fort8.c_str());

    fp8_is_open=0;
    xpp_files_remove(fort7.c_str());
    xpp_files_remove(fort9.c_str());
    xpp_files_remove(fort3.c_str());

}

/* AUTO writes fort.3/7/8/9 under HOME. A HOME that is set but unusable
   (missing, not writable) must fall back to the model's directory like an
   unset one, or the opens fail deep inside autlib1.c. */
static const char *auto_home_dir(char *dname)
{
  const char *home;

  /* xppautX gives each session its own directory (xpp_globals.h) */
  if (!program.auto_dir.empty())
    return program.auto_dir.c_str();

  home = getenv("HOME");
  if (home == NULL || !xpp_files_dir_writable(home))
    home = dname;
  return home;
}

void create_auto_file_name()
{
  /* basename()/dirname() may write into their argument or return a
     pointer into it, so each needs its own writable, NUL-terminated
     copy of xpp::model().this_file (std::string::data() is both since C++17) */
  std::string basec = xpp::model().this_file, dirc = xpp::model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* HOME = auto_home_dir(dname);

  this_auto_file=xpp::format("{}/{}",HOME,bname);
}

void open_auto(int flg) /* compatible with new auto */
{
  std::string basec = xpp::model().this_file, dirc = xpp::model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* HOME = auto_home_dir(dname);

  this_auto_file=xpp::format("{}/{}",HOME,bname);
  fort3=xpp::format("{}/fort.3",HOME);
  fort7=xpp::format("{}/fort.7",HOME);
  fort8=xpp::format("{}/fort.8",HOME);
  fort9=xpp::format("{}/fort.9",HOME);

  if(flg==1){
    xpp_files_copy((this_auto_file+".s").c_str(),fort3.c_str());
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
static int run_stability_kind(void)
{
  if(AutoTwoParam!=0||Auto.isw==2)return AUTO_STABILITY_OTHER;
  if(Auto.ips==2)return AUTO_STABILITY_PERIODIC;
  if(Auto.ips==1||Auto.ips==-1)return AUTO_STABILITY_STEADY;
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
static void stability_run_start(void)
{
  const DIAGRAM *d=Auto.irs>0?diagram_of_label(Auto.irs):NULL;
  if(d==NULL){
    auto_stability_run_start(run_stability_kind(),Auto.isw,AUTO_STABILITY_NONE,0,0,NULL,NULL);
    return;
  }
  auto_stability_run_start(run_stability_kind(),Auto.isw,point_stability_kind(d),d->itp,NODE,d->evr,d->evi);
}

/* MAIN Running routine  Assumes that Auto structure is set up */
static int auto_depth; /* do_auto's own follow-up runs (RestartLabel) are one run */

void do_auto(int iold, int isave, int itp)
{
      redraw_auto_menus();
      
    set_auto(); /* this sets up all the continuation initialization 
                   it is equivalent to reading in auto parameters
                   and running init in auto 
		*/
 
    open_auto(iold); /* this copies the relevant files .s  to fort.3 */
    if(auto_depth++==0)auto_stop_clear(); /* xppautX: T23: why this run's branches end */
    xpp_job_begin(0); /* Abort cancels it (xpp_job.h) */
    run_from=Auto.irs>0?Auto.irs:0; /* the diagram's data say where the run started */
    stability_run_start(); /* what its first point's stability is (auto_stability.h) */
    {
        std::array<double, 8> before{}; /* AutoPar's size */
        for (int i = 0; i < NAutoPar; i++) before[i] = constants[Auto_index_to_array[i]];
        go_go_auto(); /* this complets the initialization and calls the
                          main routines
                       */
        auto_restore_finite_pars(before.data()); /* leave no NaN parameter behind (QA SCI-001) */
    }
    run_from=0;
    if(xpp_job_cancelled())RestartLabel=0; /* xppautX: cancel: no follow-up run */
    xpp_job_end();
    /*     run_aut(Auto.nfpar,itp); THIS WILL CHANGE TO gogoauto stuff */ 
    close_auto(isave); /* this copies fort.8 to the .s file and other 
                          irrelevant stuff 
		       */
    
    if(RestartLabel!=0){
      xpp_log_auto("RestartLabel=%d itp=%d ips=%d nfpar=%d ilp=%d isw=%d isp=%d A2p=%d \n",RestartLabel,Auto.itp, Auto.ips,Auto.nfpar,Auto.ilp,Auto.isw,Auto.isp,AutoTwoParam);
      Auto.irs=RestartLabel;
      RestartLabel=0;
      do_auto(iold,isave, Auto.itp);
      
    }
    auto_depth--;
     ping();
      redraw_params();
}

void set_auto() /* Caution - need to include NICP here */
{
  NAutoUzr=Auto.nper;
  init_auto(NODE,Auto.nfpar,Auto.nbc,Auto.ips,Auto.irs,Auto.ilp,Auto.ntst,Auto.isp,
	    Auto.isw,Auto.nmx,Auto.npr,Auto.ds,Auto.dsmin,
	    Auto.dsmax,Auto.rl0,Auto.rl1,Auto.a0,Auto.a1,Auto.icp1,
	    Auto.icp2,Auto.icp3,Auto.icp4,Auto.icp5,Auto.nper,Auto.epsl,Auto.epsu,Auto.epss,Auto.ncol);
  
}
int auto_name_to_index(std::string_view s)
{
  int i,in;
  find_variable(s,&in);
  if(in==0)return(10);
  in=find_user_name(PARAM_BOX,s);
  for(i=0;i<NAutoPar;i++)
    if(AutoPar[i]==in)return(i);
  return(-1);
}
const char *auto_par_name(int k)
{
  return k>=0&&k<NAutoPar&&AutoPar[k]>=0&&AutoPar[k]<NUPAR?xpp::model().upar_names[AutoPar[k]].c_str():NULL;
}

namespace {
/* AUTO's parameter index's name, T for the period; empty for no such
   parameter */
std::string par_or_period_name(long index)
{
  if(index==AUTO_PERIOD_INDEX)return "T";
  return index>=0&&index<NAutoPar?par_label(static_cast<int>(index)):std::string();
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
   fit (names go to XPP_NAME_MAX) is shortened with a '~' (short_name) and
   still leaves a blank between it and the next heading: the column stays
   14 wide so the numbers below stay under it. */
std::string col_centre(const std::string &s)
{
  const std::string t=short_name(s,AUTO_COL_W-1);
  const int n=static_cast<int>(t.size());
  const int l=(AUTO_COL_W-n)/2;
  return std::string(static_cast<size_t>(l),' ')+t+std::string(static_cast<size_t>(AUTO_COL_W-n-l),' ');
}

std::string screen_col(const char *col)
{
  long p;
  const char *s=col;
  while(isspace(static_cast<unsigned char>(*s)))s++;
  if(strncmp(s,"PAR(",4)==0&&read_long(s+4,p)){
    const std::string name=par_or_period_name(p);
    if(!name.empty())return col_centre(name);
  }
  /* U(n), and the MAX(n) / MIN(n) a periodic branch prints, where AUTO has
     overwritten the U itself */
  const char *q=strchr(col,'(');
  if(q!=NULL&&strstr(col,"PAR")==NULL&&read_long(q+1,p)&&p>=1&&p<=NODE){
    size_t n=static_cast<size_t>(q-col);
    if(n>0&&col[n-1]=='U')n--; /* the name replaces the U */
    /* keep what stands in front of it: MAX, MIN, L2-NORM, INTEGRAL */
    std::string pre(col,std::min<size_t>(n,AUTO_COL_W));
    const size_t a=pre.find_first_not_of(' '),b=pre.find_last_not_of(' ');
    pre=a==std::string::npos?std::string():pre.substr(a,b-a+1);
    return col_centre(pre+(pre.empty()?"":" ")+xpp::model().uvar_names[p-1]);
  }
  return std::string(std::string_view(col).substr(0,AUTO_COL_W));
}
} // namespace

std::string auto_screen_col(const std::string &col)
{
  return screen_col(col.c_str());
}

void auto_per_par()
{
  
  static const char *m[]={"0","1","2","3","4","5","6","7","8","9"};
  static const char *const key="0123456789";
  std::array<std::string, 9> values;
  static const char *n[]={"Uzr1","Uzr2","Uzr3","Uzr4","Uzr5",
		      "Uzr6","Uzr7","Uzr8","Uzr9"};
  int status,i,in;
  char ch;
  /* "Mark values" (T21): AUTO labels (UZ) the points where a parameter or
     the period reaches one of these values */
  ch=static_cast<char>(auto_pop_up_list("Mark values: how many?",m,key,10,12,Auto.nper,10,10,no_hint,
		       Auto.hinttxt.c_str()));
  for(i=0;i<10;i++)
    if(ch==key[i])Auto.nper=i;
  NAutoUzr=Auto.nper;
  if(Auto.nper>0){
    for(i=0;i<9;i++){
      values[i] = xpp::format("{}={:g}", par_or_period_name(Auto.uzrpar[i]), Auto.period[i]);
    }
    status=do_string_box(5,2,"Mark values (UZ): parameter=value or per=value",n,values,45);
    if(status!=0)
      for(i=0;i<9;i++){
	xpp::Tokens tokens(values[i]);
	in=auto_name_to_index(tokens.next("=").value_or(std::string_view()));
	if(in>=0){
	  Auto.uzrpar[i]=in;
	  Auto.period[i]=atof(tokens.text("@").c_str());
	}
      }
  }
  for(i=0;i<9;i++){
    outperiod[i]=Auto.period[i];
    UzrPar[i]=Auto.uzrpar[i];
  }
  
}

/* auto parameters are 1-8 (0-7) and since there are only 8, need to associate them
   with real xpp parameters for which there may be many 
*/
void auto_params()
{
  static const char *n[]={"*2Par1","*2Par2","*2Par3","*2Par4","*2Par5","*2Par6","*2Par7","*2Par8"};
  int status,i,in;
  std::array<std::string, 8> values;
  for(i=0;i<8;i++){
    if(i<NAutoPar)  values[i] = xpp::model().upar_names[AutoPar[i]];
    else values[i].clear();
  }
  static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),
                            XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(2)};
  status=do_string_box_of(8,1,"Parameters",n,values,38,kinds);
  if(status!=0){
    for(i=0;i<8;i++){
      if(i<NAutoPar){
	in=find_user_name(PARAM_BOX,values[i].c_str());
	if(in>=0){
	  AutoPar[i]=in;
	  in=get_param_index(values[i].c_str());
	  Auto_index_to_array[i]=in;
	}
      }
    }
  }
}

void auto_num_par()
{
  /* grouped by what they do, which upstream's order was not: the box is 7
     rows by 4 columns, so a column is a group. Mesh and step size, then the
     ranges and tolerances, then the solver's integer knobs. */
  static const char *n[]={"Ntst","Nmax","NPr","Ncol","Ds","Dsmin","Dsmax",
		    "Par Min","Par Max","Norm Min","Norm Max","EPSL","EPSU","EPSS",
                    "IAD","MXBF","IID","ITMX","ITNW","NWTN","IADS","SuppBP"};
  int status;
  std::array<std::string, 22> values;
  values[0] = xpp::format("{:d}", Auto.ntst);
  values[1] = xpp::format("{:d}", Auto.nmx);
  values[2] = xpp::format("{:d}", Auto.npr);
  values[3] = xpp::format("{:d}", Auto.ncol);
  values[4] = xpp::format("{:g}", Auto.ds);
  values[5] = xpp::format("{:g}", Auto.dsmin);
  values[6] = xpp::format("{:g}", Auto.dsmax);
  values[7] = xpp::format("{:g}", Auto.rl0);
  values[8] = xpp::format("{:g}", Auto.rl1);
  values[9] = xpp::format("{:g}", Auto.a0);
  values[10] = xpp::format("{:g}", Auto.a1);
  values[11] = xpp::format("{:g}", Auto.epsl);
  values[12] = xpp::format("{:g}", Auto.epsu);
  values[13] = xpp::format("{:g}", Auto.epss);
  values[14] = xpp::format("{:d}", aauto.iad);
  values[15] = xpp::format("{:d}", aauto.mxbf);
  values[16] = xpp::format("{:d}", aauto.iid);
  values[17] = xpp::format("{:d}", aauto.itmx);
  values[18] = xpp::format("{:d}", aauto.itnw);
  values[19] = xpp::format("{:d}", aauto.nwtn);
  values[20] = xpp::format("{:d}", aauto.iads);
  values[21] = xpp::format("{:d}", SuppressBP); 

  static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
  status=do_string_box_of(7,4,"AutoNum",n,values,25,kinds);
  if(status!=0){
    Auto.ntst=atoi(values[0].c_str());
    Auto.nmx=atoi(values[1].c_str());
    Auto.npr=atoi(values[2].c_str());
    Auto.ncol=atoi(values[3].c_str());
    Auto.ds=atof(values[4].c_str());
    Auto.dsmin=atof(values[5].c_str());
    Auto.dsmax=atof(values[6].c_str());
    Auto.rl0=atof(values[7].c_str());
    Auto.rl1=atof(values[8].c_str());
    Auto.a0=atof(values[9].c_str());
    Auto.a1=atof(values[10].c_str());
    Auto.epsl=atof(values[11].c_str());
    Auto.epsu=atof(values[12].c_str());
    Auto.epss=atof(values[13].c_str());
    aauto.iad=atoi(values[14].c_str());
    aauto.mxbf=atoi(values[15].c_str());
    aauto.iid=atoi(values[16].c_str());
    aauto.itmx=atoi(values[17].c_str());
    aauto.itnw=atoi(values[18].c_str());
    aauto.nwtn=atoi(values[19].c_str());
    aauto.iads=atoi(values[20].c_str());
    SuppressBP=atoi(values[21].c_str());

  }

}    

void auto_plot_par()
{

  static const char *m[]={"Hi","Norm","hI-lo","Period","Two par","(Z)oom in","Zoom (O)ut",
		      "last 1 par", "last 2 par","Fit",
		    "fRequency","Average","Default","Scroll"};
  static const char *const key="hniptzo12frads";
  char ch;

  static const char *n[]={"*1Y-axis","*2Main Parm", "*2Secnd Parm", "Xmin", "Ymin",
		   "Xmax", "Ymax"};
  std::array<std::string, 7> values;
  int  status,i;
  int ii1,ii2,ji1,ji2;
  int i1=Auto.var+1;
  ch=static_cast<char>(auto_pop_up_list("Plot Type",m,key,14,10,Auto.plot,10,50,
		       aaxes_hint,Auto.hinttxt.c_str()));
  if(ch==ESC) 
    return;
  for(i=0;i<5;i++){
    if(ch==key[i])Auto.plot=i;
  }
  if(ch==key[10])Auto.plot=10;
  if(ch==key[11])Auto.plot=11;
  if(ch==key[5]){
    if(auto_rubber(&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_in(ii1,ji1,ii2,ji2);
      redraw_diagram();
    }
    return;
  }
  
  if(ch==key[6]){
    if(auto_rubber(&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_out(ii1,ji1,ii2,ji2);
     
      redraw_diagram();
    }
    return;
  }

  /* a new plot type or axes: the diagram drawn again in its quantities
     (it used to wait for reDraw, which a client drawing from data lacks) */
  if(ch==key[7]){
    load_last_plot(1);
    redraw_diagram();
    return;
  }

  if(ch==key[8]){
    load_last_plot(2);
    redraw_diagram();
    return;
  }

  if(ch==key[9]){
    auto_fit();
    redraw_diagram();
    return;
  }
  if(ch==key[12]){
    auto_default();
    redraw_diagram();
    return;
  }
  if(ch==key[13]){
    auto_scroll_window();
    redraw_diagram();
    return;
  }
  values[0] = ind_to_sym(i1);
  values[1] = xpp::model().upar_names[AutoPar[Auto.icp1]];
  values[2] = xpp::model().upar_names[AutoPar[Auto.icp2]];
  values[3] = xpp::format("{:g}", Auto.xmin);
  values[4] = xpp::format("{:g}", Auto.ymin);
  values[5] = xpp::format("{:g}", Auto.xmax);
  values[6] = xpp::format("{:g}", Auto.ymax);
  static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
  status=do_string_box_of(7,1,"AutoPlot",n,values,31,kinds);
  if(status!=0){
    /*  get variable names  */
    find_variable(values[0].c_str(),&i);
    if(i>0)
      Auto.var=i-1;
    /*  Now check the parameters  */
    i1=find_user_name(PARAM_BOX,values[1].c_str());
    if(i1>=0){
      for(i=0;i<NAutoPar;i++){
	if(i1==AutoPar[i]){
	  Auto.icp1=i;

	}
      }
    }
     i1=find_user_name(PARAM_BOX,values[2].c_str());
    if(i1>=0){
      for(i=0;i<NAutoPar;i++){
	if(i1==AutoPar[i]){
	  Auto.icp2=i;
	}
      }
    }

    Auto.xmin=atof(values[3].c_str());
    Auto.ymin=atof(values[4].c_str());
    Auto.xmax=atof(values[5].c_str());
    Auto.ymax=atof(values[6].c_str());
    if(Auto.plot<4)keep_last_plot(1);
    if(Auto.plot==4)keep_last_plot(2);
    redraw_diagram();

}
}

void auto_default()
{
  Auto.xmin=auto_xmin;
  Auto.xmax=auto_xmax;
  Auto.ymin=auto_ymin;
  Auto.ymax=auto_ymax;
}

void auto_fit()
{
  double xlo=Auto.xmin,xhi=Auto.xmax,ylo=Auto.ymin,yhi=Auto.ymax;
  bound_diagram(&xlo,&xhi,&ylo,&yhi);
  Auto.xmin=xlo;
  Auto.xmax=xhi;
  Auto.ymin=ylo;
  Auto.ymax=yhi;
}
  
void auto_zoom_in(int i1, int j1, int i2, int j2)
{
   double x1,y1,x2,y2;
   int temp;
   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   double dx = (Auto.xmax-Auto.xmin);
   double dy = (Auto.ymax-Auto.ymin);
   x1 = Auto.xmin+static_cast<double>((i1-Auto.x0))*(dx)/static_cast<double>(Auto.wid);
   x2 = Auto.xmin+static_cast<double>((i2-Auto.x0))*(dx)/static_cast<double>(Auto.wid);
   y1 = Auto.ymin+static_cast<double>((Auto.hgt+Auto.y0-j1))*(dy)/static_cast<double>(Auto.hgt);
   y2 = Auto.ymin+static_cast<double>((Auto.hgt+Auto.y0-j2))*(dy)/static_cast<double>(Auto.hgt);
 
   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx/2;
	  dy = dy/2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  Auto.xmin=x1-dx/2;
	  Auto.xmax=x1+dx/2;
	  Auto.ymin=y1-dy/2;
	  Auto.ymax=y1+dy/2;
  }
  else
  {           
	  Auto.xmin=x1;
	  Auto.ymin=y1;
	  Auto.xmax=x2;
	  Auto.ymax=y2;     
  }
  	
}

void auto_zoom_out(int i1, int j1, int i2, int j2)
{
   double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
   int temp;
   double dx = (Auto.xmax-Auto.xmin);
   double dy = (Auto.ymax-Auto.ymin);
   double a1,a2,b1,b2;

   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   a1=static_cast<double>((i1-Auto.x0))/static_cast<double>(Auto.wid);
      a2=static_cast<double>((i2-Auto.x0))/static_cast<double>(Auto.wid);
      b1=static_cast<double>((Auto.hgt+Auto.y0-j1))/static_cast<double>(Auto.hgt);
      b2=static_cast<double>((Auto.hgt+Auto.y0-j2))/static_cast<double>(Auto.hgt);

   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx*2;
	  dy = dy*2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  Auto.xmin=x1-dx/2;
	  Auto.xmax=x1+dx/2;
	  Auto.ymin=y1-dy/2;
	  Auto.ymax=y1+dy/2;
  }
  else
  {           
    x1=(a1*Auto.xmax-a2*Auto.xmin)/(a1-a2);
    x2=(Auto.xmin-Auto.xmax+a1*Auto.xmax-a2*Auto.xmin)/(a1-a2);
    y1=(b1*Auto.ymax-b2*Auto.ymin)/(b1-b2);
    y2=(Auto.ymin-Auto.ymax+b1*Auto.ymax-b2*Auto.ymin)/(b1-b2);
	  Auto.xmin=x1;
	  Auto.ymin=y1;
	  Auto.xmax=x2;
	  Auto.ymax=y2;
  }

} 

void auto_xy_plot(double *x, double *y1, double *y2, double par1, double par2, double per, double *uhigh, double *ulow, double *ubar, double a)
{
 /* a plot type none of the cases know leaves the point at (par1, 0) */
 *x=par1;
 *y1=*y2=0.0;
 switch(Auto.plot){
  case HI_P:
    *x=par1;
    *y1=uhigh[Auto.var];
    *y2=*y1;
    break;
  case NR_P:
    *x=par1;
    *y1=a;
    *y2=*y1;
    break;
  case HL_P:
    *x=par1;
    *y1=uhigh[Auto.var];
    *y2=ulow[Auto.var];
    break;
  case AV_P:
    *x=par1;
    *y1=ubar[Auto.var];
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

void add_ps_point(double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
		  int type, int flg, int lab, int npar, int icp1, int icp2, int flag2,
		  double *evr, double *evi)
{
  double x,y1,y2,par1,par2=0;
  int type1=type;
  par1=par[icp1];
  if(icp2<NAutoPar)par2=par[icp2];
  auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    Auto.lastx=x;
    Auto.lasty=y1;
  }
  if(flag2==0&&Auto.plot==P_P)
    {
  
       return;
     }
  if(flag2>0&&Auto.plot!=P_P){
  
    return;
  }

  if((flag2>0)&&(Auto.plot==P_P))
   type1=CSEQ;
  switch(type1){
 
  case CSEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;

    if(plot_export.color){
      set_linestyle(1);
      if(flag2>0)pscolset2(flag2);
    }
    else 
      set_linestyle(8);
    line_abs(static_cast<float>(x),static_cast<float>(y1),static_cast<float>(Auto.lastx),static_cast<float>(Auto.lasty));
    break;
  case CUEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    if(Auto.plot!=P_P)
      {if(plot_export.color) set_linestyle(0);else set_linestyle(4);}
    else
      {
	pscolset2(flag2);
      
      }
    line_abs(static_cast<float>(x),static_cast<float>(y1),static_cast<float>(Auto.lastx),static_cast<float>(Auto.lasty));
    break;
  case UPER:
    if(plot_export.color) 
      set_linestyle(9); 
    else 
      set_linestyle(0);
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    PointType=UPT;
    point_abs(static_cast<float>(x),static_cast<float>(y1));
    point_abs(static_cast<float>(x),static_cast<float>(y2));
    break;
  case SPER:
    if(plot_export.color)
      set_linestyle(7);
    else
      set_linestyle(0);
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    PointType=SPT;
    point_abs(static_cast<float>(x),static_cast<float>(y1));
    point_abs(static_cast<float>(x),static_cast<float>(y2)); 
    break;
  }

  Auto.lastx=x;
  Auto.lasty=y1;
}

void auto_line(double x1i, double y1i, double x2i, double y2i)
{
  double xmin,ymin,xmax,ymax;
  float x1=x1i,x2=x2i,y1=y1i,y2=y2i;
  double x1d,x2d,y1d,y2d;
  float x1_out,y1_out,x2_out,y2_out;

  get_scale(&xmin,&ymin,&xmax,&ymax);
  set_scale(Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax);
  if(clip(x1,x2,y1,y2,&x1_out,&y1_out,&x2_out,&y2_out)){
    x1d=x1_out;
    x2d=x2_out;
    y1d=y1_out;
    y2d=y2_out;
    DLINE(x1d,y1d,x2d,y2d);
  }
 
  set_scale(xmin,ymin,xmax,ymax);
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
static void show_stab(const double *evr,const double *evi,int n,int periodic)
{
  auto_data_stab(evr,evi,n,periodic);
}

/* the colour colset() and colset2() give a point */
static int auto_point_color(int type,int flag2)
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
  case CSEQ: return SEc;
  case CUEQ: return UEc;
  case SPER: return SPc;
  case UPER: return UPc;
  }
  return 0;
}

/* this bit of code is for writing points - it only saves what is
   in the current view

*/
int check_plot_type(int flag2,int icp1, int icp2)
{
  if(flag2==0 && Auto.plot==P_P)
    return 0;
  if(flag2>0  && Auto.plot!=P_P)
    return 0; 
  if(icp1!=Auto.icp1)
    return 0;
  if(flag2>0 && icp2!=Auto.icp2)
    return 0;
  return 1;

} 
/* main plotting code  */ 
void add_point(double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
	       int type, int flg, int lab, int npar, int icp1, int icp2, int icp3, int icp4, int flag2,
	       double *evr, double *evi)
{
  double x,y1,y2,par1,par2=0;
  int ix,iy1,iy2,type1=type;
  std::string bob=xpp::format("{}",lab);
  XppDiagPoint dp;
  par1=par[icp1];
  if(icp2<NAutoPar)par2=par[icp2];
auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a); /* figure out who sits on axes */
  memset(&dp,0,sizeof dp);
  dp.ibr=dpt_ibr;
  dp.pt=dpt_ntot;
  dp.itp=dpt_itp;
  dp.node=dpt_node;
  dp.from=dpt_from;
  dp.type=type;
  dp.flag2=flag2;
  dp.newseg=(flg==0);
  dp.color=auto_point_color(type,flag2);
  dp.lw=(type==CSEQ||flag2>0)?2:1;
  dp.x=x;
  dp.y1=y1;
  dp.y2=y2;
  if(flg==0){
    Auto.lastx=x;
    Auto.lasty=y1;
  }
  ix=IXVal(x);
  iy1=IYVal(y1);
  iy2=IYVal(y2);
  autobw();
if(flag2==0&&Auto.plot==P_P) /* if the point was a 1 param run and we are in 2 param plot, skip */
    {
       if(flg==0)auto_diagram(&dp); /* not drawn, but the next line starts here */
       show_stab(evr,evi,NODE,type==SPER||type==UPER);
       refreshdisplay();
       return;
     }
if(flag2>0&&Auto.plot!=P_P){ /* two parameter and not in two parameter plot, just skip it */
    if(flg==0)auto_diagram(&dp);
    show_stab(evr,evi,NODE,type==SPER||type==UPER);
    refreshdisplay();
    return;
  }

 if((flag2>0)&&(Auto.plot==P_P))
   type1=CSEQ;
 switch(type1){
  
  case CSEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=1;
    LineWidth(2);
    colset(type);
    if(flag2>0)colset2(flag2);
    auto_line(x,y1,Auto.lastx,Auto.lasty);
    autobw();
    break;
  case CUEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=1;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    auto_line(x,y1,Auto.lastx,Auto.lasty);
    autobw();
    break;
  case UPER:
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=3;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    if(chk_auto_bnds(ix,iy1))Circle(ix,iy1,3);
    if(chk_auto_bnds(ix,iy2))Circle(ix,iy2,3);
    autobw();
    break;
  case SPER:
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=2;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    if(chk_auto_bnds(ix,iy1))FillCircle(ix,iy1,3);
    if(chk_auto_bnds(ix,iy2))FillCircle(ix,iy2,3);
    autobw();
    break;
  }
  if(lab!=0){
    if(icp1==Auto.icp1){
      if(flag2==0||(flag2>0&&Auto.icp2==icp2)){
	dp.lab=lab;
	LineWidth(1);
        if(chk_auto_bnds(ix,iy1)){
	ALINE(ix-4,iy1,ix+4,iy1);
	ALINE(ix,iy1-4,ix,iy1+4); }
	if(chk_auto_bnds(ix,iy2)){
	ALINE(ix-4,iy2,ix+4,iy2);
	ALINE(ix,iy2-4,ix,iy2+4);}
	if(chk_auto_bnds(ix,iy1))ATEXT(ix+8,iy1+8,bob.c_str()); 
      }
    }
  }

  Auto.lastx=x;
  Auto.lasty=y1;
  auto_diagram(&dp);
  show_stab(evr,evi,NODE,type==SPER||type==UPER);
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

void info_header(int flag2, int icp1, int icp2)
{
  /* the names head 10-wide columns of new_info's numbers */
  auto short10=[](std::string_view name){ return short_name(name,10); };
  const std::string p1name=short10(xpp::model().upar_names[AutoPar[icp1]]);
  const std::string p2name=icp2<NAutoPar?short10(xpp::model().upar_names[AutoPar[icp2]]):std::string("   ");
  const std::string vname=short10(xpp::model().uvar_names[Auto.var]);
  SmallBase();
  std::string bob=xpp::format("  Br  Pt Ty  Lab {:>10} {:>10}       norm {:>10}     period",
	  p1name,
	  p2name,
	  vname);
  draw_auto_info(bob.c_str(),10,text_metrics.small_height+1);

}

void new_info(int ibr, int pt, const char *ty, int lab, double *par, double norm, double u0, double per, int flag2, int icp1, int icp2)
{
  double p1,p2=0.0;
  clear_auto_info();
  info_header(flag2,icp1,icp2);
  p1=par[icp1];
  if(icp2<NAutoPar)p2=par[icp2];
  std::string bob=xpp::format("{:4} {:4} {:>2} {:4} {:10.4g} {:10.4g} {:10.4g} {:10.4g} {:10.4g}",
	  ibr,pt,ty,lab,p1,p2,norm,u0,per);
  draw_auto_info(bob.c_str(),10,2*text_metrics.small_height+2);
  refreshdisplay();
}

void traverse_out(DIAGRAM *d, int *ix, int *iy, int dodraw)
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
  if(icp2<NAutoPar)par2=par[icp2];  
    auto_xy_plot(&x,&y1,&y2,par1,par2,per,d->uhi,d->ulo,d->ubar,norm);
  
    *ix=IXVal(x);
    *iy=IYVal(y1);
    if (dodraw==1)
    {
      AutoDataInfo ai;
    	XORCross(*ix,*iy);
  	new_info(ibr,pt,symb,lab,par,norm,d->u0[Auto.var],per,flag2,icp1,icp2);
      /* what the strip shows, as data */
      ai.ibr=ibr;
      ai.pt=pt;
      ai.itp=itp;
      ai.lab=lab;
      ai.type=get_bif_type(ibr,pt,lab);
      ai.flag2=flag2;
      ai.node=d->index;
      ai.sym=symb;
      ai.p1name=xpp::model().upar_names[AutoPar[icp1]].c_str();
      ai.p1=par1;
      ai.p2name=icp2<NAutoPar?xpp::model().upar_names[AutoPar[icp2]].c_str():NULL;
      ai.p2=par2;
      ai.norm=norm;
      ai.vname=xpp::model().uvar_names[Auto.var].c_str();
      ai.u=d->u0[Auto.var];
      ai.per=per;
      ai.x=x;
      ai.y=y1;
      ai.y2=y2;
      /* the point and its circle together, after new_info (whose drawing
         may flush the autoinfo event): a flush between them sent the new
         circle with the old point when a step took over 0.1 s (autocheck
         under valgrind, W21) */
      auto_data_info(&ai);
      show_stab(evr,evi,NODE,ibr<0);
    }
    if(lab>0 && load_all_labeled_orbits>0)
      load_auto_orbitx(ibr,1,lab,per);

}

void do_auto_win()
{
  if(Auto.exist==0){
    if(NODE>NAUTO){
      err_msg(xpp::format("Auto restricted to less than {} variables",NAUTO).c_str());
      return;
    }
    make_auto("It's AUTO man!","AUTO");
    Auto.exist=1;
    
  }

}

void load_last_plot(int flg)
{
 if(flg==1) {/* one parameter */
  Auto.xmin=Old1p.xmin;
  Auto.xmax=Old1p.xmax;
  Auto.ymin=Old1p.ymin;
  Auto.ymax=Old1p.ymax;
  Auto.icp1=Old1p.icp1;
  Auto.icp2=Old1p.icp2;
  Auto.plot=Old1p.plot;
 Auto.var=Old1p.var;
}
if(flg==2) {/* two parameter */
  Auto.xmin=Old2p.xmin;
  Auto.xmax=Old2p.xmax;
  Auto.ymin=Old2p.ymin;
  Auto.ymax=Old2p.ymax;
  Auto.icp1=Old2p.icp1;
  Auto.icp2=Old2p.icp2;
  Auto.plot=Old2p.plot;
 Auto.var=Old2p.var;
}

}
void keep_last_plot(int flg)
{
  if(flg==1){ /* one parameter */
    Old1p.xmin=Auto.xmin;
    Old1p.xmax=Auto.xmax;
    Old1p.ymin=Auto.ymin;
    Old1p.ymax=Auto.ymax;
    Old1p.icp1=Auto.icp1;
    Old1p.icp2=Auto.icp2;
    Old1p.plot=Auto.plot;
    Old1p.var=Auto.var;
  }
  if(flg==2){
    Old2p.xmin=Auto.xmin;
    Old2p.xmax=Auto.xmax;
    Old2p.ymin=Auto.ymin;
    Old2p.ymax=Auto.ymax;
    Old2p.icp1=Auto.icp1;
    Old2p.icp2=Auto.icp2;
    Old2p.plot=P_P;
    Old2p.var=Auto.var;
  }
}

void init_auto_win()
{
  int i;
  if(NODE>NAUTO)return;
  start_diagram(NODE); 
  for(i=0;i<10;i++){
    Auto.period[i]=11.+3.*i;
    Auto.uzrpar[i]=10;
    outperiod[i]=Auto.period[i];
    UzrPar[i]=10;
  }
  NAutoPar=8;
  if(NUPAR<8)NAutoPar=NUPAR;
  for(i=0;i<NAutoPar;i++)AutoPar[i]=i;
  for(i=0;i<NAutoPar;i++){
    Auto_index_to_array[i]=get_param_index(xpp::model().upar_names[AutoPar[i]]);
  }
  Auto.nper=0;
  grabpt.flag=0;  /*  no point in buffer  */
  Auto.exist=0;
 blrtn.torper=TOR_PERIOD;
 create_auto_file_name();
 
/*  Control -- done automatically   */
  Auto.irs=0;
  Auto.ips=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.nbc=NODE;
  Auto.nfpar=1;
  HomoFlag=0;
/*  User controls this      */
  Auto.ncol=auto_ncol;
  Auto.ntst=auto_ntst;
  Auto.nmx=auto_nmx;
  Auto.npr=auto_npr;
  Auto.ds=auto_ds;
  Auto.dsmax=auto_dsmax;
  Auto.dsmin=auto_dsmin;
  Auto.rl0=auto_rl0;
  Auto.rl1=auto_rl1;
  Auto.a0=auto_a0;
  Auto.a1=auto_a1;
  
  Auto.epsl=auto_epsl;
    Auto.epsu=auto_epsu;
  Auto.epss=auto_epss;

/* The diagram plotting stuff    */

  Auto.xmax=auto_xmax;
  Auto.xmin=auto_xmin;
  Auto.ymax=auto_ymax;
  Auto.ymin=auto_ymin;
  Auto.plot=HL_P;
  Auto.var=auto_var;

/* xpp parameters    */
  
  Auto.icp1=0;
  Auto.icp2=1;
   Auto.icp3=1;
  Auto.icp4=1;
  Auto.icp5=1;
  keep_last_plot(1);
  keep_last_plot(2);
  aauto.iad=3;
  aauto.mxbf=5;
  aauto.iid=2;
  aauto.itmx=8;
  aauto.itnw=7;
  aauto.nwtn=3;
  aauto.iads=1;
  xAuto.nunstab=1;
  xAuto.nstab=NODE-1;
}

int yes_reset_auto()
{
  if(diagram_count()<=1)return(0);
 kill_diagrams();
 FromAutoFlag=0;
    grabpt.flag=0;
    xpp_files_remove((this_auto_file+".b").c_str());
    xpp_files_remove((this_auto_file+".d").c_str());
    xpp_files_remove((this_auto_file+".s").c_str());
    diagram_mark.state=0;
    return 1;
}
int reset_auto()
{
  char ch;
    if(diagram_count()<=1)return(0);
    ch=static_cast<char>(TwoChoice("YES","NO","Destroy AUTO diagram & files","yn"));
    if(ch!='y')return(0);
   
  return(yes_reset_auto());
}

void auto_grab()
{
  traverse_diagram();
} 

void get_start_period(double *p)
{
 *p=data_store.col[0][data_store.rows-1];
}
void find_best_homo_shift(int n)
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
  for(i=0;i<data_store.rows;i++){
    d=0.0;
    for(j=0;j<n;j++){
      d+=fabs(data_store.col[j+1][i]-homo_l[j]);
    }
    if(d<dmin){
      dmin=d;
      tshift=data_store.col[0][i];
    }
  }
  HOMO_SHIFT=tshift;
  xpp_log_auto("shifting %g\n",HOMO_SHIFT);
}
void get_shifted_orbit(double *u, double t, double p, int n)
{
  double ts;
  int i,i1,i2,ip,j;
  double lam;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  ts=fmod(t*p+HOMO_SHIFT,p);
  for(i=0;i<data_store.rows;i++){
    ip=(i+1)%data_store.rows;
    if((ts>=data_store.col[0][i])&&(ts<data_store.col[0][ip])){
      i1=i;
      i2=ip;
      lam=ts-data_store.col[0][i];
      for(j=0;j<n;j++)
	u[j]=(1.0-lam)*data_store.col[j+1][i1]+lam*data_store.col[j+1][i2];
      break;
    }
  }
}
void get_start_orbit(double *u, double t, double p, int n)
{
  double tnorm,lam;
  int i1,i2,j;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  tnorm=t*(data_store.rows-1);
  i1=static_cast<int>(tnorm);
  i2=i1+1;
  if(i2>=data_store.rows)i2-=data_store.rows;
  lam=(tnorm-static_cast<double>(i1));

   for(j=0;j<n;j++)
    u[j]=(1.0-lam)*data_store.col[j+1][i1]+lam*data_store.col[j+1][i2];
}
  
void auto_start_choice()
{
  static const char *m[]={"Steady state","Periodic","Bdry Value","Homoclinic","hEteroclinic"};
  static const char *const key="spbhe";
  char ch;
  HomoFlag=0;
  if(METHOD==DISCRETE){
    auto_new_discrete();
    return;
  }
  ch=static_cast<char>(auto_pop_up_list("Start",m,key,5,13,0,10,10,arun_hint,
		       Auto.hinttxt.c_str()));
   if(ch=='s'){
    auto_new_ss();
    return;
  }
  if(ch=='p'){
  auto_start_at_per();
    return;
  }
 if(ch=='b'){
   Auto.nbc=NODE;
   auto_start_at_bvp();
   return;
 }
 if(ch=='h'){
   HomoFlag=1;
   auto_start_at_homoclinic();
     return;
   }
     
 if(ch=='e'){
   HomoFlag=2;
   auto_start_at_homoclinic();
   return;
 }

  redraw_auto_menus();
}

void torus_choice()
{
  static const char *m[]={"Two Param","Fixed period","Extend"};
  /*static const char *m[]={"Fixed period","Extend"}; */
  static const char *const key="tfe";
  char ch;
  ch=static_cast<char>(auto_pop_up_list("Torus",m,key,3,10,0,10,10,
		       no_hint,Auto.hinttxt.c_str()));
   if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
      auto_2p_fixper();
    return;
  }
  if(ch=='t'){
    auto_torus();
    return;
    } 
  redraw_auto_menus();
}
 
void per_doub_choice()
{
  static const char *m[]={"Doubling","Two Param","Fixed period","Extend"};
  static const char *const key="dtfe";
  char ch;
  ch=static_cast<char>(auto_pop_up_list("Per. Doub.",m,key,4,10,0,10,10,no_hint,Auto.hinttxt.c_str()));
  if(ch=='d'){
    auto_period_double();
    return;
  }
   if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
      auto_2p_fixper();
    return;
  }
  if(ch=='t'){
    auto_twopar_double();
    return;
  }
  redraw_auto_menus();
}
  
void periodic_choice()
{
  static const char *m[]={"Extend","Fixed Period"};
  static const char *const key="ef";
  char ch;
  ch=static_cast<char>(auto_pop_up_list("Periodic ",m,key,2,14,0,10,10,
		       no_hint,Auto.hinttxt.c_str()));
  if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
    auto_2p_fixper();
    return;
  }

  redraw_auto_menus();
}

void hopf_choice()
{
  static const char *m[]={"Periodic","Extend","New Point","Two Param"};
  static const char *const key="pent";
  char ch;
  if(METHOD==DISCRETE){
    auto_2p_hopf();
    return;
  }

  ch=static_cast<char>(auto_pop_up_list("Hopf Pt",m,key,4,10,0,10,10,
		       no_hint,Auto.hinttxt.c_str()));
  if(ch=='p'){
    auto_new_per();
    return;
  }
  if(ch=='e'){
    auto_extend_ss();
    return;
  }
  if(ch=='n'){
    auto_new_ss();
    return;
  }
  if(ch=='t'){
    auto_2p_hopf();
    return;
  }
  redraw_auto_menus();
}

void auto_run()
{
  int itp1,itp2,itp,ips;
  char ch;
  if(grabpt.flag==0){   /* the first call to AUTO   */
    auto_start_choice();
    ping();return;
  }
  if(grabpt.lab==0){
    ch=static_cast<char>(TwoChoice("YES","NO","Not Labeled Pt: New Start?","y"));
    if(ch=='y')auto_start_diff_ss();
    ping();return;
  }
    
  itp=grabpt.itp;
  itp1=itp%10;
  itp2=itp/10;
  ips=Auto.ips;
  if(itp1==3||itp2==3){  /* its a HOPF Point  */
    hopf_choice();
    ping();return;
  }
  if(itp1==7||itp2==7){ /* period doubling */
    per_doub_choice();
    ping();return;
  }
  if(ips==9){
    auto_homo_choice(itp);
    ping(); return;
  }
  if(itp1==2||itp2==2){ /* limit point */
     Auto.ips=1;
     auto_2p_limit(Auto.ips);
    ping();return;
  }
  if(itp1==5||itp2==5){ /* limit pt of periodic or BVP */
    if(Auto.ips!=4)
      Auto.ips=2;  /* this is a bit dangerous - the idea is that
                      if you are doing BVPs, then that is all you are
                      doing  
		   */
    auto_2p_limit(Auto.ips);
    ping(); return;
  }
  if(itp1==6||itp2==6||itp1==1||itp2==1){ /* branch point  */ 

  auto_branch_choice(grabpt.ibr,ips);
    ping();
    return;
  }
  if(itp1==8||itp2==8){ /* Torus 2 parameter */
    torus_choice();
    ping();
    return;
  }
  if(grabpt.ibr<0) { /* its a periodic -- just extend it  */
    periodic_choice();
    ping();return;
  }
  if(grabpt.ibr>0&&ips!=4){ /*  old steady state -- just extend it  */
    auto_extend_ss();
    ping();return;
  }
  if(grabpt.ibr>0&&ips==4){
    auto_extend_bvp();
    ping();
    return;
  }
}

void auto_homo_choice(int itp)
{
  if(itp!=5)
    auto_extend_homoclinic();
  
}
void auto_branch_choice(int ibr, int ips)
{

  static const char *m[]={"Switch","Extend","New Point","Two Param"};
  static const char *const key="sent";
  char ch;
  int ipsuse;
  ch=static_cast<char>(auto_pop_up_list("Branch Pt",m,key,4,10,0,10,10,
		       no_hint,Auto.hinttxt.c_str()));

  if(ch=='s'){
       if(ibr<0&&ips==2)
      auto_switch_per();
    else 
      if(ips==4)
	auto_switch_bvp();
      else
	auto_switch_ss();
    return;
  }
  if(ch=='e'){
    auto_extend_ss();
    return;
  }
  if(ch=='n'){
    auto_new_ss();
    return;
  }
  if(ch=='t'){
 
    ipsuse=1;
    if(ips==4)
      ipsuse=4;
    if(ibr<0)
      ipsuse=2;
    auto_2p_branch(ipsuse);
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

void auto_start_diff_ss()
{
  TypeOfCalc=EQ1;
  Auto.ips=1;
  if(METHOD==DISCRETE)Auto.ips=-1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
  AutoTwoParam=0;
  do_auto(NO_OPEN_3,APPEND,Auto.itp);
}

void auto_start_at_bvp()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
 compile_bvp();
  if(BVP_FLAG==0)
    return; 
  TypeOfCalc=BV1;
 Auto.ips=4;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;

  Auto.isp=2;
  if(SuppressBP==1) Auto.isp=0;
    
  Auto.nfpar=1;
  AutoTwoParam=0;
  NewPeriodFlag=2;
  do_auto(opn,cls,Auto.itp);
}

void auto_start_at_per()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  TypeOfCalc=PE1;
  Auto.ips=2;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;

  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
  AutoTwoParam=0;
  NewPeriodFlag=1;
  do_auto(opn,cls,Auto.itp);
}

void auto_new_ss()
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  NewPeriodFlag=0;

  if(diagram_count()>1){
    ans=reset_auto();
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
      TypeOfCalc=EQ1;
  Auto.ips=1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
      if(SuppressBP==1) Auto.isp=0;;
  Auto.nfpar=1;
   AutoTwoParam=0;
  do_auto(opn,cls,Auto.itp);
}

void auto_new_discrete()
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  NewPeriodFlag=0;
  if(diagram_count()>1){
    ans=reset_auto();
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
  TypeOfCalc=DI1;
  Auto.ips=-1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
   AutoTwoParam=0; 
  do_auto(opn,cls,Auto.itp);
}
 
void auto_extend_ss()
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
  
      TypeOfCalc=EQ1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
    
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

int get_homo_info(int flg,int *nun,int *nst,double *ul, double *ur)
{
  std::array<std::string, 100> v;
  int n=2+2*NODE;
  int i;
  int flag=0;
  /* do_string_box_of's names are read-only (const char *const *): plain
     std::strings own the text, s just points at them for the call */
  std::vector<std::string> labels(n);
  labels[0]="dim unstable";
  v[0] = xpp::format("{:d}", *nun);
  labels[NODE+1]="dim stable";
  v[NODE+1] = xpp::format("{:d}", *nst);
  for(i=0;i<NODE;i++){
    labels[i+1]=xpp::model().uvar_names[i]+"_L";
    v[i+1] = xpp::format("{:g}", ul[i]);
    labels[i+2+NODE]=xpp::model().uvar_names[i]+"_R";
    v[i+2+NODE] = xpp::format("{:g}", ur[i]);
  }
  std::vector<const char*> s(n);
  for(i=0;i<n;i++) s[i]=labels[i].c_str();

  {
    std::vector<int> kinds(n, XPP_FIELD_NUMBER);
    kinds[0]=XPP_FIELD_INTEGER;
    kinds[NODE+1]=XPP_FIELD_INTEGER;
    flag=do_string_box_of(n/2,2,"Homoclinic info",s.data(),v,16,kinds.data());
  }
  if(flag!=0){
    *nun=atoi(v[0].c_str());
    *nst=atoi(v[NODE+1].c_str());
    for(i=0;i<NODE;i++){
      ul[i]=atof(v[i+1].c_str());
      if(HomoFlag==2)
	ur[i]=atof(v[i+2+NODE].c_str());
    }
  }
  return flag;
}

void auto_extend_homoclinic()
{
   Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;

      TypeOfCalc=HO2;
  AutoTwoParam=HO2;
  NewPeriodFlag=1;
  Auto.ips=9;

  Auto.nfpar=2;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=0;
  Auto.nbc=0;
  
  if(HomoFlag==1)
    xAuto.iequib=1;
  if(HomoFlag==2)
    xAuto.iequib=-2;

  do_auto(OPEN_3,APPEND,Auto.itp);

}

void auto_start_at_homoclinic()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  int flag;
  Auto.irs=0;
  Auto.itp=0;
    TypeOfCalc=HO2;

  AutoTwoParam=HO2;
  NewPeriodFlag=1;
  Auto.ips=9;

  Auto.nfpar=2;
  Auto.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  Auto.isw=1;
  Auto.isp=0;
  Auto.nbc=0;
  
  if(HomoFlag==1){
    xAuto.iequib=1;
    find_best_homo_shift(NODE);
  }
  if(HomoFlag==2)
    xAuto.iequib=-2;
  flag=get_homo_info(HomoFlag,&xAuto.nunstab,&xAuto.nstab,homo_l,homo_r);
  if(flag)do_auto(opn,cls,Auto.itp);

}
    
void auto_new_per() /* same for extending periodic  */
{
  blrtn.torper=grabpt.torper;
  
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
      TypeOfCalc=PE1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=1;
  Auto.ilp=1;
  Auto.isw=1; /* -1 */
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
    AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_extend_bvp() /* extending bvp */
{
      TypeOfCalc=BV1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=4;
    AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_switch_per()
{
      TypeOfCalc=PE1;
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=1; /*grabpt.nfpar;*/
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_switch_bvp()
{
     TypeOfCalc=BV1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=4;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_switch_ss()
{

      TypeOfCalc=EQ1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_limit(int ips)
{
  int ipsuse=1;
  int itp1,itp2;
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0; /* was 2 */
  /* fix ips now */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==5)||(itp2==5))
      ipsuse=2;
  }

  Auto.ips=ipsuse;
  AutoTwoParam=LPP2;
  if(ipsuse==1){
    TypeOfCalc=LPE2;
    AutoTwoParam=LPE2;
  }
  else{
    TypeOfCalc=LPP2;
    AutoTwoParam=LPP2;
  }
  do_auto(OPEN_3,APPEND,Auto.itp);
}

namespace {
/* continue a grabbed period doubling (PD2) or torus bifurcation (TR2) in
   two parameters: the same periodic restart, told apart by its kind */
void auto_2p_periodic(int kind)
{
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  AutoTwoParam=kind;
  TypeOfCalc=kind;
  Auto.ips=2;
  Auto.ilp=0;
  Auto.isw=2;
  Auto.isp=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}
} // namespace

void auto_twopar_double()
{
  auto_2p_periodic(PD2);
}

void auto_torus()
{
  auto_2p_periodic(TR2);
}

void auto_2p_branch(int ips)
{
 int ipsuse=1;
  int itp1,itp2; 
 blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0; /* was 2 */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==6)||(itp2==6))
      ipsuse=2;
  }

  Auto.ips=ipsuse;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=BR2;
      TypeOfCalc=BR2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_fixper()
{
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=1; /* was1 */
  Auto.isw=1;
  Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=FP2;
  TypeOfCalc=FP2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_hopf()
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
  
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=HB2;
    TypeOfCalc=HB2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_period_double()
{

 blrtn.torper=grabpt.torper;
  Auto.ntst=2*Auto.ntst;
  Auto.irs=grabpt.lab;
  Auto.nfpar=1; /* grabpt.nfpar; */

  Auto.itp=grabpt.itp;
  Auto.ilp=1;
  Auto.isw=-1;
  TypeOfCalc=PE1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

/**********   END RUN AUTO *********************/

void auto_err(const char *s)
{
  err_msg(s);
}

void load_auto_orbit()
{
  load_auto_orbitx(grabpt.ibr,grabpt.flag,grabpt.lab,grabpt.per);
}
  void load_auto_orbitx(int ibr,int flag, int lab, double per)
{
  double *x;
  int i,j,nstor;
  double u[NAUTO],t;
  double period;
  std::string string;
  int nrow,ndim,label,flg;

  if((ibr>0&&(Auto.ips!=4)&&(Auto.ips!=3)&&(Auto.ips!=9))||flag==0)return;
   /* either nothing grabbed or just a fixed point and that is already loaded */
  string=this_auto_file+".s";
  xpp::UniqueFile fp=xpp::open_read(string.c_str());
  if(!fp){
    auto_err("No such file");
    return;
  }
  label=lab;
  period=per;
  flg=move_to_label(label,&nrow,&ndim,fp.get());
  nstor=ndim;
  if(ndim>NODE)nstor=NODE;
  if(flg==0){
    xpp_log_auto("Could not find label %d in file %s \n",label,string.c_str());
    auto_err("Cant find labeled pt");
    return;
  }
  x=&data_store.current[0];
  for(i=0;i<nrow;i++){
    get_a_row(u,&t,ndim,fp.get());
    if(Auto.ips!=4) 
      data_store.col[0][i]=t*period;
    else
      data_store.col[0][i]=t;

    for(j=0;j<nstor;j++){
      data_store.col[j+1][i]=u[j];
      x[j]=u[j];
    }
    extra(x,static_cast<double>(data_store.col[0][i]),nstor,NEQ);
    for(j=nstor;j<NEQ;j++)
      data_store.col[j+1][i]=static_cast<float>(x[j]);
  }
  data_store.rows=nrow;
  refresh_browser(nrow);
  /* insert auxiliary stuff here */
  if(load_all_labeled_orbits==2)clr_all_scrns();
  drw_all_scrns();
}

void save_auto()
{

  int status;
  std::string filename=xpp::format("{}.auto",basename(this_auto_file.data()));
  status=file_selector("Save Auto",filename,"*.auto");
  if(status==0)return;
  /* written beside filename and renamed over it once whole */
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  status=save_auto_file(w.file());
  if(status!=1){
    /* an empty diagram: say so, and leave no file without orbits (nor
       replace an existing one with it) */
    w.abort();
    auto_err("Empty diagram -- nothing to save");
    return;
  }
  w.commit();
}

/* save_auto without its dialog (xpp_session.c): 1 written, else the
   diagram was empty and fp holds only the numerics and graph header */
int save_auto_file(FILE *fp)
{
  int status;
  save_auto_numerics(fp);
  save_auto_graph(fp);
  status=save_diagram(fp,NODE);
  if(status!=1)return status;
  save_q_file(fp);
  return 1;
}
 
void save_auto_numerics(FILE *fp)
{
  int i;
  std::string line=xpp::format("{} ",NAutoPar);
  for(i=0;i<NAutoPar;i++)
    line+=xpp::format("{} ",AutoPar[i]);
  line+=xpp::format("{}\n",NAutoUzr);
  for(i=0;i<9;i++)
    line+=xpp::format("{:g} {}\n",outperiod[i],UzrPar[i]);
  line+=xpp::format("{} {} {} \n",Auto.ntst,Auto.nmx,Auto.npr);
  line+=xpp::format("{:g} {:g} {:g} \n",Auto.ds,Auto.dsmin,Auto.dsmax);
  line+=xpp::format("{:g} {:g} {:g} {:g}\n",Auto.rl0,Auto.rl1,Auto.a0,Auto.a1);
  line+=xpp::format("{} {} {} {} {} {} {}\n",aauto.iad,aauto.mxbf,aauto.iid,aauto.itmx,aauto.itnw,aauto.nwtn,aauto.iads);
  xpp::print(fp,"{}",line);
}

void load_auto_numerics(FILE *fp)
{
 int i,in;
 /* The fscanf formats this replaces ended in whitespace, which fscanf
    skipped; the token reader leaves it in the stream, and every read that
    follows (load_auto_graph, load_diagram) skips it first. */
 xpp::TokenReader tr=xpp::TokenReader::attach(fp);
 if (!tr.read(NAutoPar)) return;
 for(i=0;i<NAutoPar;i++){
   if (!tr.read(AutoPar[i])) return;
   in=get_param_index(xpp::model().upar_names[AutoPar[i]]);
   Auto_index_to_array[i]=in;
 }
 if (!tr.read(NAutoUzr)) return;
  for(i=0;i<9;i++){
    Auto.nper=NAutoUzr;
    if (!tr.read(outperiod[i]) || !tr.read(UzrPar[i])) return;
    Auto.period[i]=outperiod[i];
    Auto.uzrpar[i]=UzrPar[i];
  }

 if (!tr.read(Auto.ntst) || !tr.read(Auto.nmx) || !tr.read(Auto.npr)) return;
 if (!tr.read(Auto.ds) || !tr.read(Auto.dsmin) || !tr.read(Auto.dsmax)) return;
 if (!tr.read(Auto.rl0) || !tr.read(Auto.rl1) || !tr.read(Auto.a0) || !tr.read(Auto.a1)) return;
 if (!tr.read(aauto.iad) || !tr.read(aauto.mxbf) || !tr.read(aauto.iid) || !tr.read(aauto.itmx)
     || !tr.read(aauto.itnw) || !tr.read(aauto.nwtn) || !tr.read(aauto.iads)) return;
}

void save_auto_graph(FILE *fp)
{
  xpp::print(fp,"{:g} {:g} {:g} {:g} {} {} \n",Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax,
	Auto.var,Auto.plot);
}

void load_auto_graph(FILE *fp)
{
  xpp::TokenReader tr=xpp::TokenReader::attach(fp);
  if (!tr.read(Auto.xmin) || !tr.read(Auto.ymin) || !tr.read(Auto.xmax) || !tr.read(Auto.ymax)
      || !tr.read(Auto.var) || !tr.read(Auto.plot)) return;
}
  
void save_q_file(FILE *fp) /* I am keeping the name q_file even though they are s_files */
{
  std::string string=this_auto_file+".s";
  xpp::LineReader lr(string.c_str());
  if(!lr){
    auto_err("Couldnt open s-file");
    return;
  }
  while(auto line=lr.next())
    xpp::print(fp,"{}\n",*line);
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

void make_q_file(FILE *fp)
{
  std::string string=this_auto_file+".s";
  /* written beside the .s and renamed over it once whole */
  xpp::Writer w(string.c_str());
  if(!w){
    auto_err("Couldnt open s-file");
    return;
  }

  /* the rest of fp, the .auto's copy of the .s, without its blank lines */
  xpp::LineReader lr=xpp::LineReader::attach(fp);
  while(auto line=lr.next()){
    if(!noinfo(*line))
      w.print("{}\n",*line);
  }
  w.commit();
}

void load_auto()
{

  int ok;

  int status;
  if(diagram_count()>1){
    ok=reset_auto();
    if(ok==0)return;
  }

  std::string filename=xpp::format("{}.auto",basename(this_auto_file.data()));
  status=file_selector("Load Auto",filename,"*.auto");
  if(status==0)return;
  xpp::UniqueFile fp=xpp::open_read(filename.c_str());
  if(!fp){
    auto_err("Cannot open file");
    return;
  }
  
  load_auto_file(fp.get());
}

/* load_auto without its reset and dialog (xpp_session.c): 1 loaded,
   -1 an empty diagram */
int load_auto_file(FILE *fp)
{
  int status;
  load_auto_numerics(fp);
  load_auto_graph(fp);
  auto_data_forget(); /* the strip described the diagram this one replaces */
  status=load_diagram(fp,NODE);
  if(status!=1)return status;
  make_q_file(fp);
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

/* W26 (issue #42): a File entry beside afile_hint's 15 (menus.c), not
   inside it -- afile_hint's own 15 hints plus one more, built lazily (well
   after menus.c's static data is initialised) so afile_hint stays the one
   copy of its own text and stays reached. */
static const char **afile_hint_csv()
{
  static const char *h[16];
  static int done=0;
  if(!done){
    int i;
    for(i=0;i<15;i++)h[i]=afile_hint[i];
    h[15]="Write the diagram, and its eigenvalues/multipliers, as CSV";
    done=1;
  }
  return h;
}

/* the diagram and its eigenvalues/multipliers as CSV (csv_export.h),
   pandas.read_csv/MATLAB readtable read with no options; one file dialog
   answer names both files (csv_export_diagram_pair derives the second) */
void export_auto_csv()
{
  std::string filename="diagram.csv";
  if(!file_selector("Export CSV",filename,"*.csv"))return;
  if(!csv_export_diagram_pair(filename.c_str()))
    err_msg("Nothing to export: run or load a diagram first");
}

void auto_file()
{

  static const char *m[]={"Import orbit","Save diagram","Load diagram","Postscript","SVG",
		    "Reset diagram","Clear grab","Write pts","All info","init Data","Toggle redraw","auto raNge","sElect 2par pt","draw laBled","lOad branch","eXport CSV"};
  static const char *const key="islpvrcwadtnebox";
  char ch;
  ch=static_cast<char>(auto_pop_up_list("File",m,key,16,16,0,10,10,afile_hint_csv(),
		       Auto.hinttxt.c_str()));
  if(ch=='i'){
    load_auto_orbit();
    return;
  }
  if(ch=='s'){
    save_auto();
    return;
  }
  if(ch=='l'){
    load_auto();
    redraw_diagram(); /* the loaded diagram, at once */
    return;
  }
  if(ch=='r'){
    reset_auto();
    redraw_diagram(); /* now empty */
  }
  if(ch=='c'){
    grabpt.flag=0;
  }
  if(ch=='p'){
    NoBreakLine=1;
    post_auto();
    NoBreakLine=0;
  }
  if(ch=='v'){
    NoBreakLine=1;
    svg_auto();
    NoBreakLine=0;
  }
  if(ch=='w'){
    write_pts();
  }
  if(ch=='a'){
    write_info_out();
  }
  if(ch=='d'){
    write_init_data_file();
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
      load_browser_with_branch(diagram_mark.start_branch,diagram_mark.start_point,diagram_mark.end_point);
	}
  if(ch=='x'){
    export_auto_csv();
  }
  if(ch=='n'){
    if(diagram_mark.state<2) 
      err_msg("Mark a branch first using S and E");
    else
      do_auto_range();
  }
  if(ch=='e'){
    if(Auto.plot!=P_P){
      err_msg("Must be in 2 parameter plot");
      return;
    }
    setautopoint();

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

void auto_get_info(int *n, std::string &pname)
{
  int i1,i2,ibr;
  DIAGRAM *d,*dnew;

  if(diagram_mark.state==2){
    i1=abs(diagram_mark.start_point);
    ibr=diagram_mark.start_branch;
    i2=abs(diagram_mark.end_point);
    *n=abs(i2-i1);
    d=diagram_first();
    while(1){
      if(d->ibr==ibr && ((d->ntot==i1)||(d->ntot==(-i1))))
	{
	  pname=xpp::model().upar_names[AutoPar[d->icp1]];
	  break;
	}
       dnew=diagram_next(d);
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
    }
  }
  
}

void auto_set_mark(int i)
{
  int pt,ibr;
  if(diagram_mark.state==2){
    ibr=diagram_mark.start_branch;
    if(abs(diagram_mark.start_point)<abs(diagram_mark.end_point))
      pt=abs(diagram_mark.start_point)+i;
    else
      pt=abs(diagram_mark.end_point)+i;
    find_point(ibr,pt);
  }
}

void find_point(int ibr, int pt)
{
  int i;
  DIAGRAM *d,*dnew;
   if(diagram_count()<2)return;
   d=diagram_first();
   while(1)
     {
       if(d->ibr==ibr && ((d->ntot==pt)||(d->ntot==(-pt))))
	 {  /* need to look at both signs to ignore stability */
	   /* now we use this info to set parameters and init data */
	   for(i=0;i<NODE;i++)
	     set_ivar(i+1,d->u0[i]);
	   get_ic(0,d->u0);
	   auto_set_pars_from(d->par);
	   evaluate_derived();
	   redo_all_fun_tables();
	   redraw_params();
	   redraw_ics();
           if((d->per)>0)
	     set_total(d->per);		       
	   break;
	 }
       dnew=diagram_next(d);
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
     }
}

void do_auto_range()
{
  double t=TEND;
  
  if(diagram_mark.state==2)
    do_auto_range_go();
  TEND=t;
}

void DLINE(double a,double b,double c,double d)
{
  ALINE(IXVal(a),IYVal(b),IXVal(c),IYVal(d));
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

const char *query_special(const char *title)
{
        static const char *m[]={"BP","EP","HB","LP","MX","PD","TR","UZ"};
	static const char *const key="behlmptu";
	int ch=static_cast<char>(auto_pop_up_list(title,m,key,8,11,1,10,10,
			     aspecial_hint,Auto.hinttxt.c_str()));
	redraw_auto_menus();
	const char *k=ch!=0?strchr(key,ch):NULL;
	return k!=NULL?m[k-key]:NULL;
}

void traverse_diagram()
{
  DIAGRAM *d,*dnew,*dold;
  int done=0;
  int ix,iy,i; 
  int lalo;
  int kp;
  int xm,ym;
  diagram_mark.state=0;
  if(diagram_count()<2)return;
  
  d=diagram_first(); 
  DONT_XORCross=0;
  traverse_out(d,&ix,&iy,1);
  
  while(done==0){
    kp=xpp_ui.auto_grab_event(&xm,&ym);
    if(kp==XPP_AUTO_NODE)
    {
      /* a point of the diagram by its entry: the cursor goes there */
      dnew=diagram_point(xm);
      if(dnew!=NULL){
        clear_msg();
        XORCross(ix,iy);
        d=dnew;
        CUR_DIAGRAM=d;
        traverse_out(d,&ix,&iy,1);
      }
    }
    else if(kp==XPP_AUTO_CLICK)
    {
	{
       		clear_msg();
	        /*
		GO HOME
		*/
		XORCross(ix,iy);
		DONT_XORCross = 1;
		d=diagram_first();
		CUR_DIAGRAM=d;
		traverse_out(d,&ix,&iy,0);
                /*
		END GO HOME
		*/
       
       		/*
		GO END
		*/
		int mindex=0;
		double dist;
		double ndist = Auto.wid*Auto.hgt;
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
			dnew=diagram_next(d);
			if(dnew==NULL){dnew=d;break;}
			d=dnew;
			traverse_out(d,&ix,&iy,0);/*Need this each time to update the distance calc*/
	        }
		d=dnew;
       		CUR_DIAGRAM=d;
		load_all_labeled_orbits=lalo;
       		traverse_out(d,&ix,&iy,0);
		/*
		END GO END
		*/

		/*
		GO HOME
		*/
		XORCross(ix,iy);
		while (1){
		        if (d->index == mindex){dnew=d;break;}
        		dnew=diagram_prev(d);
        		if(dnew==NULL){dnew=d;break;}
        		d=dnew;
		}
		d=dnew;
		CUR_DIAGRAM=d;
		DONT_XORCross = 0;
		traverse_out(d,&ix,&iy,1);
                /*
		END GO HOME
		*/
		
	}
    }
    else {
        clear_msg();
	const char *nsymb;
        
	int found=0;

      switch(kp){
      case RIGHT:
	dnew=diagram_next(d);
	if(dnew==NULL)dnew=diagram_first();
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(d,&ix,&iy,1);
	break;
	
      case LEFT:
	dnew=diagram_prev(d);
	if(dnew==NULL)dnew=diagram_first();
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(d,&ix,&iy,1);
	break;
      case UP:
       if ((nsymb=query_special("Next..."))==NULL){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=diagram_next(d);
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
         Auto.hinttxt=xpp::format("  Higher {} not found",nsymb);
	 xpp_ui.auto_show_hint();
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
      case DOWN:
       if ((nsymb=query_special("Previous..."))==NULL){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=diagram_prev(d);
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
         Auto.hinttxt=xpp::format("  Lower {} not found",nsymb);
	 xpp_ui.auto_show_hint();
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break; 
      case TAB:
       XORCross(ix,iy);
       while(1){
         dnew=diagram_next(d);
         if(dnew==NULL){dnew=diagram_first();break;} /*TAB wraps*/
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
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
       d=last_diagram();
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case HOME:/*All the way to beginning*/
       XORCross(ix,iy);
       d=diagram_first();
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case PGUP: /*Same as TAB except we don't wrap*/
       XORCross(ix,iy);
       while(1){
         dnew=diagram_next(d);
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case PGDN: /*REVERSE TAB*/
       XORCross(ix,iy);
       while(1){
         dnew=diagram_prev(d);
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
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
  if(done==1){
    grabpt.ibr=d->ibr;
    grabpt.lab=d->lab;
    for(i=0;i<8;i++)
    grabpt.par[i]=d->par[i];
    grabpt.per=d->per;
    grabpt.torper=d->torper;
    for(i=0;i<NODE;i++){
      grabpt.uhi[i]=d->uhi[i];
      grabpt.ulo[i]=d->ulo[i];
      grabpt.u0[i]=d->u0[i];
      grabpt.ubar[i]=d->ubar[i];
      set_ivar(i+1,grabpt.u0[i]);
    }
    get_ic(0,grabpt.u0);
    grabpt.flag=1;
    grabpt.itp=d->itp;
    grabpt.nfpar=d->nfpar;
    auto_set_pars_from(grabpt.par);
  }
  evaluate_derived();
  redo_all_fun_tables();
  redraw_params();
  redraw_ics();
}

void MarkAuto(int x, int y)
{

  LineWidth(2);
  ALINE(x-8,y-8,x+8,y+8);
  ALINE(x+8,y-8,x-8,y+8);
  LineWidth(1);

}

void clear_msg()
{
  Auto.hinttxt.clear();
  xpp_ui.auto_show_hint();
}

void auto_update_view(float xlo,float xhi, float ylo, float yhi)
{
              Auto.xmin=xlo;
	      Auto.ymin=ylo;
	      Auto.xmax=xhi;
	      Auto.ymax=yhi;
	      redraw_diagram();

}

/* the pointer moved to pixel (i,j) of the diagram: show its coordinates */
void auto_motion_xy(int i,int j)
{
  double x,y;
    x=Auto.xmin+static_cast<double>((i-Auto.x0))*(Auto.xmax-Auto.xmin)/static_cast<double>(Auto.wid);
    y=Auto.ymin+static_cast<double>((Auto.y0-j+Auto.hgt))*(Auto.ymax-Auto.ymin)/static_cast<double>(Auto.hgt);
    auto_point_xy(x,y);
}

void auto_point_xy(double x,double y)
{
    Auto.hinttxt=xpp::format("x={:g},y={:g}",x,y);
    storeautopoint(x,y);
    xpp_ui.auto_show_hint();
}
