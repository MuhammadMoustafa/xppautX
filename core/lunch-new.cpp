#include "model.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "lunch-new.h"
#include "parserslow.h"
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
#include "delay_handle.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#define READEM 1
#define VOLTERRA 6

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

/* fn with its spaces removed, from index skip on */
std::string file_name_of(std::string_view fn, size_t skip)
{
  std::string name;
  for(size_t i=skip;i<fn.size();i++)
    if(fn[i]!=' ')name+=fn[i];
  return name;
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
  if(xpp::session().numerics.method==VOLTERRA){
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
  static const char *method[]={"Discrete","Euler","Mod. Euler",
	"Runge-Kutta","Adams","Gear","Volterra","BackEul","QualRK",
         "Stiff","CVode","DoPri5","DoPri8(3)","Rosenbrock","Symplectic"};
  xpp::print(fp,"File: {} \n\n Equations... \n",xpp::model().this_file);
  put_equations(fp);

  xpp::print(fp,"\n\n Numerical parameters ...\n");
  xpp::print(fp,"NJMP={}  NMESH={} METHOD={} EVEC_ITER={} \n",
	 xpp::session().numerics.njmp,xpp::session().numerics.nmesh,method[xpp::session().numerics.method],xpp::session().numerics.evec_iter);
  xpp::print(fp,"BVP_EPS={:g},BVP_TOL={:g},BVP_MAXIT={} \n",
	 xpp::session().numerics.bvp_eps,xpp::session().numerics.bvp_tol,xpp::session().numerics.bvp_maxit);
  xpp::print(fp,"DT={:g} T0={:g} TRANS={:g} TEND={:g} BOUND={:g} DELAY={:g} MaxPts={}\n",
	 xpp::session().numerics.delta_t,xpp::session().numerics.t0,xpp::session().numerics.trans,xpp::session().numerics.tend,xpp::session().numerics.bound,xpp::session().numerics.delay,xpp::session().numerics.max_points);
  xpp::print(fp,"EVEC_ERR={:g}, NEWT_ERR={:g} HMIN={:g} HMAX={:g} TOLER={:g} \n",
	 xpp::session().numerics.evec_err,xpp::session().numerics.newt_err,xpp::session().numerics.hmin,xpp::session().numerics.hmax,xpp::session().numerics.toler);
  const std::string &poivar=ind_to_sym(xpp::session().numerics.poivar);
  xpp::print(fp,"POIMAP={} POIVAR={} POIPLN={:g} POISGN={} \n",
        xpp::session().numerics.poimap,poivar,xpp::session().numerics.poipln,xpp::session().numerics.poisgn);

  xpp::print(fp,"\n\n Delay strings ...\n");
  for(int i=0;i<xpp::model().node;i++)xpp::print(fp,"{}\n",xpp::session().delay_string[i]);
  xpp::print(fp,"\n\n BCs ...\n");
  for(int i=0;i<xpp::model().node;i++)xpp::print(fp,"0={}\n",xpp::model().bcs[i].string.data());
  xpp::print(fp,"\n\n ICs ...\n");
  for(int i=0;i<xpp::model().node+xpp::model().nmarkov;i++)xpp::print(fp,"{}={:.16g}\n",xpp::model().uvar_names[i],xpp::session().last_ic[i]);
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
 if(xpp::session().numerics.method==VOLTERRA){
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
const char *method[]={"Discrete","Euler","Mod. Euler",
        "Runge-Kutta","Adams","Gear","Volterra","BackEul",
                      "Qual RK","Stiff","CVode","DorPrin5","DorPri8(3)",
                      "Rosenbrock","Symplectic"};
const char *pmap[]={"Poincare None","Poincare Section","Poincare Max","Period"};
if(f==READEM&&set_type==1){
  skip_heading_line(fp);
}
if(f!=READEM)
  xpp::print(fp,"# Numerical stuff\n");
io_int(&xpp::session().numerics.njmp,fp,f," nout");
io_int(&xpp::session().numerics.nmesh,fp,f," nullcline mesh");
io_int(&xpp::session().numerics.method,fp,f,method[xpp::session().numerics.method]);
 if(f==READEM){do_meth();alloc_meth();}
io_double(&xpp::session().numerics.tend,fp,f,"total");
io_double(&xpp::session().numerics.delta_t,fp,f,"DeltaT");
io_double(&xpp::session().numerics.t0,fp,f,"T0");
io_double(&xpp::session().numerics.trans,fp,f,"Transient");
io_double(&xpp::session().numerics.bound,fp,f,"Bound");
io_double(&xpp::session().numerics.hmin,fp,f,"DtMin");
io_double(&xpp::session().numerics.hmax,fp,f,"DtMax");
io_double(&xpp::session().numerics.toler,fp,f,"Tolerance");
/* fix stuff concerning the tolerance */
if(f==READEM){
   if(set_type==1)
     io_double(&xpp::session().numerics.atoler,fp,f,"Abs. Tolerance");
   else
     xpp::session().numerics.atoler=xpp::session().numerics.toler*10;
 }
 else 
   io_double(&xpp::session().numerics.atoler,fp,f,"Abs. Tolerance");

io_double(&xpp::session().numerics.delay,fp,f,"Max Delay");
io_int(&xpp::session().numerics.evec_iter,fp,f,"Eigenvector iterates");
io_double(&xpp::session().numerics.evec_err,fp,f,"Eigenvector tolerance");
io_double(&xpp::session().numerics.newt_err,fp,f,"Newton tolerance");
io_double(&xpp::session().numerics.poipln,fp,f,"Poincare plane");
io_double(&xpp::session().numerics.bvp_tol,fp,f,"Boundary value tolerance");
io_double(&xpp::session().numerics.bvp_eps,fp,f,"Boundary value epsilon");
io_int(&xpp::session().numerics.bvp_maxit,fp,f,"Boundary value iterates");
io_int(&xpp::session().numerics.poimap,fp,f,pmap[xpp::session().numerics.poimap]);

io_int(&xpp::session().numerics.poivar,fp,f,"Poincare variable");
io_int(&xpp::session().numerics.poisgn,fp,f,"Poincare sign");
io_int(&xpp::session().numerics.sos,fp,f,"Stop on Section");
io_int(&DelayFlag,fp,f,"Delay flag");
io_double(&xpp::session().data_store.current_time,fp,f,"Current time");
io_double(&xpp::session().integrator.last_time,fp,f,"Last Time");
io_int(&xpp::session().integrator.my_start,fp,f,"xpp::session().integrator.my_start");
io_int(&xpp::session().numerics.inflag,fp,f,"INFLAG");
}
void io_parameter_file(const char *fn,int flag)
{
  /* fn is the command ("!load " and the like, 6 characters) then the
     file name */
  std::string fnx=file_name_of(fn,6);
  if(flag==READEM) {
    xpp::UniqueFile fp=xpp::open_read(fnx.c_str());
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
  xpp::Writer w(fnx.c_str());
  if(!w){
    err_msg("Cannot open file");
    return;
  }
  FILE *fp=w.file();
  io_int(&xpp::model().nupar,fp,flag,"Number params");
  io_parameters(flag,fp);
  time_t ttt=time(0);
  xpp::print(fp,"\n\nFile:{}\n{}",xpp::model().this_file,ctime(&ttt));
  w.commit();
}

void io_ic_file(const char *fn,int flag)
{
  if(flag!=READEM) return;
  std::string fnx=file_name_of(fn,0);
  xpp::TokenReader tr(fnx.c_str());
  if(!tr){
    err_msg("Cannot open file");
    return;
  }
  for(int i=0;i<xpp::model().node;i++){
    if(!tr.read(xpp::session().last_ic[i])){
      err_msg(xpp::format("Expected {} initial conditions but only found {} in {}.",
                          xpp::model().node,i,fn).c_str());
      return;
    }
  }
  /* one number more is one too many */
  double extra;
  if(xpp::model().node>0 && tr.read(extra))
    err_msg(xpp::format("Found more than {} initial conditions in {}.",xpp::model().node,fn).c_str());
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
   std::string formula=xpp::model().bcs[i].string.data();
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
 int j,k;
 io_heading(f,fp,"# Graphics");
 for(j=0;j<3;j++)
   for(k=0;k<3;k++)
     io_double(&(xpp::session().plot_windows.current->rm[k][j]),fp,f,"rm");
 for(j=0;j<MAXPERPLOT;j++){
        io_int(&(xpp::session().plot_windows.current->xv[j]),fp,f," ");
        io_int(&(xpp::session().plot_windows.current->yv[j]),fp,f," ");
        io_int(&(xpp::session().plot_windows.current->zv[j]),fp,f," ");
        io_int(&(xpp::session().plot_windows.current->line[j]),fp,f," ");
        io_int(&(xpp::session().plot_windows.current->color[j]),fp,f," ");
        }

    io_double(&(xpp::session().plot_windows.current->ZPlane),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->ZView),fp,f," ");
    io_int(&(xpp::session().plot_windows.current->PerspFlag),fp,f," ");
    io_int(&(xpp::session().plot_windows.current->ThreeDFlag),fp,f,"3DFlag");
    io_int(&(xpp::session().plot_windows.current->TimeFlag),fp,f,"Timeflag");
    io_int(&(xpp::session().plot_windows.current->ColorFlag),fp,f,"Colorflag");
    io_int(&(xpp::session().plot_windows.current->grtype),fp,f,"Type");
    io_double(&(xpp::session().plot_windows.current->color_scale),fp,f,"color scale");
    io_double(&(xpp::session().plot_windows.current->min_scale),fp,f," minscale");

    io_double(&(xpp::session().plot_windows.current->xmax),fp,f," xmax");
    io_double(&(xpp::session().plot_windows.current->xmin),fp,f," xmin");
    io_double(&(xpp::session().plot_windows.current->ymax),fp,f," ymax");
    io_double(&(xpp::session().plot_windows.current->ymin),fp,f," ymin");
    io_double(&(xpp::session().plot_windows.current->zmax),fp,f," zmax");
    io_double(&(xpp::session().plot_windows.current->zmin),fp,f," zmin");
    io_double(&(xpp::session().plot_windows.current->xbar),fp,f, " ");
    io_double(&(xpp::session().plot_windows.current->dx  ),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->ybar),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->dy  ),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->zbar),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->dz  ),fp,f," ");

    io_double(&(xpp::session().plot_windows.current->Theta),fp,f," Theta");
    io_double(&(xpp::session().plot_windows.current->Phi),fp,f, " Phi");
    io_int(&(xpp::session().plot_windows.current->xshft),fp,f," xshft");
    io_int(&(xpp::session().plot_windows.current->yshft),fp,f," yshft");
    io_int(&(xpp::session().plot_windows.current->zshft),fp,f," zshft");
    io_double(&(xpp::session().plot_windows.current->xlo),fp,f," xlo");
    io_double(&(xpp::session().plot_windows.current->ylo),fp,f," ylo");
    io_double(&(xpp::session().plot_windows.current->oldxlo),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->oldylo),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->xhi),fp,f," xhi");
    io_double(&(xpp::session().plot_windows.current->yhi),fp,f," yhi");
    io_double(&(xpp::session().plot_windows.current->oldxhi),fp,f," ");
    io_double(&(xpp::session().plot_windows.current->oldyhi),fp,f," ");
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


