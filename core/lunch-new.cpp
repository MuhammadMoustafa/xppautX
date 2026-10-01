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

#include "numerics.h"
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "arrayplot.h"
#include <time.h>
#include "load_eqn.h"
#include "adj2.h"
#include "integrate.h"
#include "xpp_globals.h"
#include "xpp_batch.h"
#include "delay_handle.h"
#include "solver.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

#define READEM 1
#define WRITEM 0

static int set_type=0;

/* delay_handle.cpp's and integrate.cpp's (no header declares them yet) */

namespace {

/* An equation line of do_info/dump_eqn: dX/dT=..., X(n+1)=... or X=... */
void put_equation(const xpp::Session &s, FILE *fp, int i)
{
  if(i>=s.model().node)
    xpp::print(fp,"{}={}\n",s.model().uvar_names[i],s.model().formulas[i]);
  else if(s.numerics.method>0)
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

/* the next whole line from fp (any length -- a line longer than a fixed
   buffer is not cut, leaving the rest of it to desync every read after
   it); at the end of the file SetLineError, the line that is missing
   being what (a value's name, or "") */
std::string next_line(FILE *fp, std::string_view what)
{
  xpp::LineReader lr = xpp::LineReader::attach(fp);
  std::optional<std::string_view> line = lr.next();
  if(!line){
    std::string cause="the file ends here";
    if(!what.empty())cause+=xpp::format(", before {}",what);
    throw SetLineError{xpp::lines_read(fp)+1,std::move(cause)};
  }
  return std::string(*line);
}

/* a value's name as io_int and io_double write it after the value, for a
   message: without the blanks around it */
std::string_view value_name(std::string_view ss)
{
  while(!ss.empty()&&ss.front()==' ')ss.remove_prefix(1);
  while(!ss.empty()&&ss.back()==' ')ss.remove_suffix(1);
  return ss;
}

/* the number line starts with (what io_int and io_double write: the
   number, then blanks and its name), through parse (xpp::parse_int or
   xpp::parse_number); SetLineError when it does not start with one */
template <class T>
void line_number(FILE *fp, std::string_view ss, const char *kind, bool (*parse)(std::string_view, T &), T &value)
{
  const std::string_view name=value_name(ss);
  const std::string line=next_line(fp,name);
  std::string_view text=line;
  while(!text.empty()&&(text.front()==' '||text.front()=='\t'))text.remove_prefix(1);
  const std::string_view first=text.substr(0,text.find_first_of(" \t"));
  if(!parse(first,value))
    throw SetLineError{xpp::lines_read(fp),
                       xpp::format("\"{}\" is not {}{}",line,kind,name.empty()?std::string():xpp::format(" ({})",name))};
}

/* Reads a set file's settings from fp (read_lunch); what is wrong (the
   line, through SetLineError) when it is not one of s's model's */
void read_set(xpp::Session &s, FILE *fp)
{
  int f=READEM,ne,np,temp;
  const std::string first=next_line(fp,"");
  if(!first.empty() && first[0]=='#'){
    set_type=1;
    io_int(&ne,fp,f,"Number of equations and auxiliaries");
  }
  else {
    /* an XPPAUT set file: the number of equations first */
    set_type=0;
    if(!xpp::parse_int(first.substr(0,first.find_first_of(" \t")),ne))
      throw SetLineError{1,xpp::format("\"{}\" is neither \"## Set file\" nor the number of equations",first)};
  }
  io_int(&np,fp,f,"Number of parameters");
  if(ne!=s.model().neq||np!=s.model().nupar)
    throw SetLineError{set_type==1?3:2,
                       xpp::format("it is for {} equations and auxiliaries and {} parameters, the model has {} and {}",
                                   ne,np,s.model().neq,s.model().nupar)};
  io_numerics(s,f,fp);
  if(s.numerics.method==xpp::method::VOLTERRA){
    io_int(&temp,fp,f,"Max points for volterra");
    xpp::allocate_volterra(s,temp,1);
    s.integrator.my_start=1;
  }
  xpp::chk_delay(s);
  io_exprs(s,f,fp);
  io_graph(s,f,fp);
  if(set_type==1){
    xpp::dump_transpose_info(fp,f);
    xpp::dump_h_stuff(s,fp,f);
    dump_aplot(s,fp,f);
    dump_torus(s,fp,f);
    xpp::dump_range(s,fp,f);
  }
}

} // namespace

void file_inf(xpp::Session &s)
{
  std::string filename=s.model().this_file+".pars";
  ping();
  if(!file_selector("Save info",filename,"*.pars*"))return;
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  redraw_params();
  do_info(s,w.file());
  w.commit();
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

std::string SetLineError::text() const
{
  return xpp::format("line {}: {}",line,cause);
}

xpp::Result<> read_lunch(xpp::Session &s, FILE *fp, bool redraw)
{
  try{
    read_set(s,fp);
  }catch(const SetLineError &e){
    return xpp::fail("set file",e.text());
  }
  if(redraw&&program.interactive){
    ui.redraw_bcs();
    redraw_ics();
    ui.redraw_delays();
    redraw_params();
    ui.redraw_graph(s);
  }
  return {};
}

void write_lunch(xpp::Session &s, FILE *fp)
{
 int f=0;
 time_t ttt;

 ttt=time(0);
 xpp::print(fp,"## Set file for {} on {}",s.model().this_file,ctime(&ttt));
 io_int(&s.model().neq,fp,f,"Number of equations and auxiliaries");
 io_int(&s.model().nupar,fp,f,"Number of parameters");
 io_numerics(s,f,fp);
 if(s.numerics.method==xpp::method::VOLTERRA){
     io_int(&s.numerics.max_points,fp,f,"Max points for volterra");
     }
   io_exprs(s,f,fp);
   io_graph(s,f,fp);
    xpp::dump_transpose_info(fp,f);
   xpp::dump_h_stuff(s,fp,f);
   dump_aplot(s,fp,f);
   dump_torus(s,fp,f);
   xpp::dump_range(s,fp,f);
   dump_eqn(s,fp);
}

void do_lunch(xpp::Session &s, int f) /* f=1 to read and 0 to write */
{
  std::string filename=s.model().this_file+".set";

  if(f==READEM){
    ping();
    if(!file_selector("Load SET File",filename,"*.set"))return;
    xpp::UniqueFile fp=xpp::open_read_binary(filename.c_str());
    if(!fp){
      err_reading(filename,"Cannot open file");
      return;
    }
    if(const xpp::Result<> r=read_lunch(s,fp.get(),true);!r)
      err_msg(xpp::format("{}, {}",xpp::files::split_path(filename).second,r.error().what));
    return;
  }
  if(!file_selector("Save SET File",filename,"*.set"))return;
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  redraw_params();
  write_lunch(s,w.file());
  w.commit();
}

void dump_eqn(const xpp::Session &s, FILE *fp)
{
  xpp::print(fp,"RHS etc ...\n");
  put_equations(s,fp);
}

void io_numerics(xpp::Session &s, int f, FILE *fp)
{
const char *pmap[]={"Poincare None","Poincare Section","Poincare Max","Period"};
io_heading(f,fp,"# Numerical stuff");
io_int(&s.numerics.njmp,fp,f," nout");
io_int(&s.numerics.nmesh,fp,f," nullcline mesh");
io_int(&s.numerics.method,fp,f,xpp::solver_info(s.numerics.method).set_label);
if(f==READEM&&(s.numerics.method<0||s.numerics.method>=static_cast<int>(xpp::solvers().size())))
  throw SetLineError{xpp::lines_read(fp),xpp::format("{} is not a method's number",s.numerics.method)};
 if(f==READEM)xpp::do_meth(s);
io_double(&s.numerics.tend,fp,f,"total");
io_double(&s.numerics.delta_t,fp,f,"DeltaT");
io_double(&s.numerics.t0,fp,f,"T0");
io_double(&s.numerics.trans,fp,f,"Transient");
io_double(&s.numerics.bound,fp,f,"Bound");
io_double(&s.numerics.hmin,fp,f,"DtMin");
io_double(&s.numerics.hmax,fp,f,"DtMax");
io_double(&s.numerics.toler,fp,f,"Tolerance");
/* fix stuff concerning the tolerance */
if(f==READEM){
   if(set_type==1)
     io_double(&s.numerics.atoler,fp,f,"Abs. Tolerance");
   else
     s.numerics.atoler=s.numerics.toler*10;
 }
 else 
   io_double(&s.numerics.atoler,fp,f,"Abs. Tolerance");

io_double(&s.numerics.delay,fp,f,"Max Delay");
io_int(&s.numerics.evec_iter,fp,f,"Eigenvector iterates");
io_double(&s.numerics.evec_err,fp,f,"Eigenvector tolerance");
io_double(&s.numerics.newt_err,fp,f,"Newton tolerance");
io_double(&s.numerics.poipln,fp,f,"Poincare plane");
io_double(&s.numerics.bvp_tol,fp,f,"Boundary value tolerance");
io_double(&s.numerics.bvp_eps,fp,f,"Boundary value epsilon");
io_int(&s.numerics.bvp_maxit,fp,f,"Boundary value iterates");
io_int(&s.numerics.poimap,fp,f,pmap[s.numerics.poimap]);
if(f==READEM&&(s.numerics.poimap<0||s.numerics.poimap>=static_cast<int>(std::size(pmap))))
  throw SetLineError{xpp::lines_read(fp),xpp::format("{} is not a Poincare map's number",s.numerics.poimap)};

io_int(&s.numerics.poivar,fp,f,"Poincare variable");
io_int(&s.numerics.poisgn,fp,f,"Poincare sign");
io_int(&s.numerics.sos,fp,f,"Stop on Section");
io_int(&s.delay.flag,fp,f,"Delay flag");
io_double(&s.data_store.current_time,fp,f,"Current time");
io_double(&s.integrator.last_time,fp,f,"Last Time");
io_int(&s.integrator.my_start,fp,f,"s.integrator.my_start");
io_int(&s.numerics.inflag,fp,f,"INFLAG");
}
void io_parameter_file(xpp::Session &s, std::string_view fn,int flag)
{
  xpp::Model &m=s.model();
  /* fn is a plain file name; a filename an interactive caller must still
     pick goes through save_parameter_file/load_parameter_file below,
     which ask for it first */
  if(flag==READEM) {
    xpp::UniqueFile fp=xpp::open_read_binary(fn);
    if(!fp){
      err_reading(fn,"Cannot open file");
      return;
    }
    try{
      int np;
      io_int(&np,fp.get(),flag,"Number params");
      if(np!=m.nupar)
        throw SetLineError{1,xpp::format("it is for {} parameters, the model has {}",np,m.nupar)};
      io_parameters(s,flag,fp.get());
    }catch(const SetLineError &e){
      err_msg(xpp::format("{}, {}",xpp::files::split_path(fn).second,e.text()));
      return;
    }
    fp.reset();
    redo_stuff(s);
    return;
  }
  xpp::Writer w=open_writer_asking(std::string(fn).c_str());
  if(!w)return;
  FILE *fp=w.file();
  io_int(&m.nupar,fp,flag,"Number params");
  io_parameters(s,flag,fp);
  time_t ttt=time(0);
  xpp::print(fp,"\n\nFile:{}\n{}",m.this_file,ctime(&ttt));
  w.commit();
}

/* the -icfile / Initialconds/File format: the values alone, one per
   line, one per differential-equation variable, in the model's order --
   exactly `node` of them, as XPPAUT's own io_ic_file always read (the
   Markov chains are not in this file, in XPPAUT or here: docs/manual
   16-quick-reference.md); io_parameter_file's write shares its writer
   and overwrite-ask (open_writer_asking), the read its TokenReader */
void io_ic_file(xpp::Session &s, std::string_view fn,int flag)
{
  int n=s.model().node;
  if(flag==READEM){
    xpp::TokenReader tr(fn);
    if(!tr){
      err_reading(fn,"Cannot open file");
      return;
    }
    for(int i=0;i<n;i++){
      if(!tr.read(s.last_ic[i])){
        err_msg(xpp::format("Expected {} initial conditions but only found {} in {}.",
                            n,i,fn).c_str());
        return;
      }
    }
    /* one number more is one too many */
    double extra;
    if(n>0 && tr.read(extra))
      err_msg(xpp::format("Found more than {} initial conditions in {}.",n,fn));
    return;
  }
  xpp::Writer w=open_writer_asking(std::string(fn).c_str());
  if(!w)return;
  FILE *fp=w.file();
  for(int i=0;i<n;i++)
    xpp::print(fp,"{:.16g}\n",s.last_ic[i]);
  w.commit();
}

namespace {

/* the values panel's Save/Load of .par and .ic (docs/protocol.md
   "values"), shared by the four functions below: name empty asks for
   one like Save data does (title/wild picking the dialog and the
   extension), given skips the ask; io is io_parameter_file or
   io_ic_file, flag READEM or WRITEM */
void named_value_file(xpp::Session &s, std::string name, const char *title, const char *ext,
                       void (*io)(xpp::Session &, std::string_view, int), int flag)
{
  if(name.empty()){
    name=s.model().this_file+ext;
    if(!file_selector(title,name,xpp::format("*{}",ext)))return;
  }
  io(s,name,flag);
}

} // namespace

void save_parameter_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Save Parameters",".par",io_parameter_file,WRITEM);
}

void save_ic_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Save Initial Conditions",".ic",io_ic_file,WRITEM);
}

void load_parameter_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Load Parameters",".par",io_parameter_file,READEM);
}

void load_ic_file(xpp::Session &s, std::string name)
{
  named_value_file(s,std::move(name),"Load Initial Conditions",".ic",io_ic_file,READEM);
}

void write_values_query(const xpp::Session &s, std::string_view name, bool sets, bool pars, bool ics)
{
  const xpp::Model &m=s.model();
  xpp::Writer w(name);
  if(!w){
    xpp::log(XPP_LOG_WARN, " Unable to open {} to write \n",name);
    return;
  }
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
  w.commit();
}

void io_parameters(xpp::Session &s, int f, FILE *fp)
{
 const xpp::Model &m=s.model();
 int i;
 double z;
 for(i=0;i<m.nupar;i++){
  if(f!=READEM){
    get_val(s,m.upar_names[i],&z);
    io_double(&z,fp,f,m.upar_names[i]);
  }
  else {
    io_double(&z,fp,f," ");
    set_val(s,m.upar_names[i],z);

    }
  }
  if(f==READEM) redraw_params();
 }

void io_heading(int f, FILE *fp, const char *heading)
{
  if(f==READEM){
    if(set_type!=1)return;
    const std::string line=next_line(fp,heading);
    if(!line.starts_with("#"))
      throw SetLineError{xpp::lines_read(fp),xpp::format("\"{}\" is not the heading \"{}\"",line,heading)};
  }
  else
    xpp::print(fp,"{}\n",heading);
}

void io_exprs(xpp::Session &s, int f, FILE *fp)
{
 int i;
 double z;
 io_heading(f,fp,"# Delays");
 for(i=0;i<s.model().node;i++)io_string(s.delay_string[i],fp,f);
 io_heading(f,fp,"# Bndry conds");
 for(i=0;i<s.model().node;i++){
   std::string formula=s.bcs[i].string.data();
   io_string(formula,fp,f);
   if(f==READEM)set_bc_formula(s,i,formula);
 }
 io_heading(f,fp,"# Old ICs");
 for(i=0;i<s.model().node+s.model().nmarkov;i++)io_double(&s.last_ic[i],fp,f,s.model().uvar_names[i]);
 io_heading(f,fp,"# Ending  ICs");
 for(i=0;i<s.model().node+s.model().nmarkov;i++)io_double(&s.data_store.current[i],fp,f,s.model().uvar_names[i]);
 io_heading(f,fp,"# Parameters");
 for(i=0;i<s.model().nupar;i++){
  if(f!=READEM){
    get_val(s,s.model().upar_names[i],&z);
    io_double(&z,fp,f,s.model().upar_names[i]);
  }
  else {
    io_double(&z,fp,f,s.model().upar_names[i]);
    set_val(s,s.model().upar_names[i],z);
  }
}
}

static void io_graph_of(int f, FILE *fp, GRAPH &g)
{
 int j,k;
 io_heading(f,fp,"# Graphics");
 for(j=0;j<3;j++)
   for(k=0;k<3;k++)
     io_double(&(g.rm[k][j]),fp,f,"rm");
 for(j=0;j<MAXPERPLOT;j++){
        io_int(&(g.xv[j]),fp,f," ");
        io_int(&(g.yv[j]),fp,f," ");
        io_int(&(g.zv[j]),fp,f," ");
        io_int(&(g.line[j]),fp,f," ");
        io_int(&(g.color[j]),fp,f," ");
        }

    io_double(&(g.ZPlane),fp,f," ");
    io_double(&(g.ZView),fp,f," ");
    io_int(&(g.PerspFlag),fp,f," ");
    io_int(&(g.ThreeDFlag),fp,f,"3DFlag");
    io_int(&(g.TimeFlag),fp,f,"Timeflag");
    io_int(&(g.ColorFlag),fp,f,"Colorflag");
    io_int(&(g.grtype),fp,f,"Type");
    io_double(&(g.color_scale),fp,f,"color scale");
    io_double(&(g.min_scale),fp,f," minscale");

    io_double(&(g.xmax),fp,f," xmax");
    io_double(&(g.xmin),fp,f," xmin");
    io_double(&(g.ymax),fp,f," ymax");
    io_double(&(g.ymin),fp,f," ymin");
    io_double(&(g.zmax),fp,f," zmax");
    io_double(&(g.zmin),fp,f," zmin");
    io_double(&(g.xbar),fp,f, " ");
    io_double(&(g.dx  ),fp,f," ");
    io_double(&(g.ybar),fp,f," ");
    io_double(&(g.dy  ),fp,f," ");
    io_double(&(g.zbar),fp,f," ");
    io_double(&(g.dz  ),fp,f," ");

    io_double(&(g.Theta),fp,f," Theta");
    io_double(&(g.Phi),fp,f, " Phi");
    io_int(&(g.xshft),fp,f," xshft");
    io_int(&(g.yshft),fp,f," yshft");
    io_int(&(g.zshft),fp,f," zshft");
    io_double(&(g.xlo),fp,f," xlo");
    io_double(&(g.ylo),fp,f," ylo");
    io_double(&(g.oldxlo),fp,f," ");
    io_double(&(g.oldylo),fp,f," ");
    io_double(&(g.xhi),fp,f," xhi");
    io_double(&(g.yhi),fp,f," yhi");
    io_double(&(g.oldxhi),fp,f," ");
    io_double(&(g.oldyhi),fp,f," ");
}

void io_graph(xpp::Session &s, int f, FILE *fp)
{
  io_graph_of(f,fp,*s.plot_windows.current);
}

void write_graph(FILE *fp, GRAPH &g)
{
  io_graph_of(WRITEM,fp,g);
}

void read_graph(FILE *fp, GRAPH &g)
{
  set_type=1; /* its "# Graphics" heading */
  io_graph_of(READEM,fp,g);
}

void io_int(int *i, FILE *fp, int f, std::string_view ss)
{
 if(f==READEM)
   line_number(fp,ss,"a whole number",xpp::parse_int,*i);
 else
 xpp::print(fp,"{}   {}\n",*i,ss);
}

void io_double(double *z, FILE *fp, int f, std::string_view ss)
{
 if(f==READEM)
   line_number(fp,ss,"a number",xpp::parse_number,*z);
 else
 xpp::print(fp,"{:.16g}  {}\n",*z,ss);
}

void io_string(std::string &s, FILE *fp, int f)
{
 /* One line per string, read whole whatever its length (CR/LF tolerant),
    so the lines after it stay in step */
 if(f==READEM)
   s=next_line(fp,"");
 else
   xpp::print(fp,"{}\n",s);
}

} // namespace xpp
