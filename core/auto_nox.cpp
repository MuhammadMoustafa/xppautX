#include "integrate.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include <string>
#include "numerics.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include <string.h>
#include "parserslow.h"
#include "autevd.h"
#include "run_auto.h"
#include "auto_nox.h"
#include "auto_stop.h"
#include "auto_stability.h"
#include "auto_x11.h"
#include <libgen.h>
/* #include "f2c.h" */
#include "auto_f2c.h"
#include "auto_c.h"
#include "graf_par.h"

#include "load_eqn.h"

#include "read_dir.h"
#include "pp_shoot.h"

#include "read_dir.h"
/*#include "chunk.h"
*/

#include "kinescope.h"

#include "parserslow.h"
/*#include "graf_par.h"
*/
#include "ggets.h"

#include "init_conds.h"
#include "diagram.h"
#include "many_pops.h"
#include "browse.h"
#include "pop_list.h"



#include "menudrive.h"
#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifndef WCTYPE
#include <ctype.h>
#else
#include <wctype.h>
#endif

#include "axes2.h"
#include "graphics.h"

#include "xpplim.h"
#include "autlim.h"
#include "xAuto.h" 
#include "xpp_job.h"
#include "auto_data.h"
#include "derived.h"   /* evaluate_derived() */
#include "tabular.h"   /* redo_all_fun_tables() */
#include "my_rhs.h"    /* extra() */

namespace {
/* The dialog API (auto_pop_up_list, do_string_box, TwoChoice, err_msg,
   file_selector, ...: xpp_ui.h, pop_list.h) takes char * and char ** and
   writes through none of them, the historical C API shared far beyond
   this file; str() and strs() cast a literal or a table of literals for
   one of these calls (diagram.cpp's str()). */
char *str(const char *s) { return const_cast<char *>(s); }
char **strs(const char **s) { return const_cast<char **>(s); }
} // namespace

#define PACK_AUTO 0
#define PACK_LBF 1
#define PARAM_BOX 1

#define RUBBOX 0
#define RUBLINE 1

/* #define RIGHT 6
   #define LEFT 2 */
#define ESC 27
#define TAB 10
#define BAD 0
#define FINE 13

#define UPT 6
#define SPT 7

#define OPEN_3 1
#define NO_OPEN_3 0
#define OVERWRITE 0
#define APPEND 1

/* calculation types */

int TypeOfCalc=0;
#define LPE2 1
#define LPP2 2
#define HB2 3
#define TR2 4
#define BR2 5
#define PD2 6
#define FP2 7
#define BV1 8
#define EQ1 9
#define PE1 10
#define DI1 11
#define HO2 12

#define STD_WID 460       /* golden mean  */
#define STD_HGT 284
#define HI_P 0  /* uhi vs par */
#define NR_P 1  /* norm vs par */
#define HL_P 2  /* Hi and Lo vs par  periodic only */
#define PE_P 3  /* period vs par   */
#define P_P  4  /* param vs param  */

#define FR_P 10  /* freq vs par   */
#define AV_P 11 /* ubar vs par */
#define SPECIAL 5
#define SPER 3
#define UPER 4
#define CSEQ 1
#define CUEQ 2

#define DISCRETE 0
extern XAUTO xAuto;
/* the label the running continuation started from (Auto.irs), for its
   first point: do_auto sets it, addbif takes it (auto_run_from_take) */
static int run_from;
extern int leng[MAXODE];
extern double TOR_PERIOD;
extern float **storage;
extern int storind;
extern double constants[];
extern int PointType;
extern int xorfix;
extern int NoBreakLine;
extern char *auto_hint[],*aaxes_hint[],*afile_hint[],*arun_hint[],*no_hint[];
extern int BVP_FLAG;

extern int fp8_is_open;
extern FILE *fp8;


/*extern char *strdup(const char *s);
*/

extern int FLOWK;

namespace {
/* the diagram's marked stretch (the S and E keys in the Grab loop): the
   branch and point numbers of its start and end, and where they are drawn */
struct DiagramMark {
    int state = 0;        /* 0 nothing, 1 start marked, 2 start and end */
    int start_branch = 0, end_branch = 0;
    int start_point = 0, end_point = 0;
    int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
};
DiagramMark diagram_mark;
int auto_redraw = 1; /* AUTO's File menu Redraw toggle: only reported */
} // namespace
int SEc=20;
int UEc=0;
int SPc=26;
int UPc=28;
int HBc=0;
int LPc=20;
/*  two parameter colors  need to do this
    LP is 20 (red)
    HB  is  28 blue
    TR  is  26 (green) 
    PD  is 24  (orange)
    BR  is  27 (turquoise)
    FP  is 25  (olive)
*/

int LPP_color=0;
int LPE_color=20;
int HB_color=28;
int TR_color=26;
int PD_color=23;
int BR_color=27;
int FP_color=25;
int HO_color=29;

int RestartLabel=0;
int auto_ntst=15,auto_nmx=200,auto_npr=50,auto_ncol=4;
double auto_ds=.02,  auto_dsmax=.5,  auto_dsmin=.001;
double auto_rl0=0.0,auto_rl1=2,auto_a0=0.0,auto_a1=1000.;
double  auto_xmax=2.5,  auto_xmin=-.5,auto_ymax=3.0,auto_ymin=-3.0;
double auto_epsl=1e-4,auto_epsu=1e-4,auto_epss=1e-4;
int auto_var=0;

int is_3_there=0;

int load_all_labeled_orbits=0;

int SuppressBP=0;
ROTCHK blrtn;
  


/* gogoauto.c and diagram.cpp declare these in no header */
extern "C" int go_go_auto(void);
extern "C" void load_browser_with_branch(int ibr, int pts, int pte);

GRABPT grabpt;

extern double MyData[MAXODE];
extern DIAGRAM *bifd;

extern int NBifs;
int AutoTwoParam=0;
int NAutoPar=8;
int Auto_index_to_array[8];
int AutoPar[8];


extern unsigned int MyBackColor,MyForeColor,GrFore,GrBack;


double outperiod[20];
integer UzrPar[20];
int NAutoUzr;


/*extern char this_file[100];*/
extern char this_file[XPP_MAX_NAME];

char this_auto_file[200];
char fort3[200];
char fort7[200];
char fort8[200];
char fort9[200];


extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern char upar_names[MAXPAR][XPP_NAME_MAX+1];
extern int NUPAR;
unsigned int DONT_XORCross=0;

 
double XfromAuto,YfromAuto;
int FromAutoFlag=0;

extern int NODE,NEQ;
extern int METHOD;

int HomoFlag=0;
int sparity=0;
double homo_l[100],homo_r[100];
double HOMO_SHIFT=0.0;
extern char uvar_names[MAXODE][XPP_NAME_MAX+1];

extern int storind;

BIFUR Auto;
ADVAUTO aauto;

int NewPeriodFlag;

AUTOAX Old1p;
AUTOAX Old2p;

/* color plot stuff */
void colset(int type )
{
  switch(type) {
  case CSEQ:
    autocol(SEc);
    break;
 case CUEQ:
    autocol(UEc);
    break;
 case SPER:
    autocol(SPc);
    break;
 case UPER:
    autocol(UPc);
    break;
  }
  
}

void pscolset2(int flag2)
{
   switch(flag2){
  case LPE2:
    set_linestyle(LPE_color-19);
    break;
  case LPP2:
    set_linestyle(LPP_color);
    break;
  case HB2:
    set_linestyle(HB_color-19);
    break;
  case TR2:
    set_linestyle(TR_color-19);
    break;
  case BR2:
    set_linestyle(BR_color-19);
    break;
  case PD2:
    set_linestyle(PD_color-19);
    break;
  case FP2:
     set_linestyle(FP_color-19);
    break;
  default:
    set_linestyle(0);
  }


}
void colset2(int flag2)
{
  LineWidth(2);
  switch(flag2){
  case LPE2:
    autocol(LPE_color);
    break;
  case LPP2:
    autocol(LPP_color);
    break;
  case HB2:
    autocol(HB_color);
    break;
  case TR2:
    autocol(TR_color);
    break;
  case BR2:
    autocol(BR_color);
    break;
  case PD2:
    autocol(PD_color);
    break;
  case FP2:
     autocol(FP_color);
    break;
  default:
    autocol(0);
  }


}

void storeautopoint(double x,double y)
{
  if(Auto.plot==P_P){
    XfromAuto=x;
    YfromAuto=y;
    FromAutoFlag=1;
  }
}
void setautopoint()
{
  if(FromAutoFlag)
    {
      FromAutoFlag=0;
      set_val(upar_names[AutoPar[Auto.icp1]],XfromAuto);
      set_val(upar_names[AutoPar[Auto.icp2]],YfromAuto);
      evaluate_derived();
      redo_all_fun_tables();
      redraw_params();
    }
}
      
void get_auto_str(char *xlabel, char *ylabel)
{
  /* xlabel/ylabel are pointers here; every caller passes a
     char[AUTO_LABEL_LEN] (draw_ps_axes, draw_svg_axes, draw_bif_axes,
     ui_json.cpp's dg_ax), so that is the real size. */
 xpp_snprintf(xlabel,AUTO_LABEL_LEN,"%s",upar_names[AutoPar[Auto.icp1]]);
 switch(Auto.plot){
 case HI_P:
 case HL_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"%s",uvar_names[Auto.var]);
   break;
 case NR_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"Norm");
   break;
 case PE_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"Period");
   break;
 case FR_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"Frequency");
   break;
 case P_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"%s",upar_names[AutoPar[Auto.icp2]]);
   break;
 case AV_P:
   xpp_snprintf(ylabel,AUTO_LABEL_LEN,"%s_bar",uvar_names[Auto.var]);
   break;
 }
}

void draw_ps_axes()
{
 char sx[AUTO_LABEL_LEN],sy[AUTO_LABEL_LEN];
 set_scale(Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax);
 get_auto_str(sx,sy);
 Box_axis(Auto.xmin,Auto.xmax,Auto.ymin,Auto.ymax,sx,sy,0);
}

void draw_svg_axes()
{
 char sx[AUTO_LABEL_LEN],sy[AUTO_LABEL_LEN];
 set_scale(Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax);
 get_auto_str(sx,sy);
 Box_axis(Auto.xmin,Auto.xmax,Auto.ymin,Auto.ymax,sx,sy,0);
}

void draw_bif_axes()
{
 int x0=Auto.x0,y0=Auto.y0,ii,i0;
 int x1=x0+Auto.wid,y1=y0+Auto.hgt;
 char junk[20],xlabel[AUTO_LABEL_LEN],ylabel[AUTO_LABEL_LEN];
 clear_auto_plot();
 ALINE(x0,y0,x1,y0);
 ALINE(x1,y0,x1,y1);
 ALINE(x1,y1,x0,y1);
 ALINE(x0,y1,x0,y0);
 XPP_SPRINTF(junk,"%g",Auto.xmin);
 ATEXT(x0,y1+text_metrics.small_height+2,junk);
 XPP_SPRINTF(junk,"%g",Auto.xmax);
 ii=strlen(junk)*text_metrics.small_width;
 ATEXT(x1-ii,y1+text_metrics.small_height+2,junk);
 XPP_SPRINTF(junk,"%g",Auto.ymin);
 ii=strlen(junk);
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y1,junk);
 XPP_SPRINTF(junk,"%g",Auto.ymax);
 ii=strlen(junk);
 i0=9-ii;
 if(i0<0)i0=0;
 ATEXT(i0*text_metrics.small_width,y0+text_metrics.small_height,junk);
 get_auto_str(xlabel,ylabel);
 ATEXT((x0+x1)/2,y1+text_metrics.small_height+2,xlabel);
 ATEXT(10*text_metrics.small_width,text_metrics.small_height,ylabel);
 auto_diagram(NULL); /* the data of the diagram starts again too */
 refreshdisplay();
}
   



int IXVal(double x)
{
  double temp=(double)Auto.wid*(x-Auto.xmin)/(Auto.xmax-Auto.xmin);
  return ((int) temp+Auto.x0);
}

int IYVal(double y)
{
  double temp=(double)Auto.hgt*(y-Auto.ymin)/(Auto.ymax-Auto.ymin);
  return(Auto.hgt-(int)temp+Auto.y0);
}

int chk_auto_bnds(int ix,int iy)
{
  int x1=Auto.x0,x2=Auto.x0+Auto.wid;
  int y1=Auto.y0,y2=Auto.y0+Auto.hgt;
  if((ix>=x1)&&(ix<x2)&&(iy>=y1)&&(iy<y2))return 1;
  return 0;
}
/*   File manipulation stuff  */
void renamef(char *old, char *new_name)
{
 /* POSIX rename() replaces an existing destination; on Windows it fails, so
    the old .s was silently kept and fort.8 left behind. */
 if(rename(old,new_name)==0)return;
 remove(new_name);
 if(rename(old,new_name)==0)return;
 copyf(old,new_name);   /* the source may still be open: copy, then try to drop it */
 remove(old);
}

/* the rest of from, byte for byte, into to */
static void copy_bytes(FILE *from, FILE *to)
{
  char buf[1<<16];
  size_t n;
  while((n=fread(buf,1,sizeof buf,from))>0)
    fwrite(buf,1,n,to);
}

void copyf(char *old, char *new_name)
{
 FILE *fo;
 /* Binary: these files carry AUTO's own line ends and text mode would
    rewrite them. Both opens are checked -- on Windows fopen fails while the
    file is still open elsewhere, and writing into a NULL FILE * left fort.3
    empty, which AUTO then reported as "Restart label N not found". The
    copy goes through a temp file renamed into place (xpp::Writer), so
    new_name is either the whole copy or left as it was. */
 fo=fopen(old,"rb");
 if(fo==NULL){
   plintf("Cannot read %s \n",old);
   return;
 }
 xpp::Writer w=xpp::Writer::binary(new_name);
 if(!w){
   plintf("Cannot write %s \n",new_name);
   fclose(fo);
   return;
 }
 copy_bytes(fo,w.file());
 fclose(fo);
 w.commit();
}

/* new_name becomes old's bytes followed by its own */
void appendf(char *old, char *new_name)
{
 FILE *fo,*fn;
 fo=fopen(old,"rb");
 if(fo==NULL){
   plintf("Cannot read %s \n",old);
   return;
 }
 fn=fopen(new_name,"rb");
 if(fn==NULL){
     fclose(fo);

     copyf(old,new_name);
     return;
 }
 /* binary, like copyf(): text mode on Windows added a '\r' to every line;
    written beside new_name and renamed over it once whole */
 xpp::Writer w=xpp::Writer::binary(new_name);
 if(!w){
   xpp_log_auto("Can't write %s \n",new_name);
   fclose(fo);
   fclose(fn);
   return;
 }
 copy_bytes(fo,w.file());
 fclose(fo);
 copy_bytes(fn,w.file());
 fclose(fn);
 w.commit();
}
void deletef(char *old)
{
    remove(old);

}



void close_auto(int flg) /* labels compatible with A2K  */
{
  char string[1000];
  /* Close fp8 before the renames below: Windows refuses rename()/remove()
     on a file that is still open (see renamef/deletef), which left
     fort.8 behind next to <model>.s with the handle leaked. Linux allows
     renaming/removing an open file, which is likely why this was never
     turned on upstream -- it was dead code there, not a deliberate
     no-op. */
  if(fp8_is_open){
      fclose(fp8);
      fp8_is_open=0;
  }
  if(flg==0) {/*Overwrite*/
    XPP_SPRINTF(string,"%s.b",this_auto_file);
    renamef(fort7,string);
    XPP_SPRINTF(string,"%s.d",this_auto_file);
    renamef(fort9,string);

    XPP_SPRINTF(string,"%s.s",this_auto_file);
    renamef(fort8,string);
  }
  else {/*APPEND*/
    XPP_SPRINTF(string,"%s.b",this_auto_file);
    appendf(fort7,string);
    XPP_SPRINTF(string,"%s.d",this_auto_file); 
    appendf(fort9,string);  
    XPP_SPRINTF(string,"%s.s",this_auto_file);
    appendf(fort8,string);
  }

    deletef(fort8);

    fp8_is_open=0;
    deletef(fort7);
    deletef(fort9);
    deletef(fort3);

 
}

/* AUTO writes fort.3/7/8/9 under HOME. A HOME that is set but unusable
   (missing, not writable) must fall back to the model's directory like an
   unset one, or the opens fail deep inside autlib1.c. Probe by creating a
   scratch file: portable, and a directory can exist without being
   writable. */
static int dir_is_writable(const char *dir)
{
  char probe[300];
  FILE *fp;

  if (dir == NULL || dir[0] == 0)
    return 0;
  snprintf(probe, sizeof(probe), "%s/.xppautx_homecheck", dir);
  fp = fopen(probe, "w");
  if (fp == NULL)
    return 0;
  fclose(fp);
  remove(probe);
  return 1;
}

static char *auto_home_dir(char *dname)
{
  char *home;

  /* xppautX gives each session its own directory (xpp_globals.h) */
  if (program.auto_dir != NULL)
    return program.auto_dir;

  home = getenv("HOME");
  if (home == NULL || !dir_is_writable(home))
    home = dname;
  return home;
}

void create_auto_file_name()
{
 char *basec,*bname,*dirc,*dname;

  basec = xpp_strdup(this_file);
  dirc  = xpp_strdup(this_file);
  bname = (char*)basename(basec);
  dname = (char*)dirname(dirc);

  char* HOME = auto_home_dir(dname);

  XPP_SPRINTF(this_auto_file,"%s/%s",HOME,bname);
  xpp_free(basec); /* HOME may point into dirc: freed after its last use */
  xpp_free(dirc);
}

void open_auto(int flg) /* compatible with new auto */
{
  char string[210];
  char *basec,*bname,*dirc,*dname;

  basec = xpp_strdup(this_file);
  dirc  = xpp_strdup(this_file);
  bname = (char*)basename(basec);
  dname = (char*)dirname(dirc);

  char* HOME = auto_home_dir(dname);

  XPP_SPRINTF(this_auto_file,"%s/%s",HOME,bname);
  XPP_SPRINTF(fort3,"%s/%s",HOME,"fort.3");
  XPP_SPRINTF(fort7,"%s/%s",HOME,"fort.7");
  XPP_SPRINTF(fort8,"%s/%s",HOME,"fort.8");
  XPP_SPRINTF(fort9,"%s/%s",HOME,"fort.9");
  xpp_free(basec); /* HOME may point into dirc: freed after its last use */
  xpp_free(dirc);
  is_3_there=flg;

  if(flg==1){
    snprintf(string,sizeof string,"%s.s",this_auto_file);
    copyf(string,fort3);
  }

}

/* what a run continues, for auto_stability.h */
static int run_stability_kind(void)
{
  if(AutoTwoParam!=0||Auto.isw==2)return AUTO_STABILITY_OTHER;
  if(Auto.ips==2)return AUTO_STABILITY_PERIODIC;
  if(Auto.ips==1||Auto.ips==-1)return AUTO_STABILITY_STEADY;
  return AUTO_STABILITY_OTHER;
}

/* what a stored point is, for auto_stability.h: one-parameter periodic
   orbits have a negative branch (autlib1.cpp stplbv) */
static int point_stability_kind(const DIAGRAM *d)
{
  if(d->flag2!=0)return AUTO_STABILITY_OTHER;
  return d->ibr<0?AUTO_STABILITY_PERIODIC:AUTO_STABILITY_STEADY;
}

/* a run starts from Auto.irs's label, or from initial data: its first
   point takes the label's stored stability when it is the label's own
   solution, else it is not computed (auto_stability.h) */
static void stability_run_start(void)
{
  const DIAGRAM *d=Auto.irs>0?diagram_of_label(Auto.irs):NULL;
  if(d==NULL){
    auto_stability_run_start(run_stability_kind(),Auto.isw,AUTO_STABILITY_NONE,0,0,NULL,NULL);
    return;
  }
  auto_stability_run_start(run_stability_kind(),Auto.isw,point_stability_kind(d),d->itp,NODE,d->evr,d->evi);
}

/* MAIN Running routine  Assumes that Auto structure is set up */
static int auto_depth; /* do_auto's own follow-up runs (RestartLabel) are one run */

void do_auto(int iold, int isave, int itp)
{
      redraw_auto_menus();
      
    set_auto(); /* this sets up all the continuation initialization 
                   it is equivalent to reading in auto parameters
                   and running init in auto 
		*/
 
    open_auto(iold); /* this copies the relevant files .s  to fort.3 */
    if(auto_depth++==0)auto_stop_clear(); /* xppautX: T23: why this run's branches end */
    xpp_job_begin(0); /* Abort cancels it (xpp_job.h) */
    run_from=Auto.irs>0?Auto.irs:0; /* the diagram's data say where the run started */
    stability_run_start(); /* what its first point's stability is (auto_stability.h) */
    go_go_auto(); /* this complets the initialization and calls the 
                      main routines 
		  */
    run_from=0;
    if(xpp_job_cancelled())RestartLabel=0; /* xppautX: cancel: no follow-up run */
    xpp_job_end();
    /* plintf("AUTO opened it==%d\n",itp); */
    /*     run_aut(Auto.nfpar,itp); THIS WILL CHANGE TO gogoauto stuff */ 
    close_auto(isave); /* this copies fort.8 to the .s file and other 
                          irrelevant stuff 
		       */
    
    if(RestartLabel!=0){
      xpp_log_auto("RestartLabel=%d itp=%d ips=%d nfpar=%d ilp=%d isw=%d isp=%d A2p=%d \n",RestartLabel,Auto.itp, Auto.ips,Auto.nfpar,Auto.ilp,Auto.isw,Auto.isp,AutoTwoParam);
      Auto.irs=RestartLabel;
      RestartLabel=0;
      do_auto(iold,isave, Auto.itp);
      
    }
    auto_depth--;
     ping();
      redraw_params();
}


void set_auto() /* Caution - need to include NICP here */
{
  NAutoUzr=Auto.nper;
  init_auto(NODE,Auto.nfpar,Auto.nbc,Auto.ips,Auto.irs,Auto.ilp,Auto.ntst,Auto.isp,
	    Auto.isw,Auto.nmx,Auto.npr,Auto.ds,Auto.dsmin,
	    Auto.dsmax,Auto.rl0,Auto.rl1,Auto.a0,Auto.a1,Auto.icp1,
	    Auto.icp2,Auto.icp3,Auto.icp4,Auto.icp5,Auto.nper,Auto.epsl,Auto.epsu,Auto.epss,Auto.ncol);
  
}
int auto_name_to_index(char *s)
{
  int i,in;
  find_variable(s,&in);
  if(in==0)return(10);
  in=find_user_name(PARAM_BOX,s);
  for(i=0;i<NAutoPar;i++)
    if(AutoPar[i]==in)return(i);
  return(-1);
}
int auto_par_to_name(int index, char *s)
{
  /* s is a pointer here; its callers pass char name[AUTO_COL_W+
     XPP_NAME_MAX+2] (80) and char bob[100] -- 80 is the smaller. */
  if(index==10){
    xpp_snprintf(s,80,"T");
    return(1);
  }
  if(index<0||index>8)return(0);
  xpp_snprintf(s,80,"%s",upar_names[AutoPar[index]]);
  return(1);
}

/* AUTO heads its printed columns PAR(n) and U(n); XPP knows what the user
   called them, and already uses those names for the diagram's axes. This
   rewrites one 14-character column heading for the screen only: fort.7 and
   fort.9 keep AUTO's own format, which its restart path and other people's
   scripts read. PAR(10) and friends are the period and such, not the user's
   parameters, and auto_par_to_name leaves them alone. A name that does not
   fit (names go to XPP_NAME_MAX) is shortened with a '~' (short_name) and
   still leaves a blank between it and the next heading: the column stays
   14 wide so the numbers below stay under it. */
static void auto_col_centre(char *out,char *s)
{
  /* out is a pointer here; its callers pass char scr[AUTO_COL_W+1]
     (autlib1.c) or auto_screen_col's own out parameter, itself
     AUTO_COL_W+1 by the same reasoning. */
  int n,l;
  char t[AUTO_COL_W];
  short_name(t,s,AUTO_COL_W-1);
  n=(int)strlen(t);
  l=(AUTO_COL_W-n)/2;
  xpp_snprintf(out,AUTO_COL_W+1,"%*s%s%*s",l,"",t,AUTO_COL_W-n-l,"");
}

void auto_screen_col(char *col,char *out)
{
  long p;
  int i;
  char name[AUTO_COL_W+XPP_NAME_MAX+2],pre[AUTO_COL_W+1],*q;
  if(sscanf(col," PAR(%ld)",&p)==1&&auto_par_to_name((int)p,name)){
    auto_col_centre(out,name);
    return;
  }
  /* U(n), and the MAX(n) / MIN(n) a periodic branch prints, where AUTO has
     overwritten the U itself */
  q=strchr(col,'(');
  if(q!=NULL&&strstr(col,"PAR")==NULL&&sscanf(q,"(%ld)",&p)==1&&p>=1&&p<=NODE){
    int n=(int)(q-col);
    if(n>0&&col[n-1]=='U')n--; /* the name replaces the U */
    /* keep what stands in front of it: MAX, MIN, L2-NORM, INTEGRAL */
    XPP_SPRINTF(pre,"%.*s",n,col);
    for(i=(int)strlen(pre);i>0&&pre[i-1]==' ';i--)
      pre[i-1]=0;
    for(i=0;pre[i]==' ';i++)
      ;
    XPP_SPRINTF(name,"%s%s%s",pre+i,pre[i]?" ":"",uvar_names[p-1]);
    auto_col_centre(out,name);
    return;
  }
  xpp_snprintf(out,AUTO_COL_W+1,"%.*s",AUTO_COL_W,col);
}


void auto_per_par()
{
  
  static const char *m[]={"0","1","2","3","4","5","6","7","8","9"};
  static char key[]="0123456789";
  char values[10][MAX_LEN_SBOX];
  char bob[100],*ptr;
  static const char *n[]={"Uzr1","Uzr2","Uzr3","Uzr4","Uzr5",
		      "Uzr6","Uzr7","Uzr8","Uzr9"};
  int status,i,in;
  char ch;
  /* "Mark values" (T21): AUTO labels (UZ) the points where a parameter or
     the period reaches one of these values */
  ch=(char)auto_pop_up_list(str("Mark values: how many?"),strs(m),key,10,12,Auto.nper,10,10,no_hint,
		       Auto.hinttxt);
  for(i=0;i<10;i++)
    if(ch==key[i])Auto.nper=i;
  NAutoUzr=Auto.nper;
  if(Auto.nper>0){
    for(i=0;i<9;i++){
      auto_par_to_name(Auto.uzrpar[i],bob);


      XPP_SPRINTF(values[i],"%s=%g",bob,Auto.period[i]);
    }
    status=do_string_box(9,5,2,str("Mark values (UZ): parameter=value or per=value"),strs(n),values,45);
    if(status!=0)
      for(i=0;i<9;i++){
	ptr=get_first(values[i],"=");
	in=auto_name_to_index(ptr);
	if(in>=0){
	  Auto.uzrpar[i]=in;
	  ptr=get_next("@");
	  Auto.period[i]=atof(ptr);
	}
      }
  }
  for(i=0;i<9;i++){
    outperiod[i]=Auto.period[i];
    UzrPar[i]=Auto.uzrpar[i];
  }
  
}

/* auto parameters are 1-8 (0-7) and since there are only 8, need to associate them
   with real xpp parameters for which there may be many 
*/
void auto_params()
{
  static const char *n[]={"*2Par1","*2Par2","*2Par3","*2Par4","*2Par5","*2Par6","*2Par7","*2Par8"};
  int status,i,in;
  char values[8][MAX_LEN_SBOX];
  for(i=0;i<8;i++){
    if(i<NAutoPar)  XPP_SPRINTF(values[i],"%s",upar_names[AutoPar[i]]);
    else values[i][0]='\0';/*sprintf(values[i],"");*/
  }
  status=do_string_box(8,8,1,str("Parameters"),strs(n),values,38);
  if(status!=0){
    for(i=0;i<8;i++){
      if(i<NAutoPar){
	in=find_user_name(PARAM_BOX,values[i]);
	if(in>=0){
	  AutoPar[i]=in;
	  in=get_param_index(values[i]);
	  Auto_index_to_array[i]=in;
	  /* printf("%d -> %d %s\n",i,in, values[i]); */
	}
      }
    }
  }
}

void auto_num_par()
{
  /* grouped by what they do, which upstream's order was not: the box is 7
     rows by 4 columns, so a column is a group. Mesh and step size, then the
     ranges and tolerances, then the solver's integer knobs. */
  static const char *n[]={"Ntst","Nmax","NPr","Ncol","Ds","Dsmin","Dsmax",
		    "Par Min","Par Max","Norm Min","Norm Max","EPSL","EPSU","EPSS",
                    "IAD","MXBF","IID","ITMX","ITNW","NWTN","IADS","SuppBP"};
  int status;
  char values[22][MAX_LEN_SBOX];
  XPP_SPRINTF(values[0],"%d",Auto.ntst);
  XPP_SPRINTF(values[1],"%d",Auto.nmx);
  XPP_SPRINTF(values[2],"%d",Auto.npr);
  XPP_SPRINTF(values[3],"%d",Auto.ncol);
  XPP_SPRINTF(values[4],"%g",Auto.ds);
  XPP_SPRINTF(values[5],"%g",Auto.dsmin);
  XPP_SPRINTF(values[6],"%g",Auto.dsmax);
  XPP_SPRINTF(values[7],"%g",Auto.rl0);
  XPP_SPRINTF(values[8],"%g",Auto.rl1);
  XPP_SPRINTF(values[9],"%g",Auto.a0);
  XPP_SPRINTF(values[10],"%g",Auto.a1);
  XPP_SPRINTF(values[11],"%g",Auto.epsl);
  XPP_SPRINTF(values[12],"%g",Auto.epsu);
  XPP_SPRINTF(values[13],"%g",Auto.epss);
  XPP_SPRINTF(values[14],"%d",aauto.iad);
  XPP_SPRINTF(values[15],"%d",aauto.mxbf);
  XPP_SPRINTF(values[16],"%d",aauto.iid);
  XPP_SPRINTF(values[17],"%d",aauto.itmx);
  XPP_SPRINTF(values[18],"%d",aauto.itnw);
  XPP_SPRINTF(values[19],"%d",aauto.nwtn);
  XPP_SPRINTF(values[20],"%d",aauto.iads);
  XPP_SPRINTF(values[21],"%d",SuppressBP); 

  
  static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
  status=do_string_box_of(22,7,4,str("AutoNum"),strs(n),values,25,kinds);
  if(status!=0){
    Auto.ntst=atoi(values[0]);
    Auto.nmx=atoi(values[1]);
    Auto.npr=atoi(values[2]);
    Auto.ncol=atoi(values[3]);
    Auto.ds=atof(values[4]);
    Auto.dsmin=atof(values[5]);
    Auto.dsmax=atof(values[6]);
    Auto.rl0=atof(values[7]);
    Auto.rl1=atof(values[8]);
    Auto.a0=atof(values[9]);
    Auto.a1=atof(values[10]);
    Auto.epsl=atof(values[11]);
    Auto.epsu=atof(values[12]);
    Auto.epss=atof(values[13]);
    aauto.iad=atoi(values[14]);
    aauto.mxbf=atoi(values[15]);
    aauto.iid=atoi(values[16]);
    aauto.itmx=atoi(values[17]);
    aauto.itnw=atoi(values[18]);
    aauto.nwtn=atoi(values[19]);
    aauto.iads=atoi(values[20]);
    SuppressBP=atoi(values[21]);

    
  }

}    


void auto_plot_par()
{


  static const char *m[]={"Hi","Norm","hI-lo","Period","Two par","(Z)oom in","Zoom (O)ut",
		      "last 1 par", "last 2 par","Fit",
		    "fRequency","Average","Default","Scroll"};
  static char key[]="hniptzo12frads";
  char ch;


  static const char *n[]={"*1Y-axis","*2Main Parm", "*2Secnd Parm", "Xmin", "Ymin",
		   "Xmax", "Ymax"};
  char values[7][MAX_LEN_SBOX];
  int  status,i;
  int ii1,ii2,ji1,ji2;
  int i1=Auto.var+1;
  char n1[XPP_NAME_MAX+1];
  ch=(char)auto_pop_up_list(str("Plot Type"),strs(m),key,14,10,Auto.plot,10,50,
		       aaxes_hint,Auto.hinttxt);
  if(ch==ESC) 
    return;
  for(i=0;i<5;i++){
    if(ch==key[i])Auto.plot=i;
  }
  if(ch==key[10])Auto.plot=10;
  if(ch==key[11])Auto.plot=11;
  if(ch==key[5]){
    if(auto_rubber(&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_in(ii1,ji1,ii2,ji2);
      redraw_diagram();
    }
    return;
  }
  
  if(ch==key[6]){
    if(auto_rubber(&ii1,&ji1,&ii2,&ji2,RUBBOX)!=0){
      auto_zoom_out(ii1,ji1,ii2,ji2);
     
      redraw_diagram();
    }
    return;
  }

  /* a new plot type or axes: the diagram drawn again in its quantities
     (it used to wait for reDraw, which a client drawing from data lacks) */
  if(ch==key[7]){
    load_last_plot(1);
    redraw_diagram();
    return;
  }

  if(ch==key[8]){
    load_last_plot(2);
    redraw_diagram();
    return;
  }

  if(ch==key[9]){
    auto_fit();
    redraw_diagram();
    return;
  }
  if(ch==key[12]){
    auto_default();
    redraw_diagram();
    return;
  }
  if(ch==key[13]){
    auto_scroll_window();
    redraw_diagram();
    /* printf("I am done scrolling!!"); */
    return;
  }
  ind_to_sym(i1,n1);
  XPP_SPRINTF(values[0],"%s",n1);
  XPP_SPRINTF(values[1],"%s",upar_names[AutoPar[Auto.icp1]]);
  XPP_SPRINTF(values[2],"%s",upar_names[AutoPar[Auto.icp2]]);
  XPP_SPRINTF(values[3],"%g",Auto.xmin);
  XPP_SPRINTF(values[4],"%g",Auto.ymin);
  XPP_SPRINTF(values[5],"%g",Auto.xmax);
  XPP_SPRINTF(values[6],"%g",Auto.ymax);
  static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
  status=do_string_box_of(7,7,1,str("AutoPlot"),strs(n),values,31,kinds);
  if(status!=0){
    /*  get variable names  */
    find_variable(values[0],&i);
    if(i>0)
      Auto.var=i-1;
    /*  Now check the parameters  */
    i1=find_user_name(PARAM_BOX,values[1]);
    if(i1>=0){
      for(i=0;i<NAutoPar;i++){
	if(i1==AutoPar[i]){
	  Auto.icp1=i;

	}
      }
    }
     i1=find_user_name(PARAM_BOX,values[2]);
    if(i1>=0){
      for(i=0;i<NAutoPar;i++){
	if(i1==AutoPar[i]){
	  Auto.icp2=i;
	}
      }
    }

    Auto.xmin=atof(values[3]);
    Auto.ymin=atof(values[4]);
    Auto.xmax=atof(values[5]);
    Auto.ymax=atof(values[6]);
    if(Auto.plot<4)keep_last_plot(1);
    if(Auto.plot==4)keep_last_plot(2);
    redraw_diagram();
    

    
}
}



void auto_default()
{
  Auto.xmin=auto_xmin;
  Auto.xmax=auto_xmax;
  Auto.ymin=auto_ymin;
  Auto.ymax=auto_ymax;
}



void auto_fit()
{
  double xlo=Auto.xmin,xhi=Auto.xmax,ylo=Auto.ymin,yhi=Auto.ymax;
  bound_diagram(&xlo,&xhi,&ylo,&yhi);
  Auto.xmin=xlo;
  Auto.xmax=xhi;
  Auto.ymin=ylo;
  Auto.ymax=yhi;
}
  
void auto_zoom_in(int i1, int j1, int i2, int j2)
{
   double x1,y1,x2,y2;
   int temp;
   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   double dx = (Auto.xmax-Auto.xmin);
   double dy = (Auto.ymax-Auto.ymin);
   x1 = Auto.xmin+(double)(i1-Auto.x0)*(dx)/(double)Auto.wid;
   x2 = Auto.xmin+(double)(i2-Auto.x0)*(dx)/(double)Auto.wid;
   y1 = Auto.ymin+(double)(Auto.hgt+Auto.y0-j1)*(dy)/(double)Auto.hgt;
   y2 = Auto.ymin+(double)(Auto.hgt+Auto.y0-j2)*(dy)/(double)Auto.hgt;
 
   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx/2;
	  dy = dy/2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  Auto.xmin=x1-dx/2;
	  Auto.xmax=x1+dx/2;
	  Auto.ymin=y1-dy/2;
	  Auto.ymax=y1+dy/2;
  }
  else
  {           
	  Auto.xmin=x1;
	  Auto.ymin=y1;
	  Auto.xmax=x2;
	  Auto.ymax=y2;     
  }
  	
}

void auto_zoom_out(int i1, int j1, int i2, int j2)
{
   double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
   int temp;
   double dx = (Auto.xmax-Auto.xmin);
   double dy = (Auto.ymax-Auto.ymin);
   double a1,a2,b1,b2;

   if(i1>i2){temp=i1;i1=i2;i2=temp;}
   if(j2>j1){temp=j1;j1=j2;j2=temp;}
   a1=(double)(i1-Auto.x0)/(double)Auto.wid;
      a2=(double)(i2-Auto.x0)/(double)Auto.wid;
      b1=(double)(Auto.hgt+Auto.y0-j1)/(double)Auto.hgt;
      b2=(double)(Auto.hgt+Auto.y0-j2)/(double)Auto.hgt;

   
   if((i1==i2)||(j1==j2))
   { 
   	  if (dx < 0){dx=-dx;}
	  if (dy < 0){dy=-dy;}
	  dx = dx*2;
	  dy = dy*2;
	  /*Shrink by thirds and center (track) about the point clicked*/
	  Auto.xmin=x1-dx/2;
	  Auto.xmax=x1+dx/2;
	  Auto.ymin=y1-dy/2;
	  Auto.ymax=y1+dy/2;
  }
  else
  {           
    x1=(a1*Auto.xmax-a2*Auto.xmin)/(a1-a2);
    x2=(Auto.xmin-Auto.xmax+a1*Auto.xmax-a2*Auto.xmin)/(a1-a2);
    y1=(b1*Auto.ymax-b2*Auto.ymin)/(b1-b2);
    y2=(Auto.ymin-Auto.ymax+b1*Auto.ymax-b2*Auto.ymin)/(b1-b2);
	  Auto.xmin=x1;
	  Auto.ymin=y1;
	  Auto.xmax=x2;
	  Auto.ymax=y2;
  }

} 
 


  

void auto_xy_plot(double *x, double *y1, double *y2, double par1, double par2, double per, double *uhigh, double *ulow, double *ubar, double a)
{
 /* a plot type none of the cases know leaves the point at (par1, 0) */
 *x=par1;
 *y1=*y2=0.0;
 switch(Auto.plot){
  case HI_P:
    *x=par1;
    *y1=uhigh[Auto.var];
    *y2=*y1;
    break;
  case NR_P:
    *x=par1;
    *y1=a;
    *y2=*y1;
    break;
  case HL_P:
    *x=par1;
    *y1=uhigh[Auto.var];
    *y2=ulow[Auto.var];
    break;
  case AV_P:
    *x=par1;
    *y1=ubar[Auto.var];
    *y2=*y1;
    break;
  case PE_P:
    *x=par1;
    *y1=per;
    *y2=*y1;
    break;
  case FR_P:
    *x=par1;
    if(per>0)*y1=1./per;
    else *y1=0.0;
    *y2=*y1;
    break;
  case P_P:
    *x=par1;
    *y1=par2;
    *y2=*y1;
    break;
  }
}

int plot_point(int flag2, int icp1, int icp2)
{
  int j=1;
  if(icp1!=Auto.icp1)j=0;
  if(flag2>0&&icp2!=Auto.icp2)j=0;
  return(j);
}




void add_ps_point(double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
		  int type, int flg, int lab, int npar, int icp1, int icp2, int flag2,
		  double *evr, double *evi)
{
  double x,y1,y2,par1,par2=0;
  int type1=type;
  par1=par[icp1];
  if(icp2<NAutoPar)par2=par[icp2];
  auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    Auto.lastx=x;
    Auto.lasty=y1;
  }
  if(flag2==0&&Auto.plot==P_P)
    {
  
       return;
     }
  if(flag2>0&&Auto.plot!=P_P){
  
    return;
  }

  if((flag2>0)&&(Auto.plot==P_P))
   type1=CSEQ;
  switch(type1){
 
  case CSEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;

    if(plot_export.color){
      set_linestyle(1);
      if(flag2>0)pscolset2(flag2);
    }
    else 
      set_linestyle(8);
    line_abs((float)x,(float)y1,(float)Auto.lastx,(float)Auto.lasty);
    break;
  case CUEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    if(Auto.plot!=P_P)
      {if(plot_export.color) set_linestyle(0);else set_linestyle(4);}
    else
      {
	pscolset2(flag2);
      
      }
    line_abs((float)x,(float)y1,(float)Auto.lastx,(float)Auto.lasty);
    break;
  case UPER:
    if(plot_export.color) 
      set_linestyle(9); 
    else 
      set_linestyle(0);
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    PointType=UPT;
   /*  plintf("UP: %g %g %g\n",x,y1,y2); */
    point_abs((float)x,(float)y1);
    point_abs((float)x,(float)y2);
    break;
  case SPER:
    if(plot_export.color)
      set_linestyle(7);
    else
      set_linestyle(0);
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
   /*  plintf("SP: %g %g %g\n",x,y1,y2); */
    PointType=SPT;
    point_abs((float)x,(float)y1);
    point_abs((float)x,(float)y2); 
    break;
  }

  Auto.lastx=x;
  Auto.lasty=y1;
}




void auto_line(double x1i, double y1i, double x2i, double y2i)
{
  double xmin,ymin,xmax,ymax;
  float x1=x1i,x2=x2i,y1=y1i,y2=y2i;
  double x1d,x2d,y1d,y2d;
  float x1_out,y1_out,x2_out,y2_out;

  
  get_scale(&xmin,&ymin,&xmax,&ymax);
  set_scale(Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax);
  if(clip(x1,x2,y1,y2,&x1_out,&y1_out,&x2_out,&y2_out)){
    x1d=x1_out;
    x2d=x2_out;
    y1d=y1_out;
    y2d=y2_out;
    DLINE(x1d,y1d,x2d,y2d);
  }
 
  set_scale(xmin,ymin,xmax,ymax);
}
/* The point add_point() is given next, for the diagram's data
   (auto_diagram): its caller knows the branch and point, add_point does not. */
static int dpt_ibr,dpt_ntot,dpt_itp,dpt_node,dpt_from;
void auto_point_id(int ibr,int ntot,int itp,int node,int from)
{
  dpt_ibr=ibr;
  dpt_ntot=ntot;
  dpt_itp=itp;
  dpt_node=node;
  dpt_from=from;
}

int auto_run_from_take(void)
{
  int f=run_from;
  run_from=0;
  return f;
}

/* the stability circle as data (auto_data.h): a stored point's values
   (auto_stability.h), whether AUTO is computing it or a grab is on it */
static void show_stab(const double *evr,const double *evi,int n,int periodic)
{
  auto_data_stab(evr,evi,n,periodic);
}

/* the colour colset() and colset2() give a point */
static int auto_point_color(int type,int flag2)
{
  switch(flag2){
  case 0: break;
  case LPE2: return LPE_color;
  case LPP2: return LPP_color;
  case HB2: return HB_color;
  case TR2: return TR_color;
  case BR2: return BR_color;
  case PD2: return PD_color;
  case FP2: return FP_color;
  default: return 0;
  }
  switch(type){
  case CSEQ: return SEc;
  case CUEQ: return UEc;
  case SPER: return SPc;
  case UPER: return UPc;
  }
  return 0;
}

/* this bit of code is for writing points - it only saves what is
   in the current view

*/
int check_plot_type(int flag2,int icp1, int icp2)
{
  if(flag2==0 && Auto.plot==P_P)
    return 0;
  if(flag2>0  && Auto.plot!=P_P)
    return 0; 
  if(icp1!=Auto.icp1)
    return 0;
  if(flag2>0 && icp2!=Auto.icp2)
    return 0;
  return 1;

} 
/* main plotting code  */ 
void add_point(double *par, double per, double *uhigh, double *ulow, double *ubar, double a,
	       int type, int flg, int lab, int npar, int icp1, int icp2, int icp3, int icp4, int flag2,
	       double *evr, double *evi)
{
  double x,y1,y2,par1,par2=0;
  int ix,iy1,iy2,type1=type;
  char bob[5];
  XppDiagPoint dp;
  XPP_SPRINTF(bob,"%d",lab);
  par1=par[icp1];
  if(icp2<NAutoPar)par2=par[icp2];
auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a); /* figure out who sits on axes */
  memset(&dp,0,sizeof dp);
  dp.ibr=dpt_ibr;
  dp.pt=dpt_ntot;
  dp.itp=dpt_itp;
  dp.node=dpt_node;
  dp.from=dpt_from;
  dp.type=type;
  dp.flag2=flag2;
  dp.newseg=(flg==0);
  dp.color=auto_point_color(type,flag2);
  dp.lw=(type==CSEQ||flag2>0)?2:1;
  dp.x=x;
  dp.y1=y1;
  dp.y2=y2;
  if(flg==0){
    Auto.lastx=x;
    Auto.lasty=y1;
  }
  ix=IXVal(x);
  iy1=IYVal(y1);
  iy2=IYVal(y2);
  autobw();
if(flag2==0&&Auto.plot==P_P) /* if the point was a 1 param run and we are in 2 param plot, skip */
    {
       if(flg==0)auto_diagram(&dp); /* not drawn, but the next line starts here */
       show_stab(evr,evi,NODE,type==SPER||type==UPER);
       refreshdisplay();
       return;
     }
if(flag2>0&&Auto.plot!=P_P){ /* two parameter and not in two parameter plot, just skip it */
    if(flg==0)auto_diagram(&dp);
    show_stab(evr,evi,NODE,type==SPER||type==UPER);
    refreshdisplay();
    return;
  }

 if((flag2>0)&&(Auto.plot==P_P))
   type1=CSEQ;
 switch(type1){
  
  case CSEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=1;
    LineWidth(2);
    colset(type);
    if(flag2>0)colset2(flag2);
    auto_line(x,y1,Auto.lastx,Auto.lasty);
    autobw();
    break;
  case CUEQ:
    if(Auto.plot==PE_P||Auto.plot==FR_P)break;
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=1;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    auto_line(x,y1,Auto.lastx,Auto.lasty);
    autobw();
    break;
  case UPER:
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=3;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    if(chk_auto_bnds(ix,iy1))Circle(ix,iy1,3);
    if(chk_auto_bnds(ix,iy2))Circle(ix,iy2,3);
    autobw();
    break;
  case SPER:
    if(icp1!=Auto.icp1)break;
    if(flag2>0&&Auto.icp2!=icp2)break;
    dp.draw=2;
    LineWidth(1);
        colset(type);
	if(flag2>0)colset2(flag2);
    if(chk_auto_bnds(ix,iy1))FillCircle(ix,iy1,3);
    if(chk_auto_bnds(ix,iy2))FillCircle(ix,iy2,3);
    autobw();
    break;
  }
  if(lab!=0){
    if(icp1==Auto.icp1){
      if(flag2==0||(flag2>0&&Auto.icp2==icp2)){
	dp.lab=lab;
	LineWidth(1);
        if(chk_auto_bnds(ix,iy1)){
	ALINE(ix-4,iy1,ix+4,iy1);
	ALINE(ix,iy1-4,ix,iy1+4); }
	if(chk_auto_bnds(ix,iy2)){
	ALINE(ix-4,iy2,ix+4,iy2);
	ALINE(ix,iy2-4,ix,iy2+4);}
	if(chk_auto_bnds(ix,iy1))ATEXT(ix+8,iy1+8,bob); 
      }
    }
  }

  Auto.lastx=x;
  Auto.lasty=y1;
  auto_diagram(&dp);
  show_stab(evr,evi,NODE,type==SPER||type==UPER);
  refreshdisplay();
}
  


void get_bif_sym(char *at, int itp)
{
  /* at is a pointer here; every caller passes a char[3] (symb/nsymb/sym),
     matching the longest label written below ("BP" etc, 2 chars + NUL). */
  int i=itp%10;
  switch(i){
  case 1:
  case 6:
    xpp_snprintf(at,3,"BP");
    break;
  case 2:
  case 5:
    xpp_snprintf(at,3,"LP");
    break;
  case 3:
    xpp_snprintf(at,3,"HB");
    break;
  case -4:
    xpp_snprintf(at,3,"UZ");
    break;
  case 7:
    xpp_snprintf(at,3,"PD");
    break;
  case 8:
    xpp_snprintf(at,3,"TR");
    break;
  case 9:
    xpp_snprintf(at,3,"EP");
    break;
  case -9:
    xpp_snprintf(at,3,"MX");
    break;
  default:
    xpp_snprintf(at,3,"  ");
    break;
  }
}
    
void info_header(int flag2, int icp1, int icp2)
{
  char bob[80];
  /* the names head 10-wide columns of new_info's numbers */
  char p1name[11],p2name[11],vname[11];

  short_name(p1name,upar_names[AutoPar[icp1]],10);
  if(icp2<NAutoPar)short_name(p2name,upar_names[AutoPar[icp2]],10);
  else XPP_SPRINTF(p2name,"   ");
  short_name(vname,uvar_names[Auto.var],10);
  SmallBase();
  XPP_SPRINTF(bob,"  Br  Pt Ty  Lab %10s %10s       norm %10s     period",
	  p1name,
	  p2name,
	  vname);
  draw_auto_info(bob,10,text_metrics.small_height+1);
  
}
	  
void new_info(int ibr, int pt, char *ty, int lab, double *par, double norm, double u0, double per, int flag2, int icp1, int icp2)
{
  char bob[80];
  double p1,p2=0.0;
  clear_auto_info();
  info_header(flag2,icp1,icp2);
  p1=par[icp1];
  if(icp2<NAutoPar)p2=par[icp2];
  XPP_SPRINTF(bob,"%4d %4d %2s %4d %10.4g %10.4g %10.4g %10.4g %10.4g",
	  ibr,pt,ty,lab,p1,p2,norm,u0,per);
  draw_auto_info(bob,10,2*text_metrics.small_height+2);
  /* SmallGr(); */
  refreshdisplay();
}


void traverse_out(DIAGRAM *d, int *ix, int *iy, int dodraw)
{
  double norm,per,*par,par1,par2=0,*evr,*evi;
  int pt,itp,ibr,lab,icp1,icp2,flag2;
  double x,y1,y2;
  char symb[3];
  if (d==NULL)
  {
  	/*err_msg(str("Can not traverse to NULL diagram."));*/
	return;
  }
  norm=d->norm;
  par=d->par;

  per=d->per;
  lab=d->lab;
  itp=d->itp;
  ibr=d->ibr;
  icp1=d->icp1;
  icp2=d->icp2;
  flag2=d->flag2;
  pt=d->ntot;
  
  evr=d->evr;
  evi=d->evi;
 
  get_bif_sym(symb,itp);
 par1=par[icp1];
  if(icp2<NAutoPar)par2=par[icp2];  
    auto_xy_plot(&x,&y1,&y2,par1,par2,per,d->uhi,d->ulo,d->ubar,norm);
  
    *ix=IXVal(x);
    *iy=IYVal(y1);
    if (dodraw==1)
    {
      AutoDataInfo ai;
    	XORCross(*ix,*iy);
  	show_stab(evr,evi,NODE,ibr<0);
  	new_info(ibr,pt,symb,lab,par,norm,d->u0[Auto.var],per,flag2,icp1,icp2);
      /* what the strip shows, as data */
      ai.ibr=ibr;
      ai.pt=pt;
      ai.itp=itp;
      ai.lab=lab;
      ai.type=get_bif_type(ibr,pt,lab);
      ai.flag2=flag2;
      ai.node=d->index;
      ai.sym=symb;
      ai.p1name=upar_names[AutoPar[icp1]];
      ai.p1=par1;
      ai.p2name=icp2<NAutoPar?upar_names[AutoPar[icp2]]:NULL;
      ai.p2=par2;
      ai.norm=norm;
      ai.vname=uvar_names[Auto.var];
      ai.u=d->u0[Auto.var];
      ai.per=per;
      ai.x=x;
      ai.y=y1;
      ai.y2=y2;
      auto_data_info(&ai);
    }
    if(lab>0 && load_all_labeled_orbits>0)
      load_auto_orbitx(ibr,1,lab,per);

}
     
   



void do_auto_win()
{
  char bob[256];
  if(Auto.exist==0){
    if(NODE>NAUTO){
   XPP_SPRINTF(bob,"Auto restricted to less than %d variables",NAUTO);
      err_msg(bob);
      return;
    }
    make_auto(str("It's AUTO man!"),str("AUTO"));
    Auto.exist=1;
    
  }

}

void load_last_plot(int flg)
{
 if(flg==1) {/* one parameter */
  Auto.xmin=Old1p.xmin;
  Auto.xmax=Old1p.xmax;
  Auto.ymin=Old1p.ymin;
  Auto.ymax=Old1p.ymax;
  Auto.icp1=Old1p.icp1;
  Auto.icp2=Old1p.icp2;
  Auto.plot=Old1p.plot;
 Auto.var=Old1p.var;
}
if(flg==2) {/* two parameter */
  Auto.xmin=Old2p.xmin;
  Auto.xmax=Old2p.xmax;
  Auto.ymin=Old2p.ymin;
  Auto.ymax=Old2p.ymax;
  Auto.icp1=Old2p.icp1;
  Auto.icp2=Old2p.icp2;
  Auto.plot=Old2p.plot;
 Auto.var=Old2p.var;
}

}
void keep_last_plot(int flg)
{
  if(flg==1){ /* one parameter */
    Old1p.xmin=Auto.xmin;
    Old1p.xmax=Auto.xmax;
    Old1p.ymin=Auto.ymin;
    Old1p.ymax=Auto.ymax;
    Old1p.icp1=Auto.icp1;
    Old1p.icp2=Auto.icp2;
    Old1p.plot=Auto.plot;
    Old1p.var=Auto.var;
  }
  if(flg==2){
    Old2p.xmin=Auto.xmin;
    Old2p.xmax=Auto.xmax;
    Old2p.ymin=Auto.ymin;
    Old2p.ymax=Auto.ymax;
    Old2p.icp1=Auto.icp1;
    Old2p.icp2=Auto.icp2;
    Old2p.plot=P_P;
    Old2p.var=Auto.var;
  }
}

void init_auto_win()
{
  int i;
  if(NODE>NAUTO)return;
  start_diagram(NODE); 
  for(i=0;i<10;i++){
    Auto.period[i]=11.+3.*i;
    Auto.uzrpar[i]=10;
    outperiod[i]=Auto.period[i];
    UzrPar[i]=10;
  }
  NAutoPar=8;
  if(NUPAR<8)NAutoPar=NUPAR;
  for(i=0;i<NAutoPar;i++)AutoPar[i]=i;
  for(i=0;i<NAutoPar;i++){
    Auto_index_to_array[i]=get_param_index(upar_names[AutoPar[i]]);
    /* printf("%d -> %d, %s \n",i,Auto_index_to_array[i],upar_names[AutoPar[i]]); */
  }
  Auto.nper=0;
  grabpt.flag=0;  /*  no point in buffer  */
  Auto.exist=0;
  blrtn.irot=0;
  for(i=0;i<NODE;i++)
    blrtn.nrot[i]=0;
 blrtn.torper=TOR_PERIOD;
 create_auto_file_name();
 
/*  Control -- done automatically   */
  Auto.irs=0;
  Auto.ips=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.nbc=NODE;
  Auto.nfpar=1;
  HomoFlag=0;
/*  User controls this      */
  Auto.ncol=auto_ncol;
  Auto.ntst=auto_ntst;
  Auto.nmx=auto_nmx;
  Auto.npr=auto_npr;
  Auto.ds=auto_ds;
  Auto.dsmax=auto_dsmax;
  Auto.dsmin=auto_dsmin;
  Auto.rl0=auto_rl0;
  Auto.rl1=auto_rl1;
  Auto.a0=auto_a0;
  Auto.a1=auto_a1;
  
  Auto.epsl=auto_epsl;
    Auto.epsu=auto_epsu;
  Auto.epss=auto_epss;


/* The diagram plotting stuff    */

  Auto.xmax=auto_xmax;
  Auto.xmin=auto_xmin;
  Auto.ymax=auto_ymax;
  Auto.ymin=auto_ymin;
  Auto.plot=HL_P;
  Auto.var=auto_var;

 
/* xpp parameters    */
  
  Auto.icp1=0;
  Auto.icp2=1;
   Auto.icp3=1;
  Auto.icp4=1;
  Auto.icp5=1;
  keep_last_plot(1);
  keep_last_plot(2);
  aauto.iad=3;
  aauto.mxbf=5;
  aauto.iid=2;
  aauto.itmx=8;
  aauto.itnw=7;
  aauto.nwtn=3;
  aauto.iads=1;
  xAuto.nunstab=1;
  xAuto.nstab=NODE-1;
}

int yes_reset_auto()
{
  char string[256];
  if(NBifs<=1)return(0);
 kill_diagrams();
 FromAutoFlag=0;
    NBifs=1;
    grabpt.flag=0;
    XPP_SPRINTF(string,"%s.b",this_auto_file);
    deletef(string);
    XPP_SPRINTF(string,"%s.d",this_auto_file);
    deletef(string);
    XPP_SPRINTF(string,"%s.s",this_auto_file);
    deletef(string);
    diagram_mark.state=0;
    return 1;
}
int reset_auto()
{
  char ch;
    if(NBifs<=1)return(0);
    ch=(char)TwoChoice(str("YES"),str("NO"),str("Destroy AUTO diagram & files"),str("yn"));
    if(ch!='y')return(0);
   
  return(yes_reset_auto());
}

void auto_grab()
{
  traverse_diagram();
 /* redraw_auto_menus();
   */ 
} 

void auto_next()
{

  static const char *m[]={"EP","HB","LP","PD","MX"};
  /*static const char *m[]={"Fixed period","Extend"}; */
  static  char key[]="ehlpm";
  char ch;
  ch=(char)auto_pop_up_list(str("Special Point"),strs(m),key,5,13,0,10,10,
		       no_hint,Auto.hinttxt);
   if(ch=='e'){
    /*auto_new_per();*/
    xpp_log_auto("End point\n");
    return;
  }
  if(ch=='h'){
     xpp_log_auto("Hopf point\n");
     /* auto_2p_fixper();*/
     
    return;
  }
  if(ch=='l'){ 
     xpp_log_auto("Limit point\n");
     /* auto_2p_fixper();*/
     
    return;
  }
  if(ch=='p'){
     xpp_log_auto("Periodic point\n");
     /* auto_2p_fixper();*/
     
    return;
  }
  if(ch=='m'){ 
     xpp_log_auto("Max point\n");
     /* auto_2p_fixper();*/
     
    return;
  }
  /*traverse_diagram();
  */
 /* redraw_auto_menus();
   */ 
} 

void get_start_period(double *p)
{
 *p=storage[0][storind-1];
}
void find_best_homo_shift(int n)
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
  for(i=0;i<storind;i++){
    d=0.0;
    for(j=0;j<n;j++){
      d+=fabs(storage[j+1][i]-homo_l[j]);
    }
    if(d<dmin){
      dmin=d;
      tshift=storage[0][i];
    }
  }
  HOMO_SHIFT=tshift;
  xpp_log_auto("shifting %g\n",HOMO_SHIFT);
}
void get_shifted_orbit(double *u, double t, double p, int n)
{
  double ts;
  int i,i1,i2,ip,j;
  double lam;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  ts=fmod(t*p+HOMO_SHIFT,p);
  for(i=0;i<storind;i++){
    ip=(i+1)%storind;
    if((ts>=storage[0][i])&&(ts<storage[0][ip])){
      i1=i;
      i2=ip;
      lam=ts-storage[0][i];
      for(j=0;j<n;j++)
	u[j]=(1.0-lam)*storage[j+1][i1]+lam*storage[j+1][i2];
      break;
    }
  }
}
void get_start_orbit(double *u, double t, double p, int n)
{
  double tnorm,lam;
  int i1,i2,j;
  if(t>1.0)t-=1.0;
  if(t<0.0)t+=1.0;
  tnorm=t*(storind-1);
  i1=(int)tnorm;
  i2=i1+1;
  if(i2>=storind)i2-=storind;
  lam=(tnorm-(double)i1);

   for(j=0;j<n;j++)
    u[j]=(1.0-lam)*storage[j+1][i1]+lam*storage[j+1][i2];
}
  
void auto_start_choice()
{
  static const char *m[]={"Steady state","Periodic","Bdry Value","Homoclinic","hEteroclinic"};
  static  char key[]="spbhe";
  char ch;
  HomoFlag=0;
  if(METHOD==DISCRETE){
    auto_new_discrete();
    return;
  }
  ch=(char)auto_pop_up_list(str("Start"),strs(m),key,5,13,0,10,10,arun_hint,
		       Auto.hinttxt);
   if(ch=='s'){
    auto_new_ss();
    return;
  }
  if(ch=='p'){
  auto_start_at_per();
    return;
  }
 if(ch=='b'){
   Auto.nbc=NODE;
   auto_start_at_bvp();
   return;
 }
 if(ch=='h'){
   HomoFlag=1;
   auto_start_at_homoclinic();
     return;
   }
     
 /*  Auto.nbc=NODE-1;
   auto_start_at_bvp();
   } */
 if(ch=='e'){
   HomoFlag=2;
   auto_start_at_homoclinic();
   return;
 }

  redraw_auto_menus();
}


void torus_choice()
{
  static const char *m[]={"Two Param","Fixed period","Extend"};
  /*static const char *m[]={"Fixed period","Extend"}; */
  static  char key[]="tfe";
  char ch;
  ch=(char)auto_pop_up_list(str("Torus"),strs(m),key,3,10,0,10,10,
		       no_hint,Auto.hinttxt);
   if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
      auto_2p_fixper();
    return;
  }
  if(ch=='t'){
    auto_torus();
    return;
    } 
  redraw_auto_menus();
}
 
void per_doub_choice()
{
  static const char *m[]={"Doubling","Two Param","Fixed period","Extend"};
  static  char key[]="dtfe";
  char ch;
  ch=(char)auto_pop_up_list(str("Per. Doub."),strs(m),key,4,10,0,10,10,no_hint,Auto.hinttxt);
  if(ch=='d'){
    auto_period_double();
    return;
  }
   if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
      auto_2p_fixper();
    return;
  }
  if(ch=='t'){
    auto_twopar_double();
    return;
  }
  redraw_auto_menus();
}
  
void periodic_choice()
{
  static const char *m[]={"Extend","Fixed Period"};
  static  char key[]="ef";
  char ch;
  ch=(char)auto_pop_up_list(str("Periodic "),strs(m),key,2,14,0,10,10,
		       no_hint,Auto.hinttxt);
  if(ch=='e'){
    auto_new_per();
    return;
  }
  if(ch=='f'){
    auto_2p_fixper();
    return;
  }

  redraw_auto_menus();
}


void hopf_choice()
{
  static const char *m[]={"Periodic","Extend","New Point","Two Param"};
  static  char key[]="pent";
  char ch;
  if(METHOD==DISCRETE){
    auto_2p_hopf();
    return;
  }

  ch=(char)auto_pop_up_list(str("Hopf Pt"),strs(m),key,4,10,0,10,10,
		       no_hint,Auto.hinttxt);
  if(ch=='p'){
    auto_new_per();
    return;
  }
  if(ch=='e'){
    auto_extend_ss();
    return;
  }
  if(ch=='n'){
    auto_new_ss();
    return;
  }
  if(ch=='t'){
    auto_2p_hopf();
    return;
  }
  redraw_auto_menus();
}


void auto_run()
{
  int itp1,itp2,itp,ips;
  char ch;
  if(grabpt.flag==0){   /* the first call to AUTO   */
    auto_start_choice();
    ping();return;
  }
  if(grabpt.lab==0){
    ch=(char)TwoChoice(str("YES"),str("NO"),str("Not Labeled Pt: New Start?"),str("y"));
    if(ch=='y')auto_start_diff_ss();
    ping();return;
  }
    
  itp=grabpt.itp;
  itp1=itp%10;
  itp2=itp/10;
  ips=Auto.ips;
  /*  printf(" ips=%d itp=%d itp1= %d itp2=%d\n",ips,itp,itp1,itp2); */
  if(itp1==3||itp2==3){  /* its a HOPF Point  */
    hopf_choice();
    ping();return;
  }
  if(itp1==7||itp2==7){ /* period doubling */
    per_doub_choice();
    ping();return;
  }
  if(ips==9){
    auto_homo_choice(itp);
    ping(); return;
  }
  if(itp1==2||itp2==2){ /* limit point */
     Auto.ips=1;
     auto_2p_limit(Auto.ips);
    ping();return;
  }
  if(itp1==5||itp2==5){ /* limit pt of periodic or BVP */
    if(Auto.ips!=4)
      Auto.ips=2;  /* this is a bit dangerous - the idea is that
                      if you are doing BVPs, then that is all you are
                      doing  
		   */
    auto_2p_limit(Auto.ips);
    ping(); return;
  }
  if(itp1==6||itp2==6||itp1==1||itp2==1){ /* branch point  */ 

    
  auto_branch_choice(grabpt.ibr,ips);
    ping();
    return; /* 
    
    if(grabpt.ibr<0&&ips==2)
      auto_switch_per();
    else 
      if(ips==4)
	auto_switch_bvp();
      else
	auto_switch_ss();
    ping();
    return;   */
  }
  if(itp1==8||itp2==8){ /* Torus 2 parameter */
    torus_choice();
    ping();
    return;
  }
  if(grabpt.ibr<0) { /* its a periodic -- just extend it  */
    periodic_choice();
    ping();return;
  }
  if(grabpt.ibr>0&&ips!=4){ /*  old steady state -- just extend it  */
    auto_extend_ss();
    ping();return;
  }
  if(grabpt.ibr>0&&ips==4){
    auto_extend_bvp();
    ping();
    return;
  }
}

void auto_homo_choice(int itp)
{
  /* printf("in choice: itp=%d\n",itp); */
  if(itp!=5)
    auto_extend_homoclinic();
  
}
void auto_branch_choice(int ibr, int ips)
{

  static const char *m[]={"Switch","Extend","New Point","Two Param"};
  static  char key[]="sent";
  char ch;
  int ipsuse;
  ch=(char)auto_pop_up_list(str("Branch Pt"),strs(m),key,4,10,0,10,10,
		       no_hint,Auto.hinttxt);


  if(ch=='s'){
       if(ibr<0&&ips==2)
      auto_switch_per();
    else 
      if(ips==4)
	auto_switch_bvp();
      else
	auto_switch_ss();
    return;
  }
  if(ch=='e'){
    auto_extend_ss();
    return;
  }
  if(ch=='n'){
    auto_new_ss();
    return;
  }
  if(ch=='t'){
 
    ipsuse=1;
    if(ips==4)
      ipsuse=4;
    if(ibr<0)
      ipsuse=2;
    auto_2p_branch(ipsuse);
    /* auto_2p_limit(ips); */
    return;
  }
  redraw_auto_menus();
}


/*  RUN AUTO HERE */
/*  these are for setting the parameters to run for different choices    */

/*  Just a short recall of the AUTO parameters
   NBC = 0 unless it really is a BVP problem (not periodics or heteroclinics)
   NICP = 1 for 1 parameter and 2 fro 2 parameter and the rest will 
            be taken care of in AUTO  and I think 2 for hetero?
   ILP = 1 (0) detection (no) of folds usually 1
   ISP = 2  detect all special points! but I think maybe set to 0, 1 for 
            BVP I think 
            for 2 parameter continuation ?
   ISW = -1 branch switching 1 is for normal 2 for two parameter of folds, tori,HB, PD!!

   IPS   1 - std for steady states of ODEs
         -1 maps
         2 periodic orbits
         4 BVP  (set NBC=NODE)
         9 Homoclinic
         
   
for example   2 P continuation of HB
              IPS=1 ILP=1 NICP=2 ISP=0 ISW=2 
BVP problem   IPS=4, NICP=1 NBC=NODE ISP=1 ISW=1 ILP=1

discrete dynamical system with two par of Hopf
first IPS=-1 ISP=ISW=1  then 
NICP=2, ISW=2 at Hopf




*/   

/* Start a new point for bifurcation diagram   */

void auto_start_diff_ss()
{
  TypeOfCalc=EQ1;
  Auto.ips=1;
  if(METHOD==DISCRETE)Auto.ips=-1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
  AutoTwoParam=0;
  do_auto(NO_OPEN_3,APPEND,Auto.itp);
}

void auto_start_at_bvp()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
 compile_bvp();
  if(BVP_FLAG==0)
    return; 
  TypeOfCalc=BV1;
 Auto.ips=4;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;

  Auto.isp=2;
  if(SuppressBP==1) Auto.isp=0;
    
  Auto.nfpar=1;
  AutoTwoParam=0;
  NewPeriodFlag=2;
  do_auto(opn,cls,Auto.itp);
}


void auto_start_at_per()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  TypeOfCalc=PE1;
  Auto.ips=2;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;

  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
  AutoTwoParam=0;
  NewPeriodFlag=1;
  do_auto(opn,cls,Auto.itp);
}


  
void auto_new_ss()
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  NewPeriodFlag=0;

  if(NBifs>1){
    ans=reset_auto();
    if ((ans!=0) || (ans!=1))
    {
       plintf("Boolean response expected.\n");	
    }
   /* if(ans==0){
      opn=OPEN_3;
      cls=APPEND;
    } */
  }
      TypeOfCalc=EQ1;
  Auto.ips=1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
      if(SuppressBP==1) Auto.isp=0;;
  Auto.nfpar=1;
   AutoTwoParam=0;
  do_auto(opn,cls,Auto.itp);
}


void auto_new_discrete()
{
  int ans;
  int opn=NO_OPEN_3,cls=OVERWRITE;
  NewPeriodFlag=0;
  if(NBifs>1){
    ans=reset_auto();
    if ((ans!=0) || (ans!=1))
    {
       plintf("Boolean response expected.\n");	
    }
   /* if(ans==0){
      opn=OPEN_3;
      cls=APPEND;
    } */
  }
  TypeOfCalc=DI1;
  Auto.ips=-1;
  Auto.irs=0;
  Auto.itp=0;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.nfpar=1;
   AutoTwoParam=0; 
  do_auto(opn,cls,Auto.itp);
}
 
void auto_extend_ss()
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg(str("Can't continue infinite period Hopf!"));
  	return;
  } 
  
      TypeOfCalc=EQ1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
    
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

int get_homo_info(int flg,int *nun,int *nst,double *ul, double *ur)
{
  char **s;
  char v[100][MAX_LEN_SBOX];
  int n=2+2*NODE;
  int i;
  int flag=0;
  s=(char **)xpp_malloc(n *sizeof(char *));
  for(i=0;i<n;i++){
   s[i]=(char *)xpp_malloc(XPP_NAME_MAX+8); /* name_L, name_R */

  }
  /* each s[i] is a pointer, allocated XPP_NAME_MAX+8 bytes just above. */
  xpp_snprintf(s[0],XPP_NAME_MAX+8,"dim unstable");
  XPP_SPRINTF(v[0],"%d",*nun);
  xpp_snprintf(s[NODE+1],XPP_NAME_MAX+8,"dim stable");
  XPP_SPRINTF(v[NODE+1],"%d",*nst);
  for(i=0;i<NODE;i++){
    xpp_snprintf(s[i+1],XPP_NAME_MAX+8,"%s_L",uvar_names[i]);
    XPP_SPRINTF(v[i+1],"%g",ul[i]);
    xpp_snprintf(s[i+2+NODE],XPP_NAME_MAX+8,"%s_R",uvar_names[i]);
    XPP_SPRINTF(v[i+2+NODE],"%g",ur[i]);
  }
 
  flag=do_string_box(n,n/2,2,str("Homoclinic info"),s,v,16); 
  if(flag!=0){
    *nun=atoi(v[0]);
    *nst=atoi(v[NODE+1]);
    for(i=0;i<NODE;i++){
      ul[i]=atof(v[i+1]);
      if(HomoFlag==2)
	ur[i]=atof(v[i+2+NODE]);
    }
  }
  for(i=0;i<n;i++){
    xpp_free(s[i]);

  }
  xpp_free(s);

  return flag;
}

void three_parameter_homoclinic()
{
Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;

      TypeOfCalc=HO2;
  AutoTwoParam=HO2;
  NewPeriodFlag=1;
  Auto.ips=9;

  Auto.nfpar=3;
  Auto.ilp=0;
  Auto.isw=1;
  Auto.isp=0;
  Auto.nbc=0;
  
  if(HomoFlag==1)
    xAuto.iequib=1;
  if(HomoFlag==2)
    xAuto.iequib=-2;

  
  
  do_auto(OPEN_3,APPEND,Auto.itp);


  
}





void auto_extend_homoclinic()
{
   Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;

      TypeOfCalc=HO2;
  AutoTwoParam=HO2;
  NewPeriodFlag=1;
  Auto.ips=9;

  Auto.nfpar=2;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=0;
  Auto.nbc=0;
  
  if(HomoFlag==1)
    xAuto.iequib=1;
  if(HomoFlag==2)
    xAuto.iequib=-2;

  
  
  do_auto(OPEN_3,APPEND,Auto.itp);


  
}



    
void auto_start_at_homoclinic()
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  int flag;
  Auto.irs=0;
  Auto.itp=0;
    TypeOfCalc=HO2;


  AutoTwoParam=HO2;
  NewPeriodFlag=1;
  Auto.ips=9;

  Auto.nfpar=2;
  Auto.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  Auto.isw=1;
  Auto.isp=0;
  Auto.nbc=0;
  
  if(HomoFlag==1){
    xAuto.iequib=1;
    find_best_homo_shift(NODE);
  }
  if(HomoFlag==2)
    xAuto.iequib=-2;
  flag=get_homo_info(HomoFlag,&xAuto.nunstab,&xAuto.nstab,homo_l,homo_r);
  if(flag)do_auto(opn,cls,Auto.itp);

  
}
    
void auto_new_per() /* same for extending periodic  */
{
  blrtn.torper=grabpt.torper;
  
  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg(str("Can't continue infinite period Hopf."));
  	return;
  } 	
      TypeOfCalc=PE1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  /* Auto.nfpar=grabpt.nfpar; */
  Auto.nfpar=1;
  Auto.ilp=1;
  Auto.isw=1; /* -1 */
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
    AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_extend_bvp() /* extending bvp */
{
      TypeOfCalc=BV1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=4;
    AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}



void auto_switch_per()
{
      TypeOfCalc=PE1;
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=1; /*grabpt.nfpar;*/
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_switch_bvp()
{
     TypeOfCalc=BV1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=4;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_switch_ss()
{

      TypeOfCalc=EQ1;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=grabpt.nfpar;
  Auto.ilp=1;
  Auto.isw=-1;
  Auto.isp=1;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_limit(int ips)
{
  int ipsuse=1;
  int itp1,itp2;
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0; /* was 2 */
  /* fix ips now */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==5)||(itp2==5))
      ipsuse=2;
  }

  
  Auto.ips=ipsuse;
  AutoTwoParam=LPP2;
  if(ipsuse==1){
    TypeOfCalc=LPE2;
    AutoTwoParam=LPE2;
  }
  else{
    TypeOfCalc=LPP2;
    AutoTwoParam=LPP2;
  }
  /* printf("ips=%d  itp=%d \n",Auto.ips,Auto.itp); */
  /* plintf(" IPS = %d \n",ips); */
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_twopar_double()
{

  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  AutoTwoParam=PD2;
  TypeOfCalc=PD2;
  Auto.ips=2;
  Auto.ilp=0;
  Auto.isw=2;
  Auto.isp=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_torus()
{
  blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  AutoTwoParam=TR2;
  TypeOfCalc=TR2;
  Auto.ips=2;
  Auto.ilp=0;
  Auto.isw=2;
  Auto.isp=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_branch(int ips)
{
 int ipsuse=1;
  int itp1,itp2; 
 blrtn.torper=grabpt.torper;
  Auto.irs=grabpt.lab;
  itp1=(grabpt.itp)%10;
  itp2=abs(grabpt.itp)/10;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0; /* was 2 */
  if(ips==4)
    ipsuse=4;
  else {
    if((itp1==6)||(itp2==6))
      ipsuse=2;
  }

  
  Auto.ips=ipsuse;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=BR2;
      TypeOfCalc=BR2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

    
void auto_2p_fixper()
{
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=1; /* was1 */
  Auto.isw=1;
  Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=FP2;
  TypeOfCalc=FP2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_2p_hopf()
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if (isinf(grabpt.per))
  {
  	err_msg(str("Can't continue infinite period Hopf."));
  	return;
  } 
  
  Auto.irs=grabpt.lab;
  Auto.itp=grabpt.itp;
  Auto.nfpar=2;
  Auto.ilp=0; /* was 1 */
  Auto.isw=2;
  Auto.isp=0;
  Auto.ips=1;
  if(METHOD==DISCRETE)
    Auto.ips=-1;
  AutoTwoParam=HB2;
    TypeOfCalc=HB2;
  do_auto(OPEN_3,APPEND,Auto.itp);
}

void auto_period_double()
{

 blrtn.torper=grabpt.torper;
  Auto.ntst=2*Auto.ntst;
  Auto.irs=grabpt.lab;
  Auto.nfpar=1; /* grabpt.nfpar; */

  Auto.itp=grabpt.itp;
  Auto.ilp=1;
  Auto.isw=-1;
  TypeOfCalc=PE1;
  Auto.isp=2;
    if(SuppressBP==1) Auto.isp=0;
  Auto.ips=2;
  AutoTwoParam=0;
  do_auto(OPEN_3,APPEND,Auto.itp);
}


/**********   END RUN AUTO *********************/

void auto_err(char *s)
{
  err_msg(s);
}

void load_auto_orbit()
{
  load_auto_orbitx(grabpt.ibr,grabpt.flag,grabpt.lab,grabpt.per);
}
  void load_auto_orbitx(int ibr,int flag, int lab, double per)
{
  FILE *fp;
  double *x;
  int i,j,nstor;
  double u[NAUTO],t;
  double period;
  char string[256];
  int nrow,ndim,label,flg;
  /* printf("Loading orbit ibr=%d ips=%d flag=%d\n",grabpt.ibr,Auto.ips, grabpt.flag);  */
   
  if((ibr>0&&(Auto.ips!=4)&&(Auto.ips!=3)&&(Auto.ips!=9))||flag==0)return; 
   /* either nothing grabbed or just a fixed point and that is already loaded */
  XPP_SPRINTF(string,"%s.s",this_auto_file);
  fp=fopen(string,"r");
  if(fp==NULL){
    auto_err(str("No such file"));
    return;
  }
  label=lab;
  period=per;
  flg=move_to_label(label,&nrow,&ndim,fp);
  nstor=ndim;
  if(ndim>NODE)nstor=NODE;
  if(flg==0){
    xpp_log_auto("Could not find label %d in file %s \n",label,string);
    auto_err(str("Cant find labeled pt"));
    fclose(fp);
    return;
  }
  x=&MyData[0];
  for(i=0;i<nrow;i++){
    get_a_row(u,&t,ndim,fp);
    if(Auto.ips!=4) 
      storage[0][i]=t*period;
    else
      storage[0][i]=t;
      
    
    for(j=0;j<nstor;j++){
      storage[j+1][i]=u[j];
      x[j]=u[j];
    }
    extra(x,(double)storage[0][i],nstor,NEQ);
    for(j=nstor;j<NEQ;j++)
      storage[j+1][i]=(float)x[j];
  }
  storind=nrow;
  refresh_browser(nrow);
  /* insert auxiliary stuff here */
  if(load_all_labeled_orbits==2)clr_all_scrns();
  drw_all_scrns();
  fclose(fp);
}


     

void save_auto()
{

  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  int status;
  /* XGetInputFocus(display,&w,&rev); */
  
  XPP_SPRINTF(filename,"%s.auto",basename(this_auto_file));
  /* status=get_dialog("Save Auto","Filename",filename,"Ok","Cancel",60);
  XSetInputFocus(display,w,rev,CurrentTime);
  */
  status=file_selector(str("Save Auto"),filename,str("*.auto"));
  if(status==0)return;
  if(!may_write_file(filename))return;
  /* written beside filename and renamed over it once whole */
  xpp::Writer w(filename);
  if(!w){
    err_msg(str("Cannot open file"));
    return;
  }
  status=save_auto_file(w.file());
  if(status!=1){
    /* an empty diagram: say so, and leave no file without orbits (nor
       replace an existing one with it) */
    w.abort();
    auto_err(str("Empty diagram -- nothing to save"));
    return;
  }
  w.commit();
}

/* save_auto without its dialog (xpp_session.c): 1 written, else the
   diagram was empty and fp holds only the numerics and graph header */
int save_auto_file(FILE *fp)
{
  int status;
  save_auto_numerics(fp);
  save_auto_graph(fp);
  status=save_diagram(fp,NODE);
  if(status!=1)return status;
  save_q_file(fp);
  return 1;
}
 
void save_auto_numerics(FILE *fp)
{
  int i;
 fprintf(fp,"%d ",NAutoPar);
 for(i=0;i<NAutoPar;i++)
   fprintf(fp,"%d ",AutoPar[i]);
  fprintf(fp,"%d\n",NAutoUzr);
  for(i=0;i<9;i++)
    fprintf(fp,"%g %ld\n",outperiod[i],UzrPar[i]);
 fprintf(fp,"%d %d %d \n",Auto.ntst,Auto.nmx,Auto.npr);
 fprintf(fp,"%g %g %g \n",Auto.ds,Auto.dsmin,Auto.dsmax);
 fprintf(fp,"%g %g %g %g\n",Auto.rl0,Auto.rl1,Auto.a0,Auto.a1);
 fprintf(fp,"%d %d %d %d %d %d %d\n",aauto.iad,aauto.mxbf,aauto.iid,aauto.itmx,aauto.itnw,aauto.nwtn,aauto.iads);
}


void load_auto_numerics(FILE *fp)
{
 int i,in;
 /* The fscanf formats this replaces ended in whitespace, which fscanf
    skipped; the token reader leaves it in the stream, and every read that
    follows (load_auto_graph, load_diagram) skips it first. */
 xpp::TokenReader tr=xpp::TokenReader::attach(fp);
 if (!tr.read(NAutoPar)) return;
 for(i=0;i<NAutoPar;i++){
   if (!tr.read(AutoPar[i])) return;
   in=get_param_index(upar_names[AutoPar[i]]);
   Auto_index_to_array[i]=in;
 }
 if (!tr.read(NAutoUzr)) return;
  for(i=0;i<9;i++){
    Auto.nper=NAutoUzr;
    if (!tr.read(outperiod[i]) || !tr.read(UzrPar[i])) return;
    Auto.period[i]=outperiod[i];
    Auto.uzrpar[i]=UzrPar[i];
    /*    printf("%g %d\n",Auto.period[i],Auto.uzrpar[i]); */
  }

 if (!tr.read(Auto.ntst) || !tr.read(Auto.nmx) || !tr.read(Auto.npr)) return;
 if (!tr.read(Auto.ds) || !tr.read(Auto.dsmin) || !tr.read(Auto.dsmax)) return;
 if (!tr.read(Auto.rl0) || !tr.read(Auto.rl1) || !tr.read(Auto.a0) || !tr.read(Auto.a1)) return;
 if (!tr.read(aauto.iad) || !tr.read(aauto.mxbf) || !tr.read(aauto.iid) || !tr.read(aauto.itmx)
     || !tr.read(aauto.itnw) || !tr.read(aauto.nwtn) || !tr.read(aauto.iads)) return;
}

void save_auto_graph(FILE *fp)
{
  fprintf(fp,"%g %g %g %g %d %d \n",Auto.xmin,Auto.ymin,Auto.xmax,Auto.ymax,
	Auto.var,Auto.plot);
}

void load_auto_graph(FILE *fp)
{
  xpp::TokenReader tr=xpp::TokenReader::attach(fp);
  if (!tr.read(Auto.xmin) || !tr.read(Auto.ymin) || !tr.read(Auto.xmax) || !tr.read(Auto.ymax)
      || !tr.read(Auto.var) || !tr.read(Auto.plot)) return;
}
  
void save_q_file(FILE *fp) /* I am keeping the name q_file even though they are s_files */
{
  char string[500];
  XPP_SPRINTF(string,"%s.s",this_auto_file);
  xpp::LineReader lr(string);
  if(!lr){
    auto_err(str("Couldnt open s-file"));
    return;
  }
  while(auto line=lr.next()){
    fwrite(line->data(),1,line->size(),fp);
    fputc('\n',fp);
  }
}

void make_q_file(FILE *fp)
{
  char string[500];
  XPP_SPRINTF(string,"%s.s",this_auto_file);
  /* written beside the .s and renamed over it once whole */
  xpp::Writer w(string);
  if(!w){
    auto_err(str("Couldnt open s-file"));
    return;
  }

  /* the rest of fp, the .auto's copy of the .s, without its blank lines */
  xpp::LineReader lr=xpp::LineReader::attach(fp);
  while(auto line=lr.next()){
    std::string l(*line);
    if(!noinfo(l.data())){
      l+='\n';
      fwrite(l.data(),1,l.size(),w.file());
    }
  }
  w.commit();
}
  
int noinfo(char *s) /* get rid of any blank lines  */
{
  int n=strlen(s);
  int i;
  if(n==0)return(1);
  for(i=0;i<n;i++){
    if(!isspace(s[i]))return(0);
  }
  return(1);
}

void load_auto()
{

  int ok;
  FILE *fp;
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  int status;
  if(NBifs>1){
    ok=reset_auto();
    if(ok==0)return;
  }

  XPP_SPRINTF(filename,"%s.auto",basename(this_auto_file));
 
  status=file_selector(str("Load Auto"),filename,str("*.auto"));
  if(status==0)return;
  fp=fopen(filename,"r");
  if(fp==NULL){
    auto_err(str("Cannot open file"));
    return;
  }
  
  load_auto_file(fp);
  fclose(fp);
}

/* load_auto without its reset and dialog (xpp_session.c): 1 loaded,
   -1 an empty diagram */
int load_auto_file(FILE *fp)
{
  int status;
  load_auto_numerics(fp);
  load_auto_graph(fp);
  status=load_diagram(fp,NODE);
  if(status!=1)return status;
  make_q_file(fp);
  return 1;
}

int move_to_label(int mylab, int *nrow, int *ndim, FILE *fp)
{
  int ibr=0,ntot=0,itp=0,lab=0,nfpar=0,isw=0,ntpl=0,nar=0,nskip=0;
  int i;
  /* attached to fp: the rows after the label line found are get_a_row()'s */
  xpp::LineReader lr=xpp::LineReader::attach(fp);
  while(auto line=lr.next()){
    /* the label line's "%5ld" columns may touch: sscanf reads them apart */
    std::string l(*line);
    sscanf(l.c_str(),"%d%d %d %d %d %d %d %d %d",
	   &ibr,&ntot,&itp,&lab,&nfpar,&isw,&ntpl,&nar,&nskip);
    if(mylab==lab){
      *nrow=ntpl;
      *ndim=nar-1;
      return(1);
    }
    for(i=0;i<nskip;i++)
      if(!lr.next())break;
  }
  return(0);
}

void get_a_row(double *u, double *t, int n, FILE *fp)
 {
   int i;
   xpp::TokenReader tr=xpp::TokenReader::attach(fp);
   if (!tr.read(*t)) return;
   for(i=0;i<n;i++)
     if (!tr.read(u[i])) return;
 }



void auto_file()
{
 
  static const char *m[]={"Import orbit","Save diagram","Load diagram","Postscript","SVG",
		    "Reset diagram","Clear grab","Write pts","All info","init Data","Toggle redraw","auto raNge","sElect 2par pt","draw laBled","lOad branch"};
  static  char key[]="islpvrcwadtnebo";
  char ch;
  ch=(char)auto_pop_up_list(str("File"),strs(m),key,15,15,0,10,10,afile_hint,
		       Auto.hinttxt);
  if(ch=='i'){
    load_auto_orbit();
    return;
  }
  if(ch=='s'){
    save_auto();
    return;
  }
  if(ch=='l'){
    load_auto();
    redraw_diagram(); /* the loaded diagram, at once */
    return;
  }
  if(ch=='r'){
    reset_auto();
    redraw_diagram(); /* now empty */
  }
  if(ch=='c'){
    grabpt.flag=0;
  }
  if(ch=='p'){
    NoBreakLine=1;
    post_auto();
    NoBreakLine=0;
  }
  if(ch=='v'){
    NoBreakLine=1;
    svg_auto();
    NoBreakLine=0;
  }
  if(ch=='w'){
    write_pts();
  }
  if(ch=='a'){
    write_info_out();
  }
  if(ch=='d'){
    write_init_data_file();
  }
  if(ch=='t'){
    auto_redraw=1-auto_redraw;
    if(auto_redraw==1)err_msg(str("Redraw is ON"));
    else err_msg(str("Redraw is OFF"));
  }
  if(ch=='o'){
    if(diagram_mark.state<2)
      err_msg(str("Mark a branch first using S and E"));
    else
      load_browser_with_branch(diagram_mark.start_branch,diagram_mark.start_point,diagram_mark.end_point);
	}	
  if(ch=='n'){
    if(diagram_mark.state<2) 
      err_msg(str("Mark a branch first using S and E"));
    else
      do_auto_range();
  }
  if(ch=='e'){
    if(Auto.plot!=P_P){
      err_msg(str("Must be in 2 parameter plot"));
      return;
    }
    setautopoint();

  }
  if(ch=='b'){
    if(load_all_labeled_orbits==0){
      load_all_labeled_orbits=1;
      err_msg(str("Draw orbits - no erase"));
      return;
    }
     if(load_all_labeled_orbits==1){
      load_all_labeled_orbits=2;
      err_msg(str("Draw orbits - erase first"));
      return;
    }
      if(load_all_labeled_orbits==2){
      load_all_labeled_orbits=0;
      err_msg(str("Draw orbits off"));
      return;
    }
  }
    


}











void  auto_get_info( int *n, char *pname )
{
  int i1,i2,ibr;
  DIAGRAM *d,*dnew;


  if(diagram_mark.state==2){
    i1=abs(diagram_mark.start_point);
    ibr=diagram_mark.start_branch;
    i2=abs(diagram_mark.end_point);
    *n=abs(i2-i1);
    d=bifd;
    while(1){
      if(d->ibr==ibr && ((d->ntot==i1)||(d->ntot==(-i1))))
	{
	  /* pname's one real caller (integrate.c) passes char parn[256] */
	  xpp_strlcpy(pname,upar_names[AutoPar[d->icp1]],256);
	  break;
	}
       dnew=d->next;
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
    }
  }
  
}

void auto_set_mark(int i)
{
  int pt,ibr;
  if(diagram_mark.state==2){
    ibr=diagram_mark.start_branch;
    if(abs(diagram_mark.start_point)<abs(diagram_mark.end_point))
      pt=abs(diagram_mark.start_point)+i;
    else
      pt=abs(diagram_mark.end_point)+i;
    find_point(ibr,pt);
  }
}

void find_point(int ibr, int pt)
{
  int i;
  DIAGRAM *d,*dnew;
   if(NBifs<2)return;
   d=bifd;
   while(1)
     {
       if(d->ibr==ibr && ((d->ntot==pt)||(d->ntot==(-pt))))
	 {  /* need to look at both signs to ignore stability */
	   /* now we use this info to set parameters and init data */
	   for(i=0;i<NODE;i++)
	     set_ivar(i+1,d->u0[i]);
	   get_ic(0,d->u0);
	   for(i=0;i<NAutoPar;i++)
	     constants[Auto_index_to_array[i]]=d->par[i];
	   evaluate_derived();
	   redo_all_fun_tables();
	   redraw_params();
	   redraw_ics();
           if((d->per)>0)
	     set_total(d->per);		       
	   break;
	 }
       dnew=d->next;
       if(dnew==NULL){
	 
	 break;
       }
       d=dnew;
     }
}

void do_auto_range()
{
  double t=TEND;
  
  if(diagram_mark.state==2)
    do_auto_range_go();
  TEND=t;
}

void DLINE(double a,double b,double c,double d)
{
  ALINE(IXVal(a),IYVal(b),IXVal(c),IYVal(d));
}

/* ---- grabbing a point on the bifurcation diagram, marking a branch and
   the hint line (logic from auto_x11.c) ---- */
/* auto_c.h's LEFT/RIGHT (1/2, unused below) are for AUTO's own
   continuation direction; undef them so mykeydef.h's key codes (6/2,
   used by the switch below) don't warn about redefining a different
   value. */
#undef LEFT
#undef RIGHT
#include "mykeydef.h"
extern char *aspecial_hint[];
DIAGRAM *CUR_DIAGRAM;

int query_special(char* title,char *nsymb)
{
        /* nsymb is a pointer here; both callers pass a char[3] (symb/
           nsymb below), matching the longest label written below. */
        int status=1;
        static const char *m[]={"BP","EP","HB","LP","MX","PD","TR","UZ"};
	static  char key[]="behlmptu";
	int ch=(char)auto_pop_up_list(title,strs(m),key,8,11,1,10,10,
			     aspecial_hint,Auto.hinttxt);
	if(ch=='b'){
	  xpp_snprintf(nsymb,3,"BP");
	}
	else if(ch=='e'){
	  xpp_snprintf(nsymb,3,"EP");
	}
	else if(ch=='h'){
	   xpp_snprintf(nsymb,3,"HB");
	}
	else if(ch=='l'){ 
	   xpp_snprintf(nsymb,3,"LP");
	}
	else if(ch=='m'){ 
	   xpp_snprintf(nsymb,3,"MX");
	}
	else if(ch=='p'){
	   xpp_snprintf(nsymb,3,"PD"); 
	}
	else if(ch=='t'){
	   xpp_snprintf(nsymb,3,"TR");  
	}
	else if(ch=='u'){ 
	   xpp_snprintf(nsymb,3,"UZ");
	}
	else
	{
	   status=0;   
	   xpp_snprintf(nsymb,3,"  ");
	}
	redraw_auto_menus();
	return(status);
}

void traverse_diagram()
{
  DIAGRAM *d,*dnew,*dold;
  int done=0;
  int ix,iy,i; 
  int lalo;
  int kp;
  int xm,ym;
  diagram_mark.state=0;
  if(NBifs<2)return;
  
  d=bifd; 
  DONT_XORCross=0;
  traverse_out(d,&ix,&iy,1);
  
  while(done==0){
    kp=xpp_ui.auto_grab_event(&xm,&ym);
    if(kp==XPP_AUTO_NODE)
    {
      /* a point of the diagram by its entry: the cursor goes there */
      dnew=bifd;
      while(dnew!=NULL&&dnew->index!=xm)dnew=dnew->next;
      if(dnew!=NULL){
        clear_msg();
        XORCross(ix,iy);
        d=dnew;
        CUR_DIAGRAM=d;
        traverse_out(d,&ix,&iy,1);
      }
    }
    else if(kp==XPP_AUTO_CLICK)
    {
	{
       		clear_msg();
	        /*
		GO HOME
		*/
		XORCross(ix,iy);
		DONT_XORCross = 1;
		while (1){
        		dnew=d->prev;
        		if(dnew==NULL){dnew=d;break;}
        		/*bifd = dnew;*/
        		d=dnew;
		}
		d=dnew;
		CUR_DIAGRAM=d;
		traverse_out(d,&ix,&iy,0);
                /*
		END GO HOME
		*/
       
       		/*
		GO END
		*/
		int mindex=0;
		double dist;
		double ndist = Auto.wid*Auto.hgt;
		XORCross(ix,iy);
                lalo=load_all_labeled_orbits;
		load_all_labeled_orbits=0;
		while (1)
		{
			dist = sqrt(((double)(xm-ix))*((double)(xm-ix)) + ((double)(ym-iy))*((double)(ym-iy))); 
			if (dist<ndist)
			{
				ndist = dist;
				mindex=d->index;
			}
			dnew=d->next;
			if(dnew==NULL){dnew=d;break;}
			d=dnew;
			traverse_out(d,&ix,&iy,0);/*Need this each time to update the distance calc*/
	        }
		d=dnew;
       		CUR_DIAGRAM=d;
		load_all_labeled_orbits=lalo;
       		traverse_out(d,&ix,&iy,0);
		/*
		END GO END
		*/
		 
      
		
		/*
		GO HOME
		*/
		XORCross(ix,iy);
		while (1){
		        if (d->index == mindex){dnew=d;break;}
        		dnew=d->prev;
        		if(dnew==NULL){dnew=d;break;}
        		/*bifd = dnew;*/
        		d=dnew;
		}
		d=dnew;
		CUR_DIAGRAM=d;
		DONT_XORCross = 0;
		traverse_out(d,&ix,&iy,1);
                /*
		END GO HOME
		*/
		
	}
    }
    else {
        clear_msg();
	char symb[3],nsymb[3];
        
	int found=0;

      switch(kp){
      case RIGHT:
	dnew=d->next;
	if(dnew==NULL)dnew=bifd;
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(d,&ix,&iy,1);
	break;
	
      case LEFT:
	dnew=d->prev;
	if(dnew==NULL)dnew=bifd;
	XORCross(ix,iy);
	d=dnew;
	CUR_DIAGRAM=dnew;
	traverse_out(d,&ix,&iy,1);
	break;
      case UP:
       if (!query_special(str("Next..."),nsymb)){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=d->next;
	 if(dnew==NULL){dnew=d;break;} 
	 get_bif_sym(symb,dnew->itp);
	 if(strcmp(symb,nsymb)==0){d=dnew;found=1;break;} 
         d=dnew;
         /*if(d->lab==0)break;*/
       }
       if (found)
       {
         d=dnew;
       }
       else
       {
         snprintf(Auto.hinttxt,255,"  Higher %s not found",nsymb);
	 xpp_ui.auto_show_hint();
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
      case DOWN:
       if (!query_special(str("Previous..."),nsymb)){break;}
       XORCross(ix,iy);
       found=0;
       dold=d;
       while(1){
         dnew=d->prev;
	 if(dnew==NULL){dnew=d;break;} 
	 get_bif_sym(symb,dnew->itp);
	 if(strcmp(symb,nsymb)==0){d=dnew;found=1;break;} 
         d=dnew;
       }
       if (found)
       {
         d=dnew;
       }
       else
       {
         snprintf(Auto.hinttxt,255,"  Lower %s not found",nsymb);
	 xpp_ui.auto_show_hint();
	 d=dold;
       }
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break; 
      case TAB:
       XORCross(ix,iy);
       while(1){
         dnew=d->next;
         if(dnew==NULL){dnew=bifd;break;} /*TAB wraps*/
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
	/* New code */
      case 's': /* mark the start of a branch */
	if(diagram_mark.state==0) {
	  MarkAuto(ix,iy);
	  diagram_mark.start_branch=d->ibr;
	  diagram_mark.start_point=d->ntot;
	  diagram_mark.state=1;
	  diagram_mark.start_x=ix;
	  diagram_mark.start_y=iy;

	}
	break;
      case 'e': /* mark end of branch */
	if(diagram_mark.state==1){
	  MarkAuto(ix,iy);
	  diagram_mark.end_branch=d->ibr;
	  diagram_mark.end_point=d->ntot;
	  diagram_mark.state=2;
	  diagram_mark.end_x=ix;
	  diagram_mark.end_y=iy;

	}
	break;
       case END:/*All the way to end*/
       XORCross(ix,iy);
       while (1){
               dnew=d->next;
               if(dnew==NULL){dnew=d;break;}
               /*bifd = dnew;*/
               d=dnew;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case HOME:/*All the way to beginning*/
       XORCross(ix,iy);
       while (1){
               dnew=d->prev;
               if(dnew==NULL){dnew=d;break;}
               /*bifd = dnew;*/
               d=dnew;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case PGUP: /*Same as TAB except we don't wrap*/
       XORCross(ix,iy);
       while(1){
         dnew=d->next;
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
       case PGDN: /*REVERSE TAB*/
       XORCross(ix,iy);
       while(1){
         dnew=d->prev;
         if(dnew==NULL){dnew=d;break;}
         d=dnew;
         if(d->lab!=0)break;
       }
       d=dnew;
       CUR_DIAGRAM=d;
       traverse_out(d,&ix,&iy,1);
       break;
      
      case FINE:
	done=1;
	XORCross(ix,iy);
	/*Cross should be erased now that we have made our selection.*/
	/*Seems XORing it with new draw can tend to bring it back randomly
	depending on the order of window expose events.  Best not
	to do the XORCross function at all.*/
	DONT_XORCross = 1;
	xpp_ui.auto_grab_end(1);
	break;
      case ESC:
	done=-1;
	xpp_ui.auto_grab_end(-1);
	break;
      }
    }
    
  }
  /*XORCross(ix,iy);
*/
  /* check mark_flag branch similarity */
  if(diagram_mark.state==2){
    if(diagram_mark.start_branch!=diagram_mark.end_branch)
      diagram_mark.state=0;
  }
  if(done==1){
    grabpt.ibr=d->ibr;
    grabpt.lab=d->lab;
    for(i=0;i<8;i++)
    grabpt.par[i]=d->par[i];
    grabpt.icp1=d->icp1;
    grabpt.icp2=d->icp2;
    grabpt.per=d->per;
    grabpt.torper=d->torper;
    for(i=0;i<NODE;i++){
      grabpt.uhi[i]=d->uhi[i];
      grabpt.ulo[i]=d->ulo[i];
      grabpt.u0[i]=d->u0[i];
      grabpt.ubar[i]=d->ubar[i];
      set_ivar(i+1,grabpt.u0[i]);
    }
    get_ic(0,grabpt.u0);
    grabpt.flag=1;
    grabpt.itp=d->itp;
    grabpt.ntot=d->ntot;
    grabpt.nfpar=d->nfpar;
    grabpt.index=d->index;
    for(i=0;i<NAutoPar;i++)
      constants[Auto_index_to_array[i]]=grabpt.par[i];
  }
  evaluate_derived();
  redo_all_fun_tables();
  redraw_params();
  redraw_ics();
}

void RedrawMark()
{
  if(diagram_mark.state==2){
    MarkAuto(diagram_mark.start_x,diagram_mark.start_y);
    MarkAuto(diagram_mark.end_x,diagram_mark.end_y);
  }
}

void MarkAuto(int x, int y)
{

  LineWidth(2);
  ALINE(x-8,y-8,x+8,y+8);
  ALINE(x+8,y-8,x-8,y+8);
  LineWidth(1);



}

void clear_msg()
{
  Auto.hinttxt[0]='\0';
  xpp_ui.auto_show_hint();
}

void auto_update_view(float xlo,float xhi, float ylo, float yhi)
{
              Auto.xmin=xlo;
	      Auto.ymin=ylo;
	      Auto.xmax=xhi;
	      Auto.ymax=yhi;
	      redraw_diagram();

}

/* the pointer moved to pixel (i,j) of the diagram: show its coordinates */
void auto_motion_xy(int i,int j)
{
  double x,y;
    x=Auto.xmin+(double)(i-Auto.x0)*(Auto.xmax-Auto.xmin)/(double)Auto.wid;
    y=Auto.ymin+(double)(Auto.y0-j+Auto.hgt)*(Auto.ymax-Auto.ymin)/(double)Auto.hgt;
    auto_point_xy(x,y);
}

void auto_point_xy(double x,double y)
{
    XPP_SPRINTF(Auto.hinttxt,"x=%g,y=%g",x,y);
    storeautopoint(x,y);
    xpp_ui.auto_show_hint();
}
