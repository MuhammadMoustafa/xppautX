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
#include "my_ps.h"
#include "nullcline.h"
#include "colormap.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "markov.h"
#include "parserslow.h"

#include "xpp_files.h"
#include "load_eqn.h"
#include "form_ode.h"

#include "browse.h"
#include "numerics.h"
#include "integrate.h"
#include "odesol2.h"
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

#define PARAM 1
#define IC 2

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

std::string upper_case(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
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
 if(xpp::session().got_file==1)
 {
   xpp::UniqueFile fptr=xpp::open_read(this_file.c_str());
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
 int i;
 
 if (xpp::session().not_already_set.TIMEPLOT){xpp::session().plot_settings.timplot=1;xpp::session().not_already_set.TIMEPLOT=0;};
 if (xpp::session().not_already_set.FOREVER){xpp::session().numerics.forever=0;xpp::session().not_already_set.FOREVER=0;};
 if (xpp::session().not_already_set.BVP_TOL){xpp::session().numerics.bvp_tol=1.e-5;xpp::session().not_already_set.BVP_TOL=0;};
 if (xpp::session().not_already_set.BVP_EPS){xpp::session().numerics.bvp_eps=1.e-5;xpp::session().not_already_set.BVP_EPS=0;};
 if (xpp::session().not_already_set.BVP_MAXIT){xpp::session().numerics.bvp_maxit=20;xpp::session().not_already_set.BVP_MAXIT=0;};
 if (xpp::session().not_already_set.BVP_FLAG){xpp::session().numerics.bvp_flag=0;xpp::session().not_already_set.BVP_FLAG=0;};
 if (xpp::session().not_already_set.NMESH){xpp::session().numerics.nmesh=40;xpp::session().not_already_set.NMESH=0;};
 if (xpp::session().not_already_set.NOUT){xpp::session().numerics.njmp=1;xpp::session().not_already_set.NOUT=0;};
 if (xpp::session().not_already_set.SOS){xpp::session().numerics.sos=0;xpp::session().not_already_set.SOS=0;};
 if (xpp::session().not_already_set.FFT){xpp::session().numerics.fft=0;xpp::session().not_already_set.FFT=0;};
 if (xpp::session().not_already_set.HIST){xpp::session().numerics.hist=0;xpp::session().not_already_set.HIST=0;};
 if (xpp::session().not_already_set.PltFmtFlag){PltFmtFlag=0;xpp::session().not_already_set.PltFmtFlag=0;};
 if (xpp::session().not_already_set.AXES){xpp::session().plot_settings.axes=0;xpp::session().not_already_set.AXES=0;};
 if (xpp::session().not_already_set.TOLER){xpp::session().numerics.toler=0.001;xpp::session().not_already_set.TOLER=0;};
 if (xpp::session().not_already_set.ATOLER){xpp::session().numerics.atoler=0.001;xpp::session().not_already_set.ATOLER=0;};
 if (xpp::session().not_already_set.MaxEulIter){xpp::session().numerics.max_eul_iter=10;xpp::session().not_already_set.MaxEulIter=0;}
 if (xpp::session().not_already_set.EulTol){xpp::session().numerics.eul_tol=1.e-7;xpp::session().not_already_set.EulTol=0;};
 if (xpp::session().not_already_set.DELAY){xpp::session().numerics.delay=0.0;xpp::session().not_already_set.DELAY=0;};
 if (xpp::session().not_already_set.DTMIN){xpp::session().numerics.hmin=1e-12;xpp::session().not_already_set.DTMIN=0;};
 if (xpp::session().not_already_set.EVEC_ITER){xpp::session().numerics.evec_iter=100;xpp::session().not_already_set.EVEC_ITER=0;};
 if (xpp::session().not_already_set.EVEC_ERR){xpp::session().numerics.evec_err=.001;xpp::session().not_already_set.EVEC_ERR=0;};
 if (xpp::session().not_already_set.NEWT_ERR){xpp::session().numerics.newt_err=.001;xpp::session().not_already_set.NEWT_ERR=0;};
 if (xpp::session().not_already_set.NULL_HERE){xpp::session().numerics.null_here=0;xpp::session().not_already_set.NULL_HERE=0;};
 xpp::session().delay.stab_flag=DFNORMAL;
 if (xpp::session().not_already_set.DTMAX){xpp::session().numerics.hmax=1.000;xpp::session().not_already_set.DTMAX=0;};
 if (xpp::session().not_already_set.POIMAP){xpp::session().numerics.poimap=0;xpp::session().not_already_set.POIMAP=0;};
 if (xpp::session().not_already_set.POIVAR){xpp::session().numerics.poivar=1;xpp::session().not_already_set.POIVAR=0;};
 if (xpp::session().not_already_set.POIEXT){xpp::session().numerics.poiext=0;xpp::session().not_already_set.POIEXT=0;};
 if (xpp::session().not_already_set.POISGN){xpp::session().numerics.poisgn=1;xpp::session().not_already_set.POISGN=0;};
 if (xpp::session().not_already_set.POIPLN){xpp::session().numerics.poipln=0.0;xpp::session().not_already_set.POIPLN=0;};

 xpp::session().data_store.rows=0;

 xpp::session().numerics.storflag=0;

 xpp::session().numerics.inflag=0;
 xpp::session().integrator.solver=rung_kut;
 xpp::session().plot_settings.plot_3d=0;
 if (xpp::session().not_already_set.METHOD){xpp::session().numerics.method=3;xpp::session().not_already_set.METHOD=0;};
 if (xpp::session().not_already_set.XLO){xpp::session().plot_settings.my_xlo=0.0;xpp::session().plot_settings.x_3d[0]=xpp::session().plot_settings.my_xlo;xpp::session().not_already_set.XLO=0;xpp::session().not_already_set.XMIN=0;};
 if (xpp::session().not_already_set.XHI){xpp::session().plot_settings.my_xhi=20.0;xpp::session().plot_settings.x_3d[1]=xpp::session().plot_settings.my_xhi;xpp::session().not_already_set.XHI=0;xpp::session().not_already_set.XMAX=0;};
 if (xpp::session().not_already_set.YLO){xpp::session().plot_settings.my_ylo=-1;xpp::session().plot_settings.y_3d[0]=xpp::session().plot_settings.my_ylo;xpp::session().not_already_set.YLO=0;xpp::session().not_already_set.YMIN=0;};
 if (xpp::session().not_already_set.YHI){xpp::session().plot_settings.my_yhi=1;xpp::session().plot_settings.y_3d[0]=xpp::session().plot_settings.my_yhi;xpp::session().not_already_set.YHI=0;xpp::session().not_already_set.YMAX=0;};
 
 if (xpp::session().not_already_set.BOUND){xpp::session().numerics.bound=100;xpp::session().not_already_set.BOUND=0;};
 if (xpp::session().not_already_set.MAXSTOR){xpp::session().data_store.max_rows=5000;xpp::session().not_already_set.MAXSTOR=0;};

 if (xpp::session().not_already_set.T0){xpp::session().numerics.t0=0.0;xpp::session().not_already_set.T0=0;};
 if (xpp::session().not_already_set.TRANS){xpp::session().numerics.trans=0.0;xpp::session().not_already_set.TRANS=0;};
 if (xpp::session().not_already_set.DT){xpp::session().numerics.delta_t=.05;xpp::session().not_already_set.DT=0;};
 
 if (xpp::session().not_already_set.XMIN){xpp::session().plot_settings.x_3d[0]=-12;xpp::session().not_already_set.XMIN=0;xpp::session().not_already_set.XLO=0;};
 if (xpp::session().not_already_set.XMAX){xpp::session().plot_settings.x_3d[1]=12;xpp::session().not_already_set.XMAX=0;xpp::session().not_already_set.XHI=0;};
 if (xpp::session().not_already_set.YMIN){xpp::session().plot_settings.y_3d[0]=-12;xpp::session().not_already_set.YMIN=0;xpp::session().not_already_set.YLO=0;};
 if (xpp::session().not_already_set.YMAX){xpp::session().plot_settings.y_3d[1]=12;xpp::session().not_already_set.YMAX=0;xpp::session().not_already_set.YHI=0;};
 if (xpp::session().not_already_set.ZMIN){xpp::session().plot_settings.z_3d[0]=-12;xpp::session().not_already_set.ZMIN=0;};
 if (xpp::session().not_already_set.ZMAX){xpp::session().plot_settings.z_3d[1]=12;xpp::session().not_already_set.ZMAX=0;};
 
 if (xpp::session().not_already_set.TEND){xpp::session().numerics.tend=20.00;xpp::session().not_already_set.TEND=0;};
 if (xpp::session().not_already_set.IXPLT){xpp::session().plot_settings.ixplt=0;xpp::session().not_already_set.IXPLT=0;}
 if (xpp::session().not_already_set.IYPLT){xpp::session().plot_settings.iyplt=1;xpp::session().not_already_set.IYPLT=0;}
 if (xpp::session().not_already_set.IZPLT){xpp::session().plot_settings.izplt=1;xpp::session().not_already_set.IZPLT=0;}
 
 if (xpp::session().not_already_set.NPLOT){
   if (xpp::model().neq>2){if(xpp::session().not_already_set.IZPLT){xpp::session().plot_settings.izplt=2;}}
 xpp::session().plot_settings.npltv=1;
 for(i=0;i<10;i++){
   xpp::session().plot_settings.ix_plt[i]=xpp::session().plot_settings.ixplt;
   xpp::session().plot_settings.iy_plt[i]=xpp::session().plot_settings.iyplt;
   xpp::session().plot_settings.iz_plt[i]=xpp::session().plot_settings.izplt;
   xpp::session().plot_settings.x_lo[i]=0;
   xpp::session().plot_settings.y_lo[i]=-1;
   xpp::session().plot_settings.x_hi[i]=20;
   xpp::session().plot_settings.y_hi[i]=1;
 }
 xpp::session().not_already_set.NPLOT=0;
 }
 /* internal options go here  */
 set_internopts(NULL);

 if(xpp::UniqueFile fp=xpp::open_read(xpp::model().options_file.c_str()))
  read_defaults(fp.get());

 init_range();
 init_trans();
 init_my_aplot();
 init_txtview();

  chk_volterra();  

/*                           */

 if(xpp::session().plot_settings.izplt>xpp::model().neq)xpp::session().plot_settings.izplt=xpp::model().neq;
 if(xpp::session().plot_settings.iyplt>xpp::model().neq)xpp::session().plot_settings.iyplt=xpp::model().neq;
 if(xpp::session().plot_settings.ixplt==0||xpp::session().plot_settings.iyplt==0)
   xpp::session().plot_settings.timplot=1;
 else 
   xpp::session().plot_settings.timplot=0;
 if(xpp::session().plot_settings.x_3d[0]>=xpp::session().plot_settings.x_3d[1]){
   xpp::session().plot_settings.x_3d[0]=-1;
   xpp::session().plot_settings.x_3d[1]=1;
 }
if(xpp::session().plot_settings.y_3d[0]>=xpp::session().plot_settings.y_3d[1]){
   xpp::session().plot_settings.y_3d[0]=-1;
   xpp::session().plot_settings.y_3d[1]=1;
 }
if(xpp::session().plot_settings.z_3d[0]>=xpp::session().plot_settings.z_3d[1]){
   xpp::session().plot_settings.z_3d[0]=-1;
   xpp::session().plot_settings.z_3d[1]=1;
 }
 if(xpp::session().plot_settings.my_xlo>=xpp::session().plot_settings.my_xhi){
   xpp::session().plot_settings.my_xlo=-2.0;
   xpp::session().plot_settings.my_xhi=2.0;
 }
if(xpp::session().plot_settings.my_ylo>=xpp::session().plot_settings.my_yhi){
   xpp::session().plot_settings.my_ylo=-2.0;
   xpp::session().plot_settings.my_yhi=2.0;
 }
 if(xpp::session().plot_settings.axes<5){
   xpp::session().plot_settings.x_3d[0]=xpp::session().plot_settings.my_xlo;
   xpp::session().plot_settings.y_3d[0]=xpp::session().plot_settings.my_ylo;
   xpp::session().plot_settings.x_3d[1]=xpp::session().plot_settings.my_xhi;
   xpp::session().plot_settings.y_3d[1]=xpp::session().plot_settings.my_yhi;
 } 
 xpp::session().data_store.allocate(xpp::session().data_store.max_rows,xpp::model().neq+1);
 if(xpp::session().plot_settings.axes>=5)xpp::session().plot_settings.plot_3d=1;
 chk_delay(); /* check for delay allocation */
 alloc_h_stuff();

 alloc_v_memory();  /* allocate stuff for volterra equations */
 alloc_meth();
 arr_ic_start(); /* take care of all predefined array ics */

}

void read_defaults(FILE *fp)
{
 /* the X11 big and small fonts: read, not kept */
 std::string bob=read_line(fp);
 if (xpp::session().not_already_set.BIG_FONT_NAME && xpp::Tokens(bob).next(" "))
	xpp::session().not_already_set.BIG_FONT_NAME=0;

 bob=read_line(fp);
 if (xpp::session().not_already_set.SMALL_FONT_NAME && xpp::Tokens(bob).next(" "))
	xpp::session().not_already_set.SMALL_FONT_NAME=0;

 if (xpp::session().not_already_set.PaperWhite){int paper_white; fil_int(fp,&paper_white);xpp::session().not_already_set.PaperWhite=0;}; /* X11 only: read, not kept */
 if (xpp::session().not_already_set.IXPLT){fil_int(fp,&xpp::session().plot_settings.ixplt);xpp::session().not_already_set.IXPLT=0;};
 if (xpp::session().not_already_set.IYPLT){fil_int(fp,&xpp::session().plot_settings.iyplt);xpp::session().not_already_set.IYPLT=0;};
 if (xpp::session().not_already_set.IZPLT){fil_int(fp,&xpp::session().plot_settings.izplt);xpp::session().not_already_set.IZPLT=0;};
 if (xpp::session().not_already_set.AXES){fil_int(fp,&xpp::session().plot_settings.axes);xpp::session().not_already_set.PaperWhite=0;};
 if (xpp::session().not_already_set.NOUT){fil_int(fp,&xpp::session().numerics.njmp);xpp::session().not_already_set.NOUT=0;};
 if (xpp::session().not_already_set.NMESH){fil_int(fp,&xpp::session().numerics.nmesh);xpp::session().not_already_set.NMESH=0;};
 if (xpp::session().not_already_set.METHOD){fil_int(fp,&xpp::session().numerics.method);xpp::session().not_already_set.METHOD=0;};

 if (xpp::session().not_already_set.TIMEPLOT){fil_int(fp,&xpp::session().plot_settings.timplot);xpp::session().not_already_set.TIMEPLOT=0;};
 if (xpp::session().not_already_set.MAXSTOR){fil_int(fp,&xpp::session().data_store.max_rows);xpp::session().not_already_set.MAXSTOR=0;};
 if (xpp::session().not_already_set.TEND){fil_flt(fp,&xpp::session().numerics.tend);xpp::session().not_already_set.TEND=0;};
 if (xpp::session().not_already_set.DT){fil_flt(fp,&xpp::session().numerics.delta_t);xpp::session().not_already_set.DT=0;};
 if (xpp::session().not_already_set.T0){fil_flt(fp,&xpp::session().numerics.t0);xpp::session().not_already_set.T0=0;};
 if (xpp::session().not_already_set.TRANS){fil_flt(fp,&xpp::session().numerics.trans);xpp::session().not_already_set.TRANS=0;};
 if (xpp::session().not_already_set.BOUND){fil_flt(fp,&xpp::session().numerics.bound);xpp::session().not_already_set.BOUND=0;};
 if (xpp::session().not_already_set.DTMIN){fil_flt(fp,&xpp::session().numerics.hmin);xpp::session().not_already_set.DTMIN=0;};
 if (xpp::session().not_already_set.DTMAX){fil_flt(fp,&xpp::session().numerics.hmax);xpp::session().not_already_set.DTMIN=0;};
 if (xpp::session().not_already_set.TOLER){fil_flt(fp,&xpp::session().numerics.toler);xpp::session().not_already_set.TOLER=0;};
 if (xpp::session().not_already_set.DELAY){fil_flt(fp,&xpp::session().numerics.delay);xpp::session().not_already_set.DELAY=0;};
 if (xpp::session().not_already_set.XLO){fil_flt(fp,&xpp::session().plot_settings.my_xlo);xpp::session().not_already_set.XLO=0;};
 if (xpp::session().not_already_set.XHI){fil_flt(fp,&xpp::session().plot_settings.my_xhi);xpp::session().not_already_set.XHI=0;};
 if (xpp::session().not_already_set.YLO){fil_flt(fp,&xpp::session().plot_settings.my_ylo);xpp::session().not_already_set.YLO=0;};
 if (xpp::session().not_already_set.YHI){fil_flt(fp,&xpp::session().plot_settings.my_yhi);xpp::session().not_already_set.YHI=0;};

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

  i=find_user_name(IC,name);
  if(i>-1){
    xpp::session().last_ic[i]=atof(value);
  }
  else {
    i=find_user_name(PARAM,name);
    if(i>-1){
      set_val(name,atof(value));
    }
    else {
      set_option(name,value,1,NULL);
   }
  }
 alloc_meth();
 do_meth();
}
/*  ODE options stuff  here !!   */

int msc(const char *s1, const char *s2)
{
 /* s2 starts with s1 */
 return std::string_view(s2).starts_with(s1);
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
      name=upper_case(name);
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
    std::string upper=upper_case(name);
    if(msc("DLL_LIB",upper.c_str()))refused="dll_lib";
    else if(msc("DLL_FUN",upper.c_str()))refused="dll_fun";
  });
  if(refused)return refuse_compiled_functions(refused);
  store_option(xpp::model().options,s1);
  return 0;
}

void set_option(const char *name, const char *s2, int force, OptionsSet *mask)
{
  int i,j,f;
 static constexpr std::string_view mkey="demragvbqsc582y";
 static constexpr std::string_view Mkey="DEMRAGVBQSC582Y";
 /* the option's name is matched upper case: upper-case a copy, not the
    caller's text (a literal from the command line's options) */
 std::string upper(name);
 strupr(upper.data());
 const char *s1=upper.c_str();
 if(msc("QUIET",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "QUIET option must be 0 or 1.\n");
	exit(-1);
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
	exit(-1);
   }
   return; /* X11's bell: checked, not kept */
 }
 if(msc("BUT",s1)){
    add_user_button(s2);
    return;
  }
 /* BIGFONT .. HEIGHT and BACK were the X11 window's fonts, colours, image,
    size and paper: still accepted (old .ode and .xpprc files set them), no
    longer stored */
 if((msc("BIGFONT",s1))||(msc("BIG",s1))){
    if ((xpp::session().not_already_set.BIG_FONT_NAME||force) || ((mask!=NULL)&&(mask->BIG_FONT_NAME==1)))
    {
	xpp::session().not_already_set.BIG_FONT_NAME=0;
    }
    return;
  }
  if((msc("SMALLFONT",s1))||(msc("SMALL",s1))){;
    if ((xpp::session().not_already_set.SMALL_FONT_NAME||force) || ((mask!=NULL)&&(mask->SMALL_FONT_NAME==1)))
    {
	xpp::session().not_already_set.SMALL_FONT_NAME=0;
    }
    return;
  }
  if(msc("FORECOLOR",s1)){
    if ((xpp::session().not_already_set.UserBlack||force) || ((mask!=NULL)&&(mask->UserBlack==1)))
    {
	xpp::session().not_already_set.UserBlack=0;
    }
    return;
  }
  if(msc("BACKCOLOR",s1)){
    if ((xpp::session().not_already_set.UserWhite||force) || ((mask!=NULL)&&(mask->UserWhite==1)))
    {
	xpp::session().not_already_set.UserWhite=0;
    }
    return;
  }
  if(msc("MWCOLOR",s1)){
    if ((xpp::session().not_already_set.UserMainWinColor||force) || ((mask!=NULL)&&(mask->UserMainWinColor==1)))
    {
	xpp::session().not_already_set.UserMainWinColor=0;
    }
    return;
  }
  if(msc("DWCOLOR",s1)){
    if ((xpp::session().not_already_set.UserDrawWinColor||force) || ((mask!=NULL)&&(mask->UserDrawWinColor==1)))
    {
	xpp::session().not_already_set.UserDrawWinColor=0;
    }
    return;
  }
  if(msc("GRADS",s1)){
    if ((xpp::session().not_already_set.UserGradients||force) || ((mask!=NULL)&&(mask->UserGradients==1)))
    {
	    if(!(msc(s2,"0")||msc(s2,"1")))
	    {
   		 xpp_log(XPP_LOG_ERROR, "GRADS option must be 0 or 1.\n");
		 exit(-1);
	    }
	    xpp::session().not_already_set.UserGradients=0;
    }
    return;
  }

  if(msc("PLOTFMT",s1)){
    if ((xpp::session().not_already_set.PLOTFORMAT||force) || ((mask!=NULL)&&(mask->PLOTFORMAT==1)))
    {
    	xpp::session().plot_export.format=s2;
	xpp::session().not_already_set.PLOTFORMAT=0;
    }
    return;
  }

  if(msc("BACKIMAGE",s1)){
    if ((xpp::session().not_already_set.UserBGBitmap||force) || ((mask!=NULL)&&(mask->UserBGBitmap==1)))
    {
	xpp::session().not_already_set.UserBGBitmap=0;
    }
    return;
  }
  if(msc("WIDTH",s1)){
    if ((xpp::session().not_already_set.UserMinWidth||force)|| ((mask!=NULL)&&(mask->UserMinWidth==1)))
    {
       xpp::session().not_already_set.UserMinWidth=0;
    }
    return;
  }
  if(msc("HEIGHT",s1)){
    if ((xpp::session().not_already_set.UserMinHeight||force) || ((mask!=NULL)&&(mask->UserMinHeight==1)))
    {
	 xpp::session().not_already_set.UserMinHeight=0;
    }
    return;
  }
  if(msc("YNC",s1)){
    if ((xpp::session().not_already_set.YNullColor||force) || ((mask!=NULL)&&(mask->YNullColor==1)))
    {
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   xpp::session().nullclines.y_null_color=i;
	  }
	   xpp::session().not_already_set.YNullColor=0;
    }
  return;
  }
if(msc("XNC",s1)){
    if ((xpp::session().not_already_set.XNullColor||force) || ((mask!=NULL)&&(mask->XNullColor==1)))
    {
	    i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   xpp::session().nullclines.x_null_color=i; 
	   xpp::session().not_already_set.XNullColor=0;
	  }
	  
    }
  return;
  }

if(msc("SMC",s1)){

    if ((xpp::session().not_already_set.StableManifoldColor||force) || ((mask!=NULL)&&(mask->StableManifoldColor==1)))
    {
  
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   xpp::session().manifolds.stable_color=i;
	   xpp::session().not_already_set.StableManifoldColor=0;
	  }
    }
  return;
  }
if(msc("UMC",s1)){
    if ((xpp::session().not_already_set.UnstableManifoldColor||force) || ((mask!=NULL)&&(mask->UnstableManifoldColor==1)))
    {
	    i=atoi(s2);
	    if(i>-1&&i<11)
	    {
	     xpp::session().manifolds.unstable_color=i;
	     xpp::session().not_already_set.UnstableManifoldColor=0;
	    }
    }
   return;
  }

  if(msc("LT",s1)){
     if ((xpp::session().not_already_set.START_LINE_TYPE||force) || ((mask!=NULL)&&(mask->START_LINE_TYPE==1)))
     {
     	
	    i=atoi(s2);
	    if(i<2&&i>-6)
	    {  
	      xpp::session().plot_settings.start_line_type=i; 
	      reset_all_line_type();
	      xpp::session().not_already_set.START_LINE_TYPE=0;
	      }
     }
     return;
  }
  if(msc("SEED",s1)){ 
     if ((xpp::session().not_already_set.RandSeed||force) || ((mask!=NULL)&&(mask->RandSeed==1)))
     {
	    i=atoi(s2);
	    if(i>=0){
	      xpp::session().numerics.rand_seed=i;
	      nsrand48(xpp::session().numerics.rand_seed);  
	      xpp::session().not_already_set.RandSeed=0;
	    }
     }
    return;
  }
 if(msc("BACK",s1)){
   if ((xpp::session().not_already_set.PaperWhite||force) || ((mask!=NULL)&&(mask->PaperWhite==1)))
   {
	   xpp::session().not_already_set.PaperWhite=0;
   }
    return;
  }
 if(msc("COLORMAP",s1)){
     if ((xpp::session().not_already_set.COLORMAP||force) || ((mask!=NULL)&&(mask->COLORMAP==1)))
     {
   		i=atoi(s2);
   		if(i<7)custom_color=i;
		xpp::session().not_already_set.COLORMAP=0;

     }
   return;
 }
   if(msc("NPLOT",s1)){
     if ((xpp::session().not_already_set.NPLOT||force) || ((mask!=NULL)&&(mask->NPLOT==1)))
     {
    	xpp::session().plot_settings.npltv=atoi(s2);
	xpp::session().not_already_set.NPLOT=0;
     }
    return;
  }

   /* can now initialize several plots */
   if(msc("SIMPLOT",s1)){
     xpp::session().plot_windows.simul=1;
     return;
   }
   if(msc("MULTIWIN",s1)){
     xpp::session().plot_settings.multi_win=1;
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
    if(i>-1)xpp::session().plot_settings.ix_plt[j]=i;
    return;
  }
   if(msc(yy.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)xpp::session().plot_settings.iy_plt[j]=i;
    return;
  }
   if(msc(zz.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)xpp::session().plot_settings.iz_plt[j]=i;
    return;
  }
   if(msc(xxh.c_str(),s1)){
     xpp::session().plot_settings.x_hi[j]=atof(s2);
     return;
   }
   if(msc(xxl.c_str(),s1)){
     xpp::session().plot_settings.x_lo[j]=atof(s2);
     return;
   }
if(msc(yyh.c_str(),s1)){
     xpp::session().plot_settings.y_hi[j]=atof(s2);
     return;
   }
if(msc(yyl.c_str(),s1)){
     xpp::session().plot_settings.y_lo[j]=atof(s2);
     return;
   }
 }
   if(msc("XP",s1)){
     if ((xpp::session().not_already_set.XP||force) || ((mask!=NULL)&&(mask->XP==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)xpp::session().plot_settings.ixplt=i;
	xpp::session().not_already_set.XP=0;
	xpp::session().not_already_set.IXPLT=0;
     }
    return;
  }
   if(msc("YP",s1)){
     if ((xpp::session().not_already_set.YP||force) || ((mask!=NULL)&&(mask->YP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)xpp::session().plot_settings.iyplt=i;
	xpp::session().not_already_set.YP=0;
	xpp::session().not_already_set.IYPLT=0;
     }
    return;
  }
   if(msc("ZP",s1)){
     if ((xpp::session().not_already_set.ZP||force) || ((mask!=NULL)&&(mask->ZP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)xpp::session().plot_settings.izplt=i;

     	xpp::session().not_already_set.ZP=0;
	xpp::session().not_already_set.IZPLT=0;
     }
    return;
  }
   if(msc("AXES",s1)){
     if ((xpp::session().not_already_set.AXES||force) || ((mask!=NULL)&&(mask->AXES==1)))
     {
	 if(s2[0]=='3')
	 {
	   xpp::session().plot_settings.axes=5;
	 }
	 else 
	 {
	   xpp::session().plot_settings.axes=0;
	 } 
        
	 xpp::session().not_already_set.AXES=0;
     }
    return;
  }

   if(msc("NJMP",s1)){
     if ((xpp::session().not_already_set.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
    	xpp::session().numerics.njmp=atoi(s2);
        xpp::session().not_already_set.NOUT=0;
     }
    return;
  }
  if(msc("NOUT",s1)){
     if ((xpp::session().not_already_set.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
      xpp::session().numerics.njmp=atoi(s2);
      xpp::session().not_already_set.NOUT=0;
     }
    return;
  }
   if(msc("NMESH",s1)){
     if ((xpp::session().not_already_set.NMESH||force) || ((mask!=NULL)&&(mask->NMESH==1)))
     {
    	xpp::session().numerics.nmesh=atoi(s2);
	xpp::session().not_already_set.NMESH=0;
     }
    return;
  }
   if(msc("METH",s1)){
     if ((xpp::session().not_already_set.METHOD||force) || ((mask!=NULL)&&(mask->METHOD==1)))
     {
    for(i=0;i<15;i++)
      if(s2[0]==mkey[i]||s2[0]==Mkey[i])
	xpp::session().numerics.method=i;
      
       xpp::session().not_already_set.METHOD=0;
     }
    return;
  }
   if(msc("VMAXPTS",s1)){
     if ((xpp::session().not_already_set.VMAXPTS||force) || ((mask!=NULL)&&(mask->VMAXPTS==1)))
     {
     	xpp::session().numerics.max_points=atoi(s2);
	xpp::session().not_already_set.VMAXPTS=0;
     
     }
     return;
   }
   if(msc("MAXSTOR",s1)){ 
     if ((xpp::session().not_already_set.MAXSTOR||force) || ((mask!=NULL)&&(mask->MAXSTOR==1)))
     {
    	xpp::session().data_store.max_rows=atoi(s2);
        xpp::session().not_already_set.MAXSTOR=0;
     } 
    return;
  }
   if(msc("TOR_PER",s1)){
     if ((xpp::session().not_already_set.TOR_PER||force) || ((mask!=NULL)&&(mask->TOR_PER==1)))
     {
     	xpp::session().numerics.tor_period=atof(s2);
     	xpp::session().numerics.torus=1;
	xpp::session().not_already_set.TOR_PER=0;
     }
     return;
   }
   if(msc("JAC_EPS",s1)){
     if ((xpp::session().not_already_set.JAC_EPS||force) || ((mask!=NULL)&&(mask->JAC_EPS==1)))
     {
     	xpp::session().numerics.newt_err=atof(s2);
        xpp::session().not_already_set.JAC_EPS=0;
     }
     return;
   }
   if(msc("NEWT_TOL",s1)){
     if ((xpp::session().not_already_set.NEWT_TOL||force) || ((mask!=NULL)&&(mask->NEWT_TOL==1)))
     {
     	xpp::session().numerics.evec_err=atof(s2);
	xpp::session().not_already_set.NEWT_TOL=0;
     
     }
     return;
   }
   if(msc("NEWT_ITER",s1)){
     if ((xpp::session().not_already_set.NEWT_ITER||force) || ((mask!=NULL)&&(mask->NEWT_ITER==1)))
     {
     	xpp::session().numerics.evec_iter=atoi(s2);
	xpp::session().not_already_set.NEWT_ITER=0;
     }
     return;
   }
  if(msc("FOLD",s1)){
     if ((xpp::session().not_already_set.FOLD||force) || ((mask!=NULL)&&(mask->FOLD==1)))
     {
     find_variable(s2,&i);
     if(i>0){
       xpp::session().itor[i-1]=1;
      xpp::session().numerics.torus=1;
     }
     
     }
     return;
   }
   if(msc("TOTAL",s1)){
    if ((xpp::session().not_already_set.TEND||force) || ((mask!=NULL)&&(mask->TEND==1)))
     {
    	xpp::session().numerics.tend=atof(s2);
	xpp::session().not_already_set.TEND=0;
    }
    return;
  }
  if(msc("DTMIN",s1)){
     if ((xpp::session().not_already_set.DTMIN||force) || ((mask!=NULL)&&(mask->DTMIN==1)))
     {
    	xpp::session().numerics.hmin=atof(s2);
         xpp::session().not_already_set.DTMIN=0;
     }
    return;
  }
  if(msc("DTMAX",s1)){
     if ((xpp::session().not_already_set.DTMAX||force) || ((mask!=NULL)&&(mask->DTMAX==1)))
     {
    	xpp::session().numerics.hmax=atof(s2);
	xpp::session().not_already_set.DTMAX=0;
      }
    return;
  }
   if(msc("DT",s1)){
     if ((xpp::session().not_already_set.DT||force) || ((mask!=NULL)&&(mask->DT==1)))
     {
    	xpp::session().numerics.delta_t=atof(s2);
	xpp::session().not_already_set.DT=0;
     }
    return;
  }
   if(msc("T0",s1)){
     if ((xpp::session().not_already_set.T0||force) || ((mask!=NULL)&&(mask->T0==1)))
     { 
    	xpp::session().numerics.t0=atof(s2);
        xpp::session().not_already_set.T0=0;
     }
    return;
  }
   if(msc("TRANS",s1)){
     if ((xpp::session().not_already_set.TRANS||force) || ((mask!=NULL)&&(mask->TRANS==1)))
     {
     	xpp::session().numerics.trans=atof(s2);
        xpp::session().not_already_set.TRANS=0;
     }
    return;
  }
   if(msc("BOUND",s1)){
     if ((xpp::session().not_already_set.BOUND||force) || ((mask!=NULL)&&(mask->BOUND==1)))
     {
       xpp::session().numerics.bound=atof(s2);
       xpp::session().not_already_set.BOUND=0;
     }
    return;
  }
   if(msc("ATOL",s1)){
     if ((xpp::session().not_already_set.ATOLER||force) || ((mask!=NULL)&&(mask->ATOLER==1)))
     {
     	xpp::session().numerics.atoler=atof(s2);
        xpp::session().not_already_set.ATOLER=0;
     }
     return;
   }
   if(msc("TOL",s1)){
     if ((xpp::session().not_already_set.TOLER||force) || ((mask!=NULL)&&(mask->TOLER==1)))
     {
	xpp::session().numerics.toler=atof(s2);
	xpp::session().not_already_set.TOLER=0;
     }
    return;
  }
    
   if(msc("DELAY",s1)){
     if ((xpp::session().not_already_set.DELAY||force) || ((mask!=NULL)&&(mask->DELAY==1)))
     {
    	xpp::session().numerics.delay=atof(s2);
	xpp::session().not_already_set.DELAY=0;
     }
    return;
  }
   if(msc("BANDUP",s1)){
     if ((xpp::session().not_already_set.BANDUP||force) || ((mask!=NULL)&&(mask->BANDUP==1)))
     {
     	xpp::session().numerics.cv_bandflag=1;
     	xpp::session().numerics.cv_bandupper=atoi(s2);
     	xpp::session().not_already_set.BANDUP=0;
     }
     return;
   }
  if(msc("BANDLO",s1)){
     if ((xpp::session().not_already_set.BANDLO||force) || ((mask!=NULL)&&(mask->BANDLO==1)))
     {
     	xpp::session().numerics.cv_bandflag=1;
     	xpp::session().numerics.cv_bandlower=atoi(s2);
     	xpp::session().not_already_set.BANDLO=0;
     }
     return;
   }
  
  if(msc("PHI",s1)){
     if ((xpp::session().not_already_set.PHI||force) || ((mask!=NULL)&&(mask->PHI==1)))
     {
    	PHI0=atof(s2);
	xpp::session().not_already_set.PHI=0;
     }
    return;
  }
   if(msc("THETA",s1)){
     if ((xpp::session().not_already_set.THETA||force) || ((mask!=NULL)&&(mask->THETA==1)))
     {
    	THETA0=atof(s2);
	xpp::session().not_already_set.THETA=0;
     }
    return;
  }
   if(msc("XLO",s1)){
     if ((xpp::session().not_already_set.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
     {
    	xpp::session().plot_settings.my_xlo=atof(s2);
	xpp::session().not_already_set.XLO=0;
     }
    return;
  }
   if(msc("YLO",s1)){
    if ((xpp::session().not_already_set.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
    {
    	xpp::session().plot_settings.my_ylo=atof(s2);
	xpp::session().not_already_set.YLO=0;
    }
    return;
  }
  
   if(msc("XHI",s1)){
    if ((xpp::session().not_already_set.XHI||force) || ((mask!=NULL)&&(mask->XHI==1)))
    {
    	xpp::session().plot_settings.my_xhi=atof(s2);
        xpp::session().not_already_set.XHI=0;
    }
    return;
  }
   if(msc("YHI",s1)){
     if ((xpp::session().not_already_set.YHI||force) || ((mask!=NULL)&&(mask->YHI==1)))
     {
    	xpp::session().plot_settings.my_yhi=atof(s2);
        xpp::session().not_already_set.YHI=0;
     }
    return;
  }
   if(msc("XMAX",s1)){
     if ((xpp::session().not_already_set.XMAX||force) || ((mask!=NULL)&&(mask->XMAX==1)))
     {
    	xpp::session().plot_settings.x_3d[1]=atof(s2);
	xpp::session().not_already_set.XMAX=0;
     
     }
    return;
  }
   if(msc("YMAX",s1)){
     if ((xpp::session().not_already_set.YMAX||force) || ((mask!=NULL)&&(mask->YMAX==1)))
     {
        xpp::session().plot_settings.y_3d[1]=atof(s2);
	xpp::session().not_already_set.YMAX=0;
     }
    return;
  }
   if(msc("ZMAX",s1)){
     if ((xpp::session().not_already_set.ZMAX||force) || ((mask!=NULL)&&(mask->ZMAX==1)))
     {
        xpp::session().plot_settings.z_3d[1]=atof(s2);
	xpp::session().not_already_set.ZMAX=0;
     }
    return;
  }
   if(msc("XMIN",s1)){
     if ((xpp::session().not_already_set.XMIN||force) || ((mask!=NULL)&&(mask->XMIN==1)))
     {
        xpp::session().plot_settings.x_3d[0]=atof(s2);
	xpp::session().not_already_set.XMIN=0; 
	if ((xpp::session().not_already_set.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
	{
    	   xpp::session().plot_settings.my_xlo=atof(s2);
	   xpp::session().not_already_set.XLO=0;
	}
     }
    return;
  }
   if(msc("YMIN",s1)){
     if ((xpp::session().not_already_set.YMIN||force) || ((mask!=NULL)&&(mask->YMIN==1)))
     {
    	xpp::session().plot_settings.y_3d[0]=atof(s2);
	xpp::session().not_already_set.YMIN=0;
	if ((xpp::session().not_already_set.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
	{
    	   xpp::session().plot_settings.my_ylo=atof(s2);
	   xpp::session().not_already_set.YLO=0;
	}
     }
    return;
  }
 if(msc("ZMIN",s1)){
     if ((xpp::session().not_already_set.ZMIN||force) || ((mask!=NULL)&&(mask->ZMIN==1)))
     {
    	xpp::session().plot_settings.z_3d[0]=atof(s2);
	xpp::session().not_already_set.ZMIN=0;
     }
    return;
  }

 if(msc("POIMAP",s1)){
     if ((xpp::session().not_already_set.POIMAP||force) || ((mask!=NULL)&&(mask->POIMAP==1)))
     {
   	if(s2[0]=='m'||s2[0]=='M')xpp::session().numerics.poimap=2;
   	if(s2[0]=='s'||s2[0]=='S')xpp::session().numerics.poimap=1;
   	if(s2[0]=='p'||s2[0]=='P')xpp::session().numerics.poimap=3;
   	xpp::session().not_already_set.POIMAP=0;
   }
   return;
 }

 if(msc("POIVAR",s1)){
     if ((xpp::session().not_already_set.POIVAR||force) || ((mask!=NULL)&&(mask->POIVAR==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)xpp::session().numerics.poivar=i;
	
	xpp::session().not_already_set.POIVAR=0;
     
     }
    return;
  }
 if(msc("OUTPUT",s1)){
     if ((xpp::session().not_already_set.OUTPUT||force) || ((mask!=NULL)&&(mask->OUTPUT==1)))
     {
   	batch_options.out_file=s2;
	xpp::session().not_already_set.OUTPUT=0;
     }
   return;
 }
  
 if(msc("POISGN",s1)){
     if ((xpp::session().not_already_set.POISGN||force) || ((mask!=NULL)&&(mask->POISGN==1)))
     {
   	xpp::session().numerics.poisgn=atoi(s2);
	xpp::session().not_already_set.POISGN=0;
     }
   return;
 }
 
 if(msc("POISTOP",s1)){
     if ((xpp::session().not_already_set.POISTOP||force) || ((mask!=NULL)&&(mask->POISTOP==1)))
     {
   	xpp::session().numerics.sos=atoi(s2);
	xpp::session().not_already_set.POISTOP=0;
     }
   return;
 }
 if(msc("STOCH",s1)){
     if ((xpp::session().not_already_set.STOCH||force)|| ((mask!=NULL)&&(mask->STOCH==1)))
     {
   	xpp::session().stochastic.flag=atoi(s2);
	xpp::session().not_already_set.STOCH=0;
     }
   return;
 }
 if(msc("POIPLN",s1)){
     if ((xpp::session().not_already_set.POIPLN||force)|| ((mask!=NULL)&&(mask->POIPLN==1)))
     {
   	xpp::session().numerics.poipln=atof(s2);
	xpp::session().not_already_set.POIPLN=0;
     }
   return;
 }

 if(msc("RANGEOVER",s1)){
     if ((xpp::session().not_already_set.RANGEOVER||force)|| ((mask!=NULL)&&(mask->RANGEOVER==1)))
     {
    	xpp::session().integrator.range.item=s2;
	xpp::session().not_already_set.RANGEOVER=0;
     }

    return;
  }
 if(msc("RANGESTEP",s1)){
     if ((xpp::session().not_already_set.RANGESTEP||force)|| ((mask!=NULL)&&(mask->RANGESTEP==1)))
     {
        
   	xpp::session().integrator.range.steps=atoi(s2);
	xpp::session().not_already_set.RANGESTEP=0;
     }
   return;
 }
  
 if(msc("RANGELOW",s1)){
     if ((xpp::session().not_already_set.RANGELOW||force)|| ((mask!=NULL)&&(mask->RANGELOW==1)))
     {
   	xpp::session().integrator.range.plow=atof(s2);
   	xpp::session().not_already_set.RANGELOW=0;
     }

   return;
 }

 if(msc("RANGEHIGH",s1)){
     if ((xpp::session().not_already_set.RANGEHIGH||force)|| ((mask!=NULL)&&(mask->RANGEHIGH==1)))
     {
   	xpp::session().integrator.range.phigh=atof(s2);
	xpp::session().not_already_set.RANGEHIGH=0;
     }
   return;
 }
 
 if(msc("RANGERESET",s1)){
     if ((xpp::session().not_already_set.RANGERESET||force)|| ((mask!=NULL)&&(mask->RANGERESET==1)))
     {
	 if(s2[0]=='y'||s2[0]=='Y')
	 {
	  xpp::session().integrator.range.reset=1;
	 }
	 else
	 {
	  xpp::session().integrator.range.reset=0;
	 } 
	  xpp::session().not_already_set.RANGERESET=0;
     }
  	return;
   }

 if(msc("RANGEOLDIC",s1)){
     if ((xpp::session().not_already_set.RANGEOLDIC||force)|| ((mask!=NULL)&&(mask->RANGEOLDIC==1)))
     {
  	if(s2[0]=='y'||s2[0]=='Y')
	{
   		xpp::session().integrator.range.oldic=1;
   	}
	else
	{ 
   		xpp::session().integrator.range.oldic=0;
	}
	
   	xpp::session().not_already_set.RANGEOLDIC=0;
     }
      return;
 }

 if(msc("RANGE",s1)){
     if ((xpp::session().not_already_set.RANGE||force)|| ((mask!=NULL)&&(mask->RANGE==1)))
     {
   	batch_options.range=atoi(s2);
	xpp::session().not_already_set.RANGE=0;
     }
   return;
 }
 
 if(msc("NTST",s1)){
     if ((xpp::session().not_already_set.NTST||force)|| ((mask!=NULL)&&(mask->NTST==1)))
     {
   	xpp::session().auto_state.options.ntst=atoi(s2);
	xpp::session().not_already_set.NTST=0;
     }
   return;
 }
if(msc("NMAX",s1)){
   if ((xpp::session().not_already_set.NMAX||force)|| ((mask!=NULL)&&(mask->NMAX==1)))
   {
   	xpp::session().auto_state.options.nmx=atoi(s2);
	xpp::session().not_already_set.NMAX=0;
   }
   return;
 }
if(msc("NPR",s1)){
   if ((xpp::session().not_already_set.NPR||force)|| ((mask!=NULL)&&(mask->NPR==1)))
   {
   	xpp::session().auto_state.options.npr=atoi(s2);
	xpp::session().not_already_set.NPR=0;
   }
   return;
 }
 if(msc("NCOL",s1)){
   if ((xpp::session().not_already_set.NCOL||force)|| ((mask!=NULL)&&(mask->NCOL==1)))
   {
   	xpp::session().auto_state.options.ncol=atoi(s2);
   	xpp::session().not_already_set.NCOL=0;
   }
   return;
 }

if(msc("DSMIN",s1)){
   if ((xpp::session().not_already_set.DSMIN||force)|| ((mask!=NULL)&&(mask->DSMIN==1)))
   {
   	xpp::session().auto_state.options.dsmin=atof(s2);
	xpp::session().not_already_set.DSMIN=0;
   }
   return;
 }
if(msc("DSMAX",s1)){
   if ((xpp::session().not_already_set.DSMAX||force)|| ((mask!=NULL)&&(mask->DSMAX==1)))
   {
   	xpp::session().auto_state.options.dsmax=atof(s2);
   	xpp::session().not_already_set.DSMAX=0;
   }
   return;
 }
if(msc("DS",s1)){
    if ((xpp::session().not_already_set.DS||force)|| ((mask!=NULL)&&(mask->DS==1)))
    {
   	xpp::session().auto_state.options.ds=atof(s2);
	xpp::session().not_already_set.DS=0;
    }
 
   return;
 }
if(msc("PARMIN",s1)){
   if ((xpp::session().not_already_set.XMAX||force)|| ((mask!=NULL)&&(mask->XMAX==1)))
   {
   	xpp::session().auto_state.options.rl0=atof(s2);
	xpp::session().not_already_set.XMAX=0;
   }
   return;
 }
if(msc("PARMAX",s1)){
    if ((xpp::session().not_already_set.PARMAX||force)|| ((mask!=NULL)&&(mask->PARMAX==1)))
    {
   	xpp::session().auto_state.options.rl1=atof(s2);
	xpp::session().not_already_set.PARMAX=0;
    }
   return;
 }
if(msc("NORMMIN",s1)){
     if ((xpp::session().not_already_set.NORMMIN||force)|| ((mask!=NULL)&&(mask->NORMMIN==1)))
     {
   	xpp::session().auto_state.options.a0=atof(s2);
	xpp::session().not_already_set.NORMMIN=0;
     }
   return;
 }
if(msc("NORMMAX",s1)){
     if ((xpp::session().not_already_set.NORMMAX||force)|| ((mask!=NULL)&&(mask->NORMMAX==1)))
     {
   	xpp::session().auto_state.options.a1=atof(s2);
   	xpp::session().not_already_set.NORMMAX=0;
     }
   return;
 }
 if(msc("EPSL",s1)){
     if ((xpp::session().not_already_set.EPSL||force)|| ((mask!=NULL)&&(mask->EPSL==1)))
     {
   	xpp::session().auto_state.options.epsl=atof(s2);
	xpp::session().not_already_set.EPSL=0;
     }
   return;
 }

if(msc("EPSU",s1)){
     if ((xpp::session().not_already_set.EPSU||force)|| ((mask!=NULL)&&(mask->EPSU==1)))
     {
   	xpp::session().auto_state.options.epsu=atof(s2);
	xpp::session().not_already_set.EPSU=0;
     }
   return;
 }
if(msc("EPSS",s1)){
     if ((xpp::session().not_already_set.EPSS||force)|| ((mask!=NULL)&&(mask->EPSS==1)))
     {
   	xpp::session().auto_state.options.epss=atof(s2);
	xpp::session().not_already_set.EPSS=0;
     }
   return;
 }
 if(msc("RUNNOW",s1)){
     if ((xpp::session().not_already_set.RUNNOW||force)|| ((mask!=NULL)&&(mask->RUNNOW==1)))
     {
   	xpp::session().run_immediately=atoi(s2);
	xpp::session().not_already_set.RUNNOW=0;
     }
   return;
 }

 if(msc("SEC",s1)){
     if ((xpp::session().not_already_set.SEC||force)|| ((mask!=NULL)&&(mask->SEC==1)))
     {
   	xpp::session().auto_state.stable_eq_color=atoi(s2);
	xpp::session().not_already_set.SEC=0;
     }
   return;
 }
 if(msc("UEC",s1)){
     if ((xpp::session().not_already_set.UEC||force)|| ((mask!=NULL)&&(mask->UEC==1)))
     {
   	xpp::session().auto_state.unstable_eq_color=atoi(s2);
	xpp::session().not_already_set.UEC=0;
     }
   return;
 }
 if(msc("SPC",s1)){
     if ((xpp::session().not_already_set.SPC||force)|| ((mask!=NULL)&&(mask->SPC==1)))
     {
   	xpp::session().auto_state.stable_po_color=atoi(s2);
	xpp::session().not_already_set.SPC=0;
     }
   return;
 }
 if(msc("UPC",s1)){
     if ((xpp::session().not_already_set.UPC||force)|| ((mask!=NULL)&&(mask->UPC==1)))
     {
   	xpp::session().auto_state.unstable_po_color=atoi(s2);
	xpp::session().not_already_set.UPC=0;
     }
   return;
 }

 if(msc("AUTOEVAL",s1)){
     if ((xpp::session().not_already_set.AUTOEVAL||force)|| ((mask!=NULL)&&(mask->AUTOEVAL==1)))
     {
   	f=atoi(s2);
   	set_auto_eval_flags(f);
	xpp::session().not_already_set.AUTOEVAL=0;
    }
   return;
 }
if(msc("AUTOXMAX",s1)){
     if ((xpp::session().not_already_set.AUTOXMAX||force)|| ((mask!=NULL)&&(mask->AUTOXMAX==1)))
     {
 	xpp::session().auto_state.options.xmax=atof(s2);
	xpp::session().not_already_set.AUTOXMAX=0;
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
   	PS_FONT=s2;
	xpp::session().not_already_set.PS_FONT=0;
     }
   return;
 }

if(msc("PS_LW",s1)){
   if ((xpp::session().not_already_set.PS_LW||force)|| ((mask!=NULL)&&(mask->PS_LW==1)))
   {
  	PS_LW=atof(s2);
	xpp::session().not_already_set.PS_LW=0;
   }
   return;
 }

if(msc("PS_FSIZE",s1)){
     if ((xpp::session().not_already_set.PS_FSIZE||force)|| ((mask!=NULL)&&(mask->PS_FSIZE==1)))
     {
  	PS_FONTSIZE=atoi(s2);
	xpp::session().not_already_set.PS_FSIZE=0;
     }
   return;
 }

if(msc("PS_COLOR",s1)){
     if ((xpp::session().not_already_set.PS_COLOR||force)|| ((mask!=NULL)&&(mask->PS_COLOR==1)))
     {
  	PSColorFlag=atoi(s2);
  	xpp::session().plot_export.color=PSColorFlag;
	xpp::session().not_already_set.PS_COLOR=0;
     }
   return;
 }
if(msc("TUTORIAL",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "TUTORIAL option must be 0 or 1.\n");
	exit(-1);
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

