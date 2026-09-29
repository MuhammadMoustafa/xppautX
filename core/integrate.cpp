#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "auto_nox.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "integrate.h"

#include "storage.h"

#include "expr.h"
#include "markov.h"
#include "tabular.h"
#include "adj2.h"
#include "browse.h"
#include "derived.h"
#include "gear.h"
#include "graf_par.h"
#include "graphics.h"
#include "kinescope.h"
#include "lunch-new.h"
#include "del_stab.h"
#include "flags.h"
#include "histogram.h"

#include "nullcline.h"

#include "pp_shoot.h"
#include "dae_fun.h"
#include "my_ps.h"
#include "my_svg.h"
#include "numerics.h"
#include "my_rhs.h" /* extra */
#include "volterra2.h"
#include <stdlib.h> 
#include "aniparse.h"
#include "delay_handle.h"

/*    this is the main integrator routine  
      for phase-plane  
      It takes the steps looks at the interrupts, plots and stores the data
      It also loads the delay stuff if required      
 
*/

/* The steps are the method's xpp::Solver's (solver.h): the Session's
   integrator.solver, which xpp::start_solver made for numerics.method. */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include "menudrive.h"
#include "arrayplot.h"
#include "xpp_job.h"
#include "phase_data.h"
#include "xpp_batch.h"
#include "colormap.h"
#include "comline.h"

/* a row was just stored (storage[.][storind-1]): a replayed script may stop
   the job here (xpp_job.h), and a front end may show the run growing */
static void row_stored(void)
{
  xpp_job_rows_stored(xpp::session().data_store.rows, xpp::session().data_store.col[0][xpp::session().data_store.rows-1]);
  rows_stored(xpp::session().data_store.rows);
}
#define MSWTCH(u,v) memcpy(static_cast<void *>((u)),static_cast<void *>((v)),xpp::session().solver_work.xpv.node*sizeof(double))

#define READEM 1

#define ESCAPE 27
#define FIRSTCOLOR 30

#define PARAM 1
#define IC 2

#define MAXFP 400
#define NAR_IC 50

constexpr int OnTheFly=1;

struct ARRAY_IC {
  int index0,type;
  std::string formula;
  int n;
  std::string var;
  int j1,j2;
};
static int ar_ic_defined=0;
static ARRAY_IC ar_ic[NAR_IC];
namespace {
/* the fixed points the Monte Carlo search found: each one's values and
   its eigenvalues' real and imaginary parts, NODE of each */
struct {
  int n,flag;
  std::array<std::vector<double>, MAXFP> x, er, em;
} fixptlist;
}

typedef struct 
{
  int n;
  double tol;
  double xlo[MAXODE],xhi[MAXODE];
} FIXPTGUESS;

static FIXPTGUESS fixptguess;


static int STOP_FLAG=0;
 struct {
         std::string item;
   int steps,shoot,col,movie,mc;
	 double plow,phigh;
       } eq_range;

void save_batch_shoot();


void init_ar_ic()
{
  int i;
  for(i=0;i<NAR_IC;i++){
    ar_ic[i].index0=-1;
    ar_ic[i].formula.clear();
    ar_ic[i].n=0;
    ar_ic[i].var.clear();
    ar_ic[i].type=0;
  }
}
    
void dump_range(FILE *fp, int f)
{
  xpp::Session &s=xpp::session();
  if(f==READEM){
    auto reader=xpp::LineReader::attach(fp);
    if(!reader.next())return;
  }
  else
    xpp::print(fp,"# Range information\n");
  io_string(eq_range.item,fp,f);
  io_int(&eq_range.col,fp,f,"eq-range stab col");
  io_int(&eq_range.shoot,fp,f,"shoot flag 1=on");
  io_int(&eq_range.steps,fp,f,"eq-range steps");
  io_double(&eq_range.plow,fp,f,"eq_range low");
  io_double(&eq_range.phigh,fp,f,"eq_range high");
  io_string(s.integrator.range.item,fp,f);
  io_string(s.integrator.range.item2,fp,f);
  io_int(&s.integrator.range.steps,fp,f,"Range steps");
  io_int(&s.integrator.range.cycle,fp,f,"Cycle color 1=on");
  io_int(&s.integrator.range.reset,fp,f,"Reset data 1=on");
  io_int(&s.integrator.range.oldic,fp,f,"Use old I.C.s 1=yes");
  io_double(&s.integrator.range.plow,fp,f,"Par1 low");
  io_double(&s.integrator.range.plow2,fp,f,"Par2 low");
  io_double(&s.integrator.range.phigh,fp,f,"Par1 high");
  io_double(&s.integrator.range.phigh2,fp,f,"Par2 high");
  dump_shoot_range(fp,f);
  if(f==READEM)s.integrator.range.steps2=s.integrator.range.steps;
}
void init_range()
{
 xpp::Session &s=xpp::session();
 eq_range.col=-1;
 eq_range.mc=0;
 eq_range.shoot=0;
 eq_range.steps=10;
 eq_range.plow=0.0;
 eq_range.phigh=1.0;
 eq_range.movie=0;
 eq_range.item=xpp::model().upar_names[0];
 s.integrator.range.type=0;
 s.integrator.range.rtype=0;
 s.integrator.range.index=s.integrator.range.index2=0;
 if (s.not_already_set.RANGESTEP)
 {
 	s.integrator.range.steps=20;
	s.not_already_set.RANGESTEP=0;
 }
 s.integrator.range.steps2=20;
 if (s.not_already_set.RANGELOW)
 {
 	s.integrator.range.plow=s.integrator.range.plow2=0.0;
	s.not_already_set.RANGELOW=0;
 }
 
 if (s.not_already_set.RANGEHIGH)
 {
 	s.integrator.range.phigh=s.integrator.range.phigh2=1.0;
 	s.not_already_set.RANGEHIGH=0;
 }
 if (s.not_already_set.RANGERESET)
 {
 	s.integrator.range.reset=1;
 	s.not_already_set.RANGERESET=0;
 }
 if (s.not_already_set.RANGEOLDIC)
 {
 	s.integrator.range.oldic=1;
 	s.not_already_set.RANGEOLDIC=0;
 }
 s.integrator.range.cycle=0;
 s.integrator.range.movie=0;
 if (s.not_already_set.RANGEOVER)
 {
 	s.integrator.range.item=xpp::model().uvar_names[0];
	s.not_already_set.RANGEOVER=0;
 }
 s.integrator.range.item2=xpp::model().uvar_names[0];
 init_shoot_range(xpp::model().upar_names[0]); 
 init_monte_carlo();
}

int set_up_eq_range()
{
static const char *n[]={"*2Range over","Steps","Start","End",
		     "Shoot (Y/N)",
		  "Stability col","Movie (Y/N)","Monte Carlo (Y/N)"};
 std::array<std::string, 8> values;
 int status,i;
 static  const char *yn[]={"N","Y"};
 values[0] = eq_range.item;
 values[1] = xpp::format("{}", eq_range.steps);
 values[2] = xpp::format("{:.16g}", eq_range.plow);
 values[3] = xpp::format("{:.16g}", eq_range.phigh);
 values[4] = yn[eq_range.shoot];
 values[5] = xpp::format("{}", eq_range.col);
 values[6] = yn[eq_range.movie];
values[7] = yn[eq_range.mc];

 static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_TEXT,XPP_FIELD_INTEGER,XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(8,1,"Range Equilibria",n,values,kinds);
 if(status!=0){
   eq_range.item=values[0];
   i=find_user_name(PARAM,eq_range.item);
   if(i<0){
        err_msg("No such parameter");
       return(0);
     }
   
   eq_range.steps=atoi(values[1].c_str());
   if(eq_range.steps<=0)eq_range.steps=10;
   eq_range.plow=atof(values[2].c_str());
   eq_range.phigh=atof(values[3].c_str());
   if(values[4][0]=='Y'||values[4][0]=='y')eq_range.shoot=1;
   else eq_range.shoot=0;
   if(values[6][0]=='Y'||values[6][0]=='y')eq_range.movie=1;
   else eq_range.movie=0;
    if(values[7][0]=='Y'||values[6][0]=='y')eq_range.mc=1;
   else eq_range.mc=0;
   eq_range.col=atoi(values[5].c_str());
   if(eq_range.col<=1||eq_range.col>(xpp::model().neq+1))eq_range.col=-1;
 
 return(1);
 }
 return(0);
}

void cont_integ()
{
  xpp::Session &s=xpp::session();
  double tetemp;
  double *x;
  double dif;
  if(s.numerics.inflag==0||s.numerics.fft!=0||s.numerics.hist!=0)return;
  tetemp=s.numerics.tend;
  wipe_rep();
  data_back();
  if(new_float("Continue until:",&tetemp)==-1)return;
  x=&s.data_store.current[0];
  tetemp=fabs(tetemp);
  if(fabs(s.data_store.current_time)>=tetemp)return;
  dif=tetemp-fabs(s.data_store.current_time);
  s.integrator.my_start=1;  /*  I know it is wasteful to restart, but lets be safe.... */
  integrate(&s.data_store.current_time,x,dif,s.numerics.delta_t,1,s.numerics.njmp,&s.integrator.my_start);
  ping();
  refresh_browser(s.data_store.rows);
}

namespace {
/* what a range varies: item a parameter (PARAM) or else a variable (IC),
   and its index; 0 (and a message) when it is neither */
int find_range_item(const std::string &item, int *type, int *index)
{
 int i=find_user_name(PARAM,item);
 if(i>-1){
   *type=PARAM;
   *index=i;
   return 1;
 }
 i=find_user_name(IC,item);
 if(i<=-1){
   err_msg(xpp::format(" {} is not a parameter or variable !",item).c_str());
   return(0);
 }
 *type=IC;
 *index=i;
 return 1;
}
}

int range_item()
{
 return find_range_item(xpp::session().integrator.range.item,&xpp::session().integrator.range.type,&xpp::session().integrator.range.index);
}

int range_item2()
{
 return find_range_item(xpp::session().integrator.range.item2,&xpp::session().integrator.range.type2,&xpp::session().integrator.range.index2);
}

int set_up_range()
{
 xpp::Session &s=xpp::session();
 static const char *n[]={"*3Range over","Steps","Start","End",
		     "Reset storage (Y/N)",
		     "Use old ic's (Y/N)","Cycle color (Y/N)","Movie(Y/N)"};
 std::array<std::string, 8> values;
 int status;
 static  const char *yn[]={"N","Y"};
 if(!program.interactive){ /* no dialog: the range the options set */
   if(range_item()==0)return 0;
   s.integrator.range_flag=1;
   return 1;
 }

 values[0] = s.integrator.range.item;
 values[1] = xpp::format("{}", s.integrator.range.steps);
 values[2] = xpp::format("{:.16g}", s.integrator.range.plow);
 values[3] = xpp::format("{:.16g}", s.integrator.range.phigh);
 values[4] = yn[s.integrator.range.reset];
 values[5] = yn[s.integrator.range.oldic];
 values[6] = yn[s.integrator.range.cycle];
 values[7] = yn[s.integrator.range.movie];
 
 static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(8,1,"Range Integrate",n,values,kinds);
 if(status!=0){
   s.integrator.range.item=values[0];
   if(range_item()==0)return 0;
   s.integrator.range.steps=atoi(values[1].c_str());
   if(s.integrator.range.steps<=0)s.integrator.range.steps=10;
   s.integrator.range.plow=atof(values[2].c_str());
   s.integrator.range.phigh=atof(values[3].c_str());
   if(values[4][0]=='Y'||values[4][0]=='y')s.integrator.range.reset=1;
   else s.integrator.range.reset=0;
   if(values[5][0]=='Y'||values[5][0]=='y')s.integrator.range.oldic=1;
   else s.integrator.range.oldic=0;
    if(values[6][0]=='Y'||values[6][0]=='y')s.integrator.range.cycle=1;
   else s.integrator.range.cycle=0;
    if(values[7][0]=='Y'||values[7][0]=='y')s.integrator.range.movie=1;
   else s.integrator.range.movie=0;
 s.integrator.range_flag=1;
 return(1);
 }
 return(0);
}

int set_up_range2()
{
 xpp::Session &s=xpp::session();
 static const char *n[]={"*3Vary1","Start1","End1",
                   "*3Vary2","Start2","End2","Steps",
		     "Reset storage (Y/N)",
		     "Use old ic's (Y/N)","Cycle color (Y/N)","Movie(Y/N)",
                      "Crv(1) Array(2)","Steps2"};
 std::array<std::string, 13> values;
 int status;
 static  const char *yn[]={"N","Y"};
 if(!program.interactive){
   return(range_item());
 }
 values[0] = s.integrator.range.item;
  values[1] = xpp::format("{:.16g}", s.integrator.range.plow);
 values[2] = xpp::format("{:.16g}", s.integrator.range.phigh);
 values[3] = s.integrator.range.item2;
  values[4] = xpp::format("{:.16g}", s.integrator.range.plow2);
 values[5] = xpp::format("{:.16g}", s.integrator.range.phigh2);
values[6] = xpp::format("{}", s.integrator.range.steps);
 values[7] = yn[s.integrator.range.reset];
 values[8] = yn[s.integrator.range.oldic];
 values[9] = yn[s.integrator.range.cycle];
 values[10] = yn[s.integrator.range.movie];
 if(s.integrator.range.rtype==2)
  values[11] = "2";
 else
   values[11] = "1";
 values[12] = xpp::format("{}", s.integrator.range.steps2);
 static const int kinds[]={XPP_FIELD_NAME_IN(3),XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_NAME_IN(3),XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,
                           XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,
                           XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
 status=do_string_box_of(7,2,"Double Range Integrate",n,values,kinds);
 if(status!=0){
   s.integrator.range.item=values[0];
   
   if(range_item()==0)return 0;
    s.integrator.range.item2=values[3];
   
   if(range_item2()==0)return 0;
   s.integrator.range.steps=atoi(values[6].c_str());
      s.integrator.range.steps2=atoi(values[12].c_str());
   if(s.integrator.range.steps<=0)s.integrator.range.steps=10;
    if(s.integrator.range.steps2<=0)s.integrator.range.steps2=10;
  
   s.integrator.range.plow=atof(values[1].c_str());
   s.integrator.range.phigh=atof(values[2].c_str());
    s.integrator.range.plow2=atof(values[4].c_str());
   s.integrator.range.phigh2=atof(values[5].c_str());
   if(values[7][0]=='Y'||values[7][0]=='y')s.integrator.range.reset=1;
   else s.integrator.range.reset=0;
   if(values[8][0]=='Y'||values[8][0]=='y')s.integrator.range.oldic=1;
   else s.integrator.range.oldic=0;
    if(values[9][0]=='Y'||values[9][0]=='y')s.integrator.range.cycle=1;
   else s.integrator.range.cycle=0;
    if(values[10][0]=='Y'||values[10][0]=='y')s.integrator.range.movie=1;
   else s.integrator.range.movie=0;
   s.integrator.range.rtype=atoi(values[11].c_str());

 s.integrator.range_flag=1;
 return(1);
 }
 return(0);
}

void init_monte_carlo()
{
  int i;
  fixptguess.tol=.001;
  fixptguess.n=100;
  for(i=0;i<xpp::model().node;i++){
    fixptguess.xlo[i]=-10;
    fixptguess.xhi[i]=10;
  }
  fixptlist.flag=0;
  fixptlist.n=0;
}

void monte_carlo()
{
  int append=0;
  int i=0,done=0,ishoot=0;
  double z;
  new_int("Append(1/0",&append);
  new_int("Shoot (1/0)",&ishoot);
  new_int("# Guesses:",&fixptguess.n);
  new_float("Tolerance:",&fixptguess.tol);
  while(1){
    z=fixptguess.xlo[i];
    done=new_float(xpp::format("{}_lo :",xpp::model().uvar_names[i]).c_str(),&z);
    if(done==0)
      fixptguess.xlo[i]=z;
    if(done==-1)break;
    z=fixptguess.xhi[i];
    done=new_float(xpp::format("{}_hi :",xpp::model().uvar_names[i]).c_str(),&z);
    if(done==0)
      fixptguess.xhi[i]=z;
    if(done==-1)break;
    i++;
    if(i>=xpp::model().node)
      break;
  }
  do_monte_carlo_search(append, 1,ishoot);
}

void do_monte_carlo_search(int append, int stuffbrowse,int ishoot)
{
  xpp::Session &s=xpp::session();
  int i,j,k,m,n=fixptguess.n;
  int ierr,is_new=1;
  double x[MAXODE],sum;
  double er[MAXODE],em[MAXODE];
  if(append==0)
    fixptlist.n=0;

  if(fixptlist.flag==0){
    for(i=0;i<MAXFP;i++){
      fixptlist.x[i].assign(xpp::model().node,0.0);
      fixptlist.er[i].assign(xpp::model().node,0.0);
      fixptlist.em[i].assign(xpp::model().node,0.0);
    }
    fixptlist.flag=1;
  }
  for(i=0;i<n;i++){
    for(j=0;j<xpp::model().node;j++){ 
      x[j]=ndrand48()*(fixptguess.xhi[j]-fixptguess.xlo[j])+fixptguess.xlo[j];
    }
    do_sing_info(x,s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,xpp::model().node,er,em,&ierr);
    if(ierr==0){
      m=fixptlist.n;
      if(m==0){ /* first fixed point found */
	fixptlist.n=1;
	xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	for(j=0;j<xpp::model().node;j++){
	  fixptlist.x[0][j]=x[j];
	  fixptlist.er[0][j]=er[j];
	  fixptlist.em[0][j]=em[j];
          if(ishoot)shoot_this_now();
	  xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	}
      }
      else { /* there are others  better compare them */
	is_new=1;
	for(k=0;k<m;k++){
	  sum=0.0;
	  for(j=0;j<xpp::model().node;j++)
	    sum+=fabs(x[j]-fixptlist.x[k][j]);
	  if(sum<fixptguess.tol)
	    is_new=0;
	}
	if(is_new==1){
	  m=fixptlist.n;
	  fixptlist.n++;
	  if(m<MAXFP){
	    xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	    for(j=0;j<xpp::model().node;j++){
	      fixptlist.x[m][j]=x[j];
	      fixptlist.er[m][j]=er[j];
	      fixptlist.em[m][j]=em[j];
	      if(ishoot)shoot_this_now();
	      xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	    }
	  }
	}
      }
    }
  }
  if(stuffbrowse) {
    reset_browser();
    s.data_store.rows=0;
    m=fixptlist.n;
    for(i=0;i<m;i++){
      s.data_store.col[0][s.data_store.rows]=static_cast<float>(i);
      for(j=0;j<xpp::model().node;j++)s.data_store.col[j+1][s.data_store.rows]=static_cast<float>(fixptlist.x[i][j]);
      s.data_store.rows++;
    }
    refresh_browser(s.data_store.rows);
  }
}

void do_eq_range(double *x)
{
 xpp::Session &s=xpp::session();
 double parlo,parhi,dpar,temp;
 int npar,stabcol,i,j,ierr;
 int mc;
 float stabinfo;

 if(set_up_eq_range()==0)return;

 wipe_rep();
 data_back();
 parlo=eq_range.plow;
 parhi=eq_range.phigh;
 
 npar=eq_range.steps;
 dpar=(parhi-parlo)/static_cast<double>(npar);
 stabcol=eq_range.col;
 mc=eq_range.mc;
 s.data_store.rows=0;
 s.integrator.delay_err=0;
 s.numerics.endsing=0;
 s.numerics.par_fol=1;
 s.numerics.pauser=0;
 s.numerics.shoot=eq_range.shoot;
 reset_browser();
 if(mc==1){
   eq_range.movie=1;
   s.numerics.shoot=0;
 }
 if(eq_range.movie)reset_film();
 for(i=0;i<=npar;i++)
   {
     if(eq_range.movie)
       clear_draw_window();
      temp=parlo+dpar*static_cast<double>(i);
      set_val(eq_range.item,temp);
      s.numerics.par_fol=1;
      {
        std::string bob=xpp::format("{}={:.16g}",eq_range.item,temp);
        bottom_msg(2,bob.c_str());
        evaluate_derived();
        /*  I think  */ redo_all_fun_tables();
        if(mc) {
	  do_monte_carlo_search(0,0,1);
        }
        else {
        if(s.delay.flag)
	  do_delay_sing(x,s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,
		        xpp::model().node,&ierr,&stabinfo);
        else do_sing(x,s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,
		     xpp::model().node,&ierr,&stabinfo);
        }
        if(eq_range.movie){
	  draw_label(s.plot_windows.draw_win);
          xpp_ui.put_text(5,10,bob.c_str());
	  if(xpp_ui.film_clip()==0)err_msg("Out of film");
        }
      }
      if(mc==0){
      s.data_store.col[0][s.data_store.rows]=temp;
      for(j=0;j<xpp::model().node;j++)s.data_store.col[j+1][s.data_store.rows]=static_cast<float>(x[j]);
      for(j=xpp::model().node;j<xpp::model().node+xpp::model().nmarkov;j++)s.data_store.col[j+1][s.data_store.rows]=0.0;
      if(stabcol>0)s.data_store.col[stabcol-1][s.data_store.rows]=stabinfo;

      s.data_store.rows++;
      row_stored();}
      if(s.numerics.endsing==1)break;
    }
    refresh_browser(s.data_store.rows);
 s.numerics.par_fol=0;
}

void swap_color(int *col, int rorw)
{
 if(rorw)xpp::session().plot_windows.current->color[0]=*col;
 else *col=xpp::session().plot_windows.current->color[0];
}

void set_cycle(int flag, int *icol)
{
 if(flag==0)return;
 xpp::session().plot_windows.current->color[0]=*icol+1;
 *icol=*icol+1;
  if(*icol==10)*icol=0;
} 

int do_auto_range_go()
{
  double *x;
  x=&xpp::session().data_store.current[0];
  return(do_range(x,2));
}

namespace {

/* Seeds the random generator for a run about to start (Go, a do_range
   sweep, usual_integrate_stuff's plain run): rand_seed is the seed shown
   or set for the next run (docs/roadmap.md W71, "@ seed=" in
   load_eqn.cpp, Stochastic > New seed in markov.cpp, -newseed in
   expr_symbols.cpp's init_rpn -- each of those already calls nsrand48
   with it immediately too, unchanged, so this reapplies exactly the
   same value and changes nothing there); apply it, log it and keep it
   as last_seed for the protocol's state and a saved data file's
   header/metadata, then draw a fresh rand_seed from a seed stream of
   its own (xpp_next_seed) so an untouched field still gives fresh
   noise next time (a first run's noise and every example md5 stay
   exactly what they were). */
void seed_this_run()
{
  xpp::Session &s=xpp::session();
  const int seed=s.numerics.rand_seed;
  nsrand48(seed);
  s.numerics.last_seed=seed;
  xpp::log(XPP_LOG_INFO,"Go: seed {}\n",seed);
  s.numerics.rand_seed=xpp_next_seed(seed);
}

} // namespace

int do_range(double *x, int flag)  /* 0 for 1-param 1 for 2 parameter 2 for Auto range */
{
  xpp::Session &s=xpp::session();

  std::string bob;
  std::string parn; /* auto_get_info writes the parameter's name */
 int ivar=0,ivar2=0,res=0,oldic=0;
 int nit=20,i=0,j=0,itype=0,itype2=0,cycle=0,icol=0,nit2=0,iii=0;
 int color=s.plot_windows.current->color[0];
 double t,dpar,plow=0.0,phigh=1.0,p=0.0,plow2=0.0,phigh2=0.0,p2=0.0,dpar2=0.0;
 double temp,temp2;
 int ierr=0;
 if(flag==0||flag==2){ 
        s.integrator.range.rtype=0;
        if(set_up_range()==0)return(-1);
 }
 if(flag==1){

   if(set_up_range2()==0)return -1;
 }
 xpp::Computation computing; /* the whole range, between its integrations too (xpp_job.h) */

 seed_this_run(); /* the whole sweep (Stochastic > Compute's many runs
                      included) is one computation, one seed */
 s.integrator.my_start=1;
 itype=s.integrator.range.type;
 ivar=s.integrator.range.index;
 
 res=s.integrator.range.reset;
 oldic=s.integrator.range.oldic;
 nit=s.integrator.range.steps;
 plow=s.integrator.range.plow;
 phigh=s.integrator.range.phigh;
 
 cycle=s.integrator.range.cycle;
 dpar=(phigh-plow)/static_cast<double>(nit);

 get_ic(2,x);
 s.data_store.rows=0;
 s.numerics.storflag=1;
 s.numerics.pauser=0;
nit2=0;
if(s.integrator.range.rtype==2)nit2=s.integrator.range.steps2; 
if(s.integrator.range.type==PARAM)get_val(s.integrator.range.item,&temp);
 alloc_liap(nit); /* make space */
 if(s.integrator.range.rtype>0){
 itype2=s.integrator.range.type2;
 ivar2=s.integrator.range.index2;
 plow2=s.integrator.range.plow2;
 phigh2=s.integrator.range.phigh2;
  if(s.integrator.range.rtype==2)dpar2=(phigh2-plow2)/static_cast<double>(nit2);
  else dpar2=(phigh2-plow2)/static_cast<double>(nit);
  if(s.integrator.range.type2==PARAM)get_val(s.integrator.range.item2,&temp2);
 
 }

 if(s.integrator.range.movie)reset_film();
 if(flag==2){
   auto_get_info(&nit,parn);
   nit2=0;
 }
 for(j=0;j<=nit2;j++){
 for(i=0;i<=nit;i++)
  {
    if(s.integrator.range.movie)clear_draw_window();
   if(cycle)s.plot_windows.current->color[0]=icol+1;
   icol++;
   if(icol==10)icol=0;
   t=s.numerics.t0;
   s.integrator.my_start=1;
   s.numerics.poiext=0;

   if(flag!=2){
     p=plow+dpar*static_cast<double>(i);
     if(s.integrator.range.rtype==1)
       p2=plow2+dpar2*static_cast<double>(i);
     if(s.integrator.range.rtype==2)
       p2=plow2+dpar2*static_cast<double>(j);
   
     if(oldic==1){
       get_ic(1,x);
       
       if(s.delay.flag){
	 /* restart initial data */
	 if(do_init_delay(s.numerics.delay)==0)break;
       }
     }

     if(itype==IC)x[ivar]=p;
     else {
       set_val(s.integrator.range.item,p);
       redo_all_fun_tables(); 
       re_evaluate_kernels();
       
     }
     if(s.integrator.range.rtype>0){
       if(itype2==IC)x[ivar2]=p2;
       else {
	 set_val(s.integrator.range.item2,p2);
	 redo_all_fun_tables();
	 re_evaluate_kernels();
       }
     }
     if(program.interactive){   
       if(s.integrator.range.rtype>0)
	 bob=xpp::format("{}={:.16g}  {}={:.16g}",s.integrator.range.item,p,s.integrator.range.item2,p2);
       else
	 bob=xpp::format("{}={:.16g}  i={}",s.integrator.range.item,p,i);
       bottom_msg(2,bob.c_str());
     }
   }  /* normal range stuff   */ 
   else {  /* auto range stuff */
     auto_set_mark(i);
     get_ic(2,x);
     get_val(parn,&temp);
     bob=xpp::format("{:.230}={:.16g}",parn,temp);
     bottom_msg(2,bob.c_str());
   }
   do_start_flags(x,&s.data_store.current_time);
if(fabs(s.data_store.current_time)>=s.numerics.trans&&s.numerics.storflag==1&&s.numerics.poimap==0)
  {
    s.data_store.col[0][s.data_store.rows]=static_cast<float>(s.data_store.current_time);
    extra(x,s.data_store.current_time,xpp::model().node,xpp::model().neq);
    for(iii=0;iii<xpp::model().neq;iii++)s.data_store.col[1+iii][s.data_store.rows]=static_cast<float>(x[iii]);
    s.data_store.rows++;
  }

 if(integrate(&t,x,s.numerics.tend,s.numerics.delta_t,1,s.numerics.njmp,&s.integrator.my_start)==1){
   ierr=-1;
   break;
 }
 if(s.stochastic.flag)
   append_stoch(i,s.data_store.rows);

 if(s.integrator.range.movie){
   xpp_ui.put_text(5,10,bob.c_str());
   redraw_dfield();
	create_new_cline();
   draw_label(s.plot_windows.draw_win);
   if(xpp_ui.film_clip()==0){err_msg("Out of film");break;}
 }
 refresh_browser(s.data_store.rows);
 if(s.integrator.adj_range==1){
   bob=xpp::format("{}_{:g}",s.integrator.range.item,p);
   data_get_mybrowser(s.data_store.rows-1);
   compute_one_period(static_cast<double>(s.data_store.col[0][s.data_store.rows-1]),s.last_ic.data(),bob.c_str());
 }

 do_this_liaprun(i,p);  /* sends parameter and index back */
 if(s.data_store.rows>2)auto_freeze_it();
 if(s.array_plot.range==1)
   draw_one_array_plot(bob.c_str());
 
 if(res==1||s.stochastic.flag)
   {
     if(batch_options.range==1){
       post_process_stuff();
       write_this_run(batch_options.out_file.c_str(),i);
     }
     s.data_store.rows=0;
   }
  }
 }
 if(s.array_plot.range==1){
   s.array_plot.range=0;
   close_aplot_files();
 }
 if(oldic==1)get_ic(1,x);
 else get_ic(0,x);
 if(s.integrator.range.type==PARAM)set_val(s.integrator.range.item,temp);
 if(s.integrator.range.rtype>0)
   if(s.integrator.range.type2==PARAM)set_val(s.integrator.range.item2,temp2);
 evaluate_derived();
s.plot_windows.current->color[0]=color;
 s.numerics.inflag=1;
 
 ping();
 s.integrator.adj_range=0;
 if(s.stochastic.flag)
   do_stats(ierr);

 return(ierr);

}

void write_equilibrium(const char *name, int shoot)
{
  xpp::Session &s=xpp::session();
  const int n=xpp::model().node;
  std::array<double,MAXODE> x,er,em;
  int ierr;
  for(int i=0;i<n;i++)
    x[i]=s.last_ic[i];

  do_sing_info(x.data(),s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,n,er.data(),em.data(),&ierr);
  if(ierr!=0)return;
  xpp::Writer w(name);
  if(w){
    for(int i=0;i<n;i++)
      w.print("{:g} {:g} {:g}\n",x[i],er[i],em[i]);
    w.commit();
  }
  if(shoot)
    save_batch_shoot();
}
  
void find_equilib_com(int com)
{
 xpp::Session &s=xpp::session();
 int ierr;
 float xm,ym;
 int im,jm;
 int iv,jv;
 float stabinfo;
 double *x,oldtrans;

 x=&s.data_store.current[0];
 if(s.numerics.fft||s.numerics.hist||xpp::model().nkernel>0)return;

 s.numerics.storflag=0;
 s.numerics.poimap=0;
 oldtrans=s.numerics.trans;
 s.numerics.trans=0.0;
 evaluate_derived(); 
 switch(com){
 case 2: 
 do_eq_range(x);
 
   return;
  case 1:
    /*  Get mouse values  */
        iv=s.plot_windows.current->xv[0]-1;
        jv=s.plot_windows.current->yv[0]-1;
    if(iv<0||iv>=xpp::model().node||jv<0||jv>=xpp::model().node||s.plot_windows.current->grtype>=5||jv==iv){
      err_msg("Not in useable 2D plane...");
      return;
    }

	 /* get mouse click x,y  */
         get_ic(1,x);
	 MessageBox("Click on guess");
	 if(GetMouseXY(&im,&jm)){
	   scale_to_real(im,jm,&xm,&ym);
	   x[iv]=static_cast<double>(xm);
	   x[jv]=static_cast<double>(ym);
	 }
         
        KillMessageBox();
	 break;
 case 3: monte_carlo();
         return;
 case 0:
 default:
        get_ic(2,x);
        break;
 }

 if(s.delay.flag){
   do_delay_sing(x,s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,xpp::model().node,&ierr,&stabinfo);
   ping();
 }
 else
    do_sing(x,s.numerics.newt_err,s.numerics.evec_err,s.numerics.bound,s.numerics.evec_iter,xpp::model().node,&ierr,&stabinfo);
 s.numerics.trans=oldtrans;
 
}
 
int write_this_run(const char *file, int i)
{
  if(!xpp::session().integrator.suppress_out){
  std::string outfile=xpp::format("{}.{}",file,i);
  xpp::Writer w(outfile.c_str());
  if(!w){
    xpp::log(XPP_LOG_WARN, "Couldnt open {}\n",outfile.c_str());
    return -1;
  }
  write_mybrowser_data(w);
  w.commit();
  }
   if(xpp::session().integrator.make_plot_flag)dump_ps(i);
  return(1);
}
  
void do_init_data(int com)
{
  xpp::Session &s=xpp::session();
  char ch;
  int i,si;
  double *x;
  double old_dt=s.numerics.delta_t;
  std::string icfile;
  float xm,ym;
  int im,jm,oldstart,iv,jv,badmouse;

  oldstart=s.integrator.my_start;
  s.integrator.my_start=1;
  x=&s.data_store.current[0];
  s.integrator.range_flag=0;
  s.integrator.delay_err=0;
  reset_dae();
  if(s.numerics.fft||s.numerics.hist)return;

  if(com==M_ID){      /* dont want to wipe out everything! */
    get_new_guesses();
    return;
  }

 data_back();
  wipe_rep();
  s.data_store.current_time=s.numerics.t0;
 
  s.numerics.storflag=1;
  s.numerics.poiext=0;
  s.data_store.rows=0;
  reset_browser();

  switch(com){
  case M_IR: /* do range   */
    if(do_range(x,0)!=0&&!program.interactive)
      xpp::log(XPP_LOG_WARN, " Errors occured in range integration \n");
    return;
  case M_I2:
    do_range(x,1);
    return;
  case M_IS:
  case M_IL:
    if(s.numerics.inflag==0){
      ping();
      err_msg("No prior solution");
      return;
    }
    get_ic(0,x);
    if(com==M_IS){
      s.numerics.t0=s.integrator.last_time;
      s.data_store.current_time=s.numerics.t0;
    }
    if(s.numerics.method==xpp::method::VOLTERRA&&oldstart==0){
      ch=static_cast<char>(TwoChoice("No","Yes","Reset integrals?","ny"));
      if(ch=='n')s.integrator.my_start=oldstart;
    }
    break;
  case M_IO:
    get_ic(1,x);
    if(s.delay.flag){
      /* restart initial data */
      if(do_init_delay(s.numerics.delay)==0)return;
    }
   set_init_guess();
    break;
  case M_IM:
  case M_II:
        iv=s.plot_windows.current->xv[0]-1;
        jv=s.plot_windows.current->yv[0]-1;
    if(iv<0||iv>=xpp::model().node||jv<0||jv>=xpp::model().node||s.plot_windows.current->grtype>=5||jv==iv){
      err_msg("Not in useable 2D plane...");
      return;
    }

    /*  Get mouse values  */
    if(com==M_IM){
        get_ic(1,x);
	MessageBox("Click on initial data");
	if(GetMouseXY(&im,&jm)){
	  scale_to_real(im,jm,&xm,&ym);
	  im=s.plot_windows.current->xv[0]-1;
	  jm=s.plot_windows.current->yv[0]-1;
	  x[iv]=static_cast<double>(xm);
	  x[jv]=static_cast<double>(ym);
	  s.last_ic[im]=x[im];
	  s.last_ic[jm]=x[jm];
	  KillMessageBox();
  
	  if(s.delay.flag){
	    /* restart initial data */
	    if(do_init_delay(s.numerics.delay)==0)return;
	  }
	}
	else {
	   KillMessageBox();
	   return;
	}
    }
    else {
      s.integrator.suppress_bounds=1;

	MessageBox("Click on initial data -- ESC to quit");
	while(1){
          get_ic(1,x);
	  badmouse=GetMouseXY(&im,&jm);
	  if(badmouse==0)break;
	  scale_to_real(im,jm,&xm,&ym);
	  im=s.plot_windows.current->xv[0]-1;
	  jm=s.plot_windows.current->yv[0]-1;
	  x[iv]=static_cast<double>(xm);
	  x[jv]=static_cast<double>(ym);
	  s.last_ic[im]=x[im];
	  s.last_ic[jm]=x[jm];
	  if(s.delay.flag){
	    /* restart initial data */
	    if(do_init_delay(s.numerics.delay)==0)break;
	  }
          s.integrator.my_start=1;
          s.data_store.current_time=s.numerics.t0;
	  usual_integrate_stuff(x);
	}
	KillMessageBox();
	s.integrator.suppress_bounds=0;
	return;
    }
    break;
  case M_IN:
    man_ic(); 
    get_ic(2,x);
    set_init_guess();
    break; 
  case M_IU:
    if(form_ic()==0)return;
    get_ic(2,x);
    break;
  case M_IH:
    if(s.manifolds.ic_flag==0){
      err_msg("No shooting data available");
      break;
    }
    si=1;
    new_int(xpp::format("Which? (1-{})",s.manifolds.count).c_str(),&si);
    si--;
    if(si<s.manifolds.count&&si>=0){
      for(i=0;i<xpp::model().node;i++)
	s.last_ic[i]=s.manifolds.ic[si][i];
      get_ic(2,x);
    }
    else
      err_msg("Out of range");
    break;
  case M_IF:
    icfile.clear();
    if(!file_selector("Read initial data",icfile,"*.dat"))return;
    {
      xpp::TokenReader reader(icfile.c_str());
      if(!reader){
        err_msg(" Cant open IC file");
        return;
      }
      for(i=0;i<xpp::model().node;i++)
        if(!reader.read(s.last_ic[i])){
          err_msg(" IC file too short");
          break;
        }
    }
    get_ic(2,x);
    break;
      
  case M_IB:
    s.numerics.delta_t=-fabs(s.numerics.delta_t);
      get_ic(2,x);
      set_init_guess();
      if(s.delay.flag){
      /* restart initial data */
      if(do_init_delay(s.numerics.delay)==0)return;
    }
      break;
  case M_IG:
  default:
  	
    set_init_guess();
    
    get_ic(2,x); 
    
    if(s.delay.flag){
      /* restart initial data */
      if(do_init_delay(s.numerics.delay)==0)return;
    }
    break;
  }
if(usual_integrate_stuff(x)!=0&&!program.interactive)
  xpp::log(XPP_LOG_WARN, " Integration not completed -- will write anyway...\n");
s.numerics.delta_t=old_dt;
}	
void run_now()
{
 
  double *x;
 xpp::session().integrator.my_start=1;
 x=&xpp::session().data_store.current[0];
 xpp::session().integrator.range_flag=0;
 xpp::session().integrator.delay_err=0;
 reset_dae();
 xpp::session().data_store.current_time=xpp::session().numerics.t0;
 get_ic(2,x); 
  xpp::session().numerics.storflag=1;
  xpp::session().numerics.poiext=0;
  xpp::session().data_store.rows=0;
  reset_browser();
  usual_integrate_stuff(x);
 }

void do_start_flags(double *x,double *t)
{
 int iflagstart=1;
 double tnew=*t;
 double sss;
 one_flag_step(x,x,&iflagstart,*t,&tnew,xpp::model().node,&sss);

}
int usual_integrate_stuff(double *x)
{
  xpp::Session &s=xpp::session();
  int i;

  seed_this_run();
  do_start_flags(x,&s.data_store.current_time);
   if(fabs(s.data_store.current_time)>=s.numerics.trans&&s.numerics.storflag==1&&s.numerics.poimap==0)
    {
      s.data_store.col[0][0]=static_cast<float>(s.data_store.current_time);
      extra(x,s.data_store.current_time,xpp::model().node,xpp::model().neq);
      for(i=0;i<xpp::model().neq;i++)s.data_store.col[1+i][0]=static_cast<float>(x[i]);
      s.data_store.rows=1;
    }
 
  xpp_job_begin(0); /* Abort cancels it (xpp_job.h) */
  const int failed=integrate(&s.data_store.current_time,x,s.numerics.tend,s.numerics.delta_t,1,s.numerics.njmp,&s.integrator.my_start);
  xpp_job_end();
  
  ping();
  s.numerics.inflag=1;
  refresh_browser(s.data_store.rows);
  if(program.interactive){
 auto_freeze_it();
  redraw_ics();
  }
  return failed;
}
/*  form_ic  --  u_i(0) = F(i)  where  "i" is represented by "t"
    or  
    u[5..20]=f([j]) 
*/

namespace {
/* the array IC newic[j1..j2]: the one already used for it, else the first
   free one (the first slot when none is free) made into it */
ARRAY_IC &array_ic_for(const char *newic, int j1, int j2)
{
  int ihot=-1;
  int ifree=-1;
  for(int i=0;i<NAR_IC;i++){
    if(ar_ic[i].index0==-1&&ifree==-1&&ar_ic[i].type==0)
      ifree=i;
    if(ar_ic[i].var==newic&&ar_ic[i].j1==j1&&ar_ic[i].j2==j2)
      ihot=i;
  }
  if(ihot==-1){
    ihot=ifree==-1?0:ifree;
    ar_ic[ihot].var=newic;
    ar_ic[ihot].type=2;
    ar_ic[ihot].j1=j1;
    ar_ic[ihot].j2=j2;
  }
  return ar_ic[ihot];
}
}

void do_new_array_ic(const char *newic, int j1, int j2)
{
  ARRAY_IC &ic=array_ic_for(newic,j1,j2);
  new_string_of("Formula:",ic.formula,XPP_FIELD_EXPRESSION);
  evaluate_ar_ic(ic.var.c_str(),ic.formula.c_str(),ic.j1,ic.j2);
}

void store_new_array_ic(const char *newic, int j1, int j2, const char *formula)
{
  array_ic_for(newic,j1,j2).formula=formula;
}
std::vector<ArrayInitialValue> array_initial_values()
{
  std::vector<ArrayInitialValue> out;
  if(ar_ic_defined==0)return out;
  int group=0;
  for(const ARRAY_IC &ic : ar_ic){
    if(ic.type!=2)continue;
    group++;
    for(int j=ic.j1;j<=ic.j2;j++){
      ArrayInitialValue v;
      subsk(ic.var.c_str(),v.var,j,1);
      subsk(ic.formula.c_str(),v.formula,j,1);
      v.j=j;
      v.group=group;
      out.push_back(std::move(v));
    }
  }
  return out;
}

void evaluate_ar_ic(const char *v, const char *f, int j1, int j2)
{
  int j;
  int i,flag;
  double z;
  std::string vp, fp;
  for(j=j1;j<=j2;j++){
    i=-1;
    subsk(v,vp,j,1);
    find_variable(vp.c_str(),&i);
    if(i>0){
      subsk(f,fp,j,1);
      flag=do_calc(fp.c_str(),&z);
      if(flag!=-1)
	xpp::session().last_ic[i-1]=z;
      else 
	return;
    }
  }

}
int extract_ic_data(char *big)
{
  int j1,j2,flag2;
  de_space(big);
  /* u[j1..j2](0)=formula: the front up to "(", the formula 4 on from it */
  const std::string_view line=big;
  const size_t open=line.find('(');
  if(open==std::string_view::npos)return(-1);
  std::string front(line.substr(0,open));
  const std::string back(open+4<line.size()?line.substr(open+4):std::string_view());

  /* now fix it up */
  big[0]='#';
  big[1]=' ';
  std::string newic;
  search_array(front.data(),newic,&j1,&j2,&flag2);
  if(flag2==1){
    store_new_array_ic(newic.c_str(),j1,j2,back.c_str());
    ar_ic_defined=1;
  }
  return(1);

}
  
void arr_ic_start()
{
  int i;
  if(ar_ic_defined==0) return;
  for(i=0;i<NAR_IC;i++){
    if(ar_ic[i].type==2){
      evaluate_ar_ic(ar_ic[i].var.c_str(),ar_ic[i].formula.c_str(),
		     ar_ic[i].j1,ar_ic[i].j2);
    }
  }

}

int set_array_ic()
{
 std::string junk;
 std::string newic;
 int i,index0,myar=-1;
 int i1,in;
 int j1,j2,flag2;
 double z;
 int flag;
 if(new_string("Variable: ",junk)==0)return 0;
 search_array(junk.data(),newic,&j1,&j2,&flag2);
 if(flag2==1)
   {
     do_new_array_ic(newic.c_str(),j1,j2);
   }
 else {
   find_variable(junk.c_str(),&i);
   if(i<=-1)
     return 0;
   index0=i;
   for(i=0;i<NAR_IC;i++){
     if(ar_ic[i].type==2)continue;
     if(index0==ar_ic[i].index0){
       myar=i;
       break;
     }
   }
   if(myar<0){
     for(i=0;i<NAR_IC;i++){
       if(ar_ic[i].type==2)continue;
       if(ar_ic[i].index0==-1){
	 myar=i;
	 break;
       }
     }
   }
   if(myar<0)myar=0;
   
   /* Now we have an element in the array index */
   ar_ic[myar].index0=index0;
   ar_ic[myar].type=0;
   new_int("Number elements:",&ar_ic[myar].n);
   new_string_of("u=F(t-i0):",ar_ic[myar].formula,XPP_FIELD_EXPRESSION);
   i1=index0-1;
   in=i1+ar_ic[myar].n;
   if(i1>xpp::model().node||in>xpp::model().node)return 0; /* out of bounds */
   for(i=i1;i<in;i++){
     set_val("t",static_cast<double>((i-i1)));
     flag=do_calc(ar_ic[myar].formula.c_str(),&z);
     if(flag==-1){
       err_msg("Bad formula");
       return 1;
     }
     xpp::session().last_ic[i]=z;
   }
 }
   return 1;
}

int form_ic()
{
  int ans;
  while(1){
    ans=set_array_ic();
    if(ans==0)break;
  }
  return 1;
}

void get_ic(int it, double *x)
{
  int i;
  switch(it){
  case 0:
    for(i=0;i<xpp::model().node+xpp::model().nmarkov;i++)xpp::session().last_ic[i]=x[i];
    break;
  case 1:
  case 2:
    for(i=0;i<xpp::model().node+xpp::model().nmarkov;i++)x[i]=xpp::session().last_ic[i];
    break;
   }
}

int ode_int(double *y, double *t, int *istart, int ishow)
{
 xpp::Session &s=xpp::session();
 xpp::Solver &solver=*s.integrator.solver;
 int nodes=s.solver_work.xpv.node+s.solver_work.xpv.nvec;
 int nit,nout=s.numerics.njmp;
 double tend=s.numerics.tend;
 double dt=s.numerics.delta_t;
  if(solver.traits().discrete){
 nit=tend;
 dt=dt/fabs(dt);
 }
 else nit=(tend+.1*fabs(dt))/fabs(dt);
 if(ishow==1){

 integrate(t,y,tend,dt,1,nout,istart);

 return(1);
}
 MSWTCH(s.solver_work.xpv.x,y);
 evaluate_derived(); 
 solver.begin(istart);
 xpp::SolverStep step{.y=s.solver_work.xpv.x,.t=t,.neq=nodes,.start=istart};
 if(solver.traits().fixed_step){
   step.dt=dt;
   step.steps=nit;
 }
 else{
   step.tout=*t+tend*dt/fabs(dt);
   step.hguess=&dt;
 }
 xpp::SolverResult r=solver.advance(step);
 MSWTCH(y,s.solver_work.xpv.x);
 if(!r.ok){
   xpp::report_solver_failure(r,s.integrator.range_flag);
   return(0);
 }
 solver.finish();
 return(1);
}

int integrate(double *t, double *x, double tend, double dt, int count, int nout, int *start)
{
  xpp::Session &s=xpp::session();
  xpp::Computation computing; /* what Escape stops (xpp_job.h) */
  xpp::Solver &solver=*s.integrator.solver;

 float xv[MAXODE+1],xvold[MAXODE+1];
 float oldperiod=0.0;
 double xprime[MAXODE],oldxprime[MAXODE],hguess=dt;

 int torcross[MAXODE];
 int nodes=s.solver_work.xpv.node+s.solver_work.xpv.nvec-xpp::model().nmarkov;

 int rval=0;
 double oldx[MAXODE],oldt=0,dint,dxp,sect,sect1,tout,tzero=*t;
 double sss,tnew=*t;
 int iflagstart=1;
 float tscal=tend,tv;
 
 char esc;
 int ieqn,i,pflag=0;
 int icount=0;
 int nit;
 int cwidth=0;
 /* new poincare map stuff */

  int i_nan=0; /* NaN */
MSWTCH(s.solver_work.xpv.x,x);

if(program.interactive) cwidth=get_command_width();

 s.integrator.last_time=*t;
 evaluate_derived();

 solver.begin(start);
 if(solver.traits().discrete){
 nit=tend;
 dt=dt/fabs(dt);
 }
 else nit=(tend+fabs(dt)*.1)/fabs(dt); 
 /* else nit=tend/fabs(dt); */
 nit=(nit+nout-1)/nout;
 if(nit==0)return(rval);
 one_flag_step(s.solver_work.xpv.x,s.solver_work.xpv.x,&iflagstart,*t,&tnew,nodes,&sss);
 MSWTCH(x,s.solver_work.xpv.x);
 extra(x,*t,xpp::model().node,xpp::model().neq); /* Note this takes care of initializing Markov variables */
  MSWTCH(s.solver_work.xpv.x,x);
 xv[0]=static_cast<float>(*t);
 for(ieqn=1;ieqn<=xpp::model().neq;ieqn++)xv[ieqn]=static_cast<float>(x[ieqn-1]);
 if(s.animation.options.on_the_fly)on_the_fly(1); 
   
 if(s.numerics.poimap)
 {
 oldt=*t;
 for(ieqn=0;ieqn<xpp::model().neq;ieqn++)oldx[ieqn]=x[ieqn];
 }
 if(dt<0.0)tscal=-tend;
 if(tscal==0.0)tscal=1.0;
 stor_delay(x);
 /* xppautX: the rows a cancel before the first step finds (xpp_job.h) */
 xpp_job_rows_stored(s.data_store.rows, s.data_store.rows > 0 ? s.data_store.col[0][s.data_store.rows-1] : *t);

 while(1)
 {
	
	   if(!solver.traits().fixed_step){
	     /* on to the next output time, in steps of the method's own */
	     tout=tzero+dt*(icount+1);
	     if(fabs(dt)<fabs(s.numerics.hmin)){
	       s.integrator.last_time=*t;
	       solver.finish();
	       return(1);
	     }
	     MSWTCH(s.solver_work.xpv.x,x);
	     xpp::SolverResult r=solver.advance({.y=s.solver_work.xpv.x,.t=t,.neq=nodes,.start=start,
						 .tout=tout,.hguess=&hguess});
	     MSWTCH(x,s.solver_work.xpv.x);
	     stor_delay(x);
	     if(s.integrator.delay_err){
	       s.integrator.delay_err=0;
	       err_dae();
	       s.integrator.last_time=*t;
	       return(1);
	     }
	     if(!r.ok){
	       xpp::report_solver_failure(r,s.integrator.range_flag||s.integrator.suppress_bounds);
	       s.integrator.last_time=*t;
	       return(1);
	     }
	   }
	   else{
	     /* nout steps of dt */
	     MSWTCH(s.solver_work.xpv.x,x);
	     xpp::SolverResult r=solver.advance({.y=s.solver_work.xpv.x,.t=t,.neq=nodes,.start=start,
						 .dt=dt,.steps=nout});
	     MSWTCH(x,s.solver_work.xpv.x);
	     if(!r.ok){
	       /* a range or a run without bounds checks goes on */
	       bool quiet=s.integrator.range_flag||s.integrator.suppress_bounds;
	       xpp::report_solver_failure(r,quiet);
	       if(!quiet){
		 s.integrator.last_time=*t;
		 return(1);
	       }
	     }
	   }
	   /*   START POST INTEGRATE STUFF */           

	   extra(x,*t,xpp::model().node,xpp::model().neq);

          if (s.numerics.torus == 1) {
	for (ieqn = 0; ieqn < xpp::model().neq; ieqn++) {
	        torcross[ieqn]=0;
		if (s.itor[ieqn] == 1) {
			if (x[ieqn] > s.numerics.tor_period) {
				x[ieqn] -= s.numerics.tor_period;
                                torcross[ieqn]=-1;
			}
			if (x[ieqn] < 0) {
				x[ieqn] += s.numerics.tor_period;
                                torcross[ieqn]=1;
			}
		      }
	      }
      }
	   xvold[0]=xv[0];
           for(ieqn=1;ieqn<(xpp::model().neq+1);ieqn++)
           {
	    xvold[ieqn]=xv[ieqn];
	    xv[ieqn]=static_cast<float>(x[ieqn-1]);
	/* trap NaN using isnan() in math.h 
	   modified the out of bounds message as well
	   print all the variables on the terminal window, haven't decide
	   should I store them or not. 
	   If use with nout=1, can pinpoint the offensive variable(s)
	*/	    
	    if(isnan(x[ieqn-1])!=0)
            {
             std::string error_message=xpp::format(" {} is NaN at t = {} ",
             xpp::model().uvar_names[ieqn-1],*t);
 i_nan=0;
	         xpp::log(XPP_LOG_DEBUG, "variable\tf(t-1)\tf(t) \n");
                for(i_nan=1;i_nan<=ieqn;i_nan++)
		 {
 		 xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",
             		xpp::model().uvar_names[i_nan-1],xvold[i_nan],xv[i_nan]);
		 }
		for(;i_nan<=xpp::model().neq;i_nan++) 
		 {
 		 xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",
             		xpp::model().uvar_names[i_nan-1],xv[i_nan],static_cast<float>(x[i_nan-1]));
		 }	
     	     err_msg(error_message.c_str());
             rval=1;
             break;
             }
       /* end of NaN */     
            if(fabs(x[ieqn-1])>s.numerics.bound)
            {
	     if(s.integrator.range_flag||s.integrator.suppress_bounds)break;
             std::string error_message=xpp::format(" {} out of bounds at t = {} ",
             xpp::model().uvar_names[ieqn-1],*t);
 i_nan=0;
	         xpp::log(XPP_LOG_DEBUG, "variable\tf(t-1)\tf(t) \n");
                for(i_nan=1;i_nan<=ieqn;i_nan++)
		 {
 		 xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",
             		xpp::model().uvar_names[i_nan-1],xvold[i_nan],xv[i_nan]);
		 }
		for(;i_nan<=xpp::model().neq;i_nan++) 
		 {
 		 xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",
             		xpp::model().uvar_names[i_nan-1],xv[i_nan],static_cast<float>(x[i_nan-1]));
		 }	
	     err_msg(error_message.c_str());
             rval=1;

             break;
            }
           }
	                
        /*   This is where the progresser goes   */
	   if(program.interactive){ plot_command(nit,icount,cwidth); 
	   esc=my_abort();

           {
            
             if(esc==ESCAPE) break;
	     if(esc=='/'){rval=1;s.numerics.endsing=1;break;}
	    
           }
	}        
	if(STOP_FLAG==1){STOP_FLAG=0;break;}
           if(s.integrator.delay_err){err_dae();rval=1;s.numerics.endsing=1;s.integrator.delay_err=0;break;}
           if(ieqn<(xpp::model().neq+1))break;
           tv=static_cast<float>(*t);
	   xv[0]=tv;
 if((s.numerics.poimap==2)&&!(s.numerics.poivar==0))
 {
  pflag=0;
  if((oldx[s.numerics.poivar-1]<x[s.numerics.poivar-1])&&!(s.numerics.poiext<0))s.numerics.poiext=1;
  if((oldx[s.numerics.poivar-1]>x[s.numerics.poivar-1])&&!(s.numerics.poiext>0))s.numerics.poiext=-1;
  if(  ( !(oldx[s.numerics.poivar-1]<x[s.numerics.poivar-1]) && (s.numerics.poiext>0) )||
       ( !(oldx[s.numerics.poivar-1]>x[s.numerics.poivar-1]) && (s.numerics.poiext<0) )
    )
  {
     if(s.numerics.poisgn*s.numerics.poiext>=0)
      {
	/*  We will interpolate to get a good local extremum   */
	
	s.integrator.rhs(*t,x,xprime,xpp::model().neq);
	s.integrator.rhs(oldt,oldx,oldxprime,xpp::model().neq);
        dxp=xprime[s.numerics.poivar-1]-oldxprime[s.numerics.poivar-1];
        if(dxp==0.0){
	  err_msg("Cannot zero RHS for max/min - use a variable");
	  return(1); 
	}
	dint=xprime[s.numerics.poivar-1]/dxp;

	tv=(1-dint)**t+dint*oldt;
	xv[0]=tv;
	for(i=1;i<=xpp::model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
	pflag=1;
        
      }
      s.numerics.poiext=-s.numerics.poiext;
   }
  goto poi;
 }

 /*  here is code for a formula type map --  F(X,t)=0 
  */
 if(s.numerics.poimap==4) {

 }
 
 if(s.numerics.poimap==1||s.numerics.poimap==3)
 {
    if(s.numerics.poivar==0)

     {
     sect1=fmod(fabs(oldt),fabs(s.numerics.poipln));
     sect=fmod(fabs(*t),fabs(s.numerics.poipln));
     if(sect<sect1)
     {
     dint=sect/(s.numerics.poipln+sect-sect1);
     i=static_cast<int>((fabs(*t)/fabs(s.numerics.poipln)));
     tv=static_cast<float>(s.numerics.poipln)*i;
     xv[0]=tv;
     for(i=1;i<=xpp::model().neq;i++)xv[i]=static_cast<float>((dint*oldx[i-1]+(1-dint)*x[i-1]));
     pflag=1;
     }
     else pflag=0;
    }

    else

    {
     if(!(s.numerics.poisgn<0))
     {
     if((oldx[s.numerics.poivar-1]<s.numerics.poipln)&&!(x[s.numerics.poivar-1]<s.numerics.poipln))
     {
      dint=(x[s.numerics.poivar-1]-s.numerics.poipln)/(x[s.numerics.poivar-1]-oldx[s.numerics.poivar-1]);
      tv=(1-dint)**t+dint*oldt;
      xv[0]=tv;
      for(i=1;i<=xpp::model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
      pflag=1;
      goto poi;

     }
     else pflag=0;
     }
    if(!(s.numerics.poisgn>0))
     {
       if((oldx[s.numerics.poivar-1]>s.numerics.poipln)&&!(x[s.numerics.poivar-1]>s.numerics.poipln))
       {
        dint=(x[s.numerics.poivar-1]-s.numerics.poipln)/(x[s.numerics.poivar-1]-oldx[s.numerics.poivar-1]);
        tv=(1-dint)**t+dint*oldt;
        xv[0]=tv;
        for(i=1;i<=xpp::model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
        pflag=1;
       }
       else pflag=0;
     }
    }
poi:    for(i=0;i<xpp::model().neq;i++)oldx[i]=x[i];
    oldt=*t;
    if(pflag==0)goto out;
 }

/*	   Plotting and storing data      */
 if(s.numerics.poimap==3&&pflag==1){
   if(oldperiod==0.0){
     pflag=0; /* this is the first hit !! */
     oldperiod=*t;
     goto out;
   }
   xv[0]=*t-oldperiod;
   oldperiod=*t;
 }
     
          if(!(fabs(*t)<s.numerics.trans)&&program.interactive&&OnTheFly)
	  {
	     plot_the_graphs(xv,xvold,xpp::model().node,xpp::model().neq,fabs(dt*s.numerics.njmp),torcross,0); 

	  }

	   if((s.numerics.storflag==1)&&(count!=0)&&(s.data_store.rows<s.data_store.max_rows)&&!(fabs(*t)<s.numerics.trans))
	   {
           if(s.animation.options.on_the_fly)on_the_fly(0);
           for(ieqn=0;ieqn<=xpp::model().neq;ieqn++)
		 s.data_store.col[ieqn][s.data_store.rows]=xv[ieqn];
	    s.data_store.rows++;
	    row_stored(); /* xppautX: replay stops here, a front end shows the run grow */
	    if(!(s.data_store.rows<s.data_store.max_rows))
            if(stor_full()==0)break;
	    if((pflag==1)&&(s.numerics.sos==1))break;
	   }

out:
           icount++;
           if(icount>=nit&&count!=0)break;

	   /* END POST INTEGRATE ANALYSIS  */
 }
 
       s.integrator.last_time=*t;
       solver.finish();
       return(rval);
  }
void send_halt(double *y, double t)
{
  STOP_FLAG=1;
}
void send_output(double *y,double t)
{
  double yy[MAXODE];
  int i;
  for(i=0;i<xpp::model().node;i++)
    yy[i]=y[i];
  extra(yy,t,xpp::model().node,xpp::model().neq);
  if((xpp::session().numerics.storflag==1)&&(xpp::session().data_store.rows<xpp::session().data_store.max_rows)){
    
    for(i=0;i<xpp::model().neq;i++)
      xpp::session().data_store.col[i+1][xpp::session().data_store.rows]=static_cast<float>(yy[i]);
    xpp::session().data_store.col[0][xpp::session().data_store.rows]=static_cast<float>(t);
    xpp::session().data_store.rows++;
    row_stored();
  }
}

  void  do_plot(float *oldxpl, float *oldypl, float *oldzpl, float *xpl, float *ypl, float *zpl)
{
	int ip,np=xpp::session().plot_windows.current->nvars;
        
        for(ip=0;ip<np;ip++){
           if(xpp::session().plot_windows.current->ColorFlag==0){

	     set_linestyle(xpp::session().plot_windows.current->color[ip]);
	   }
           if(xpp::session().plot_windows.current->line[ip]<=0)
           {
	    xpp::session().drawing.point_radius=-xpp::session().plot_windows.current->line[ip];
	   if(xpp::session().plot_windows.current->ThreeDFlag==0) point_abs(xpl[ip],ypl[ip]);
	   else point_3d(xpl[ip],ypl[ip],zpl[ip]);
           }
           else
	   {
	    if(xpp::session().plot_windows.current->ThreeDFlag==0){
            
	      line_abs(oldxpl[ip],oldypl[ip],xpl[ip],ypl[ip]);
	    }
            else line_3d(oldxpl[ip],oldypl[ip],oldzpl[ip],
		         xpl[ip],ypl[ip],zpl[ip]);
	   }
       }
}

/*
 old restore is in restore.c

*/

void plot_the_graphs(float *xv,float *xvold,int node,int neq,double ddt,int *tc,int flag)
{
 for_each_shown_window(flag,[&]{plot_one_graph(xv,xvold,node,neq,ddt,tc);});
}

void plot_one_graph(float *xv,float *xvold,int node,int neq,double ddt,int *tc)
{
 int *xvar,*yvar,*zvar;
 int NPlots,ip;
 float oldxpl[MAXPERPLOT],oldypl[MAXPERPLOT],oldzpl[MAXPERPLOT];
 float xpl[MAXPERPLOT],ypl[MAXPERPLOT],zpl[MAXPERPLOT];
 NPlots=xpp::session().plot_windows.current->nvars;
 xvar=xpp::session().plot_windows.current->xv;
 yvar=xpp::session().plot_windows.current->yv;
 zvar=xpp::session().plot_windows.current->zv;
 for(ip=0;ip<xpp::model().neq;ip++){
   if(xpp::session().itor[ip]==1)
     xvold[ip+1]=xvold[ip+1]+tc[ip]*xpp::session().numerics.tor_period;
 }
 for(ip=0;ip<NPlots;ip++){
 oldxpl[ip]=xvold[xvar[ip]];
 oldypl[ip]=xvold[yvar[ip]];
 oldzpl[ip]=xvold[zvar[ip]];
 xpl[ip]=xv[xvar[ip]];
 ypl[ip]=xv[yvar[ip]];
 zpl[ip]=xv[zvar[ip]];
 }
 if(xpp::session().plot_windows.current->ColorFlag)
   comp_color(xv,xvold,xpp::model().node,static_cast<float>(ddt));
 do_plot(oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
 phase_data_flow_step(NPlots,oldxpl,oldypl,xpl,ypl,xpp::session().plot_windows.current->color); /* Dir.field/flow's Flow as data */
}
void restore(int i1, int i2)
{
  xpp::Session &s=xpp::session();
  int ip,np=s.plot_windows.current->nvars;
  int ZSHFT,YSHFT,XSHFT;
  int i,j,kxoff,kyoff,kzoff;
  int iiXPLT,iiYPLT,iiZPLT;
  float oldxpl,oldypl,oldzpl,xpl,ypl,zpl;
  float v1[MAXODE+1],v2[MAXODE+1];
  float **data;

  data=get_browser_data();
  XSHFT=s.plot_windows.current->xshft;
  YSHFT=s.plot_windows.current->yshft;
  ZSHFT=s.plot_windows.current->zshft;
  if(i1<ZSHFT)i1=ZSHFT;
  if(i1<YSHFT)i1=YSHFT;
  if(i1<XSHFT)i1=XSHFT;
  if(s.data_store.rows<2)return;

   for(ip=0;ip<np;ip++){
     if (s.plot_file.plt_fmt_flag==SVGFMT)
     {
  	   xpp::print(s.plot_file.svgfile,"<g>\n");
     } 
     kxoff=i1-XSHFT;
     kzoff=i1-ZSHFT;
     kyoff=i1-YSHFT;

    iiXPLT=s.plot_windows.current->xv[ip];
    iiYPLT=s.plot_windows.current->yv[ip];
    iiZPLT=s.plot_windows.current->zv[ip];
    set_linestyle(s.plot_windows.current->color[ip]);
    oldxpl=data[iiXPLT][kxoff];
    oldypl=data[iiYPLT][kyoff];
    oldzpl=data[iiZPLT][kzoff];
    for(i=i1;i<i2;i++){
      {
	xpl=data[iiXPLT][kxoff];
	ypl=data[iiYPLT][kyoff];
	zpl=data[iiZPLT][kzoff];
      }
      
      if(s.numerics.torus==1)
      {
	if (fabs(oldxpl-xpl)>static_cast<float>((.5*s.numerics.tor_period)))oldxpl=xpl;
	if (fabs(oldypl-ypl)>static_cast<float>((.5*s.numerics.tor_period)))oldypl=ypl;
	if (fabs(oldzpl-zpl)>static_cast<float>((.5*s.numerics.tor_period)))oldzpl=zpl;
      }
      if(s.plot_windows.current->ColorFlag!=0&&i>i1){
	  for(j=0;j<=xpp::model().neq;j++){
	    v1[j]=data[j][i];
	    v2[j]=data[j][i-1];
	  }

	  comp_color(v1,v2,xpp::model().node,
		     static_cast<float>(fabs(data[0][i]-data[0][i+1])));
	}     /* ignored by postscript */
      if(s.plot_windows.current->line[ip]<=0){
	s.drawing.point_radius=-s.plot_windows.current->line[ip];
	if(s.plot_windows.current->ThreeDFlag==0)point_abs(xpl,ypl);
	else point_3d(xpl,ypl,zpl);
      }
      else {
	if(s.plot_windows.current->ThreeDFlag==0)
	  line_abs(oldxpl,oldypl,xpl,ypl);
	else
	  line_3d(oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
      }
     /*noplot:*/
      oldxpl=xpl;
      oldypl=ypl;
      oldzpl=zpl;
      kxoff++;
      kyoff++;
      kzoff++;
     
    }
    if (s.plot_file.plt_fmt_flag==SVGFMT)
     {
  	   xpp::print(s.plot_file.svgfile,"</g>\n");
     } 
    
  }
}

/*  Sets the color according to the velocity or z-value */
void comp_color(float *v1, float *v2, int n, float dt)
{
 int i,cur_color;
 float sum;
 float min_scale=static_cast<float>((xpp::session().plot_windows.current->min_scale));
 float color_scale=static_cast<float>((xpp::session().plot_windows.current->color_scale));
 if(xpp::session().plot_windows.current->ColorFlag==2){
   sum=v1[xpp::session().plot_windows.current->ColorValue];
 }
 else
   {
     for(i=0,sum=0.0;i<n;i++)sum+=static_cast<float>(fabs(static_cast<double>((v1[i+1]-v2[i+1]))));
     sum=sum/(dt);
   }
 cur_color=static_cast<int>(((sum-min_scale)*static_cast<float>(color_table.count)/color_scale));
 if(cur_color<0)cur_color=0;
 if(cur_color>color_table.count)cur_color=color_table.count-1;
  cur_color+=FIRSTCOLOR;
  if (program.interactive){set_color(cur_color);}
 if(xpp::session().plot_file.plt_fmt_flag==1){ps_do_color(cur_color);}
 else if(xpp::session().plot_file.plt_fmt_flag==SVGFMT){svg_do_color(cur_color);}
}

void shoot_easy(double *x)
{
  double t=0.0;
  int i;
  xpp::session().integrator.suppress_bounds=1;
integrate(&t,x,xpp::session().numerics.tend,xpp::session().numerics.delta_t,1,xpp::session().numerics.njmp,&i);
  xpp::session().integrator.suppress_bounds=0;
}

void shoot(double *x, double *xg, double *evec, int sgn)
{
 int i;
 double t=0.0;
 xpp::session().integrator.suppress_bounds=1;
 for(i=0;i<xpp::model().node;i++)
 x[i]=xg[i]+sgn*evec[i]*xpp::session().numerics.delta_t*.1;
i=1;
 integrate(&t,x,xpp::session().numerics.tend,xpp::session().numerics.delta_t,1,xpp::session().numerics.njmp,&i);
 ping();
  xpp::session().integrator.suppress_bounds=0;
}

void stop_integration()
{
 /*  set some global error here... */
  if(xpp::session().integrator.delay_err==0)
    err_msg("Delay too large or negative");
 xpp::session().integrator.delay_err=1;
}

int stor_full()
{

 char ch;
 int nrow=2*xpp::session().data_store.max_rows;
 if(xpp::session().data_store.grow(xpp::model().neq+1,nrow)){
   xpp::session().data_store.max_rows=nrow;
   return 1;
 }

 if(!program.interactive){
   xpp::log(XPP_LOG_WARN, " Storage full -- increase maxstor \n");
   return(0);
 }
 if(xpp::session().numerics.forever)goto ov;
 ping();
 ch=static_cast<char>(TwoChoice("YES","NO","Storage full: Overwrite?",
		     "yn"));
 if(ch=='y')
 {
ov:
  xpp::session().data_store.rows=0;
  return(1);
 }
  return(0);
}
