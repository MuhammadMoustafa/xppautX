#include <charconv>
#include <cstring>
#include "mykeydef.h"
#include "image_format.h"
#include "solver.h"
#include "xpp_math.h"
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "auto_nox.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "xpp_files.h"
#include "model_files.h"
#include "integrate.h"
#include "jacobian.h"

#include "storage.h"

#include "expr.h"
#include "markov.h"
#include "tabular.h"
#include "adj2.h"
#include "browse.h"
#include "derived.h"
#include "equilibria.h"
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
#include "numerics.h"
#include "numerics_settings.h"
#include "my_rhs.h" /* extra */
#include "volterra2.h"
#include <stdlib.h> 
#include <cmath>
#include <climits>
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
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <string_view>
#include <vector>
#include "menudrive.h"
#include "arrayplot.h"
#include "xpp_job.h"
#include "phase_data.h"
#include "xpp_batch.h"
#include "colormap.h"
#include "comline.h"

namespace xpp {

DenseEvent dense_event(Session &s, double from, double until, double step_from, double step_until,
                      const std::function<void(double,std::span<double>)> &state,
                      std::span<double> scratch, std::span<double> derivative, bool sections)
{
  DenseEvent event;
  event.time=until;
  const double direction=until>from?1:-1;
  // Simultaneous events agree to rounding precision in model time.
  constexpr int simultaneous_time_ulps=128;
  const double coincidence=simultaneous_time_ulps*std::numeric_limits<double>::epsilon()*std::max({1.0,fabs(from),fabs(until)});
  const auto consider=[&](double time,int flag,bool section,int bound){
    if(!event.found||(time-event.time)*direction < -coincidence){
      event={}; event.found=true; event.time=time;
    }
    if(fabs(time-event.time)<=coincidence){
      if(flag>=0){event.flags.push_back(flag);event.flag_count++;}
      event.section=event.section||section;
      if(bound>=0)event.bound_variable=bound;
    }
  };
  const auto scan=[&](const std::function<double(double)> &value,int sign,int flag,bool section,int bound,bool nointerp){
    const double left=nointerp?step_from:from, right=nointerp?step_until:until;
    if(nointerp&&until!=step_until)return;
    double a=left,fa=value(a);
    for(int j=1;j<=DENSE_EVENT_SUBDIVISIONS;++j){
      const double b=j==DENSE_EVENT_SUBDIVISIONS?right:left+(right-left)*j/DENSE_EVENT_SUBDIVISIONS;
      const double fb=value(b);
      const bool up=fa<0&&fb>=0, down=fa>0&&fb<=0;
      if((sign>=0&&up)||(sign<=0&&down)){
        const double root=nointerp?step_until:bracketed_root(value,a,b,fa,fb);
        if((root-from)*direction>coincidence){consider(root,flag,section,bound);break;}
      }
      a=b;fa=fb;
    }
  };
  for(int i=0;i<s.model().flag_count();++i){
    const auto &flag=s.model().flags[i];
    scan([&](double t){state(t,scratch);set_fix_rhs(s,t,scratch.data());return evaluate(s,flag.comcond);},
         flag.sign,i,false,-1,flag.nointerp!=0);
  }
  const auto &num=s.numerics;
  if(sections&&num.poincare_map!=PoincareMap::none){
    if(num.poincare_variable==0&&num.poincare_map!=PoincareMap::max){
      const double period=fabs(num.poincare_plane);
      if(period>0){
        const double k=direction>0?floor(from/period)+1:ceil(from/period)-1;
        const double time=k*period;
        if((time-from)*direction>0&&(until-time)*direction>=0)consider(time,-1,true,-1);
      }
    }else if(num.poincare_variable>0){
      const int index=num.poincare_variable-1;
      scan([&](double t){
        state(t,scratch);
        if(num.poincare_map==PoincareMap::max){
          s.integrator.rhs(t,scratch.data(),derivative.data(),s.model().node);
          return derivative[index];
        }
        if(index>=s.model().node)extra(s,scratch.data(),t,s.model().node,s.model().neq);
        return scratch[index]-num.poincare_plane;
      },num.poincare_map==PoincareMap::max?-static_cast<int>(num.poincare_sign)*static_cast<int>(direction):static_cast<int>(num.poincare_sign),-1,true,-1,false);
    }
  }
  if(s.numerics.stop_at_limit&&!(s.integrator.range_flag||s.integrator.suppress_bounds)){
    state(from,scratch);
    for(int i=0;i<s.model().node;++i)derivative[i]=fabs(scratch[i])-num.state_limit;
    double a=from;
    for(int j=1;j<=DENSE_EVENT_SUBDIVISIONS;++j){
      const double b=j==DENSE_EVENT_SUBDIVISIONS?until:from+(until-from)*j/DENSE_EVENT_SUBDIVISIONS;
      state(b,scratch);
      for(int i=0;i<s.model().node;++i){
        const double fb=fabs(scratch[i])-num.state_limit,fa=derivative[i];
        if(fa<=0&&fb>0){
          const double root=bracketed_root([&](double time){state(time,scratch);return fabs(scratch[i])-num.state_limit;},a,b,fa,fb);
          consider(root,-1,false,i);
          state(b,scratch);
        }
        derivative[i]=fb;
      }
      a=b;
    }
  }
  return event;
}

/* a row was just stored (storage[.][storind-1]): a replayed script may stop
   the job here (xpp_job.h), and a front end may show the run growing */
static void row_stored(xpp::Session &s)
{
  xpp::job::report_rows(s.data_store.rows, s.data_store.col[0][s.data_store.rows-1]);
  rows_stored(s,s.data_store.rows);
}
/* the state x of the Session's solver work copied into u from v (its node ODEs) */
static void mswtch(const xpp::Session &s, double *u, const double *v)
{
  memcpy(u,v,s.solver_work.xpv.node*sizeof(double));
}



#define PARAM 1
#define IC 2


constexpr int OnTheFly=1;



    
void init_range(xpp::Session &s)
{
 s.integrator.eq_range.col=-1;
 s.integrator.eq_range.mc=0;
 s.integrator.eq_range.shoot=0;
 s.integrator.eq_range.steps=10;
 s.integrator.eq_range.plow=0.0;
 s.integrator.eq_range.phigh=1.0;
 s.integrator.eq_range.movie=0;
 s.integrator.eq_range.item=s.model().upar_names[0];
 s.integrator.range.type=0;
 s.integrator.range.rtype=0;
 s.integrator.range.index=s.integrator.range.index2=0;
 s.integrator.range.steps2=20;
 s.integrator.range.cycle=0;
 s.integrator.range.movie=0;
 s.integrator.range.item2=s.model().uvar_names[0];
 init_shoot_range(s,s.model().upar_names[0]); 
 init_monte_carlo(s);
}

int set_up_eq_range(xpp::Session &s)
{
 std::array<xpp::FormField,8> fields={
   xpp::FormField{.name="*2Range over",.kind=XPP_FIELD_NAME_IN(2),.value=s.integrator.eq_range.item},
   xpp::FormField{.name="Steps",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator.eq_range.steps),.min=1},
   xpp::FormField::real_number("Start",s.integrator.eq_range.plow),
   xpp::FormField::real_number("End",s.integrator.eq_range.phigh),
   xpp::FormField::boolean("Shoot",s.integrator.eq_range.shoot),
   xpp::FormField{.name="Stability col",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator.eq_range.col)},
   xpp::FormField::boolean("Movie",s.integrator.eq_range.movie),
   xpp::FormField::boolean("Monte Carlo",s.integrator.eq_range.mc)};
 if(!xpp::ask_form(s,"Range Equilibria",fields))return 0;
 s.integrator.eq_range.item=fields[0].value;
 if(find_user_name(s.model(),PARAM,s.integrator.eq_range.item)<0){
   command_error("range",xpp::format("No such parameter {}",s.integrator.eq_range.item));
   return 0;
 }
 s.integrator.eq_range.steps=fields[1].whole;
 s.integrator.eq_range.plow=fields[2].real;
 s.integrator.eq_range.phigh=fields[3].real;
 s.integrator.eq_range.shoot=fields[4].flag;
 s.integrator.eq_range.col=fields[5].whole;
 if(s.integrator.eq_range.col<=1||s.integrator.eq_range.col>(s.model().neq+1))s.integrator.eq_range.col=-1;
 s.integrator.eq_range.movie=fields[6].flag;
 s.integrator.eq_range.mc=fields[7].flag;
 return 1;
}

namespace {
/* what a range varies: item a parameter (PARAM) or else a variable (IC),
   and its index; 0 (and a message) when it is neither */
int find_range_item(const xpp::Session &s, const std::string &item, int *type, int *index)
{
 int i=find_user_name(s.model(),PARAM,item);
 if(i>-1){
   *type=PARAM;
   *index=i;
   return 1;
 }
 i=find_user_name(s.model(),IC,item);
 if(i<=-1){
   command_error("range",xpp::format("{} is not a parameter or variable",item));
   return(0);
 }
 *type=IC;
 *index=i;
 return 1;
}
}

int range_item(xpp::Session &s)
{
 return find_range_item(s,s.integrator.range.item,&s.integrator.range.type,&s.integrator.range.index);
}

int range_item2(xpp::Session &s)
{
 return find_range_item(s,s.integrator.range.item2,&s.integrator.range.type2,&s.integrator.range.index2);
}

int set_up_range(xpp::Session &s)
{
 if(!program.interactive){ /* no dialog: the range the options set */
   if(range_item(s)==0)return 0;
   s.integrator.range_flag=1;
   return 1;
 }
 std::array<xpp::FormField,9> fields={
   xpp::FormField{.name="*3Range over",.value=s.integrator.range.item},
   xpp::FormField{.name="Steps",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator.range.steps),.min=1},
   xpp::FormField::real_number("Start",s.integrator.range.plow),
   xpp::FormField::real_number("End",s.integrator.range.phigh),
   xpp::FormField::boolean("Reset storage",s.integrator.range.reset),
   xpp::FormField::boolean("Use old ic's",s.integrator.range.oldic),
   xpp::FormField::boolean("Cycle color",s.integrator.range.cycle),
   xpp::FormField::boolean("Movie",s.integrator.range.movie),
   xpp::FormField::boolean("Write each run to its own file",s.integrator.range.write_files)};
 if(!xpp::ask_form(s,"Range Integrate",fields))return 0;
 s.integrator.range.item=fields[0].value;
 if(range_item(s)==0)return 0;
 s.integrator.range.steps=fields[1].whole;
 s.integrator.range.plow=fields[2].real;
 s.integrator.range.phigh=fields[3].real;
 s.integrator.range.reset=fields[4].flag;
 s.integrator.range.oldic=fields[5].flag;
 s.integrator.range.cycle=fields[6].flag;
 s.integrator.range.movie=fields[7].flag;
 s.integrator.range.write_files=fields[8].flag;
 s.integrator.range_flag=1;
 return 1;
}

int set_up_range2(xpp::Session &s)
{
 if(!program.interactive){
   return(range_item(s));
 }
 std::array<xpp::FormField,14> fields={
   xpp::FormField{.name="*3Vary1",.kind=XPP_FIELD_NAME_IN(3),.value=s.integrator.range.item},
   xpp::FormField::real_number("Start1",s.integrator.range.plow),
   xpp::FormField::real_number("End1",s.integrator.range.phigh),
   xpp::FormField{.name="*3Vary2",.kind=XPP_FIELD_NAME_IN(3),.value=s.integrator.range.item2},
   xpp::FormField::real_number("Start2",s.integrator.range.plow2),
   xpp::FormField::real_number("End2",s.integrator.range.phigh2),
   xpp::FormField{.name="Steps",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator.range.steps),.min=1},
   xpp::FormField::boolean("Reset storage",s.integrator.range.reset),
   xpp::FormField::boolean("Use old ic's",s.integrator.range.oldic),
   xpp::FormField::boolean("Cycle color",s.integrator.range.cycle),
   xpp::FormField::boolean("Movie",s.integrator.range.movie),
   xpp::FormField::choice("Vary both",{"together","independently"},s.integrator.range.rtype==2?1:0),
   xpp::FormField{.name="Steps2",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator.range.steps2),.min=1},
   xpp::FormField::boolean("Write each run to its own file",s.integrator.range.write_files)};
 if(!xpp::ask_form(s,"Double Range Integrate",fields))return 0;
 s.integrator.range.item=fields[0].value;
 if(range_item(s)==0)return 0;
 s.integrator.range.item2=fields[3].value;
 if(range_item2(s)==0)return 0;
 s.integrator.range.plow=fields[1].real;
 s.integrator.range.phigh=fields[2].real;
 s.integrator.range.plow2=fields[4].real;
 s.integrator.range.phigh2=fields[5].real;
 s.integrator.range.steps=fields[6].whole;
 s.integrator.range.reset=fields[7].flag;
 s.integrator.range.oldic=fields[8].flag;
 s.integrator.range.cycle=fields[9].flag;
 s.integrator.range.movie=fields[10].flag;
 s.integrator.range.rtype=fields[11].whole+1;
 s.integrator.range.steps2=fields[12].whole;
 s.integrator.range.write_files=fields[13].flag;
 s.integrator.range_flag=1;
 return 1;
}

void init_monte_carlo(xpp::Session &s)
{
  s.integrator.fixptguess.tol=.001;
  s.integrator.fixptguess.n=100;
  s.integrator.fixptguess.xlo.assign(s.model().node,-10);
  s.integrator.fixptguess.xhi.assign(s.model().node,10);
  s.integrator.fixptlist.flag=0;
  s.integrator.fixptlist.n=0;
}

void monte_carlo(xpp::Session &s)
{
  int append=0;
  int i=0,done=0,ishoot=0;
  double z;
  new_int("Append(1/0",&append);
  new_int("Shoot (1/0)",&ishoot);
  new_int("# Guesses:",&s.integrator.fixptguess.n);
  new_float(s,"Tolerance:",&s.integrator.fixptguess.tol);
  while(1){
    z=s.integrator.fixptguess.xlo[i];
    done=new_float(s,xpp::format("{}_lo :",s.model().uvar_names[i]),&z);
    if(done==0)
      s.integrator.fixptguess.xlo[i]=z;
    if(done==-1)break;
    z=s.integrator.fixptguess.xhi[i];
    done=new_float(s,xpp::format("{}_hi :",s.model().uvar_names[i]),&z);
    if(done==0)
      s.integrator.fixptguess.xhi[i]=z;
    if(done==-1)break;
    i++;
    if(i>=s.model().node)
      break;
  }
  const xpp::Result<> r=do_monte_carlo_search(s,append, 1,ishoot);
  if(!r)xpp::show_error(r.error());
}

xpp::Result<> do_monte_carlo_search(xpp::Session &s, int append, int stuffbrowse,int ishoot)
{
  xpp::FirstError failure; /* a manifold's */
  int i,j,k,m,n=s.integrator.fixptguess.n;
  int ierr,is_new=1;
  int attempted=0,converged=0;
  double sum;
  std::vector<double> x(s.model().node),er(s.model().node),em(s.model().node);
  if(append==0)
    s.integrator.fixptlist.n=0;

  if(s.integrator.fixptlist.flag==0){
    s.integrator.fixptlist.x.resize(MAXFP);
    s.integrator.fixptlist.er.resize(MAXFP);
    s.integrator.fixptlist.em.resize(MAXFP);
    for(i=0;i<MAXFP;i++){
      s.integrator.fixptlist.x[i].assign(s.model().node,0.0);
      s.integrator.fixptlist.er[i].assign(s.model().node,0.0);
      s.integrator.fixptlist.em[i].assign(s.model().node,0.0);
    }
    s.integrator.fixptlist.flag=1;
  }
  for(i=0;i<n;i++){
    if(xpp::job::cancelled())break;
    attempted++;
    ierr=1;
    for(j=0;j<s.model().node;j++){ 
      x[j]=s.random.uniform()*(s.integrator.fixptguess.xhi[j]-s.integrator.fixptguess.xlo[j])+s.integrator.fixptguess.xlo[j];
    }
    if(auto r=do_sing_info(s,x.data(),s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,s.model().node,er.data(),em.data(),&ierr);!r){failure.keep(r);if(ierr!=0)break;}
    if(ierr==0){
      converged++;
      m=s.integrator.fixptlist.n;
      if(m==0){ /* first fixed point found */
	s.integrator.fixptlist.n=1;
	xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	for(j=0;j<s.model().node;j++){
	  s.integrator.fixptlist.x[0][j]=x[j];
	  s.integrator.fixptlist.er[0][j]=er[j];
	  s.integrator.fixptlist.em[0][j]=em[j];
          if(ishoot)failure.keep(shoot_this_now(s));
	  xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	}
      }
      else { /* there are others  better compare them */
	is_new=1;
	for(k=0;k<m;k++){
	  sum=0.0;
	  for(j=0;j<s.model().node;j++)
	    sum+=fabs(x[j]-s.integrator.fixptlist.x[k][j]);
	  if(sum<s.integrator.fixptguess.tol)
	    is_new=0;
	}
	if(is_new==1){
	  m=s.integrator.fixptlist.n;
	  if(m>=MAXFP){
	    failure.keep(xpp::Error{"equilibria",xpp::format("The Monte Carlo search found {} equilibria, the most it keeps: stopped after {} of {} guesses",MAXFP,i,n),command_place()});
	    break;
	  }
	  s.integrator.fixptlist.n++;
	  {
	    xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	    for(j=0;j<s.model().node;j++){
	      s.integrator.fixptlist.x[m][j]=x[j];
	      s.integrator.fixptlist.er[m][j]=er[j];
	      s.integrator.fixptlist.em[m][j]=em[j];
	      if(ishoot)failure.keep(shoot_this_now(s));
	      xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	    }
	  }
	}
      }
    }
  }
  if(stuffbrowse) {
    reset_browser(s);
    s.data_store.rows=0;
    m=s.integrator.fixptlist.n;
    for(i=0;i<m;i++){
      if(s.data_store.rows==s.data_store.max_rows){
        const auto grown=stor_full(s);
        if(!grown){failure.keep(grown);break;}
      }
      s.data_store.col[0][s.data_store.rows]=static_cast<float>(i);
      for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=static_cast<float>(s.integrator.fixptlist.x[i][j]);
      s.data_store.rows++;
    }
    refresh_browser(s,s.data_store.rows);
  }
  bottom_msg(0,xpp::format("Monte Carlo: {} of {} guesses converged to {} distinct equilibria ({} requested; {}); {}",
      converged,attempted,s.integrator.fixptlist.n,n,
      xpp::named_settings(s,{"max_iterations","newton_tolerance","search_limit"}),
      xpp::job::cancelled()?"stopped by the user":attempted<n?"stopped early":"search finished"));
  return failure.result();
}

namespace {
/* a range's movie takes a frame of each step into the kinescope: false
   when it is out of film */
using TakeFrame = std::function<bool()>;
/* a sweep's progress line ("p=0.3  i=4"), which the command shows */
using ShowProgress = std::function<void(const std::string &)>;

/* a range command's movie (Range's, Range Equilibria's): the frames its
   sweep takes go into s's kinescope until it is out of film, which the
   command says once the sweep has ended */
class RangeFilm {
public:
  explicit RangeFilm(xpp::Session &s) : s_(s) {}
  TakeFrame taker()
  {
    return [this]{
      if(result_)result_=ui.film_clip(s_);
      return result_.has_value();
    };
  }
  static ShowProgress progress()
  {
    return [](const std::string &line){ bottom_msg(2,line); };
  }
  const xpp::Result<> &result() const { return result_; }
private:
  xpp::Session &s_;
  xpp::Result<> result_;
};

/* Range Equilibria's sweep over s.integrator.eq_range: each step's equilibrium (or
   Monte Carlo's) stored as a row; the first step that failed is
   returned once the sweep has ended */
xpp::Result<> eq_range_sweep(xpp::Session &s, double *x, const TakeFrame &take_frame, const ShowProgress &show_progress)
{
 double parlo,parhi,dpar,temp;
 int npar,stabcol,i,j,ierr;
 int mc;
 float stabinfo;
 xpp::FirstError failure;
 int without_root=0; /* the values the search found no equilibrium at */
 std::string first_without;

 wipe_rep(s.browser);
 data_back(s);
 parlo=s.integrator.eq_range.plow;
 parhi=s.integrator.eq_range.phigh;
 
 npar=s.integrator.eq_range.steps;
 dpar=(parhi-parlo)/static_cast<double>(npar);
 stabcol=s.integrator.eq_range.col;
 mc=s.integrator.eq_range.mc;
 s.data_store.rows=0;
 s.integrator.step_error.reset();
 s.numerics.endsing=false;
 s.numerics.par_fol=true;
 s.numerics.pauser=false;
 s.numerics.shoot=s.integrator.eq_range.shoot;
 reset_browser(s);
 if(mc==1){
   s.integrator.eq_range.movie=1;
   s.numerics.shoot=false;
 }
 if(s.integrator.eq_range.movie)reset_film(s);
 for(i=0;i<=npar;i++)
   {
     if(xpp::job::cancelled())break;
     if(s.integrator.eq_range.movie)
       clear_draw_window(s);
      temp=parlo+dpar*static_cast<double>(i);
      bool found=true;
      set_val(s,s.integrator.eq_range.item,temp);
      s.numerics.par_fol=true;
      {
        std::string bob=xpp::format("{}={:.16g}",s.integrator.eq_range.item,temp);
        show_progress(bob);
        evaluate_derived(s);
        /*  I think  */ failure.keep(redo_all_fun_tables(s));
        if(mc) {
	  failure.keep(do_monte_carlo_search(s,0,0,1));
        }
        else {
        const xpp::Result<> search=s.delay.flag
          ? do_delay_sing(s,x,s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,
		          s.model().node,&ierr,&stabinfo)
          : do_sing(s,x,s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,
		    s.model().node,&ierr,&stabinfo);
        if(!search){
          /* a value with no equilibrium has no row; the sweep goes on */
          const std::string note=xpp::format("{} = {}: no equilibrium found ({})",s.integrator.eq_range.item,xpp::number(temp),search.error().what);
          xpp::log(XPP_LOG_WARN,"Range Equilibria step {} of {}: {}\n",i+1,npar+1,note);
          if(without_root++==0)first_without=note;
          found=false;
        }
        }
        if(s.integrator.eq_range.movie){
	  draw_label(s,s.plot_windows.draw_win);
	  take_frame(); /* out of film, the sweep goes on without frames */
        }
      }
      if(mc==0&&found){
      if(s.data_store.rows==s.data_store.max_rows){
        const auto grown=stor_full(s);
        if(!grown){failure.keep(grown);break;}
      }
      s.data_store.col[0][s.data_store.rows]=temp;
      for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=static_cast<float>(x[j]);
      for(j=s.model().node;j<s.model().node+s.model().nmarkov;j++)s.data_store.col[j+1][s.data_store.rows]=0.0;
      if(stabcol>0)s.data_store.col[stabcol-1][s.data_store.rows]=stabinfo;

      s.data_store.rows++;
      row_stored(s);}
      if(s.numerics.endsing||xpp::job::cancelled())break;
    }
    refresh_browser(s,s.data_store.rows);
 s.numerics.par_fol=false;
 if(without_root>0)bottom_msg(0,xpp::format("{} of {} values have no equilibrium; the first: {}",without_root,npar+1,first_without));
 return failure.result();
}
}

void do_eq_range(xpp::Session &s, double *x)
{
 if(set_up_eq_range(s)==0)return;
 RangeFilm film(s);
 const xpp::Result<> r=eq_range_sweep(s,x,film.taker(),film.progress());
 if(!r)xpp::show_error(r.error());
 else if(!film.result())xpp::show_error(film.result().error());
}

void swap_color(xpp::Session &s, xpp::Color *col, int rorw)
{
 if(rorw)s.plot_windows.current->color[0]=*col;
 else *col=s.plot_windows.current->color[0];
}

void set_cycle(xpp::Session &s, int flag, int *icol)
{
 if(flag==0)return;
 s.plot_windows.current->color[0]=xpp::xppaut_color(*icol+1); /* XPPAUT cycles red to purple */
 *icol=*icol+1;
  if(*icol==10)*icol=0;
} 

int do_auto_range_go(xpp::Session &s)
{
  double *x;
  x=&s.data_store.current[0];
  return(do_range(s,x,2));
}

namespace {

/* Seeds the random generator for a run about to start (Go, a do_range
   sweep, usual_integrate_stuff's plain run): rand_seed is the seed shown
   or set for the next run (docs/roadmap.md W71, "@ seed=" in
   load_eqn.cpp, Stochastic > New seed in markov.cpp, --newseed in
   expr_symbols.cpp's init_rpn -- each of those already seeds s.random
   with it immediately too, unchanged, so this reapplies exactly the
   same value and changes nothing there); apply it, log it and keep it
   as last_seed for the protocol's state and a saved data file's
   header/metadata, then draw a fresh rand_seed from a seed stream of
   its own (xpp::next_seed) so an untouched field still gives fresh
   noise next time (a first run's noise and every example md5 stay
   exactly what they were). */
void seed_this_run(xpp::Session &s)
{
  const int seed=s.numerics.rand_seed;
  s.random.seed(seed);
  s.numerics.last_seed=seed;
  xpp::log(XPP_LOG_INFO,"Go: seed {}\n",seed);
  s.numerics.rand_seed=xpp::next_seed(seed);
}

/* A run that ended early without failing (an event's arret, a section that
   stops the run) says why, once: its note is shown and cleared. */
void show_stop_note(xpp::Session &s)
{
  if(s.integrator.stop_note.empty())return;
  bottom_msg(0,s.integrator.stop_note);
  s.integrator.stop_note.clear();
}

/* Range's sweep (flag 0 one parameter, 1 two, 2 AUTO's range over its
   diagram), the settings made: one integration per step; -1 when one
   stopped early (the sweep ends there), else 0; or the error it failed
   with */
xpp::Result<int> range_sweep(xpp::Session &s, double *x, int flag, const TakeFrame &take_frame, const ShowProgress &show_progress)
{
  std::optional<xpp::Error> failure;
  bool complete=true; /* a failed capture can stop the sweep without an integration error */

  std::string bob;
  std::string parn; /* auto_get_info writes the parameter's name */
 int ivar=0,ivar2=0,res=0,oldic=0;
 int nit=20,i=0,j=0,itype=0,itype2=0,cycle=0,icol=0,nit2=0,iii=0;
 xpp::Color color=s.plot_windows.current->color[0];
 double t,dpar,plow=0.0,phigh=1.0,p=0.0,plow2=0.0,phigh2=0.0,p2=0.0,dpar2=0.0;
 double temp,temp2;
 int ierr=0;
 xpp::Computation computing; /* the whole range, between its integrations too (xpp_job.h) */

 seed_this_run(s); /* the whole sweep (Stochastic > Compute's many runs
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

 get_ic(s,2,x);
 s.data_store.rows=0;
 s.numerics.storflag=true;
 s.numerics.pauser=false;
nit2=0;
if(s.integrator.range.rtype==2)nit2=s.integrator.range.steps2; 
if(s.integrator.range.type==PARAM)get_val(s,s.integrator.range.item,&temp);
 alloc_liap(s,nit); /* make space */
 if(s.integrator.range.rtype>0){
 itype2=s.integrator.range.type2;
 ivar2=s.integrator.range.index2;
 plow2=s.integrator.range.plow2;
 phigh2=s.integrator.range.phigh2;
  if(s.integrator.range.rtype==2)dpar2=(phigh2-plow2)/static_cast<double>(nit2);
  else dpar2=(phigh2-plow2)/static_cast<double>(nit);
  if(s.integrator.range.type2==PARAM)get_val(s,s.integrator.range.item2,&temp2);
 
 }

 if(s.integrator.range.movie)reset_film(s);
 if(flag==2){
   auto_get_info(s, &nit,parn);
   nit2=0;
 }
 const int runs=(nit+1)*(nit2+1);
 int early=0; /* the runs that ended before their end without failing */
 std::string first_early; /* the first of them, with its parameter value */
 for(j=0;j<=nit2;j++){
 for(i=0;i<=nit;i++)
  {
    if(s.integrator.range.movie)clear_draw_window(s);
   if(cycle)s.plot_windows.current->color[0]=xpp::xppaut_color(icol+1);
   icol++;
   if(icol==10)icol=0;
   t=s.numerics.start_time;
   s.integrator.my_start=1;
   s.numerics.poiext=0;

   if(flag!=2){
     p=plow+dpar*static_cast<double>(i);
     if(s.integrator.range.rtype==1)
       p2=plow2+dpar2*static_cast<double>(i);
     if(s.integrator.range.rtype==2)
       p2=plow2+dpar2*static_cast<double>(j);
   
     if(oldic==1){
       get_ic(s,1,x);
       
       if(s.delay.flag){
	 /* restart initial data */
	 if(const xpp::Result<> r=do_init_delay(s,s.numerics.delay);!r){
	   failure=r.error();
	   complete=false;
	   break;
	 }
       }
     }

     if(itype==IC)x[ivar]=p;
     else {
       set_val(s,s.integrator.range.item,p);
       if(auto t=redo_all_fun_tables(s);!t&&!failure)failure=t.error();
       re_evaluate_kernels(s);
       
     }
     if(s.integrator.range.rtype>0){
       if(itype2==IC)x[ivar2]=p2;
       else {
	 set_val(s,s.integrator.range.item2,p2);
	 if(auto t=redo_all_fun_tables(s);!t&&!failure)failure=t.error();
	 re_evaluate_kernels(s);
       }
     }
     if(program.interactive){   
       if(s.integrator.range.rtype>0)
	 bob=xpp::format("{}={:.16g}  {}={:.16g}",s.integrator.range.item,p,s.integrator.range.item2,p2);
       else
	 bob=xpp::format("{}={:.16g}  i={}",s.integrator.range.item,p,i);
       show_progress(bob);
     }
   }  /* normal range stuff   */ 
   else {  /* auto range stuff */
     auto_set_mark(s, i);
     get_ic(s,2,x);
     get_val(s,parn,&temp);
     bob=xpp::format("{:.230}={:.16g}",parn,temp);
     show_progress(bob);
   }
   const int run_number=j*(nit+1)+i+1;
   const std::string where=flag==2?xpp::format("{} = {}",parn,xpp::number(temp))
     :s.integrator.range.rtype>0?xpp::format("{} = {}, {} = {}",s.integrator.range.item,xpp::number(p),s.integrator.range.item2,xpp::number(p2))
     :xpp::format("{} = {}",s.integrator.range.item,xpp::number(p));
   do_start_flags(s,x,&s.data_store.current_time);
if(stored_from(s.numerics.store_from,s.data_store.current_time,s.numerics.delta_t)&&s.numerics.storflag&&s.numerics.poincare_map==PoincareMap::none)
  {
    s.data_store.col[0][s.data_store.rows]=static_cast<float>(s.data_store.current_time);
    extra(s,x,s.data_store.current_time,s.model().node,s.model().neq);
    for(iii=0;iii<s.model().neq;iii++)s.data_store.col[1+iii][s.data_store.rows]=static_cast<float>(x[iii]);
    s.data_store.rows++;
  }

 const xpp::Result<int> run=integrate(s,&t,x,s.numerics.total_time,s.numerics.delta_t,1,s.numerics.store_every,&s.integrator.my_start);
 if(!run||*run==1||xpp::job::cancelled()){
   if(!run){
     failure=run.error();
     failure->what=xpp::format("Range stopped at {} (run {} of {}): {}",where,run_number,runs,failure->what);
     failure->action.reset(); /* Continue goes on from a single run, not a sweep's */
   }
   ierr=-1;
   complete=false;
   break;
 }
 if(!s.integrator.stop_note.empty()){
   const std::string note=xpp::format("{}: {}",where,s.integrator.stop_note);
   xpp::log(XPP_LOG_WARN,"Range run {} of {} ended early at {}\n",run_number,runs,note);
   if(early++==0)first_early=note;
   s.integrator.stop_note.clear();
 }
 if(s.stochastic.flag)
   append_stoch(s,i,s.data_store.rows);

 if(s.integrator.range.movie){
   redraw_dfield(s);
	create_new_cline(s);
   draw_label(s,s.plot_windows.draw_win);
   if(!take_frame()){complete=false;break;}
 }
 do_batch_nclines(s);
 do_batch_dfield(s);
 refresh_browser(s,s.data_store.rows);
 if(s.integrator.adj_range==1){
   bob=xpp::format("{}_{:g}",s.integrator.range.item,p);
   data_get_mybrowser(s,s.data_store.rows-1);
   compute_one_period(s,static_cast<double>(s.data_store.col[0][s.data_store.rows-1]),s.last_ic.data(),bob.c_str());
 }

 do_this_liaprun(s,i,p);  /* sends parameter and index back */
 if(s.data_store.rows>2)auto_freeze_it(s);
 if(s.array_plot.range==1)
   draw_one_array_plot(s,bob);
 
 if(res==1||s.stochastic.flag)
   {
     if(s.integrator.range.write_files){
       post_process_stuff(s);
       const std::string file = program.interactive
         ? xpp::include_path(s.model().this_file,xpp::files::output_name(s.model().this_file,".dat"))
         : s.batch_out_file;
       write_this_run(s,file.c_str(),j*(nit+1)+i);
     }
     s.data_store.rows=0;
   }
  }
  if(!complete)break;
 }
 if(early>0)bottom_msg(0,xpp::format("{} of {} runs ended early; the first at {}",early,runs,first_early));
 if(s.array_plot.range==1){
   s.array_plot.range=0;
   close_aplot_files(s, complete && !failure && ierr == 0 && !xpp::job::cancelled());
 }
 if(oldic==1)get_ic(s,1,x);
 else set_initial_data(s,{x,static_cast<size_t>(s.model().node+s.model().nmarkov)});
 if(s.integrator.range.type==PARAM)set_val(s,s.integrator.range.item,temp);
 if(s.integrator.range.rtype>0)
   if(s.integrator.range.type2==PARAM)set_val(s,s.integrator.range.item2,temp2);
 evaluate_derived(s);
s.plot_windows.current->color[0]=color;
 s.numerics.inflag=true;
 
 ping();
 s.integrator.adj_range=0;
 if(s.stochastic.flag)
   do_stats(s,ierr);

 if(failure)return std::unexpected(std::move(*failure));
 return(ierr);
}

} // namespace

int do_range(xpp::Session &s, double *x, int flag)  /* 0 for 1-param 1 for 2 parameter 2 for Auto range */
{
 if(flag==0||flag==2){
   s.integrator.range.rtype=0;
   if(set_up_range(s)==0)return(-1);
 }
 if(flag==1){
   if(set_up_range2(s)==0)return -1;
 }
 RangeFilm film(s);
 const xpp::Result<int> r=range_sweep(s,x,flag,film.taker(),film.progress());
 if(!r){
   xpp::show_error(r.error());
   return -1;
 }
 if(!film.result())xpp::show_error(film.result().error());
 return *r;
}

void write_equilibrium(xpp::Session &s, const char *name, int shoot)
{
  const int n=s.model().node;
  std::vector<double> x(s.last_ic.begin(),s.last_ic.begin()+n),er(n),em(n);
  int ierr;

  if(!xpp::ok_or_show(do_sing_info(s,x.data(),s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,n,er.data(),em.data(),&ierr)))return;
  if(ierr!=0)return;
  xpp::Writer w=xpp::open_writer_asking(name);
  if(!w)return;
  for(int i=0;i<n;i++)
    w.print("{:g} {:g} {:g}\n",x[i],er[i],em[i]);
  if(!xpp::ok_or_show(xpp::commit_save(w)))return;
  if(shoot)
    save_batch_shoot(s);
}
  
void find_equilib_com(xpp::Session &s, int com)
{
 int ierr;
 float xm,ym;
 int im,jm;
 int iv,jv;
 float stabinfo;
 double *x,oldtrans;

 x=&s.data_store.current[0];
 if(s.numerics.fft||s.numerics.hist||s.model().nkernel>0)return;

 s.numerics.storflag=false;
 s.numerics.poincare_map=PoincareMap::none;
 oldtrans=s.numerics.store_from;
 s.numerics.store_from=0.0;
 evaluate_derived(s); 
 switch(com){
 case 2: 
 do_eq_range(s,x);
 
   return;
  case 1:
    /*  Get mouse values  */
        iv=s.plot_windows.current->xv[0]-1;
        jv=s.plot_windows.current->yv[0]-1;
    if(iv<0||iv>=s.model().node||jv<0||jv>=s.model().node||s.plot_windows.current->grtype>=5||jv==iv){
      command_error("initialconds","Not in a usable 2D plane");
      return;
    }

	 /* get mouse click x,y  */
         get_ic(s,1,x);
	 MessageBox("Click on guess");
	 if(GetMouseXY(s,&im,&jm)){
	   scale_to_real(s,im,jm,&xm,&ym);
	   x[iv]=static_cast<double>(xm);
	   x[jv]=static_cast<double>(ym);
	 }
         
        KillMessageBox();
	 break;
 case 3: monte_carlo(s);
         return;
 case 0:
 default:
        get_ic(s,2,x);
        break;
 }

 xpp::Result<> r;
 if(s.delay.flag){
   r=do_delay_sing(s,x,s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,s.model().node,&ierr,&stabinfo);
   ping();
 }
 else
    r=do_sing(s,x,s.numerics.equilibria.jacobian_step,s.numerics.equilibria.newton_tolerance,s.numerics.equilibria.search_limit,s.numerics.equilibria.max_iterations,s.model().node,&ierr,&stabinfo);
 s.numerics.store_from=oldtrans;
 if(!r)xpp::show_error(r.error());
 
}
 
int write_this_run(xpp::Session &s, const char *file, int i)
{
  if(!s.integrator.suppress_out){
  std::string outfile=xpp::format("{}.{}",file,i);
  if(!save_ready(s.browser.view.iend>s.browser.view.istart))return -1;
  xpp::Writer w=xpp::open_writer_asking(outfile.c_str(),false,nullptr,false,s.output_place);
  if(!w)return -1;
  write_mybrowser_data(s,w);
  xpp::ok_or_show(xpp::commit_save(w));
  }
   if(s.integrator.make_plot_flag)dump_ps(s,i);
  return(1);
}
  
void do_init_data(xpp::Session &s, int com)
{
  char ch;
  int si;
  double *x;
  double old_dt=s.numerics.delta_t;
  std::string icfile;
  float xm,ym;
  int im,jm,oldstart,iv,jv,badmouse;

  oldstart=s.integrator.my_start;
  s.integrator.my_start=1;
  x=&s.data_store.current[0];
  s.integrator.range_flag=0;
  s.integrator.step_error.reset();
  reset_dae(s);
  if(s.numerics.fft||s.numerics.hist)return;

  if(com==M_ID){      /* dont want to wipe out everything! */
    get_new_guesses(s);
    return;
  }

 data_back(s);
  wipe_rep(s.browser);
  s.data_store.current_time=s.numerics.start_time;
 
  s.numerics.storflag=true;
  s.numerics.poiext=0;
  s.data_store.rows=0;
  reset_browser(s);

  switch(com){
  case M_IR: /* do range   */
    do_range(s,x,0);
    return;
  case M_I2:
    do_range(s,x,1);
    return;
  case M_IS:
  case M_IL:
    if(!s.numerics.inflag){
      ping();
      command_error("initialconds","No prior solution");
      return;
    }
    xpp::push_value_undo(s); /* Run from last state overwrites the initial conditions */
    set_initial_data(s,{x,static_cast<size_t>(s.model().node+s.model().nmarkov)});
    if(com==M_IS){
      s.numerics.start_time=s.integrator.last_time;
      s.data_store.current_time=s.numerics.start_time;
    }
    if(xpp::solver_info(s.numerics.method).traits.integral_history&&oldstart==0){
      ch=static_cast<char>(TwoChoice("No","Yes","Reset integrals?","ny"));
      if(ch=='n')s.integrator.my_start=oldstart;
    }
    break;
  case M_IO:
    get_ic(s,1,x);
    if(s.delay.flag){
      /* restart initial data */
      if(!xpp::ok_or_show(do_init_delay(s,s.numerics.delay)))return;
    }
   set_init_guess(s);
    break;
  case M_IM:
  case M_II:
        iv=s.plot_windows.current->xv[0]-1;
        jv=s.plot_windows.current->yv[0]-1;
    if(iv<0||iv>=s.model().node||jv<0||jv>=s.model().node||s.plot_windows.current->grtype>=5||jv==iv){
      command_error("initialconds","Not in a usable 2D plane");
      return;
    }

    /*  Get mouse values  */
    if(com==M_IM){
        get_ic(s,1,x);
	MessageBox("Click on initial data");
	if(GetMouseXY(s,&im,&jm)){
	  scale_to_real(s,im,jm,&xm,&ym);
	  im=s.plot_windows.current->xv[0]-1;
	  jm=s.plot_windows.current->yv[0]-1;
	  x[iv]=static_cast<double>(xm);
	  x[jv]=static_cast<double>(ym);
	  s.last_ic[im]=x[im];
	  s.last_ic[jm]=x[jm];
	  KillMessageBox();
  
	  if(s.delay.flag){
	    /* restart initial data */
	    if(!xpp::ok_or_show(do_init_delay(s,s.numerics.delay)))return;
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
          get_ic(s,1,x);
	  badmouse=GetMouseXY(s,&im,&jm);
	  if(badmouse==0)break;
	  scale_to_real(s,im,jm,&xm,&ym);
	  im=s.plot_windows.current->xv[0]-1;
	  jm=s.plot_windows.current->yv[0]-1;
	  x[iv]=static_cast<double>(xm);
	  x[jv]=static_cast<double>(ym);
	  s.last_ic[im]=x[im];
	  s.last_ic[jm]=x[jm];
	  if(s.delay.flag){
	    /* restart initial data */
	    if(!xpp::ok_or_show(do_init_delay(s,s.numerics.delay)))break;
	  }
          s.integrator.my_start=1;
          s.data_store.current_time=s.numerics.start_time;
	  usual_integrate_stuff(s,x);
	}
	KillMessageBox();
	s.integrator.suppress_bounds=0;
	return;
    }
    break;
  case M_IN:
    man_ic(s); 
    get_ic(s,2,x);
    set_init_guess(s);
    break; 
  case M_IU:
    if(form_ic(s)==0)return;
    get_ic(s,2,x);
    break;
  case M_IH:
    if(s.manifolds.ic_flag==0){
      command_error("initialconds","No shooting data available");
      break;
    }
    si=1;
    new_int(xpp::format("Which? (1-{})",s.manifolds.count),&si);
    si--;
    if(si<s.manifolds.count&&si>=0){
      set_initial_data(s,std::span(s.manifolds.ic[si]).first(s.model().node));
      get_ic(s,2,x);
    }
    else
      command_error("initialconds","Out of range");
    break;
  case M_IF:
    icfile.clear();
    if(!file_selector("Read initial data",icfile,"*.dat"))return;
    /* the initial-conditions file, all of it or nothing (W125) */
    load_ic_file_named(s,icfile);
    get_ic(s,2,x);
    break;
      
  case M_IB:
    s.numerics.delta_t=-fabs(s.numerics.delta_t);
      get_ic(s,2,x);
      set_init_guess(s);
      if(s.delay.flag){
      /* restart initial data */
      if(!xpp::ok_or_show(do_init_delay(s,s.numerics.delay)))return;
    }
      break;
  case M_IG:
  default:
  	
    set_init_guess(s);
    
    get_ic(s,2,x); 
    
    if(s.delay.flag){
      /* restart initial data */
      if(!xpp::ok_or_show(do_init_delay(s,s.numerics.delay)))return;
    }
    break;
  }
usual_integrate_stuff(s,x);
s.numerics.delta_t=old_dt;
}	
void run_now(xpp::Session &s)
{
 
  double *x;
 s.integrator.my_start=1;
 x=&s.data_store.current[0];
 s.integrator.range_flag=0;
 s.integrator.step_error.reset();
 reset_dae(s);
 s.data_store.current_time=s.numerics.start_time;
 get_ic(s,2,x); 
  s.numerics.storflag=true;
  s.numerics.poiext=0;
  s.data_store.rows=0;
  reset_browser(s);
  usual_integrate_stuff(s,x);
 }

void do_start_flags(xpp::Session &s, double *x,double *t)
{
 int iflagstart=1;
 double tnew=*t;
 double sss;
 one_flag_step(s,x,x,&iflagstart,*t,&tnew,s.model().node,&sss);

}
int usual_integrate_stuff(xpp::Session &s, double *x)
{
  int i;

  seed_this_run(s);
  do_start_flags(s,x,&s.data_store.current_time);
   if(stored_from(s.numerics.store_from,s.data_store.current_time,s.numerics.delta_t)&&s.numerics.storflag&&s.numerics.poincare_map==PoincareMap::none)
    {
      s.data_store.col[0][0]=static_cast<float>(s.data_store.current_time);
      extra(s,x,s.data_store.current_time,s.model().node,s.model().neq);
      for(i=0;i<s.model().neq;i++)s.data_store.col[1+i][0]=static_cast<float>(x[i]);
      s.data_store.rows=1;
    }
  if(s.integrator.step_error){
    refresh_browser(s,s.data_store.rows);
    xpp::show_error(take_step_error(s).error());
    return 1;
  }
 
  const xpp::Result<int> r=[&]{
    const xpp::Job job; /* Abort cancels it (xpp_job.h) */
    return integrate(s,&s.data_store.current_time,x,s.numerics.total_time,s.numerics.delta_t,1,s.numerics.store_every,&s.integrator.my_start);
  }();
  
  ping();
  s.numerics.inflag=true;
  do_batch_nclines(s);
  do_batch_dfield(s);
  refresh_browser(s,s.data_store.rows);
  show_stop_note(s);
  if(program.interactive){
 auto_freeze_it(s);
  redraw_ics();
  }
  if(!r){
    xpp::show_error(r.error());
    return 1;
  }
  return *r;
}
/*  form_ic  --  u_i(0) = F(i)  where  "i" is represented by "t"
    or  
    u[5..20]=f([j]) 
*/

namespace {
/* the array IC newic[j1..j2]: the one already used for it, else the first
   free one (the first slot when none is free) made into it */
ArrayIc &array_ic_for(ArrayIcs &ics, const char *newic, int j1, int j2)
{
  int ihot=-1;
  int ifree=-1;
  for(int i=0;i<NAR_IC;i++){
    if(ics.ics[i].index0==-1&&ifree==-1&&ics.ics[i].type==0)
      ifree=i;
    if(ics.ics[i].var==newic&&ics.ics[i].j1==j1&&ics.ics[i].j2==j2)
      ihot=i;
  }
  if(ihot==-1){
    ihot=ifree==-1?0:ifree;
    ics.ics[ihot].var=newic;
    ics.ics[ihot].type=2;
    ics.ics[ihot].j1=j1;
    ics.ics[ihot].j2=j2;
  }
  return ics.ics[ihot];
}
}

void do_new_array_ic(xpp::Session &s, const char *newic, int j1, int j2)
{
  ArrayIc &ic=array_ic_for(s.integrator.array_ics,newic,j1,j2);
  new_string_of("Formula:",ic.formula,XPP_FIELD_EXPRESSION);
  evaluate_ar_ic(s,ic.var.c_str(),ic.formula.c_str(),ic.j1,ic.j2);
}

std::vector<ArrayInitialValue> array_initial_values(const xpp::Model &m)
{
  std::vector<ArrayInitialValue> out;
  if(m.array_ics.defined==0)return out;
  int group=0;
  for(const ArrayIc &ic : m.array_ics.ics){
    if(ic.type!=2)continue;
    group++;
    for(int j=ic.j1;j<=ic.j2;j++){
      ArrayInitialValue v;
      subsk(ic.var,v.var,j,1);
      subsk(ic.formula,v.formula,j,1);
      v.j=j;
      v.group=group;
      out.push_back(std::move(v));
    }
  }
  return out;
}

void evaluate_ar_ic(xpp::Session &s, const char *v, const char *f, int j1, int j2)
{
  int j;
  int i,flag;
  double z;
  std::string vp, fp;
  for(j=j1;j<=j2;j++){
    i=-1;
    subsk(v,vp,j,1);
    find_variable(s,vp.c_str(),&i);
    if(i>0){
      subsk(f,fp,j,1);
      flag=do_calc(s,fp,&z);
      if(flag!=-1)
	s.last_ic[i-1]=z;
      else 
	return;
    }
  }

}
int extract_ic_data(xpp::Model &m, char *big)
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
    array_ic_for(m.array_ics,newic.c_str(),j1,j2).formula=back;
    m.array_ics.defined=1;
  }
  return(1);

}
  
void arr_ic_start(xpp::Session &s)
{
  int i;
  s.integrator.array_ics=s.model().array_ics; /* the run's own, which Initialconds edits */
  const ArrayIcs &ics=s.integrator.array_ics;
  if(ics.defined==0) return;
  for(i=0;i<NAR_IC;i++){
    if(ics.ics[i].type==2){
      evaluate_ar_ic(s,ics.ics[i].var.c_str(),ics.ics[i].formula.c_str(),
		     ics.ics[i].j1,ics.ics[i].j2);
    }
  }

}

int set_array_ic(xpp::Session &s)
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
     do_new_array_ic(s,newic.c_str(),j1,j2);
   }
 else {
   find_variable(s,junk.c_str(),&i);
   if(i<=-1)
     return 0;
   index0=i;
   for(i=0;i<NAR_IC;i++){
     if(s.integrator.array_ics.ics[i].type==2)continue;
     if(index0==s.integrator.array_ics.ics[i].index0){
       myar=i;
       break;
     }
   }
   if(myar<0){
     for(i=0;i<NAR_IC;i++){
       if(s.integrator.array_ics.ics[i].type==2)continue;
       if(s.integrator.array_ics.ics[i].index0==-1){
	 myar=i;
	 break;
       }
     }
   }
   if(myar<0)myar=0;
   
   /* Now we have an element in the array index */
   s.integrator.array_ics.ics[myar].index0=index0;
   s.integrator.array_ics.ics[myar].type=0;
   new_int("Number elements:",&s.integrator.array_ics.ics[myar].n);
   new_string_of("u=F(t-i0):",s.integrator.array_ics.ics[myar].formula,XPP_FIELD_EXPRESSION);
   i1=index0-1;
   in=i1+s.integrator.array_ics.ics[myar].n;
   if(i1>s.model().node||in>s.model().node)return 0; /* out of bounds */
   for(i=i1;i<in;i++){
     set_val(s,"t",static_cast<double>((i-i1)));
     flag=do_calc(s,s.integrator.array_ics.ics[myar].formula,&z);
     if(flag==-1){
       command_error("initialconds",xpp::format("Bad formula {}",s.integrator.array_ics.ics[myar].formula));
       return 1;
     }
     s.last_ic[i]=z;
   }
 }
   return 1;
}

int form_ic(xpp::Session &s)
{
  int ans;
  while(1){
    ans=set_array_ic(s);
    if(ans==0)break;
  }
  return 1;
}

void set_initial_data(xpp::Session &s, std::span<const double> state)
{
  const auto nodes=static_cast<size_t>(s.model().node);
  const auto states=nodes+static_cast<size_t>(s.model().nmarkov);
  if((state.size()!=nodes && state.size()!=states) || s.last_ic.size()<states)
    throw std::length_error("initial data does not match the model state");
  std::copy(state.begin(),state.end(),s.last_ic.begin());
}

void get_ic(xpp::Session &s, int it, double *x)
{
  int i;
  switch(it){
  case 1:
  case 2:
    for(i=0;i<s.model().node+s.model().nmarkov;i++)x[i]=s.last_ic[i];
    break;
   }
}

xpp::Result<> ode_int(xpp::Session &s, double *y, double *t, int *istart, int ishow)
{
 xpp::Solver &solver=*s.integrator.solver;
 int nodes=s.solver_work.xpv.node+s.solver_work.xpv.nvec;
 int nit,store_every=s.numerics.store_every;
 double tend=s.numerics.total_time;
 double dt=s.numerics.delta_t;
  if(solver.traits().discrete){
 nit=tend;
 dt=dt/fabs(dt);
 }
 else nit=(tend+.1*fabs(dt))/fabs(dt);
 if(ishow==1){
 /* shown as it runs: a failure only ends the integration, the shooting
    goes on from where it stopped (the error is the caller's to show) */
 return integrate(s,t,y,tend,dt,1,store_every,istart).transform([](int){});
}
 mswtch(s,s.solver_work.xpv.x,y);
 evaluate_derived(s); 
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
 xpp::Result<> r=solver.advance(step);
 mswtch(s,y,s.solver_work.xpv.x);
 if(!r){
   ping();
   return r;
 }
 solver.finish();
 return {};
}

namespace {
/* The digits a state shows at `decimals` places, kept as text in a
   stack buffer (std::to_chars: no allocation). A formatted signed zero has
   the same numeric value as positive zero. A value too long for the
   buffer (|x| near 1e308 with 15 decimals fits: 309+1+15+1) cannot occur. */
SteadyDigits steady_digits(double value, int decimals)
{
  SteadyDigits d;
  const auto r=std::to_chars(d.text.data(),d.text.data()+d.text.size(),xpp::canonical_nan(value),std::chars_format::fixed,decimals);
  d.size=static_cast<unsigned short>(r.ptr-d.text.data());
  const std::string_view text(d.text.data(),d.size);
  if(text.starts_with('-')&&text.find_first_not_of("0.",1)==std::string_view::npos){
    std::memmove(d.text.data(),d.text.data()+1,d.size-1);
    --d.size;
  }
  return d;
}
}

void xpp::SteadyStateMonitor::begin(std::span<const double> state, double time)
{
  previous_.clear();
  for(double value:state)previous_.push_back(steady_digits(value,settings_.decimals));
  previous_time_=time;
  unchanged_=0;
}

bool xpp::SteadyStateMonitor::observe(std::span<const double> state, double time)
{
  bool same=state.size()==previous_.size()&&!state.empty()&&std::isfinite(time)&&time>previous_time_;
  next_.clear();
  for(std::size_t i=0;i<state.size();++i){
    next_.push_back(steady_digits(state[i],settings_.decimals));
    if(!std::isfinite(state[i])||i>=previous_.size()||!(next_.back()==previous_[i]))same=false;
  }
  unchanged_=same?unchanged_+(time-previous_time_):0;
  previous_.swap(next_);
  previous_time_=time;
  return same&&unchanged_>=settings_.hold;
}

Result<> validate_steady_state(SteadyStateSettings settings, double dt, double start)
{
  if(settings.decimals<0||settings.decimals>MAX_STEADY_DECIMALS)
    return fail("steady",format("Decimal places must be between 0 and {}",MAX_STEADY_DECIMALS),command_place());
  if(!std::isfinite(dt)||dt<=0||!std::isfinite(start)||!std::isfinite(start+dt)||start+dt<=start)
    return fail("steady","Dt must advance time with a finite positive step",command_place());
  if(!std::isfinite(settings.hold)||!std::isfinite(settings.maximum)||settings.hold<dt||settings.maximum<settings.hold||
     !std::isfinite(start+settings.maximum)||settings.maximum/dt>MAX_RUN_STEPS)
    return fail("steady","Hold duration must be at least Dt, maximum duration must cover the hold, and the step count must fit the integrator",command_place());
  return {};
}

Result<> continue_to(Session &s, double until)
{
  /* one output group: store_every steps of Dt (of 1, for a map); the run ends on a group boundary,
     so the end is rounded up to the output grid and the stride is honoured */
  const double interval=(s.integrator.solver->traits().discrete?1:s.numerics.delta_t)*s.numerics.store_every;
  const double duration=until-s.data_store.current_time;
  if(!s.numerics.inflag||s.numerics.fft||s.numerics.hist)
    return fail("continue","Needs a prior trajectory; disable FFT and histogram modes",command_place());
  if(!std::isfinite(until)||!std::isfinite(duration)||duration<=0||!std::isfinite(interval)||interval<=0||
     s.numerics.delta_t<=0||duration/interval>MAX_RUN_STEPS)
    return fail("continue","End time must be beyond the current time with a positive Dt and a step count that fits the integrator",command_place());
  /* Round up to the next grid point: never short of the end asked for. */
  const double length=std::max(std::ceil(duration/interval-CONTINUE_GRID_TOLERANCE),1.0)*interval;
  wipe_rep(s.browser);
  data_back(s);
  s.integrator.my_start=1;  /*  I know it is wasteful to restart, but lets be safe.... */
  const xpp::Result<int> r=integrate(s,&s.data_store.current_time,&s.data_store.current[0],length,s.numerics.delta_t,1,s.numerics.store_every,&s.integrator.my_start);
  ping();
  do_batch_nclines(s);
  do_batch_dfield(s);
  refresh_browser(s,s.data_store.rows);
  show_stop_note(s);
  return r.transform([](int){});
}

Result<> continue_for(Session &s, double extra)
{
  return continue_to(s,s.data_store.current_time+extra);
}

Result<> continue_total(Session &s)
{
  return continue_for(s,s.numerics.total_time);
}

Result<> run_to_steady_state(Session &s, SteadyStateSettings settings)
{
  const double interval=s.integrator.solver->traits().discrete?(s.numerics.delta_t>0?1:-1):s.numerics.delta_t;
  if(auto valid=validate_steady_state(settings,interval,s.numerics.start_time);!valid)return valid;
  if(s.model().node+s.model().nmarkov==0||s.numerics.poincare_map!=PoincareMap::none||s.numerics.fft||s.numerics.hist)
    return fail("steady","Needs model states and ordinary trajectory output (disable Poincare, FFT and histogram modes)",command_place());
  if(!s.integrator.solver->traits().fixed_step&&interval<std::fabs(s.numerics.min_step))
    return fail("steady","Dt must be at least the solver's minimum step",command_place());
  /* One core comparison per configured Dt, regardless of output stride.
     Restore the user's duration/stride before deferred settings apply. */
  struct Restore {
    Session &s;
    double duration;
    int stride;
    ~Restore(){s.numerics.total_time=duration;s.numerics.store_every=stride;s.integrator.steady.reset();}
  } restore{s,s.numerics.total_time,s.numerics.store_every};
  s.numerics.total_time=std::floor(settings.maximum/interval)*interval;
  s.numerics.store_every=1;
  s.integrator.steady.emplace(settings);
  s.integrator.steady_result=SteadyStateResult{"failed",settings.decimals};
  s.integrator.last_time=s.numerics.start_time;
  do_init_data(s,M_IG);
  s.integrator.steady_result->time=s.integrator.last_time;
  return {};
}

std::unexpected<xpp::Error> take_step_error(xpp::Session &s)
{
  xpp::Error e=std::move(*s.integrator.step_error);
  s.integrator.step_error.reset();
  reset_dae(s);
  return std::unexpected(std::move(e));
}

namespace {
/* A run a failed exact Jacobian stopped (W260) is offered with what goes
   on from there: the commands that switch the Jacobian to differences
   and continue it to the end it was going to (end), when it went forward
   (Continue's own condition); nothing switches without them. */
xpp::Error stopped(xpp::Error e, double end, double dt)
{
  if(e.where!=xpp::JACOBIAN_FAILED||dt<=0)return e;
  e.action=xpp::switch_to_differences(xpp::format(R"({{"cmd":"continue","until":{}}})",xpp::number(end)));
  return e;
}
}

namespace {
/* what a global event that set arret says: the time, the condition and its line */
std::string halt_note(const xpp::Session &s, int flag, double t)
{
  const xpp::Place where=xpp::model_place(s.model(),odex::Statement::Kind::Event,flag);
  std::string text=xpp::format("Stopped at t = {}: the global event \"{}\" set arret",t,s.model().flags[flag].cond);
  if(where.line>0)text+=xpp::format(" (model line {})",where.line);
  return text;
}

std::string section_note(double t)
{
  return xpp::format("Stopped at the section crossing t = {} (poincare_stop = true)",t);
}
}

xpp::Result<int> integrate(xpp::Session &s, double *t, double *x, double tend, double dt, int count, int store_every, int *start)
{
  if(!s.integrator.steady)s.integrator.steady_result.reset();
  s.integrator.stop_note.clear();
  xpp::Computation computing; /* what Escape stops (xpp_job.h) */
  const DaeRun dae_run(s); /* the run follows one branch of a DAE's solutions */
  xpp::Solver &solver=*s.integrator.solver;
  if(auto limits=xpp::check_step_limits(s,solver.info(),dt,tend);!limits)return std::unexpected(limits.error());
  if(auto from=xpp::check_store_from(s,*t,dt,tend);!from)return std::unexpected(from.error());

 /* an added column (add_stor_col) plotted live has index above neq and no live value: it reads 0 */
 const std::size_t neq_size=static_cast<std::size_t>(s.model().neq);
 std::vector<float> xv_values(neq_size+1+xpp::DataStore::ADDED_COLUMNS),xvold_values(neq_size+1+xpp::DataStore::ADDED_COLUMNS);
 float *const xv=xv_values.data(),*const xvold=xvold_values.data();
 float oldperiod=0.0;
 std::vector<double> xprime_values(neq_size),oldxprime_values(neq_size);
 double *const xprime=xprime_values.data(),*const oldxprime=oldxprime_values.data(),hguess=dt;

 std::vector<int> torcross_values(neq_size);
 int *const torcross=torcross_values.data();
 int nodes=s.solver_work.xpv.node+s.solver_work.xpv.nvec-s.model().nmarkov;

 const double section_start=*t;
 bool section_crossed=false;
 int rval=0;
 std::optional<xpp::Error> failure; /* why the loop stopped, when it failed */
 std::vector<double> oldx_values(neq_size);
 double *const oldx=oldx_values.data(),oldt=0,dint,dxp,sect,sect1,tout=*t,tzero=*t;
 double sss,tnew=*t;
 int iflagstart=1;
 float tscal=tend,tv;
 
 char esc;
 int ieqn,i,pflag=0;
 int icount=0;
 int nit;
 /* a steady-state run needs only the last values, so it is never bounded by
    storage: when the store fills it keeps every second row and stores every
    second event from then on */
 int steady_stride=1,steady_seen=0;
 int cwidth=0;
 /* new poincare map stuff */

mswtch(s,s.solver_work.xpv.x,x);

if(program.interactive) cwidth=get_command_width();

 s.integrator.last_time=*t;
 evaluate_derived(s);
 if(s.integrator.step_error)return take_step_error(s);

 solver.begin(start);
 if(solver.traits().discrete){
 nit=tend;
 dt=dt/fabs(dt);
 }
 else nit=(tend+fabs(dt)*.1)/fabs(dt);
 /* else nit=tend/fabs(dt); */
 nit=(nit+store_every-1)/store_every;
 if(nit==0)return(rval);
 one_flag_step(s,s.solver_work.xpv.x,s.solver_work.xpv.x,&iflagstart,*t,&tnew,nodes,&sss);
 mswtch(s,x,s.solver_work.xpv.x);
 extra(s,x,*t,s.model().node,s.model().neq); /* Note this takes care of initializing Markov variables */
 if(s.integrator.step_error)return take_step_error(s);
  mswtch(s,s.solver_work.xpv.x,x);
 if(s.integrator.steady)s.integrator.steady->begin(std::span(x,static_cast<std::size_t>(s.model().node+s.model().nmarkov)),*t);
 xv[0]=static_cast<float>(*t);
 for(ieqn=1;ieqn<=s.model().neq;ieqn++)xv[ieqn]=static_cast<float>(x[ieqn-1]);
 if(s.animation.options.on_the_fly)on_the_fly(s,1); 
   
 if(s.numerics.poincare_map!=PoincareMap::none)
 {
 oldt=*t;
 for(ieqn=0;ieqn<s.model().neq;ieqn++)oldx[ieqn]=x[ieqn];
 }
 if(dt<0.0)tscal=-tend;
 if(tscal==0.0)tscal=1.0;
 stor_delay(s,x);
 /* xppautX: the rows a cancel before the first step finds (xpp_job.h) */
 xpp::job::report_rows(s.data_store.rows, s.data_store.rows > 0 ? s.data_store.col[0][s.data_store.rows-1] : *t);

 /* the variables' last two values up to ieqn, then the rest, at DEBUG: what
    --debug shows about a state that stopped the run */
 const auto trace_states=[&](int ieqn){
   xpp::log(XPP_LOG_DEBUG, "variable\tf(t-1)\tf(t) \n");
   int k=1;
   for(;k<=ieqn;k++)
     xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",s.model().uvar_names[k-1],xvold[k],xv[k]);
   for(;k<=s.model().neq;k++)
     xpp::log(XPP_LOG_DEBUG, " {}\t{:g}\t{:g}\n",s.model().uvar_names[k-1],xv[k],static_cast<float>(x[k-1]));
 };

 while(1)
 {
	
	   if(!solver.traits().fixed_step){
	     /* on to the next output time, in steps of the method's own */
	     tout=tzero+dt*(icount+1);
	     mswtch(s,s.solver_work.xpv.x,x);
	     xpp::Result<> r=solver.advance({.y=s.solver_work.xpv.x,.t=t,.neq=nodes,.start=start,
						 .tout=tout,.hguess=&hguess,.sections=true});
	     mswtch(s,x,s.solver_work.xpv.x);
	     if(!solver.traits().dense_output||*t==tout)stor_delay(s,x);
	     if(s.integrator.step_error){
	       s.integrator.last_time=*t;
	       return take_step_error(s);
	     }
	     if(!r){
	       ping();
	       s.integrator.last_time=*t;
	       return std::unexpected(stopped(r.error(),tzero+tend,dt));
	     }
	   }
	   else{
	     /* store_every steps of dt */
	     mswtch(s,s.solver_work.xpv.x,x);
	     xpp::Result<> r=solver.advance({.y=s.solver_work.xpv.x,.t=t,.neq=nodes,.start=start,
						 .dt=dt,.steps=store_every});
	     mswtch(s,x,s.solver_work.xpv.x);
	     if(!r){
	       ping();
	       s.integrator.last_time=*t;
	       return std::unexpected(stopped(r.error(),tzero+tend,dt));
	     }
	   }
	   /*   START POST INTEGRATE STUFF */           

	   extra(s,x,*t,s.model().node,s.model().neq);

          if (s.numerics.torus) {
	for (ieqn = 0; ieqn < s.model().neq; ieqn++) {
	        torcross[ieqn]=0;
		if (s.itor[ieqn] == 1) {
			if (x[ieqn] > s.numerics.torus_period) {
				x[ieqn] -= s.numerics.torus_period;
                                torcross[ieqn]=-1;
			}
			if (x[ieqn] < 0) {
				x[ieqn] += s.numerics.torus_period;
                                torcross[ieqn]=1;
			}
		      }
	      }
      }
	   xvold[0]=xv[0];
           for(ieqn=1;ieqn<(s.model().neq+1);ieqn++)
           {
	    xvold[ieqn]=xv[ieqn];
	    xv[ieqn]=static_cast<float>(x[ieqn-1]);
	/* trap NaN using isnan() in math.h 
	   modified the out of bounds message as well
	   print all the variables on the terminal window, haven't decide
	   should I store them or not. 
	   If use with store_every=1, can pinpoint the offensive variable(s)
	*/	    
	    if(isnan(x[ieqn-1])!=0)
            {
             trace_states(ieqn);
             failure=xpp::Error{"integration",xpp::format("{} is NaN at t = {}",s.model().uvar_names[ieqn-1],*t),xpp::model_place(s.model(),s.model().uvar_names[ieqn-1])};
             break;
             }
       /* end of NaN */     
            if(s.numerics.stop_at_limit&&fabs(x[ieqn-1])>s.numerics.state_limit)
            {
             const std::string error_message=xpp::format("{} out of bounds at t = {} (state_limit = {})",
             s.model().uvar_names[ieqn-1],*t,xpp::number(s.numerics.state_limit));
	     if(s.integrator.range_flag||s.integrator.suppress_bounds){
	       s.integrator.stop_note=error_message; /* the run ends, its sweep or drawing goes on */
	       break;
	     }
             trace_states(ieqn);
             failure=xpp::Error{"integration",error_message,xpp::model_place(s.model(),s.model().uvar_names[ieqn-1])};
             break;
            }
           }
	                
        /*   This is where the progresser goes   */
	   if(program.interactive) plot_command(nit,icount,cwidth);
	   esc=my_abort();

           {
            
             if(esc==ESC){if(s.integrator.steady)s.integrator.steady_result->status="stopped";break;}
	     if(esc=='/'){rval=1;s.numerics.endsing=true;if(s.integrator.steady)s.integrator.steady_result->status="stopped";break;}
	    
           }
	if(s.integrator.stop_flag==1){
	  s.integrator.stop_flag=0;
	  if(s.integrator.halt_flag>=0)s.integrator.stop_note=halt_note(s,s.integrator.halt_flag,*t);
	  s.integrator.halt_flag=-1;
	  if(s.integrator.steady)s.integrator.steady_result->status="stopped";
	  break;
	}
           if(s.integrator.step_error){
             s.numerics.endsing=true;
             xpp::Error e=take_step_error(s).error();
             if(!failure)failure=std::move(e); /* a variable's NaN came first */
             break;
           }
           if(ieqn<(s.model().neq+1))break;
           ui.live_state(s, x, *t);
           tv=static_cast<float>(*t);
	   xv[0]=tv;
 if(solver.traits().dense_output&&s.numerics.poincare_map!=PoincareMap::none){
   pflag=solver.section_found()?1:0;
   if(!pflag)goto out;
   goto dense_poi;
 }
 if((s.numerics.poincare_map==PoincareMap::max)&&!(s.numerics.poincare_variable==0))
 {
  pflag=0;
  if((oldx[s.numerics.poincare_variable-1]<x[s.numerics.poincare_variable-1])&&!(s.numerics.poiext<0))s.numerics.poiext=1;
  if((oldx[s.numerics.poincare_variable-1]>x[s.numerics.poincare_variable-1])&&!(s.numerics.poiext>0))s.numerics.poiext=-1;
  if(  ( !(oldx[s.numerics.poincare_variable-1]<x[s.numerics.poincare_variable-1]) && (s.numerics.poiext>0) )||
       ( !(oldx[s.numerics.poincare_variable-1]>x[s.numerics.poincare_variable-1]) && (s.numerics.poiext<0) )
    )
  {
     if(static_cast<int>(s.numerics.poincare_sign)*s.numerics.poiext>=0)
      {
	/*  We will interpolate to get a good local extremum   */
	
	s.integrator.rhs(*t,x,xprime,s.model().neq);
	s.integrator.rhs(oldt,oldx,oldxprime,s.model().neq);
        dxp=xprime[s.numerics.poincare_variable-1]-oldxprime[s.numerics.poincare_variable-1];
        if(dxp==0.0)
	  return xpp::fail("Poincare map","Cannot zero RHS for max/min - use a variable",command_place());
	dint=xprime[s.numerics.poincare_variable-1]/dxp;

	tv=(1-dint)**t+dint*oldt;
	xv[0]=tv;
	for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
	pflag=1;
        
      }
      s.numerics.poiext=-s.numerics.poiext;
   }
  goto poi;
 }

 if(s.numerics.poincare_map==PoincareMap::section||s.numerics.poincare_map==PoincareMap::period)
 {
    if(s.numerics.poincare_variable==0)

     {
     sect1=fmod(fabs(oldt),fabs(s.numerics.poincare_plane));
     sect=fmod(fabs(*t),fabs(s.numerics.poincare_plane));
     if(sect<sect1)
     {
     dint=sect/(s.numerics.poincare_plane+sect-sect1);
     i=static_cast<int>((fabs(*t)/fabs(s.numerics.poincare_plane)));
     tv=static_cast<float>(s.numerics.poincare_plane)*i;
     xv[0]=tv;
     for(i=1;i<=s.model().neq;i++)xv[i]=static_cast<float>((dint*oldx[i-1]+(1-dint)*x[i-1]));
     pflag=1;
     }
     else pflag=0;
    }

    else

    {
     if(s.numerics.poincare_sign!=SectionSign::negative)
     {
     if((oldx[s.numerics.poincare_variable-1]<s.numerics.poincare_plane)&&!(x[s.numerics.poincare_variable-1]<s.numerics.poincare_plane))
     {
      dint=(x[s.numerics.poincare_variable-1]-s.numerics.poincare_plane)/(x[s.numerics.poincare_variable-1]-oldx[s.numerics.poincare_variable-1]);
      tv=(1-dint)**t+dint*oldt;
      xv[0]=tv;
      for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
      pflag=1;
      goto poi;

     }
     else pflag=0;
     }
    if(s.numerics.poincare_sign!=SectionSign::positive)
     {
       if((oldx[s.numerics.poincare_variable-1]>s.numerics.poincare_plane)&&!(x[s.numerics.poincare_variable-1]>s.numerics.poincare_plane))
       {
        dint=(x[s.numerics.poincare_variable-1]-s.numerics.poincare_plane)/(x[s.numerics.poincare_variable-1]-oldx[s.numerics.poincare_variable-1]);
        tv=(1-dint)**t+dint*oldt;
        xv[0]=tv;
        for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
        pflag=1;
       }
       else pflag=0;
     }
    }
poi:    for(i=0;i<s.model().neq;i++)oldx[i]=x[i];
    oldt=*t;
    if(pflag==0)goto out;
 }

dense_poi:
 if(pflag==1)section_crossed=true;
/*	   Plotting and storing data      */
 if(s.numerics.poincare_map==PoincareMap::period&&pflag==1){
   if(oldperiod==0.0){
     pflag=0; /* this is the first hit !! */
     oldperiod=*t;
     goto out;
   }
   xv[0]=*t-oldperiod;
   oldperiod=*t;
 }
     
          if(stored_from(s.numerics.store_from,*t,dt)&&program.interactive&&OnTheFly)
	  {
	     plot_the_graphs(s,xv,xvold,s.model().node,s.model().neq,fabs(dt*s.numerics.store_every),torcross,0); 

	  }

	   if((s.numerics.storflag)&&(count!=0)&&(s.data_store.rows<s.data_store.max_rows)&&stored_from(s.numerics.store_from,*t,dt)
	      &&(!s.integrator.steady||++steady_seen%steady_stride==0))
	   {
           if(s.animation.options.on_the_fly)on_the_fly(s,0);
           for(ieqn=0;ieqn<=s.model().neq;ieqn++)
		 s.data_store.col[ieqn][s.data_store.rows]=xv[ieqn];
	    s.data_store.rows++;
	    row_stored(s); /* xppautX: replay stops here, a front end shows the run grow */
	    if(!(s.data_store.rows<s.data_store.max_rows)){
            if(s.integrator.steady){
              s.data_store.thin(s.model().neq+1);
              steady_stride*=2;
              steady_seen=0;
            }
            else {
              const auto grown=stor_full(s);
              if(!grown){stop_integration(s,grown.error());failure=take_step_error(s).error();break;}
            }
            }
	    if((pflag==1)&&(s.numerics.poincare_stop)){s.integrator.stop_note=section_note(*t);break;}
	   }

out:
           if(solver.section_found()&&s.numerics.poincare_stop){s.integrator.stop_note=section_note(*t);break;}
           if(!solver.traits().dense_output||*t==tout)icount++;
           if(s.integrator.steady&&xpp::job::cancelled()){
             s.integrator.steady_result->status="stopped";
             break;
           }
           if(s.integrator.steady&&s.integrator.steady->observe(std::span(x,static_cast<std::size_t>(s.model().node+s.model().nmarkov)),*t)){
             s.integrator.steady_result->status="settled";
             break;
           }
           if(s.integrator.steady&&icount>=nit)s.integrator.steady_result->status="limit";
           if(icount>=nit&&count!=0)break;

	   /* END POST INTEGRATE ANALYSIS  */
 }
 
       s.integrator.last_time=*t;
       solver.finish();
       if(failure)return std::unexpected(std::move(*failure));
       if(s.numerics.poincare_map!=PoincareMap::none && !section_crossed &&
          s.integrator.stop_note.empty() && !xpp::job::cancelled()){
         s.integrator.stop_note=xpp::format("No crossing found ({}) in [{}, {}]; change the section or integrate longer",
             xpp::named_settings(s,{"poincare_map","poincare_variable","poincare_plane"}),section_start,*t);
       }
       return(rval);
  }
void send_halt(xpp::Session &s, int flag)
{
  s.integrator.stop_flag=1;
  s.integrator.halt_flag=flag;
}
void send_output(xpp::Session &s, double *y,double t)
{
  std::vector<double> yy(s.model().neq);
  int i;
  for(i=0;i<s.model().node;i++)
    yy[i]=y[i];
  extra(s,yy.data(),t,s.model().node,s.model().neq);
  if((s.numerics.storflag)&&(s.data_store.rows<s.data_store.max_rows)){
    
    for(i=0;i<s.model().neq;i++)
      s.data_store.col[i+1][s.data_store.rows]=static_cast<float>(yy[i]);
    s.data_store.col[0][s.data_store.rows]=static_cast<float>(t);
    s.data_store.rows++;
    row_stored(s);
  }
}

  /* the segment or the point of curve ip, in the pen its colour and style make */
static void draw_curve_step(xpp::Session &s, int ip, float oldx, float oldy, float oldz, float x, float y, float z)
{
  const xpp::LineStyle &style=s.plot_windows.current->style[ip];
  const bool flat=s.plot_windows.current->ThreeDFlag==0;
  if(style.draw!=xpp::LineDraw::points){
    if(flat)line_abs(s,oldx,oldy,x,y);
    else line_3d(s,oldx,oldy,oldz,x,y,z);
  }
  if(style.draw!=xpp::LineDraw::line){
    if(flat)point_abs(s,x,y);
    else point_3d(s,x,y,z);
  }
}

void  do_plot(xpp::Session &s, float *oldxpl, float *oldypl, float *oldzpl, float *xpl, float *ypl, float *zpl)
{
	int ip,np=s.plot_windows.current->nvars;
        
        for(ip=0;ip<np;ip++){
           if(s.plot_windows.current->ColorFlag==0)
	     xpp::set_pen(s,s.plot_windows.current->color[ip],s.plot_windows.current->style[ip]);
           else xpp::set_pen_style(s,s.plot_windows.current->style[ip]);
           draw_curve_step(s,ip,oldxpl[ip],oldypl[ip],oldzpl[ip],xpl[ip],ypl[ip],zpl[ip]);
       }
}

/*
 old restore is in restore.c

*/

void plot_the_graphs(xpp::Session &s, float *xv,float *xvold,int node,int neq,double ddt,int *tc,int flag)
{
 for_each_shown_window(s,flag,[&]{plot_one_graph(s,xv,xvold,node,neq,ddt,tc);});
}

void plot_one_graph(xpp::Session &s, float *xv,float *xvold,int node,int neq,double ddt,int *tc)
{
 int *xvar,*yvar,*zvar;
 int NPlots,ip;
 auto &points=s.plot_windows.current->plot_points;
 float *oldxpl=points[0].data(), *oldypl=points[1].data(), *oldzpl=points[2].data();
 float *xpl=points[3].data(), *ypl=points[4].data(), *zpl=points[5].data();
 NPlots=s.plot_windows.current->nvars;
 xvar=s.plot_windows.current->xv.data();
 yvar=s.plot_windows.current->yv.data();
 zvar=s.plot_windows.current->zv.data();
 for(ip=0;ip<s.model().neq;ip++){
   if(s.itor[ip]==1)
     xvold[ip+1]=xvold[ip+1]+tc[ip]*s.numerics.torus_period;
 }
 for(ip=0;ip<NPlots;ip++){
 oldxpl[ip]=xvold[xvar[ip]];
 oldypl[ip]=xvold[yvar[ip]];
 oldzpl[ip]=xvold[zvar[ip]];
 xpl[ip]=xv[xvar[ip]];
 ypl[ip]=xv[yvar[ip]];
 zpl[ip]=xv[zvar[ip]];
 }
 if(s.plot_windows.current->ColorFlag)
   comp_color(s,xv,xvold,s.model().node,static_cast<float>(ddt));
 do_plot(s,oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
 phase_data_flow_step(s,NPlots,oldxpl,oldypl,xpl,ypl,s.plot_windows.current->color.data()); /* Dir.field/flow's Flow as data */
}
void restore(xpp::Session &s, int i1, int i2)
{
  int ip,np=s.plot_windows.current->nvars;
  int ZSHFT,YSHFT,XSHFT;
  int i,j,kxoff,kyoff,kzoff;
  int iiXPLT,iiYPLT,iiZPLT;
  float oldxpl,oldypl,oldzpl,xpl,ypl,zpl;
  std::vector<float> v1(s.model().neq+1),v2(s.model().neq+1);
  float **data;

  data=get_browser_data(s);
  XSHFT=s.plot_windows.current->xshft;
  YSHFT=s.plot_windows.current->yshft;
  ZSHFT=s.plot_windows.current->zshft;
  if(i1<ZSHFT)i1=ZSHFT;
  if(i1<YSHFT)i1=YSHFT;
  if(i1<XSHFT)i1=XSHFT;
  if(s.data_store.rows<2)return;

   for(ip=0;ip<np;ip++){
     xpp::picture_group(s, true);
     kxoff=i1-XSHFT;
     kzoff=i1-ZSHFT;
     kyoff=i1-YSHFT;

    iiXPLT=s.plot_windows.current->xv[ip];
    iiYPLT=s.plot_windows.current->yv[ip];
    iiZPLT=s.plot_windows.current->zv[ip];
    xpp::set_pen(s,s.plot_windows.current->color[ip],s.plot_windows.current->style[ip]);
    oldxpl=data[iiXPLT][kxoff];
    oldypl=data[iiYPLT][kyoff];
    oldzpl=data[iiZPLT][kzoff];
    for(i=i1;i<i2;i++){
      {
	xpl=data[iiXPLT][kxoff];
	ypl=data[iiYPLT][kyoff];
	zpl=data[iiZPLT][kzoff];
      }
      
      if(s.numerics.torus)
      {
	if (fabs(oldxpl-xpl)>static_cast<float>((.5*s.numerics.torus_period)))oldxpl=xpl;
	if (fabs(oldypl-ypl)>static_cast<float>((.5*s.numerics.torus_period)))oldypl=ypl;
	if (fabs(oldzpl-zpl)>static_cast<float>((.5*s.numerics.torus_period)))oldzpl=zpl;
      }
      if(s.plot_windows.current->ColorFlag!=0&&i>i1){
	  for(j=0;j<=s.model().neq;j++){
	    v1[j]=data[j][i];
	    v2[j]=data[j][i-1];
	  }

	  comp_color(s,v1.data(),v2.data(),s.model().node,
		     static_cast<float>(fabs(data[0][i]-data[0][i+1])));
	}
      draw_curve_step(s,ip,oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
     /*noplot:*/
      oldxpl=xpl;
      oldypl=ypl;
      oldzpl=zpl;
      kxoff++;
      kyoff++;
      kzoff++;
     
    }
    xpp::picture_group(s, false);
    
  }
}

/*  Sets the color according to the velocity or z-value */
void comp_color(xpp::Session &s, float *v1, float *v2, int n, float dt)
{
 int i;
 float sum;
 float min_scale=static_cast<float>((s.plot_windows.current->min_scale));
 float color_scale=static_cast<float>((s.plot_windows.current->color_scale));
 if(s.plot_windows.current->ColorFlag==2){
   sum=v1[s.plot_windows.current->ColorValue];
 }
 else
   {
     for(i=0,sum=0.0;i<n;i++)sum+=static_cast<float>(fabs(static_cast<double>((v1[i+1]-v2[i+1]))));
     sum=sum/(dt);
   }
 xpp::set_pen_color(s,xpp::ramp_color(s.ramp(),static_cast<double>((sum-min_scale)/color_scale)));
}

/* an invariant set's branch ends where it leaves the box (state_limit) or an
   event stops it: said once, at INFO, since drawing the set means that */
static void log_branch_end(xpp::Session &s)
{
  if(s.integrator.stop_note.empty())return;
  xpp::log(XPP_LOG_INFO,"Invariant set branch ended: {}\n",s.integrator.stop_note);
  s.integrator.stop_note.clear();
}

xpp::Result<> shoot_easy(xpp::Session &s, double *x)
{
  double t=0.0;
  int i;
  s.integrator.suppress_bounds=1;
  const xpp::Result<int> r=integrate(s,&t,x,s.numerics.total_time,s.numerics.delta_t,1,s.numerics.store_every,&i);
  s.integrator.suppress_bounds=0;
  log_branch_end(s);
  return r.transform([](int){});
}

xpp::Result<> shoot(xpp::Session &s, double *x, double *xg, double *evec, int sgn)
{
 int i;
 double t=0.0;
 s.integrator.suppress_bounds=1;
 for(i=0;i<s.model().node;i++)
 x[i]=xg[i]+sgn*evec[i]*s.numerics.delta_t*.1;
i=1;
 const xpp::Result<int> r=integrate(s,&t,x,s.numerics.total_time,s.numerics.delta_t,1,s.numerics.store_every,&i);
 ping();
  s.integrator.suppress_bounds=0;
 log_branch_end(s);
 return r.transform([](int){});
}

void stop_integration(xpp::Session &s, xpp::Error why)
{
  std::optional<xpp::Error> &e=s.integrator.step_error;
  if(!e)e=std::move(why);
}

xpp::Result<> stor_full(xpp::Session &s)
{
 if(s.data_store.max_rows>INT_MAX/2)
   return xpp::fail("storage","Cannot allocate sufficient storage",command_place());
 const int nrow=2*s.data_store.max_rows;
 const auto grown=s.data_store.grow(s.model().neq+1,nrow);
 if(!grown)return grown;
 s.data_store.max_rows=nrow;
 return {};
}

} // namespace xpp
