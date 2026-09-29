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
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#define READEM 1
#define WRITEM 0

static int set_type=0;

/* delay_handle.cpp's and integrate.cpp's (no header declares them yet) */

namespace {

/* An equation line of do_info/dump_eqn: dX/dT=..., X(n+1)=... or X=... */
void put_equation(FILE *fp, int i)
{
  if(i>=xpp::model().node)
    xpp::print(fp,"{}={}\n",xpp::model().uvar_names[i],xpp::model().formulas[i]);
  else if(xpp::session().numerics.method>0)
    xpp::print(fp,"d{}/dT={}\n",xpp::model().uvar_names[i],xpp::model().formulas[i]);
  else
    xpp::print(fp,"{}(n+1)={}\n",xpp::model().uvar_names[i],xpp::model().formulas[i]);
}

/* do_info/dump_eqn's equations, fixed variables and functions */
void put_equations(FILE *fp)
{
  for(int i=0;i<xpp::model().neq;i++)put_equation(fp,i);
  if(xpp::model().fix_var>0){
    xpp::print(fp,"\nwhere ...\n");
    for(int i=0;i<xpp::model().fix_var;i++)
      xpp::print(fp,"{} = {} \n",xpp::model().fixinfo[i].name,xpp::model().fixinfo[i].value);
  }
  if(xpp::model().nfun>0){
    xpp::print(fp,"\nUser-defined functions:\n");
    user_fun_info(fp);
  }
}

/* The parameters four to a line, each after prefix ("" or "%% ") */
void put_parameters(FILE *fp, const char *prefix)
{
  double z;
  for(int i=0;i<xpp::model().nupar;i++){
    get_val(xpp::model().upar_names[i],&z);
    xpp::print(fp,"{}{}={:.16g}   ",prefix,xpp::model().upar_names[i],z);
    if(i%4==3) xpp::print(fp,"\n");
  }
  xpp::print(fp,"\n");
}

/* f==READEM&&set_type==1: skip one line (a "# ..." heading write_lunch
   put there, read but discarded), of any length. */
void skip_heading_line(FILE *fp)
{
  xpp::LineReader lr = xpp::LineReader::attach(fp);
  lr.next();
}

/* the next whole line from fp (any length -- a line longer than a fixed
   buffer is not cut, leaving the rest of it to desync every read after
   it), or nullopt at end of file */
std::optional<std::string> next_line(FILE *fp)
{
  xpp::LineReader lr = xpp::LineReader::attach(fp);
  std::optional<std::string_view> line = lr.next();
  if(!line) return std::nullopt;
  return std::string(*line);
}

/* Reads a set file's settings from fp. ask: report a problem with
   err_msg (File/Read set) rather than a WARN (read_lunch: a session, the
   command line's -setfile). 1 when read. */
int read_set(FILE *fp, bool ask)
{
  int f=READEM,ne,np,temp;
  std::optional<std::string> first=next_line(fp);
  if(!first){
    if(ask) err_msg("Cannot read file");
    else xpp::log(XPP_LOG_WARN, "Set file read failed\n");
    return 0;
  }
  if(!first->empty() && (*first)[0]=='#'){
    set_type=1;
    io_int(&ne,fp,f," ");
  }
  else {
    ne=atoi(first->c_str());
    set_type=0;
  }
  io_int(&np,fp,f," ");
  if(ne!=xpp::model().neq||np!=xpp::model().nupar){
    if(ask) err_msg("Incompatible parameters");
    else xpp::log(XPP_LOG_WARN, "Set file has incompatible parameters\n");
    return 0;
  }
  io_numerics(f,fp);
  if(xpp::session().numerics.method==xpp::method::VOLTERRA){
    io_int(&temp,fp,f," ");
    allocate_volterra(temp,1);
    xpp::session().integrator.my_start=1;
  }
  chk_delay();
  io_exprs(f,fp);
  io_graph(f,fp);
  if(set_type==1){
    dump_transpose_info(fp,f);
    dump_h_stuff(fp,f);
    dump_aplot(fp,f);
    dump_torus(fp,f);
    dump_range(fp,f);
  }
  return 1;
}

} // namespace

void file_inf()
{
  std::string filename=xpp::model().this_file+".pars";
  ping();
  if(!file_selector("Save info",filename,"*.pars*"))return;
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  redraw_params();
  do_info(w.file());
  w.commit();
}

void ps_write_pars(FILE *fp)
{
  xpp::print(fp,"\n %% {} \n %% Parameters ...\n",xpp::model().this_file);
  put_parameters(fp,"%% ");
}

void do_info(FILE *fp)
{
  xpp::Session &s=xpp::session();
  xpp::print(fp,"File: {} \n\n Equations... \n",xpp::model().this_file);
  put_equations(fp);

  xpp::print(fp,"\n\n Numerical parameters ...\n");
  xpp::print(fp,"NJMP={}  NMESH={} METHOD={} EVEC_ITER={} \n",
	 s.numerics.njmp,s.numerics.nmesh,xpp::solver_info(s.numerics.method).name,s.numerics.evec_iter);
  xpp::print(fp,"BVP_EPS={:g},BVP_TOL={:g},BVP_MAXIT={} \n",
	 s.numerics.bvp_eps,s.numerics.bvp_tol,s.numerics.bvp_maxit);
  xpp::print(fp,"DT={:g} T0={:g} TRANS={:g} TEND={:g} BOUND={:g} DELAY={:g} MaxPts={}\n",
	 s.numerics.delta_t,s.numerics.t0,s.numerics.trans,s.numerics.tend,s.numerics.bound,s.numerics.delay,s.numerics.max_points);
  xpp::print(fp,"EVEC_ERR={:g}, NEWT_ERR={:g} HMIN={:g} HMAX={:g} TOLER={:g} \n",
	 s.numerics.evec_err,s.numerics.newt_err,s.numerics.hmin,s.numerics.hmax,s.numerics.toler);
  const std::string &poivar=ind_to_sym(s.numerics.poivar);
  xpp::print(fp,"POIMAP={} POIVAR={} POIPLN={:g} POISGN={} \n",
        s.numerics.poimap,poivar,s.numerics.poipln,s.numerics.poisgn);

  xpp::print(fp,"\n\n Delay strings ...\n");
  for(int i=0;i<xpp::model().node;i++)xpp::print(fp,"{}\n",s.delay_string[i]);
  xpp::print(fp,"\n\n BCs ...\n");
  for(int i=0;i<xpp::model().node;i++)xpp::print(fp,"0={}\n",s.bcs[i].string.data());
  xpp::print(fp,"\n\n ICs ...\n");
  for(int i=0;i<xpp::model().node+xpp::model().nmarkov;i++)xpp::print(fp,"{}={:.16g}\n",xpp::model().uvar_names[i],s.last_ic[i]);
  xpp::print(fp,"\n\n Parameters ...\n");
  put_parameters(fp,"");
}

int read_lunch(FILE *fp)
{
  return read_set(fp,false);
}

void write_lunch(FILE *fp)
{
 int f=0;
 time_t ttt;

 ttt=time(0);
 xpp::print(fp,"## Set file for {} on {}",xpp::model().this_file,ctime(&ttt));
 io_int(&xpp::model().neq,fp,f,"Number of equations and auxiliaries");
 io_int(&xpp::model().nupar,fp,f,"Number of parameters");
 io_numerics(f,fp);
 if(xpp::session().numerics.method==xpp::method::VOLTERRA){
     io_int(&xpp::session().numerics.max_points,fp,f,"Max points for volterra");
     }
   io_exprs(f,fp);
   io_graph(f,fp);
    dump_transpose_info(fp,f);
   dump_h_stuff(fp,f);
   dump_aplot(fp,f);
   dump_torus(fp,f);
   dump_range(fp,f);
   dump_eqn(fp);
}

void do_lunch(int f) /* f=1 to read and 0 to write */
{
  std::string filename=xpp::model().this_file+".set";

  if(f==READEM){
    ping();
    if(!file_selector("Load SET File",filename,"*.set"))return;
    xpp::UniqueFile fp=xpp::open_read(filename.c_str());
    if(!fp){
      err_msg("Cannot open file");
      return;
    }
    read_set(fp.get(),true);
    return;
  }
  if(!file_selector("Save SET File",filename,"*.set"))return;
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return;
  redraw_params();
  write_lunch(w.file());
  w.commit();
}

void dump_eqn(FILE *fp)
{
  xpp::print(fp,"RHS etc ...\n");
  put_equations(fp);
}

void io_numerics(int f, FILE *fp)
{
  xpp::Session &s=xpp::session();
const char *pmap[]={"Poincare None","Poincare Section","Poincare Max","Period"};
if(f==READEM&&set_type==1){
  skip_heading_line(fp);
}
if(f!=READEM)
  xpp::print(fp,"# Numerical stuff\n");
io_int(&s.numerics.njmp,fp,f," nout");
io_int(&s.numerics.nmesh,fp,f," nullcline mesh");
io_int(&s.numerics.method,fp,f,xpp::solver_info(s.numerics.method).set_label);
 if(f==READEM)do_meth();
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

io_int(&s.numerics.poivar,fp,f,"Poincare variable");
io_int(&s.numerics.poisgn,fp,f,"Poincare sign");
io_int(&s.numerics.sos,fp,f,"Stop on Section");
io_int(&s.delay.flag,fp,f,"Delay flag");
io_double(&s.data_store.current_time,fp,f,"Current time");
io_double(&s.integrator.last_time,fp,f,"Last Time");
io_int(&s.integrator.my_start,fp,f,"s.integrator.my_start");
io_int(&s.numerics.inflag,fp,f,"INFLAG");
}
void io_parameter_file(const char *fn,int flag)
{
  /* fn is a plain file name; a filename an interactive caller must still
     pick goes through save_parameter_file/load_parameter_file below,
     which ask for it first */
  if(flag==READEM) {
    xpp::UniqueFile fp=xpp::open_read(fn);
    if(!fp){
      err_msg("Cannot open file");
      return;
    }
    int np;
    io_int(&np,fp.get(),flag," ");
    if(np!=xpp::model().nupar){
      xpp::log(XPP_LOG_DEBUG, "{}\n",np);
      xpp::log(XPP_LOG_DEBUG, "{}\n",xpp::model().nupar);
      err_msg("Incompatible parameters");
      return;
    }
    io_parameters(flag,fp.get());
    fp.reset();
    redo_stuff();
    return;
  }
  xpp::Writer w=open_writer_asking(fn);
  if(!w)return;
  FILE *fp=w.file();
  io_int(&xpp::model().nupar,fp,flag,"Number params");
  io_parameters(flag,fp);
  time_t ttt=time(0);
  xpp::print(fp,"\n\nFile:{}\n{}",xpp::model().this_file,ctime(&ttt));
  w.commit();
}

/* the -icfile / Initialconds/File format: the values alone, one per
   line, one per differential-equation variable, in the model's order --
   exactly `node` of them, as XPPAUT's own io_ic_file always read (the
   Markov chains are not in this file, in XPPAUT or here: docs/manual
   16-quick-reference.md); io_parameter_file's write shares its writer
   and overwrite-ask (open_writer_asking), the read its TokenReader */
void io_ic_file(const char *fn,int flag)
{
  int n=xpp::model().node;
  if(flag==READEM){
    xpp::TokenReader tr(fn);
    if(!tr){
      err_msg("Cannot open file");
      return;
    }
    for(int i=0;i<n;i++){
      if(!tr.read(xpp::session().last_ic[i])){
        err_msg(xpp::format("Expected {} initial conditions but only found {} in {}.",
                            n,i,fn).c_str());
        return;
      }
    }
    /* one number more is one too many */
    double extra;
    if(n>0 && tr.read(extra))
      err_msg(xpp::format("Found more than {} initial conditions in {}.",n,fn).c_str());
    return;
  }
  xpp::Writer w=open_writer_asking(fn);
  if(!w)return;
  FILE *fp=w.file();
  for(int i=0;i<n;i++)
    xpp::print(fp,"{:.16g}\n",xpp::session().last_ic[i]);
  w.commit();
}

namespace {

/* the values panel's Save/Load of .par and .ic (docs/protocol.md
   "values"), shared by the four entry points below: name empty asks for
   one like Save data does (title/wild picking the dialog and the
   extension), given skips the ask; io is io_parameter_file or
   io_ic_file, flag READEM or WRITEM */
void named_value_file(std::string name, const char *title, const char *ext,
                       void (*io)(const char *, int), int flag)
{
  if(name.empty()){
    name=xpp::model().this_file+ext;
    if(!file_selector(title,name,xpp::format("*{}",ext).c_str()))return;
  }
  io(name.c_str(),flag);
}

} // namespace

void save_parameter_file(std::string name)
{
  named_value_file(std::move(name),"Save Parameters",".par",io_parameter_file,WRITEM);
}

void save_ic_file(std::string name)
{
  named_value_file(std::move(name),"Save Initial Conditions",".ic",io_ic_file,WRITEM);
}

void load_parameter_file(std::string name)
{
  named_value_file(std::move(name),"Load Parameters",".par",io_parameter_file,READEM);
}

void load_ic_file(std::string name)
{
  named_value_file(std::move(name),"Load Initial Conditions",".ic",io_ic_file,READEM);
}

void write_values_query(const char *name, bool sets, bool pars, bool ics)
{
  const xpp::Model &m=xpp::model();
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
      w.print("{} {:f}\n",m.uvar_names[i],xpp::session().last_ic[i]);
  }
  w.commit();
}

void io_parameters(int f, FILE *fp)
{
 int i;
 double z;
 for(i=0;i<xpp::model().nupar;i++){
  if(f!=READEM){
    get_val(xpp::model().upar_names[i],&z);
    io_double(&z,fp,f,xpp::model().upar_names[i]);
  }
  else {
    io_double(&z,fp,f," ");
    set_val(xpp::model().upar_names[i],z);

    }
  }
  if(f==READEM) redraw_params();
 }

/* A "# ..." heading: skipped on reading a new-style set file, written
   on writing one */
static void io_heading(int f, FILE *fp, const char *heading)
{
  if(f==READEM){
    if(set_type==1)skip_heading_line(fp);
  }
  else
    xpp::print(fp,"{}\n",heading);
}

void io_exprs(int f, FILE *fp)
{
 int i;
 double z;
 io_heading(f,fp,"# Delays");
 for(i=0;i<xpp::model().node;i++)io_string(xpp::session().delay_string[i],fp,f);
 io_heading(f,fp,"# Bndry conds");
 for(i=0;i<xpp::model().node;i++){
   std::string formula=xpp::session().bcs[i].string.data();
   io_string(formula,fp,f);
   if(f==READEM)set_bc_formula(i,formula);
 }
 io_heading(f,fp,"# Old ICs");
 for(i=0;i<xpp::model().node+xpp::model().nmarkov;i++)io_double(&xpp::session().last_ic[i],fp,f,xpp::model().uvar_names[i]);
 io_heading(f,fp,"# Ending  ICs");
 for(i=0;i<xpp::model().node+xpp::model().nmarkov;i++)io_double(&xpp::session().data_store.current[i],fp,f,xpp::model().uvar_names[i]);
 io_heading(f,fp,"# Parameters");
 for(i=0;i<xpp::model().nupar;i++){
  if(f!=READEM){
    get_val(xpp::model().upar_names[i],&z);
    io_double(&z,fp,f,xpp::model().upar_names[i]);
  }
  else {
    io_double(&z,fp,f," ");
    set_val(xpp::model().upar_names[i],z);
  }
}

 if(f==READEM&&program.interactive){
   xpp_ui.redraw_bcs();
   redraw_ics();
   xpp_ui.redraw_delays();
   redraw_params();
 }
}

void io_graph(int f, FILE *fp)
{
 xpp::Session &s=xpp::session();
 int j,k;
 io_heading(f,fp,"# Graphics");
 for(j=0;j<3;j++)
   for(k=0;k<3;k++)
     io_double(&(s.plot_windows.current->rm[k][j]),fp,f,"rm");
 for(j=0;j<MAXPERPLOT;j++){
        io_int(&(s.plot_windows.current->xv[j]),fp,f," ");
        io_int(&(s.plot_windows.current->yv[j]),fp,f," ");
        io_int(&(s.plot_windows.current->zv[j]),fp,f," ");
        io_int(&(s.plot_windows.current->line[j]),fp,f," ");
        io_int(&(s.plot_windows.current->color[j]),fp,f," ");
        }

    io_double(&(s.plot_windows.current->ZPlane),fp,f," ");
    io_double(&(s.plot_windows.current->ZView),fp,f," ");
    io_int(&(s.plot_windows.current->PerspFlag),fp,f," ");
    io_int(&(s.plot_windows.current->ThreeDFlag),fp,f,"3DFlag");
    io_int(&(s.plot_windows.current->TimeFlag),fp,f,"Timeflag");
    io_int(&(s.plot_windows.current->ColorFlag),fp,f,"Colorflag");
    io_int(&(s.plot_windows.current->grtype),fp,f,"Type");
    io_double(&(s.plot_windows.current->color_scale),fp,f,"color scale");
    io_double(&(s.plot_windows.current->min_scale),fp,f," minscale");

    io_double(&(s.plot_windows.current->xmax),fp,f," xmax");
    io_double(&(s.plot_windows.current->xmin),fp,f," xmin");
    io_double(&(s.plot_windows.current->ymax),fp,f," ymax");
    io_double(&(s.plot_windows.current->ymin),fp,f," ymin");
    io_double(&(s.plot_windows.current->zmax),fp,f," zmax");
    io_double(&(s.plot_windows.current->zmin),fp,f," zmin");
    io_double(&(s.plot_windows.current->xbar),fp,f, " ");
    io_double(&(s.plot_windows.current->dx  ),fp,f," ");
    io_double(&(s.plot_windows.current->ybar),fp,f," ");
    io_double(&(s.plot_windows.current->dy  ),fp,f," ");
    io_double(&(s.plot_windows.current->zbar),fp,f," ");
    io_double(&(s.plot_windows.current->dz  ),fp,f," ");

    io_double(&(s.plot_windows.current->Theta),fp,f," Theta");
    io_double(&(s.plot_windows.current->Phi),fp,f, " Phi");
    io_int(&(s.plot_windows.current->xshft),fp,f," xshft");
    io_int(&(s.plot_windows.current->yshft),fp,f," yshft");
    io_int(&(s.plot_windows.current->zshft),fp,f," zshft");
    io_double(&(s.plot_windows.current->xlo),fp,f," xlo");
    io_double(&(s.plot_windows.current->ylo),fp,f," ylo");
    io_double(&(s.plot_windows.current->oldxlo),fp,f," ");
    io_double(&(s.plot_windows.current->oldylo),fp,f," ");
    io_double(&(s.plot_windows.current->xhi),fp,f," xhi");
    io_double(&(s.plot_windows.current->yhi),fp,f," yhi");
    io_double(&(s.plot_windows.current->oldxhi),fp,f," ");
    io_double(&(s.plot_windows.current->oldyhi),fp,f," ");
    if(f==READEM&&program.interactive)xpp_ui.redraw_graph();
}

void io_int(int *i, FILE *fp, int f, std::string_view ss)
{
 if(f==READEM){
   std::optional<std::string> bob=next_line(fp);
   if(!bob){*i=0;return;}
   *i=atoi(bob->c_str());
 }
 else
 xpp::print(fp,"{}   {}\n",*i,ss);
}

void io_double(double *z, FILE *fp, int f, std::string_view ss)
{
 if(f==READEM){
   std::optional<std::string> bob=next_line(fp);
   if(!bob){*z=0.0;return;}
   *z=atof(bob->c_str());
 }
 else
 xpp::print(fp,"{:.16g}  {}\n",*z,ss);
}

void io_string(std::string &s, FILE *fp, int f)
{
 /* One line per string, read whole whatever its length (CR/LF tolerant),
    so the lines after it stay in step; "" at the end of the file */
 if(f==READEM)
   s=next_line(fp).value_or(std::string());
 else
   xpp::print(fp,"{}\n",s);
}


