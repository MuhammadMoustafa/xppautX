#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "load_eqn.h"
#include "markov.h"
#include "xpp_mem.h"
#include "parserslow.h"

#include "read_dir.h"

#include "main.h"
#include "ggets.h"
#include "dae_fun.h"
#include "derived.h"
#include "extra.h"
#include "init_conds.h"
#include "browse.h"
#include "txtread.h"
#include "numerics.h"
#include "integrate.h"
#include "odesol2.h"
#include "adj2.h"
#include "arrayplot.h"
#include "lunch-new.h"
#include "graphics.h"

/*#include "macdirent.h"
*/

#include <dirent.h>
#include "userbut.h"
#include "volterra2.h"
#include "storage.h"
#include "tabular.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include "xpplim.h"
#include "xpp_io.h"
#include "xpp_batch.h"
#include "xpp_log.h"
#include "many_pops.h"
#include "graf_par.h"
#include "xpp_globals.h"

#define PARAM 1
#define IC 2


#define DFNORMAL 1
#define MAXOPT 1000
#define READEM 1

OptionsSet notAlreadySet;

typedef struct {
  int nbins,nbins2,type,col,col2,fftc;
  double xlo,xhi;
  double ylo,yhi;
  char cond[80];
} HIST_INFO;

extern HIST_INFO hist_inf;
extern int spec_col,spec_wid,spec_win,spec_col2,post_process;




namespace {

/* the @ option lines .xpprc and the command line stored for
   set_internopts, each whole */
std::vector<std::string> interopt;

/* strtok's tokens (get_first/get_next) over a copy of the text, each
   call naming its own delimiters as strtok's did */
class Tokenizer {
public:
  explicit Tokenizer(std::string_view text) : text_(text) {}
  /* the next token, or false at the end */
  bool next(std::string_view delims, std::string_view &token)
  {
    size_t start = text_.find_first_not_of(delims, pos_);
    if (start == std::string_view::npos) {
      pos_ = text_.size();
      return false;
    }
    size_t end = text_.find_first_of(delims, start);
    if (end == std::string_view::npos) end = text_.size();
    token = text_.substr(start, end - start);
    pos_ = end < text_.size() ? end + 1 : end;
    return true;
  }
private:
  std::string_view text_;
  size_t pos_ = 0;
};

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
   an empty name or value left out */
template <class F>
void each_option(std::string_view line, std::string_view delims, F set)
{
  Tokenizer tok(line);
  std::string_view t;
  if (!tok.next(" ,", t)) return;
  std::string name, value;
  while (tok.next(delims, t)) {
    split_apart(t, name, value);
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

bool is_directory(const char *path)
{
  DIR *dir = opendir(path);
  if (dir == nullptr) return false;
  closedir(dir);
  return true;
}

} // namespace

int RunImmediately=0;
XppSlider sliders[XPP_NSLIDERS] = {{"", 0.0, 1.0}, {"", 0.0, 1.0}, {"", 0.0, 1.0}};




extern int NCBatch,DFBatch;
extern int DF_GRID;

typedef struct {
  char *name;
  char *does;
  unsigned int use;
} INTERN_SET;

extern int XNullColor,YNullColor,StableManifoldColor,UnstableManifoldColor;
int IX_PLT[10],IY_PLT[10],IZ_PLT[10],NPltV;
int MultiWin=0;
double X_LO[10],Y_LO[10],X_HI[10],Y_HI[10];
int START_LINE_TYPE=1;
INTERN_SET intern_set[MAX_INTERN_SET];
int Nintern_set=0;

extern int STOCH_FLAG;
extern char uvar_names[MAXODE][XPP_NAME_MAX+1]; 

extern int custom_color;
extern int del_stab_flag;
extern int MaxPoints;
extern double THETA0,PHI0;
/*void set_option(char *s1,const char *s2);
*/

/*   this file has all of the phaseplane parameters defined   
     and created.  All other files should use external stuff
    to use them. (Except eqn forming stuff)
 */

 double last_ic[MAXODE];

extern int PSColorFlag,PS_FONTSIZE;
extern char PS_FONT[100];
extern double PS_LW;

extern int SEc,UEc,SPc,UPc;

 char delay_string[MAXODE][80];
 int itor[MAXODE];
 /*char this_file[100];
 */
 char this_file[XPP_MAX_NAME];
 char this_internset[XPP_MAX_NAME];
 float oldhp_x,oldhp_y,my_pl_wid,my_pl_ht;
 int mov_ind;
 int  storind,STORFLAG,INFLAG,MAXSTOR;
 double x_3d[2],y_3d[2],z_3d[2];
 int IXPLT,IYPLT,IZPLT;
 int AXES,TIMPLOT,PLOT_3D;
 double MY_XLO,MY_YLO,MY_XHI,MY_YHI;
 double TOR_PERIOD=6.2831853071795864770;
 int TORUS=0;
 int NEQ;
 std::string options_file;

/*   Numerical stuff ....   */

 double DELTA_T,TEND,T0,TRANS,
	NULL_ERR,EVEC_ERR,NEWT_ERR;
 double BOUND,DELAY,TOLER,ATOLER,HMIN,HMAX;
 double BVP_EPS,BVP_TOL;

 double POIPLN;

 extern int RandSeed;
 int MaxEulIter;
double EulTol;
extern int cv_bandflag,cv_bandupper,cv_bandlower;
 int NMESH,NJMP,METHOD;
 int EVEC_ITER;
 int BVP_MAXIT,BVP_FLAG;

 int POIMAP,POIVAR,POISGN,SOS;
   int FFT,NULL_HERE,POIEXT;
  int HIST,FOREVER;

 /*  control of range stuff  */

 int PAUSER,ENDSING,SHOOT,PAR_FOL;


/*  custon color stuff  */

extern char ColorVia[XPP_NAME_MAX+1];
extern double ColorViaLo,ColorViaHi;
extern int ColorizeFlag;


/* AUTO STUFF  */
extern int auto_ntst,auto_nmx,auto_npr,auto_ncol;
extern double auto_ds,  auto_dsmax,  auto_dsmin;
extern double auto_rl0,auto_rl1,auto_a0,auto_a1;
extern double auto_epss,auto_epsl,auto_epsu;
extern int auto_var;
extern double auto_xmin,auto_xmax,auto_ymin,auto_ymax;

 extern int PltFmtFlag;

 int xorfix,silent,got_file;

   

void dump_torus(FILE *fp, int f)
{
  int i;
  if(f==READEM){
    xpp::LineReader lr = xpp::LineReader::attach(fp);
    if(!lr.next())return;
  }
  else
    std::fputs("# Torus information \n",fp);
  io_int(&TORUS,fp,f," Torus flag 1=ON");
  io_double(&TOR_PERIOD,fp,f,"Torus period");
  if(TORUS){
    for(i=0;i<NEQ;i++)
      io_int(&itor[i],fp,f,uvar_names[i]);
  }
}


void load_eqn()
{
 int okay=0;
 int std=0;
 init_ar_ic();
 for(int i=0;i<MAXODE;i++)
 {
  itor[i]=0;
  XPP_FORMAT_TO_BUF(delay_string[i],"0.0");
 }
 if(strcmp(this_file,"/dev/stdin")==0)std=1;
 if (got_file==1&&(std==0)&&is_directory(this_file))
 {
   change_directory(this_file);
   make_eqn();
   return;
 }
 if(got_file==1)
 {
   FILE *fptr=std::fopen(this_file,"r");
   if(fptr!=NULL)
   {
     if(std==1)XPP_FORMAT_TO_BUF(this_file,"console");
     okay=get_eqn(fptr);
     if(std==0)
       std::fclose(fptr);
     if(okay==1)return;
   }
 }
 while(okay==0)
 {
   const char *start=getenv("XPPSTART");
   if (start!=NULL && is_directory(start))
     change_directory(start);
   okay=make_eqn();
 }
}

void set_all_vals()
{
 int i;
 
 if (notAlreadySet.TIMEPLOT){TIMPLOT=1;notAlreadySet.TIMEPLOT=0;};
 if (notAlreadySet.FOREVER){FOREVER=0;notAlreadySet.FOREVER=0;};
 if (notAlreadySet.BVP_TOL){BVP_TOL=1.e-5;notAlreadySet.BVP_TOL=0;};
 if (notAlreadySet.BVP_EPS){BVP_EPS=1.e-5;notAlreadySet.BVP_EPS=0;};
 if (notAlreadySet.BVP_MAXIT){BVP_MAXIT=20;notAlreadySet.BVP_MAXIT=0;};
 if (notAlreadySet.BVP_FLAG){BVP_FLAG=0;notAlreadySet.BVP_FLAG=0;};
 if (notAlreadySet.NMESH){NMESH=40;notAlreadySet.NMESH=0;};
 if (notAlreadySet.NOUT){NJMP=1;notAlreadySet.NOUT=0;};
 if (notAlreadySet.SOS){SOS=0;notAlreadySet.SOS=0;};
 if (notAlreadySet.FFT){FFT=0;notAlreadySet.FFT=0;};
 if (notAlreadySet.HIST){HIST=0;notAlreadySet.HIST=0;};
 if (notAlreadySet.PltFmtFlag){PltFmtFlag=0;notAlreadySet.PltFmtFlag=0;};
 if (notAlreadySet.AXES){AXES=0;notAlreadySet.AXES=0;};
 if (notAlreadySet.TOLER){TOLER=0.001;notAlreadySet.TOLER=0;};
 if (notAlreadySet.ATOLER){ATOLER=0.001;notAlreadySet.ATOLER=0;};
 if (notAlreadySet.MaxEulIter){MaxEulIter=10;notAlreadySet.MaxEulIter=0;}
 if (notAlreadySet.EulTol){EulTol=1.e-7;notAlreadySet.EulTol=0;};
 if (notAlreadySet.DELAY){DELAY=0.0;notAlreadySet.DELAY=0;};
 if (notAlreadySet.DTMIN){HMIN=1e-12;notAlreadySet.DTMIN=0;};
 if (notAlreadySet.EVEC_ITER){EVEC_ITER=100;notAlreadySet.EVEC_ITER=0;};
 if (notAlreadySet.EVEC_ERR){EVEC_ERR=.001;notAlreadySet.EVEC_ERR=0;};
 if (notAlreadySet.NULL_ERR){NULL_ERR=.001;notAlreadySet.NULL_ERR=0;};
 if (notAlreadySet.NEWT_ERR){NEWT_ERR=.001;notAlreadySet.NEWT_ERR=0;};
 if (notAlreadySet.NULL_HERE){NULL_HERE=0;notAlreadySet.NULL_HERE=0;};
 del_stab_flag=DFNORMAL;
 if (notAlreadySet.DTMAX){HMAX=1.000;notAlreadySet.DTMAX=0;};
 if (notAlreadySet.POIMAP){POIMAP=0;notAlreadySet.POIMAP=0;};
 if (notAlreadySet.POIVAR){POIVAR=1;notAlreadySet.POIVAR=0;};
 if (notAlreadySet.POIEXT){POIEXT=0;notAlreadySet.POIEXT=0;};
 if (notAlreadySet.POISGN){POISGN=1;notAlreadySet.POISGN=0;};
 if (notAlreadySet.POIPLN){POIPLN=0.0;notAlreadySet.POIPLN=0;};

 storind=0;
 mov_ind=0;


 STORFLAG=0;


 INFLAG=0;
 oldhp_x=-100000.0 ;
 oldhp_y=-100000.0;
 solver=rung_kut;
 PLOT_3D=0;
 if (notAlreadySet.METHOD){METHOD=3;notAlreadySet.METHOD=0;};
 if (notAlreadySet.XLO){MY_XLO=0.0;x_3d[0]=MY_XLO;notAlreadySet.XLO=0;notAlreadySet.XMIN=0;};
 if (notAlreadySet.XHI){MY_XHI=20.0;x_3d[1]=MY_XHI;notAlreadySet.XHI=0;notAlreadySet.XMAX=0;};
 if (notAlreadySet.YLO){MY_YLO=-1;y_3d[0]=MY_YLO;notAlreadySet.YLO=0;notAlreadySet.YMIN=0;};
 if (notAlreadySet.YHI){MY_YHI=1;y_3d[0]=MY_YHI;notAlreadySet.YHI=0;notAlreadySet.YMAX=0;};
 
 if (notAlreadySet.BOUND){BOUND=100;notAlreadySet.BOUND=0;};
 if (notAlreadySet.MAXSTOR){MAXSTOR=5000;notAlreadySet.MAXSTOR=0;};
 my_pl_wid=10000. ;
 my_pl_ht=7000.  ;

 /* TORUS=0; */ 
 if (notAlreadySet.T0){T0=0.0;notAlreadySet.T0=0;};
 if (notAlreadySet.TRANS){TRANS=0.0;notAlreadySet.TRANS=0;};
 if (notAlreadySet.DT){DELTA_T=.05;notAlreadySet.DT=0;};
 /*  if (notAlreadySet.JAC_EPS){NEWT_ERR=.001;notAlreadySet.JAC_EPS=0;}; */
 
 if (notAlreadySet.XMIN){x_3d[0]=-12;notAlreadySet.XMIN=0;notAlreadySet.XLO=0;};
 if (notAlreadySet.XMAX){x_3d[1]=12;notAlreadySet.XMAX=0;notAlreadySet.XHI=0;};
 if (notAlreadySet.YMIN){y_3d[0]=-12;notAlreadySet.YMIN=0;notAlreadySet.YLO=0;};
 if (notAlreadySet.YMAX){y_3d[1]=12;notAlreadySet.YMAX=0;notAlreadySet.YHI=0;};
 if (notAlreadySet.ZMIN){z_3d[0]=-12;notAlreadySet.ZMIN=0;};
 if (notAlreadySet.ZMAX){z_3d[1]=12;notAlreadySet.ZMAX=0;};
 
 if (notAlreadySet.TEND){TEND=20.00;notAlreadySet.TEND=0;};
 /* TOR_PERIOD=6.2831853071795864770; */
 if (notAlreadySet.IXPLT){IXPLT=0;notAlreadySet.IXPLT=0;}
 if (notAlreadySet.IYPLT){IYPLT=1;notAlreadySet.IYPLT=0;}
 if (notAlreadySet.IZPLT){IZPLT=1;notAlreadySet.IZPLT=0;}
 
 if (notAlreadySet.NPLOT){
   if (NEQ>2){if(notAlreadySet.IZPLT){IZPLT=2;}}
 NPltV=1;
 for(i=0;i<10;i++){
   IX_PLT[i]=IXPLT;
   IY_PLT[i]=IYPLT;
   IZ_PLT[i]=IZPLT;
   X_LO[i]=0;
   Y_LO[i]=-1;
   X_HI[i]=20;
   Y_HI[i]=1;
 }
 notAlreadySet.NPLOT=0;
 }
 /* internal options go here  */
 set_internopts(NULL);
 

 if(FILE *fp=std::fopen(options_file.c_str(),"r"))
 {
  read_defaults(fp);
  std::fclose(fp);
 }


 init_range();
 init_trans();
 init_my_aplot();
 init_txtview();

  chk_volterra();  

/*                           */

 if(IZPLT>NEQ)IZPLT=NEQ;
 if(IYPLT>NEQ)IYPLT=NEQ;
 if(IXPLT==0||IYPLT==0)
   TIMPLOT=1;
 else 
   TIMPLOT=0;
 if(x_3d[0]>=x_3d[1]){
   x_3d[0]=-1;
   x_3d[1]=1;
 }
if(y_3d[0]>=y_3d[1]){
   y_3d[0]=-1;
   y_3d[1]=1;
 }
if(z_3d[0]>=z_3d[1]){
   z_3d[0]=-1;
   z_3d[1]=1;
 }
 if(MY_XLO>=MY_XHI){
   MY_XLO=-2.0;
   MY_XHI=2.0;
 }
if(MY_YLO>=MY_YHI){
   MY_YLO=-2.0;
   MY_YHI=2.0;
 }
 if(AXES<5){
   x_3d[0]=MY_XLO;
   y_3d[0]=MY_YLO;
   x_3d[1]=MY_XHI;
   y_3d[1]=MY_YHI;
 } 
 init_stor(MAXSTOR,NEQ+1);
 if(AXES>=5)PLOT_3D=1;
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
 Tokenizer font(bob);
 std::string_view name;
 if (notAlreadySet.BIG_FONT_NAME && font.next(" ",name))
	notAlreadySet.BIG_FONT_NAME=0;

 bob=read_line(fp);
 Tokenizer small_font(bob);
 if (notAlreadySet.SMALL_FONT_NAME && small_font.next(" ",name))
	notAlreadySet.SMALL_FONT_NAME=0;

 if (notAlreadySet.PaperWhite){int paper_white; fil_int(fp,&paper_white);notAlreadySet.PaperWhite=0;}; /* X11 only: read, not kept */
 if (notAlreadySet.IXPLT){fil_int(fp,&IXPLT);notAlreadySet.IXPLT=0;};
 if (notAlreadySet.IYPLT){fil_int(fp,&IYPLT);notAlreadySet.IYPLT=0;};
 if (notAlreadySet.IZPLT){fil_int(fp,&IZPLT);notAlreadySet.IZPLT=0;};
 if (notAlreadySet.AXES){fil_int(fp,&AXES);notAlreadySet.PaperWhite=0;};
 if (notAlreadySet.NOUT){fil_int(fp,&NJMP);notAlreadySet.NOUT=0;};
 if (notAlreadySet.NMESH){fil_int(fp,&NMESH);notAlreadySet.NMESH=0;};
 if (notAlreadySet.METHOD){fil_int(fp,&METHOD);notAlreadySet.METHOD=0;};

 if (notAlreadySet.TIMEPLOT){fil_int(fp,&TIMPLOT);notAlreadySet.TIMEPLOT=0;};
 if (notAlreadySet.MAXSTOR){fil_int(fp,&MAXSTOR);notAlreadySet.MAXSTOR=0;};
 if (notAlreadySet.TEND){fil_flt(fp,&TEND);notAlreadySet.TEND=0;};
 if (notAlreadySet.DT){fil_flt(fp,&DELTA_T);notAlreadySet.DT=0;};
 if (notAlreadySet.T0){fil_flt(fp,&T0);notAlreadySet.T0=0;};
 if (notAlreadySet.TRANS){fil_flt(fp,&TRANS);notAlreadySet.TRANS=0;};
 if (notAlreadySet.BOUND){fil_flt(fp,&BOUND);notAlreadySet.BOUND=0;};
 if (notAlreadySet.DTMIN){fil_flt(fp,&HMIN);notAlreadySet.DTMIN=0;};
 if (notAlreadySet.DTMAX){fil_flt(fp,&HMAX);notAlreadySet.DTMIN=0;};
 if (notAlreadySet.TOLER){fil_flt(fp,&TOLER);notAlreadySet.TOLER=0;};
 if (notAlreadySet.DELAY){fil_flt(fp,&DELAY);notAlreadySet.DELAY=0;};
 if (notAlreadySet.XLO){fil_flt(fp,&MY_XLO);notAlreadySet.XLO=0;};
 if (notAlreadySet.XHI){fil_flt(fp,&MY_XHI);notAlreadySet.XHI=0;};
 if (notAlreadySet.YLO){fil_flt(fp,&MY_YLO);notAlreadySet.YLO=0;};
 if (notAlreadySet.YHI){fil_flt(fp,&MY_YHI);notAlreadySet.YHI=0;};

 
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
  int j=Nintern_set;
  if(Nintern_set>=MAX_INTERN_SET){
   xpp_log(XPP_LOG_WARN, " %s not added -- too many must be less than %d \n",
	   name,MAX_INTERN_SET);
    return;
  }
  intern_set[j].use=1;
  /* "$ " then does without its braces, commas as spaces */
  std::string bob="$ ";
  for(const char *p=does;*p;p++){
    if(*p=='}'||*p=='{')
      continue;
    bob+=*p==','?' ':*p;
  }
  /* INTERN_SET is C API (comline.h): xpp_strdup'd text */
  intern_set[j].name=xpp_strdup(name);
  intern_set[j].does=xpp_strdup(bob.c_str());
 xpp_log(XPP_LOG_INFO, " added %s doing %s \n",
	 intern_set[j].name,intern_set[j].does);
  Nintern_set++;
}


void extract_action(const char *ptr)
{
  Tokenizer tok(ptr);
  std::string_view t;
  if(!tok.next(" ",t))return;
  std::string name,value;
  while(tok.next(" ,;\n",t)){
    split_apart(t,name,value);
    if(!name.empty()&&!value.empty())
      do_intern_set(name.c_str(),value.c_str());
  }
}

void extract_internset(int j)
{
  extract_action(intern_set[j].does);
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
    last_ic[i]=atof(value);
  }
  else {
    i=find_user_name(PARAM,name);
    if(i>-1){
      set_val(name,atof(value));
    }
    else {
      /*     set_option(name,value,0,NULL); */
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
  for(const std::string &opt : interopt)
    each_option(opt," ,\n\r",[mask](const std::string &name,const std::string &value){
      set_option(name.c_str(),value.c_str(),0,mask);
    });
  interopt.clear();
}

void set_internopts_xpprc_and_comline()
{
  if(interopt.empty())return;
  /* QUIET and LOGFILE first */
  for(const std::string &opt : interopt){
    Tokenizer tok(opt);
    std::string_view t;
    if(!tok.next(" ,",t))continue;
    std::string name,value;
    while(tok.next(" ,\n\r",t)){
      split_apart(t,name,value);
      name=upper_case(name);
      if(name=="QUIET"||name=="LOGFILE")
        set_option(name.c_str(),value.c_str(),0,NULL);
    }
  }

  /*We make a BOOLEAN MASK using the current OptionsSet*/
  /*This allows options to be overwritten multiple times within .xpprc
  but prevents overwriting across comline, .xpprc etc.
  */
  OptionsSet mask = notAlreadySet;
  for(const std::string &opt : interopt)
    each_option(opt," ,\n\r",[&mask](const std::string &name,const std::string &value){
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
  if(interopt.size()>=MAXOPT){
   xpp_log(XPP_LOG_WARN, "to many options set %s ignored\n",s1);
    return;
  }
  interopt.emplace_back(s1);
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
      if (log_settings.file != NULL)       
      { 		         
     	  fclose(log_settings.file);       
      } 		         
      log_settings.file=fopen(s2,"w");     
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
    if ((notAlreadySet.BIG_FONT_NAME||force) || ((mask!=NULL)&&(mask->BIG_FONT_NAME==1)))
    {
	notAlreadySet.BIG_FONT_NAME=0;
    }
    return;
  }
  if((msc("SMALLFONT",s1))||(msc("SMALL",s1))){;
    if ((notAlreadySet.SMALL_FONT_NAME||force) || ((mask!=NULL)&&(mask->SMALL_FONT_NAME==1)))
    {
	notAlreadySet.SMALL_FONT_NAME=0;
    }
    return;
  }
  if(msc("FORECOLOR",s1)){
    if ((notAlreadySet.UserBlack||force) || ((mask!=NULL)&&(mask->UserBlack==1)))
    {
	notAlreadySet.UserBlack=0;
    }
    return;
  }
  if(msc("BACKCOLOR",s1)){
    if ((notAlreadySet.UserWhite||force) || ((mask!=NULL)&&(mask->UserWhite==1)))
    {
	notAlreadySet.UserWhite=0;
    }
    return;
  }
  if(msc("MWCOLOR",s1)){
    if ((notAlreadySet.UserMainWinColor||force) || ((mask!=NULL)&&(mask->UserMainWinColor==1)))
    {
	notAlreadySet.UserMainWinColor=0;
    }
    return;
  }
  if(msc("DWCOLOR",s1)){
    if ((notAlreadySet.UserDrawWinColor||force) || ((mask!=NULL)&&(mask->UserDrawWinColor==1)))
    {
	notAlreadySet.UserDrawWinColor=0;
    }
    return;
  }
  if(msc("GRADS",s1)){
    if ((notAlreadySet.UserGradients||force) || ((mask!=NULL)&&(mask->UserGradients==1)))
    {
	    if(!(msc(s2,"0")||msc(s2,"1")))
	    {
   		 xpp_log(XPP_LOG_ERROR, "GRADS option must be 0 or 1.\n");
		 exit(-1);
	    }
	    notAlreadySet.UserGradients=0;
    }
    return;
  }



  if(msc("PLOTFMT",s1)){
    if ((notAlreadySet.PLOTFORMAT||force) || ((mask!=NULL)&&(mask->PLOTFORMAT==1)))
    {
    	XPP_FORMAT_TO_BUF(plot_export.format,"{}",s2);
	notAlreadySet.PLOTFORMAT=0;
    }
    return;
  }

  

  if(msc("BACKIMAGE",s1)){
    if ((notAlreadySet.UserBGBitmap||force) || ((mask!=NULL)&&(mask->UserBGBitmap==1)))
    {
	notAlreadySet.UserBGBitmap=0;
    }
    return;
  }
  if(msc("WIDTH",s1)){
    if ((notAlreadySet.UserMinWidth||force)|| ((mask!=NULL)&&(mask->UserMinWidth==1)))
    {
       notAlreadySet.UserMinWidth=0;
    }
    return;
  }
  if(msc("HEIGHT",s1)){
    if ((notAlreadySet.UserMinHeight||force) || ((mask!=NULL)&&(mask->UserMinHeight==1)))
    {
	 notAlreadySet.UserMinHeight=0;
    }
    return;
  }
  if(msc("YNC",s1)){
    if ((notAlreadySet.YNullColor||force) || ((mask!=NULL)&&(mask->YNullColor==1)))
    {
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   YNullColor=i;
	  }
	   notAlreadySet.YNullColor=0;
    }
  return;
  }
if(msc("XNC",s1)){
    if ((notAlreadySet.XNullColor||force) || ((mask!=NULL)&&(mask->XNullColor==1)))
    {
	    i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   XNullColor=i; 
	   notAlreadySet.XNullColor=0;
	  }
	  
    }
  return;
  }

if(msc("SMC",s1)){

    if ((notAlreadySet.StableManifoldColor||force) || ((mask!=NULL)&&(mask->StableManifoldColor==1)))
    {
  
	  i=atoi(s2);
	  if(i>-1&&i<11)
	  {
	   StableManifoldColor=i;
	   notAlreadySet.StableManifoldColor=0;
	  }
    }
  return;
  }
if(msc("UMC",s1)){
    if ((notAlreadySet.UnstableManifoldColor||force) || ((mask!=NULL)&&(mask->UnstableManifoldColor==1)))
    {
	    i=atoi(s2);
	    if(i>-1&&i<11)
	    {
	     UnstableManifoldColor=i;
	     notAlreadySet.UnstableManifoldColor=0;
	    }
    }
   return;
  }

  if(msc("LT",s1)){
     if ((notAlreadySet.START_LINE_TYPE||force) || ((mask!=NULL)&&(mask->START_LINE_TYPE==1)))
     {
     	
	    i=atoi(s2);
	    if(i<2&&i>-6)
	    {  
	      START_LINE_TYPE=i; 
	      reset_all_line_type();
	      notAlreadySet.START_LINE_TYPE=0;
	      }
     }
     return;
  }
  if(msc("SEED",s1)){ 
     if ((notAlreadySet.RandSeed||force) || ((mask!=NULL)&&(mask->RandSeed==1)))
     {
	    i=atoi(s2);
	    if(i>=0){
	      RandSeed=i;
	      nsrand48(RandSeed);  
	      notAlreadySet.RandSeed=0;
	    }
     }
    return;
  }
 if(msc("BACK",s1)){
   if ((notAlreadySet.PaperWhite||force) || ((mask!=NULL)&&(mask->PaperWhite==1)))
   {
	   notAlreadySet.PaperWhite=0;
   }
    return;
  }
 if(msc("COLORMAP",s1)){
     if ((notAlreadySet.COLORMAP||force) || ((mask!=NULL)&&(mask->COLORMAP==1)))
     {
   		i=atoi(s2);
   		if(i<7)custom_color=i;
		notAlreadySet.COLORMAP=0;

     }
   return;
 }
   if(msc("NPLOT",s1)){
     if ((notAlreadySet.NPLOT||force) || ((mask!=NULL)&&(mask->NPLOT==1)))
     {
    	NPltV=atoi(s2);
	notAlreadySet.NPLOT=0;
     }
    return;
  }

   if(msc("DLL_LIB",s1)){
      if ((notAlreadySet.DLL_LIB||force) || ((mask!=NULL)&&(mask->DLL_LIB==1)))
     {
     set_dll_library(s2);
     notAlreadySet.DLL_LIB=0;
     }
     return;
   }
   if(msc("DLL_FUN",s1)){
     if ((notAlreadySet.DLL_FUN||force) || ((mask!=NULL)&&(mask->DLL_FUN==1)))
     {
     	set_dll_function(s2);
     	notAlreadySet.DLL_FUN=0;
     }
     return;
   }
   /* can now initialize several plots */
   if(msc("SIMPLOT",s1)){
     plot_windows.simul=1;
     return;
   }
   if(msc("MULTIWIN",s1)){
     MultiWin=1;
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
    if(i>-1)IX_PLT[j]=i;
    return;
  }
   if(msc(yy.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)IY_PLT[j]=i;
    return;
  }
   if(msc(zz.c_str(),s1)){
     find_variable(s2,&i);
    if(i>-1)IZ_PLT[j]=i;
    return;
  }
   if(msc(xxh.c_str(),s1)){
     X_HI[j]=atof(s2);
     return;
   }
   if(msc(xxl.c_str(),s1)){
     X_LO[j]=atof(s2);
     return;
   }
if(msc(yyh.c_str(),s1)){
     Y_HI[j]=atof(s2);
     return;
   }
if(msc(yyl.c_str(),s1)){
     Y_LO[j]=atof(s2);
     return;
   }
 }
   if(msc("XP",s1)){
     if ((notAlreadySet.XP||force) || ((mask!=NULL)&&(mask->XP==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)IXPLT=i;
	notAlreadySet.XP=0;
	notAlreadySet.IXPLT=0;
     }
    return;
  }
   if(msc("YP",s1)){
     if ((notAlreadySet.YP||force) || ((mask!=NULL)&&(mask->YP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)IYPLT=i;
	notAlreadySet.YP=0;
	notAlreadySet.IYPLT=0;
     }
    return;
  }
   if(msc("ZP",s1)){
     if ((notAlreadySet.ZP||force) || ((mask!=NULL)&&(mask->ZP==1)))
     {
     	find_variable(s2,&i);
    	if(i>-1)IZPLT=i;

     	notAlreadySet.ZP=0;
	notAlreadySet.IZPLT=0;
     }
    return;
  }
   if(msc("AXES",s1)){
     if ((notAlreadySet.AXES||force) || ((mask!=NULL)&&(mask->AXES==1)))
     {
	 if(s2[0]=='3')
	 {
	   AXES=5;
	 }
	 else 
	 {
	   AXES=0;
	 } 
        
	 notAlreadySet.AXES=0;
     }
    return;
  }

   if(msc("NJMP",s1)){
     if ((notAlreadySet.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
    	NJMP=atoi(s2);
        notAlreadySet.NOUT=0;
     }
    return;
  }
  if(msc("NOUT",s1)){
     if ((notAlreadySet.NOUT||force) || ((mask!=NULL)&&(mask->NOUT==1)))
     {
      NJMP=atoi(s2);
      notAlreadySet.NOUT=0;
     }
    return;
  }
   if(msc("NMESH",s1)){
     if ((notAlreadySet.NMESH||force) || ((mask!=NULL)&&(mask->NMESH==1)))
     {
    	NMESH=atoi(s2);
	notAlreadySet.NMESH=0;
     }
    return;
  }
   if(msc("METH",s1)){
     if ((notAlreadySet.METHOD||force) || ((mask!=NULL)&&(mask->METHOD==1)))
     {
    for(i=0;i<15;i++)
      if(s2[0]==mkey[i]||s2[0]==Mkey[i])
	METHOD=i;
      
       notAlreadySet.METHOD=0;
     }
    return;
  }
   if(msc("VMAXPTS",s1)){
     if ((notAlreadySet.VMAXPTS||force) || ((mask!=NULL)&&(mask->VMAXPTS==1)))
     {
     	MaxPoints=atoi(s2);
	notAlreadySet.VMAXPTS=0;
     
     }
     return;
   }
   if(msc("MAXSTOR",s1)){ 
     if ((notAlreadySet.MAXSTOR||force) || ((mask!=NULL)&&(mask->MAXSTOR==1)))
     {
    	MAXSTOR=atoi(s2);
        notAlreadySet.MAXSTOR=0;
     } 
    return;
  }
   if(msc("TOR_PER",s1)){
     if ((notAlreadySet.TOR_PER||force) || ((mask!=NULL)&&(mask->TOR_PER==1)))
     {
     	TOR_PERIOD=atof(s2);
     	TORUS=1;
	notAlreadySet.TOR_PER=0;
     }
     return;
   }
   if(msc("JAC_EPS",s1)){
     if ((notAlreadySet.JAC_EPS||force) || ((mask!=NULL)&&(mask->JAC_EPS==1)))
     {
     	NEWT_ERR=atof(s2);
        notAlreadySet.JAC_EPS=0;
     }
     return;
   }
   if(msc("NEWT_TOL",s1)){
     if ((notAlreadySet.NEWT_TOL||force) || ((mask!=NULL)&&(mask->NEWT_TOL==1)))
     {
     	EVEC_ERR=atof(s2);
	notAlreadySet.NEWT_TOL=0;
     
     }
     return;
   }
   if(msc("NEWT_ITER",s1)){
     if ((notAlreadySet.NEWT_ITER||force) || ((mask!=NULL)&&(mask->NEWT_ITER==1)))
     {
     	EVEC_ITER=atoi(s2);
	notAlreadySet.NEWT_ITER=0;
     }
     return;
   }
  if(msc("FOLD",s1)){
     if ((notAlreadySet.FOLD||force) || ((mask!=NULL)&&(mask->FOLD==1)))
     {
     find_variable(s2,&i);
     if(i>0){
       itor[i-1]=1;
      TORUS=1;
     }
     
     }
     return;
   }
   if(msc("TOTAL",s1)){
    if ((notAlreadySet.TEND||force) || ((mask!=NULL)&&(mask->TEND==1)))
     {
    	TEND=atof(s2);
	notAlreadySet.TEND=0;
    }
    return;
  }
  if(msc("DTMIN",s1)){
     if ((notAlreadySet.DTMIN||force) || ((mask!=NULL)&&(mask->DTMIN==1)))
     {
    	HMIN=atof(s2);
         notAlreadySet.DTMIN=0;
     }
    return;
  }
  if(msc("DTMAX",s1)){
     if ((notAlreadySet.DTMAX||force) || ((mask!=NULL)&&(mask->DTMAX==1)))
     {
    	HMAX=atof(s2);
	notAlreadySet.DTMAX=0;
      }
    return;
  }
   if(msc("DT",s1)){
     if ((notAlreadySet.DT||force) || ((mask!=NULL)&&(mask->DT==1)))
     {
    	DELTA_T=atof(s2);
	notAlreadySet.DT=0;
     }
    return;
  }
   if(msc("T0",s1)){
     if ((notAlreadySet.T0||force) || ((mask!=NULL)&&(mask->T0==1)))
     { 
    	T0=atof(s2);
        notAlreadySet.T0=0;
     }
    return;
  }
   if(msc("TRANS",s1)){
     if ((notAlreadySet.TRANS||force) || ((mask!=NULL)&&(mask->TRANS==1)))
     {
     	TRANS=atof(s2);
        notAlreadySet.TRANS=0;
     }
    return;
  }
   if(msc("BOUND",s1)){
     if ((notAlreadySet.BOUND||force) || ((mask!=NULL)&&(mask->BOUND==1)))
     {
       BOUND=atof(s2);
       notAlreadySet.BOUND=0;
     }
    return;
  }
   if(msc("ATOL",s1)){
     if ((notAlreadySet.ATOLER||force) || ((mask!=NULL)&&(mask->ATOLER==1)))
     {
     	ATOLER=atof(s2);
        notAlreadySet.ATOLER=0;
     }
     return;
   }
   if(msc("TOL",s1)){
     if ((notAlreadySet.TOLER||force) || ((mask!=NULL)&&(mask->TOLER==1)))
     {
	TOLER=atof(s2);
	notAlreadySet.TOLER=0;
     }
    return;
  }
    
   if(msc("DELAY",s1)){
     if ((notAlreadySet.DELAY||force) || ((mask!=NULL)&&(mask->DELAY==1)))
     {
    	DELAY=atof(s2);
	notAlreadySet.DELAY=0;
     }
    return;
  }
   if(msc("BANDUP",s1)){
     if ((notAlreadySet.BANDUP||force) || ((mask!=NULL)&&(mask->BANDUP==1)))
     {
     	cv_bandflag=1;
     	cv_bandupper=atoi(s2);
     	notAlreadySet.BANDUP=0;
     }
     return;
   }
  if(msc("BANDLO",s1)){
     if ((notAlreadySet.BANDLO||force) || ((mask!=NULL)&&(mask->BANDLO==1)))
     {
     	cv_bandflag=1;
     	cv_bandlower=atoi(s2);
     	notAlreadySet.BANDLO=0;
     }
     return;
   }
  
  if(msc("PHI",s1)){
     if ((notAlreadySet.PHI||force) || ((mask!=NULL)&&(mask->PHI==1)))
     {
    	PHI0=atof(s2);
	notAlreadySet.PHI=0;
     }
    return;
  }
   if(msc("THETA",s1)){
     if ((notAlreadySet.THETA||force) || ((mask!=NULL)&&(mask->THETA==1)))
     {
    	THETA0=atof(s2);
	notAlreadySet.THETA=0;
     }
    return;
  }
   if(msc("XLO",s1)){
     if ((notAlreadySet.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
     {
    	MY_XLO=atof(s2);
	notAlreadySet.XLO=0;
     }
    return;
  }
   if(msc("YLO",s1)){
    if ((notAlreadySet.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
    {
    	MY_YLO=atof(s2);
	notAlreadySet.YLO=0;
    }
    return;
  }
  
   if(msc("XHI",s1)){
    if ((notAlreadySet.XHI||force) || ((mask!=NULL)&&(mask->XHI==1)))
    {
    	MY_XHI=atof(s2);
        notAlreadySet.XHI=0;
    }
    return;
  }
   if(msc("YHI",s1)){
     if ((notAlreadySet.YHI||force) || ((mask!=NULL)&&(mask->YHI==1)))
     {
    	MY_YHI=atof(s2);
        notAlreadySet.YHI=0;
     }
    return;
  }
   if(msc("XMAX",s1)){
     if ((notAlreadySet.XMAX||force) || ((mask!=NULL)&&(mask->XMAX==1)))
     {
    	x_3d[1]=atof(s2);
	notAlreadySet.XMAX=0;
     
     }
    return;
  }
   if(msc("YMAX",s1)){
     if ((notAlreadySet.YMAX||force) || ((mask!=NULL)&&(mask->YMAX==1)))
     {
        y_3d[1]=atof(s2);
	notAlreadySet.YMAX=0;
     }
    return;
  }
   if(msc("ZMAX",s1)){
     if ((notAlreadySet.ZMAX||force) || ((mask!=NULL)&&(mask->ZMAX==1)))
     {
        z_3d[1]=atof(s2);
	notAlreadySet.ZMAX=0;
     }
    return;
  }
   if(msc("XMIN",s1)){
     /*  printf("Trying to set XMIN %d =%s\n",notAlreadySet.XMIN,s2); */
     if ((notAlreadySet.XMIN||force) || ((mask!=NULL)&&(mask->XMIN==1)))
     {
        x_3d[0]=atof(s2);
	notAlreadySet.XMIN=0; 
	if ((notAlreadySet.XLO||force) || ((mask!=NULL)&&(mask->XLO==1)))
	{
    	   MY_XLO=atof(s2);
	   notAlreadySet.XLO=0;
	}
     }
    return;
  }
   if(msc("YMIN",s1)){
     if ((notAlreadySet.YMIN||force) || ((mask!=NULL)&&(mask->YMIN==1)))
     {
    	y_3d[0]=atof(s2);
	notAlreadySet.YMIN=0;
	if ((notAlreadySet.YLO||force) || ((mask!=NULL)&&(mask->YLO==1)))
	{
    	   MY_YLO=atof(s2);
	   notAlreadySet.YLO=0;
	}
     }
    return;
  }
 if(msc("ZMIN",s1)){
     if ((notAlreadySet.ZMIN||force) || ((mask!=NULL)&&(mask->ZMIN==1)))
     {
    	z_3d[0]=atof(s2);
	notAlreadySet.ZMIN=0;
     }
    return;
  }

 if(msc("POIMAP",s1)){
     if ((notAlreadySet.POIMAP||force) || ((mask!=NULL)&&(mask->POIMAP==1)))
     {
   	if(s2[0]=='m'||s2[0]=='M')POIMAP=2;
   	if(s2[0]=='s'||s2[0]=='S')POIMAP=1;
   	if(s2[0]=='p'||s2[0]=='P')POIMAP=3;
   	notAlreadySet.POIMAP=0;
   }
   return;
 }

 if(msc("POIVAR",s1)){
     if ((notAlreadySet.POIVAR||force) || ((mask!=NULL)&&(mask->POIVAR==1)))
     {
    	find_variable(s2,&i);
    	if(i>-1)POIVAR=i;
	
	notAlreadySet.POIVAR=0;
     
     }
    return;
  }
 if(msc("OUTPUT",s1)){
     if ((notAlreadySet.OUTPUT||force) || ((mask!=NULL)&&(mask->OUTPUT==1)))
     {
   	XPP_FORMAT_TO_BUF(batch_options.out_file,"{}",s2);
	notAlreadySet.OUTPUT=0;
     }
   return;
 }
  
 if(msc("POISGN",s1)){
     if ((notAlreadySet.POISGN||force) || ((mask!=NULL)&&(mask->POISGN==1)))
     {
   	POISGN=atoi(s2);
	notAlreadySet.POISGN=0;
     }
   return;
 }
 
 if(msc("POISTOP",s1)){
     if ((notAlreadySet.POISTOP||force) || ((mask!=NULL)&&(mask->POISTOP==1)))
     {
   	SOS=atoi(s2);
	notAlreadySet.POISTOP=0;
     }
   return;
 }
 if(msc("STOCH",s1)){
     if ((notAlreadySet.STOCH||force)|| ((mask!=NULL)&&(mask->STOCH==1)))
     {
   	STOCH_FLAG=atoi(s2);
	notAlreadySet.STOCH=0;
     }
   return;
 }
 if(msc("POIPLN",s1)){
     if ((notAlreadySet.POIPLN||force)|| ((mask!=NULL)&&(mask->POIPLN==1)))
     {
   	POIPLN=atof(s2);
	notAlreadySet.POIPLN=0;
     }
   return;
 }
  
 

 if(msc("RANGEOVER",s1)){
     if ((notAlreadySet.RANGEOVER||force)|| ((mask!=NULL)&&(mask->RANGEOVER==1)))
     {
    	snprintf(range.item,sizeof(range.item),"%s",s2);
	notAlreadySet.RANGEOVER=0;
     }

    return;
  }
 if(msc("RANGESTEP",s1)){
     if ((notAlreadySet.RANGESTEP||force)|| ((mask!=NULL)&&(mask->RANGESTEP==1)))
     {
        
   	range.steps=atoi(s2);
	notAlreadySet.RANGESTEP=0;
     }
   return;
 }
  
 if(msc("RANGELOW",s1)){
     if ((notAlreadySet.RANGELOW||force)|| ((mask!=NULL)&&(mask->RANGELOW==1)))
     {
   	range.plow=atof(s2);
   	notAlreadySet.RANGELOW=0;
     }

   return;
 }

 if(msc("RANGEHIGH",s1)){
     if ((notAlreadySet.RANGEHIGH||force)|| ((mask!=NULL)&&(mask->RANGEHIGH==1)))
     {
   	range.phigh=atof(s2);
	notAlreadySet.RANGEHIGH=0;
     }
   return;
 }
 
 if(msc("RANGERESET",s1)){
     if ((notAlreadySet.RANGERESET||force)|| ((mask!=NULL)&&(mask->RANGERESET==1)))
     {
	 if(s2[0]=='y'||s2[0]=='Y')
	 {
	  range.reset=1;
	 }
	 else
	 {
	  range.reset=0;
	 } 
	  notAlreadySet.RANGERESET=0;
     }
  	return;
   }

 if(msc("RANGEOLDIC",s1)){
     if ((notAlreadySet.RANGEOLDIC||force)|| ((mask!=NULL)&&(mask->RANGEOLDIC==1)))
     {
  	if(s2[0]=='y'||s2[0]=='Y')
	{
   		range.oldic=1;
   	}
	else
	{ 
   		range.oldic=0;
	}
	
   	notAlreadySet.RANGEOLDIC=0;
     }
      return;
 }
 
   
 if(msc("RANGE",s1)){
     if ((notAlreadySet.RANGE||force)|| ((mask!=NULL)&&(mask->RANGE==1)))
     {
   	batch_options.range=atoi(s2);
	notAlreadySet.RANGE=0;
     }
   return;
 }
 
 if(msc("NTST",s1)){
     if ((notAlreadySet.NTST||force)|| ((mask!=NULL)&&(mask->NTST==1)))
     {
   	auto_ntst=atoi(s2);
	notAlreadySet.NTST=0;
     }
   return;
 }
if(msc("NMAX",s1)){
   if ((notAlreadySet.NMAX||force)|| ((mask!=NULL)&&(mask->NMAX==1)))
   {
   	auto_nmx=atoi(s2);
	notAlreadySet.NMAX=0;
   }
   return;
 }
if(msc("NPR",s1)){
   if ((notAlreadySet.NPR||force)|| ((mask!=NULL)&&(mask->NPR==1)))
   {
   	auto_npr=atoi(s2);
	notAlreadySet.NPR=0;
   }
   return;
 }
 if(msc("NCOL",s1)){
   if ((notAlreadySet.NCOL||force)|| ((mask!=NULL)&&(mask->NCOL==1)))
   {
   	auto_ncol=atoi(s2);
   	notAlreadySet.NCOL=0;
   }
   return;
 }


if(msc("DSMIN",s1)){
   if ((notAlreadySet.DSMIN||force)|| ((mask!=NULL)&&(mask->DSMIN==1)))
   {
   	auto_dsmin=atof(s2);
	notAlreadySet.DSMIN=0;
   }
   return;
 }
if(msc("DSMAX",s1)){
   if ((notAlreadySet.DSMAX||force)|| ((mask!=NULL)&&(mask->DSMAX==1)))
   {
   	auto_dsmax=atof(s2);
   	notAlreadySet.DSMAX=0;
   }
   return;
 }
if(msc("DS",s1)){
    if ((notAlreadySet.DS||force)|| ((mask!=NULL)&&(mask->DS==1)))
    {
   	auto_ds=atof(s2);
	notAlreadySet.DS=0;
    }
 
   return;
 }
if(msc("PARMIN",s1)){
   if ((notAlreadySet.XMAX||force)|| ((mask!=NULL)&&(mask->XMAX==1)))
   {
   	auto_rl0=atof(s2);
	notAlreadySet.XMAX=0;
   }
   return;
 }
if(msc("PARMAX",s1)){
    if ((notAlreadySet.PARMAX||force)|| ((mask!=NULL)&&(mask->PARMAX==1)))
    {
   	auto_rl1=atof(s2);
	notAlreadySet.PARMAX=0;
    }
   return;
 }
if(msc("NORMMIN",s1)){
     if ((notAlreadySet.NORMMIN||force)|| ((mask!=NULL)&&(mask->NORMMIN==1)))
     {
   	auto_a0=atof(s2);
	notAlreadySet.NORMMIN=0;
     }
   return;
 }
if(msc("NORMMAX",s1)){
     if ((notAlreadySet.NORMMAX||force)|| ((mask!=NULL)&&(mask->NORMMAX==1)))
     {
   	auto_a1=atof(s2);
   	notAlreadySet.NORMMAX=0;
     }
   return;
 }
 if(msc("EPSL",s1)){
     if ((notAlreadySet.EPSL||force)|| ((mask!=NULL)&&(mask->EPSL==1)))
     {
   	auto_epsl=atof(s2);
	notAlreadySet.EPSL=0;
     }
   return;
 }

if(msc("EPSU",s1)){
     if ((notAlreadySet.EPSU||force)|| ((mask!=NULL)&&(mask->EPSU==1)))
     {
   	auto_epsu=atof(s2);
	notAlreadySet.EPSU=0;
     }
   return;
 }
if(msc("EPSS",s1)){
     if ((notAlreadySet.EPSS||force)|| ((mask!=NULL)&&(mask->EPSS==1)))
     {
   	auto_epss=atof(s2);
	notAlreadySet.EPSS=0;
     }
   return;
 }
 if(msc("RUNNOW",s1)){
     if ((notAlreadySet.RUNNOW||force)|| ((mask!=NULL)&&(mask->RUNNOW==1)))
     {
   	RunImmediately=atoi(s2);
	notAlreadySet.RUNNOW=0;
     }
   return;
 }

 if(msc("SEC",s1)){
     if ((notAlreadySet.SEC||force)|| ((mask!=NULL)&&(mask->SEC==1)))
     {
   	SEc=atoi(s2);
	notAlreadySet.SEC=0;
     }
   return;
 }
 if(msc("UEC",s1)){
     if ((notAlreadySet.UEC||force)|| ((mask!=NULL)&&(mask->UEC==1)))
     {
   	UEc=atoi(s2);
	notAlreadySet.UEC=0;
     }
   return;
 }
 if(msc("SPC",s1)){
     if ((notAlreadySet.SPC||force)|| ((mask!=NULL)&&(mask->SPC==1)))
     {
   	SPc=atoi(s2);
	notAlreadySet.SPC=0;
     }
   return;
 }
 if(msc("UPC",s1)){
     if ((notAlreadySet.UPC||force)|| ((mask!=NULL)&&(mask->UPC==1)))
     {
   	UPc=atoi(s2);
	notAlreadySet.UPC=0;
     }
   return;
 }

 if(msc("AUTOEVAL",s1)){
     if ((notAlreadySet.AUTOEVAL||force)|| ((mask!=NULL)&&(mask->AUTOEVAL==1)))
     {
   	f=atoi(s2);
   	set_auto_eval_flags(f);
	notAlreadySet.AUTOEVAL=0;
    }
   return;
 }
if(msc("AUTOXMAX",s1)){
     if ((notAlreadySet.AUTOXMAX||force)|| ((mask!=NULL)&&(mask->AUTOXMAX==1)))
     {
 	auto_xmax=atof(s2);
	notAlreadySet.AUTOXMAX=0;
     }
 return;
}
if(msc("AUTOYMAX",s1)){
     if ((notAlreadySet.AUTOYMAX||force)|| ((mask!=NULL)&&(mask->AUTOYMAX==1)))
     {
 		auto_ymax=atof(s2);
		notAlreadySet.AUTOYMAX=0;
     }
 return;
}
if(msc("AUTOXMIN",s1)){
     if ((notAlreadySet.AUTOXMIN||force)|| ((mask!=NULL)&&(mask->AUTOXMIN==1)))
     {
 	auto_xmin=atof(s2);
	notAlreadySet.AUTOXMIN=0;
     }
 return;
}
if(msc("AUTOYMIN",s1)){
     if ((notAlreadySet.AUTOYMIN||force)|| ((mask!=NULL)&&(mask->AUTOYMIN==1)))
     {
 	auto_ymin=atof(s2);
	notAlreadySet.AUTOYMIN=0;
     }
 return;
}
if(msc("AUTOVAR",s1)){
     if ((notAlreadySet.AUTOVAR||force)|| ((mask!=NULL)&&(mask->AUTOVAR==1)))
     {
     	find_variable(s2,&i);
    	if(i>0)auto_var=i-1;
	notAlreadySet.AUTOVAR=0;
    }
    return;
  }

/* postscript options */

 if(msc("PS_FONT",s1)){
     if ((notAlreadySet.PS_FONT||force)|| ((mask!=NULL)&&(mask->PS_FONT==1)))
     {
   	XPP_FORMAT_TO_BUF(PS_FONT,"{}",s2);
	notAlreadySet.PS_FONT=0;
     }
   return;
 }

if(msc("PS_LW",s1)){
   if ((notAlreadySet.PS_LW||force)|| ((mask!=NULL)&&(mask->PS_LW==1)))
   {
  	PS_LW=atof(s2);
	notAlreadySet.PS_LW=0;
   }
   return;
 }

if(msc("PS_FSIZE",s1)){
     if ((notAlreadySet.PS_FSIZE||force)|| ((mask!=NULL)&&(mask->PS_FSIZE==1)))
     {
  	PS_FONTSIZE=atoi(s2);
	notAlreadySet.PS_FSIZE=0;
     }
   return;
 }

if(msc("PS_COLOR",s1)){
     if ((notAlreadySet.PS_COLOR||force)|| ((mask!=NULL)&&(mask->PS_COLOR==1)))
     {
  	PSColorFlag=atoi(s2);
  	plot_export.color=PSColorFlag;
	notAlreadySet.PS_COLOR=0;
     }
   return;
 }
if(msc("TUTORIAL",s1)){
   if(!(msc(s2,"0")||msc(s2,"1")))
   {
   	xpp_log(XPP_LOG_ERROR, "TUTORIAL option must be 0 or 1.\n");
	exit(-1);
   }
   if ((notAlreadySet.TUTORIAL||force) || ((mask!=NULL)&&(mask->TUTORIAL==1)))
   {
   	program.tutorial=atoi(s2);
	notAlreadySet.TUTORIAL=0;
   }
   return;
 }
 if(msc("S1",s1)){
     if ((notAlreadySet.SLIDER1||force) || ((mask!=NULL)&&(mask->SLIDER1==1)))
     {
	snprintf(sliders[0].var,sizeof(sliders[0].var),"%s",s2);
	notAlreadySet.SLIDER1=0;
     }
    return;
  }

if(msc("S2",s1)){
     if ((notAlreadySet.SLIDER2||force) || ((mask!=NULL)&&(mask->SLIDER2==1)))
     {
    	snprintf(sliders[1].var,sizeof(sliders[1].var),"%s",s2);
	notAlreadySet.SLIDER2=0;
     }
    return;
   }
 if(msc("S3",s1)){
     if ((notAlreadySet.SLIDER3||force) || ((mask!=NULL)&&(mask->SLIDER3==1)))
     {	
     	snprintf(sliders[2].var,sizeof(sliders[2].var),"%s",s2);
	notAlreadySet.SLIDER3=0;
     }
    return;
  }
  if(msc("SLO1",s1)){
     if ((notAlreadySet.SLIDER1LO||force) || ((mask!=NULL)&&(mask->SLIDER1LO==1)))
     {
    	sliders[0].lo=atof(s2);
	notAlreadySet.SLIDER1LO=0;
     }
    return;
  }

if(msc("SLO2",s1)){
     if ((notAlreadySet.SLIDER2LO||force) || ((mask!=NULL)&&(mask->SLIDER2LO==1)))
     {
    	sliders[1].lo=atof(s2);
	notAlreadySet.SLIDER2LO=0;
     }
    return;
   }
 if(msc("SLO3",s1)){
     if ((notAlreadySet.SLIDER3LO||force) || ((mask!=NULL)&&(mask->SLIDER3LO==1)))
     {
    	sliders[2].lo=atof(s2);
	notAlreadySet.SLIDER3LO=0;
     }
    return;
  }
 if(msc("SHI1",s1)){
     if ((notAlreadySet.SLIDER1HI||force) || ((mask!=NULL)&&(mask->SLIDER1HI==1)))
     {
    	sliders[0].hi=atof(s2);
	notAlreadySet.SLIDER1HI=0;
     }
    return;
  }
 if(msc("SHI2",s1)){
     if ((notAlreadySet.SLIDER2HI||force) || ((mask!=NULL)&&(mask->SLIDER2HI==1)))
     {
    	sliders[1].hi=atof(s2);
	notAlreadySet.SLIDER2HI=0;
     }
    return;
   }
 if(msc("SHI3",s1)){
     if ((notAlreadySet.SLIDER3HI||force) || ((mask!=NULL)&&(mask->SLIDER3HI==1)))
     {
    	sliders[2].hi=atof(s2);
	notAlreadySet.SLIDER3HI=0;
     }
    return;
 }

 /* postprocessing options
    This is rally only relevant for batch jobs as it 
    writes files then
 */

 if(msc("POSTPROCESS",s1)){
     if ((notAlreadySet.POSTPROCESS||force) || ((mask!=NULL)&&(mask->POSTPROCESS==1)))
     {
    	post_process=atoi(s2);
	notAlreadySet.POSTPROCESS=0;
     }
    return;
   }
   
 if(msc("HISTLO",s1)){
     if ((notAlreadySet.HISTLO||force) || ((mask!=NULL)&&(mask->HISTLO==1)))
     {
    	hist_inf.xlo=atof(s2);
	notAlreadySet.HISTLO=0;
     }
    return;
  }

 if(msc("HISTHI",s1)){
     if ((notAlreadySet.HISTHI||force) || ((mask!=NULL)&&(mask->HISTHI==1)))
     {
    	hist_inf.xhi=atof(s2);
	notAlreadySet.HISTHI=0;
     }
    return;
  }

 if(msc("HISTBINS",s1)){
     if ((notAlreadySet.HISTBINS||force) || ((mask!=NULL)&&(mask->HISTBINS==1)))
     {
    	hist_inf.nbins=atoi(s2);
	notAlreadySet.HISTBINS=0;
     }
    return;
  }

 if(msc("HISTCOL",s1)){
     if ((notAlreadySet.HISTCOL||force) || ((mask!=NULL)&&(mask->HISTCOL==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) hist_inf.col=i;
	notAlreadySet.HISTCOL=0;
     }
    return;
  }

 if(msc("HISTLO2",s1)){
     if ((notAlreadySet.HISTLO2||force) || ((mask!=NULL)&&(mask->HISTLO2==1)))
     {
    	hist_inf.ylo=atof(s2);
	notAlreadySet.HISTLO2=0;
     }
    return;
  }

 if(msc("HISTHI2",s1)){
     if ((notAlreadySet.HISTHI2||force) || ((mask!=NULL)&&(mask->HISTHI2==1)))
     {
    	hist_inf.yhi=atof(s2);
	notAlreadySet.HISTHI2=0;
     }
    return;
  }

 if(msc("HISTBINS2",s1)){
     if ((notAlreadySet.HISTBINS2||force) || ((mask!=NULL)&&(mask->HISTBINS2==1)))
     {
    	hist_inf.nbins2=atoi(s2);
	notAlreadySet.HISTBINS2=0;
     }
    return;
  }

 if(msc("HISTCOL2",s1)){
     if ((notAlreadySet.HISTCOL2||force) || ((mask!=NULL)&&(mask->HISTCOL2==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) hist_inf.col2=i;
	notAlreadySet.HISTCOL2=0;
     }
    return;
  }


 if(msc("SPECCOL",s1)){
     if ((notAlreadySet.SPECCOL||force) || ((mask!=NULL)&&(mask->SPECCOL==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) spec_col=i;
	notAlreadySet.SPECCOL=0;
     }
    return;
  }

 if(msc("SPECCOL2",s1)){
     if ((notAlreadySet.SPECCOL2||force) || ((mask!=NULL)&&(mask->SPECCOL2==1)))
     {
       find_variable(s2,&i);
       if(i>(-1)) spec_col2=i;
	notAlreadySet.SPECCOL2=0;
     }
    return;
  }

 if(msc("SPECWIDTH",s1)){
     if ((notAlreadySet.SPECWIDTH||force) || ((mask!=NULL)&&(mask->SPECWIDTH==1)))
     {
       spec_wid=atoi(s2);
	notAlreadySet.SPECWIDTH=0;
     }
    return;
  }

 if(msc("SPECWIN",s1)){
     if ((notAlreadySet.SPECWIN||force) || ((mask!=NULL)&&(mask->SPECWIN==1)))
     {
       spec_win=atoi(s2);
	notAlreadySet.SPECWIN=0;
     }
    return;
  }


  if(msc("DFGRID",s1)){
     if ((notAlreadySet.DFGRID||force)|| ((mask!=NULL)&&(mask->DFGRID==1)))
     { 
     	DF_GRID=atoi(s2);
	notAlreadySet.DFGRID=0;
     }
   return;
 }
  if(msc("DFDRAW",s1)){ 
     if ((notAlreadySet.DFBATCH||force)|| ((mask!=NULL)&&(mask->DFBATCH==1)))
     { 
     	DFBatch=atoi(s2);
	notAlreadySet.DFBATCH=0;
     }
   return;
 }
   if(msc("NCDRAW",s1)){
     if ((notAlreadySet.NCBATCH||force)|| ((mask!=NULL)&&(mask->NCBATCH==1)))
     { 
     	NCBatch=atoi(s2);
	notAlreadySet.NCBATCH=0;
     }
   return;
   }

   /* colorize customizing !! */
   if(msc("COLORVIA",s1))
     {
       if ((notAlreadySet.COLORVIA||force)|| ((mask!=NULL)&&(mask->COLORVIA==1)))
       snprintf(ColorVia,sizeof(ColorVia),"%s",s2);
       	notAlreadySet.COLORVIA=0;
       return;
     }
   if(msc("COLORIZE",s1))
     {
          if ((notAlreadySet.COLORIZE||force)|| ((mask!=NULL)&&(mask->COLORIZE==1)))
       ColorizeFlag=atoi(s2);
          	notAlreadySet.COLORIZE=0;
          return;
     }
   if(msc("COLORLO",s1))
     {
              if ((notAlreadySet.COLORLO||force)|| ((mask!=NULL)&&(mask->COLORLO==1)))
       ColorViaLo=atof(s2);
	             	notAlreadySet.COLORLO=0;
          return;
     }
   if(msc("COLORHI",s1))
     {
              if ((notAlreadySet.COLORHI||force)|| ((mask!=NULL)&&(mask->COLORHI==1)))
       ColorViaHi=atof(s2);
              	notAlreadySet.COLORHI=0;
       return;
     }

xpp_log(XPP_LOG_WARN, "Option %s not recognized\n",s1);
  
}





