#include "xpp_ui.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "lunch-new.h"
#include "parserslow.h"
#include "edit_rhs.h"
#include "browse.h"
#include "ggets.h"
#include "graf_par.h"
#include "volterra2.h"
#include "storage.h"
#include "init_conds.h"

#include "numerics.h"
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "arrayplot.h"
#include <time.h>
#include "xpplim.h"
#include "struct.h"
#include "shoot.h"
#include "load_eqn.h"
#include "adj2.h"
#include "integrate.h"
#include "xpp_batch.h"
#include "many_pops.h"
#include "xpp_globals.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#define READEM 1
#define VOLTERRA 6
#define MAXUFUN 50
#define PARAMBOX 1




 extern BC_STRUCT my_bc[MAXODE];

int set_type=0;

extern FIXINFO fixinfo[MAXODE];
extern int FIX_VAR,NFUN;
 
 extern int NJMP,NMESH,METHOD,NODE,POIMAP,POIVAR,POISGN,SOS,INFLAG,NMarkov;
 extern int NUPAR,NEQ,BVP_MAXIT,EVEC_ITER,DelayFlag,MyStart;
 extern double last_ic[MAXODE],MyData[MAXODE],MyTime,LastTime;
 extern double TEND,DELTA_T,T0,TRANS,BOUND,HMIN,HMAX,TOLER,ATOLER,DELAY;
 extern double POIPLN,EVEC_ERR,NEWT_ERR;
extern double BVP_TOL,BVP_EPS;
extern int MaxPoints;

 extern char upar_names[MAXPAR][XPP_NAME_MAX+1],this_file[XPP_MAX_NAME],delay_string[MAXODE][80];
 extern char uvar_names[MAXODE][XPP_NAME_MAX+1]; 
 extern char *ode_names[MAXODE],*fix_names[MAXODE];
namespace {

/* fprintf's type-checked counterpart: std::format (xpp::format) into fp */
template <class... Args>
void put(FILE *fp, std::format_string<Args...> fmt, Args &&...args)
{
  std::string s = xpp::format(fmt, std::forward<Args>(args)...);
  std::fwrite(s.data(), 1, s.size(), fp);
}

struct FileCloser {
  void operator()(FILE *fp) const noexcept { std::fclose(fp); }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

/* file_selector's dialog writes up to 256 bytes into the buffer it is
   given: ask with name as the default, name the answer on OK */
bool choose_file(const char *title, std::string &name, const char *wild)
{
  std::array<char, XPP_MAX_NAME+10> buf{};
  name.copy(buf.data(), std::min(name.size(), buf.size()-1));
  if(!file_selector(title, buf.data(), wild)) return false;
  name = buf.data();
  return true;
}

/* open_write_file's ask (overwrite?) and error, through a writer that
   replaces fil only on commit */
bool open_writer(xpp::Writer &w, const std::string &fil)
{
  if(!may_write_file(fil.c_str())) return false;
  w = xpp::Writer(fil.c_str());
  if(!w){
    err_msg("Cannot open file");
    return false;
  }
  return true;
}

/* An equation line of do_info/dump_eqn: dX/dT=..., X(n+1)=... or X=... */
void put_equation(FILE *fp, int i)
{
  if(i>=NODE)
    put(fp,"{}={}\n",uvar_names[i],ode_names[i]);
  else if(METHOD>0)
    put(fp,"d{}/dT={}\n",uvar_names[i],ode_names[i]);
  else
    put(fp,"{}(n+1)={}\n",uvar_names[i],ode_names[i]);
}

/* do_info/dump_eqn's equations, fixed variables and functions */
void put_equations(FILE *fp)
{
  for(int i=0;i<NEQ;i++)put_equation(fp,i);
  if(FIX_VAR>0){
    put(fp,"\nwhere ...\n");
    for(int i=0;i<FIX_VAR;i++)
      put(fp,"{} = {} \n",fixinfo[i].name,fixinfo[i].value);
  }
  if(NFUN>0){
    put(fp,"\nUser-defined functions:\n");
    user_fun_info(fp);
  }
}

/* The parameters four to a line, each after prefix ("" or "%% ") */
void put_parameters(FILE *fp, const char *prefix)
{
  double z;
  for(int i=0;i<NUPAR;i++){
    get_val(upar_names[i],&z);
    put(fp,"{}{}={:.16g}   ",prefix,upar_names[i],z);
    if(i%4==3) put(fp,"\n");
  }
  put(fp,"\n");
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
  if(ne!=NEQ||np!=NUPAR){
    if(ask) err_msg("Incompatible parameters");
    else xpp::log(XPP_LOG_WARN, "Set file has incompatible parameters\n");
    return 0;
  }
  io_numerics(f,fp);
  if(METHOD==VOLTERRA){
    io_int(&temp,fp,f," ");
    allocate_volterra(temp,1);
    MyStart=1;
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
  std::string filename=std::string(this_file)+".pars";
  ping();
  if(!choose_file("Save info",filename,"*.pars*"))return;
  xpp::Writer w;
  if(!open_writer(w,filename))return;
  redraw_params();
  do_info(w.file());
  w.commit();
}


void ps_write_pars(FILE *fp)
{
  put(fp,"\n %% {} \n %% Parameters ...\n",this_file);
  put_parameters(fp,"%% ");
}

void do_info(FILE *fp)
{
  static const char *method[]={"Discrete","Euler","Mod. Euler",
	"Runge-Kutta","Adams","Gear","Volterra","BackEul","QualRK",
         "Stiff","CVode","DoPri5","DoPri8(3)","Rosenbrock","Symplectic"};
  put(fp,"File: {} \n\n Equations... \n",this_file);
  put_equations(fp);

  put(fp,"\n\n Numerical parameters ...\n");
  put(fp,"NJMP={}  NMESH={} METHOD={} EVEC_ITER={} \n",
	 NJMP,NMESH,method[METHOD],EVEC_ITER);
  put(fp,"BVP_EPS={:g},BVP_TOL={:g},BVP_MAXIT={} \n",
	 BVP_EPS,BVP_TOL,BVP_MAXIT);
  put(fp,"DT={:g} T0={:g} TRANS={:g} TEND={:g} BOUND={:g} DELAY={:g} MaxPts={}\n",
	 DELTA_T,T0,TRANS,TEND,BOUND,DELAY,MaxPoints);
  put(fp,"EVEC_ERR={:g}, NEWT_ERR={:g} HMIN={:g} HMAX={:g} TOLER={:g} \n",
	 EVEC_ERR,NEWT_ERR,HMIN,HMAX,TOLER);
  const char *poivar=POIVAR==0?"T":uvar_names[POIVAR-1];
  put(fp,"POIMAP={} POIVAR={} POIPLN={:g} POISGN={} \n",
        POIMAP,poivar,POIPLN,POISGN);

  put(fp,"\n\n Delay strings ...\n");
  for(int i=0;i<NODE;i++)put(fp,"{}\n",delay_string[i]);
  put(fp,"\n\n BCs ...\n");
  for(int i=0;i<NODE;i++)put(fp,"0={}\n",my_bc[i].string);
  put(fp,"\n\n ICs ...\n");
  for(int i=0;i<NODE+NMarkov;i++)put(fp,"{}={:.16g}\n",uvar_names[i],last_ic[i]);
  put(fp,"\n\n Parameters ...\n");
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
 put(fp,"## Set file for {} on {}",this_file,ctime(&ttt));
 io_int(&NEQ,fp,f,"Number of equations and auxiliaries");
 io_int(&NUPAR,fp,f,"Number of parameters");
 io_numerics(f,fp);
 if(METHOD==VOLTERRA){
     io_int(&MaxPoints,fp,f,"Max points for volterra");
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
  std::string filename=std::string(this_file)+".set";

  if(f==READEM){
    ping();
    if(!choose_file("Load SET File",filename,"*.set"))return;
    FilePtr fp(std::fopen(filename.c_str(),"r"));
    if(!fp){
      err_msg("Cannot open file");
      return;
    }
    read_set(fp.get(),true);
    return;
  }
  if(!choose_file("Save SET File",filename,"*.set"))return;
  xpp::Writer w;
  if(!open_writer(w,filename))return;
  redraw_params();
  write_lunch(w.file());
  w.commit();
}



void dump_eqn(FILE *fp)
{
  put(fp,"RHS etc ...\n");
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
  put(fp,"# Numerical stuff\n");
io_int(&NJMP,fp,f," nout");
io_int(&NMESH,fp,f," nullcline mesh");
io_int(&METHOD,fp,f,method[METHOD]);
 if(f==READEM){do_meth();alloc_meth();}
io_double(&TEND,fp,f,"total");
io_double(&DELTA_T,fp,f,"DeltaT");
io_double(&T0,fp,f,"T0");
io_double(&TRANS,fp,f,"Transient");
io_double(&BOUND,fp,f,"Bound");
io_double(&HMIN,fp,f,"DtMin");
io_double(&HMAX,fp,f,"DtMax");
io_double(&TOLER,fp,f,"Tolerance");
/* fix stuff concerning the tolerance */
if(f==READEM){
   if(set_type==1)
     io_double(&ATOLER,fp,f,"Abs. Tolerance");
   else
     ATOLER=TOLER*10;
 }
 else 
   io_double(&ATOLER,fp,f,"Abs. Tolerance");

io_double(&DELAY,fp,f,"Max Delay");
io_int(&EVEC_ITER,fp,f,"Eigenvector iterates");
io_double(&EVEC_ERR,fp,f,"Eigenvector tolerance");
io_double(&NEWT_ERR,fp,f,"Newton tolerance");
io_double(&POIPLN,fp,f,"Poincare plane");
io_double(&BVP_TOL,fp,f,"Boundary value tolerance");
io_double(&BVP_EPS,fp,f,"Boundary value epsilon");
io_int(&BVP_MAXIT,fp,f,"Boundary value iterates");
io_int(&POIMAP,fp,f,pmap[POIMAP]);

io_int(&POIVAR,fp,f,"Poincare variable");
io_int(&POISGN,fp,f,"Poincare sign");
io_int(&SOS,fp,f,"Stop on Section");
io_int(&DelayFlag,fp,f,"Delay flag");
io_double(&MyTime,fp,f,"Current time");
io_double(&LastTime,fp,f,"Last Time");
io_int(&MyStart,fp,f,"MyStart");
io_int(&INFLAG,fp,f,"INFLAG");
}
void io_parameter_file(const char *fn,int flag)
{
  /* fn is the command ("!load " and the like, 6 characters) then the
     file name */
  std::string fnx=file_name_of(fn,6);
  if(flag==READEM) {
    FilePtr fp(std::fopen(fnx.c_str(),"r"));
    if(!fp){
      err_msg("Cannot open file");
      return;
    }
    int np;
    io_int(&np,fp.get(),flag," ");
    if(np!=NUPAR){
      xpp::log(XPP_LOG_DEBUG, "{}\n",np);
      xpp::log(XPP_LOG_DEBUG, "{}\n",NUPAR);
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
  io_int(&NUPAR,fp,flag,"Number params");
  io_parameters(flag,fp);
  time_t ttt=time(0);
  put(fp,"\n\nFile:{}\n{}",this_file,ctime(&ttt));
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
  for(int i=0;i<NODE;i++){
    if(!tr.read(last_ic[i])){
      err_msg(xpp::format("Expected {} initial conditions but only found {} in {}.",
                          NODE,i,fn).c_str());
      return;
    }
  }
  /* one number more is one too many */
  double extra;
  if(NODE>0 && tr.read(extra))
    err_msg(xpp::format("Found more than {} initial conditions in {}.",NODE,fn).c_str());
}



void io_parameters(int f, FILE *fp)
{
 int i;
 double z;
 for(i=0;i<NUPAR;i++){
  if(f!=READEM){
    get_val(upar_names[i],&z);
    io_double(&z,fp,f,upar_names[i]);
  }
  else {
    io_double(&z,fp,f," ");
    set_val(upar_names[i],z);

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
    put(fp,"{}\n",heading);
}

void io_exprs(int f, FILE *fp)
{
 int i;
 double z;
 io_heading(f,fp,"# Delays");
 for(i=0;i<NODE;i++)io_string(delay_string[i],sizeof(delay_string[i]),fp,f);
 io_heading(f,fp,"# Bndry conds");
 for(i=0;i<NODE;i++)io_string(my_bc[i].string,256,fp,f);
 io_heading(f,fp,"# Old ICs");
 for(i=0;i<NODE+NMarkov;i++)io_double(&last_ic[i],fp,f,uvar_names[i]);
 io_heading(f,fp,"# Ending  ICs");
 for(i=0;i<NODE+NMarkov;i++)io_double(&MyData[i],fp,f,uvar_names[i]);
 io_heading(f,fp,"# Parameters");
 for(i=0;i<NUPAR;i++){
  if(f!=READEM){
    get_val(upar_names[i],&z);
    io_double(&z,fp,f,upar_names[i]);
  }
  else {
    io_double(&z,fp,f," ");
    set_val(upar_names[i],z);
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
     io_double(&(plot_windows.current->rm[k][j]),fp,f,"rm");
 for(j=0;j<MAXPERPLOT;j++){
        io_int(&(plot_windows.current->xv[j]),fp,f," ");
        io_int(&(plot_windows.current->yv[j]),fp,f," ");
        io_int(&(plot_windows.current->zv[j]),fp,f," ");
        io_int(&(plot_windows.current->line[j]),fp,f," ");
        io_int(&(plot_windows.current->color[j]),fp,f," ");
        }

    io_double(&(plot_windows.current->ZPlane),fp,f," ");
    io_double(&(plot_windows.current->ZView),fp,f," ");
    io_int(&(plot_windows.current->PerspFlag),fp,f," ");
    io_int(&(plot_windows.current->ThreeDFlag),fp,f,"3DFlag");
    io_int(&(plot_windows.current->TimeFlag),fp,f,"Timeflag");
    io_int(&(plot_windows.current->ColorFlag),fp,f,"Colorflag");
    io_int(&(plot_windows.current->grtype),fp,f,"Type");
    io_double(&(plot_windows.current->color_scale),fp,f,"color scale");
    io_double(&(plot_windows.current->min_scale),fp,f," minscale");

    io_double(&(plot_windows.current->xmax),fp,f," xmax");
    io_double(&(plot_windows.current->xmin),fp,f," xmin");
    io_double(&(plot_windows.current->ymax),fp,f," ymax");
    io_double(&(plot_windows.current->ymin),fp,f," ymin");
    io_double(&(plot_windows.current->zmax),fp,f," zmax");
    io_double(&(plot_windows.current->zmin),fp,f," zmin");
    io_double(&(plot_windows.current->xbar),fp,f, " ");
    io_double(&(plot_windows.current->dx  ),fp,f," ");
    io_double(&(plot_windows.current->ybar),fp,f," ");
    io_double(&(plot_windows.current->dy  ),fp,f," ");
    io_double(&(plot_windows.current->zbar),fp,f," ");
    io_double(&(plot_windows.current->dz  ),fp,f," ");

    io_double(&(plot_windows.current->Theta),fp,f," Theta");
    io_double(&(plot_windows.current->Phi),fp,f, " Phi");
    io_int(&(plot_windows.current->xshft),fp,f," xshft");
    io_int(&(plot_windows.current->yshft),fp,f," yshft");
    io_int(&(plot_windows.current->zshft),fp,f," zshft");
    io_double(&(plot_windows.current->xlo),fp,f," xlo");
    io_double(&(plot_windows.current->ylo),fp,f," ylo");
    io_double(&(plot_windows.current->oldxlo),fp,f," ");
    io_double(&(plot_windows.current->oldylo),fp,f," ");
    io_double(&(plot_windows.current->xhi),fp,f," xhi");
    io_double(&(plot_windows.current->yhi),fp,f," yhi");
    io_double(&(plot_windows.current->oldxhi),fp,f," ");
    io_double(&(plot_windows.current->oldyhi),fp,f," ");
    if(f==READEM&&program.interactive)xpp_ui.redraw_graph();
}

void io_int(int *i, FILE *fp, int f, const char *ss)
{
 if(f==READEM){
   std::optional<std::string> bob=next_line(fp);
   if(!bob){*i=0;return;}
   *i=atoi(bob->c_str());
 }
 else
 put(fp,"{}   {}\n",*i,ss);
}

void io_double(double *z, FILE *fp, int f, const char *ss)
{
 if(f==READEM){
   std::optional<std::string> bob=next_line(fp);
   if(!bob){*z=0.0;return;}
   *z=atof(bob->c_str());
 }
 else
 put(fp,"{:.16g}  {}\n",*z,ss);
}

void io_string(char *s, int len, FILE *fp, int f)
{
 /* One line per string. xpp_line_reader reads the whole line whatever its
    length (CR/LF tolerant), so the lines after it stay in step even when
    it is longer than s (len bytes) holds; s gets its start, safely cut
    (xpp_strlcpy: s is the caller's, of len bytes) rather than overflowing. Files
    written with the old 10-character names read the same. */
 if(f==READEM){
   std::optional<std::string> line=next_line(fp);
   if(!line){s[0]=0;return;}
   xpp_strlcpy(s,line->c_str(),static_cast<size_t>(len));
 }
 else
   put(fp,"{}\n",s);
}


    








