/* The set format (lunch-new.h): a set file (.set), a parameter file
   (.par), an initial-conditions file (.ic), written here and read through
   xpp_io.h's Lines whole, checked, then applied in one step (W125). */
#include "model.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "lunch-new.h"
#include "expr.h"
#include "browse.h"
#include "volterra2.h"
#include "storage.h"
#include "xpp_session.h"
#include "xpp_files.h"

#include "numerics.h"
#include <stdio.h>
#include <time.h>
#include "load_eqn.h"
#include "xpp_globals.h"
#include "xpp_batch.h"
#include "delay_handle.h"
#include "solver.h"
#include "model_options.h"
#include <array>
#include <string>
#include <string_view>
#include <type_traits>

namespace xpp {

namespace {

/* the Poincare map's names, as a set file labels its number with them */
constexpr std::array<const char *, 4> poincare_names = {"Poincare None", "Poincare Section", "Poincare Max", "Period"};

/* An equation line of do_info/dump_eqn: dX/dT=..., X(n+1)=... or X=... */
void put_equation(const xpp::Session &s, FILE *fp, int i)
{
  if(i>=s.model().node)
    xpp::print(fp,"{}={}\n",s.model().uvar_names[i],s.model().formulas[i]);
  else if(!xpp::solver_info(s.numerics.method).traits.discrete)
    xpp::print(fp,"d{}/dT={}\n",s.model().uvar_names[i],s.model().formulas[i]);
  else
    xpp::print(fp,"{}(n+1)={}\n",s.model().uvar_names[i],s.model().formulas[i]);
}

/* do_info/dump_eqn's equations, fixed variables and functions */
void put_equations(const xpp::Session &s, FILE *fp)
{
  for(int i=0;i<s.model().neq;i++)put_equation(s,fp,i);
  if(s.model().fix_var>0){
    xpp::print(fp,"\nwhere ...\n");
    for(int i=0;i<s.model().fix_var;i++)
      xpp::print(fp,"{} = {} \n",s.model().fixinfo[i].name,s.model().fixinfo[i].value);
  }
  if(s.model().nfun>0){
    xpp::print(fp,"\nUser-defined functions:\n");
    user_fun_info(s.model(),fp);
  }
}

/* The parameters four to a line, each after prefix ("" or "%% ") */
void put_parameters(const xpp::Session &s, FILE *fp, const char *prefix)
{
  double z;
  for(int i=0;i<s.model().nupar;i++){
    get_val(s,s.model().upar_names[i],&z);
    xpp::print(fp,"{}{}={:.16g}   ",prefix,s.model().upar_names[i],z);
    if(i%4==3) xpp::print(fp,"\n");
  }
  xpp::print(fp,"\n");
}

/* the value of parameter i of s */
double parameter(const xpp::Session &s, int i)
{
  double z=0;
  get_val(s,s.model().upar_names[i],&z);
  return z;
}

/* ---- a set file's parts, written ---- */

void write_numerics(const xpp::Session &s, FILE *fp)
{
  const NumericsSettings &n=s.numerics;
  write_heading(fp,"# Numerical stuff");
  write_whole(fp,n.njmp," nout");
  write_whole(fp,n.nmesh," nullcline mesh");
  write_whole(fp,n.method,xpp::solver_info(n.method).set_label);
  write_real(fp,n.tend,"total");
  write_real(fp,n.delta_t,"DeltaT");
  write_real(fp,n.t0,"T0");
  write_real(fp,n.trans,"Transient");
  write_real(fp,n.bound,"Bound");
  write_real(fp,n.hmin,"DtMin");
  write_real(fp,n.hmax,"DtMax");
  write_real(fp,n.toler,"Tolerance");
  write_real(fp,n.atoler,"Abs. Tolerance");
  write_real(fp,n.delay,"Max Delay");
  write_whole(fp,n.evec_iter,"Eigenvector iterates");
  write_real(fp,n.evec_err,"Eigenvector tolerance");
  write_real(fp,n.newt_err,"Newton tolerance");
  write_real(fp,n.poipln,"Poincare plane");
  write_real(fp,n.bvp_tol,"Boundary value tolerance");
  write_real(fp,n.bvp_eps,"Boundary value epsilon");
  write_whole(fp,n.bvp_maxit,"Boundary value iterates");
  write_whole(fp,n.poimap,poincare_names[static_cast<std::size_t>(n.poimap)]);
  write_whole(fp,n.poivar,"Poincare variable");
  write_whole(fp,n.poisgn,"Poincare sign");
  write_whole(fp,n.sos,"Stop on Section");
  write_whole(fp,s.delay.flag,"Delay flag");
  write_real(fp,s.data_store.current_time,"Current time");
  write_real(fp,s.integrator.last_time,"Last Time");
  write_whole(fp,s.integrator.my_start,"MyStart");
  write_whole(fp,n.inflag,"INFLAG");
}

void write_exprs(const xpp::Session &s, FILE *fp)
{
  const xpp::Model &m=s.model();
  write_heading(fp,"# Delays");
  for(int i=0;i<m.node;i++)write_text(fp,s.delay_string[i]);
  write_heading(fp,"# Bndry conds");
  for(int i=0;i<m.node;i++)write_text(fp,s.bcs[i].string.data());
  write_heading(fp,"# Old ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)write_real(fp,s.last_ic[i],m.uvar_names[i]);
  write_heading(fp,"# Ending  ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)write_real(fp,s.data_store.current[i],m.uvar_names[i]);
  write_heading(fp,"# Parameters");
  for(int i=0;i<m.nupar;i++)write_real(fp,parameter(s,i),m.upar_names[i]);
}

/* Transpose's settings, the H functions' coupling, the array plot, the
   torus and the ranges: the parts a set file of xppautX's has after the
   graphics */
void write_more(const xpp::Session &s, FILE *fp)
{
  const auto &t=s.adjoint.transpose;
  write_heading(fp,"# Transpose variables etc");
  write_text(fp,t.firstcol);
  write_whole(fp,t.ncol,"n columns");
  write_whole(fp,t.nrow,"n rows");
  write_whole(fp,t.rowskip,"row skip");
  write_whole(fp,t.colskip,"col skip");
  write_whole(fp,t.row0,"row 0");

  write_heading(fp,"# Coupling stuff for H funs");
  for(int i=0;i<s.model().node;i++)write_text(fp,s.adjoint.coup_string[i]);

  const APLOT &a=s.array_plot.plot;
  write_heading(fp,"# Array plot stuff");
  write_text(fp,a.name);
  write_whole(fp,a.nacross,"NCols");
  write_whole(fp,a.nstart,"Row 1");
  write_whole(fp,a.ndown,"NRows");
  write_whole(fp,a.nskip,"RowSkip");
  write_real(fp,a.zmin,"Zmin");
  write_real(fp,a.zmax,"Zmax");

  write_heading(fp,"# Torus information ");
  write_whole(fp,s.numerics.torus," Torus flag 1=ON");
  write_real(fp,s.numerics.tor_period,"Torus period");
  if(s.numerics.torus)
    for(int i=0;i<s.model().neq;i++)write_whole(fp,s.itor[i],s.model().uvar_names[i]);

  const EquilibriumRange &e=s.integrator.eq_range;
  write_heading(fp,"# Range information");
  write_text(fp,e.item);
  write_whole(fp,e.col,"eq-range stab col");
  write_whole(fp,e.shoot,"shoot flag 1=on");
  write_whole(fp,e.steps,"eq-range steps");
  write_real(fp,e.plow,"eq_range low");
  write_real(fp,e.phigh,"eq_range high");
  const RangeVars &r=s.integrator.range;
  write_text(fp,r.item);
  write_text(fp,r.item2);
  write_whole(fp,r.steps,"Range steps");
  write_whole(fp,r.cycle,"Cycle color 1=on");
  write_whole(fp,r.reset,"Reset data 1=on");
  write_whole(fp,r.oldic,"Use old I.C.s 1=yes");
  write_real(fp,r.plow,"Par1 low");
  write_real(fp,r.plow2,"Par2 low");
  write_real(fp,r.phigh,"Par1 high");
  write_real(fp,r.phigh2,"Par2 high");
  const ShootRange &h=s.shoot_range;
  write_text(fp,h.item);
  write_whole(fp,h.side,"BVP side");
  write_whole(fp,h.cycle,"color cycle flag 1=on");
  write_whole(fp,h.steps,"BVP range steps");
  write_real(fp,h.plow,"BVP range low");
  write_real(fp,h.phigh,"BVP range high");
}

/* ---- a set file, read ---- */

/* the value v of the line just read refused (ReadFailed) when the
   numerics setting key (the option table's, model_options.h) does not
   take it: the rule an @ line, the Numerics menu and `set num` check */
void check_setting(Lines &l, std::string_view key, double v)
{
  const OptionRow *row=numerics_option(key);
  if(const char *no=rule_problem(row->rule,v))l.fail(xpp::format("{} {}",row->label,no));
}

/* the numerics into f, each checked as the Numerics menu checks it */
void read_numerics(const xpp::Session &s, Lines &l, SetFile &f)
{
  l.heading("# Numerical stuff");
  f.njmp=l.whole("nout",true);
  check_setting(l,"nout",f.njmp);
  f.nmesh=l.whole("nullcline mesh",true);
  check_setting(l,"nmesh",f.nmesh);
  f.method=l.whole("the method");
  const auto picked=check_method(s.model(),f.method,l.error(l.line(),"").place);
  if(!picked)l.fail(picked.error().what);
  f.method=*picked;
  l.check_name(xpp::solver_info(f.method).set_label);
  f.tend=l.real("total",true);
  f.delta_t=l.real("DeltaT",true);
  check_setting(l,"dt",f.delta_t);
  f.t0=l.real("T0",true);
  f.trans=l.real("Transient",true);
  f.bound=l.real("Bound",true);
  check_setting(l,"bound",f.bound);
  f.hmin=l.real("DtMin",true);
  check_setting(l,"dtmin",f.hmin);
  f.hmax=l.real("DtMax",true);
  check_setting(l,"dtmax",f.hmax);
  f.toler=l.real("Tolerance",true);
  check_setting(l,"tol",f.toler);
  f.atoler=l.real("Abs. Tolerance",true);
  check_setting(l,"atol",f.atoler);
  f.delay=l.real("Max Delay",true);
  check_setting(l,"delay",f.delay);
  f.evec_iter=l.whole("Eigenvector iterates",true);
  check_setting(l,"newt_iter",f.evec_iter);
  f.evec_err=l.real("Eigenvector tolerance",true);
  check_setting(l,"newt_tol",f.evec_err);
  f.newt_err=l.real("Newton tolerance",true);
  check_setting(l,"jac_eps",f.newt_err);
  f.poipln=l.real("Poincare plane",true);
  f.bvp_tol=l.real("Boundary value tolerance",true);
  check_setting(l,"bvp_tol",f.bvp_tol);
  f.bvp_eps=l.real("Boundary value epsilon",true);
  check_setting(l,"bvp_eps",f.bvp_eps);
  f.bvp_maxit=l.whole("Boundary value iterates",true);
  check_setting(l,"bvp_maxit",f.bvp_maxit);
  f.poimap=l.whole("the Poincare map");
  if(f.poimap<0||f.poimap>=static_cast<int>(poincare_names.size()))
    l.fail(xpp::format("{} is not a Poincare map's number (0 to {})",f.poimap,poincare_names.size()-1));
  l.check_name(poincare_names[static_cast<std::size_t>(f.poimap)]);
  f.poivar=l.whole("Poincare variable",true);
  if(f.poivar<0||f.poivar>s.model().neq)
    l.fail(xpp::format("{} is not a variable's number (0 to {})",f.poivar,s.model().neq));
  f.poisgn=l.whole("Poincare sign",true);
  f.sos=l.whole("Stop on Section",true);
  f.delay_flag=l.whole("Delay flag",true);
  f.current_time=l.real("Current time",true);
  f.last_time=l.real("Last Time",true);
  f.my_start=l.whole("MyStart",true);
  f.inflag=l.whole("INFLAG",true);
  /* The checked solver owns integral history when its trait says so. */
  if(xpp::solver_info(f.method).traits.integral_history){
    f.volterra_points=l.whole("Max points for volterra",true);
    if(*f.volterra_points<1)l.fail(xpp::format("{} points for Volterra: at least 1",*f.volterra_points));
  }
}

void read_exprs(const xpp::Session &s, Lines &l, SetFile &f)
{
  const xpp::Model &m=s.model();
  l.heading("# Delays");
  for(int i=0;i<m.node;i++)f.delays.emplace_back(l.next(xpp::format("the delay of {}",m.uvar_names[i])));
  l.heading("# Bndry conds");
  for(int i=0;i<m.node;i++){
    const std::string_view bc=l.next(xpp::format("boundary condition {}",i+1));
    /* the room the session's boundary condition has, its NUL left out */
    const std::size_t room=s.bcs[i].string.empty()?0:s.bcs[i].string.size()-1;
    if(bc.size()>room)l.fail(xpp::format("a boundary condition of {} characters: at most {}",bc.size(),room));
    f.bcs.emplace_back(bc);
  }
  l.heading("# Old ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)f.last_ic.push_back(l.real(m.uvar_names[i],true));
  l.heading("# Ending  ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)f.current.push_back(l.real(m.uvar_names[i],true));
  l.heading("# Parameters");
  for(int i=0;i<m.nupar;i++)f.params.push_back(l.real(m.upar_names[i],true));
}

void read_more(const xpp::Session &s, Lines &l, SetFile &f)
{
  auto &t=f.transpose;
  l.heading("# Transpose variables etc");
  t.firstcol=l.next("Transpose's first column");
  t.ncol=l.whole("n columns",true);
  t.nrow=l.whole("n rows",true);
  t.rowskip=l.whole("row skip",true);
  t.colskip=l.whole("col skip",true);
  t.row0=l.whole("row 0",true);

  l.heading("# Coupling stuff for H funs");
  for(int i=0;i<s.model().node;i++)f.coupling.emplace_back(l.next(xpp::format("the coupling of {}",s.model().uvar_names[i])));

  APLOT &a=f.aplot;
  l.heading("# Array plot stuff");
  a.name=l.next("the array plot's first column");
  a.nacross=l.whole("NCols",true);
  a.nstart=l.whole("Row 1",true);
  a.ndown=l.whole("NRows",true);
  a.nskip=l.whole("RowSkip",true);
  a.zmin=l.real("Zmin",true);
  a.zmax=l.real("Zmax",true);

  l.heading("# Torus information");
  f.torus=l.whole("Torus flag 1=ON",true);
  if(f.torus!=0&&f.torus!=1)l.fail(xpp::format("the torus flag {}: 0 or 1",f.torus));
  f.tor_period=l.real("Torus period",true);
  if(f.torus)
    for(int i=0;i<s.model().neq;i++)f.itor.push_back(l.whole(s.model().uvar_names[i],true));

  EquilibriumRange &e=f.eq_range;
  l.heading("# Range information");
  e.item=l.next("the equilibrium range's parameter");
  e.col=l.whole("eq-range stab col",true);
  e.shoot=l.whole("shoot flag 1=on",true);
  e.steps=l.whole("eq-range steps",true);
  e.plow=l.real("eq_range low",true);
  e.phigh=l.real("eq_range high",true);
  RangeVars &r=f.range;
  r.item=l.next("the range's parameter");
  r.item2=l.next("the range's second parameter");
  r.steps=l.whole("Range steps",true);
  r.cycle=l.whole("Cycle color 1=on",true);
  r.reset=l.whole("Reset data 1=on",true);
  r.oldic=l.whole("Use old I.C.s 1=yes",true);
  r.plow=l.real("Par1 low",true);
  r.plow2=l.real("Par2 low",true);
  r.phigh=l.real("Par1 high",true);
  r.phigh2=l.real("Par2 high",true);
  r.steps2=r.steps;
  ShootRange &h=f.shoot_range;
  h.item=l.next("the BVP range's parameter");
  h.side=l.whole("BVP side",true);
  h.cycle=l.whole("color cycle flag 1=on",true);
  h.steps=l.whole("BVP range steps",true);
  h.plow=l.real("BVP range low",true);
  h.phigh=l.real("BVP range high",true);
}

/* the set file whose lines are l, for s; ReadFailed at a line that is
   wrong. A session's (session: its model.set) ends at its last value;
   XPPAUT's (an import) has its model's equations after it, "RHS etc ...",
   written for a reader and not read, and is refused without them. */
SetFile read_set(const xpp::Session &s, Lines &l, bool session)
{
  const xpp::Model &m=s.model();
  SetFile f;
  const std::string_view first=l.next("## Set file");
  if(!first.starts_with("## Set file"))l.fail(xpp::format("\"{}\" is not \"## Set file\"",first));
  const int ne=l.whole("Number of equations and auxiliaries",true);
  const int ne_line=l.line();
  const int np=l.whole("Number of parameters",true);
  if(ne!=m.neq||np!=m.nupar)
    l.fail(ne!=m.neq?ne_line:l.line(),xpp::format("it is for {} equations and auxiliaries and {} parameters, the model has {} and {}",
                               ne,np,m.neq,m.nupar));
  read_numerics(s,l,f);
  read_exprs(s,l,f);
  /* the active window's graphics; in a session's check, before the load
     has an active window, the main one's */
  f.graph=s.plot_windows.current?*s.plot_windows.current:s.plot_windows.graph[0];
  read_graph(l,f.graph);
  f.transpose=s.adjoint.transpose;
  f.aplot=s.array_plot.plot;
  f.eq_range=s.integrator.eq_range;
  f.range=s.integrator.range;
  f.shoot_range=s.shoot_range;
  read_more(s,l,f);
  if(session)l.end();
  else{
    const std::string_view rest=l.next("the equations (\"RHS etc ...\") XPPAUT's set file ends with");
    if(rest!="RHS etc ...")l.fail(xpp::format("\"{}\" where the equations (\"RHS etc ...\") of XPPAUT's set file were due",rest));
  }
  return f;
}

/* text is a time as ctime writes it ("Wed Jun 30 21:49:08 1993": the
   day, the month, the day of the month, the time and the year) */
bool is_ctime(std::string_view text)
{
  constexpr std::string_view days="SunMonTueWedThuFriSat",months="JanFebMarAprMayJunJulAugSepOctNovDec";
  const auto named=[text](std::size_t at, std::string_view names){
    const std::size_t i=names.find(text.substr(at,3));
    return i!=std::string_view::npos&&i%3==0;
  };
  const auto digit=[text](std::size_t at){ return text[at]>='0'&&text[at]<='9'; };
  /* "Www Mmm dd hh:mm:ss " then the year: twenty characters */
  constexpr std::size_t year_at=20;
  if(text.size()<=year_at||!named(0,days)||!named(4,months))return false;
  for(const std::size_t at : {3u,7u,10u,19u})if(text[at]!=' ')return false;
  if(text[13]!=':'||text[16]!=':')return false;
  if(!(text[8]==' '||digit(8)))return false;
  for(const std::size_t at : {9u,11u,12u,14u,15u,17u,18u})if(!digit(at))return false;
  for(std::size_t at=year_at;at<text.size();at++)if(!digit(at))return false;
  return true;
}

/* a parameter file's values, the model's parameters' in order; the lines
   after them are the trailer write_parameter_file writes (XPPAUT's too):
   "File:" and the model's name, then the time it was written */
std::vector<double> read_parameters(const xpp::Model &m, Lines &l)
{
  const int np=l.whole("Number params");
  if(np!=m.nupar)l.fail(xpp::format("it is for {} parameters, the model has {}",np,m.nupar));
  std::vector<double> z;
  for(int i=0;i<m.nupar;i++)z.push_back(l.real(m.upar_names[i]));
  while(!l.at_end()){
    const std::string_view line=l.next();
    if(line.empty())continue;
    if(!line.starts_with("File:"))l.fail(xpp::format("\"{}\" after the parameters, where \"File:\" or the end was due",line));
    const std::string_view time=l.next("the time the file was written");
    if(!is_ctime(time))l.fail(xpp::format("\"{}\" where the time the file was written (\"Wed Jun 30 21:49:08 1993\") was due",time));
    l.end();
  }
  return z;
}

/* an initial-conditions file's values: one per differential equation */
std::vector<double> read_ics(const xpp::Model &m, Lines &l)
{
  std::vector<double> z;
  for(int i=0;i<m.node;i++)z.push_back(l.real(m.uvar_names[i]));
  l.end();
  return z;
}

} // namespace

void write_whole(FILE *fp, int value, std::string_view name) { xpp::print(fp,"{}   {}\n",value,name); }

void write_real(FILE *fp, double value, std::string_view name) { xpp::print(fp,"{:.16g}  {}\n",value,name); }

void write_text(FILE *fp, std::string_view text) { xpp::print(fp,"{}\n",text); }

void write_heading(FILE *fp, std::string_view heading) { xpp::print(fp,"{}\n",heading); }

namespace {

/* a plot window's settings as a set file holds them, in its order: each
   member given to f with its name (the set file's label after it) */
template <class G, class F>
void graph_settings(G &g, F &&f)
{
  for(int j=0;j<3;j++)
    for(int k=0;k<3;k++)f(g.rm[k][j],"rm");
  for(int j=0;j<MAXPERPLOT;j++){
    f(g.xv[j]," ");
    f(g.yv[j]," ");
    f(g.zv[j]," ");
    f(g.line[j]," ");
    f(g.color[j]," ");
  }
  f(g.ZPlane," ");
  f(g.ZView," ");
  f(g.PerspFlag," ");
  f(g.ThreeDFlag,"3DFlag");
  f(g.TimeFlag,"Timeflag");
  f(g.ColorFlag,"Colorflag");
  f(g.grtype,"Type");
  f(g.color_scale,"color scale");
  f(g.min_scale," minscale");
  f(g.xmax," xmax");
  f(g.xmin," xmin");
  f(g.ymax," ymax");
  f(g.ymin," ymin");
  f(g.zmax," zmax");
  f(g.zmin," zmin");
  f(g.xbar," ");
  f(g.dx," ");
  f(g.ybar," ");
  f(g.dy," ");
  f(g.zbar," ");
  f(g.dz," ");
  f(g.Theta," Theta");
  f(g.Phi," Phi");
  f(g.xshft," xshft");
  f(g.yshft," yshft");
  f(g.zshft," zshft");
  f(g.xlo," xlo");
  f(g.ylo," ylo");
  f(g.oldxlo," ");
  f(g.oldylo," ");
  f(g.xhi," xhi");
  f(g.yhi," yhi");
  f(g.oldxhi," ");
  f(g.oldyhi," ");
}

} // namespace

void write_graph(FILE *fp, const GRAPH &g)
{
  write_heading(fp,"# Graphics");
  graph_settings(g,[fp](const auto &v,const char *name){
    if constexpr(std::is_same_v<std::decay_t<decltype(v)>,int>)write_whole(fp,v,name);
    else write_real(fp,v,name);
  });
}

void read_graph(Lines &l, GRAPH &g)
{
  l.heading("# Graphics");
  graph_settings(g,[&l](auto &v,const char *name){
    if constexpr(std::is_same_v<std::decay_t<decltype(v)>,int>)v=l.whole(name,true);
    else v=l.real(name,true);
  });
}

void copy_graph_settings(const GRAPH &from, GRAPH &to)
{
  std::vector<double> values;
  graph_settings(from,[&values](const auto &v,const char *){ values.push_back(v); });
  std::size_t k=0;
  graph_settings(to,[&](auto &v,const char *){ v=static_cast<std::decay_t<decltype(v)>>(values[k++]); });
}

Result<SetFile> read_session_set(const xpp::Session &s, std::string file, std::string_view text)
{
  return read_lines("set file",std::move(file),text,[&s](Lines &l){ return read_set(s,l,true); });
}

void apply_set_file(xpp::Session &s, const SetFile &f, bool redraw)
{
  const xpp::Model &m=s.model();
  NumericsSettings &n=s.numerics;
  n.njmp=f.njmp;
  n.nmesh=f.nmesh;
  n.method=f.method;
  xpp::do_meth(s);
  n.tend=f.tend;
  n.delta_t=f.delta_t;
  n.t0=f.t0;
  n.trans=f.trans;
  n.bound=f.bound;
  n.hmin=f.hmin;
  n.hmax=f.hmax;
  n.toler=f.toler;
  n.atoler=f.atoler;
  n.delay=f.delay;
  n.evec_iter=f.evec_iter;
  n.evec_err=f.evec_err;
  n.newt_err=f.newt_err;
  n.poipln=f.poipln;
  n.bvp_tol=f.bvp_tol;
  n.bvp_eps=f.bvp_eps;
  n.bvp_maxit=f.bvp_maxit;
  n.poimap=f.poimap;
  n.poivar=f.poivar;
  n.poisgn=f.poisgn;
  n.sos=f.sos;
  s.delay.flag=f.delay_flag;
  s.data_store.current_time=f.current_time;
  s.integrator.last_time=f.last_time;
  s.integrator.my_start=f.my_start;
  n.inflag=f.inflag;
  if(f.volterra_points){
    xpp::allocate_volterra(s,*f.volterra_points,1);
    s.integrator.my_start=1;
  }
  xpp::chk_delay(s);
  for(int i=0;i<m.node;i++){
    s.delay_string[i]=f.delays[i];
    set_bc_formula(s,i,f.bcs[i]);
  }
  for(int i=0;i<m.node+m.nmarkov;i++){
    s.last_ic[i]=f.last_ic[i];
    s.data_store.current[i]=f.current[i];
  }
  for(int i=0;i<m.nupar;i++)set_val(s,m.upar_names[i],f.params[i]);
  copy_graph_settings(f.graph,*s.plot_windows.current);
  s.adjoint.transpose=f.transpose;
  for(int i=0;i<m.node;i++)s.adjoint.coup_string[i]=f.coupling[i];
  s.array_plot.plot=f.aplot;
  n.torus=f.torus;
  n.tor_period=f.tor_period;
  for(std::size_t i=0;i<f.itor.size();i++)s.itor[i]=f.itor[i];
  s.integrator.eq_range=f.eq_range;
  s.integrator.range=f.range;
  s.shoot_range=f.shoot_range;
  if(redraw&&program.interactive){
    ui.redraw_bcs();
    redraw_ics();
    ui.redraw_delays();
    redraw_params();
    ui.redraw_graph(s);
  }
}

Result<> import_xppaut_set(xpp::Session &s, std::string_view path, bool redraw)
{
  Result<SetFile> f=read_file_lines("set file",path,[&s](Lines &l){ return read_set(s,l,false); });
  if(!f)return std::unexpected(f.error());
  apply_set_file(s,*f,redraw);
  const auto [folder,base]=xpp::files::split_path(path);
  const std::size_t dot=base.find_last_of('.');
  const std::string file=(folder.empty()?std::string():folder+"/")+base.substr(0,dot)+std::string(xpp::snapx::extension);
  Result<bool> saved=xpp_session_save_file(s,file,true);
  if(!saved){
    saved.error().what=xpp::format("Import of {} applied, but saving session {} failed: {}. The imported values remain applied.",
                                   path,file,saved.error().what);
    return std::unexpected(saved.error());
  }
  if(!*saved){
    xpp::bottom_msg(0,xpp::format("Imported {}. Saving {} was declined; the imported values remain applied.",path,file));
    return {};
  }
  xpp::bottom_msg(0,xpp::format("Imported {}. The session now open is {}.",path,file));
  return {};
}

void file_inf(xpp::Session &s)
{
  std::string filename=s.model().this_file+".pars";
  ping();
  if(!file_selector("Save info",filename,"*.pars*"))return;
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  redraw_params();
  do_info(s,w.file());
  xpp::ok_or_show(xpp::commit_save(w));
}

void ps_write_pars(const xpp::Session &s, FILE *fp)
{
  xpp::print(fp,"\n %% {} \n %% Parameters ...\n",s.model().this_file);
  put_parameters(s,fp,"%% ");
}

void do_info(const xpp::Session &s, FILE *fp)
{
  xpp::print(fp,"File: {} \n\n Equations... \n",s.model().this_file);
  put_equations(s,fp);

  xpp::print(fp,"\n\n Numerical parameters ...\n");
  xpp::print(fp,"NJMP={}  NMESH={} METHOD={} EVEC_ITER={} \n",
	 s.numerics.njmp,s.numerics.nmesh,xpp::solver_info(s.numerics.method).name,s.numerics.evec_iter);
  xpp::print(fp,"BVP_EPS={:g},BVP_TOL={:g},BVP_MAXIT={} \n",
	 s.numerics.bvp_eps,s.numerics.bvp_tol,s.numerics.bvp_maxit);
  xpp::print(fp,"DT={:g} T0={:g} TRANS={:g} TEND={:g} BOUND={:g} DELAY={:g} MaxPts={}\n",
	 s.numerics.delta_t,s.numerics.t0,s.numerics.trans,s.numerics.tend,s.numerics.bound,s.numerics.delay,s.numerics.max_points);
  xpp::print(fp,"EVEC_ERR={:g}, NEWT_ERR={:g} HMIN={:g} HMAX={:g} TOLER={:g} \n",
	 s.numerics.evec_err,s.numerics.newt_err,s.numerics.hmin,s.numerics.hmax,s.numerics.toler);
  const std::string &poivar=ind_to_sym(s,s.numerics.poivar);
  xpp::print(fp,"POIMAP={} POIVAR={} POIPLN={:g} POISGN={} \n",
        s.numerics.poimap,poivar,s.numerics.poipln,s.numerics.poisgn);

  xpp::print(fp,"\n\n Delay strings ...\n");
  for(int i=0;i<s.model().node;i++)xpp::print(fp,"{}\n",s.delay_string[i]);
  xpp::print(fp,"\n\n BCs ...\n");
  for(int i=0;i<s.model().node;i++)xpp::print(fp,"0={}\n",s.bcs[i].string.data());
  xpp::print(fp,"\n\n ICs ...\n");
  for(int i=0;i<s.model().node+s.model().nmarkov;i++)xpp::print(fp,"{}={:.16g}\n",s.model().uvar_names[i],s.last_ic[i]);
  xpp::print(fp,"\n\n Parameters ...\n");
  put_parameters(s,fp,"");
}

void write_lunch(xpp::Session &s, FILE *fp)
{
  time_t ttt=time(0);
  xpp::print(fp,"## Set file for {} on {}",s.model().this_file,ctime(&ttt));
  write_whole(fp,s.model().neq,"Number of equations and auxiliaries");
  write_whole(fp,s.model().nupar,"Number of parameters");
  write_numerics(s,fp);
  if(xpp::solver_info(s.numerics.method).traits.integral_history)write_whole(fp,s.numerics.max_points,"Max points for volterra");
  write_exprs(s,fp);
  write_graph(fp,*s.plot_windows.current);
  write_more(s,fp);
}

void import_xppaut_set_command(xpp::Session &s)
{
  std::string filename=s.model().this_file+".set";
  ping();
  if(!file_selector("Import XPPAUT set",filename,"*.set"))return;
  if(const Result<> r=import_xppaut_set(s,filename,true);!r)show_error(r.error());
}

Result<std::vector<double>> read_parameter_file(const xpp::Model &m, std::string_view path)
{
  return read_file_lines("parameter file",path,[&m](Lines &l){ return read_parameters(m,l); });
}

Result<std::vector<double>> read_ic_file(const xpp::Model &m, std::string_view path)
{
  return read_file_lines("initial conditions file",path,[&m](Lines &l){ return read_ics(m,l); });
}

void load_parameter_file_named(xpp::Session &s, std::string_view fn)
{
  const xpp::Model &m=s.model();
  const Result<std::vector<double>> z=read_parameter_file(m,fn);
  if(!z){
    show_error(z.error());
    return;
  }
  for(int i=0;i<m.nupar;i++)set_val(s,m.upar_names[i],(*z)[static_cast<std::size_t>(i)]);
  redraw_params();
  redo_stuff(s);
}

void write_parameter_file(xpp::Session &s, std::string_view fn)
{
  const xpp::Model &m=s.model();
  xpp::Writer w=open_writer_asking(std::string(fn).c_str());
  if(!w)return;
  FILE *fp=w.file();
  write_whole(fp,m.nupar,"Number params");
  for(int i=0;i<m.nupar;i++)write_real(fp,parameter(s,i),m.upar_names[i]);
  time_t ttt=time(0);
  xpp::print(fp,"\n\nFile:{}\n{}",m.this_file,ctime(&ttt));
  xpp::ok_or_show(xpp::commit_save(w));
}

/* the --icfile / Initialconds/File format: the values alone, one per
   line, one per differential-equation variable, in the model's order --
   exactly `node` of them, as XPPAUT's own io_ic_file always read (the
   Markov chains are not in this file, in XPPAUT or here: docs/manual
   16-quick-reference.md) */
void load_ic_file_named(xpp::Session &s, std::string_view fn)
{
  const xpp::Model &m=s.model();
  const Result<std::vector<double>> z=read_ic_file(m,fn);
  if(!z){
    show_error(z.error());
    return;
  }
  for(int i=0;i<m.node;i++)s.last_ic[i]=(*z)[static_cast<std::size_t>(i)];
}

void write_ic_file(const xpp::Session &s, std::string_view fn)
{
  xpp::Writer w=open_writer_asking(std::string(fn).c_str());
  if(!w)return;
  for(int i=0;i<s.model().node;i++)w.print("{:.16g}\n",s.last_ic[i]);
  xpp::ok_or_show(xpp::commit_save(w));
}

namespace {

/* the values panel's Save/Load of .par and .ic (docs/protocol.md
   "values"), shared by the four functions below: name empty asks for
   one like Save data does (title/wild picking the dialog and the
   extension), given skips the ask; then io(s, name); a save with
   nothing to write (available false) stops before the name is asked */
template <class F>
void named_value_file(xpp::Session &s, std::string name, const char *title, const char *ext, F io, bool available = true)
{
  if(!save_ready(available))return;
  if(name.empty()){
    name=s.model().this_file+ext;
    if(!file_selector(title,name,xpp::format("*{}",ext)))return;
  }
  io(s,name);
}

} // namespace

void save_parameter_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Save Parameters",".par",write_parameter_file,s.model().nupar>0);
}

void save_ic_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Save Initial Conditions",".ic",write_ic_file,s.model().node>0);
}

void load_parameter_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Load Parameters",".par",load_parameter_file_named);
}

void load_ic_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Load Initial Conditions",".ic",load_ic_file_named);
}

void write_values_query(const xpp::Session &s, std::string_view name, bool sets, bool pars, bool ics)
{
  const xpp::Model &m=s.model();
  if(!save_ready((sets&&!m.intern_sets.empty())||(pars&&m.nupar>0)||(ics&&m.neq>0)))return;
  xpp::Writer w=xpp::open_writer_asking(name);
  if(!w)return;
  if(sets){
    w.print("#Internal sets query:\n");
    for(std::size_t i=0;i<m.intern_sets.size();i++)
      w.print("{} {} {}\n",m.intern_sets[i].name,batch_options.uses_intern_set(i),m.intern_sets[i].does);
  }
  if(pars){
    w.print("#Parameters query:\n");
    for(int i=0;i<m.nupar;i++)
      w.print("{} {:f}\n",m.upar_names[i],m.default_val[i]);
  }
  if(ics){
    w.print("#Initial conditions query:\n");
    for(int i=0;i<m.neq;i++)
      w.print("{} {:f}\n",m.uvar_names[i],s.last_ic[i]);
  }
  xpp::ok_or_show(xpp::commit_save(w));
}

} // namespace xpp
