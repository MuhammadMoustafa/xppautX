/* XPPAUT's set file (.set), read for an import only (lunch-new.h), the
   settings file (.setx) it is converted to, and the parameter file (.par)
   and initial-conditions file (.ic): read through xpp_io.h's Lines whole,
   checked, then applied in one step (W125). */
#include "model.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "lunch-new.h"
#include "expr.h"
#include "browse.h"
#include "setx_file.h"
#include "numerics_settings.h"
#include "odex.h"
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

/* ---- XPPAUT's set file, read (an import only) ----
   Positional lines, a value and then its label, read by their order: the
   layout XPPAUT writes and nothing of ours writes. It is read into the
   Settings of a .setx (setx_file.h), every value checked and every label
   compared. */

/* the Poincare map's names, as a set file labels its number with them */
constexpr std::array<const char *, 4> poincare_names = {"Poincare None", "Poincare Section", "Poincare Max", "Period"};

/* the name XPPAUT's set file gives the store-every setting */
constexpr std::string_view XPPAUT_STORE_EVERY="nout";

/* the saved setting key's value v, from the line just read, kept as text in f:
   refused (ReadFailed) when the setting's rule does not take it, and left
   out when the model has no such setting (the delay of a model without
   delays, the Volterra points of another method) */
void keep_setting(const xpp::Session &s, Lines &l, Settings &f, int method, std::string_view key, double v)
{
  const OptionRow *row=saved_setting(key);
  if(const char *no=rule_problem(row->rule,v,row->maximum))l.fail(xpp::format("{} {}",row->label.empty()?key:row->label,no));
  if(setting_applies(s,*row,method))f.set_setting(row,xpp::number(v));
}

/* the line's whole number as the setting key, which keep_setting checks */
void keep_whole(const xpp::Session &s, Lines &l, Settings &f, int method, std::string_view key, std::string_view label)
{
  keep_setting(s,l,f,method,key,l.whole(label,true));
}

void keep_real(const xpp::Session &s, Lines &l, Settings &f, int method, std::string_view key, std::string_view label)
{
  keep_setting(s,l,f,method,key,l.real(label,true));
}

/* the numerics, each checked as the Numerics menu checks it; the
   store-every line is named as XPPAUT's file names it */
void read_numerics(const xpp::Session &s, Lines &l, Settings &f)
{
  l.heading("# Numerical stuff");
  /* the method comes third: the lines before it are read as the model's own method takes them */
  int method=s.numerics.method;
  const int store_every=l.whole(XPPAUT_STORE_EVERY,true);
  const int store_every_line=l.line();
  const int mesh=l.whole("nullcline mesh",true);
  const int mesh_line=l.line();
  const int id=l.whole("the method");
  const auto picked=check_method(s.model(),id,l.error(l.line(),"").place);
  if(!picked)l.fail(picked.error().what);
  method=*picked;
  l.check_name(xpp::solver_info(method).set_label);
  f.set_setting(saved_setting("method"),std::string(xpp::solver_info(method).key));
  const auto early=[&](int line, std::string_view key, int v){
    const OptionRow *row=saved_setting(key);
    if(const char *no=rule_problem(row->rule,v,row->maximum))l.fail(line,xpp::format("{} {}",row->label,no));
    f.set_setting(row,xpp::number(v));
  };
  early(store_every_line,"store_every",store_every);
  early(mesh_line,"nullcline_mesh",mesh);
  keep_real(s,l,f,method,"total_time","total");
  keep_real(s,l,f,method,"dt","DeltaT");
  keep_real(s,l,f,method,"start_time","T0");
  keep_real(s,l,f,method,"transient_time","Transient");
  keep_real(s,l,f,method,"bound","Bound");
  keep_real(s,l,f,method,"min_step","DtMin");
  keep_real(s,l,f,method,"max_step","DtMax");
  keep_real(s,l,f,method,"tolerance","Tolerance");
  keep_real(s,l,f,method,"abs_tolerance","Abs. Tolerance");
  keep_real(s,l,f,method,"delay","Max Delay");
  keep_whole(s,l,f,method,"singpt_max_iterates","Eigenvector iterates");
  keep_real(s,l,f,method,"singpt_newton_tolerance","Eigenvector tolerance");
  /* XPPAUT's set file labels the Jacobian's epsilon with Newton's name */
  keep_real(s,l,f,method,"singpt_jacobian_epsilon","Newton tolerance");
  keep_real(s,l,f,method,"poincare_plane","Poincare plane");
  keep_real(s,l,f,method,"bvp_tolerance","Boundary value tolerance");
  keep_real(s,l,f,method,"bvp_epsilon","Boundary value epsilon");
  keep_whole(s,l,f,method,"bvp_max_iterates","Boundary value iterates");
  const int map=l.whole("the Poincare map");
  if(map<0||map>=static_cast<int>(poincare_names.size()))
    l.fail(xpp::format("{} is not a Poincare map's number (0 to {})",map,poincare_names.size()-1));
  l.check_name(poincare_names[static_cast<std::size_t>(map)]);
  const OptionRow *map_row=saved_setting("poincare_map");
  f.set_setting(map_row,std::string(map_row->choices[static_cast<std::size_t>(map)]));
  const int variable=l.whole("Poincare variable",true);
  if(variable<0||variable>s.model().neq)
    l.fail(xpp::format("{} is not a variable's number (0 to {})",variable,s.model().neq));
  f.set_setting(saved_setting("poincare_variable"),xpp::ind_to_sym(s,variable));
  keep_whole(s,l,f,method,"poincare_sign","Poincare sign");
  keep_whole(s,l,f,method,"poincare_stop","Stop on Section");
  f.delay_flag=l.whole("Delay flag",true);
  f.current_time=l.real("Current time",true);
  f.last_time=l.real("Last Time",true);
  f.my_start=l.whole("MyStart",true);
  f.inflag=l.whole("INFLAG",true);
  /* The checked solver owns integral history when its trait says so. */
  if(xpp::solver_info(method).traits.integral_history){
    const int points=l.whole("Max points for volterra",true);
    if(points<1)l.fail(xpp::format("{} points for Volterra: at least 1",points));
    f.set_setting(saved_setting("vmaxpts"),xpp::number(points));
  }
}

void read_exprs(const xpp::Session &s, Lines &l, Settings &f)
{
  const xpp::Model &m=s.model();
  l.heading("# Delays");
  for(int i=0;i<m.node;i++)f.delays[static_cast<std::size_t>(i)]=l.next(xpp::format("the delay of {}",m.uvar_names[static_cast<std::size_t>(i)]));
  l.heading("# Bndry conds");
  for(int i=0;i<m.node;i++){
    const std::string_view bc=l.next(xpp::format("boundary condition {}",i+1));
    /* the room the session's boundary condition has, its NUL left out */
    const std::size_t room=s.bcs[static_cast<std::size_t>(i)].string.empty()?0:s.bcs[static_cast<std::size_t>(i)].string.size()-1;
    if(bc.size()>room)l.fail(xpp::format("a boundary condition of {} characters: at most {}",bc.size(),room));
    f.bcs[static_cast<std::size_t>(i)]=bc;
  }
  l.heading("# Old ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)f.last_ic[static_cast<std::size_t>(i)]=l.real(m.uvar_names[static_cast<std::size_t>(i)],true);
  l.heading("# Ending  ICs");
  for(int i=0;i<m.node+m.nmarkov;i++)f.current[static_cast<std::size_t>(i)]=l.real(m.uvar_names[static_cast<std::size_t>(i)],true);
  l.heading("# Parameters");
  for(int i=0;i<m.nupar;i++)f.params[static_cast<std::size_t>(i)]=l.real(m.upar_names[static_cast<std::size_t>(i)],true);
}

/* a plot window's settings as XPPAUT's set file holds them, in its order:
   each member given to f with its name (the set file's label after it) */
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

/* the active window's graphics into g; the variables of its curves are
   columns the model has (the settings file names them) */
void read_graph(const xpp::Session &s, Lines &l, GRAPH &g)
{
  l.heading("# Graphics");
  const int first=l.line();
  graph_settings(g,[&l](auto &v,const char *name){
    if constexpr(std::is_same_v<std::decay_t<decltype(v)>,int>)v=l.whole(name,true);
    else v=l.real(name,true);
  });
  /* the 9 numbers of the rotation, then 5 lines for each curve */
  constexpr int rotation_lines=9,lines_per_curve=5;
  for(int j=0;j<g.nvars;j++){
    const int base=first+rotation_lines+lines_per_curve*j;
    const std::array<int,3> columns={g.xv[j],g.yv[j],g.zv[j]};
    for(std::size_t k=0;k<columns.size();k++)
      if(columns[k]<0||columns[k]>s.model().neq)
        l.fail(base+1+static_cast<int>(k),xpp::format("{} is not a variable's number (0 to {})",columns[k],s.model().neq));
  }
}

/* Allocation dimensions share the native settings reader's rule and cap. */
int read_dimension(Lines &l, std::string_view label)
{
  const int v=l.whole(label,true);
  if(const char *why=rule_problem(OptionRule::whole_positive,v,settings_dimension_limit))
    l.fail(xpp::format("{}={}: {} (maximum {})",label,v,why,settings_dimension_limit));
  return v;
}

void read_more(const xpp::Session &s, Lines &l, Settings &f)
{
  auto &t=f.transpose;
  l.heading("# Transpose variables etc");
  t.firstcol=l.next("Transpose's first column");
  t.ncol=read_dimension(l,"n columns");
  t.nrow=read_dimension(l,"n rows");
  t.rowskip=l.whole("row skip",true);
  t.colskip=l.whole("col skip",true);
  t.row0=l.whole("row 0",true);

  l.heading("# Coupling stuff for H funs");
  for(int i=0;i<s.model().node;i++)f.coupling[static_cast<std::size_t>(i)]=l.next(xpp::format("the coupling of {}",s.model().uvar_names[static_cast<std::size_t>(i)]));

  APLOT &a=f.aplot;
  l.heading("# Array plot stuff");
  a.name=l.next("the array plot's first column");
  a.nacross=read_dimension(l,"NCols");
  a.nstart=l.whole("Row 1",true);
  a.ndown=read_dimension(l,"NRows");
  a.nskip=l.whole("RowSkip",true);
  a.zmin=l.real("Zmin",true);
  a.zmax=l.real("Zmax",true);

  l.heading("# Torus information");
  f.torus=l.whole("Torus flag 1=ON",true);
  if(f.torus!=0&&f.torus!=1)l.fail(xpp::format("the torus flag {}: 0 or 1",f.torus));
  f.torus_period=l.real("Torus period",true);
  if(f.torus)
    for(int i=0;i<s.model().neq;i++)f.itor[static_cast<std::size_t>(i)]=l.whole(s.model().uvar_names[static_cast<std::size_t>(i)],true);

  EquilibriumRange &e=f.eq_range;
  l.heading("# Range information");
  e.item=l.next("the equilibrium range's parameter");
  e.col=l.whole("eq-range stab col",true);
  e.shoot=l.whole("shoot flag 1=on",true);
  e.steps=l.whole("eq-range steps",true);
  e.plow=l.real("eq_range low",true);
  e.phigh=l.real("eq_range high",true);
  f.set_setting(saved_setting("rangeover"),std::string(l.next("the range's parameter")));
  f.range_item2=l.next("the range's second parameter");
  f.set_setting(saved_setting("rangestep"),xpp::number(l.whole("Range steps",true)));
  f.range_cycle=l.whole("Cycle color 1=on",true);
  const auto yes_no=[&](std::string_view key, std::string_view label){
    const int v=l.whole(label,true);
    const OptionRow *row=saved_setting(key);
    if(v<0||v>=static_cast<int>(row->choices.size()))l.fail(xpp::format("{} {}: 0 or 1",label,v));
    f.set_setting(row,std::string(row->choices[static_cast<std::size_t>(v)]));
  };
  yes_no("rangereset","Reset data 1=on");
  yes_no("rangeoldic","Use old I.C.s 1=yes");
  f.set_setting(saved_setting("rangelow"),xpp::number(l.real("Par1 low",true)));
  f.range_low2=l.real("Par2 low",true);
  f.set_setting(saved_setting("rangehigh"),xpp::number(l.real("Par1 high",true)));
  f.range_high2=l.real("Par2 high",true);
  ShootRange &h=f.shoot_range;
  h.item=l.next("the BVP range's parameter");
  h.side=l.whole("BVP side",true);
  h.cycle=l.whole("color cycle flag 1=on",true);
  h.steps=l.whole("BVP range steps",true);
  h.plow=l.real("BVP range low",true);
  h.phigh=l.real("BVP range high",true);
}

/* XPPAUT's set file whose lines are l, for s; ReadFailed at a line that is
   wrong. It has its model's equations after it, "RHS etc ...", written for
   a reader and not read, and is refused without them. */
Settings read_set(const xpp::Session &s, Lines &l)
{
  const xpp::Model &m=s.model();
  Settings f=capture_settings(s,SettingsUse::file);
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
  read_graph(s,l,f.graph);
  read_more(s,l,f);
  const std::string_view rest=l.next("the equations (\"RHS etc ...\") XPPAUT's set file ends with");
  if(rest!="RHS etc ...")l.fail(xpp::format("\"{}\" where the equations (\"RHS etc ...\") of XPPAUT's set file were due",rest));
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

Result<> import_settings(xpp::Session &s, std::string_view path, bool redraw)
{
  std::string bytes;
  /* the settings of a file a user may have been sent are read within a bound */
  if(!xpp::read_bytes(path,bytes,xpp::settings_bytes_limit))
    return std::unexpected(xpp::files::exists(path)?xpp::files::open_error("settings file",path,xpp::format("cannot be read within {} bytes",xpp::settings_bytes_limit))
                                                   :xpp::files::open_error("settings file",path));
  std::string file(path);
  std::string note;
  if(!xpp::files::has_extension(path,xpp::setx_extension)){
    /* XPPAUT's set file: converted to a .setx beside it, which is what is used */
    Result<Settings> legacy=xpp::read_lines("set file",file,bytes,[&s](Lines &l){ return read_set(s,l); });
    if(!legacy)return std::unexpected(legacy.error());
    const Result<std::string> text=setx_text(s,*legacy);
    if(!text)return std::unexpected(text.error());
    const auto [folder,base]=xpp::files::split_path(path);
    file=(folder.empty()?std::string():folder+"/")+base.substr(0,base.find_last_of('.'))+std::string(xpp::setx_extension);
    const Result<bool> written=xpp::odex::write_converted(std::string(path),file,*text);
    if(!written)return std::unexpected(written.error());
    if(!xpp::read_bytes(file,bytes,xpp::settings_bytes_limit))return std::unexpected(xpp::files::open_error("settings file",file));
    note=xpp::format("Imported {}: {} {}, which is applied.",path,*written?"converted to":"already converted to",file);
  }
  Result<Settings> f=read_settings(s,file,bytes,SettingsUse::file);
  if(!f)return std::unexpected(f.error());
  apply_settings(s,*f,redraw);
  xpp::bottom_msg(0,note.empty()?xpp::format("Applied {}.",path):note);
  return {};
}

void file_inf(xpp::Session &s)
{
  std::string filename=xpp::files::output_name(s.model().this_file,".pars");
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
  xpp::print(fp,"STORE_EVERY={}  NULLCLINE_MESH={} METHOD={} SINGPT_MAX_ITERATES={} \n",
	 s.numerics.store_every,s.numerics.nullcline_mesh,xpp::solver_info(s.numerics.method).name,s.numerics.singpt_max_iterates);
  xpp::print(fp,"BVP_EPSILON={:g},BVP_TOLERANCE={:g},BVP_MAX_ITERATES={} \n",
	 s.numerics.bvp_epsilon,s.numerics.bvp_tolerance,s.numerics.bvp_max_iterates);
  xpp::print(fp,"DT={:g} START_TIME={:g} TRANSIENT_TIME={:g} TOTAL_TIME={:g} BOUND={:g} DELAY={:g} MaxPts={}\n",
	 s.numerics.delta_t,s.numerics.start_time,s.numerics.transient_time,s.numerics.total_time,s.numerics.bound,s.numerics.delay,s.numerics.max_points);
  xpp::print(fp,"SINGPT_NEWTON_TOLERANCE={:g}, SINGPT_JACOBIAN_EPSILON={:g} MIN_STEP={:g} MAX_STEP={:g} TOLERANCE={:g} \n",
	 s.numerics.singpt_newton_tolerance,s.numerics.singpt_jacobian_epsilon,s.numerics.min_step,s.numerics.max_step,s.numerics.tolerance);
  const std::string &poincare_variable=ind_to_sym(s,s.numerics.poincare_variable);
  xpp::print(fp,"POINCARE_MAP={} POINCARE_VARIABLE={} POINCARE_PLANE={:g} POINCARE_SIGN={} \n",
        s.numerics.poincare_map,poincare_variable,s.numerics.poincare_plane,s.numerics.poincare_sign);

  xpp::print(fp,"\n\n Delay strings ...\n");
  for(int i=0;i<s.model().node;i++)xpp::print(fp,"{}\n",s.delay_string[i]);
  xpp::print(fp,"\n\n BCs ...\n");
  for(int i=0;i<s.model().node;i++)xpp::print(fp,"0={}\n",s.bcs[i].string.data());
  xpp::print(fp,"\n\n ICs ...\n");
  for(int i=0;i<s.model().node+s.model().nmarkov;i++)xpp::print(fp,"{}={:.16g}\n",s.model().uvar_names[i],s.last_ic[i]);
  xpp::print(fp,"\n\n Parameters ...\n");
  put_parameters(s,fp,"");
}

void import_settings_command(xpp::Session &s)
{
  std::string filename=xpp::files::output_name(s.model().this_file,".set");
  ping();
  if(!file_selector("Import settings",filename,"*.setx *.set"))return;
  if(const Result<> r=import_settings(s,filename,true);!r)show_error(r.error());
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
  for(int i=0;i<m.nupar;i++)write_real(fp,parameter_value(s,i),m.upar_names[i]);
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
    name=xpp::files::output_name(s.model().this_file,ext);
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
