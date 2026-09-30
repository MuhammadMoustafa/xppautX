#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "my_ps.h"
#include "nullcline.h"
#include "colormap.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "markov.h"
#include "expr.h"
#include "xpp_io.h"

#include "xpp_files.h"
#include "xpp_zip.h"
#include "model_files.h"
#include "load_eqn.h"
#include "form_ode.h"
#include "odex.h"

#include "browse.h"
#include "numerics.h"
#include "integrate.h"
#include "adj2.h"
#include "arrayplot.h"
#include "lunch-new.h"
#include "graphics.h"

#include "userbut.h"
#include "volterra2.h"
#include "storage.h"
#include "histogram.h"
#include "tabular.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include "xpp_batch.h"
#include "xpp_log.h"
#include "graf_par.h"
#include "xpp_globals.h"
#include "delay_handle.h"


#define DFNORMAL 1
#define MAXOPT 1000
#define READEM 1


namespace {

/* the @ option lines of .xpprc, each whole, until
   set_internopts_xpprc_and_comline applies them (the model's own are
   xpp::Model's options) */
std::vector<std::string> interopt;

/* how many of the model's options set_internopts has applied: each call
   applies those the parser added since the call before (xpp_load_model's,
   after the parse, applies them all; set_all_vals' own finds none new) */
std::size_t options_applied=0;

/* s1 appended to options, unless they already hold MAXOPT */
void store_option(std::vector<std::string> &options, const char *s1)
{
  if(options.size()>=MAXOPT){
   xpp_log(XPP_LOG_WARN, "to many options set %s ignored\n",s1);
    return;
  }
  options.emplace_back(s1);
}

/* "name=value" split at its first '='; value "" when there is none */
void split_apart(std::string_view bob, std::string &name, std::string &value)
{
  size_t k = bob.find('=');
  if (k == std::string_view::npos) {
    name = bob;
    value.clear();
  } else {
    name = bob.substr(0, k);
    value = bob.substr(k + 1);
  }
}

/* every name=value of an option line (its first token, the @ or $,
   skipped; then tokens split at delims) to set(name, value), those with
   an empty name or value left out; first: the delimiters of the first
   token */
template <class F>
void each_option(std::string_view line, std::string_view first, std::string_view delims, F set)
{
  xpp::Tokens tok(line);
  if (!tok.next(first)) return;
  std::string name, value;
  while (std::optional<std::string_view> t = tok.next(delims)) {
    split_apart(*t, name, value);
    if (!name.empty() && !value.empty()) set(name, value);
  }
}

/* one line of fp, whatever its length ("" at the end) */
std::string read_line(FILE *fp)
{
  xpp::LineReader lr = xpp::LineReader::attach(fp);
  return std::string(lr.next().value_or(std::string_view()));
}

} // namespace



/*   this file has all of the phaseplane parameters defined   
     and created.  All other files should use external stuff
    to use them. (Except eqn forming stuff)
 */




void dump_torus(FILE *fp, int f)
{
  int i;
  if(f==READEM){
    xpp::LineReader lr = xpp::LineReader::attach(fp);
    if(!lr.next())return;
  }
  else
    std::fputs("# Torus information \n",fp);
  io_int(&xpp::session().numerics.torus,fp,f," Torus flag 1=ON");
  io_double(&xpp::session().numerics.tor_period,fp,f,"Torus period");
  if(xpp::session().numerics.torus){
    for(i=0;i<xpp::model().neq;i++)
      io_int(&xpp::session().itor[i],fp,f,xpp::model().uvar_names[i]);
  }
}

void load_eqn()
{
 int okay=0;
 int std=0;
 options_applied=0;
 init_ar_ic();
 for(int i=0;i<MAXODE;i++)
 {
  xpp::session().itor[i]=0;
  xpp::session().delay_string[i]="0.0";
 }
 std::string &this_file=xpp::model().this_file;
 if(this_file=="/dev/stdin")std=1;
 if (xpp::session().got_file==1&&(std==0)&&xpp_files_is_dir(this_file.c_str()))
 {
   xpp_files_change_dir(this_file.c_str());
   make_eqn();
   return;
 }
 /* the model's file: text (a zip or another binary file is refused, its
    bytes never shown as a parse error's line), and for a saved model its
    saved copy (model_files.h) */
 if(xpp::session().got_file==1&&std==0)
 {
   std::string bytes;
   const bool read=xpp::read_model_file(this_file,bytes);
   if(read&&!xpp::is_model_text(bytes))
   {
     xpp::log(XPP_LOG_ERROR, "{} is not a model: {}\n",this_file,
              xpp::zip::is_zip(bytes)?"it is a zip file (an AUTO file is a .autox, a session file a .snapx)":"it is a binary file");
     xpp_model_failed();
   }
   if(!read&&!xpp::model().saved_in.empty())
   {
     xpp::log(XPP_LOG_ERROR, "{} is not saved in {}\n",this_file,xpp::model().saved_in);
     xpp_model_failed();
   }
 }
 /* an .odex model: its own reader, then the same builder (odex.h) */
 if(xpp::session().got_file==1&&std==0&&xpp::odex::is_odex(this_file))
 {
   okay=xpp::odex::load(this_file);
   if(okay==1)return;
 }
 if(xpp::session().got_file==1)
 {
   xpp::UniqueFile fptr=std==1?xpp::open_read(this_file.c_str()):xpp::open_model_file(this_file);
   if(fptr)
   {
     if(std==1)this_file="console";
     okay=get_eqn(fptr.get());
     if(okay==1)return;
   }
 }
 while(okay==0)
 {
   const char *start=getenv("XPPSTART");
   if (start!=NULL && xpp_files_is_dir(start))
     xpp_files_change_dir(start);
   okay=make_eqn();
 }
}

void set_all_vals()
{
 xpp::Session &s=xpp::session();
 int i;
 
 if (s.not_already_set.TIMEPLOT){s.plot_settings.timplot=1;s.not_already_set.TIMEPLOT=0;};
 if (s.not_already_set.FOREVER){s.numerics.forever=0;s.not_already_set.FOREVER=0;};
 if (s.not_already_set.BVP_TOL){s.numerics.bvp_tol=1.e-5;s.not_already_set.BVP_TOL=0;};
 if (s.not_already_set.BVP_EPS){s.numerics.bvp_eps=1.e-5;s.not_already_set.BVP_EPS=0;};
 if (s.not_already_set.BVP_MAXIT){s.numerics.bvp_maxit=20;s.not_already_set.BVP_MAXIT=0;};
 if (s.not_already_set.BVP_FLAG){s.numerics.bvp_flag=0;s.not_already_set.BVP_FLAG=0;};
 if (s.not_already_set.NMESH){s.numerics.nmesh=40;s.not_already_set.NMESH=0;};
 if (s.not_already_set.NOUT){s.numerics.njmp=1;s.not_already_set.NOUT=0;};
 if (s.not_already_set.SOS){s.numerics.sos=0;s.not_already_set.SOS=0;};
 if (s.not_already_set.FFT){s.numerics.fft=0;s.not_already_set.FFT=0;};
 if (s.not_already_set.HIST){s.numerics.hist=0;s.not_already_set.HIST=0;};
 if (s.not_already_set.PltFmtFlag){s.plot_file.plt_fmt_flag=0;s.not_already_set.PltFmtFlag=0;};
 if (s.not_already_set.AXES){s.plot_settings.axes=0;s.not_already_set.AXES=0;};
 if (s.not_already_set.TOLER){s.numerics.toler=0.001;s.not_already_set.TOLER=0;};
 if (s.not_already_set.ATOLER){s.numerics.atoler=0.001;s.not_already_set.ATOLER=0;};
 if (s.not_already_set.MaxEulIter){s.numerics.max_eul_iter=10;s.not_already_set.MaxEulIter=0;}
 if (s.not_already_set.EulTol){s.numerics.eul_tol=1.e-7;s.not_already_set.EulTol=0;};
 if (s.not_already_set.DELAY){s.numerics.delay=0.0;s.not_already_set.DELAY=0;};
 if (s.not_already_set.DTMIN){s.numerics.hmin=1e-12;s.not_already_set.DTMIN=0;};
 if (s.not_already_set.EVEC_ITER){s.numerics.evec_iter=100;s.not_already_set.EVEC_ITER=0;};
 if (s.not_already_set.EVEC_ERR){s.numerics.evec_err=.001;s.not_already_set.EVEC_ERR=0;};
 if (s.not_already_set.NEWT_ERR){s.numerics.newt_err=.001;s.not_already_set.NEWT_ERR=0;};
 if (s.not_already_set.NULL_HERE){s.numerics.null_here=0;s.not_already_set.NULL_HERE=0;};
 s.delay.stab_flag=DFNORMAL;
 if (s.not_already_set.DTMAX){s.numerics.hmax=1.000;s.not_already_set.DTMAX=0;};
 if (s.not_already_set.POIMAP){s.numerics.poimap=0;s.not_already_set.POIMAP=0;};
 if (s.not_already_set.POIVAR){s.numerics.poivar=1;s.not_already_set.POIVAR=0;};
 if (s.not_already_set.POIEXT){s.numerics.poiext=0;s.not_already_set.POIEXT=0;};
 if (s.not_already_set.POISGN){s.numerics.poisgn=1;s.not_already_set.POISGN=0;};
 if (s.not_already_set.POIPLN){s.numerics.poipln=0.0;s.not_already_set.POIPLN=0;};

 s.data_store.rows=0;

 s.numerics.storflag=0;

 s.numerics.inflag=0;
 s.plot_settings.plot_3d=0;
 if (s.not_already_set.METHOD){s.numerics.method=3;s.not_already_set.METHOD=0;};
 if (s.not_already_set.XLO){s.plot_settings.my_xlo=0.0;s.plot_settings.x_3d[0]=s.plot_settings.my_xlo;s.not_already_set.XLO=0;s.not_already_set.XMIN=0;};
 if (s.not_already_set.XHI){s.plot_settings.my_xhi=20.0;s.plot_settings.x_3d[1]=s.plot_settings.my_xhi;s.not_already_set.XHI=0;s.not_already_set.XMAX=0;};
 if (s.not_already_set.YLO){s.plot_settings.my_ylo=-1;s.plot_settings.y_3d[0]=s.plot_settings.my_ylo;s.not_already_set.YLO=0;s.not_already_set.YMIN=0;};
 if (s.not_already_set.YHI){s.plot_settings.my_yhi=1;s.plot_settings.y_3d[0]=s.plot_settings.my_yhi;s.not_already_set.YHI=0;s.not_already_set.YMAX=0;};
 
 if (s.not_already_set.BOUND){s.numerics.bound=100;s.not_already_set.BOUND=0;};
 if (s.not_already_set.MAXSTOR){s.data_store.max_rows=5000;s.not_already_set.MAXSTOR=0;};

 if (s.not_already_set.T0){s.numerics.t0=0.0;s.not_already_set.T0=0;};
 if (s.not_already_set.TRANS){s.numerics.trans=0.0;s.not_already_set.TRANS=0;};
 if (s.not_already_set.DT){s.numerics.delta_t=.05;s.not_already_set.DT=0;};
 
 if (s.not_already_set.XMIN){s.plot_settings.x_3d[0]=-12;s.not_already_set.XMIN=0;s.not_already_set.XLO=0;};
 if (s.not_already_set.XMAX){s.plot_settings.x_3d[1]=12;s.not_already_set.XMAX=0;s.not_already_set.XHI=0;};
 if (s.not_already_set.YMIN){s.plot_settings.y_3d[0]=-12;s.not_already_set.YMIN=0;s.not_already_set.YLO=0;};
 if (s.not_already_set.YMAX){s.plot_settings.y_3d[1]=12;s.not_already_set.YMAX=0;s.not_already_set.YHI=0;};
 if (s.not_already_set.ZMIN){s.plot_settings.z_3d[0]=-12;s.not_already_set.ZMIN=0;};
 if (s.not_already_set.ZMAX){s.plot_settings.z_3d[1]=12;s.not_already_set.ZMAX=0;};
 
 if (s.not_already_set.TEND){s.numerics.tend=20.00;s.not_already_set.TEND=0;};
 if (s.not_already_set.IXPLT){s.plot_settings.ixplt=0;s.not_already_set.IXPLT=0;}
 if (s.not_already_set.IYPLT){s.plot_settings.iyplt=1;s.not_already_set.IYPLT=0;}
 if (s.not_already_set.IZPLT){s.plot_settings.izplt=1;s.not_already_set.IZPLT=0;}
 
 if (s.not_already_set.NPLOT){
   if (xpp::model().neq>2){if(s.not_already_set.IZPLT){s.plot_settings.izplt=2;}}
 s.plot_settings.npltv=1;
 for(i=0;i<10;i++){
   s.plot_settings.ix_plt[i]=s.plot_settings.ixplt;
   s.plot_settings.iy_plt[i]=s.plot_settings.iyplt;
   s.plot_settings.iz_plt[i]=s.plot_settings.izplt;
   s.plot_settings.x_lo[i]=0;
   s.plot_settings.y_lo[i]=-1;
   s.plot_settings.x_hi[i]=20;
   s.plot_settings.y_hi[i]=1;
 }
 s.not_already_set.NPLOT=0;
 }
 /* internal options go here  */
 set_internopts(NULL);

 if(xpp::UniqueFile fp=xpp::open_model_file(xpp::model().options_file))
  read_defaults(fp.get());

 init_range();
 init_trans();
 init_my_aplot();
 init_txtview();

  chk_volterra();  

/*                           */

 if(s.plot_settings.izplt>xpp::model().neq)s.plot_settings.izplt=xpp::model().neq;
 if(s.plot_settings.iyplt>xpp::model().neq)s.plot_settings.iyplt=xpp::model().neq;
 if(s.plot_settings.ixplt==0||s.plot_settings.iyplt==0)
   s.plot_settings.timplot=1;
 else 
   s.plot_settings.timplot=0;
 if(s.plot_settings.x_3d[0]>=s.plot_settings.x_3d[1]){
   s.plot_settings.x_3d[0]=-1;
   s.plot_settings.x_3d[1]=1;
 }
if(s.plot_settings.y_3d[0]>=s.plot_settings.y_3d[1]){
   s.plot_settings.y_3d[0]=-1;
   s.plot_settings.y_3d[1]=1;
 }
if(s.plot_settings.z_3d[0]>=s.plot_settings.z_3d[1]){
   s.plot_settings.z_3d[0]=-1;
   s.plot_settings.z_3d[1]=1;
 }
 if(s.plot_settings.my_xlo>=s.plot_settings.my_xhi){
   s.plot_settings.my_xlo=-2.0;
   s.plot_settings.my_xhi=2.0;
 }
if(s.plot_settings.my_ylo>=s.plot_settings.my_yhi){
   s.plot_settings.my_ylo=-2.0;
   s.plot_settings.my_yhi=2.0;
 }
 if(s.plot_settings.axes<5){
   s.plot_settings.x_3d[0]=s.plot_settings.my_xlo;
   s.plot_settings.y_3d[0]=s.plot_settings.my_ylo;
   s.plot_settings.x_3d[1]=s.plot_settings.my_xhi;
   s.plot_settings.y_3d[1]=s.plot_settings.my_yhi;
 } 
 s.data_store.allocate(s.data_store.max_rows,xpp::model().neq+1);
 if(s.plot_settings.axes>=5)s.plot_settings.plot_3d=1;
 chk_delay(); /* check for delay allocation */
 alloc_h_stuff();

 alloc_v_memory();  /* allocate stuff for volterra equations */
 xpp::start_solver();
 set_initial_values(); /* the initial values given as formulas */
 arr_ic_start(); /* take care of all predefined array ics */

}

void read_defaults(FILE *fp)
{
 xpp::Session &s=xpp::session();
 /* the X11 big and small fonts: read, not kept */
 std::string bob=read_line(fp);
 if (s.not_already_set.BIG_FONT_NAME && xpp::Tokens(bob).next(" "))
	s.not_already_set.BIG_FONT_NAME=0;

 bob=read_line(fp);
 if (s.not_already_set.SMALL_FONT_NAME && xpp::Tokens(bob).next(" "))
	s.not_already_set.SMALL_FONT_NAME=0;

 if (s.not_already_set.PaperWhite){int paper_white; fil_int(fp,&paper_white);s.not_already_set.PaperWhite=0;}; /* X11 only: read, not kept */
 if (s.not_already_set.IXPLT){fil_int(fp,&s.plot_settings.ixplt);s.not_already_set.IXPLT=0;};
 if (s.not_already_set.IYPLT){fil_int(fp,&s.plot_settings.iyplt);s.not_already_set.IYPLT=0;};
 if (s.not_already_set.IZPLT){fil_int(fp,&s.plot_settings.izplt);s.not_already_set.IZPLT=0;};
 if (s.not_already_set.AXES){fil_int(fp,&s.plot_settings.axes);s.not_already_set.PaperWhite=0;};
 if (s.not_already_set.NOUT){fil_int(fp,&s.numerics.njmp);s.not_already_set.NOUT=0;};
 if (s.not_already_set.NMESH){fil_int(fp,&s.numerics.nmesh);s.not_already_set.NMESH=0;};
 if (s.not_already_set.METHOD){fil_int(fp,&s.numerics.method);s.not_already_set.METHOD=0;};

 if (s.not_already_set.TIMEPLOT){fil_int(fp,&s.plot_settings.timplot);s.not_already_set.TIMEPLOT=0;};
 if (s.not_already_set.MAXSTOR){fil_int(fp,&s.data_store.max_rows);s.not_already_set.MAXSTOR=0;};
 if (s.not_already_set.TEND){fil_flt(fp,&s.numerics.tend);s.not_already_set.TEND=0;};
 if (s.not_already_set.DT){fil_flt(fp,&s.numerics.delta_t);s.not_already_set.DT=0;};
 if (s.not_already_set.T0){fil_flt(fp,&s.numerics.t0);s.not_already_set.T0=0;};
 if (s.not_already_set.TRANS){fil_flt(fp,&s.numerics.trans);s.not_already_set.TRANS=0;};
 if (s.not_already_set.BOUND){fil_flt(fp,&s.numerics.bound);s.not_already_set.BOUND=0;};
 if (s.not_already_set.DTMIN){fil_flt(fp,&s.numerics.hmin);s.not_already_set.DTMIN=0;};
 if (s.not_already_set.DTMAX){fil_flt(fp,&s.numerics.hmax);s.not_already_set.DTMIN=0;};
 if (s.not_already_set.TOLER){fil_flt(fp,&s.numerics.toler);s.not_already_set.TOLER=0;};
 if (s.not_already_set.DELAY){fil_flt(fp,&s.numerics.delay);s.not_already_set.DELAY=0;};
 if (s.not_already_set.XLO){fil_flt(fp,&s.plot_settings.my_xlo);s.not_already_set.XLO=0;};
 if (s.not_already_set.XHI){fil_flt(fp,&s.plot_settings.my_xhi);s.not_already_set.XHI=0;};
 if (s.not_already_set.YLO){fil_flt(fp,&s.plot_settings.my_ylo);s.not_already_set.YLO=0;};
 if (s.not_already_set.YHI){fil_flt(fp,&s.plot_settings.my_yhi);s.not_already_set.YHI=0;};

}

void fil_flt(FILE *fpt, double *val)
{
 *val=atof(read_line(fpt).c_str());
}

void fil_int(FILE *fpt, int *val)
{
 *val=atoi(read_line(fpt).c_str());
}

/* here is some new code for internal set files:
   format of the file is a long string of the form:
   { x=y, z=w, q=p , .... }
*/

void add_intern_set(const char *name, const char *does)
{
  std::vector<xpp::Model::InternalSet> &sets=xpp::model().intern_sets;
  if(sets.size()>=MAX_INTERN_SET){
   xpp_log(XPP_LOG_WARN, " %s not added -- too many must be less than %d \n",
	   name,MAX_INTERN_SET);
    return;
  }
  /* "$ " then does without its braces, commas as spaces */
  std::string bob="$ ";
  for(const char *p=does;*p;p++){
    if(*p=='}'||*p=='{')
      continue;
    bob+=*p==','?' ':*p;
  }
  sets.push_back({name,bob});
 xpp_log(XPP_LOG_INFO, " added %s doing %s \n",
	 sets.back().name.c_str(),sets.back().does.c_str());
}

std::string intern_set_default_name()
{
  const std::vector<xpp::Model::InternalSet> &sets=xpp::model().intern_sets;
  for(std::size_t n=1;;n++){
    std::string name=xpp::format("set{}",n);
    bool taken=false;
    for(const auto &s : sets)
      if(xpp::equal_ignoring_case(s.name,name))taken=true;
    if(!taken)return name;
  }
}

std::string intern_set_name_problem(std::string_view name)
{
  if(!xpp::odex::is_name(name)||xpp::odex::is_reserved(name))
    return xpp::format("{} is not a valid name for a set",name);
  for(const auto &s : xpp::model().intern_sets)
    if(xpp::equal_ignoring_case(s.name,name))
      return xpp::format("{} is already a set of the model",name);
  return "";
}

std::string intern_set_line(std::string_view name)
{
  const xpp::Model &m=xpp::model();
  std::string line=xpp::format("set {} {{",name);
  const char *sep="";
  for(int i=0;i<m.nupar;i++){
    double z=0;
    get_val(m.upar_names[i],&z);
    line+=xpp::format("{}{}={}",sep,m.upar_names[i],xpp::number(z));
    sep=",";
  }
  for(int i=0;i<m.node+m.nmarkov;i++){
    line+=xpp::format("{}{}={}",sep,m.uvar_names[i],xpp::number(xpp::session().last_ic[i]));
    sep=",";
  }
  return line+"}";
}

void extract_action(const char *ptr)
{
  each_option(ptr," "," ,;\n",[](const std::string &name,const std::string &value){
    do_intern_set(name.c_str(),value.c_str());
  });
}

void extract_internset(int j)
{
  extract_action(xpp::model().intern_sets[j].does.c_str());
}

void do_intern_set(const char *name1, const char *value)
{
  int i;
  /* convert only drops white space: the name fits name1's length */
  std::string buf(name1);
  convert(name1,buf.data());
  const char *name=buf.c_str();

  i=find_user_name(ICBOX,name);
  if(i>-1){
    xpp::session().last_ic[i]=atof(value);
  }
  else {
    i=find_user_name(PARAMBOX,name);
    if(i>-1){
      set_val(name,atof(value));
    }
    else {
      set_option(name,value,1,NULL);
   }
  }
 do_meth();
}
/*  ODE options stuff  here !!   */

int msc(const char *s1, const char *s2)
{
 /* s2 starts with s1 */
 return std::string_view(s2).starts_with(s1);
}  
  
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set)
{
  std::vector<std::pair<std::string, std::string>> out;
  auto keep=[&out](const std::string &name,const std::string &value){ out.emplace_back(name,value); };
  if(set)each_option(line," "," ,;\n",keep);
  else each_option(line," ,"," ,\n\r",keep);
  return out;
}

void set_internopts(OptionsSet *mask)
{
  const std::vector<std::string> &options=xpp::model().options;
  for(;options_applied<options.size();options_applied++)
    each_option(options[options_applied]," ,"," ,\n\r",[mask](const std::string &name,const std::string &value){
      set_option(name.c_str(),value.c_str(),0,mask);
    });
}

void set_internopts_xpprc_and_comline()
{
  if(interopt.empty())return;
  /* QUIET and LOGFILE first */
  for(const std::string &opt : interopt){
    xpp::Tokens tok(opt);
    if(!tok.next(" ,"))continue;
    std::string name,value;
    while(std::optional<std::string_view> t=tok.next(" ,\n\r")){
      split_apart(*t,name,value);
      name=xpp::upper_case(name);
      if(name=="QUIET"||name=="LOGFILE")
        set_option(name.c_str(),value.c_str(),0,NULL);
    }
  }

  /*We make a BOOLEAN MASK using the current OptionsSet*/
  /*This allows options to be overwritten multiple times within .xpprc
  but prevents overwriting across comline, .xpprc etc.
  */
  OptionsSet mask = xpp::session().not_already_set;
  for(const std::string &opt : interopt)
    each_option(opt," ,"," ,\n\r",[&mask](const std::string &name,const std::string &value){
      set_option(name.c_str(),value.c_str(),0,&mask);
    });

  /*
  We leave a fresh start for options specified in the ODE file.
  */
  interopt.clear();
}

void check_for_xpprc()
{
  const char *home=getenv("HOME");
  if(home==NULL)return;
  xpp::LineReader lr((std::string(home)+"/.xpprc").c_str());
  if(!lr)return;
  while(std::optional<std::string_view> line=lr.next()){
    if(!line->empty()&&(*line)[0]=='@')
      stor_internopts(std::string(*line).c_str());
  }
}

void stor_internopts(const char *s1)
{
  store_option(interopt,s1);
}

int add_model_option(const char *s1)
{
  /* dll_lib= and dll_fun= named a compiled library and its function */
  const char *refused=nullptr;
  each_option(s1," ,"," ,\n\r",[&refused](const std::string &name,const std::string &){
    if(refused)return;
    std::string upper=xpp::upper_case(name);
    if(msc("DLL_LIB",upper.c_str()))refused="dll_lib";
    else if(msc("DLL_FUN",upper.c_str()))refused="dll_fun";
  });
  if(refused)return refuse_compiled_functions(refused);
  store_option(xpp::model().options,s1);
  return 0;
}

void set_option(const char *name, const char *s2, int force, OptionsSet *mask)
{
 xpp::Session &s=xpp::session();
  int i,j,f;
 static constexpr std::string_view mkey="demragvbqsc582y";
 static constexpr std::string_view Mkey="DEMRAGVBQSC582Y";
 /* the option's name is matched upper case: upper-case a copy, not the
    caller's text (a literal from the command line's options) */
 std::string upper(name);
 xpp::to_upper(upper.data());
 const char *s1=upper.c_str();
 if(msc("QUIET",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "QUIET option must be 0 or 1.\n");
	xpp_model_failed(); /* a load fails, else the program ends */
   }
   if (log_settings.quiet_from_command_line==0)/*Will be 1 if -quiet was specified on the command line.*/
   {
   	log_settings.verbose=(atoi(s2)==0);
   }
   return;
 }
 if(msc("LOGFILE",s1)){
   if (log_settings.file_from_command_line==0) /*Will be 1 if -logfile was specified on the command line.*/
   {
      xpp_log_open_file(s2);
   }
   return;
 }
 if(msc("BELL",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "BELL option must be 0 or 1.\n");
	xpp_model_failed(); /* a load fails, else the program ends */
   }
   return; /* X11's bell: checked, not kept */
 }
 if(msc("BUT",s1)){
    add_user_button(s,s2);
    return;
  }
 /* BIGFONT .. HEIGHT and BACK were the X11 window's fonts, colours, image,
    size and paper: still accepted (old .ode and .xpprc files set them), no
    longer stored */
 if((msc("BIGFONT",s1))||(msc("BIG",s1))){
    if ((s.not_already_set.BIG_FONT_NAME||force) || ((mask!=NULL)&&(mask->BIG_FONT_NAME==1)))
    {
	s.not_already_set.BIG_FONT_NAME=0;
    }
    return;
  }
  if((msc("SMALLFONT",s1))||(msc("SMALL",s1))){;
    if ((s.not_already_set.SMALL_FONT_NAME||force) || ((mask!=NULL)&&(mask->SMALL_FONT_NAME==1)))
    {
	s.not_already_set.SMALL_FONT_NAME=0;
    }
    return;
  }
  if(msc("FORECOLOR",s1)){
    if ((s.not_already_set.UserBlack||force) || ((mask!=NULL)&&(mask->UserBlack==1)))
    {
	s.not_already_set.UserBlack=0;
    }
    return;
  }
  if(msc("BACKCOLOR",s1)){
    if ((s.not_already_set.UserWhite||force) || ((mask!=NULL)&&(mask->UserWhite==1)))
    {
	s.not_already_set.UserWhite=0;
    }
    return;
  }
  if(msc("MWCOLOR",s1)){
    if ((s.not_already_set.UserMainWinColor||force) || ((mask!=NULL)&&(mask->UserMainWinColor==1)))
    {
	s.not_already_set.UserMainWinColor=0;
    }
    return;
  }
  if(msc("DWCOLOR",s1)){
    if ((s.not_already_set.UserDrawWinColor||force) || ((mask!=NULL)&&(mask->UserDrawWinColor==1)))
    {
	s.not_already_set.UserDrawWinColor=0;
    }
    return;
  }
  if(msc("GRADS",s1)){
    if ((s.not_already_set.UserGradients||force) || ((mask!=NULL)&&(mask->UserGradients==1)))
    {
	    if(!(msc(s2,"0")||msc(s2,"1")))
	    {
   		 xpp_log(XPP_LOG_ERROR, "GRADS option must be 0 or 1.\n");
		 xpp_model_failed(); /* a load fails, else the program ends */
	    }
	    s.not_already_set.UserGradients=0;
    }
    return;
  }

  if(msc("PLOTFMT",s1)){
    if ((s.not_already_set.PLOTFORMAT||force) || ((mask!=NULL)&&(mask->PLOTFORMAT==1)))
    {
    	s.plot_export.format=s2;
	s.not_already_set.PLOTFORMAT=0;
    }
    return;
  }

  if(msc("BACKIMAGE",s1)){
    if ((s.not_already_set.UserBGBitmap||force) || ((mask!=NULL)&&(mask->UserBGBitmap==1)))
    {
	s.not_already_set.UserBGBitmap=0;
    }
    return;
  }
  if(msc("WIDTH",s1)){
    if ((s.not_already_set.UserMinWidth||force)|| ((mask!=NULL)&&(mask->UserMinWidth==1)))
    {
       s.not_already_set.UserMinWidth=0;
    }
    return;
  }
  if(msc("HEIGHT",s1)){
    if ((s.not_already_set.UserMinHeight||force) || ((mask!=NULL)&&(mask->UserMinHeight==1)))
    {
	 s.not_already_set.UserMinHeight=0;
    }
    return;
  }
  if(msc("YNC",s1)){
    if ((s.not_already_set.YNullColor||force) || ((mask!=NULL)&&(mask->YNullColor==1)))
    {
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   s.nullclines.y_null_color=i;
	  }
	   s.not_already_set.YNullColor=0;
    }
  return;
  }
if(msc("XNC",s1)){
    if ((s.not_already_set.XNullColor||force) || ((mask!=NULL)&&(mask->XNullColor==1)))
    {
	    i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   s.nullclines.x_null_color=i; 
	   s.not_already_set.XNullColor=0;
	  }
	  
    }
  return;
  }

if(msc("SMC",s1)){

    if ((s.not_already_set.StableManifoldColor||force) || ((mask!=NULL)&&(mask->StableManifoldColor==1)))
    {
  
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   s.manifolds.stable_color=i;
	   s.not_already_set.StableManifoldColor=0;
	  }
    }
  return;
  }
if(msc("UMC",s1)){
    if ((s.not_already_set.UnstableManifoldColor||force) || ((mask!=NULL)&&(mask->UnstableManifoldColor==1)))
    {
	    i=atoi(s2);
	    if(i>-1&&i<11)
	    {
	     s.manifolds.unstable_color=i;
	     s.not_already_set.UnstableManifoldColor=0;
	    }
    }
   return;
  }

  if(msc("LT",s1)){
     if ((s.not_already_set.START_LINE_TYPE||force) || ((mask!=NULL)&&(mask->START_LINE_TYPE==1)))
     {
     	
	    i=atoi(s2);
	    if(i<2&&i>-6)
	    {  
	      s.plot_settings.start_line_type=i; 
	      reset_all_line_type(s);
	      s.not_already_set.START_LINE_TYPE=0;
	      }
     }
     return;
  }
  if(msc("SEED",s1)){ 
     if ((s.not_already_set.RandSeed||force) || ((mask!=NULL)&&(mask->RandSeed==1)))
     {
	    i=atoi(s2);
	    if(i>=0){
	      s.numerics.rand_seed=i;
	      nsrand48(s.numerics.rand_seed);  
	      s.not_already_set.RandSeed=0;
	    }
     }
    return;
  }
 if(msc("BACK",s1)){
   if ((s.not_already_set.PaperWhite||force) || ((mask!=NULL)&&(mask->PaperWhite==1)))
   {
	   s.not_already_set.PaperWhite=0;
   }
    return;
  }
 if(msc("COLORMAP",s1)){
     if ((s.not_already_set.COLORMAP||force) || ((mask!=NULL)&&(mask->COLORMAP==1)))
     {
   		i=atoi(s2);
   		if(i<7)custom_color=i;
		s.not_already_set.COLORMAP=0;

     }
   return;
 }
   if(msc("NPLOT",s1)){
     if ((s.not_already_set.NPLOT||force) || ((mask!=NULL)&&(mask->NPLOT==1)))
     {
    	s.plot_settings.npltv=atoi(s2);
	s.not_already_set.NPLOT=0;
     }
    return;
  }

   /* can now initialize several plots */
   if(msc("SIMPLOT",s1)){
     s.plot_windows.simul=1;
     return;
   }
   if(msc("MULTIWIN",s1)){
     s.plot_settings.multi_win=1;
     return;
   }
 for(j=2;j<=8;j++){
      std::string xx=xpp::format("XP{}",j);
      std::string yy=xpp::format("YP{}",j);
      std::string zz=xpp::format("ZP{}",j);
      std::string xxh=xpp::format("XHI{}",j);
      std::string xxl=xpp::format("XLO{}",j);
      std::string yyh=xpp::format("YHI{}",j);
      std::string yyl=xpp::format("YLO{}",j);
    if(msc(xx.c_str(),s1)){
    find_variable(s2,&i);
    if(i>-1)s.plot_settings.ix_plt[j]=i;
    return;
  }
   if(msc(yy.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)s.plot_settings.iy_plt[j]=i;
    return;
  }
   if(msc(zz.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)s.plot_settings.iz_plt[j]=i;
    return;
  }
   if(msc(xxh.c_str(),s1)){
     s.plot_settings.x_hi[j]=atof(s2);
     return;
   }
   if(msc(xxl.c_str(),s1)){
     s.plot_settings.x_lo[j]=atof(s2);
     return;
   }
if(msc(yyh.c_str(),s1)){
     s.plot_settings.y_hi[j]=atof(s2);
     return;
   }
if(msc(yyl.c_str(),s1)){
     s.plot_settings.y_lo[j]=atof(s2);
     return;
   }
 }
   if(msc("XP",s1)){
     if ((s.not_already_set.XP||force) || ((mask!=NULL)&&(mask->XP==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)s.plot_settings.ixplt=i;
	s.not_already_set.XP=0;
	s.not_already_set.IXPLT=0;
     }
    return;
  }
   if(msc("YP",s1)){
     if ((s.not_already_set.YP||force) || ((mask!=NULL)&&(mask->YP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)s.plot_settings.iyplt=i;
	s.not_already_set.YP=0;
	s.not_already_set.IYPLT=0;
     }
    return;
  }
   if(msc("ZP",s1)){
     if ((s.not_already_set.ZP||force) || ((mask!=NULL)&&(mask->ZP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)s.plot_settings.izplt=i;

     	s.not_already_set.ZP=0;
	s.not_already_set.IZPLT=0;
     }
    return;
  }
   if(msc("AXES",s1)){
     if ((s.not_already_set.AXES||force) || ((mask!=NULL)&&(mask->AXES==1)))
     {
	 if(s2[0]=='3')
	 {
	   s.plot_settings.axes=5;
	 }
	 else 
	 {
	   s.plot_settings.axes=0;
	 } 
        
	 s.not_already_set.AXES=0;
     }
    return;
  }

   if(msc("NJMP",s1)){
     if ((s.not_already_set.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
    	s.numerics.njmp=atoi(s2);
        s.not_already_set.NOUT=0;
     }
    return;
  }
  if(msc("NOUT",s1)){
     if ((s.not_already_set.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
      s.numerics.njmp=atoi(s2);
      s.not_already_set.NOUT=0;
     }
    return;
  }
   if(msc("NMESH",s1)){
     if ((s.not_already_set.NMESH||force) || ((mask!=NULL)&&(mask->NMESH==1)))
     {
    	s.numerics.nmesh=atoi(s2);
	s.not_already_set.NMESH=0;
     }
    return;
  }
   if(msc("METH",s1)){
     if ((s.not_already_set.METHOD||force) || ((mask!=NULL)&&(mask->METHOD==1)))
     {
    for(i=0;i<15;i++)
      if(s2[0]==mkey[i]||s2[0]==Mkey[i])
	s.numerics.method=i;
      
       s.not_already_set.METHOD=0;
     }
    return;
  }
   if(msc("VMAXPTS",s1)){
     if ((s.not_already_set.VMAXPTS||force) || ((mask!=NULL)&&(mask->VMAXPTS==1)))
     {
     	s.numerics.max_points=atoi(s2);
	s.not_already_set.VMAXPTS=0;
     
     }
     return;
   }
   if(msc("MAXSTOR",s1)){ 
     if ((s.not_already_set.MAXSTOR||force) || ((mask!=NULL)&&(mask->MAXSTOR==1)))
     {
    	s.data_store.max_rows=atoi(s2);
        s.not_already_set.MAXSTOR=0;
     } 
    return;
  }
   if(msc("TOR_PER",s1)){
     if ((s.not_already_set.TOR_PER||force) || ((mask!=NULL)&&(mask->TOR_PER==1)))
     {
     	s.numerics.tor_period=atof(s2);
     	s.numerics.torus=1;
	s.not_already_set.TOR_PER=0;
     }
     return;
   }
   if(msc("JAC_EPS",s1)){
     if ((s.not_already_set.JAC_EPS||force) || ((mask!=NULL)&&(mask->JAC_EPS==1)))
     {
     	s.numerics.newt_err=atof(s2);
        s.not_already_set.JAC_EPS=0;
     }
     return;
   }
   if(msc("NEWT_TOL",s1)){
     if ((s.not_already_set.NEWT_TOL||force) || ((mask!=NULL)&&(mask->NEWT_TOL==1)))
     {
     	s.numerics.evec_err=atof(s2);
	s.not_already_set.NEWT_TOL=0;
     
     }
     return;
   }
   if(msc("NEWT_ITER",s1)){
     if ((s.not_already_set.NEWT_ITER||force) || ((mask!=NULL)&&(mask->NEWT_ITER==1)))
     {
     	s.numerics.evec_iter=atoi(s2);
	s.not_already_set.NEWT_ITER=0;
     }
     return;
   }
  if(msc("FOLD",s1)){
     if ((s.not_already_set.FOLD||force) || ((mask!=NULL)&&(mask->FOLD==1)))
     {
     find_variable(s2,&i);
     if(i>0){
       s.itor[i-1]=1;
      s.numerics.torus=1;
     }
     
     }
     return;
   }
   if(msc("TOTAL",s1)){
    if ((s.not_already_set.TEND||force) || ((mask!=NULL)&&(mask->TEND==1)))
     {
    	s.numerics.tend=atof(s2);
	s.not_already_set.TEND=0;
    }
    return;
  }
  if(msc("DTMIN",s1)){
     if ((s.not_already_set.DTMIN||force) || ((mask!=NULL)&&(mask->DTMIN==1)))
     {
    	s.numerics.hmin=atof(s2);
         s.not_already_set.DTMIN=0;
     }
    return;
  }
  if(msc("DTMAX",s1)){
     if ((s.not_already_set.DTMAX||force) || ((mask!=NULL)&&(mask->DTMAX==1)))
     {
    	s.numerics.hmax=atof(s2);
	s.not_already_set.DTMAX=0;
      }
    return;
  }
   if(msc("DT",s1)){
     if ((s.not_already_set.DT||force) || ((mask!=NULL)&&(mask->DT==1)))
     {
    	s.numerics.delta_t=atof(s2);
	s.not_already_set.DT=0;
     }
    return;
  }
   if(msc("T0",s1)){
     if ((s.not_already_set.T0||force) || ((mask!=NULL)&&(mask->T0==1)))
     { 
    	s.numerics.t0=atof(s2);
        s.not_already_set.T0=0;
     }
    return;
  }
   if(msc("TRANS",s1)){
     if ((s.not_already_set.TRANS||force) || ((mask!=NULL)&&(mask->TRANS==1)))
     {
     	s.numerics.trans=atof(s2);
        s.not_already_set.TRANS=0;
     }
    return;
  }
   if(msc("BOUND",s1)){
     if ((s.not_already_set.BOUND||force) || ((mask!=NULL)&&(mask->BOUND==1)))
     {
       s.numerics.bound=atof(s2);
       s.not_already_set.BOUND=0;
     }
    return;
  }
   if(msc("ATOL",s1)){
     if ((s.not_already_set.ATOLER||force) || ((mask!=NULL)&&(mask->ATOLER==1)))
     {
     	s.numerics.atoler=atof(s2);
        s.not_already_set.ATOLER=0;
     }
     return;
   }
   if(msc("TOL",s1)){
     if ((s.not_already_set.TOLER||force) || ((mask!=NULL)&&(mask->TOLER==1)))
     {
	s.numerics.toler=atof(s2);
	s.not_already_set.TOLER=0;
     }
    return;
  }
    
   if(msc("DELAY",s1)){
     if ((s.not_already_set.DELAY||force) || ((mask!=NULL)&&(mask->DELAY==1)))
     {
    	s.numerics.delay=atof(s2);
	s.not_already_set.DELAY=0;
     }
    return;
  }
   if(msc("BANDUP",s1)){
     if ((s.not_already_set.BANDUP||force) || ((mask!=NULL)&&(mask->BANDUP==1)))
     {
     	s.numerics.cv_bandflag=1;
     	s.numerics.cv_bandupper=atoi(s2);
     	s.not_already_set.BANDUP=0;
     }
     return;
   }
  if(msc("BANDLO",s1)){
     if ((s.not_already_set.BANDLO||force) || ((mask!=NULL)&&(mask->BANDLO==1)))
     {
     	s.numerics.cv_bandflag=1;
     	s.numerics.cv_bandlower=atoi(s2);
     	s.not_already_set.BANDLO=0;
     }
     return;
   }
  
  if(msc("PHI",s1)){
     if ((s.not_already_set.PHI||force) || ((mask!=NULL)&&(mask->PHI==1)))
     {
    	s.drawing.phi0=atof(s2);
	s.not_already_set.PHI=0;
     }
    return;
  }
   if(msc("THETA",s1)){
     if ((s.not_already_set.THETA||force) || ((mask!=NULL)&&(mask->THETA==1)))
     {
    	s.drawing.theta0=atof(s2);
	s.not_already_set.THETA=0;
     }
    return;
  }
   if(msc("XLO",s1)){
     if ((s.not_already_set.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
     {
    	s.plot_settings.my_xlo=atof(s2);
	s.not_already_set.XLO=0;
     }
    return;
  }
   if(msc("YLO",s1)){
    if ((s.not_already_set.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
    {
    	s.plot_settings.my_ylo=atof(s2);
	s.not_already_set.YLO=0;
    }
    return;
  }
  
   if(msc("XHI",s1)){
    if ((s.not_already_set.XHI||force) || ((mask!=NULL)&&(mask->XHI==1)))
    {
    	s.plot_settings.my_xhi=atof(s2);
        s.not_already_set.XHI=0;
    }
    return;
  }
   if(msc("YHI",s1)){
     if ((s.not_already_set.YHI||force) || ((mask!=NULL)&&(mask->YHI==1)))
     {
    	s.plot_settings.my_yhi=atof(s2);
        s.not_already_set.YHI=0;
     }
    return;
  }
   if(msc("XMAX",s1)){
     if ((s.not_already_set.XMAX||force) || ((mask!=NULL)&&(mask->XMAX==1)))
     {
    	s.plot_settings.x_3d[1]=atof(s2);
	s.not_already_set.XMAX=0;
     
     }
    return;
  }
   if(msc("YMAX",s1)){
     if ((s.not_already_set.YMAX||force) || ((mask!=NULL)&&(mask->YMAX==1)))
     {
        s.plot_settings.y_3d[1]=atof(s2);
	s.not_already_set.YMAX=0;
     }
    return;
  }
   if(msc("ZMAX",s1)){
     if ((s.not_already_set.ZMAX||force) || ((mask!=NULL)&&(mask->ZMAX==1)))
     {
        s.plot_settings.z_3d[1]=atof(s2);
	s.not_already_set.ZMAX=0;
     }
    return;
  }
   if(msc("XMIN",s1)){
     if ((s.not_already_set.XMIN||force) || ((mask!=NULL)&&(mask->XMIN==1)))
     {
        s.plot_settings.x_3d[0]=atof(s2);
	s.not_already_set.XMIN=0; 
	if ((s.not_already_set.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
	{
    	   s.plot_settings.my_xlo=atof(s2);
	   s.not_already_set.XLO=0;
	}
     }
    return;
  }
   if(msc("YMIN",s1)){
     if ((s.not_already_set.YMIN||force) || ((mask!=NULL)&&(mask->YMIN==1)))
     {
    	s.plot_settings.y_3d[0]=atof(s2);
	s.not_already_set.YMIN=0;
	if ((s.not_already_set.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
	{
    	   s.plot_settings.my_ylo=atof(s2);
	   s.not_already_set.YLO=0;
	}
     }
    return;
  }
 if(msc("ZMIN",s1)){
     if ((s.not_already_set.ZMIN||force) || ((mask!=NULL)&&(mask->ZMIN==1)))
     {
    	s.plot_settings.z_3d[0]=atof(s2);
	s.not_already_set.ZMIN=0;
     }
    return;
  }

 if(msc("POIMAP",s1)){
     if ((s.not_already_set.POIMAP||force) || ((mask!=NULL)&&(mask->POIMAP==1)))
     {
   	if(s2[0]=='m'||s2[0]=='M')s.numerics.poimap=2;
   	if(s2[0]=='s'||s2[0]=='S')s.numerics.poimap=1;
   	if(s2[0]=='p'||s2[0]=='P')s.numerics.poimap=3;
   	s.not_already_set.POIMAP=0;
   }
   return;
 }

 if(msc("POIVAR",s1)){
     if ((s.not_already_set.POIVAR||force) || ((mask!=NULL)&&(mask->POIVAR==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)s.numerics.poivar=i;
	
	s.not_already_set.POIVAR=0;
     
     }
    return;
  }
 if(msc("OUTPUT",s1)){
     if ((s.not_already_set.OUTPUT||force) || ((mask!=NULL)&&(mask->OUTPUT==1)))
     {
   	batch_options.out_file=s2;
	s.not_already_set.OUTPUT=0;
     }
   return;
 }
  
 if(msc("POISGN",s1)){
     if ((s.not_already_set.POISGN||force) || ((mask!=NULL)&&(mask->POISGN==1)))
     {
   	s.numerics.poisgn=atoi(s2);
	s.not_already_set.POISGN=0;
     }
   return;
 }
 
 if(msc("POISTOP",s1)){
     if ((s.not_already_set.POISTOP||force) || ((mask!=NULL)&&(mask->POISTOP==1)))
     {
   	s.numerics.sos=atoi(s2);
	s.not_already_set.POISTOP=0;
     }
   return;
 }
 if(msc("STOCH",s1)){
     if ((s.not_already_set.STOCH||force)|| ((mask!=NULL)&&(mask->STOCH==1)))
     {
   	s.stochastic.flag=atoi(s2);
	s.not_already_set.STOCH=0;
     }
   return;
 }
 if(msc("POIPLN",s1)){
     if ((s.not_already_set.POIPLN||force)|| ((mask!=NULL)&&(mask->POIPLN==1)))
     {
   	s.numerics.poipln=atof(s2);
	s.not_already_set.POIPLN=0;
     }
   return;
 }

 if(msc("RANGEOVER",s1)){
     if ((s.not_already_set.RANGEOVER||force)|| ((mask!=NULL)&&(mask->RANGEOVER==1)))
     {
    	s.integrator.range.item=s2;
	s.not_already_set.RANGEOVER=0;
     }

    return;
  }
 if(msc("RANGESTEP",s1)){
     if ((s.not_already_set.RANGESTEP||force)|| ((mask!=NULL)&&(mask->RANGESTEP==1)))
     {
        
   	s.integrator.range.steps=atoi(s2);
	s.not_already_set.RANGESTEP=0;
     }
   return;
 }
  
 if(msc("RANGELOW",s1)){
     if ((s.not_already_set.RANGELOW||force)|| ((mask!=NULL)&&(mask->RANGELOW==1)))
     {
   	s.integrator.range.plow=atof(s2);
   	s.not_already_set.RANGELOW=0;
     }

   return;
 }

 if(msc("RANGEHIGH",s1)){
     if ((s.not_already_set.RANGEHIGH||force)|| ((mask!=NULL)&&(mask->RANGEHIGH==1)))
     {
   	s.integrator.range.phigh=atof(s2);
	s.not_already_set.RANGEHIGH=0;
     }
   return;
 }
 
 if(msc("RANGERESET",s1)){
     if ((s.not_already_set.RANGERESET||force)|| ((mask!=NULL)&&(mask->RANGERESET==1)))
     {
	 if(s2[0]=='y'||s2[0]=='Y')
	 {
	  s.integrator.range.reset=1;
	 }
	 else
	 {
	  s.integrator.range.reset=0;
	 } 
	  s.not_already_set.RANGERESET=0;
     }
  	return;
   }

 if(msc("RANGEOLDIC",s1)){
     if ((s.not_already_set.RANGEOLDIC||force)|| ((mask!=NULL)&&(mask->RANGEOLDIC==1)))
     {
  	if(s2[0]=='y'||s2[0]=='Y')
	{
   		s.integrator.range.oldic=1;
   	}
	else
	{ 
   		s.integrator.range.oldic=0;
	}
	
   	s.not_already_set.RANGEOLDIC=0;
     }
      return;
 }

 if(msc("RANGE",s1)){
     if ((s.not_already_set.RANGE||force)|| ((mask!=NULL)&&(mask->RANGE==1)))
     {
   	batch_options.range=atoi(s2);
	s.not_already_set.RANGE=0;
     }
   return;
 }
 
 if(msc("NTST",s1)){
     if ((s.not_already_set.NTST||force)|| ((mask!=NULL)&&(mask->NTST==1)))
     {
   	s.auto_state.options.ntst=atoi(s2);
	s.not_already_set.NTST=0;
     }
   return;
 }
if(msc("NMAX",s1)){
   if ((s.not_already_set.NMAX||force)|| ((mask!=NULL)&&(mask->NMAX==1)))
   {
   	s.auto_state.options.nmx=atoi(s2);
	s.not_already_set.NMAX=0;
   }
   return;
 }
if(msc("NPR",s1)){
   if ((s.not_already_set.NPR||force)|| ((mask!=NULL)&&(mask->NPR==1)))
   {
   	s.auto_state.options.npr=atoi(s2);
	s.not_already_set.NPR=0;
   }
   return;
 }
 if(msc("NCOL",s1)){
   if ((s.not_already_set.NCOL||force)|| ((mask!=NULL)&&(mask->NCOL==1)))
   {
   	s.auto_state.options.ncol=atoi(s2);
   	s.not_already_set.NCOL=0;
   }
   return;
 }

if(msc("DSMIN",s1)){
   if ((s.not_already_set.DSMIN||force)|| ((mask!=NULL)&&(mask->DSMIN==1)))
   {
   	s.auto_state.options.dsmin=atof(s2);
	s.not_already_set.DSMIN=0;
   }
   return;
 }
if(msc("DSMAX",s1)){
   if ((s.not_already_set.DSMAX||force)|| ((mask!=NULL)&&(mask->DSMAX==1)))
   {
   	s.auto_state.options.dsmax=atof(s2);
   	s.not_already_set.DSMAX=0;
   }
   return;
 }
if(msc("DS",s1)){
    if ((s.not_already_set.DS||force)|| ((mask!=NULL)&&(mask->DS==1)))
    {
   	s.auto_state.options.ds=atof(s2);
	s.not_already_set.DS=0;
    }
 
   return;
 }
if(msc("PARMIN",s1)){
   if ((s.not_already_set.XMAX||force)|| ((mask!=NULL)&&(mask->XMAX==1)))
   {
   	s.auto_state.options.rl0=atof(s2);
	s.not_already_set.XMAX=0;
   }
   return;
 }
if(msc("PARMAX",s1)){
    if ((s.not_already_set.PARMAX||force)|| ((mask!=NULL)&&(mask->PARMAX==1)))
    {
   	s.auto_state.options.rl1=atof(s2);
	s.not_already_set.PARMAX=0;
    }
   return;
 }
if(msc("NORMMIN",s1)){
     if ((s.not_already_set.NORMMIN||force)|| ((mask!=NULL)&&(mask->NORMMIN==1)))
     {
   	s.auto_state.options.a0=atof(s2);
	s.not_already_set.NORMMIN=0;
     }
   return;
 }
if(msc("NORMMAX",s1)){
     if ((s.not_already_set.NORMMAX||force)|| ((mask!=NULL)&&(mask->NORMMAX==1)))
     {
   	s.auto_state.options.a1=atof(s2);
   	s.not_already_set.NORMMAX=0;
     }
   return;
 }
 if(msc("EPSL",s1)){
     if ((s.not_already_set.EPSL||force)|| ((mask!=NULL)&&(mask->EPSL==1)))
     {
   	s.auto_state.options.epsl=atof(s2);
	s.not_already_set.EPSL=0;
     }
   return;
 }

if(msc("EPSU",s1)){
     if ((s.not_already_set.EPSU||force)|| ((mask!=NULL)&&(mask->EPSU==1)))
     {
   	s.auto_state.options.epsu=atof(s2);
	s.not_already_set.EPSU=0;
     }
   return;
 }
if(msc("EPSS",s1)){
     if ((s.not_already_set.EPSS||force)|| ((mask!=NULL)&&(mask->EPSS==1)))
     {
   	s.auto_state.options.epss=atof(s2);
	s.not_already_set.EPSS=0;
     }
   return;
 }
 if(msc("RUNNOW",s1)){
     if ((s.not_already_set.RUNNOW||force)|| ((mask!=NULL)&&(mask->RUNNOW==1)))
     {
   	s.run_immediately=atoi(s2);
	s.not_already_set.RUNNOW=0;
     }
   return;
 }

 if(msc("SEC",s1)){
     if ((s.not_already_set.SEC||force)|| ((mask!=NULL)&&(mask->SEC==1)))
     {
   	s.auto_state.stable_eq_color=atoi(s2);
	s.not_already_set.SEC=0;
     }
   return;
 }
 if(msc("UEC",s1)){
     if ((s.not_already_set.UEC||force)|| ((mask!=NULL)&&(mask->UEC==1)))
     {
   	s.auto_state.unstable_eq_color=atoi(s2);
	s.not_already_set.UEC=0;
     }
   return;
 }
 if(msc("SPC",s1)){
     if ((s.not_already_set.SPC||force)|| ((mask!=NULL)&&(mask->SPC==1)))
     {
   	s.auto_state.stable_po_color=atoi(s2);
	s.not_already_set.SPC=0;
     }
   return;
 }
 if(msc("UPC",s1)){
     if ((s.not_already_set.UPC||force)|| ((mask!=NULL)&&(mask->UPC==1)))
     {
   	s.auto_state.unstable_po_color=atoi(s2);
	s.not_already_set.UPC=0;
     }
   return;
 }

 if(msc("AUTOEVAL",s1)){
     if ((s.not_already_set.AUTOEVAL||force)|| ((mask!=NULL)&&(mask->AUTOEVAL==1)))
     {
   	f=atoi(s2);
   	set_auto_eval_flags(s,f);
	s.not_already_set.AUTOEVAL=0;
    }
   return;
 }
if(msc("AUTOXMAX",s1)){
     if ((s.not_already_set.AUTOXMAX||force)|| ((mask!=NULL)&&(mask->AUTOXMAX==1)))
     {
 	s.auto_state.options.xmax=atof(s2);
	s.not_already_set.AUTOXMAX=0;
     }
 return;
}
if(msc("AUTOYMAX",s1)){
     if ((xpp::session().not_already_set.AUTOYMAX||force)|| ((mask!=NULL)&&(mask->AUTOYMAX==1)))
     {
 		xpp::session().auto_state.options.ymax=atof(s2);
		xpp::session().not_already_set.AUTOYMAX=0;
     }
 return;
}
if(msc("AUTOXMIN",s1)){
     if ((xpp::session().not_already_set.AUTOXMIN||force)|| ((mask!=NULL)&&(mask->AUTOXMIN==1)))
     {
 	xpp::session().auto_state.options.xmin=atof(s2);
	xpp::session().not_already_set.AUTOXMIN=0;
     }
 return;
}
if(msc("AUTOYMIN",s1)){
     if ((xpp::session().not_already_set.AUTOYMIN||force)|| ((mask!=NULL)&&(mask->AUTOYMIN==1)))
     {
 	xpp::session().auto_state.options.ymin=atof(s2);
	xpp::session().not_already_set.AUTOYMIN=0;
     }
 return;
}
if(msc("AUTOVAR",s1)){
     if ((xpp::session().not_already_set.AUTOVAR||force)|| ((mask!=NULL)&&(mask->AUTOVAR==1)))
     {
     	find_variable(s2,&i);
    	if(i>0)xpp::session().auto_state.options.var=i-1;
	xpp::session().not_already_set.AUTOVAR=0;
    }
    return;
  }

/* postscript options */

 if(msc("PS_FONT",s1)){
     if ((xpp::session().not_already_set.PS_FONT||force)|| ((mask!=NULL)&&(mask->PS_FONT==1)))
     {
   	xpp::session().plot_file.ps_font=s2;
	xpp::session().not_already_set.PS_FONT=0;
     }
   return;
 }

if(msc("PS_LW",s1)){
   if ((xpp::session().not_already_set.PS_LW||force)|| ((mask!=NULL)&&(mask->PS_LW==1)))
   {
  	xpp::session().plot_file.ps_lw=atof(s2);
	xpp::session().not_already_set.PS_LW=0;
   }
   return;
 }

if(msc("PS_FSIZE",s1)){
     if ((xpp::session().not_already_set.PS_FSIZE||force)|| ((mask!=NULL)&&(mask->PS_FSIZE==1)))
     {
  	xpp::session().plot_file.ps_font_size=atoi(s2);
	xpp::session().not_already_set.PS_FSIZE=0;
     }
   return;
 }

if(msc("PS_COLOR",s1)){
     if ((xpp::session().not_already_set.PS_COLOR||force)|| ((mask!=NULL)&&(mask->PS_COLOR==1)))
     {
  	xpp::session().plot_file.ps_color_flag=atoi(s2);
  	xpp::session().plot_export.color=xpp::session().plot_file.ps_color_flag;
	xpp::session().not_already_set.PS_COLOR=0;
     }
   return;
 }
if(msc("TUTORIAL",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "TUTORIAL option must be 0 or 1.\n");
	xpp_model_failed(); /* a load fails, else the program ends */
   }
   if ((xpp::session().not_already_set.TUTORIAL||force) || ((mask!=NULL)&&(mask->TUTORIAL==1)))
   {
   	program.tutorial=atoi(s2);
	xpp::session().not_already_set.TUTORIAL=0;
   }
   return;
 }
 if(msc("S1",s1)){
     if ((xpp::session().not_already_set.SLIDER1||force) || ((mask!=NULL)&&(mask->SLIDER1==1)))
     {
	xpp::session().sliders[0].var=s2;
	xpp::session().not_already_set.SLIDER1=0;
     }
    return;
  }

if(msc("S2",s1)){
     if ((xpp::session().not_already_set.SLIDER2||force) || ((mask!=NULL)&&(mask->SLIDER2==1)))
     {
    	xpp::session().sliders[1].var=s2;
	xpp::session().not_already_set.SLIDER2=0;
     }
    return;
   }
 if(msc("S3",s1)){
     if ((xpp::session().not_already_set.SLIDER3||force) || ((mask!=NULL)&&(mask->SLIDER3==1)))
     {	
     	xpp::session().sliders[2].var=s2;
	xpp::session().not_already_set.SLIDER3=0;
     }
    return;
  }
  if(msc("SLO1",s1)){
     if ((xpp::session().not_already_set.SLIDER1LO||force) || ((mask!=NULL)&&(mask->SLIDER1LO==1)))
     {
    	xpp::session().sliders[0].lo=atof(s2);
	xpp::session().not_already_set.SLIDER1LO=0;
     }
    return;
  }

if(msc("SLO2",s1)){
     if ((xpp::session().not_already_set.SLIDER2LO||force) || ((mask!=NULL)&&(mask->SLIDER2LO==1)))
     {
    	xpp::session().sliders[1].lo=atof(s2);
	xpp::session().not_already_set.SLIDER2LO=0;
     }
    return;
   }
 if(msc("SLO3",s1)){
     if ((xpp::session().not_already_set.SLIDER3LO||force) || ((mask!=NULL)&&(mask->SLIDER3LO==1)))
     {
    	xpp::session().sliders[2].lo=atof(s2);
	xpp::session().not_already_set.SLIDER3LO=0;
     }
    return;
  }
 if(msc("SHI1",s1)){
     if ((xpp::session().not_already_set.SLIDER1HI||force) || ((mask!=NULL)&&(mask->SLIDER1HI==1)))
     {
    	xpp::session().sliders[0].hi=atof(s2);
	xpp::session().not_already_set.SLIDER1HI=0;
     }
    return;
  }
 if(msc("SHI2",s1)){
     if ((xpp::session().not_already_set.SLIDER2HI||force) || ((mask!=NULL)&&(mask->SLIDER2HI==1)))
     {
    	xpp::session().sliders[1].hi=atof(s2);
	xpp::session().not_already_set.SLIDER2HI=0;
     }
    return;
   }
 if(msc("SHI3",s1)){
     if ((xpp::session().not_already_set.SLIDER3HI||force) || ((mask!=NULL)&&(mask->SLIDER3HI==1)))
     {
    	xpp::session().sliders[2].hi=atof(s2);
	xpp::session().not_already_set.SLIDER3HI=0;
     }
    return;
 }

 /* postprocessing options
    This is rally only relevant for batch jobs as it 
    writes files then
 */

 if(msc("POSTPROCESS",s1)){
     if ((xpp::session().not_already_set.POSTPROCESS||force) || ((mask!=NULL)&&(mask->POSTPROCESS==1)))
     {
    	xpp::session().histogram.post_process=atoi(s2);
	xpp::session().not_already_set.POSTPROCESS=0;
     }
    return;
   }
   
 if(msc("HISTLO",s1)){
     if ((xpp::session().not_already_set.HISTLO||force) || ((mask!=NULL)&&(mask->HISTLO==1)))
     {
    	xpp::session().histogram.info.xlo=atof(s2);
	xpp::session().not_already_set.HISTLO=0;
     }
    return;
  }

 if(msc("HISTHI",s1)){
     if ((xpp::session().not_already_set.HISTHI||force) || ((mask!=NULL)&&(mask->HISTHI==1)))
     {
    	xpp::session().histogram.info.xhi=atof(s2);
	xpp::session().not_already_set.HISTHI=0;
     }
    return;
  }

 if(msc("HISTBINS",s1)){
     if ((xpp::session().not_already_set.HISTBINS||force) || ((mask!=NULL)&&(mask->HISTBINS==1)))
     {
    	xpp::session().histogram.info.nbins=atoi(s2);
	xpp::session().not_already_set.HISTBINS=0;
     }
    return;
  }

 if(msc("HISTCOL",s1)){
     if ((xpp::session().not_already_set.HISTCOL||force) || ((mask!=NULL)&&(mask->HISTCOL==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) xpp::session().histogram.info.col=i;
	xpp::session().not_already_set.HISTCOL=0;
     }
    return;
  }

 if(msc("HISTLO2",s1)){
     if ((xpp::session().not_already_set.HISTLO2||force) || ((mask!=NULL)&&(mask->HISTLO2==1)))
     {
    	xpp::session().histogram.info.ylo=atof(s2);
	xpp::session().not_already_set.HISTLO2=0;
     }
    return;
  }

 if(msc("HISTHI2",s1)){
     if ((xpp::session().not_already_set.HISTHI2||force) || ((mask!=NULL)&&(mask->HISTHI2==1)))
     {
    	xpp::session().histogram.info.yhi=atof(s2);
	xpp::session().not_already_set.HISTHI2=0;
     }
    return;
  }

 if(msc("HISTBINS2",s1)){
     if ((xpp::session().not_already_set.HISTBINS2||force) || ((mask!=NULL)&&(mask->HISTBINS2==1)))
     {
    	xpp::session().histogram.info.nbins2=atoi(s2);
	xpp::session().not_already_set.HISTBINS2=0;
     }
    return;
  }

 if(msc("HISTCOL2",s1)){
     if ((xpp::session().not_already_set.HISTCOL2||force) || ((mask!=NULL)&&(mask->HISTCOL2==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) xpp::session().histogram.info.col2=i;
	xpp::session().not_already_set.HISTCOL2=0;
     }
    return;
  }

 if(msc("SPECCOL",s1)){
     if ((xpp::session().not_already_set.SPECCOL||force) || ((mask!=NULL)&&(mask->SPECCOL==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) xpp::session().histogram.spec_col=i;
	xpp::session().not_already_set.SPECCOL=0;
     }
    return;
  }

 if(msc("SPECCOL2",s1)){
     if ((xpp::session().not_already_set.SPECCOL2||force) || ((mask!=NULL)&&(mask->SPECCOL2==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) xpp::session().histogram.spec_col2=i;
	xpp::session().not_already_set.SPECCOL2=0;
     }
    return;
  }

 if(msc("SPECWIDTH",s1)){
     if ((xpp::session().not_already_set.SPECWIDTH||force) || ((mask!=NULL)&&(mask->SPECWIDTH==1)))
     {
       xpp::session().histogram.spec_wid=atoi(s2);
	xpp::session().not_already_set.SPECWIDTH=0;
     }
    return;
  }

 if(msc("SPECWIN",s1)){
     if ((xpp::session().not_already_set.SPECWIN||force) || ((mask!=NULL)&&(mask->SPECWIN==1)))
     {
       xpp::session().histogram.spec_win=atoi(s2);
	xpp::session().not_already_set.SPECWIN=0;
     }
    return;
  }

  if(msc("DFGRID",s1)){
     if ((xpp::session().not_already_set.DFGRID||force)|| ((mask!=NULL)&&(mask->DFGRID==1)))
     { 
     	xpp::session().nullclines.df_grid=atoi(s2);
	xpp::session().not_already_set.DFGRID=0;
     }
   return;
 }
  if(msc("DFDRAW",s1)){ 
     if ((xpp::session().not_already_set.DFBATCH||force)|| ((mask!=NULL)&&(mask->DFBATCH==1)))
     { 
     	xpp::session().nullclines.df_batch=atoi(s2);
	xpp::session().not_already_set.DFBATCH=0;
     }
   return;
 }
   if(msc("NCDRAW",s1)){
     if ((xpp::session().not_already_set.NCBATCH||force)|| ((mask!=NULL)&&(mask->NCBATCH==1)))
     { 
     	xpp::session().nullclines.nc_batch=atoi(s2);
	xpp::session().not_already_set.NCBATCH=0;
     }
   return;
   }

   /* colorize customizing !! */
   if(msc("COLORVIA",s1))
     {
       if ((xpp::session().not_already_set.COLORVIA||force)|| ((mask!=NULL)&&(mask->COLORVIA==1)))
       xpp::session().nullclines.color_via=s2;
       	xpp::session().not_already_set.COLORVIA=0;
       return;
     }
   if(msc("COLORIZE",s1))
     {
          if ((xpp::session().not_already_set.COLORIZE||force)|| ((mask!=NULL)&&(mask->COLORIZE==1)))
       xpp::session().nullclines.colorize_flag=atoi(s2);
          	xpp::session().not_already_set.COLORIZE=0;
          return;
     }
   if(msc("COLORLO",s1))
     {
              if ((xpp::session().not_already_set.COLORLO||force)|| ((mask!=NULL)&&(mask->COLORLO==1)))
       xpp::session().nullclines.color_via_lo=atof(s2);
	             	xpp::session().not_already_set.COLORLO=0;
          return;
     }
   if(msc("COLORHI",s1))
     {
              if ((xpp::session().not_already_set.COLORHI||force)|| ((mask!=NULL)&&(mask->COLORHI==1)))
       xpp::session().nullclines.color_via_hi=atof(s2);
              	xpp::session().not_already_set.COLORHI=0;
       return;
     }

xpp_log(XPP_LOG_WARN, "Option %s not recognized\n",s1);
  
}

