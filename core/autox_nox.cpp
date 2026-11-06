#include "autox_unknowns.h"
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
#include "autox_evd.h"
#include <charconv>
#include "autox_stop.h"
#include "jacobian.h"
#include "autox_stability.h"
#include "csv_export.h"
#include "image_format.h"
#include <libgen.h>
#include "graf_par.h"


#include "pp_shoot.h"

#include "xpp_files.h"
#include "xpp_zip.h"
#include "xpp_session_autox.h"
#include "autox_settings.h"
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
#include "autox_data.h"
#include "derived.h"   /* evaluate_derived() */
#include "tabular.h"   /* redo_all_fun_tables() */
#include "getvar.h"    /* setvar() */
#include "my_rhs.h"    /* extra() */
#include "menus.h"
#include "picture.h"
#include "solver.h"


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

/* autoX's continuation parameters back into the model (find_point, the
   Grab loop's Return): a diverged run's stored point can hold a
   non-finite value (QA SCI-001, -nan in the "state" event's pars). Skip
   it and keep the model's previous value instead, once per parameter. */
static void autox_set_pars_from(xpp::Session &s, const double *par)
{
    for (int i = 0; i < s.model().nupar; i++) {
        const int idx = s.model().upar_con[i];
        if (std::isfinite(par[i])) s.parser.constants[idx] = par[i];
        else
            xpp::log(XPP_LOG_WARN, "autoX: {} from the diagram is not finite; keeping {:.16g}",
                     s.model().upar_names[i], s.parser.constants[idx]);
    }
}

/* do_autox's cleanup after go_go_autox(): a run that diverges can leave
   autoX's own working copy of a continuation parameter (autox_pp.cpp's func,
   synced into constants[] every RHS call) not finite. Restore what the
   run started from instead of leaving -nan in the model (QA SCI-001). */
void autox_restore_finite_pars(xpp::Session &s, const double *before)
{
    for (int i = 0; i < s.model().nupar; i++) {
        const int idx = s.model().upar_con[i];
        if (!std::isfinite(s.parser.constants[idx])) {
            xpp::log(XPP_LOG_WARN, "autoX: the run left {} not finite; keeping {:.16g}",
                     s.model().upar_names[i], before[i]);
            s.parser.constants[idx] = before[i];
        }
    }
}





/* the colour of the two-parameter curve of kind flag2 */
static xpp::Color two_parameter_color(AutoxCalculation flag2)
{
  switch(flag2){
  case LPE2: return LPE_color;
  case LPP2: return LPP_color;
  case autox_neutral_saddle_curve:
  case autox_hopf_curve: return HB_color;
  case TR2: return TR_color;
  case BR2: return BR_color;
  case PD2: return PD_color;
  case FP2: return FP_color;
  default: return xpp::foreground_color;
  }
}

void storeautoxpoint(xpp::Session &s, double x,double y)
{
  if(s.autox_state.axes().plot==AutoxProjection::two_parameter){
    s.autox_state.from_autox_x=x;
    s.autox_state.from_autox_y=y;
    s.autox_state.from_autox_flag=true;
  }
}
void setautoxpoint(xpp::Session &s)
{
  if(s.autox_state.from_autox_flag)
    {
      s.autox_state.from_autox_flag=false;
      xpp::set_val(s,s.model().upar_names[s.autox_state.axes().icp1],s.autox_state.from_autox_x);
      xpp::set_val(s,s.model().upar_names[s.autox_state.axes().icp2],s.autox_state.from_autox_y);
      xpp::evaluate_derived(s);
      xpp::ok_or_show(xpp::redo_all_fun_tables(s));
      redraw_params();
    }
}
      
namespace {
/* autoX's parameter k's name, or "" (autox_par_name) */
std::string par_label(const xpp::Session &s, int k)
{
  const char *p=autox_par_name(s, k);
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
  case AutoxProjection::maximum:
  case AutoxProjection::maximum_and_minimum:
    l.y=s.model().uvar_names[ax.var];
    break;
  case AutoxProjection::norm:
    l.y="Norm";
    break;
  case AutoxProjection::period:
    l.y="Period";
    break;
  case AutoxProjection::frequency:
    l.y="Frequency";
    break;
  case AutoxProjection::two_parameter:
    l.y=par_label(s, ax.icp2);
    break;
  case AutoxProjection::average:
    l.y=s.model().uvar_names[ax.var]+"_bar";
    break;
  }
  return l;
}
} // namespace

void get_autox_str(const xpp::Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel)
{
  AxisLabels l=axis_labels(s, ax);
  xlabel=std::move(l.x);
  ylabel=std::move(l.y);
}

/* the diagram's axes in a PostScript or SVG export (diagram.cpp
   export_diagram), whichever ps_init/svg_init began */
void draw_export_axes(xpp::Session &s)
{
 set_scale(s,s.autox_state.axes().xmin,s.autox_state.axes().ymin,s.autox_state.axes().xmax,s.autox_state.axes().ymax);
 const AxisLabels l=axis_labels(s, s.autox_state.axes());
 Box_axis(s,s.autox_state.axes().xmin,s.autox_state.axes().xmax,s.autox_state.axes().ymin,s.autox_state.axes().ymax,l.x.c_str(),l.y.c_str(),0);
}

void draw_bif_axes(xpp::Session &s)
{
 /* the data of the diagram starts again, in every view */
 for(int v=0;v<static_cast<int>(s.autox_state.views.size());v++)autox_diagram(s,v,NULL);
 refreshdisplay(s);
}

int IXVal(const xpp::Session &s, double x)
{
  double temp=static_cast<double>(s.autox_state.bifur.wid)*(x-s.autox_state.axes().xmin)/(s.autox_state.axes().xmax-s.autox_state.axes().xmin);
  return (static_cast<int>(temp)+s.autox_state.bifur.x0);
}

int IYVal(const xpp::Session &s, double y)
{
  double temp=static_cast<double>(s.autox_state.bifur.hgt)*(y-s.autox_state.axes().ymin)/(s.autox_state.axes().ymax-s.autox_state.axes().ymin);
  return(s.autox_state.bifur.hgt-static_cast<int>(temp)+s.autox_state.bifur.y0);
}

void close_autox(xpp::Session &s, AutoxRun &run, int flg) /* labels compatible with A2K  */
{
  /* Close fp8 before the renames below: Windows refuses rename()/remove()
     on a file that is still open (see xpp::files::move), which left
     fort.8 behind next to <model>.s with the handle leaked. Linux allows
     renaming/removing an open file, which is likely why this was never
     turned on upstream -- it was dead code there, not a deliberate
     no-op. */
  run.fp8.reset();
  if(flg==0) {/*Overwrite*/
    xpp::files::move(run.fort7.c_str(),(s.autox_state.file+".b").c_str());
    xpp::files::move(run.fort9.c_str(),(s.autox_state.file+".d").c_str());
    xpp::files::move(run.fort8.c_str(),(s.autox_state.file+".s").c_str());
  }
  else {/*APPEND*/
    xpp::files::prepend(run.fort7.c_str(),(s.autox_state.file+".b").c_str());
    xpp::files::prepend(run.fort9.c_str(),(s.autox_state.file+".d").c_str());
    xpp::files::prepend(run.fort8.c_str(),(s.autox_state.file+".s").c_str());
  }

    xpp::files::remove(run.fort8.c_str());

    xpp::files::remove(run.fort7.c_str());
    xpp::files::remove(run.fort9.c_str());
    xpp::files::remove(run.fort3.c_str());

}

/* autoX keeps its .b/.d/.s files under HOME. A HOME that is set but unusable
   (missing, not writable) must fall back to the model's directory like an
   unset one, or the moves fail at the end of a run. */
static const char *autox_home_dir(xpp::Session &s, char *dname)
{
  const char *home;

  /* xppautX gives each session its own directory (xpp_globals.h) */
  if (!s.autox_state.dir.empty())
    return s.autox_state.dir.c_str();

  home = getenv("HOME");
  if (home == NULL || !xpp::files::dir_writable(home))
    home = dname;
  return home;
}

void create_autox_file_name(xpp::Session &s)
{
  /* basename()/dirname() may write into their argument or return a
     pointer into it, so each needs its own writable, NUL-terminated
     copy of s.model().this_file (std::string::data() is both since C++17) */
  std::string basec = s.model().this_file, dirc = s.model().this_file;
  char *bname = static_cast<char*>(basename(basec.data()));
  char *dname = static_cast<char*>(dirname(dirc.data()));

  const char* autox_dir = autox_home_dir(s, dname);

  s.autox_state.file=xpp::format("{}/{}",autox_dir,bname);
}

xpp::Result<> open_autox(xpp::Session &s, AutoxRun &run, int flg) /* compatible with new auto */
{
  /* the .b, .d and .s files close_autox moves the run's fort files to: a folder that cannot take them
     is refused before the run, as its fort files used to be */
  const std::string solutions=xpp::files::split_path(s.autox_state.file).first;
  if(!xpp::files::dir_writable(solutions))
    return xpp::fail("autox",xpp::format("cannot write autoX's files: the folder {} is not writable",solutions),xpp::command_place());
  run.folder.emplace();
  if(run.folder->path().empty())
    return xpp::fail("autox","Cannot make autoX's scratch folder for the run",xpp::command_place());
  run.fort3=run.folder->file("fort.3");
  run.fort7=run.folder->file("fort.7");
  run.fort8=run.folder->file("fort.8");
  run.fort9=run.folder->file("fort.9");

  if(flg==1){
    xpp::files::copy((s.autox_state.file+".s").c_str(),run.fort3.c_str());
  }
  return {};
}

const char *autox_fort_path(const AutoxRun &run, int unit)
{
  switch(unit){
  case 3: return run.fort3.c_str();
  case 7: return run.fort7.c_str();
  case 8: return run.fort8.c_str();
  case 9: return run.fort9.c_str();
  default: return "";
  }
}

/* what a run continues, for autox_stability.h */
static AutoxStabilityKind run_stability_kind(xpp::Session &s)
{
  if(s.autox_state.two_param!=no_calculation||s.autox_state.bifur.isw==2)return AUTOX_STABILITY_OTHER;
  if(s.autox_state.bifur.ips==xpp::AutoxProblem::Periodic)return AUTOX_STABILITY_PERIODIC;
  if(xpp::problem_is_steady_or_map(s.autox_state.bifur.ips))return AUTOX_STABILITY_STEADY;
  return AUTOX_STABILITY_OTHER;
}

/* what a stored point is, for autox_stability.h: one-parameter periodic
   orbits have a negative branch (autox_lib1.cpp stplbv) */
static AutoxStabilityKind point_stability_kind(const DIAGRAM *d)
{
  if(d->flag2!=no_calculation)return AUTOX_STABILITY_OTHER;
  return d->ibr<0?AUTOX_STABILITY_PERIODIC:AUTOX_STABILITY_STEADY;
}

/* a run starts from Auto.irs's label, or from initial data: its first
   point takes the label's stored stability when it is the label's own
   solution, else it is not computed (autox_stability.h) */
static void stability_run_start(xpp::Session &s, AutoxRun &run)
{
  const DIAGRAM *d=s.autox_state.bifur.irs>0?diagram_of_label(s,s.autox_state.bifur.irs):NULL;
  if(d==NULL){
    autox_stability_run_start(run.stability,run_stability_kind(s),s.autox_state.bifur.isw,AUTOX_STABILITY_NONE,AutoxPointCode{},{},{});
    return;
  }
  autox_stability_run_start(run.stability,run_stability_kind(s),s.autox_state.bifur.isw,point_stability_kind(d),d->itp,d->evr,d->evi);
}

static std::string autox_solution_key(long branch, long point, long type, long label)
{
  return xpp::format("{:5} {:4} {:4} {:4}",branch,point,type,label);
}

std::optional<std::pair<long,std::size_t>> autox_integer_at(std::string_view line, std::size_t at)
{
  while(at<line.size()&&line[at]==' ')at++;
  long value=0;
  const char *begin=line.data()+at,*limit=line.data()+line.size();
  const auto [stop,error]=std::from_chars(begin,limit,value);
  if(error!=std::errc()||stop==begin||(stop!=limit&&*stop!=' '&&*stop!=','))return std::nullopt; /* a note's "{:4} {:5}, NOTE:" has a comma */
  return std::pair<long,std::size_t>(value,static_cast<std::size_t>(stop-line.data()));
}

std::string autox_solution_header_text(const AutoxSolutionHeader &h)
{
  return xpp::format("{} {:4} {:4} {:4} {:4} {:4} {:4} {:4} {:4}\n",autox_solution_key(h.ibr,h.ntot,autox_point_encode(h.itp),h.lab),
                     h.nfpr,h.isw,h.ntpl,h.nar,h.nskip,h.ntst,h.ncol,h.npar);
}

std::string autox_point_row_key(long branch, long point, std::string_view mark, long label)
{
  return xpp::format("{:4} {:5}  {:<4} {:4}",branch,point,mark,label);
}

namespace {
/* a type mark autoX writes (autox_point_row_key's): none, or one of the table's */
bool is_point_mark(std::string_view text)
{
  const std::size_t last=text.find_last_not_of(' ');
  if(last==std::string_view::npos)return true;
  const std::string_view mark=text.substr(0,last+1);
  return std::any_of(autox_point_names.begin(),autox_point_names.end(),[&](const AutoxPointName &p){return mark==p.mark;});
}
}

std::string autox_renumbered_line(std::string_view line, const std::function<int(int)> &branch, const std::function<int(int)> &label)
{
  constexpr std::string_view iteration=" Branch ";
  if(line.substr(0,iteration.size())==iteration){ /* " Branch {:2} N={:5} ..." */
    const auto number=autox_integer_at(line,iteration.size());
    if(!number)return std::string(line);
    return xpp::format("{}{:2}{}",iteration,branch(static_cast<int>(number->first)),line.substr(number->second));
  }
  /* the stop lines and notes: "Branch 2 stopped ...", "Br 2 Pt 30: ...", "Warning: Br 2 Pt 30: ..." */
  for(const std::string_view prefix:{std::string_view("Branch "),std::string_view("Br "),std::string_view("Warning: Br "),
                                      std::string_view("autoX: Br "),std::string_view("autoX: branch ")})
    if(line.substr(0,prefix.size())==prefix)
      if(const auto number=autox_integer_at(line,prefix.size()))
        return xpp::format("{}{}{}",prefix,branch(static_cast<int>(number->first)),line.substr(number->second));
  const auto first=autox_integer_at(line,0);
  if(!first)return std::string(line);
  const auto point=autox_integer_at(line,first->second);
  if(!point||line[first->second]!=' ')return std::string(line);
  const long moved=branch(static_cast<int>(first->first));
  /* a point's row: two blanks, the mark in four columns, a blank, the label (autox_point_row_key) */
  constexpr std::size_t before_mark=2,mark_width=4;
  const std::size_t mark_at=point->second+before_mark,label_at=mark_at+mark_width+1;
  if(line.size()>label_at&&line.substr(point->second,before_mark)=="  "&&is_point_mark(line.substr(mark_at,mark_width))&&line[label_at-1]==' ')
    if(const auto number=autox_integer_at(line,label_at))
      return autox_point_row_key(moved,point->first,line.substr(mark_at,mark_width).substr(0,line.substr(mark_at,mark_width).find_last_not_of(' ')+1),
                                 label(static_cast<int>(number->first)))+std::string(line.substr(number->second));
  /* any other line that begins with a branch and a point: the branch alone, in its four columns */
  return xpp::format("{:4}",moved)+std::string(line.substr(first->second));
}

std::string autox_branch_row_key(long branch, long point, long type, long label)
{
  return xpp::format("{:4} {:5} {:3} {:3}",branch,point,type,label);
}

AutoxDepth::AutoxDepth(Session &s) : depth(s.autox_state.depth) { if(depth++==0)autox_stop_clear(s.autox_state.stop); } /* xppautX: T23: why this run's branches end */

std::optional<xpp::Error> autox_numerics(AutoxRun &run)
{
  try {
    go_go_autox(run); /* this complets the initialization and calls the
                         main routines
                      */
  } catch (const xpp::AutoxFailed &e) {
    /* W63a: what the numerics used to exit() on ends the run
       instead, as a cancel does (autox_state.h) */
    return e.error;
  } catch (const std::length_error &e) {
    return xpp::Error{"autoX",e.what(),xpp::command_place()};
  } catch (const std::bad_alloc &) {
    return xpp::Error{"autoX","Cannot allocate autoX workspace",xpp::command_place()};
  }
  return std::nullopt;
}

Error autox_shown_failure(Error failed)
{
  failed.what="autoX stopped: "+failed.what;
  xpp::log_autox("{}\n",failed.text()); /* the autoX window's Output */
  if(failed.place.file.empty() && failed.place.line==0)failed.place=command_place();
  return failed;
}

std::optional<xpp::Error> autox_end_run(xpp::Session &s, AutoxRun &run, std::optional<xpp::Error> failed, int isave)
{
  std::optional<xpp::Error> reported; /* what the run says went wrong, for the command to return */
  xpp::autox_stop_merge(s.autox_state.stop,std::move(run.stop)); /* what the Session shows of why branches ended */
  close_autox(s, run, isave); /* this copies fort.8 to the .s file and other 
                        irrelevant stuff 
		     */
  if(failed.has_value())reported=autox_shown_failure(std::move(*failed));
  if(run.jacobian_failure){
    xpp::Error e=std::move(*run.jacobian_failure);
    e.what="autoX stopped: "+e.what;
    /* the run stored the last converged point as an end point (the
       cancel it made itself: autox_pp.cpp): the continuation goes on from
       its label, with the settings of this run */
    const DIAGRAM *last=s.autox_state.stop.last_why==xpp::AUTOX_STOP_JACOBIAN?last_diagram(s.diagram):NULL;
    if(last!=NULL&&last->lab>0)
      e.action=xpp::switch_to_differences(xpp::format(R"({{"cmd":"autox.run","kind":"{}","label":{}}})",xpp::autox_run_kind_name(s.autox_state.run_kind),last->lab));
    xpp::log_autox("{}\n",e.what);
    if(!reported)reported=std::move(e);
  }
  return reported;
}

Result<> autox_set_up_run(Session &s, AutoxRun &run, int iold)
{
  run.session=&s;
  init_autox(s.autox_state,run.params,s.model().node,s.numerics.jacobian==JACOBIAN_EXACT); /* the continuation's initialization from the settings: what reading in AUTO's parameters and running its init did */
  if(auto opened=open_autox(s,run,iold);!opened)return opened; /* the run's folder; this copies the relevant files .s  to fort.3 */
  run.run_from=s.autox_state.bifur.irs>0?s.autox_state.bifur.irs:0; /* the diagram's data say where the run started */
  stability_run_start(s,run); /* what its first point's stability is (autox_stability.h) */
  return {};
}

xpp::Result<> autox_run_prologue(xpp::Session &s, int &isave)
{
    // A model parameter must exist before run setup selects its solver index.
    if(s.model().nupar==0)
      return xpp::fail("autox", "autoX continuation needs a model parameter", command_place());
    /* Retaining an older branch retains its label solutions too. A fresh
       calculation may request overwrite only after the diagram is reset. */
    if(s.autox_state.diag_flag && isave==OVERWRITE)isave=APPEND;
    if(const DIAGRAM *point=diagram_of_label(s,s.autox_state.bifur.irs))
      autox_set_pars_from(s,diagram_parameters(*point).data());
    return {};
}

/* MAIN Running routine  Assumes that Auto structure is set up */
xpp::Result<> do_autox(xpp::Session &s, int iold, int isave, xpp::AutoxPointCode itp)
{
    if(auto prepared=autox_run_prologue(s,isave);!prepared)return prepared;

    const AutoxDepth depth(s);
    std::optional<xpp::Error> reported; /* what the run says went wrong, for the command to return */
    int restart_label=0; /* the label a follow-up run goes on from, 0 for none */
    { /* the run lives in here: its memory and folder are gone before a follow-up run starts */
    AutoxRun run; /* this run's own state: nothing of it is the Session's but the model and settings it reads */
    create_autox_file_name(s); /* where the run's solutions are kept: the dir may have changed since the last run */
    if(auto set_up=autox_set_up_run(s,run,iold);!set_up)return set_up;
    std::optional<xpp::Error> failed; /* the run reports its failure after cleanup */
    {
      const xpp::Job job; /* Abort cancels it (xpp_job.h) */
      {
          xpp::Computation computing; /* what Abort stops (xpp_job.h) */
          std::vector<double> before(s.model().nupar);
          for (int i = 0; i < s.model().nupar; i++) before[i] = s.parser.constants[s.model().upar_con[i]];
          failed=autox_numerics(run);
          autox_restore_finite_pars(s, before.data()); /* leave no NaN parameter behind (QA SCI-001) */
      }
      if(xpp::job::cancelled()||failed.has_value())run.restart_label=0; /* xppautX: cancel: no follow-up run */
    }
    reported=autox_end_run(s,run,std::move(failed),isave);
    restart_label=run.restart_label;
    }
    
    if(restart_label!=0){
      xpp::log_autox("RestartLabel={:d} itp={} ips={:d} nfpar={:d} ilp={:d} isw={:d} isp={:d} A2p={} \n",restart_label,xpp::autox_point_text(s.autox_state.bifur.itp), xpp::autox_problem_code(s.autox_state.bifur.ips),s.autox_state.bifur.nfpar,s.autox_state.bifur.ilp,s.autox_state.bifur.isw,s.autox_state.bifur.isp,autox_calculation_name(s.autox_state.two_param));
      s.autox_state.bifur.irs=restart_label;
      const xpp::Result<> again=do_autox(s, iold,isave, s.autox_state.bifur.itp);
      if(!again&&!reported)reported=again.error();
    }
     ping();
      redraw_params();
    if(reported)return std::unexpected(std::move(*reported));
    return {};
}

void autox_label_parameters(const AutoxRun &run, int label, double *parameters)
{
  const xpp::Session &s=*run.session;
  const DIAGRAM *point=diagram_of_label(s,label);
  if(!point)return; /* a library-generated starting solution may have no stored point */
  for(int k=0;k<run.params.nicp;k++){
    const int slot=run.params.icp[k];
    const int parameter=s.autox_state.parameter_layout.model_index(slot);
    if(parameter>=0 &&
       std::find(point->parameter_indices.begin(),point->parameter_indices.end(),parameter)==point->parameter_indices.end())
      parameters[slot]=diagram_parameter(*point,parameter);
  }
}

const char *autox_par_name(const xpp::Session &s, int k)
{
  return k>=0&&k<s.model().nupar?s.model().upar_names[k].c_str():NULL;
}

namespace {
/* autoX's parameter index's name, T for the period; empty for no such
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

/* autoX heads its printed columns PAR(n) and U(n); XPP knows what the user
   called them, and already uses those names for the diagram's axes. This
   rewrites one 14-character column heading for the screen only: fort.7 and
   fort.9 keep autoX's own format, which its restart path and other people's
   scripts read. PAR(10) and friends are the period and such, not the user's
   parameters, and par_or_period_name leaves them alone. A name that does not
   fit (a name may be any length) is shortened with a '~' (short_name) and
   still leaves a blank between it and the next heading: the column stays
   14 wide so the numbers below stay under it. */
std::string col_centre(const std::string &s)
{
  const std::string t=xpp::short_name(s,autox_column_width-1);
  const int n=static_cast<int>(t.size());
  const int l=(autox_column_width-n)/2;
  return std::string(static_cast<size_t>(l),' ')+t+std::string(static_cast<size_t>(autox_column_width-n-l),' ');
}

std::string screen_col(const xpp::Session &s, const char *col)
{
  long p;
  const char *c=col;
  while(isspace(static_cast<unsigned char>(*c)))c++;
  if(strncmp(c,"PAR(",4)==0&&read_long(c+4,p)){
    const std::string name=par_or_period_name(s, s.autox_state.parameter_layout.model_index(static_cast<int>(p)), p==xpp::autox_unknown::period);
    if(!name.empty())return col_centre(name);
  }
  /* U(n), and the MAX(n) / MIN(n) a periodic branch prints, where autoX has
     overwritten the U itself */
  const char *q=strchr(col,'(');
  if(q!=NULL&&strstr(col,"PAR")==NULL&&read_long(q+1,p)&&p>=1&&p<=s.model().node){
    size_t n=static_cast<size_t>(q-col);
    if(n>0&&col[n-1]=='U')n--; /* the name replaces the U */
    /* keep what stands in front of it: MAX, MIN, L2-NORM, INTEGRAL */
    std::string pre(col,std::min<size_t>(n,autox_column_width));
    const size_t a=pre.find_first_not_of(' '),b=pre.find_last_not_of(' ');
    pre=a==std::string::npos?std::string():pre.substr(a,b-a+1);
    return col_centre(pre+(pre.empty()?"":" ")+s.model().uvar_names[p-1]);
  }
  return std::string(std::string_view(col).substr(0,autox_column_width));
}
} // namespace

std::string autox_screen_col(const xpp::Session &s, const std::string &col)
{
  return screen_col(s, col.c_str());
}



/* Numerics field i as a typed form field of its shape (the value as the settings hold it) */






void autox_default(xpp::Session &s)
{
  s.autox_state.axes().xmin=s.autox_state.options.xmin;
  s.autox_state.axes().xmax=s.autox_state.options.xmax;
  s.autox_state.axes().ymin=s.autox_state.options.ymin;
  s.autox_state.axes().ymax=s.autox_state.options.ymax;
}

void autox_fit(xpp::Session &s)
{
  double xlo=s.autox_state.axes().xmin,xhi=s.autox_state.axes().xmax,ylo=s.autox_state.axes().ymin,yhi=s.autox_state.axes().ymax;
  bound_diagram(s, &xlo,&xhi,&ylo,&yhi);
  /* a flat quantity (a steady branch's period, all 0) is widened as the
     plot window's Fit does: an empty range divides by zero in IXVal/IYVal */
  double mid,span;
  check_val(&xlo,&xhi,&mid,&span);
  check_val(&ylo,&yhi,&mid,&span);
  s.autox_state.axes().xmin=xlo;
  s.autox_state.axes().xmax=xhi;
  s.autox_state.axes().ymin=ylo;
  s.autox_state.axes().ymax=yhi;
}
  
void autox_hide_earlier(xpp::Session &s)
{
  s.autox_view.earlier=autox_points_held(s);
  s.autox_view.show_earlier=false;
  draw_bif_axes(s);
}

void autox_zoom_to(xpp::Session &s, double x1, double x2, double y1, double y2)
{
  s.autox_state.axes().xmin=x1;
  s.autox_state.axes().ymin=y1;
  s.autox_state.axes().xmax=x2;
  s.autox_state.axes().ymax=y2;
}

void autox_scroll_by(xpp::Session &s, double dx, double dy)
{
  s.autox_state.axes().xmin+=dx;
  s.autox_state.axes().xmax+=dx;
  s.autox_state.axes().ymin+=dy;
  s.autox_state.axes().ymax+=dy;
}

/* the view shows what it did, scaled so that what it showed now fills the fractions (a1, a2) of its width and (b1, b2) of its
   height (the box a zoom out shrinks it into, as fractions of the range shown) */
static void autox_zoom_out_fractions(xpp::Session &s, double a1, double a2, double b1, double b2)
{
  const double xmin=s.autox_state.axes().xmin,xmax=s.autox_state.axes().xmax,ymin=s.autox_state.axes().ymin,ymax=s.autox_state.axes().ymax;
  const double x1=(a1*xmax-a2*xmin)/(a1-a2);
  const double x2=(xmin-xmax+a1*xmax-a2*xmin)/(a1-a2);
  const double y1=(b1*ymax-b2*ymin)/(b1-b2);
  const double y2=(ymin-ymax+b1*ymax-b2*ymin)/(b1-b2);
  autox_zoom_to(s,x1,x2,y1,y2);
}

void autox_zoom_out_to(xpp::Session &s, double x1, double x2, double y1, double y2)
{
  const double dx=s.autox_state.axes().xmax-s.autox_state.axes().xmin,dy=s.autox_state.axes().ymax-s.autox_state.axes().ymin;
  autox_zoom_out_fractions(s,(x1-s.autox_state.axes().xmin)/dx,(x2-s.autox_state.axes().xmin)/dx,(y1-s.autox_state.axes().ymin)/dy,(y2-s.autox_state.axes().ymin)/dy);
}

void autox_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> ubar, double a)
{
 /* a plot type none of the cases know leaves the point at (par1, 0) */
 *x=par1;
 *y1=*y2=0.0;
 switch(ax->plot){
  case AutoxProjection::maximum:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=*y1;
    break;
  case AutoxProjection::norm:
    *x=par1;
    *y1=a;
    *y2=*y1;
    break;
  case AutoxProjection::maximum_and_minimum:
    *x=par1;
    *y1=uhigh[ax->var];
    *y2=ulow[ax->var];
    break;
  case AutoxProjection::average:
    *x=par1;
    *y1=ubar[ax->var];
    *y2=*y1;
    break;
  case AutoxProjection::period:
    *x=par1;
    *y1=per;
    *y2=*y1;
    break;
  case AutoxProjection::frequency:
    *x=par1;
    if(per>0)*y1=1./per;
    else *y1=0.0;
    *y2=*y1;
    break;
  case AutoxProjection::two_parameter:
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
  const xpp::AutoxAxis icp1=d.icp1,icp2=d.icp2;
  const AutoxCalculation flag2=d.flag2;
  double x,y1,y2,par1,par2=0;
  DiagramPointType type1=type;
  par1=par[icp1.index];
  if(icp2.is_parameter())par2=par[icp2.index];
  autox_xy_plot(&s.autox_state.axes(),&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    s.autox_state.bifur.lastx=x;
    s.autox_state.bifur.lasty=y1;
  }
  if(flag2==no_calculation&&s.autox_state.axes().plot==AutoxProjection::two_parameter)
    {
  
       return;
     }
  if(flag2!=no_calculation&&s.autox_state.axes().plot!=AutoxProjection::two_parameter){
  
    return;
  }

  if((flag2!=no_calculation)&&(s.autox_state.axes().plot==AutoxProjection::two_parameter))
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
    if(s.autox_state.axes().plot==AutoxProjection::period||s.autox_state.axes().plot==AutoxProjection::frequency)break;
    if(!icp1.is_parameter(s.autox_state.axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.autox_state.axes().icp2))break;
    if(type1==UNSTABLE_EQ){
      line_style.width=1;
      line_style.dash=xpp::LineDash::dashed;
    }
    xpp::set_pen(s,color_of(type1==STABLE_EQ?s.autox_state.stable_eq_color:s.autox_state.unstable_eq_color),line_style);
    line_abs(s,static_cast<float>(x),static_cast<float>(y1),static_cast<float>(s.autox_state.bifur.lastx),static_cast<float>(s.autox_state.bifur.lasty));
    break;
  case UNSTABLE_PERIODIC:
  case STABLE_PERIODIC:
    if(!icp1.is_parameter(s.autox_state.axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.autox_state.axes().icp2))break;
    /* a stable periodic orbit's extremes are filled circles, an unstable one's open */
    xpp::set_pen(s,type1==STABLE_PERIODIC?s.autox_state.stable_po_color:s.autox_state.unstable_po_color,dots);
    s.plot_file.pen.hollow=type1==UNSTABLE_PERIODIC;
    point_abs(s,static_cast<float>(x),static_cast<float>(y1));
    point_abs(s,static_cast<float>(x),static_cast<float>(y2));
    break;
  }

  s.autox_state.bifur.lastx=x;
  s.autox_state.bifur.lasty=y1;
}

int autox_run_from_take(AutoxRun &run)
{
  int f=run.run_from;
  run.run_from=0;
  return f;
}

/* the stability circle as data (autox_data.h): a stored point's values
   (autox_stability.h), whether autoX is computing it or a grab is on it */
static void show_stab(xpp::Session &s, std::span<const double> evr,std::span<const double> evi,int periodic)
{
  autox_data_stab(s,evr,evi,periodic);
}

/* the colour a point is drawn in: its kind of two-parameter curve's, or its stability's */
static xpp::Color autox_point_color(xpp::Session &s, DiagramPointType type,AutoxCalculation flag2)
{
  if(flag2!=no_calculation)return two_parameter_color(flag2);
  switch(type){
  case STABLE_EQ: return s.autox_state.stable_eq_color;
  case UNSTABLE_EQ: return s.autox_state.unstable_eq_color;
  case STABLE_PERIODIC: return s.autox_state.stable_po_color;
  case UNSTABLE_PERIODIC: return s.autox_state.unstable_po_color;
  }
  return xpp::foreground_color;
}

/* this bit of code is for writing points - it only saves what is
   in the current view

*/
int check_plot_type(const xpp::Session &s, AutoxCalculation flag2, xpp::AutoxAxis icp1, xpp::AutoxAxis icp2)
{
  if(flag2==no_calculation && s.autox_state.axes().plot==AutoxProjection::two_parameter)
    return 0;
  if(flag2!=no_calculation  && s.autox_state.axes().plot!=AutoxProjection::two_parameter)
    return 0; 
  if(!icp1.is_parameter(s.autox_state.axes().icp1))
    return 0;
  if(flag2!=no_calculation && !icp2.is_parameter(s.autox_state.axes().icp2))
    return 0;
  return 1;

} 
namespace {

/* the point add_point() is given, in view v (axes ax): its data
   (autox_diagram) and, in the active view, its drawing. A point the view
   does not plot (a one-parameter point in a two-parameter view, a
   two-parameter point in any other) goes to the data too, with no
   coordinates, so every view holds one entry per point of the diagram in
   the same order: a point's index is the same in every view (the grab,
   Clear's earlier branches). */
void view_point(xpp::Session &s, int v, const AUTOAX &ax, bool active, const DIAGRAM &d, DiagramPointType type,
                int flg)
{
  const int lab=d.lab;
  const AutoxCalculation flag2=d.flag2;
  const xpp::AutoxAxis icp1=d.icp1,icp2=d.icp2;
  double x,y1,y2,par1,par2=0;
  DiagramPointType type1=type;
  XppDiagPoint dp;
  par1=diagram_axis_value(d,icp1);
  if(icp2.is_parameter())par2=diagram_axis_value(d,icp2);
  autox_xy_plot(&ax,&x,&y1,&y2,par1,par2,d.per,d.uhi,d.ulo,d.ubar,d.norm); /* figure out who sits on axes */
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
  dp.color=autox_point_color(s, type,flag2);
  dp.lw=(type==STABLE_EQ||flag2!=no_calculation)?2:1;
  dp.x=x;
  dp.y1=y1;
  dp.y2=y2;
  if(active&&flg==0){
    s.autox_state.bifur.lastx=x;
    s.autox_state.bifur.lasty=y1;
  }
  if((flag2==no_calculation&&ax.plot==AutoxProjection::two_parameter)||(flag2!=no_calculation&&ax.plot!=AutoxProjection::two_parameter)){
    dp.x=dp.y1=dp.y2=std::numeric_limits<double>::quiet_NaN();
    autox_diagram(s,v,&dp);
    return;
  }
  if(flag2!=no_calculation) /* a two-parameter point in a two-parameter view */
    type1=STABLE_EQ;
  switch(type1){
  case STABLE_EQ:
  case UNSTABLE_EQ:
    if(ax.plot==AutoxProjection::period||ax.plot==AutoxProjection::frequency)break;
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
    s.autox_state.bifur.lastx=x;
    s.autox_state.bifur.lasty=y1;
  }
  autox_diagram(s,v,&dp);
}

} // namespace

/* main plotting code: the point in every view */
void add_point(xpp::Session &s, const DIAGRAM &d, DiagramPointType type, int flg)
{
  for(int v=0;v<static_cast<int>(s.autox_state.views.size());v++)
    view_point(s, v,s.autox_state.views[static_cast<std::size_t>(v)].axes,v==s.autox_state.active_view,d,type,flg);
  show_stab(s, d.evr,d.evi,type==STABLE_PERIODIC||type==UNSTABLE_PERIODIC);
  refreshdisplay(s);
}

void traverse_out(xpp::Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw)
{
  double norm,per,*par,par1,par2=0;
  int pt,ibr,lab;
  AutoxCalculation flag2;
  xpp::AutoxAxis icp1,icp2;
  AutoxPointCode itp;
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

  symb=autox_point_kind_name(itp.kind).data();
 par1=par[icp1.index];
  if(icp2.is_parameter())par2=par[icp2.index];
    autox_xy_plot(&s.autox_state.axes(),&x,&y1,&y2,par1,par2,per,d->uhi,d->ulo,d->ubar,norm);
  
    *ix=IXVal(s, x);
    *iy=IYVal(s, y1);
    if (dodraw==1)
    {
      AutoxDataInfo ai;
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
      ai.vname=s.model().uvar_names[s.autox_state.axes().var].c_str();
      ai.u=d->u0[s.autox_state.axes().var];
      ai.per=per;
      ai.x=x;
      ai.y=y1;
      ai.y2=y2;
      /* the point and its circle together: a flush between them sent the new
         circle with the old point when a step took over 0.1 s (autoxcheck
         under valgrind, W21) */
      autox_data_info(s,&ai);
      show_stab(s, d->evr,d->evi,ibr<0);
    }
    if(lab>0 && s.autox_state.load_all_labeled_orbits!=LabeledOrbits::Off)
      xpp::ok_or_show(load_autox_orbitx(s,ibr,1,lab,per));

}

void do_autox_win(xpp::Session &s)
{
  if(s.autox_state.bifur.exist==0){
    s.autox_state.bifur.exist=1;
    for(int i=0;i<static_cast<int>(s.autox_state.views.size());++i){
      auto &view=s.autox_state.views[i];
      const auto existing=find_window(s,view.name);
      if(existing && (existing->kind!=WindowKind::diagram || existing->index!=i))
        view.name=next_window_name(s,WindowKind::diagram);
    }
    s.selected_window=s.autox_state.views[s.autox_state.active_view].name;
    make_autox(s,s.selected_window,"autoX");
    
  }

}

void load_last_plot(xpp::Session &s, LastPlot which)
{
  s.autox_state.axes()=which==LastPlot::OneParameter?s.autox_state.old1p:s.autox_state.old2p;
}
void keep_last_plot(xpp::Session &s, LastPlot which)
{
  if(which==LastPlot::OneParameter)s.autox_state.old1p=s.autox_state.axes();
  else{
    s.autox_state.old2p=s.autox_state.axes();
    s.autox_state.old2p.plot=AutoxProjection::two_parameter;
  }
}
void keep_axes_as_last(xpp::Session &s)
{
  switch(s.autox_state.axes().plot){
  case AutoxProjection::maximum:
  case AutoxProjection::norm:
  case AutoxProjection::maximum_and_minimum:
  case AutoxProjection::period:
    keep_last_plot(s, LastPlot::OneParameter);
    break;
  case AutoxProjection::two_parameter:
    keep_last_plot(s, LastPlot::TwoParameter);
    break;
  case AutoxProjection::frequency:
  case AutoxProjection::average:
    break; /* XPPAUT keeps neither */
  }
}

void autox_new_view(xpp::Session &s)
{
  AutoxDiagramView v;
  v.axes=s.autox_state.axes();
  v.name=xpp::next_window_name(s,xpp::WindowKind::diagram);
  s.autox_state.views.push_back(v);
  s.autox_state.active_view=static_cast<int>(s.autox_state.views.size())-1;
  s.selected_window=s.autox_state.views.back().name;
  if(s.autox_state.bifur.exist)redraw_diagram(s);
}

int autox_close_view(xpp::Session &s, int k)
{
  const int n=static_cast<int>(s.autox_state.views.size());
  if(k<0||k>=n||n<2)return 0;
  s.autox_state.views.erase(s.autox_state.views.begin()+k);
  /* the active one keeps its view; closed, the one in its place (or the
     last) takes over */
  if(s.autox_state.active_view>k)s.autox_state.active_view--;
  else if(s.autox_state.active_view==k)s.autox_state.active_view=std::min(k,n-2);
  if(s.autox_state.bifur.exist)redraw_diagram(s);
  return 1;
}

int autox_activate_view(xpp::Session &s, int k)
{
  if(k<0||k>=static_cast<int>(s.autox_state.views.size()))return 0;
  s.autox_state.active_view=k;
  s.selected_window=s.autox_state.views[k].name;
  return 1;
}

void init_autox_win(xpp::Session &s)
{
  s.autox_state.bifur.marks.clear();
  s.autox_state.parameter_layout.model_parameters=s.model().nupar;
  s.autox_state.parameter_layout.block=std::max(xpp::autox_unknown::block_size,
      xpp::autox_unknown::homoclinic_start+xpp::autox_unknown::homoclinic_blocks*s.model().node);
  for (auto *v : {&s.autox_state.grabpt.uhi, &s.autox_state.grabpt.ulo, &s.autox_state.grabpt.u0,
                 &s.autox_state.grabpt.ubar, &s.autox_state.homo_l, &s.autox_state.homo_r}) v->resize(s.model().node);
  start_diagram(s, s.model().node);
  s.autox_state.grabpt.flag=0;  /*  no point in buffer  */
  s.autox_state.bifur.exist=0;
 s.autox_state.blrtn.torper=s.numerics.torus_period;
 create_autox_file_name(s);
 
/*  Control -- done automatically   */
  s.autox_state.bifur.irs=0;
  s.autox_state.bifur.ips=xpp::AutoxProblem::SteadyState;
  s.autox_state.bifur.isp=1;
    if(s.autox_state.suppress_bp) s.autox_state.bifur.isp=0;
  s.autox_state.bifur.ilp=1;
  s.autox_state.bifur.isw=1;
  s.autox_state.bifur.nbc=s.model().node;
  s.autox_state.bifur.nfpar=1;
  s.autox_state.homo_flag=HomoclinicRun::None;
/*  User controls this      */
  s.autox_state.bifur.ncol=s.autox_state.options.ncol;
  s.autox_state.bifur.ntst=s.autox_state.options.ntst;
  s.autox_state.bifur.nmx=s.autox_state.options.nmx;
  s.autox_state.bifur.npr=s.autox_state.options.npr;
  s.autox_state.bifur.ds=s.autox_state.options.ds;
  s.autox_state.bifur.dsmax=s.autox_state.options.dsmax;
  s.autox_state.bifur.dsmin=s.autox_state.options.dsmin;
  s.autox_state.bifur.rl0=s.autox_state.options.rl0;
  s.autox_state.bifur.rl1=s.autox_state.options.rl1;
  s.autox_state.bifur.a0=s.autox_state.options.a0;
  s.autox_state.bifur.a1=s.autox_state.options.a1;
  
  s.autox_state.bifur.epsl=s.autox_state.options.epsl;
    s.autox_state.bifur.epsu=s.autox_state.options.epsu;
  s.autox_state.bifur.epss=s.autox_state.options.epss;

/* The diagram plotting stuff: one view (W50)    */
  s.autox_state.views.assign(1,AutoxDiagramView{});
  s.autox_state.active_view=0;
  s.autox_state.axes().xmax=s.autox_state.options.xmax;
  s.autox_state.axes().xmin=s.autox_state.options.xmin;
  s.autox_state.axes().ymax=s.autox_state.options.ymax;
  s.autox_state.axes().ymin=s.autox_state.options.ymin;
  s.autox_state.axes().plot=AutoxProjection::maximum_and_minimum;
  s.autox_state.axes().var=s.autox_state.options.var;

/* xpp parameters    */
  
  s.autox_state.axes().icp1=0;
  s.autox_state.axes().icp2=1;
   s.autox_state.bifur.icp3=1;
  s.autox_state.bifur.icp4=1;
  s.autox_state.bifur.icp5=1;
  keep_last_plot(s, LastPlot::OneParameter);
  keep_last_plot(s, LastPlot::TwoParameter);
  s.autox_state.advanced={};
  s.autox_state.homo_equilibria.nunstab=1;
  s.autox_state.homo_equilibria.nstab=s.model().node-1;
}

/* Clear selections and obsolete run files when a diagram is replaced. An import
   has already committed its new solutions, so keeps those bytes. */
static void forget_autox_diagram(xpp::Session &s, bool remove_solutions)
{
  s.autox_state.from_autox_flag=false;
  s.autox_state.grabpt.flag=0;
  s.autox_state.diagram_mark.state=0;
  xpp::files::remove(s.autox_state.file+".b");
  xpp::files::remove(s.autox_state.file+".d");
  if(remove_solutions)xpp::files::remove(s.autox_state.file+".s");
}

int yes_reset_autox(xpp::Session &s)
{
  if(diagram_count(s.diagram)<=1)return(0);
 start_diagram(s,s.model().node);
 forget_autox_diagram(s,true);
    return 1;
}

void autox_grab(xpp::Session &s)
{
  traverse_diagram(s);
} 

void get_start_period(xpp::Session &s, double *p)
{
 *p=s.autox_state.bifur.ips==xpp::AutoxProblem::Periodic?s.autox_state.start_return.period:static_cast<double>(s.data_store.col[0][s.data_store.rows-1]);
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
      d+=fabs(s.data_store.col[j+1][i]-s.autox_state.homo_l[j]);
    }
    if(d<dmin){
      dmin=d;
      tshift=s.data_store.col[0][i];
    }
  }
  s.autox_state.homo_shift=tshift;
  xpp::log_autox("shifting {:g}\n",s.autox_state.homo_shift);
}
void get_start_orbit(xpp::Session &s, double *u, double t, double p, int n)
/* the orbit at the fraction t of its period, interpolated between the stored rows: of the rows up to the first return when
   the start is periodic, all of them otherwise (rows on an even time grid, as XPPAUT takes them) */
{
  double tnorm,lam;
  int i1,i2,j;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  tnorm=t*(s.autox_state.bifur.ips==xpp::AutoxProblem::Periodic?s.autox_state.start_return.index:static_cast<double>(s.data_store.rows-1));
  i1=static_cast<int>(tnorm);
  i2=i1+1;
  if(i2>=s.data_store.rows)i2-=s.data_store.rows;
  lam=(tnorm-static_cast<double>(i1));

   for(j=0;j<n;j++)
    u[j]=(1.0-lam)*s.data_store.col[j+1][i1]+lam*s.data_store.col[j+1][i2];
}
  

xpp::Result<> load_autox_orbit(xpp::Session &s, std::optional<int> label)
{
  if(!label)return load_autox_orbitx(s,s.autox_state.grabpt.ibr,s.autox_state.grabpt.flag,s.autox_state.grabpt.lab,s.autox_state.grabpt.per);
  const DIAGRAM *d=diagram_of_label(s,*label);
  if(!d)return xpp::fail("autox",xpp::format("No point labelled {}",*label),command_place());
  return load_autox_orbitx(s,d->ibr,1,*label,d->per);
}
xpp::Result<> load_autox_orbitx(xpp::Session &s, int ibr,int flag, int lab, double per)
{
  double *x;
  int i,j,nstor;
  std::vector<double> u(s.model().node);
  double t;
  double period;
  std::string string;
  int nrow,ndim,label,flg;

  if((ibr>0&&(s.autox_state.bifur.ips!=xpp::AutoxProblem::BoundaryValue)&&(s.autox_state.bifur.ips!=xpp::AutoxProblem::Homoclinic))||flag==0)return {};
   /* either nothing grabbed or just a fixed point and that is already loaded */
  string=s.autox_state.file+".s";
  xpp::UniqueFile fp=xpp::open_read(string.c_str());
  if(!fp){
    return xpp::fail_reading("autoX","cannot be read",string);
  }
  label=lab;
  period=per;
  flg=move_to_label(s,label,&nrow,&ndim,fp.get());
  if(flg==0){
    xpp::log_autox("Could not find label {:d} in file {} \n",label,string.c_str());
    return xpp::fail("autoX",xpp::format("No point labelled {} in {}",label,string),command_place());
  }
  if(nrow<1||ndim<1||ndim>s.model().node)
    return xpp::fail_reading("autoX","invalid orbit dimensions",string);
  nstor=std::min(ndim,s.model().node);
  if(nrow>s.data_store.max_rows){
    if(const xpp::Result<> grown=s.data_store.grow(s.model().neq+1,nrow);!grown)return grown;
    s.data_store.max_rows=nrow;
  }
  x=&s.data_store.current[0];
  for(i=0;i<nrow;i++){
    get_a_row(u.data(),&t,ndim,fp.get());
    if(s.autox_state.bifur.ips!=xpp::AutoxProblem::BoundaryValue) 
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
  if(s.autox_state.load_all_labeled_orbits==LabeledOrbits::ClearAndDraw)clr_all_scrns(s);
  drw_all_scrns(s);
  return {};
}

// Foreign PAR numbering is translated here; unselected prefix entries are not model values.
static int imported_solver_index(const xpp::Session &s, std::span<const int> imported, int slot)
{
  return slot>=0 && static_cast<std::size_t>(slot)<imported.size()
    ? s.autox_state.parameter_layout.solver_index(imported[slot]) : slot;
}

static xpp::Result<std::string> convert_autox_solutions(const xpp::Session &s, std::span<const int> imported,
    std::string_view text, const std::string &file)
{
  /* Validate the foreign restart payload before translating its selected slots. */
  const auto checked=xpp::check_autox_solutions(text,file,AutoxState::state_copies*s.model().node,s.autox_state.parameter_layout.block,true);
  if(!checked)return std::unexpected(checked.error());
  const auto converted=xpp::read_lines("import autoX solutions",file,text,[&s,imported](xpp::Lines &lines){
    std::string out;

    while(!lines.at_end()){
      xpp::AutoxSolutionHeader h;
      const auto header=lines.next();
      if(xpp::trim_blanks(header).empty())continue;
      auto reader=xpp::TokenReader::of_text(header);
      if(!xpp::read_autox_solution_header(reader,h,s.autox_state.parameter_layout.block))lines.fail("invalid foreign solution header");
      if(h.npar<static_cast<long>(imported.size()))lines.fail("foreign solutions omit selected parameters");
      const std::size_t orbit=static_cast<std::size_t>(h.ntpl)*h.nar;
      const std::size_t prefix=orbit+(h.ntst?2*h.nfpr+static_cast<std::size_t>(h.ntpl)*(h.nar-1):0);
      // A finite double's shortest round-trip spelling plus separator fits in 32 bytes.
      constexpr std::size_t imported_number_bytes = 32;
      // Header integers also fit in that allowance; reserve twelve header fields.
      const std::size_t expanded=(prefix+s.autox_state.parameter_layout.size()+autox_solution_header_fields)*imported_number_bytes;
      if(expanded>xpp::zip::archive_bytes_limit-out.size())lines.fail("translated autoX solutions exceed the session memory limit");
      std::vector<double> parameters(s.autox_state.parameter_layout.size());
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
      h.nskip=body_rows+(h.npar+autox_solution_columns-1)/autox_solution_columns;
      const std::string header_text=xpp::format("{} {} {} {} {} {} {} {} {} {} {} {}\n",h.ibr,h.ntot,xpp::autox_point_encode(h.itp),h.lab,h.nfpr,h.isw,h.ntpl,h.nar,h.nskip,h.ntst,h.ncol,h.npar);
      if(header_text.size()+body.size()+parameters.size()*imported_number_bytes>xpp::zip::archive_bytes_limit-out.size())
        lines.fail("translated autoX solutions exceed the session memory limit");
      out+=header_text;
      out+=body;
      for(std::size_t i=0;i<parameters.size();i++)out+=xpp::number(parameters[i])+((i+1)%autox_solution_columns==0 || i+1==parameters.size()?"\n":" ");
    }
    return out;
  });
  return converted;
}

/* Foreign XPPAUT diagrams remain an explicit import, separate from Load. */
xpp::Result<> import_autox(xpp::Session &s, const std::optional<std::string> &file)
{
  std::string filename=xpp::files::output_name(s.model().this_file, ".auto");
  if(file)filename=*file;
  else if(!file_selector("Import XPPAUT diagram",filename,"*.auto"))return {};
  xpp::UniqueFile fp=xpp::open_read(filename);
  if(!fp)return std::unexpected(xpp::files::open_error("autoX",filename));
  return import_autox_file(s,fp.get(),filename);
}

std::string autox_solutions_file(const xpp::Session &s)
{
  return s.autox_state.file+".s";
}

namespace {
struct ImportedAutox {
  xpp::AutoxSettingsSet settings;
  int mark_count=0;
  std::vector<int> parameters;
  std::deque<DIAGRAM> points;
  std::string solutions;
};
}

xpp::Result<> import_autox_file(xpp::Session &s, FILE *fp, const std::string &file)
{
  std::string text;
  if(!xpp::read_bytes(fp,text,xpp::zip::archive_bytes_limit))
    return xpp::fail("import autoX","cannot read foreign autoX file within the session memory limit",xpp::Place{file});
  auto read=xpp::read_lines("import autoX",file,text,[&](xpp::Lines &lines){
    ImportedAutox held;
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
    if(!first.read(held.mark_count) || held.mark_count<0 || held.mark_count>xpp::autox_foreign_mark_rows || !first.at_end())
      lines.fail("foreign mark count exceeds the stored mark slots");
    // Validate every encoded row, including inactive foreign marks.
    settings.nmarks=xpp::autox_foreign_mark_rows;
    settings.mark_name.resize(*settings.nmarks);
    settings.mark_value.resize(*settings.nmarks);
    settings.par1=s.model().upar_names[held.parameters.front()];
    if(count>1)settings.par2=s.model().upar_names[held.parameters[1]];
    for(int k=0;k<xpp::autox_foreign_mark_rows;k++){
      int slot;
      double value;
      lines.numbers(value,slot);
      const int solver=imported_solver_index(s,held.parameters,slot);
      const int parameter=s.autox_state.parameter_layout.model_index(solver);
      if(slot<0 || (solver!=xpp::autox_unknown::period && parameter<0))
        lines.fail("invalid foreign mark parameter index");
      settings.mark_value[k]=value;
      settings.mark_name[k]=solver==xpp::autox_unknown::period ? "T" : s.model().upar_names[parameter];
      locations[xpp::format("mark{}",k)]=lines.line();
    }
    const auto numeric=[&](std::initializer_list<int> fields){
      auto row=xpp::TokenReader::of_text(lines.next("autoX numerics"));
      for(int field:fields){
        auto &value=settings.num[field];
        if(!row.read(value))lines.fail("invalid or incomplete autoX numerics");
        if(field==xpp::AUTOX_NUM_MAX_BRANCH_SWITCHES){
          /* XPPAUT's MXBF is one signed number: its sign is the direction (W531 keeps them apart) */
          settings.num[xpp::AUTOX_NUM_SWITCH_ONE_DIRECTION]=value<0;
          settings.has_num[xpp::AUTOX_NUM_SWITCH_ONE_DIRECTION]=1;
          value=std::fabs(value);
        }
        std::string why;
        if(!xpp::autox_settings_num_ok(field,value,why))lines.fail(why);
        settings.has_num[field]=1;
        locations[xpp::autox_settings_num_key(field)]=lines.line();
      }
      if(!row.at_end())lines.fail("extra autoX numeric fields");
    };
    numeric({xpp::AUTOX_NUM_MESH_INTERVALS,xpp::AUTOX_NUM_MAX_STEPS,xpp::AUTOX_NUM_OUTPUT_EVERY});
    numeric({xpp::AUTOX_NUM_STEP_SIZE,xpp::AUTOX_NUM_MIN_STEP_SIZE,xpp::AUTOX_NUM_MAX_STEP_SIZE});
    numeric({xpp::AUTOX_NUM_PARAMETER_MIN,xpp::AUTOX_NUM_PARAMETER_MAX,xpp::AUTOX_NUM_NORM_MIN,xpp::AUTOX_NUM_NORM_MAX});
    numeric({xpp::AUTOX_NUM_MESH_ADAPT_EVERY,xpp::AUTOX_NUM_MAX_BRANCH_SWITCHES,xpp::AUTOX_NUM_OUTPUT_DETAIL,xpp::AUTOX_NUM_MAX_LOCATION_ITERATIONS,xpp::AUTOX_NUM_MAX_NEWTON_ITERATIONS,xpp::AUTOX_NUM_FULL_NEWTON_AFTER,xpp::AUTOX_NUM_STEP_ADAPT_EVERY});
    int var,plot;
    lines.numbers(settings.range[0],settings.range[2],settings.range[1],settings.range[3],var,plot);
    if(var<0 || var>=s.model().node)lines.fail("foreign axis variable is outside the model");
    settings.var=s.model().uvar_names[var];
    settings.plot=xpp::autox_projection_from_xppaut(plot);
    if(!settings.plot)lines.fail(xpp::format("{} is not one of XPPAUT's plot types (0-4, 10, 11)",plot));
    settings.has_range.fill(1);
    // XPPAUT omits Ncol; the retained collocation order must still be supported.
    settings.has_num[xpp::AUTOX_NUM_COLLOCATION_POINTS]=1;
    settings.num[xpp::AUTOX_NUM_COLLOCATION_POINTS]=xpp::autox_settings_now(s).num[xpp::AUTOX_NUM_COLLOCATION_POINTS];
    if(const xpp::Result<> checked=xpp::autox_settings_check(s,settings,-1);!checked){
      const std::string &key=checked.error().field;
      lines.fail(locations.contains(key)?locations.at(key):lines.line(),checked.error().what);
    }
    std::vector<double> parameters;
    for(int k=0;k<s.model().nupar;k++)parameters.push_back(s.parser.constants[s.model().upar_con[k]]);
    held.points=xpp::snapx::autox_members::read_foreign_diagram(lines,s.model().node,held.parameters,parameters,
        s.autox_state.type_of_calc,s.autox_state.two_param,s.autox_state.blrtn.torper);
    const int offset=lines.line();
    std::string tail;
    while(!lines.at_end())tail+=std::string(lines.next())+'\n';
    const auto solutions=convert_autox_solutions(s,held.parameters,tail,file);
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
  auto restored=xpp::snapx::autox_members::restore_members(s,
      {held.settings,std::move(held.points),{},held.solutions});
  if(!restored)return restored;
  s.autox_state.imported_parameters=std::move(held.parameters);
  forget_autox_diagram(s,false);
  return {};
}

/* Bound expanded solution values by the archive memory budget before any
   restart mesh or orbit-table allocation can use counts from a sent file. */
constexpr std::size_t autox_solution_values_limit = zip::archive_bytes_limit / sizeof(double);

bool read_autox_solution_header(TokenReader &reader, AutoxSolutionHeader &h, long parameters)
{
    long itp_code{};
    for (long *v : {&h.ibr, &h.ntot, &itp_code, &h.lab, &h.nfpr, &h.isw, &h.ntpl, &h.nar, &h.nskip, &h.ntst, &h.ncol, &h.npar})
        if (!reader.read(*v) || *v <= std::numeric_limits<int>::min() || *v > std::numeric_limits<int>::max()) return false;
    /* a stored point's type is one of autoX's: the header of any other is bad */
    const auto itp = xpp::autox_point_decode(itp_code);
    if (!itp) return false;
    h.itp = *itp;
    std::string why;
    const bool collocation = h.ntst == 0 ? h.ncol == 0 : h.equilibrium_direction() || autox_settings_num_ok(AUTOX_NUM_COLLOCATION_POINTS, h.ncol, why);
    const bool mesh = h.ntst == 0 || h.equilibrium_direction() || autox_settings_num_ok(AUTOX_NUM_MESH_INTERVALS, h.ntst, why);
    return h.nfpr >= 0 && h.nfpr <= parameters && h.ntpl >= 1 && static_cast<std::size_t>(h.ntpl) <= autox_solution_values_limit / static_cast<std::size_t>(h.nar < 2 ? 2 : h.nar) && h.nar >= 2 && static_cast<std::size_t>(h.nar) <= autox_solution_values_limit
        && h.nskip >= h.ntpl && h.ntst >= 0 && h.ntst <= h.nskip
        && mesh && collocation && h.npar >= 0 && h.npar <= parameters && h.nfpr <= h.npar
        && (h.ntst == 0 || h.ntpl == static_cast<long long>(h.ntst) * h.ncol + 1);
}

bool read_autox_solution_rows(TokenReader &reader, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol,
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

bool read_autox_solution_parameters(TokenReader &reader, const AutoxSolutionHeader &header,
                                   const AutoxRun &run, double *parameters)
{
    for (long i = 0; i < header.npar; ++i)
        if (!reader.read(parameters[i])) return false;
    autox_label_parameters(run, static_cast<int>(header.lab), parameters);
    return true;
}

Result<> check_autox_solutions(std::string_view text, std::string file, int variables, int parameters, bool foreign, int hopf_dimension)
{
    const auto read = read_lines("autoX's solutions", std::move(file), text, [variables, parameters, foreign, hopf_dimension](Lines &l) {
        while (!l.at_end()) {
            const std::string_view head = l.next();
            if (head.empty()) continue;
            AutoxSolutionHeader h;
            TokenReader header = TokenReader::of_text(head);
            if (!read_autox_solution_header(header, h, parameters) || !header.at_end())
                l.fail(xpp::format("invalid autoX solution header or counts: {}",head));
            if (!foreign && h.npar != parameters)
                l.fail(xpp::format("autoX solution parameter layout has {} slots; expected {}: {}",h.npar,parameters,head));
            if (h.nar - 1 > variables || h.npar > parameters || h.nfpr > h.npar)
                l.fail(xpp::format("autoX solution counts ({}, {}, {}) exceed this model's storage ({}, {})",h.nar-1,h.npar,h.nfpr,variables,parameters));
            if (!foreign && hopf_dimension && h.ntst == 0 && (h.itp.started_from(AutoxPointKind::Hopf) || h.itp.started_from(AutoxPointKind::NeimarkSacker))
                && h.nar != hopf_dimension + 1)
                l.fail(xpp::format("autoX Hopf solution has {} states; expected {} (the current layout): {}", h.nar - 1, hopf_dimension, head));
            const long rows = h.ntpl, columns = h.nar, payload = h.nskip, parameter_values = h.npar;
            const std::size_t orbit_values = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
            const std::size_t expected = orbit_values + parameter_values
                + (h.ntst ? 2 * h.nfpr + static_cast<std::size_t>(rows) * (columns - 1) : 0);
            if (expected > autox_solution_values_limit) l.fail("autoX solution counts exceed the session memory limit");
            std::size_t values = 0;
            const std::string payload_name=xpp::format("an autoX solution payload row for label {}",h.lab);
            for (int row = 0; row < payload; row++) {
                const auto row_text = l.next(payload_name);
                Tokens tokens(row_text);
                if (trim_blanks(row_text).empty()) l.fail("empty autoX solution payload row");
                while (const auto word = tokens.next(" \t\r\n")) {
                    double value;
                    if (!parse_number(*word, value) || !std::isfinite(value)) l.fail("invalid autoX solution number");
                    if (h.ntst && values >= orbit_values && values < orbit_values + h.nfpr) {
                        int parameter;
                        if (!parse_int(*word, parameter) || parameter < 0 || parameter >= h.npar)
                            l.fail("autoX restart parameter index is out of bounds");
                    }
                    values++;
                }
            }
            if (values != expected)
                l.fail(xpp::format("autoX solution label {} payload does not match its header counts",h.lab));
        }
        return true;
    });
    if (!read) return std::unexpected(read.error());
    return {};
}

int move_to_label(const Session &s, int mylab, int *nrow, int *ndim, FILE *fp)
{
  AutoxSolutionHeader h;
  TokenReader tr = TokenReader::attach(fp);
  while (read_autox_solution_header(tr, h, s.autox_state.parameter_layout.size())) {
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
  read_autox_solution_rows(reader, n, 0, 0, 1, 0, n, u, t, false, 0, {});
}

/* the diagram and its eigenvalues/multipliers as CSV (csv_export.h),
   pandas.read_csv/MATLAB readtable read with no options; one file dialog
   answer names both files (csv_export_diagram_pair derives the second) */
xpp::Result<> export_autox_csv(xpp::Session &s, const std::optional<std::string> &file)
{
  if(diagram_count(s.diagram)<2)return xpp::fail("autox","The diagram has no points to write",command_place());
  std::string filename=xpp::files::output_name(s.model().this_file,".csv","diagram");
  if(file)filename=*file;
  else if(!file_selector("Export CSV",filename,"*.csv"))return {};
  const xpp::Result<bool> written=csv_export_diagram_pair(s,filename.c_str());
  if(!written)return std::unexpected(written.error());
  return {};
}



void autox_get_info(xpp::Session &s, int *n, std::string &pname)
{
  if(s.autox_state.diagram_mark.state==2){
    const int i1=abs(s.autox_state.diagram_mark.start_point);
    *n=abs(abs(s.autox_state.diagram_mark.end_point)-i1);
    if(const DIAGRAM *d=diagram_find(s.diagram,s.autox_state.diagram_mark.start_branch,i1))
      pname=s.model().upar_names[d->icp1.index];
  }
}

void autox_set_mark(xpp::Session &s, int i)
{
  int pt,ibr;
  if(s.autox_state.diagram_mark.state==2){
    ibr=s.autox_state.diagram_mark.start_branch;
    if(abs(s.autox_state.diagram_mark.start_point)<abs(s.autox_state.diagram_mark.end_point))
      pt=abs(s.autox_state.diagram_mark.start_point)+i;
    else
      pt=abs(s.autox_state.diagram_mark.end_point)+i;
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
  autox_set_pars_from(s, diagram_parameters(*d).data());
  xpp::evaluate_derived(s);
  xpp::ok_or_show(xpp::redo_all_fun_tables(s));
  redraw_params();
  redraw_ics();
  if((d->per)>0)
    xpp::set_total(s,d->per);
}

void do_autox_range(xpp::Session &s)
{
  double t=s.numerics.total_time;
  
  if(s.autox_state.diagram_mark.state==2)
    xpp::do_autox_range_go(s);
  s.numerics.total_time=t;
}

/* ---- grabbing a point on the bifurcation diagram, marking a branch and
   the hint line (logic from autox_x11.c) ---- */
static void grab_diagram_point(xpp::Session &s, const DIAGRAM *d);
static void finish_grab(xpp::Session &s);

/* the point types the grab-by-type menu offers, in the menu's items' order (menus.cpp: its keys behlmptu) */
constexpr std::array<xpp::AutoxPointKind, 8> autox_grab_kinds={
	xpp::AutoxPointKind::BranchPoint, xpp::AutoxPointKind::End, xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Fold,
	xpp::AutoxPointKind::MaxSteps, xpp::AutoxPointKind::PeriodDoubling, xpp::AutoxPointKind::NeimarkSacker, xpp::AutoxPointKind::UserZero};

std::optional<xpp::AutoxPointKind> query_special(const char *title)
{
	XppMenu menu=menu_autox_special;
	menu.title=title;
	int ch=static_cast<char>(menu_choose(&menu,1));
	int i=ch!=0?menu_index(&menu,ch):-1;
	if(i<0) return std::nullopt;
	return autox_grab_kinds[static_cast<std::size_t>(i)];
}

void traverse_diagram(xpp::Session &s)
{
  std::deque<DIAGRAM> &points=s.diagram.points;
  int done=0;
  int ix,iy;
  int xm,ym;
  s.autox_state.diagram_mark.state=0;
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
  const auto type_search=[&](int step, const char *title, const char *missing){
    const auto wanted=query_special(title);
    if(!wanted)return;
    const int i=search(step,[&](const DIAGRAM &d){ return d.itp.kind==*wanted; });
    if(i>=0)at=i;
    else{
      s.autox_state.bifur.hinttxt=xpp::format("  {} {} not found",missing,xpp::autox_point_kind_label(*wanted));
      ui.autox_show_hint(s);
    }
    show(1);
  };
  const auto labelled=[](const DIAGRAM &d){ return d.lab!=0; };

  show(1);
  while(done==0){
    const int kp=ui.autox_grab_event(s,&xm,&ym);
    if(kp==XPP_AUTOX_NODE){
      /* a point of the diagram by its entry: the cursor goes there */
      if(xm>=0&&xm<=last){
        clear_msg(s);
        at=xm;
        show(1);
      }
    }
    else if(kp==XPP_AUTOX_CLICK){
      /* the point nearest the click: every point visited for its place */
      clear_msg(s);
      at=0;
      show(0);
      int nearest=0;
      double ndist=s.autox_state.bifur.wid*s.autox_state.bifur.hgt;
      const LabeledOrbits lalo=s.autox_state.load_all_labeled_orbits;
      s.autox_state.load_all_labeled_orbits=LabeledOrbits::Off;
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
      s.autox_state.load_all_labeled_orbits=lalo;
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
        type_search(1,"Next...","Higher");
        break;
      case DOWN:
        type_search(-1,"Previous...","Lower");
        break;
      case TAB: /* the next labelled point; wraps to the first */
        {
          const int i=search(1,labelled);
          at=i>=0?i:0;
        }
        show(1);
        break;
      case 's': /* mark the start of a branch */
        if(s.autox_state.diagram_mark.state==0) {
          s.autox_state.diagram_mark.start_branch=points[at].ibr;
          s.autox_state.diagram_mark.start_point=points[at].ntot;
          s.autox_state.diagram_mark.state=1;
        }
        break;
      case 'e': /* mark end of branch */
        if(s.autox_state.diagram_mark.state==1){
          s.autox_state.diagram_mark.end_branch=points[at].ibr;
          s.autox_state.diagram_mark.end_point=points[at].ntot;
          s.autox_state.diagram_mark.state=2;
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
        ui.autox_grab_end(1);
        break;
      case ESC:
        done=-1;
        ui.autox_grab_end(-1);
        break;
      }
    }
  }
  /* check mark_flag branch similarity */
  if(s.autox_state.diagram_mark.state==2){
    if(s.autox_state.diagram_mark.start_branch!=s.autox_state.diagram_mark.end_branch)
      s.autox_state.diagram_mark.state=0;
  }
  if(done==1) grab_diagram_point(s, &points[at]);
  finish_grab(s);
}

/* takes diagram point d exactly as traverse_diagram's Return does: s.autox_state.grabpt,
   the parameters, the initial condition. The caller then calls
   finish_grab(), which traverse_diagram does whether or not a point was
   taken. */
GRABPT autox_point_data(const DIAGRAM &d)
{
  GRABPT point;
  point.ibr=d.ibr;
  point.lab=d.lab;
  point.par=diagram_parameters(d);
  point.per=d.per;
  point.torper=d.torper;
  point.uhi=d.uhi;
  point.ulo=d.ulo;
  point.u0=d.u0;
  point.ubar=d.ubar;
  point.flag=1;
  point.itp=d.itp;
  point.curve=d.flag2;
  point.calc=d.calc;
  point.nfpar=d.nfpar;
  return point;
}

static void grab_diagram_point(Session &s, const DIAGRAM *d)
{
  s.autox_state.grabpt=autox_point_data(*d);
  for(int i=0;i<s.model().node;i++)setvar(s,i+1,s.autox_state.grabpt.u0[static_cast<std::size_t>(i)]);
  set_initial_data(s,s.autox_state.grabpt.u0);
  autox_set_pars_from(s,s.autox_state.grabpt.par.data());
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
   s.autox_state.grabpt, parameters, info strip and stability circle, and what a
   following Run starts from. Returns 0 and changes nothing for a label no
   stored point has. */
int autox_grab_label(xpp::Session &s, int lab)
{
  return grab_and_finish(s, diagram_of_label(s,lab));
}

/* grabs the index'th (1-based) stored point of the point type `type` (its name,
   autox_point_kind_name), in stored order: "the 2nd hopf".
   Returns 0 and changes nothing when there is no such point. */
const DIAGRAM *autox_point_by_type(const Session &s, AutoxPointKind type, int index)
{
  int count=0;
  if(index<1 || diagram_count(s.diagram)<2)return nullptr;
  for(const DIAGRAM &d:s.diagram.points)
    if(d.lab!=0 && d.itp.kind==type && ++count==index)return &d;
  return nullptr;
}

int autox_grab_type_index(Session &s, AutoxPointKind type, int index)
{
  return grab_and_finish(s,autox_point_by_type(s,type,index));
}

void clear_msg(xpp::Session &s)
{
  s.autox_state.bifur.hinttxt.clear();
  ui.autox_show_hint(s);
}

void autox_point_xy(xpp::Session &s, double x,double y)
{
    s.autox_state.bifur.hinttxt=xpp::format("x={:g},y={:g}",x,y);
    storeautoxpoint(s, x,y);
    ui.autox_show_hint(s);
}


/* ---- autoX's private scratch directory (session.h: AutoxState::dir),
   made by xpp::files::make_temp_dir ---- */
void cleanup_autox_dir(xpp::Session &s)
{
  std::string &dir=s.autox_state.dir;
  if (!dir.empty()) {
    xpp::files::remove_temp_dir(dir.c_str());
    dir.clear();
  }
}

void renew_autox_dir(xpp::Session &s)
{
  std::string &dir=s.autox_state.dir;
  if (dir.empty()) return; /* none was made: autoX writes beside the model */
  xpp::files::remove_temp_dir(dir.c_str());
  dir=xpp::files::make_temp_dir();
}

} // namespace xpp
