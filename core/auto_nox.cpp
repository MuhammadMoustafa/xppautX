#include "auto_unknowns.h"
#include "model.h"
#include "session.h"
#include "many_pops.h"
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
#include "typed_values.h"
#include <string.h>
#include "autevd.h"
#include "auto_stop.h"
#include "jacobian.h"
#include "auto_stability.h"
#include "csv_export.h"
#include "image_format.h"
#include <libgen.h>
#include "graf_par.h"


#include "pp_shoot.h"

#include "xpp_files.h"
#include "xpp_zip.h"
#include "xpp_session_auto.h"
#include "auto_settings.h"
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
#include "jacobian.h"
#include "graphics.h"

#include "xpp_job.h"
#include "auto_data.h"
#include "derived.h"   /* evaluate_derived() */
#include "tabular.h"   /* redo_all_fun_tables() */
#include "getvar.h"    /* setvar() */
#include "my_rhs.h"    /* extra() */
#include "menus.h"
#include "picture.h"
#include "solver.h"

#define PARAM_BOX 1

#define RUBBOX 0


#define OPEN_3 1
#define NO_OPEN_3 0
#define OVERWRITE 0
#define APPEND 1

#include "mykeydef.h"
namespace xpp {



/*  two parameter curves' colours by their kind (XPPAUT's)  */

constexpr xpp::Color LPP_color=xpp::xppaut_color(0);  /* the foreground */
constexpr xpp::Color LPE_color=xpp::xppaut_color(1);  /* red */
constexpr xpp::Color HB_color=xpp::xppaut_color(9);   /* blue */
constexpr xpp::Color TR_color=xpp::xppaut_color(7);   /* green */
constexpr xpp::Color PD_color=xpp::xppaut_color(4);   /* orange */
constexpr xpp::Color BR_color=xpp::xppaut_color(8);   /* turquoise */
constexpr xpp::Color FP_color=xpp::xppaut_color(6);   /* olive */

/* AUTO's continuation parameters back into the model (find_point, the
   Grab loop's Return): a diverged run's stored point can hold a
   non-finite value (QA SCI-001, -nan in the "state" event's pars). Skip
   it and keep the model's previous value instead, once per parameter. */
static void auto_set_pars_from(xpp::Session &s, const double *par)
{
    for (int i = 0; i < s.model().nupar; i++) {
        const int idx = s.model().upar_con[i];
        if (std::isfinite(par[i])) s.parser.constants[idx] = par[i];
        else
            xpp::log(XPP_LOG_WARN, "AUTO: {} from the diagram is not finite; keeping {:.16g}",
                     s.model().upar_names[i], s.parser.constants[idx]);
    }
}

/* do_auto's cleanup after go_go_auto(): a run that diverges can leave
   AUTO's own working copy of a continuation parameter (autpp.cpp's func,
   synced into constants[] every RHS call) not finite. Restore what the
   run started from instead of leaving -nan in the model (QA SCI-001). */
static void auto_restore_finite_pars(xpp::Session &s, const double *before)
{
    for (int i = 0; i < s.model().nupar; i++) {
        const int idx = s.model().upar_con[i];
        if (!std::isfinite(s.parser.constants[idx])) {
            xpp::log(XPP_LOG_WARN, "AUTO: the run left {} not finite; keeping {:.16g}",
                     s.model().upar_names[i], before[i]);
            s.parser.constants[idx] = before[i];
        }
    }
}





/* the colour of the two-parameter curve of kind flag2 */
static xpp::Color two_parameter_color(AutoCalculation flag2)
{
  switch(flag2){
  case LPE2: return LPE_color;
  case LPP2: return LPP_color;
  case auto_neutral_saddle_curve:
  case auto_hopf_curve: return HB_color;
  case TR2: return TR_color;
  case BR2: return BR_color;
  case PD2: return PD_color;
  case FP2: return FP_color;
  default: return xpp::foreground_color;
  }
}

void storeautopoint(xpp::Session &s, double x,double y)
{
  if(s.auto_state.axes().plot==AutoProjection::two_parameter){
    s.auto_state.from_auto_x=x;
    s.auto_state.from_auto_y=y;
    s.auto_state.from_auto_flag=true;
  }
}
void setautopoint(xpp::Session &s)
{
  if(s.auto_state.from_auto_flag)
    {
      s.auto_state.from_auto_flag=false;
      xpp::set_val(s,s.model().upar_names[s.auto_state.axes().icp1],s.auto_state.from_auto_x);
      xpp::set_val(s,s.model().upar_names[s.auto_state.axes().icp2],s.auto_state.from_auto_y);
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
  case AutoProjection::maximum:
  case AutoProjection::maximum_and_minimum:
    l.y=s.model().uvar_names[ax.var];
    break;
  case AutoProjection::norm:
    l.y="Norm";
    break;
  case AutoProjection::period:
    l.y="Period";
    break;
  case AutoProjection::frequency:
    l.y="Frequency";
    break;
  case AutoProjection::two_parameter:
    l.y=par_label(s, ax.icp2);
    break;
  case AutoProjection::average:
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
 /* the data of the diagram starts again, in every view */
 for(int v=0;v<static_cast<int>(s.auto_state.views.size());v++)auto_diagram(s,v,NULL);
 refreshdisplay(s);
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
    xpp::files::move(s.auto_state.fort7.c_str(),(s.auto_state.file+".b").c_str());
    xpp::files::move(s.auto_state.fort9.c_str(),(s.auto_state.file+".d").c_str());
    xpp::files::move(s.auto_state.fort8.c_str(),(s.auto_state.file+".s").c_str());
  }
  else {/*APPEND*/
    xpp::files::prepend(s.auto_state.fort7.c_str(),(s.auto_state.file+".b").c_str());
    xpp::files::prepend(s.auto_state.fort9.c_str(),(s.auto_state.file+".d").c_str());
    xpp::files::prepend(s.auto_state.fort8.c_str(),(s.auto_state.file+".s").c_str());
  }

    xpp::files::remove(s.auto_state.fort8.c_str());

    xpp::files::remove(s.auto_state.fort7.c_str());
    xpp::files::remove(s.auto_state.fort9.c_str());
    xpp::files::remove(s.auto_state.fort3.c_str());

}

/* AUTO writes fort.3/7/8/9 under HOME. A HOME that is set but unusable
   (missing, not writable) must fall back to the model's directory like an
   unset one, or the opens fail deep inside autlib1.cpp. */
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

  const char* auto_dir = auto_home_dir(s, dname);

  s.auto_state.file=xpp::format("{}/{}",auto_dir,bname);
}

void open_auto(xpp::Session &s, int flg) /* compatible with new auto */
{
  std::string basec = s.model().this_file, dirc = s.model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* auto_dir = auto_home_dir(s, dname);

  s.auto_state.file=xpp::format("{}/{}",auto_dir,bname);
  s.auto_state.fort3=xpp::format("{}/fort.3",auto_dir);
  s.auto_state.fort7=xpp::format("{}/fort.7",auto_dir);
  s.auto_state.fort8=xpp::format("{}/fort.8",auto_dir);
  s.auto_state.fort9=xpp::format("{}/fort.9",auto_dir);

  if(flg==1){
    xpp::files::copy((s.auto_state.file+".s").c_str(),s.auto_state.fort3.c_str());
  }

}

const char *auto_fort_path(const xpp::Session &s, int unit)
{
  switch(unit){
  case 3: return s.auto_state.fort3.c_str();
  case 7: return s.auto_state.fort7.c_str();
  case 8: return s.auto_state.fort8.c_str();
  case 9: return s.auto_state.fort9.c_str();
  default: return "";
  }
}

/* what a run continues, for auto_stability.h */
static AutoStabilityKind run_stability_kind(xpp::Session &s)
{
  if(s.auto_state.two_param!=no_calculation||s.auto_state.bifur.isw==2)return AUTO_STABILITY_OTHER;
  if(s.auto_state.bifur.ips==xpp::AutoProblem::Periodic)return AUTO_STABILITY_PERIODIC;
  if(xpp::problem_is_steady_or_map(s.auto_state.bifur.ips))return AUTO_STABILITY_STEADY;
  return AUTO_STABILITY_OTHER;
}

/* what a stored point is, for auto_stability.h: one-parameter periodic
   orbits have a negative branch (autlib1.cpp stplbv) */
static AutoStabilityKind point_stability_kind(const DIAGRAM *d)
{
  if(d->flag2!=no_calculation)return AUTO_STABILITY_OTHER;
  return d->ibr<0?AUTO_STABILITY_PERIODIC:AUTO_STABILITY_STEADY;
}

/* a run starts from Auto.irs's label, or from initial data: its first
   point takes the label's stored stability when it is the label's own
   solution, else it is not computed (auto_stability.h) */
static void stability_run_start(xpp::Session &s)
{
  const DIAGRAM *d=s.auto_state.bifur.irs>0?diagram_of_label(s,s.auto_state.bifur.irs):NULL;
  if(d==NULL){
    auto_stability_run_start(s.auto_state.stability,run_stability_kind(s),s.auto_state.bifur.isw,AUTO_STABILITY_NONE,AutoPointCode{},{},{});
    return;
  }
  auto_stability_run_start(s.auto_state.stability,run_stability_kind(s),s.auto_state.bifur.isw,point_stability_kind(d),d->itp,d->evr,d->evi);
}

/* MAIN Running routine  Assumes that Auto structure is set up */
namespace {
/* one do_auto call's depth, however it ends */
struct AutoDepth {
  int &depth;
  explicit AutoDepth(xpp::Session &s) : depth(s.auto_state.depth) { if(depth++==0)auto_stop_clear(s.auto_state.stop); } /* xppautX: T23: why this run's branches end */
  ~AutoDepth() { depth--; }
  AutoDepth(const AutoDepth &)=delete;
  AutoDepth &operator=(const AutoDepth &)=delete;
};
}

void do_auto(xpp::Session &s, int iold, int isave, xpp::AutoPointCode itp)
{
    // A model parameter must exist before run setup selects its solver index.
    if(s.model().nupar==0){
      command_error("auto", "AUTO continuation needs a model parameter");
      return;
    }
      /* Retaining an older branch retains its label solutions too. A fresh
         calculation may request overwrite only after the diagram is reset. */
      if(s.auto_state.diag_flag && isave==OVERWRITE)isave=APPEND;
      if(const DIAGRAM *point=diagram_of_label(s,s.auto_state.bifur.irs))
        auto_set_pars_from(s,diagram_parameters(*point).data());
      
    set_auto(s); /* this sets up all the continuation initialization 
                   it is equivalent to reading in auto parameters
                   and running init in auto 
		*/
 
    open_auto(s, iold); /* this copies the relevant files .s  to fort.3 */
    const AutoDepth depth(s);
    s.auto_lib.jacobian_failure.reset();
    std::optional<xpp::Error> failed; /* the run reports its failure after cleanup */
    {
      const xpp::Job job; /* Abort cancels it (xpp_job.h) */
      s.auto_state.run_from=s.auto_state.bifur.irs>0?s.auto_state.bifur.irs:0; /* the diagram's data say where the run started */
      stability_run_start(s); /* what its first point's stability is (auto_stability.h) */
      {
          xpp::Computation computing; /* what Abort stops (xpp_job.h) */
          std::vector<double> before(s.model().nupar);
          for (int i = 0; i < s.model().nupar; i++) before[i] = s.parser.constants[s.model().upar_con[i]];
          try {
              go_go_auto(s); /* this complets the initialization and calls the
                                main routines
                             */
          } catch (const xpp::AutoFailed &e) {
              /* W63a: what the numerics used to exit() on ends the run
                 instead, as a cancel does (auto_state.h) */
              failed = e.error;
          } catch (const std::length_error &e) {
              failed = xpp::Error{"AUTO",e.what(),xpp::command_place()};
          } catch (const std::bad_alloc &) {
              failed = xpp::Error{"AUTO","Cannot allocate AUTO workspace",xpp::command_place()};
          }
          auto_restore_finite_pars(s, before.data()); /* leave no NaN parameter behind (QA SCI-001) */
      }
      s.auto_state.run_from=0;
      if(xpp::job::cancelled()||failed.has_value())s.auto_state.restart_label=0; /* xppautX: cancel: no follow-up run */
      if(failed.has_value())s.auto_lib.restart_flag=false;
    }
    /*     run_aut(Auto.nfpar,itp); THIS WILL CHANGE TO gogoauto stuff */ 
    close_auto(s, isave); /* this copies fort.8 to the .s file and other 
                          irrelevant stuff 
		       */
    if(failed.has_value()){
      failed->what="AUTO stopped: "+failed->what;
      xpp::log_auto("{}\n",failed->text()); /* the AUTO window's Output */
      if(failed->place.file.empty() && failed->place.line==0)
        command_error("auto", failed->what);
      else xpp::show_error(*failed);
    }
    if(s.auto_lib.jacobian_failure){
      xpp::Error e=std::move(*s.auto_lib.jacobian_failure);
      s.auto_lib.jacobian_failure.reset();
      e.what="AUTO stopped: "+e.what;
      /* the run stored the last converged point as an end point (the
         cancel it made itself: autpp.cpp): the continuation goes on from
         its label, with the settings of this run */
      const DIAGRAM *last=s.auto_state.stop.last_why==xpp::AUTO_STOP_JACOBIAN?last_diagram(s.diagram):NULL;
      if(last!=NULL&&last->lab>0)
        e.action=xpp::switch_to_differences(xpp::format(R"({{"cmd":"auto","op":"restart","label":{}}})",last->lab));
      xpp::log_auto("{}\n",e.what);
      xpp::show_error(e);
    }
    
    if(s.auto_state.restart_label!=0){
      xpp::log_auto("RestartLabel={:d} itp={:d} ips={:d} nfpar={:d} ilp={:d} isw={:d} isp={:d} A2p={} \n",s.auto_state.restart_label,xpp::auto_point_encode(s.auto_state.bifur.itp), xpp::auto_problem_code(s.auto_state.bifur.ips),s.auto_state.bifur.nfpar,s.auto_state.bifur.ilp,s.auto_state.bifur.isw,s.auto_state.bifur.isp,auto_calculation_name(s.auto_state.two_param));
      s.auto_state.bifur.irs=s.auto_state.restart_label;
      s.auto_state.restart_label=0;
      do_auto(s, iold,isave, s.auto_state.bifur.itp);
      
    }
     ping();
      redraw_params();
}

void auto_restart(xpp::Session &s, int label)
{
  if(!auto_grab_label(s,label)){
    command_error("auto",xpp::format("Restart: no point labelled {}",label));
    return;
  }
  s.auto_state.bifur.irs=label;
  do_auto(s,OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_label_parameters(const xpp::Session &s, int label, double *parameters)
{
  const DIAGRAM *point=diagram_of_label(s,label);
  if(!point)return; /* a library-generated starting solution may have no stored point */
  for(int k=0;k<s.auto_state.run.nicp;k++){
    const int slot=s.auto_state.run.icp[k];
    const int parameter=s.auto_state.parameter_layout.model_index(slot);
    if(parameter>=0 &&
       std::find(point->parameter_indices.begin(),point->parameter_indices.end(),parameter)==point->parameter_indices.end())
      parameters[slot]=diagram_parameter(*point,parameter);
  }
}

void set_auto(xpp::Session &s) /* Caution - need to include NICP here */
{
  init_auto(s.auto_state,s.model().node,s.numerics.jacobian==JACOBIAN_EXACT);
  
}
const char *auto_par_name(const xpp::Session &s, int k)
{
  return k>=0&&k<s.model().nupar?s.model().upar_names[k].c_str():NULL;
}

namespace {
/* AUTO's parameter index's name, T for the period; empty for no such
   parameter */
std::string par_or_period_name(const xpp::Session &s, long index, bool period)
{
  if(period)return "T";
  const int k=static_cast<int>(index);
  return k>=0&&k<s.model().nupar?par_label(s,k):std::string();
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
  const std::string t=xpp::short_name(s,auto_column_width-1);
  const int n=static_cast<int>(t.size());
  const int l=(auto_column_width-n)/2;
  return std::string(static_cast<size_t>(l),' ')+t+std::string(static_cast<size_t>(auto_column_width-n-l),' ');
}

std::string screen_col(const xpp::Session &s, const char *col)
{
  long p;
  const char *c=col;
  while(isspace(static_cast<unsigned char>(*c)))c++;
  if(strncmp(c,"PAR(",4)==0&&read_long(c+4,p)){
    const std::string name=par_or_period_name(s, s.auto_state.parameter_layout.model_index(static_cast<int>(p)), p==xpp::auto_unknown::period);
    if(!name.empty())return col_centre(name);
  }
  /* U(n), and the MAX(n) / MIN(n) a periodic branch prints, where AUTO has
     overwritten the U itself */
  const char *q=strchr(col,'(');
  if(q!=NULL&&strstr(col,"PAR")==NULL&&read_long(q+1,p)&&p>=1&&p<=s.model().node){
    size_t n=static_cast<size_t>(q-col);
    if(n>0&&col[n-1]=='U')n--; /* the name replaces the U */
    /* keep what stands in front of it: MAX, MIN, L2-NORM, INTEGRAL */
    std::string pre(col,std::min<size_t>(n,auto_column_width));
    const size_t a=pre.find_first_not_of(' '),b=pre.find_last_not_of(' ');
    pre=a==std::string::npos?std::string():pre.substr(a,b-a+1);
    return col_centre(pre+(pre.empty()?"":" ")+s.model().uvar_names[p-1]);
  }
  return std::string(std::string_view(col).substr(0,auto_column_width));
}
} // namespace

std::string auto_screen_col(const xpp::Session &s, const std::string &col)
{
  return screen_col(s, col.c_str());
}

void auto_per_par(xpp::Session &s)
{
  std::string count=xpp::number(s.auto_state.bifur.marks.size());
  if(!new_string_of("Mark values: how many?",count,XPP_FIELD_INTEGER))return;
  xpp::TypedFields fields(s);
  const int n=fields.integer("Mark count",count);
  if(n<0 || n>xpp::auto_file_marks_limit)fields.fail("Mark count",xpp::format("must be from 0 to {}",xpp::auto_file_marks_limit));
  if(fields.error()) { command_error("auto",*fields.error()); return; }
  xpp::AutoSettingsSet set;
  set.nmarks=n;
  set.mark_name.resize(n,"T");
  set.mark_value.resize(n);
  std::vector<std::string> labels(n),values(n);
  std::vector<const char *> names(n);
  for(int i=0;i<n;i++){
    labels[i]=xpp::format("Uzr{}",i+1);
    names[i]=labels[i].c_str();
    const auto &marks=s.auto_state.bifur.marks;
    values[i]=i<static_cast<int>(marks.size()) ?
      xpp::format("{}={}",par_or_period_name(s,marks[i].parameter,marks[i].period),xpp::number(marks[i].value)) : xpp::format("T={}",xpp::number(xpp::auto_initial_mark_period+xpp::auto_mark_period_spacing*i));
  }
  if(n && !do_string_box(5,2,"Mark values (UZ): parameter=value or T=value",names.data(),values))return;
  for(int i=0;i<n;i++){
    const auto equal=values[i].find('=');
    if(equal==std::string::npos){ fields.fail(labels[i],"expected parameter=value or T=value"); break; }
    set.mark_name[i]=xpp::trim_blanks(std::string_view(values[i]).substr(0,equal));
    // The historical form also accepts per for the period.
    if(strcasecmp(set.mark_name[i].c_str(),"per")==0)set.mark_name[i]="T";
    set.mark_value[i]=fields.number(labels[i],values[i].substr(equal+1));
  }
  std::string why;
  if(fields.error())command_error("auto",*fields.error());
  else if(xpp::auto_settings_apply(s,set,why)!=0)command_error("auto",why);
}

void auto_num_par(xpp::Session &s)
{
  /* Mesh and step size, ranges and tolerances, integer knobs, then named
     special points. Four columns; the row count follows the field count. */
  std::array<const char *,xpp::AUTO_NUM_N+xpp::auto_codim_kinds.size()> n;
  for(int i=0;i<xpp::AUTO_NUM_N;++i) n[i]=xpp::auto_settings_num_label(i);
  for(std::size_t i=0;i<xpp::auto_codim_kinds.size();++i) n[xpp::AUTO_NUM_N+i]=xpp::auto_codim_keys[i];
  int status;
  std::array<std::string, xpp::AUTO_NUM_N+xpp::auto_codim_kinds.size()> values;
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
  values[21] = s.auto_state.suppress_bp ? "true" : "false";

  for (std::size_t i=0;i<xpp::auto_codim_kinds.size();++i)
    values[xpp::AUTO_NUM_N+i]=s.auto_state.special_points[i] ? "true" : "false";
  static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_BOOLEAN,
                            XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,
                            XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,
                            XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT};
  status=do_string_box_of((n.size()+3)/4,4,"AutoNum",n.data(),values,kinds);
  if(status!=0){
    auto enabled=s.auto_state.special_points;
    for (std::size_t i=0;i<xpp::auto_codim_kinds.size();++i) {
      if (!xpp::parse_bool(values[xpp::AUTO_NUM_N+i],enabled[i])) {
        command_error("auto",xpp::format("{} must be true or false",xpp::auto_codim_keys[i])); return;
      }
    }
    xpp::AutoSettingsSet points;
    xpp::TypedFields fields(s);
    for (int i=0; i<xpp::AUTO_NUM_N; ++i) {
      points.has_num[i]=1;
      points.num[i]=i==xpp::AUTO_NUM_SUPPBP ? fields.boolean(n[i],values[i]) : fields.number(n[i],values[i]);
    }
    if(fields.error()) { command_error("auto",*fields.error()); return; }
    for(std::size_t i=0;i<enabled.size();++i) points.special_points[i]=enabled[i];
    std::string why;
    if(xpp::auto_settings_apply(s,points,why)!=0) command_error("auto",why);
  }

}    

void auto_plot_par(xpp::Session &s)
{

  const char *const key=menu_auto_plot_type.keys;
  char ch;

  static const char *const n[]={"*1Y-axis","*2Main Parm", "*2Secnd Parm", "Xmin", "Ymin",
		   "Xmax", "Ymax"};
  std::array<std::string, 7> values;
  int  status,i;
  int ii1,ii2,ji1,ji2;
  int i1=s.auto_state.axes().var+1;
  /* the menu's items are XPPAUT's Axes menu, its projections at XPPAUT's numbers */
  int shown=0;
  for(i=0;i<menu_auto_plot_type.n;i++)
    if(xpp::auto_projection_from_xppaut(i)==s.auto_state.axes().plot)shown=i;
  ch=static_cast<char>(menu_choose(&menu_auto_plot_type,shown));
  if(ch==ESC) 
    return;
  for(i=0;i<menu_auto_plot_type.n;i++)
    if(ch==key[i])
      if(const auto p=xpp::auto_projection_from_xppaut(i))s.auto_state.axes().plot=*p;
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
    load_last_plot(s, LastPlot::OneParameter);
    redraw_diagram(s);
    return;
  }

  if(ch==key[8]){
    load_last_plot(s, LastPlot::TwoParameter);
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
  values[1] = s.model().upar_names[s.auto_state.axes().icp1];
  values[2] = s.model().upar_names[s.auto_state.axes().icp2];
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
      for(i=0;i<s.model().nupar;i++){
	if(i1==i){
	  s.auto_state.axes().icp1=i;

	}
      }
    }
     i1=xpp::find_user_name(s.model(),PARAM_BOX,values[2].c_str());
    if(i1>=0){
      for(i=0;i<s.model().nupar;i++){
	if(i1==i){
	  s.auto_state.axes().icp2=i;
	}
      }
    }

    s.auto_state.axes().xmin=atof(values[3].c_str());
    s.auto_state.axes().ymin=atof(values[4].c_str());
    s.auto_state.axes().xmax=atof(values[5].c_str());
    s.auto_state.axes().ymax=atof(values[6].c_str());
    keep_axes_as_last(s);
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

void auto_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> ubar, double a)
{
 /* a plot type none of the cases know leaves the point at (par1, 0) */
 *x=par1;
 *y1=*y2=0.0;
 switch(ax->plot){
  case AutoProjection::maximum:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=*y1;
    break;
  case AutoProjection::norm:
    *x=par1;
    *y1=a;
    *y2=*y1;
    break;
  case AutoProjection::maximum_and_minimum:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=ulow[ax->var];
    break;
  case AutoProjection::average:
    *x=par1;
    *y1=ubar[ax->var];
    *y2=*y1;
    break;
  case AutoProjection::period:
    *x=par1;
    *y1=per;
    *y2=*y1;
    break;
  case AutoProjection::frequency:
    *x=par1;
    if(per>0)*y1=1./per;
    else *y1=0.0;
    *y2=*y1;
    break;
  case AutoProjection::two_parameter:
    *x=par1;
    *y1=par2;
    *y2=*y1;
    break;
  }
}

void add_ps_point(xpp::Session &s, const DIAGRAM &d, DiagramPointType type, int flg)
{
  const auto parameters=diagram_parameters(d);
  const double *par=parameters.data();
  const double per=d.per,a=d.norm;
  const std::span<const double> uhigh=d.uhi,ulow=d.ulo,ubar=d.ubar;
  const xpp::AutoAxis icp1=d.icp1,icp2=d.icp2;
  const AutoCalculation flag2=d.flag2;
  double x,y1,y2,par1,par2=0;
  DiagramPointType type1=type;
  par1=par[icp1.index];
  if(icp2.is_parameter())par2=par[icp2.index];
  auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    s.auto_state.bifur.lastx=x;
    s.auto_state.bifur.lasty=y1;
  }
  if(flag2==no_calculation&&s.auto_state.axes().plot==AutoProjection::two_parameter)
    {
  
       return;
     }
  if(flag2!=no_calculation&&s.auto_state.axes().plot!=AutoProjection::two_parameter){
  
    return;
  }

  if((flag2!=no_calculation)&&(s.auto_state.axes().plot==AutoProjection::two_parameter))
   type1=STABLE_EQ;
  /* a stable equilibrium is drawn thick, an unstable one dashed, in the options' colours, a two-parameter curve in its kind's */
  const auto color_of=[&](const xpp::Color &option){return flag2!=no_calculation?two_parameter_color(flag2):option;};
  xpp::LineStyle line_style;
  line_style.width=2;
  xpp::LineStyle dots;
  dots.draw=xpp::LineDraw::points;
  dots.marker_size=3;
  switch(type1){
  case STABLE_EQ:
  case UNSTABLE_EQ:
    if(s.auto_state.axes().plot==AutoProjection::period||s.auto_state.axes().plot==AutoProjection::frequency)break;
    if(!icp1.is_parameter(s.auto_state.axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.auto_state.axes().icp2))break;
    if(type1==UNSTABLE_EQ){
      line_style.width=1;
      line_style.dash=xpp::LineDash::dashed;
    }
    xpp::set_pen(s,color_of(type1==STABLE_EQ?s.auto_state.stable_eq_color:s.auto_state.unstable_eq_color),line_style);
    line_abs(s,static_cast<float>(x),static_cast<float>(y1),static_cast<float>(s.auto_state.bifur.lastx),static_cast<float>(s.auto_state.bifur.lasty));
    break;
  case UNSTABLE_PERIODIC:
  case STABLE_PERIODIC:
    if(!icp1.is_parameter(s.auto_state.axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.auto_state.axes().icp2))break;
    /* a stable periodic orbit's extremes are filled circles, an unstable one's open */
    xpp::set_pen(s,type1==STABLE_PERIODIC?s.auto_state.stable_po_color:s.auto_state.unstable_po_color,dots);
    s.plot_file.pen.hollow=type1==UNSTABLE_PERIODIC;
    point_abs(s,static_cast<float>(x),static_cast<float>(y1));
    point_abs(s,static_cast<float>(x),static_cast<float>(y2));
    break;
  }

  s.auto_state.bifur.lastx=x;
  s.auto_state.bifur.lasty=y1;
}

int auto_run_from_take(xpp::Session &s)
{
  int f=s.auto_state.run_from;
  s.auto_state.run_from=0;
  return f;
}

/* the stability circle as data (auto_data.h): a stored point's values
   (auto_stability.h), whether AUTO is computing it or a grab is on it */
static void show_stab(xpp::Session &s, std::span<const double> evr,std::span<const double> evi,int periodic)
{
  auto_data_stab(s,evr,evi,periodic);
}

/* the colour a point is drawn in: its kind of two-parameter curve's, or its stability's */
static xpp::Color auto_point_color(xpp::Session &s, DiagramPointType type,AutoCalculation flag2)
{
  if(flag2!=no_calculation)return two_parameter_color(flag2);
  switch(type){
  case STABLE_EQ: return s.auto_state.stable_eq_color;
  case UNSTABLE_EQ: return s.auto_state.unstable_eq_color;
  case STABLE_PERIODIC: return s.auto_state.stable_po_color;
  case UNSTABLE_PERIODIC: return s.auto_state.unstable_po_color;
  }
  return xpp::foreground_color;
}

/* this bit of code is for writing points - it only saves what is
   in the current view

*/
int check_plot_type(const xpp::Session &s, AutoCalculation flag2, xpp::AutoAxis icp1, xpp::AutoAxis icp2)
{
  if(flag2==no_calculation && s.auto_state.axes().plot==AutoProjection::two_parameter)
    return 0;
  if(flag2!=no_calculation  && s.auto_state.axes().plot!=AutoProjection::two_parameter)
    return 0; 
  if(!icp1.is_parameter(s.auto_state.axes().icp1))
    return 0;
  if(flag2!=no_calculation && !icp2.is_parameter(s.auto_state.axes().icp2))
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
void view_point(xpp::Session &s, int v, const AUTOAX &ax, bool active, const DIAGRAM &d, DiagramPointType type,
                int flg)
{
  const int lab=d.lab;
  const AutoCalculation flag2=d.flag2;
  const xpp::AutoAxis icp1=d.icp1,icp2=d.icp2;
  double x,y1,y2,par1,par2=0;
  DiagramPointType type1=type;
  XppDiagPoint dp;
  par1=diagram_axis_value(d,icp1);
  if(icp2.is_parameter())par2=diagram_axis_value(d,icp2);
  auto_xy_plot(&ax,&x,&y1,&y2,par1,par2,d.per,d.uhi,d.ulo,d.ubar,d.norm); /* figure out who sits on axes */
  dp=XppDiagPoint{};
  dp.ibr=d.ibr;
  dp.pt=d.ntot;
  dp.itp=d.itp;
  dp.node=d.index;
  dp.from=d.from;
  dp.jacobian=d.jacobian;
  dp.type=type;
  dp.flag2=flag2;
  dp.newseg=(flg==0);
  dp.color=auto_point_color(s, type,flag2);
  dp.lw=(type==STABLE_EQ||flag2!=no_calculation)?2:1;
  dp.x=x;
  dp.y1=y1;
  dp.y2=y2;
  if(active&&flg==0){
    s.auto_state.bifur.lastx=x;
    s.auto_state.bifur.lasty=y1;
  }
  if((flag2==no_calculation&&ax.plot==AutoProjection::two_parameter)||(flag2!=no_calculation&&ax.plot!=AutoProjection::two_parameter)){
    dp.x=dp.y1=dp.y2=std::numeric_limits<double>::quiet_NaN();
    auto_diagram(s,v,&dp);
    return;
  }
  if(flag2!=no_calculation) /* a two-parameter point in a two-parameter view */
    type1=STABLE_EQ;
  switch(type1){
  case STABLE_EQ:
  case UNSTABLE_EQ:
    if(ax.plot==AutoProjection::period||ax.plot==AutoProjection::frequency)break;
    if(!icp1.is_parameter(ax.icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(ax.icp2))break;
    dp.draw=1;
    break;
  case UNSTABLE_PERIODIC:
  case STABLE_PERIODIC:
    if(!icp1.is_parameter(ax.icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(ax.icp2))break;
    dp.draw=type1==STABLE_PERIODIC?2:3;
    break;
  }
  if(lab!=0&&icp1.is_parameter(ax.icp1)&&(flag2==no_calculation||icp2.is_parameter(ax.icp2))){
    dp.lab=lab;
  }
  if(active){
    s.auto_state.bifur.lastx=x;
    s.auto_state.bifur.lasty=y1;
  }
  auto_diagram(s,v,&dp);
}

} // namespace

/* main plotting code: the point in every view */
void add_point(xpp::Session &s, const DIAGRAM &d, DiagramPointType type, int flg)
{
  for(int v=0;v<static_cast<int>(s.auto_state.views.size());v++)
    view_point(s, v,s.auto_state.views[static_cast<std::size_t>(v)].axes,v==s.auto_state.active_view,d,type,flg);
  show_stab(s, d.evr,d.evi,type==STABLE_PERIODIC||type==UNSTABLE_PERIODIC);
  refreshdisplay(s);
}

void traverse_out(xpp::Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw)
{
  double norm,per,*par,par1,par2=0;
  int pt,ibr,lab;
  AutoCalculation flag2;
  xpp::AutoAxis icp1,icp2;
  AutoPointCode itp;
  double x,y1,y2;
  const char *symb;
  if (d==NULL)
  {
	return;
  }
  norm=d->norm;
  auto parameters=diagram_parameters(*d);
  par=parameters.data();

  per=d->per;
  lab=d->lab;
  itp=d->itp;
  ibr=d->ibr;
  icp1=d->icp1;
  icp2=d->icp2;
  flag2=d->flag2;
  pt=d->ntot;

  symb=auto_point_symbol(itp);
 par1=par[icp1.index];
  if(icp2.is_parameter())par2=par[icp2.index];
    auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,per,d->uhi,d->ulo,d->ubar,norm);
  
    *ix=IXVal(s, x);
    *iy=IYVal(s, y1);
    if (dodraw==1)
    {
      AutoDataInfo ai;
      /* what the strip shows, as data */
      ai.ibr=ibr;
      ai.pt=pt;
      ai.itp=itp;
      ai.lab=lab;
      ai.type=get_bif_type(ibr,pt);
      ai.flag2=flag2;
      ai.node=d->index;
      ai.jacobian=d->jacobian;
      ai.sym=symb;
      ai.p1name=s.model().upar_names[icp1.index].c_str();
      ai.p1=par1;
      ai.p2name=icp2.is_parameter()?s.model().upar_names[icp2.index].c_str():NULL;
      ai.p2=par2;
      ai.norm=norm;
      ai.vname=s.model().uvar_names[s.auto_state.axes().var].c_str();
      ai.u=d->u0[s.auto_state.axes().var];
      ai.per=per;
      ai.x=x;
      ai.y=y1;
      ai.y2=y2;
      /* the point and its circle together: a flush between them sent the new
         circle with the old point when a step took over 0.1 s (autocheck
         under valgrind, W21) */
      auto_data_info(s,&ai);
      show_stab(s, d->evr,d->evi,ibr<0);
    }
    if(lab>0 && s.auto_state.load_all_labeled_orbits!=LabeledOrbits::Off)
      xpp::ok_or_show(load_auto_orbitx(s,ibr,1,lab,per));

}

void do_auto_win(xpp::Session &s)
{
  if(s.auto_state.bifur.exist==0){
    s.auto_state.bifur.exist=1;
    for(int i=0;i<static_cast<int>(s.auto_state.views.size());++i){
      auto &view=s.auto_state.views[i];
      const auto existing=find_window(s,view.name);
      if(existing && (existing->kind!=WindowKind::diagram || existing->index!=i))
        view.name=next_window_name(s,WindowKind::diagram);
    }
    s.selected_window=s.auto_state.views[s.auto_state.active_view].name;
    make_auto(s,s.selected_window,"AUTO");
    
  }

}

void load_last_plot(xpp::Session &s, LastPlot which)
{
  s.auto_state.axes()=which==LastPlot::OneParameter?s.auto_state.old1p:s.auto_state.old2p;
}
void keep_last_plot(xpp::Session &s, LastPlot which)
{
  if(which==LastPlot::OneParameter)s.auto_state.old1p=s.auto_state.axes();
  else{
    s.auto_state.old2p=s.auto_state.axes();
    s.auto_state.old2p.plot=AutoProjection::two_parameter;
  }
}
void keep_axes_as_last(xpp::Session &s)
{
  switch(s.auto_state.axes().plot){
  case AutoProjection::maximum:
  case AutoProjection::norm:
  case AutoProjection::maximum_and_minimum:
  case AutoProjection::period:
    keep_last_plot(s, LastPlot::OneParameter);
    break;
  case AutoProjection::two_parameter:
    keep_last_plot(s, LastPlot::TwoParameter);
    break;
  case AutoProjection::frequency:
  case AutoProjection::average:
    break; /* XPPAUT keeps neither */
  }
}

void auto_new_view(xpp::Session &s)
{
  AutoDiagramView v;
  v.axes=s.auto_state.axes();
  v.name=xpp::next_window_name(s,xpp::WindowKind::diagram);
  s.auto_state.views.push_back(v);
  s.auto_state.active_view=static_cast<int>(s.auto_state.views.size())-1;
  s.selected_window=s.auto_state.views.back().name;
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
  s.selected_window=s.auto_state.views[k].name;
  return 1;
}

void init_auto_win(xpp::Session &s)
{
  s.auto_state.bifur.marks.clear();
  s.auto_state.parameter_layout.model_parameters=s.model().nupar;
  s.auto_state.parameter_layout.block=std::max(xpp::auto_unknown::block_size,
      xpp::auto_unknown::homoclinic_start+xpp::auto_unknown::homoclinic_blocks*s.model().node);
  s.auto_lib.parameter_count=s.auto_state.parameter_layout.size();
  for (auto *v : {&s.auto_state.grabpt.uhi, &s.auto_state.grabpt.ulo, &s.auto_state.grabpt.u0,
                 &s.auto_state.grabpt.ubar, &s.auto_state.homo_l, &s.auto_state.homo_r}) v->resize(s.model().node);
  start_diagram(s, s.model().node);
  s.auto_state.grabpt.flag=0;  /*  no point in buffer  */
  s.auto_state.bifur.exist=0;
 s.auto_state.blrtn.torper=s.numerics.torus_period;
 create_auto_file_name(s);
 
/*  Control -- done automatically   */
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.nbc=s.model().node;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.homo_flag=HomoclinicRun::None;
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
  s.auto_state.axes().plot=AutoProjection::maximum_and_minimum;
  s.auto_state.axes().var=s.auto_state.options.var;

/* xpp parameters    */
  
  s.auto_state.axes().icp1=0;
  s.auto_state.axes().icp2=1;
   s.auto_state.bifur.icp3=1;
  s.auto_state.bifur.icp4=1;
  s.auto_state.bifur.icp5=1;
  keep_last_plot(s, LastPlot::OneParameter);
  keep_last_plot(s, LastPlot::TwoParameter);
  s.auto_state.advanced={};
  s.auto_state.run.nunstab=1;
  s.auto_state.run.nstab=s.model().node-1;
}

/* Clear selections and obsolete run files when a diagram is replaced. An import
   has already committed its new solutions, so keeps those bytes. */
static void forget_auto_diagram(xpp::Session &s, bool remove_solutions)
{
  s.auto_state.from_auto_flag=false;
  s.auto_state.grabpt.flag=0;
  s.auto_state.diagram_mark.state=0;
  xpp::files::remove(s.auto_state.file+".b");
  xpp::files::remove(s.auto_state.file+".d");
  if(remove_solutions)xpp::files::remove(s.auto_state.file+".s");
}

int yes_reset_auto(xpp::Session &s)
{
  if(diagram_count(s.diagram)<=1)return(0);
 start_diagram(s,s.model().node);
 forget_auto_diagram(s,true);
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
  s.auto_state.homo_shift=tshift;
  xpp::log_auto("shifting {:g}\n",s.auto_state.homo_shift);
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
  s.auto_state.homo_flag=HomoclinicRun::None;
  if(xpp::solver_info(s.numerics.method).traits.discrete){
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
    command_error("auto", "Integrate first: this start takes its orbit from the last integration");
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
   s.auto_state.homo_flag=HomoclinicRun::Homoclinic;
   auto_start_at_homoclinic(s);
     return;
   }
     
 if(ch=='e'){
   s.auto_state.homo_flag=HomoclinicRun::Heteroclinic;
   auto_start_at_homoclinic(s);
   return;
 }

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

}

void hopf_choice(xpp::Session &s)
{
  if(xpp::solver_info(s.numerics.method).traits.discrete){
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
}

void auto_run(xpp::Session &s)
{
  char ch;
  if(s.auto_state.grabpt.flag==0){   /* the first call to AUTO   */
    auto_start_choice(s);
    ping();return;
  }
  if(s.auto_state.grabpt.lab==0){
    ch=static_cast<char>(TwoChoice("YES","NO","Not Labeled Pt: New Start?","y"));
    if(ch=='y')auto_start_diff_ss(s);
    ping();return;
  }
    
  const xpp::AutoPointCode itp=s.auto_state.grabpt.itp;
  const xpp::AutoProblem ips=s.auto_state.bifur.ips;
  /* AUTO's own: the kind (itp%10) and start (itp/10) of a UZ or MX point
     are negative, so a negative code names none of the points below */
  const bool plain=itp.kind>=xpp::AutoPointKind::Regular;
  const auto involves=[&](xpp::AutoPointKind k){ return plain && (itp.kind==k || itp.start==k); };
  if(s.auto_state.grabpt.curve==auto_neutral_saddle_curve){
    auto_2p_hopf(s);
    ping();return;
  }
  if(involves(xpp::AutoPointKind::Hopf)){  /* its a HOPF Point  */
    hopf_choice(s);
    ping();return;
  }
  if(involves(xpp::AutoPointKind::PeriodDoubling)){ /* period doubling */
    per_doub_choice(s);
    ping();return;
  }
  if(ips==xpp::AutoProblem::Homoclinic){
    auto_homo_choice(s, itp);
    ping(); return;
  }
  if(involves(xpp::AutoPointKind::Fold)){ /* limit point */
     s.auto_state.bifur.ips=xpp::solver_info(s.numerics.method).traits.discrete ? xpp::AutoProblem::DiscreteMap : xpp::AutoProblem::SteadyState;
     auto_2p_limit(s, s.auto_state.bifur.ips);
    ping();return;
  }
  if(involves(xpp::AutoPointKind::PeriodicFold)){ /* limit pt of periodic or BVP */
    if(s.auto_state.bifur.ips!=xpp::AutoProblem::BoundaryValue)
      s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;  /* this is a bit dangerous - the idea is that
                      if you are doing BVPs, then that is all you are
                      doing  
		   */
    auto_2p_limit(s, s.auto_state.bifur.ips);
    ping(); return;
  }
  if(involves(xpp::AutoPointKind::PeriodicBranch)||involves(xpp::AutoPointKind::BranchPoint)){ /* branch point  */ 

  auto_branch_choice(s, s.auto_state.grabpt.ibr,ips);
    ping();
    return;
  }
  if(involves(xpp::AutoPointKind::Torus)){ /* Torus 2 parameter */
    torus_choice(s);
    ping();
    return;
  }
  if(s.auto_state.grabpt.ibr<0) { /* its a periodic -- just extend it  */
    periodic_choice(s);
    ping();return;
  }
  if(s.auto_state.grabpt.ibr>0&&ips!=xpp::AutoProblem::BoundaryValue){ /*  old steady state -- just extend it  */
    auto_extend_ss(s);
    ping();return;
  }
  if(s.auto_state.grabpt.ibr>0&&ips==xpp::AutoProblem::BoundaryValue){
    auto_extend_bvp(s);
    ping();
    return;
  }
}

void auto_homo_choice(xpp::Session &s, xpp::AutoPointCode itp)
{
  if(!itp.is(xpp::AutoPointKind::PeriodicFold))
    auto_extend_homoclinic(s);
  
}
void auto_branch_choice(xpp::Session &s, int ibr, xpp::AutoProblem ips)
{

  char ch;
  xpp::AutoProblem ipsuse;
  ch=static_cast<char>(menu_choose(&menu_auto_branch,0));

  if(ch=='s'){
       if(ibr<0&&ips==xpp::AutoProblem::Periodic)
      auto_switch_per(s);
    else 
      if(ips==xpp::AutoProblem::BoundaryValue)
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
 
    ipsuse=xpp::AutoProblem::SteadyState;
    if(ips==xpp::AutoProblem::BoundaryValue)
      ipsuse=xpp::AutoProblem::BoundaryValue;
    if(ibr<0)
      ipsuse=xpp::AutoProblem::Periodic;
    auto_2p_branch(s, ipsuse);
    return;
  }
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
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  if(xpp::solver_info(s.numerics.method).traits.discrete)s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=no_calculation;
  do_auto(s, NO_OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_start_at_bvp(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
 xpp::compile_bvp(s);
  if(!s.numerics.bvp_flag)
    return; 
  s.auto_state.type_of_calc=BV1;
 s.auto_state.bifur.ips=xpp::AutoProblem::BoundaryValue;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
  if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
    
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=no_calculation;
  s.auto_state.new_period_flag=true;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_start_at_per(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=no_calculation;
  s.auto_state.new_period_flag=true;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_new_ss(xpp::Session &s)
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=false;

  if(diagram_count(s.diagram)>1){
    ans=reset_auto(s);
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
      if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=no_calculation;
  do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

void auto_new_discrete(xpp::Session &s)
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=false;
  if(diagram_count(s.diagram)>1){
    ans=reset_auto(s);
    if ((ans!=0) && (ans!=1))
    {
       xpp::log(XPP_LOG_WARN, "Boolean response expected.\n");
    }
  }
  s.auto_state.type_of_calc=DI1;
  s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=no_calculation;
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
  
  if (isinf(s.auto_state.grabpt.per))
  {
  	command_error("auto", "Can't continue infinite period Hopf!");
  	return;
  } 
  
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  if(xpp::solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
    
  s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

int get_homo_info(xpp::Session &s, int *nun,int *nst,double *ul, double *ur)
{
  int n=2+2*s.model().node;
  std::vector<std::string> v(n);
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
    const int unstable=atoi(v[0].c_str()),stable=atoi(v[s.model().node+1].c_str());
    /* HomCont's projection takes that many directions at each end: one
       condition for each of the equilibrium's dimensions */
    if(unstable<0||stable<0||unstable+stable!=s.model().node){
      command_error("auto",xpp::format("dim unstable ({}) and dim stable ({}) must add up to the number of variables, {}",
                                       unstable,stable,s.model().node));
      return 0;
    }
    *nun=unstable;
    *nst=stable;
    for(i=0;i<s.model().node;i++){
      ul[i]=atof(v[i+1].c_str());
      if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)
	ur[i]=atof(v[i+2+s.model().node].c_str());
    }
  }
  return flag;
}

void auto_extend_homoclinic(xpp::Session &s)
{
   s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;

      s.auto_state.type_of_calc=HO2;
  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=true;
  s.auto_state.bifur.ips=xpp::AutoProblem::Homoclinic;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==HomoclinicRun::Homoclinic)
    s.auto_state.run.iequib=1;
  if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)
    s.auto_state.run.iequib=-2;

  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);

}

void auto_start_at_homoclinic(xpp::Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  int flag;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=xpp::AutoPointCode{};
    s.auto_state.type_of_calc=HO2;

  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=true;
  s.auto_state.bifur.ips=xpp::AutoProblem::Homoclinic;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==HomoclinicRun::Homoclinic)
    s.auto_state.run.iequib=1;
  if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)
    s.auto_state.run.iequib=-2;
  flag=get_homo_info(s, &s.auto_state.run.nunstab,&s.auto_state.run.nstab,s.auto_state.homo_l.data(),s.auto_state.homo_r.data());
  /* the shift is measured from the equilibrium the form has just given */
  if(flag&&s.auto_state.homo_flag==HomoclinicRun::Homoclinic)find_best_homo_shift(s, s.model().node);
  if(flag)do_auto(s, opn,cls,s.auto_state.bifur.itp);

}
    
void auto_new_per(xpp::Session &s) /* same for extending periodic  */
{
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  
  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(s.auto_state.grabpt.per))
  {
  	command_error("auto", "Can't continue infinite period Hopf.");
  	return;
  } 	
      s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1; /* -1 */
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;
    s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_extend_bvp(xpp::Session &s) /* extending bvp */
{
      s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::BoundaryValue;
    s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_per(xpp::Session &s)
{
      s.auto_state.type_of_calc=PE1;
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=1; /*s.auto_state.grabpt.nfpar;*/
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;
  s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_bvp(xpp::Session &s)
{
     s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::BoundaryValue;
  s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_switch_ss(xpp::Session &s)
{

      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  if(xpp::solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_2p_limit(xpp::Session &s, xpp::AutoProblem ips)
{
  xpp::AutoProblem ipsuse=xpp::AutoProblem::SteadyState;
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  const xpp::AutoPointCode grabbed=s.auto_state.grabpt.itp;
  s.auto_state.bifur.itp=grabbed;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=1; /* was 2 */
  /* fix ips now */
  if(ips==xpp::AutoProblem::DiscreteMap) ipsuse=ips;
  if(ips==xpp::AutoProblem::BoundaryValue)
    ipsuse=xpp::AutoProblem::BoundaryValue;
  else {
    if(grabbed.kind==xpp::AutoPointKind::PeriodicFold||grabbed.start==xpp::AutoPointKind::PeriodicFold)
      ipsuse=xpp::AutoProblem::Periodic;
  }

  if(!xpp::problem_is_steady_or_map(ipsuse))
    s.auto_state.bifur.isp=ipsuse==xpp::AutoProblem::Periodic && !s.auto_state.suppress_bp ? 2 : 0;
  s.auto_state.bifur.ips=ipsuse;
  s.auto_state.two_param=LPP2;
  if(xpp::problem_is_steady_or_map(ipsuse)){
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
void auto_2p_periodic(xpp::Session &s, AutoCalculation kind)
{
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.two_param=kind;
  s.auto_state.type_of_calc=kind;
  s.auto_state.bifur.ips=xpp::solver_info(s.numerics.method).traits.discrete ? xpp::AutoProblem::DiscreteMap : xpp::AutoProblem::Periodic;
  s.auto_state.bifur.ilp=0;
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=s.auto_state.suppress_bp ? 0 : (s.auto_state.bifur.ips==xpp::AutoProblem::DiscreteMap ? 1 : 2);
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

void auto_2p_branch(xpp::Session &s, xpp::AutoProblem ips)
{
 xpp::AutoProblem ipsuse=xpp::AutoProblem::SteadyState;
 s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  const xpp::AutoPointCode grabbed=s.auto_state.grabpt.itp;
  s.auto_state.bifur.itp=grabbed;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0; /* was 2 */
  if(ips==xpp::AutoProblem::BoundaryValue)
    ipsuse=xpp::AutoProblem::BoundaryValue;
  else {
    if(grabbed.kind==xpp::AutoPointKind::PeriodicBranch||grabbed.start==xpp::AutoPointKind::PeriodicBranch)
      ipsuse=xpp::AutoProblem::Periodic;
  }

  s.auto_state.bifur.ips=ipsuse;
  if(xpp::solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  s.auto_state.two_param=BR2;
      s.auto_state.type_of_calc=BR2;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_2p_fixper(xpp::Session &s)
{
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* was1 */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;
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
  
  if (isinf(s.auto_state.grabpt.per))
  {
  	command_error("auto", "Can't continue infinite period Hopf.");
  	return;
  } 
  
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=1;
  s.auto_state.bifur.ips=xpp::AutoProblem::SteadyState;
  if(xpp::solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=xpp::AutoProblem::DiscreteMap;
  if(s.auto_state.bifur.ips!=xpp::AutoProblem::SteadyState) s.auto_state.bifur.isp=0;
  s.auto_state.two_param=auto_hopf_curve;
    s.auto_state.type_of_calc=auto_hopf_curve;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

void auto_period_double(xpp::Session &s)
{

 s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.ntst=2*s.auto_state.bifur.ntst;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.nfpar=1; /* s.auto_state.grabpt.nfpar; */

  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=xpp::AutoProblem::Periodic;
  s.auto_state.two_param=no_calculation;
  do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

/**********   END RUN AUTO *********************/

void load_auto_orbit(xpp::Session &s)
{
  xpp::ok_or_show(load_auto_orbitx(s,s.auto_state.grabpt.ibr,s.auto_state.grabpt.flag,s.auto_state.grabpt.lab,s.auto_state.grabpt.per));
}
xpp::Result<> load_auto_orbitx(xpp::Session &s, int ibr,int flag, int lab, double per)
{
  double *x;
  int i,j,nstor;
  std::vector<double> u(s.model().node);
  double t;
  double period;
  std::string string;
  int nrow,ndim,label,flg;

  if((ibr>0&&(s.auto_state.bifur.ips!=xpp::AutoProblem::BoundaryValue)&&(s.auto_state.bifur.ips!=xpp::AutoProblem::Homoclinic))||flag==0)return {};
   /* either nothing grabbed or just a fixed point and that is already loaded */
  string=s.auto_state.file+".s";
  xpp::UniqueFile fp=xpp::open_read(string.c_str());
  if(!fp){
    return xpp::fail_reading("AUTO","cannot be read",string);
  }
  label=lab;
  period=per;
  flg=move_to_label(s,label,&nrow,&ndim,fp.get());
  if(flg==0){
    xpp::log_auto("Could not find label {:d} in file {} \n",label,string.c_str());
    return xpp::fail("AUTO",xpp::format("No point labelled {} in {}",label,string),command_place());
  }
  if(nrow<1||ndim<1||ndim>s.model().node)
    return xpp::fail_reading("AUTO","invalid orbit dimensions",string);
  nstor=std::min(ndim,s.model().node);
  if(nrow>s.data_store.max_rows){
    if(const xpp::Result<> grown=s.data_store.grow(s.model().neq+1,nrow);!grown)return grown;
    s.data_store.max_rows=nrow;
  }
  x=&s.data_store.current[0];
  for(i=0;i<nrow;i++){
    get_a_row(u.data(),&t,ndim,fp.get());
    if(s.auto_state.bifur.ips!=xpp::AutoProblem::BoundaryValue) 
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
  if(s.auto_state.load_all_labeled_orbits==LabeledOrbits::ClearAndDraw)clr_all_scrns(s);
  drw_all_scrns(s);
  return {};
}

void save_auto(xpp::Session &s)
{
  if(!xpp::save_ready(diagram_count(s.diagram)>=2))return;
  xpp_session_save(s, nullptr, -1);
}

// Foreign PAR numbering is translated here; unselected prefix entries are not model values.
static int imported_solver_index(const xpp::Session &s, std::span<const int> imported, int slot)
{
  return slot>=0 && static_cast<std::size_t>(slot)<imported.size()
    ? s.auto_state.parameter_layout.solver_index(imported[slot]) : slot;
}

static xpp::Result<std::string> convert_auto_solutions(const xpp::Session &s, std::span<const int> imported,
    std::string_view text, const std::string &file)
{
  /* Validate the foreign restart payload before translating its selected slots. */
  const auto checked=xpp::check_auto_solutions(text,file,AutoState::state_copies*s.model().node,s.auto_state.parameter_layout.block,true);
  if(!checked)return std::unexpected(checked.error());
  const auto converted=xpp::read_lines("import AUTO solutions",file,text,[&s,imported](xpp::Lines &lines){
    std::string out;

    while(!lines.at_end()){
      xpp::AutoSolutionHeader h;
      const auto header=lines.next();
      if(xpp::trim_blanks(header).empty())continue;
      auto reader=xpp::TokenReader::of_text(header);
      if(!xpp::read_auto_solution_header(reader,h,s.auto_state.parameter_layout.block))lines.fail("invalid foreign solution header");
      if(h.npar<static_cast<long>(imported.size()))lines.fail("foreign solutions omit selected parameters");
      const std::size_t orbit=static_cast<std::size_t>(h.ntpl)*h.nar;
      const std::size_t prefix=orbit+(h.ntst?2*h.nfpr+static_cast<std::size_t>(h.ntpl)*(h.nar-1):0);
      // A finite double's shortest round-trip spelling plus separator fits in 32 bytes.
      constexpr std::size_t imported_number_bytes = 32;
      // Header integers also fit in that allowance; reserve twelve header fields.
      const std::size_t expanded=(prefix+s.auto_state.parameter_layout.size()+auto_solution_header_fields)*imported_number_bytes;
      if(expanded>xpp::zip::archive_bytes_limit-out.size())lines.fail("translated AUTO solutions exceed the session memory limit");
      std::vector<double> parameters(s.auto_state.parameter_layout.size());
      std::string body;
      std::size_t count=0;
      int body_rows=0;
      for(int row=0;row<h.nskip;row++){
        const auto text_row=lines.next();
        xpp::Tokens tokens(text_row);
        std::string converted_row;
        while(const auto word=tokens.next(" \t\r\n")){
          if(count>=prefix){
            const int slot=static_cast<int>(count-prefix);
            double value;
            if(!xpp::parse_number(*word,value))lines.fail("invalid foreign parameter");
            if(slot>=xppaut_parameter_prefix || static_cast<std::size_t>(slot)<imported.size())parameters[imported_solver_index(s,imported,slot)]=value;
          } else {
            std::string value(*word);
            if(h.ntst && count>=orbit && count<orbit+static_cast<std::size_t>(h.nfpr)){
              int slot;
              if(!xpp::parse_int(*word,slot))lines.fail("invalid foreign restart index");
              if(slot<xppaut_parameter_prefix && static_cast<std::size_t>(slot)>=imported.size())lines.fail("restart names an unselected foreign parameter");
              value=xpp::number(imported_solver_index(s,imported,slot));
            }
            if(!converted_row.empty())converted_row+=' ';
            converted_row+=value;
          }
          count++;
        }
        if(!converted_row.empty()){
          body+=converted_row+'\n';
          body_rows++;
        }
      }
      h.npar=static_cast<long>(parameters.size());
      h.nskip=body_rows+(h.npar+auto_solution_columns-1)/auto_solution_columns;
      const std::string header_text=xpp::format("{} {} {} {} {} {} {} {} {} {} {} {}\n",h.ibr,h.ntot,xpp::auto_point_encode(h.itp),h.lab,h.nfpr,h.isw,h.ntpl,h.nar,h.nskip,h.ntst,h.ncol,h.npar);
      if(header_text.size()+body.size()+parameters.size()*imported_number_bytes>xpp::zip::archive_bytes_limit-out.size())
        lines.fail("translated AUTO solutions exceed the session memory limit");
      out+=header_text;
      out+=body;
      for(std::size_t i=0;i<parameters.size();i++)out+=xpp::number(parameters[i])+((i+1)%auto_solution_columns==0 || i+1==parameters.size()?"\n":" ");
    }
    return out;
  });
  return converted;
}

void load_auto(xpp::Session &s)
{
  xpp_session_load(s, nullptr);
}

/* Foreign XPPAUT diagrams remain an explicit import, separate from Load. */
static void import_auto(xpp::Session &s)
{
  std::string filename=xpp::files::output_name(s.model().this_file, ".auto");
  if(!file_selector("Import XPPAUT diagram",filename,"*.auto"))return;
  xpp::UniqueFile fp=xpp::open_read(filename);
  if(!fp){show_error(xpp::files::open_error("AUTO",filename));return;}
  xpp::ok_or_show(import_auto_file(s,fp.get(),filename));
}

std::string auto_solutions_file(const xpp::Session &s)
{
  return s.auto_state.file+".s";
}

namespace {
struct ImportedAuto {
  xpp::AutoSettingsSet settings;
  int mark_count=0;
  std::vector<int> parameters;
  std::deque<DIAGRAM> points;
  std::string solutions;
};
}

xpp::Result<> import_auto_file(xpp::Session &s, FILE *fp, const std::string &file)
{
  std::string text;
  if(!xpp::read_bytes(fp,text,xpp::zip::archive_bytes_limit))
    return xpp::fail("import AUTO","cannot read foreign AUTO file within the session memory limit",xpp::Place{file});
  auto read=xpp::read_lines("import AUTO",file,text,[&](xpp::Lines &lines){
    ImportedAuto held;
    auto &settings=held.settings;
    std::map<std::string,int> locations;
    auto first=xpp::TokenReader::of_text(lines.next("selected parameters and marks"));
    int count;
    if(!first.read(count) || count<1 || count>std::min(s.model().nupar,xppaut_parameter_prefix))
      lines.fail("invalid foreign selected parameter count");
    for(int k=0;k<count;k++){
      int parameter;
      if(!first.read(parameter) || parameter<0 || parameter>=s.model().nupar
          || std::find(held.parameters.begin(),held.parameters.end(),parameter)!=held.parameters.end())
        lines.fail("invalid or repeated foreign model parameter index");
      held.parameters.push_back(parameter);
    }
    if(!first.read(held.mark_count) || held.mark_count<0 || held.mark_count>xpp::auto_foreign_mark_rows || !first.at_end())
      lines.fail("foreign mark count exceeds the stored mark slots");
    // Validate every encoded row, including inactive foreign marks.
    settings.nmarks=xpp::auto_foreign_mark_rows;
    settings.mark_name.resize(*settings.nmarks);
    settings.mark_value.resize(*settings.nmarks);
    settings.par1=s.model().upar_names[held.parameters.front()];
    if(count>1)settings.par2=s.model().upar_names[held.parameters[1]];
    for(int k=0;k<xpp::auto_foreign_mark_rows;k++){
      int slot;
      double value;
      lines.numbers(value,slot);
      const int solver=imported_solver_index(s,held.parameters,slot);
      const int parameter=s.auto_state.parameter_layout.model_index(solver);
      if(slot<0 || (solver!=xpp::auto_unknown::period && parameter<0))
        lines.fail("invalid foreign mark parameter index");
      settings.mark_value[k]=value;
      settings.mark_name[k]=solver==xpp::auto_unknown::period ? "T" : s.model().upar_names[parameter];
      locations[xpp::format("mark{}",k)]=lines.line();
    }
    const auto numeric=[&](std::initializer_list<int> fields){
      auto row=xpp::TokenReader::of_text(lines.next("AUTO numerics"));
      for(int field:fields){
        auto &value=settings.num[field];
        if(!row.read(value))lines.fail("invalid or incomplete AUTO numerics");
        std::string why;
        if(!xpp::auto_settings_num_ok(field,value,why))lines.fail(why);
        settings.has_num[field]=1;
        locations[xpp::auto_settings_num_key(field)]=lines.line();
      }
      if(!row.at_end())lines.fail("extra AUTO numeric fields");
    };
    numeric({xpp::AUTO_NUM_NTST,xpp::AUTO_NUM_NMX,xpp::AUTO_NUM_NPR});
    numeric({xpp::AUTO_NUM_DS,xpp::AUTO_NUM_DSMIN,xpp::AUTO_NUM_DSMAX});
    numeric({xpp::AUTO_NUM_RL0,xpp::AUTO_NUM_RL1,xpp::AUTO_NUM_A0,xpp::AUTO_NUM_A1});
    numeric({xpp::AUTO_NUM_IAD,xpp::AUTO_NUM_MXBF,xpp::AUTO_NUM_IID,xpp::AUTO_NUM_ITMX,xpp::AUTO_NUM_ITNW,xpp::AUTO_NUM_NWTN,xpp::AUTO_NUM_IADS});
    int var,plot;
    lines.numbers(settings.range[0],settings.range[2],settings.range[1],settings.range[3],var,plot);
    if(var<0 || var>=s.model().node)lines.fail("foreign axis variable is outside the model");
    settings.var=s.model().uvar_names[var];
    settings.plot=xpp::auto_projection_from_xppaut(plot);
    if(!settings.plot)lines.fail(xpp::format("{} is not one of XPPAUT's plot types (0-4, 10, 11)",plot));
    settings.has_range.fill(1);
    // XPPAUT omits Ncol; the retained collocation order must still be supported.
    settings.has_num[xpp::AUTO_NUM_NCOL]=1;
    settings.num[xpp::AUTO_NUM_NCOL]=xpp::auto_settings_now(s).num[xpp::AUTO_NUM_NCOL];
    if(const xpp::Result<> checked=xpp::auto_settings_check(s,settings,-1);!checked){
      const std::string &key=checked.error().field;
      lines.fail(locations.contains(key)?locations.at(key):lines.line(),checked.error().what);
    }
    std::vector<double> parameters;
    for(int k=0;k<s.model().nupar;k++)parameters.push_back(s.parser.constants[s.model().upar_con[k]]);
    held.points=xpp::snapx::auto_members::read_foreign_diagram(lines,s.model().node,held.parameters,parameters,
        s.auto_state.type_of_calc,s.auto_state.two_param,s.auto_state.blrtn.torper);
    const int offset=lines.line();
    std::string tail;
    while(!lines.at_end())tail+=std::string(lines.next())+'\n';
    const auto solutions=convert_auto_solutions(s,held.parameters,tail,file);
    if(!solutions)lines.fail(offset+solutions.error().place.line,solutions.error().what);
    held.solutions=*solutions;
    return held;
  });
  if(!read)return std::unexpected(read.error());
  auto held=std::move(*read);
  held.settings.nmarks=held.mark_count;
  held.settings.mark_name.resize(held.mark_count);
  held.settings.mark_value.resize(held.mark_count);
  // The session owner commits restart bytes before any held state, as for .snapx.
  auto restored=xpp::snapx::auto_members::restore_members(s,
      {held.settings,std::move(held.points),{},held.solutions});
  if(!restored)return restored;
  s.auto_state.imported_parameters=std::move(held.parameters);
  forget_auto_diagram(s,false);
  return {};
}

/* Bound expanded solution values by the archive memory budget before any
   restart mesh or orbit-table allocation can use counts from a sent file. */
constexpr std::size_t auto_solution_values_limit = zip::archive_bytes_limit / sizeof(double);

bool read_auto_solution_header(TokenReader &reader, AutoSolutionHeader &h, long parameters)
{
    long itp_code{};
    for (long *v : {&h.ibr, &h.ntot, &itp_code, &h.lab, &h.nfpr, &h.isw, &h.ntpl, &h.nar, &h.nskip, &h.ntst, &h.ncol, &h.npar})
        if (!reader.read(*v) || *v <= std::numeric_limits<int>::min() || *v > std::numeric_limits<int>::max()) return false;
    /* a stored point's type is one of AUTO's: the header of any other is bad */
    const auto itp = xpp::auto_point_decode(itp_code);
    if (!itp) return false;
    h.itp = *itp;
    std::string why;
    const bool collocation = h.ntst == 0 ? h.ncol == 0 : auto_settings_num_ok(AUTO_NUM_NCOL, h.ncol, why);
    const bool mesh = h.ntst == 0 || auto_settings_num_ok(AUTO_NUM_NTST, h.ntst, why);
    return h.nfpr >= 0 && h.nfpr <= parameters && h.ntpl >= 1 && static_cast<std::size_t>(h.ntpl) <= auto_solution_values_limit / static_cast<std::size_t>(h.nar < 2 ? 2 : h.nar) && h.nar >= 2 && static_cast<std::size_t>(h.nar) <= auto_solution_values_limit
        && h.nskip >= h.ntpl && h.ntst >= 0 && h.ntst <= h.nskip
        && mesh && collocation && h.npar >= 0 && h.npar <= parameters && h.nfpr <= h.npar
        && (h.ntst == 0 || h.ntpl == static_cast<long long>(h.ntst) * h.ncol + 1);
}

bool read_auto_solution_rows(TokenReader &reader, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol,
                             std::ptrdiff_t stride, std::ptrdiff_t first, std::ptrdiff_t count, double *values,
                             double *tm, bool line_bounded, std::ptrdiff_t extra_lines,
                             const std::function<void(std::ptrdiff_t, std::ptrdiff_t, double)> &complete)
{
    const auto finish_line = [&] {
        if (line_bounded) {
            reader.skip_line();
            for (std::ptrdiff_t line = 0; line < extra_lines; ++line) reader.skip_line();
        }
    };
    for (std::ptrdiff_t j = 0; j < ntst; ++j) {
        double first_time = 0.;
        for (std::ptrdiff_t i = 0; i < ncol; ++i) {
            double time = 0.;
            if (tm) {
                if (!reader.read(time)) return false;
                if (i == 0) first_time = time;
            }
            for (std::ptrdiff_t k = first; k < first + count; ++k)
                if (!reader.read(values[j + (i * ndim + k) * stride])) return false;
            finish_line();
            if (complete) complete(j, i, time);
        }
        if (tm) tm[j] = first_time;
    }
    if (tm && !reader.read(tm[ntst])) return false;
    for (std::ptrdiff_t k = first; k < first + count; ++k)
        if (!reader.read(values[ntst + k * stride])) return false;
    finish_line();
    return true;
}

bool read_auto_solution_parameters(TokenReader &reader, const AutoSolutionHeader &header,
                                   const Session &session, double *parameters)
{
    for (long i = 0; i < header.npar; ++i)
        if (!reader.read(parameters[i])) return false;
    auto_label_parameters(session, static_cast<int>(header.lab), parameters);
    return true;
}

Result<> check_auto_solutions(std::string_view text, std::string file, int variables, int parameters, bool foreign, int hopf_dimension)
{
    const auto read = read_lines("AUTO's solutions", std::move(file), text, [variables, parameters, foreign, hopf_dimension](Lines &l) {
        while (!l.at_end()) {
            const std::string_view head = l.next();
            if (head.empty()) continue;
            AutoSolutionHeader h;
            TokenReader header = TokenReader::of_text(head);
            if (!read_auto_solution_header(header, h, parameters) || !header.at_end())
                l.fail(xpp::format("invalid AUTO solution header or counts: {}",head));
            if (!foreign && h.npar != parameters)
                l.fail(xpp::format("AUTO solution parameter layout has {} slots; expected {}: {}",h.npar,parameters,head));
            if (h.nar - 1 > variables || h.npar > parameters || h.nfpr > h.npar)
                l.fail(xpp::format("AUTO solution counts ({}, {}, {}) exceed this model's storage ({}, {})",h.nar-1,h.npar,h.nfpr,variables,parameters));
            if (!foreign && hopf_dimension && h.ntst == 0 && (h.itp.started_from(AutoPointKind::Hopf) || h.itp.started_from(AutoPointKind::Torus))
                && h.nar != hopf_dimension + 1)
                l.fail(xpp::format("AUTO Hopf solution has {} states; expected {} (the current layout): {}", h.nar - 1, hopf_dimension, head));
            const long rows = h.ntpl, columns = h.nar, payload = h.nskip, parameter_values = h.npar;
            const std::size_t orbit_values = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
            const std::size_t expected = orbit_values + parameter_values
                + (h.ntst ? 2 * h.nfpr + static_cast<std::size_t>(rows) * (columns - 1) : 0);
            if (expected > auto_solution_values_limit) l.fail("AUTO solution counts exceed the session memory limit");
            std::size_t values = 0;
            const std::string payload_name=xpp::format("an AUTO solution payload row for label {}",h.lab);
            for (int row = 0; row < payload; row++) {
                const auto row_text = l.next(payload_name);
                Tokens tokens(row_text);
                if (trim_blanks(row_text).empty()) l.fail("empty AUTO solution payload row");
                while (const auto word = tokens.next(" \t\r\n")) {
                    double value;
                    if (!parse_number(*word, value) || !std::isfinite(value)) l.fail("invalid AUTO solution number");
                    if (h.ntst && values >= orbit_values && values < orbit_values + h.nfpr) {
                        int parameter;
                        if (!parse_int(*word, parameter) || parameter < 0 || parameter >= h.npar)
                            l.fail("AUTO restart parameter index is out of bounds");
                    }
                    values++;
                }
            }
            if (values != expected)
                l.fail(xpp::format("AUTO solution label {} payload does not match its header counts",h.lab));
        }
        return true;
    });
    if (!read) return std::unexpected(read.error());
    return {};
}

int move_to_label(const Session &s, int mylab, int *nrow, int *ndim, FILE *fp)
{
  AutoSolutionHeader h;
  TokenReader tr = TokenReader::attach(fp);
  while (read_auto_solution_header(tr, h, s.auto_state.parameter_layout.size())) {
    tr.skip_line();
    if (mylab == h.lab) {
      *nrow = static_cast<int>(h.ntpl);
      *ndim = static_cast<int>(h.nar) - 1;
      return 1;
    }
    for (long i = 0; i < h.nskip; i++)
      if (!tr.skip_line()) return 0;
  }
  return 0;
}

void get_a_row(double *u, double *t, int n, FILE *fp)
{
  auto reader = TokenReader::attach(fp);
  read_auto_solution_rows(reader, n, 0, 0, 1, 0, n, u, t, false, 0, {});
}

/* the diagram and its eigenvalues/multipliers as CSV (csv_export.h),
   pandas.read_csv/MATLAB readtable read with no options; one file dialog
   answer names both files (csv_export_diagram_pair derives the second) */
void export_auto_csv(xpp::Session &s)
{
  if(!xpp::save_ready(diagram_count(s.diagram)>=2))return;
  std::string filename=xpp::files::output_name(s.model().this_file,".csv","diagram");
  if(!file_selector("Export CSV",filename,"*.csv"))return;
  const xpp::Result<bool> written=csv_export_diagram_pair(s,filename.c_str());
  if(!written)xpp::show_error(written.error());
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
    s.auto_state.grabpt.flag=0;
  }
  if(ch=='p'){
    export_auto_picture(s, xpp::IMAGE_FORMAT_PDF);
  }
  if(ch=='v'){
    export_auto_picture(s, xpp::IMAGE_FORMAT_SVG);
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
    s.auto_state.redraw=!s.auto_state.redraw;
    if(s.auto_state.redraw)bottom_msg(0,"Redraw is ON");
    else bottom_msg(0,"Redraw is OFF");
  }
  if(ch=='o'){
    if(s.auto_state.diagram_mark.state<2)
      command_error("auto", "Mark a branch first using S and E");
    else
      load_browser_with_branch(s, s.auto_state.diagram_mark.start_branch,s.auto_state.diagram_mark.start_point,s.auto_state.diagram_mark.end_point);
	}
  if(ch=='x'){
    export_auto_csv(s);
  }
  if(ch=='f')import_auto(s);
  if(ch=='n'){
    if(s.auto_state.diagram_mark.state<2) 
      command_error("auto", "Mark a branch first using S and E");
    else
      do_auto_range(s);
  }
  if(ch=='e'){
    if(s.auto_state.axes().plot!=AutoProjection::two_parameter){
      command_error("auto", "Must be in 2 parameter plot");
      return;
    }
    setautopoint(s);

  }
  if(ch=='b'){
    switch(s.auto_state.load_all_labeled_orbits){
    case LabeledOrbits::Off:
      s.auto_state.load_all_labeled_orbits=LabeledOrbits::Draw;
      bottom_msg(0,"Draw orbits - no erase");
      return;
    case LabeledOrbits::Draw:
      s.auto_state.load_all_labeled_orbits=LabeledOrbits::ClearAndDraw;
      bottom_msg(0,"Draw orbits - erase first");
      return;
    case LabeledOrbits::ClearAndDraw:
      s.auto_state.load_all_labeled_orbits=LabeledOrbits::Off;
      bottom_msg(0,"Draw orbits off");
      return;
    }
  }

}

void auto_get_info(xpp::Session &s, int *n, std::string &pname)
{
  if(s.auto_state.diagram_mark.state==2){
    const int i1=abs(s.auto_state.diagram_mark.start_point);
    *n=abs(abs(s.auto_state.diagram_mark.end_point)-i1);
    if(const DIAGRAM *d=diagram_find(s.diagram,s.auto_state.diagram_mark.start_branch,i1))
      pname=s.model().upar_names[d->icp1.index];
  }
}

void auto_set_mark(xpp::Session &s, int i)
{
  int pt,ibr;
  if(s.auto_state.diagram_mark.state==2){
    ibr=s.auto_state.diagram_mark.start_branch;
    if(abs(s.auto_state.diagram_mark.start_point)<abs(s.auto_state.diagram_mark.end_point))
      pt=abs(s.auto_state.diagram_mark.start_point)+i;
    else
      pt=abs(s.auto_state.diagram_mark.end_point)+i;
    find_point(s, ibr,pt);
  }
}

void find_point(xpp::Session &s, int ibr, int pt)
{
  if(diagram_count(s.diagram)<2)return;
  const DIAGRAM *d=diagram_find(s.diagram,ibr,pt);
  if(d==NULL)return;
  /* now we use this info to set parameters and init data */
  for(int i=0;i<s.model().node;i++)
    setvar(s,i+1,d->u0[i]);
  xpp::set_initial_data(s,d->u0);
  auto_set_pars_from(s, diagram_parameters(*d).data());
  xpp::evaluate_derived(s);
  xpp::ok_or_show(xpp::redo_all_fun_tables(s));
  redraw_params();
  redraw_ics();
  if((d->per)>0)
    xpp::set_total(s,d->per);
}

void do_auto_range(xpp::Session &s)
{
  double t=s.numerics.total_time;
  
  if(s.auto_state.diagram_mark.state==2)
    xpp::do_auto_range_go(s);
  s.numerics.total_time=t;
}

/* ---- grabbing a point on the bifurcation diagram, marking a branch and
   the hint line (logic from auto_x11.c) ---- */
static void grab_diagram_point(xpp::Session &s, const DIAGRAM *d);
static void finish_grab(xpp::Session &s);

const char *query_special(const char *title)
{
	XppMenu menu=menu_auto_special;
	menu.title=title;
	int ch=static_cast<char>(menu_choose(&menu,1));
	int i=ch!=0?menu_index(&menu,ch):-1;
	return i>=0?menu.items[i]:NULL;
}

void traverse_diagram(xpp::Session &s)
{
  std::deque<DIAGRAM> &points=s.diagram.points;
  int done=0;
  int ix,iy;
  int xm,ym;
  s.auto_state.diagram_mark.state=0;
  if(diagram_count(s.diagram)<2)return;
  const int last=diagram_count(s.diagram)-1;
  int at=0; /* the point the cursor is on */
  const auto show=[&](int dodraw){ traverse_out(s,&points[at],&ix,&iy,dodraw); };
  /* the next (step 1) or previous (-1) point from the cursor's for which
     wanted holds, or -1 */
  const auto search=[&](int step, auto wanted){
    for(int i=at+step;i>=0&&i<=last;i+=step)
      if(wanted(points[i]))return i;
    return -1;
  };
  const auto symbol_search=[&](int step, const char *title, const char *missing){
    const char *nsymb=query_special(title);
    if(nsymb==NULL)return;
    const int i=search(step,[&](const DIAGRAM &d){ return strcmp(auto_point_symbol(d.itp),nsymb)==0; });
    if(i>=0)at=i;
    else{
      s.auto_state.bifur.hinttxt=xpp::format("  {} {} not found",missing,nsymb);
      ui.auto_show_hint(s);
    }
    show(1);
  };
  const auto labelled=[](const DIAGRAM &d){ return d.lab!=0; };

  show(1);
  while(done==0){
    const int kp=ui.auto_grab_event(s,&xm,&ym);
    if(kp==XPP_AUTO_NODE){
      /* a point of the diagram by its entry: the cursor goes there */
      if(xm>=0&&xm<=last){
        clear_msg(s);
        at=xm;
        show(1);
      }
    }
    else if(kp==XPP_AUTO_CLICK){
      /* the point nearest the click: every point visited for its place */
      clear_msg(s);
      at=0;
      show(0);
      int nearest=0;
      double ndist=s.auto_state.bifur.wid*s.auto_state.bifur.hgt;
      const LabeledOrbits lalo=s.auto_state.load_all_labeled_orbits;
      s.auto_state.load_all_labeled_orbits=LabeledOrbits::Off;
      for(;;){
        const double dist=sqrt((static_cast<double>((xm-ix)))*(static_cast<double>((xm-ix))) + (static_cast<double>((ym-iy)))*(static_cast<double>((ym-iy))));
        if(dist<ndist){
          ndist=dist;
          nearest=at;
        }
        if(at==last)break;
        ++at;
        show(0);
      }
      s.auto_state.load_all_labeled_orbits=lalo;
      show(0);
      at=nearest;
      show(1);
    }
    else {
      clear_msg(s);
      switch(kp){
      case RIGHT:
        at=at<last?at+1:0;
        show(1);
        break;
      case LEFT:
        at=at>0?at-1:0;
        show(1);
        break;
      case UP:
        symbol_search(1,"Next...","Higher");
        break;
      case DOWN:
        symbol_search(-1,"Previous...","Lower");
        break;
      case TAB: /* the next labelled point; wraps to the first */
        {
          const int i=search(1,labelled);
          at=i>=0?i:0;
        }
        show(1);
        break;
      case 's': /* mark the start of a branch */
        if(s.auto_state.diagram_mark.state==0) {
          s.auto_state.diagram_mark.start_branch=points[at].ibr;
          s.auto_state.diagram_mark.start_point=points[at].ntot;
          s.auto_state.diagram_mark.state=1;
        }
        break;
      case 'e': /* mark end of branch */
        if(s.auto_state.diagram_mark.state==1){
          s.auto_state.diagram_mark.end_branch=points[at].ibr;
          s.auto_state.diagram_mark.end_point=points[at].ntot;
          s.auto_state.diagram_mark.state=2;
        }
        break;
      case END: /* all the way to the end */
        at=last;
        show(1);
        break;
      case HOME: /* all the way to the beginning */
        at=0;
        show(1);
        break;
      case PGUP: /* the next labelled point, no wrap: the last point when none */
        {
          const int i=search(1,labelled);
          at=i>=0?i:last;
        }
        show(1);
        break;
      case PGDN: /* the previous labelled point: the first point when none */
        {
          const int i=search(-1,labelled);
          at=i>=0?i:0;
        }
        show(1);
        break;
      case FINE:
        done=1;
        ui.auto_grab_end(1);
        break;
      case ESC:
        done=-1;
        ui.auto_grab_end(-1);
        break;
      }
    }
  }
  /* check mark_flag branch similarity */
  if(s.auto_state.diagram_mark.state==2){
    if(s.auto_state.diagram_mark.start_branch!=s.auto_state.diagram_mark.end_branch)
      s.auto_state.diagram_mark.state=0;
  }
  if(done==1) grab_diagram_point(s, &points[at]);
  finish_grab(s);
}

/* takes diagram point d exactly as traverse_diagram's Return does: s.auto_state.grabpt,
   the parameters, the initial condition. The caller then calls
   finish_grab(), which traverse_diagram does whether or not a point was
   taken. */
static void grab_diagram_point(xpp::Session &s, const DIAGRAM *d)
{
  int i;
  s.auto_state.grabpt.ibr=d->ibr;
  s.auto_state.grabpt.lab=d->lab;
  s.auto_state.grabpt.par=diagram_parameters(*d);
  s.auto_state.grabpt.per=d->per;
  s.auto_state.grabpt.torper=d->torper;
  for(i=0;i<s.model().node;i++){
    s.auto_state.grabpt.uhi[i]=d->uhi[i];
    s.auto_state.grabpt.ulo[i]=d->ulo[i];
    s.auto_state.grabpt.u0[i]=d->u0[i];
    s.auto_state.grabpt.ubar[i]=d->ubar[i];
    setvar(s,i+1,s.auto_state.grabpt.u0[i]);
  }
  xpp::set_initial_data(s,s.auto_state.grabpt.u0);
  s.auto_state.grabpt.flag=1;
  s.auto_state.grabpt.itp=d->itp;
  s.auto_state.grabpt.curve=d->flag2;
  s.auto_state.grabpt.nfpar=d->nfpar;
  auto_set_pars_from(s, s.auto_state.grabpt.par.data());
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
   s.auto_state.grabpt, parameters, info strip and stability circle, and what a
   following Run starts from. Returns 0 and changes nothing for a label no
   stored point has. */
int auto_grab_label(xpp::Session &s, int lab)
{
  return grab_and_finish(s, diagram_of_label(s,lab));
}

/* grabs the index'th (1-based) stored point of AUTO's type `type`
   (auto_point_symbol's BP/EP/HB/LP/MX/PD/TR/UZ), in stored order: "the 2nd HB".
   Returns 0 and changes nothing when there is no such point. */
int auto_grab_type_index(xpp::Session &s, const char *type, int index)
{
  int count=0;
  if(index<1 || diagram_count(s.diagram)<2) return 0;
  for(const DIAGRAM &d:s.diagram.points)
    if(d.lab!=0 && strcmp(auto_point_symbol(d.itp),type)==0 && ++count==index)
      return grab_and_finish(s, &d);
  return 0;
}

void clear_msg(xpp::Session &s)
{
  s.auto_state.bifur.hinttxt.clear();
  ui.auto_show_hint(s);
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
    ui.auto_show_hint(s);
}


/* ---- AUTO's private scratch directory (session.h: AutoState::dir),
   made by xpp::files::make_temp_dir ---- */
void cleanup_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (!dir.empty()) {
    xpp::files::remove_temp_dir(dir.c_str());
    dir.clear();
  }
}

void renew_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (dir.empty()) return; /* none was made: AUTO writes beside the model */
  xpp::files::remove_temp_dir(dir.c_str());
  dir=xpp::files::make_temp_dir();
}

} // namespace xpp
