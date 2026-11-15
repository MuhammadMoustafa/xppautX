#include <charconv>
#include <cstring>
#include "mykeydef.h"
#include "value_undo.h"
#include "solver.h"
#include "xpp_math.h"
#include "model.h"
#include "ode_read.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "typed_fields.h"
#include "session_values.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "xpp_path.h"
#include "model_place.h"
#include "integrate.h"
#include "jacobian.h"

#include "storage.h"

#include "expr.h"
#include "markov.h"
#include "tabular.h"
#include "derived.h"
#include "flags.h"


#include "dae_fun.h"
#include "numerics.h"
#include "model_options.h"
#include "model_numerics.h"
#include "my_rhs.h" /* extra */
#include "model_native.h"
#include "volterra2.h"
#include <stdlib.h> 
#include <cmath>
#include "ani_state.h"
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
#include <new>
#include <stdexcept>
#include "xpp_job.h"
#include "plot_state.h"
#include "colormap.h"
#include "session.h"
#include "line_style.h"
#include "odex.h"
#include "struct.h"

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
    scan([&](double t){state(t,scratch);set_fix_rhs(s,t,scratch.data());return evaluate(s.parser,flag.comcond);},
         flag.sign,i,false,-1,flag.nointerp!=0);
  }
  const auto &num=*s.numerics;
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
          s.integrator->rhs(t,scratch.data(),derivative.data(),s.model().node);
          return derivative[index];
        }
        if(index>=s.model().node)extra(s,scratch.data(),t,s.model().node,s.model().neq);
        return scratch[index]-num.poincare_plane;
      },num.poincare_map==PoincareMap::max?-static_cast<int>(num.poincare_sign)*static_cast<int>(direction):static_cast<int>(num.poincare_sign),-1,true,-1,false);
    }
  }
  if(s.numerics->stop_at_limit&&!(s.integrator->range_flag||s.integrator->suppress_bounds)){
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
   the job here (xpp_job.h), and a front end may show the run growing (a sweep's worker does neither: the main thread
   reports the rows it takes in) */
void row_stored(xpp::Session &s)
{
  if(s.integrator->on_worker)return;
  const xpp::DataStore &d=*s.data_store;
  xpp::job::report_rows(d.rows, d.col[0][d.rows-1]);
  rows_stored(s,d.rows);
}
/* the state x of the Session's solver work copied into u from v (its node ODEs) */
/* mswtch: XPPAUT 8.0 integrate.c:81: MSWTCH(u,v) is a memcpy of the node values */
static void copy_state(const xpp::Session &s, double *u, const double *v)
{
  memcpy(u,v,s.solver_work->xpv.node*sizeof(double));
}



#define PARAM 1
#define IC 2


constexpr int OnTheFly=1;



    
void init_range(xpp::Session &s)
{
 s.integrator->range.type=0;
 s.integrator->range.rtype=RANGE_SINGLE;
 s.integrator->range.index=s.integrator->range.index2=0;
 s.integrator->range.steps2=20;
 s.integrator->range.movie=0;
 init_monte_carlo(s);
}

Result<std::int64_t> sweep_run_count(const Session &s, int steps, int steps2)
{
  /* both counts are int, so their product fits 64 bits: the check is on the runs, the sweep's indices being int */
  const std::int64_t runs=(static_cast<std::int64_t>(steps)+1)*(static_cast<std::int64_t>(steps2)+1);
  if(runs<=MAX_RANGE_RUNS)return runs;
  return std::unexpected(Error{"Range",format("Range: a sweep of {} runs (steps = {}, steps2 = {}) is more than the {} a sweep takes; fewer steps on either axis",
                                              runs,steps,steps2,MAX_RANGE_RUNS),settings_place(s.model(),{"sweep.steps"})});
}

namespace {
/* what a range varies: item a parameter (PARAM) or else a variable (IC), and its index */
xpp::Result<> find_range_item(const xpp::Session &s, const std::string &item, int *type, int *index)
{
 int i=find_user_name(s.model(),PARAM,item);
 if(i>-1){
   *type=PARAM;
   *index=i;
   return {};
 }
 i=find_user_name(s.model(),IC,item);
 if(i<=-1)return xpp::fail("range",xpp::format("{} is not a parameter or variable",item),command_place());
 *type=IC;
 *index=i;
 return {};
}
}

int range_item(xpp::Session &s)
{
 return xpp::ok_or_show(find_range_item(s,s.integrator->range.item,&s.integrator->range.type,&s.integrator->range.index))?1:0;
}

int range_item2(xpp::Session &s)
{
 return xpp::ok_or_show(find_range_item(s,s.integrator->range.item2,&s.integrator->range.type2,&s.integrator->range.index2))?1:0;
}

xpp::Result<> resolve_range_items(xpp::Session &s, bool two)
{
 auto range=s.integrator->range;
 if(auto checked=resolve_range_items(s,range,two); !checked)return checked;
 s.integrator->range=range;
 return {};
}

xpp::Result<> resolve_range_items(const Session &s, RangeVars &range, bool two)
{
 if(auto first=find_range_item(s,range.item,&range.type,&range.index);!first)return first;
 if(two)return find_range_item(s,range.item2,&range.type2,&range.index2);
 return {};
}

int set_up_range(xpp::Session &s)
{
 if(!program.interactive){ /* no dialog: the range the options set */
   if(range_item(s)==0)return 0;
   s.integrator->range_flag=1;
   return 1;
 }
 std::array<xpp::FormField,9> fields={
   xpp::FormField{.name="*3Range over",.value=s.integrator->range.item},
   xpp::FormField{.name="Steps",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator->range.steps),.min=1},
   xpp::FormField::real_number("Start",s.integrator->range.plow),
   xpp::FormField::real_number("End",s.integrator->range.phigh),
   xpp::FormField::boolean("Reset storage",s.integrator->range.reset),
   xpp::FormField::boolean("Use old ic's",s.integrator->range.oldic),
   xpp::FormField::boolean("Cycle color",s.integrator->range.cycle),
   xpp::FormField::boolean("Movie",s.integrator->range.movie),
   xpp::FormField::boolean("Write each run to its own file",s.integrator->range.write_files)};
 if(!xpp::ask_form(s,"Range Integrate",fields))return 0;
 s.integrator->range.item=fields[0].value;
 if(range_item(s)==0)return 0;
 s.integrator->range.steps=fields[1].whole;
 s.integrator->range.plow=fields[2].real;
 s.integrator->range.phigh=fields[3].real;
 s.integrator->range.reset=fields[4].flag;
 s.integrator->range.oldic=fields[5].flag;
 s.integrator->range.cycle=fields[6].flag;
 s.integrator->range.movie=fields[7].flag;
 s.integrator->range.write_files=fields[8].flag;
 s.integrator->range_flag=1;
 return 1;
}

int set_up_range2(xpp::Session &s)
{
 if(!program.interactive){
   return(range_item(s));
 }
 std::array<xpp::FormField,14> fields={
   xpp::FormField{.name="*3Vary1",.kind=XPP_FIELD_NAME_IN(3),.value=s.integrator->range.item},
   xpp::FormField::real_number("Start1",s.integrator->range.plow),
   xpp::FormField::real_number("End1",s.integrator->range.phigh),
   xpp::FormField{.name="*3Vary2",.kind=XPP_FIELD_NAME_IN(3),.value=s.integrator->range.item2},
   xpp::FormField::real_number("Start2",s.integrator->range.plow2),
   xpp::FormField::real_number("End2",s.integrator->range.phigh2),
   xpp::FormField{.name="Steps",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator->range.steps),.min=1},
   xpp::FormField::boolean("Reset storage",s.integrator->range.reset),
   xpp::FormField::boolean("Use old ic's",s.integrator->range.oldic),
   xpp::FormField::boolean("Cycle color",s.integrator->range.cycle),
   xpp::FormField::boolean("Movie",s.integrator->range.movie),
   xpp::FormField::choice("Vary both",{"together","independently"},s.integrator->range.rtype==RANGE_INDEPENDENT?1:0),
   xpp::FormField{.name="Steps2",.kind=XPP_FIELD_INTEGER,.value=xpp::format("{}",s.integrator->range.steps2),.min=1},
   xpp::FormField::boolean("Write each run to its own file",s.integrator->range.write_files)};
 if(!xpp::ask_form(s,"Double Range Integrate",fields))return 0;
 s.integrator->range.item=fields[0].value;
 if(range_item(s)==0)return 0;
 s.integrator->range.item2=fields[3].value;
 if(range_item2(s)==0)return 0;
 s.integrator->range.plow=fields[1].real;
 s.integrator->range.phigh=fields[2].real;
 s.integrator->range.plow2=fields[4].real;
 s.integrator->range.phigh2=fields[5].real;
 s.integrator->range.steps=fields[6].whole;
 s.integrator->range.reset=fields[7].flag;
 s.integrator->range.oldic=fields[8].flag;
 s.integrator->range.cycle=fields[9].flag;
 s.integrator->range.movie=fields[10].flag;
 s.integrator->range.rtype=fields[11].whole==1?RANGE_INDEPENDENT:RANGE_TOGETHER;
 s.integrator->range.steps2=fields[12].whole;
 s.integrator->range.write_files=fields[13].flag;
 s.integrator->range_flag=1;
 return 1;
}

void init_monte_carlo(xpp::Session &s)
{
  s.integrator->fixptguess.tol=.001;
  s.integrator->fixptguess.n=100;
  s.integrator->fixptguess.xlo.assign(s.model().node,-10);
  s.integrator->fixptguess.xhi.assign(s.model().node,10);
  s.integrator->fixptlist.flag=0;
  s.integrator->fixptlist.n=0;
}

void swap_color(xpp::Session &s, xpp::Color *col, int rorw)
{
 if(rorw)s.plot_windows->current->color[0]=*col;
 else *col=s.plot_windows->current->color[0];
}

void change_current_color(xpp::Session &s, const xpp::Color &newcolor, xpp::Color *old)
{
 swap_color(s,old,0);
 s.plot_windows->current->color[0]=newcolor;
}

void set_cycle(xpp::Session &s, int flag, int *icol)
{
 if(flag==0)return;
 s.plot_windows->current->color[0]=xpp::xppaut_color(*icol+1); /* XPPAUT cycles red to purple */
 *icol=*icol+1;
  if(*icol==10)*icol=0;
} 

void do_start_flags(xpp::Session &s, double *x,double *t)
{
 int iflagstart=1;
 double tnew=*t;
 double sss;
 one_flag_step(s,x,x,&iflagstart,*t,&tnew,s.model().node,&sss);

}
/*  form_ic  --  u_i(0) = F(i)  where  "i" is represented by "t"
    or  
    u[5..20]=f([j]) 
*/

void evaluate_ar_ic(xpp::Session &s, const char *v, const char *f, int j1, int j2)
{
  int j;
  int i,flag;
  double z;
  std::string vp, fp;
  for(j=j1;j<=j2;j++){
    subsk(v,vp,j,1);
    i=find_user_name(s.model(),IC,vp);
    if(i>=0){
      subsk(f,fp,j,1);
      flag=do_calc(s,fp,&z);
      if(flag!=-1)
	s.last_ic[i]=z;
      else 
	return;
    }
  }

}
void arr_ic_start(xpp::Session &s)
{
  int i;
  s.integrator->array_ics=s.model().array_ics; /* the run's own, which Initialconds edits */
  const ArrayIcs &ics=s.integrator->array_ics;
  if(ics.defined==0) return;
  for(i=0;i<NAR_IC;i++){
    if(ics.ics[i].type==2){
      evaluate_ar_ic(s,ics.ics[i].var.c_str(),ics.ics[i].formula.c_str(),
		     ics.ics[i].j1,ics.ics[i].j2);
    }
  }

}

namespace {
xpp::Result<std::vector<int>> ic_problem(std::string what)
{
  return xpp::fail("initial conditions",std::move(what),xpp::command_place());
}
}

xpp::Result<std::vector<int>> set_ic_formula(xpp::Session &s, std::string_view variable, std::string_view formula)
{
  const xpp::Model &m=s.model();
  std::string text(variable),base;
  int j1,j2,array;
  search_array(text.data(),base,&j1,&j2,&array);
  std::vector<int> indices;
  std::vector<double> values;
  if(array!=1){
    const int index=find_user_name(m,ICBOX,variable);
    if(index<0)return ic_problem(xpp::format("{} is not a variable of the model",variable));
    const xpp::Result<double> value=evaluate_formula(s,formula);
    if(!value)return ic_problem(xpp::format("{}: {}",formula,value.error().what));
    indices.push_back(index);
    values.push_back(*value);
  }
  else{
    if(j2<j1)return ic_problem(xpp::format("{} is an empty range of variables",variable));
    for(int j=j1;j<=j2;j++){
      xpp::TypedFields fields;
      std::string name,element;
      subsk(base,name,j,1,&fields);
      subsk(formula,element,j,1,&fields);
      if(fields.error())return ic_problem(*fields.error());
      const int index=find_user_name(m,ICBOX,name);
      if(index<0)return ic_problem(xpp::format("{} is not a variable of the model (element {} of {})",name,j,variable));
      const xpp::Result<double> value=evaluate_formula(s,element);
      if(!value)return ic_problem(xpp::format("{} for {}: {}",element,name,value.error().what));
      indices.push_back(index);
      values.push_back(*value);
    }
  }
  xpp::push_value_undo(s);
  for(std::size_t k=0;k<indices.size();k++)s.last_ic[static_cast<std::size_t>(indices[k])]=values[k];
  /* the array's formula is kept, as the model's own are (arr_ic_start) */
  if(array==1)array_ic_for(s.integrator->array_ics,base.c_str(),j1,j2).formula=formula;
  redraw_ics();
  return indices;
}

xpp::Result<std::vector<int>> set_ic_array(xpp::Session &s, std::string_view name, int size, std::string_view formula)
{
  const xpp::Model &m=s.model();
  const int first=find_user_name(m,ICBOX,name);
  if(first<0)return ic_problem(xpp::format("{} is not a variable of the model",name));
  if(size<1||size>m.node-first)
    return ic_problem(xpp::format("{} elements from {} do not fit: the model has {} differential equations, {} is number {}",size,name,m.node,name,first+1));
  std::vector<int> indices;
  std::vector<double> values;
  for(int k=0;k<size;k++){
    set_val(s.parser,"t",static_cast<double>(k)); /* the formula's t is the element's offset from the first */
    const xpp::Result<double> value=evaluate_formula(s,formula);
    if(!value)return ic_problem(xpp::format("{}: {}",formula,value.error().what));
    indices.push_back(first+k);
    values.push_back(*value);
  }
  for(std::size_t k=0;k<indices.size();k++)s.last_ic[static_cast<std::size_t>(indices[k])]=values[k];
  redraw_ics();
  return indices;
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
 xpp::Solver &solver=*s.integrator->solver;
 int nodes=s.solver_work->xpv.node+s.solver_work->xpv.nvec;
 int store_every=s.numerics->store_every;
 double tend=s.numerics->total_time;
 double dt=s.numerics->delta_t;
 const auto steps=xpp::step_count(s,solver.info(),dt,tend);
 if(!steps)return std::unexpected(steps.error());
 if(solver.traits().discrete) dt=dt/fabs(dt);
 if(ishow==1){
 /* shown as it runs: a failure only ends the integration, the shooting
    goes on from where it stopped (the error is the caller's to show) */
 return integrate(s,t,y,tend,dt,1,store_every,istart).transform([](int){});
}
 int per_advance=0; /* a fixed-step method's steps in one advance */
 if(solver.traits().fixed_step){
   const auto counted=xpp::steps_per_advance(s,solver.info(),*steps);
   if(!counted)return std::unexpected(counted.error());
   per_advance=*counted;
 }
 copy_state(s,s.solver_work->xpv.x,y);
 evaluate_derived(s.parser);
 solver.begin(istart);
 xpp::SolverStep step{.y=s.solver_work->xpv.x,.t=t,.neq=nodes,.start=istart};
 if(solver.traits().fixed_step){
   step.dt=dt;
   step.steps=per_advance;
 }
 else{
   step.tout=*t+tend*dt/fabs(dt);
   step.hguess=&dt;
 }
 xpp::Result<> r=solver.advance(step);
 copy_state(s,y,s.solver_work->xpv.x);
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

std::unexpected<xpp::Error> take_step_error(xpp::Session &s)
{
  xpp::Error e=take_stop(s.parser);
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
  e.action=xpp::switch_to_differences(xpp::format(R"({{"cmd":"run.continue","until":{}}})",xpp::number(end)));
  return e;
}
}

namespace {
/* what a global event that set arret says: the time, the condition and its line */
std::string halt_note(const xpp::Session &s, int flag, double t)
{
  const xpp::Place where=xpp::model_statement_place(s.model(),odex::Statement::Kind::Event,flag);
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
  if(!s.integrator->steady)s.integrator->steady_result.reset();
  s.integrator->stop_note.clear();
  /* what Escape stops (xpp_job.h); a sweep's worker is not the main thread: its sweep holds the computation and the front end is the main thread's */
  const bool front_end=!s.integrator->on_worker;
  std::optional<xpp::Computation> computing;
  if(front_end)computing.emplace();
  const DaeRun dae_run(s); /* the run follows one branch of a DAE's solutions */
  xpp::Solver &solver=*s.integrator->solver;
  if(auto limits=xpp::check_step_limits(s,solver.info(),dt,tend);!limits)return std::unexpected(limits.error());
  if(auto from=xpp::check_store_from(s,*t,dt,tend);!from)return std::unexpected(from.error());
  const auto steps=xpp::step_count(s,solver.info(),dt,tend);
  if(!steps)return std::unexpected(steps.error());

 /* an added column (add_stor_col) plotted live has index above neq and no live value: it reads 0 */
 const std::size_t neq_size=static_cast<std::size_t>(s.model().neq);
 /* the row the step stores, at the store's precision (doubles, rounded by the store), and its float copy for the live plot */
 std::vector<double> xv_values(neq_size+1+xpp::DataStore::ADDED_COLUMNS),xvold_values(neq_size+1+xpp::DataStore::ADDED_COLUMNS);
 double *const xv=xv_values.data(),*const xvold=xvold_values.data();
 std::vector<float> plot_xv(xv_values.size()),plot_xvold(xv_values.size());
 double oldperiod=0.0;
 std::vector<double> xprime_values(neq_size),oldxprime_values(neq_size);
 double *const xprime=xprime_values.data(),*const oldxprime=oldxprime_values.data(),hguess=dt;

 std::vector<int> torcross_values(neq_size);
 int *const torcross=torcross_values.data();
 int nodes=s.solver_work->xpv.node+s.solver_work->xpv.nvec-s.model().nmarkov;

 const double section_start=*t;
 bool section_crossed=false;
 int rval=0;
 std::optional<xpp::Error> failure; /* why the loop stopped, when it failed */
 std::vector<double> oldx_values(neq_size);
 double *const oldx=oldx_values.data(),oldt=0,dint,dxp,sect,sect1,tout=*t,tzero=*t;
 double sss,tnew=*t;
 int iflagstart=1;
 float tscal=tend;
 double tv;
 
 char esc;
 int ieqn,i,pflag=0;
 std::int64_t icount=0; /* output steps taken: a run may take more than an int counts (W678) */
 std::int64_t nit;
 /* a steady-state run needs only the last values, so it is never bounded by
    storage: when the store fills it keeps every second row and stores every
    second event from then on */
 int steady_stride=1,steady_seen=0;
 int cwidth=0;
 /* new poincare map stuff */

copy_state(s,s.solver_work->xpv.x,x);

if(program.interactive&&front_end) cwidth=get_command_width();

 s.integrator->last_time=*t;
 evaluate_derived(s.parser);
 if(s.parser.step_error)return take_step_error(s);

 solver.begin(start);
 if(solver.traits().discrete) dt=dt/fabs(dt);
 nit=(*steps+store_every-1)/store_every;
 if(nit==0)return(rval);
 one_flag_step(s,s.solver_work->xpv.x,s.solver_work->xpv.x,&iflagstart,*t,&tnew,nodes,&sss);
 copy_state(s,x,s.solver_work->xpv.x);
 extra(s,x,*t,s.model().node,s.model().neq); /* Note this takes care of initializing Markov variables */
 if(s.parser.step_error)return take_step_error(s);
  copy_state(s,s.solver_work->xpv.x,x);
 if(s.integrator->steady)s.integrator->steady->begin(std::span(x,static_cast<std::size_t>(s.model().node+s.model().nmarkov)),*t);
 xv[0]=*t;
 for(ieqn=1;ieqn<=s.model().neq;ieqn++)xv[ieqn]=x[ieqn-1];
 if(s.animation->options.on_the_fly)ui.ani_on_the_fly(s,1); 
   
 if(s.numerics->poincare_map!=PoincareMap::none)
 {
 oldt=*t;
 for(ieqn=0;ieqn<s.model().neq;ieqn++)oldx[ieqn]=x[ieqn];
 }
 if(dt<0.0)tscal=-tend;
 if(tscal==0.0)tscal=1.0;
 stor_delay(s,x);
 /* xppautX: the rows a cancel before the first step finds (xpp_job.h) */
 if(front_end)xpp::job::report_rows(s.data_store->rows, s.data_store->rows > 0 ? s.data_store->col[0][s.data_store->rows-1] : *t);

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
	     copy_state(s,s.solver_work->xpv.x,x);
	     xpp::Result<> r=solver.advance({.y=s.solver_work->xpv.x,.t=t,.neq=nodes,.start=start,
						 .tout=tout,.hguess=&hguess,.sections=true});
	     copy_state(s,x,s.solver_work->xpv.x);
	     if(!solver.traits().dense_output||*t==tout)stor_delay(s,x);
	     if(s.parser.step_error){
	       s.integrator->last_time=*t;
	       return take_step_error(s);
	     }
	     if(!r){
	       if(front_end)ping();
	       s.integrator->last_time=*t;
	       return std::unexpected(stopped(r.error(),tzero+tend,dt));
	     }
	   }
	   else{
	     /* store_every steps of dt */
	     copy_state(s,s.solver_work->xpv.x,x);
	     xpp::Result<> r=solver.advance({.y=s.solver_work->xpv.x,.t=t,.neq=nodes,.start=start,
						 .dt=dt,.steps=store_every});
	     copy_state(s,x,s.solver_work->xpv.x);
	     if(!r){
	       if(front_end)ping();
	       s.integrator->last_time=*t;
	       return std::unexpected(stopped(r.error(),tzero+tend,dt));
	     }
	   }
	   /*   START POST INTEGRATE STUFF */           

	   extra(s,x,*t,s.model().node,s.model().neq);

          if (s.numerics->torus) {
	for (ieqn = 0; ieqn < s.model().neq; ieqn++) {
	        torcross[ieqn]=0;
		if (s.itor[ieqn] == 1) {
			if (x[ieqn] > s.numerics->torus_period) {
				x[ieqn] -= s.numerics->torus_period;
                                torcross[ieqn]=-1;
			}
			if (x[ieqn] < 0) {
				x[ieqn] += s.numerics->torus_period;
                                torcross[ieqn]=1;
			}
		      }
	      }
      }
	   xvold[0]=xv[0];
           for(ieqn=1;ieqn<(s.model().neq+1);ieqn++)
           {
	    xvold[ieqn]=xv[ieqn];
	    xv[ieqn]=x[ieqn-1];
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
            if(s.numerics->stop_at_limit&&fabs(x[ieqn-1])>s.numerics->state_limit)
            {
             const std::string error_message=xpp::format("{} out of bounds at t = {} (state_limit = {})",
             s.model().uvar_names[ieqn-1],*t,xpp::number(s.numerics->state_limit));
	     if(s.integrator->range_flag||s.integrator->suppress_bounds){
	       s.integrator->stop_note=error_message; /* the run ends, its sweep or drawing goes on */
	       break;
	     }
             trace_states(ieqn);
             failure=xpp::Error{"integration",error_message,xpp::model_place(s.model(),s.model().uvar_names[ieqn-1])};
             break;
            }
           }
	                
        /*   This is where the progresser goes   */
	   if(program.interactive&&front_end) plot_command(nit,icount,cwidth);
	   esc=front_end?my_abort():xpp::job::cancelled()?ESC:KEY_NONE; /* a worker only sees the cancel the main thread (or Stop) made */

           {
            
             if(esc==ESC){if(s.integrator->steady)s.integrator->steady_result->status="stopped";break;}
	     if(esc=='/'){rval=1;s.numerics->endsing=true;if(s.integrator->steady)s.integrator->steady_result->status="stopped";break;}
	    
           }
	if(s.integrator->stop_flag==1){
	  s.integrator->stop_flag=0;
	  if(s.integrator->halt_flag>=0)s.integrator->stop_note=halt_note(s,s.integrator->halt_flag,*t);
	  s.integrator->halt_flag=-1;
	  if(s.integrator->steady)s.integrator->steady_result->status="stopped";
	  break;
	}
           if(s.parser.step_error){
             s.numerics->endsing=true;
             xpp::Error e=take_step_error(s).error();
             if(!failure)failure=std::move(e); /* a variable's NaN came first */
             break;
           }
           if(ieqn<(s.model().neq+1))break;
           if(front_end)ui.live_state(s, x, *t);
           tv=*t;
	   xv[0]=tv;
 if(solver.traits().dense_output&&s.numerics->poincare_map!=PoincareMap::none){
   pflag=solver.section_found()?1:0;
   if(!pflag)goto out;
   goto dense_poi;
 }
 if((s.numerics->poincare_map==PoincareMap::max)&&!(s.numerics->poincare_variable==0))
 {
  pflag=0;
  if((oldx[s.numerics->poincare_variable-1]<x[s.numerics->poincare_variable-1])&&!(s.numerics->poiext<0))s.numerics->poiext=1;
  if((oldx[s.numerics->poincare_variable-1]>x[s.numerics->poincare_variable-1])&&!(s.numerics->poiext>0))s.numerics->poiext=-1;
  if(  ( !(oldx[s.numerics->poincare_variable-1]<x[s.numerics->poincare_variable-1]) && (s.numerics->poiext>0) )||
       ( !(oldx[s.numerics->poincare_variable-1]>x[s.numerics->poincare_variable-1]) && (s.numerics->poiext<0) )
    )
  {
     if(static_cast<int>(s.numerics->poincare_sign)*s.numerics->poiext>=0)
      {
	/*  We will interpolate to get a good local extremum   */
	
	s.integrator->rhs(*t,x,xprime,s.model().neq);
	s.integrator->rhs(oldt,oldx,oldxprime,s.model().neq);
        dxp=xprime[s.numerics->poincare_variable-1]-oldxprime[s.numerics->poincare_variable-1];
        if(dxp==0.0)
	  return xpp::fail("Poincare map","Cannot zero RHS for max/min - use a variable",command_place());
	dint=xprime[s.numerics->poincare_variable-1]/dxp;

	tv=(1-dint)**t+dint*oldt;
	xv[0]=tv;
	for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
	pflag=1;
        
      }
      s.numerics->poiext=-s.numerics->poiext;
   }
  goto poi;
 }

 if(s.numerics->poincare_map==PoincareMap::section||s.numerics->poincare_map==PoincareMap::period)
 {
    if(s.numerics->poincare_variable==0)

     {
     sect1=fmod(fabs(oldt),fabs(s.numerics->poincare_plane));
     sect=fmod(fabs(*t),fabs(s.numerics->poincare_plane));
     if(sect<sect1)
     {
     dint=sect/(s.numerics->poincare_plane+sect-sect1);
     i=static_cast<int>((fabs(*t)/fabs(s.numerics->poincare_plane)));
     tv=s.data_store->round(s.data_store->round(s.numerics->poincare_plane)*i);
     xv[0]=tv;
     for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
     pflag=1;
     }
     else pflag=0;
    }

    else

    {
     if(s.numerics->poincare_sign!=SectionSign::negative)
     {
     if((oldx[s.numerics->poincare_variable-1]<s.numerics->poincare_plane)&&!(x[s.numerics->poincare_variable-1]<s.numerics->poincare_plane))
     {
      dint=(x[s.numerics->poincare_variable-1]-s.numerics->poincare_plane)/(x[s.numerics->poincare_variable-1]-oldx[s.numerics->poincare_variable-1]);
      tv=(1-dint)**t+dint*oldt;
      xv[0]=tv;
      for(i=1;i<=s.model().neq;i++)xv[i]=dint*oldx[i-1]+(1-dint)*x[i-1];
      pflag=1;
      goto poi;

     }
     else pflag=0;
     }
    if(s.numerics->poincare_sign!=SectionSign::positive)
     {
       if((oldx[s.numerics->poincare_variable-1]>s.numerics->poincare_plane)&&!(x[s.numerics->poincare_variable-1]>s.numerics->poincare_plane))
       {
        dint=(x[s.numerics->poincare_variable-1]-s.numerics->poincare_plane)/(x[s.numerics->poincare_variable-1]-oldx[s.numerics->poincare_variable-1]);
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
 if(s.numerics->poincare_map==PoincareMap::period&&pflag==1){
   if(oldperiod==0.0){
     pflag=0; /* this is the first hit !! */
     oldperiod=s.data_store->round(*t);
     goto out;
   }
   xv[0]=*t-oldperiod;
   oldperiod=s.data_store->round(*t);
 }
     
          if(stored_from(s.numerics->store_from,*t,dt)&&program.interactive&&front_end&&OnTheFly)
	  {
	     std::ranges::copy(xv_values,plot_xv.begin());
	     std::ranges::copy(xvold_values,plot_xvold.begin());
	     ui.step_computed(s,plot_xv,plot_xvold,fabs(dt*s.numerics->store_every),std::span<const int>(torcross,static_cast<std::size_t>(s.model().neq)));

	  }

	   if((s.numerics->storflag)&&(count!=0)&&(s.data_store->rows<s.data_store->max_rows)&&stored_from(s.numerics->store_from,*t,dt)
	      &&(!s.integrator->steady||++steady_seen%steady_stride==0))
	   {
           if(s.animation->options.on_the_fly)ui.ani_on_the_fly(s,0);
           for(ieqn=0;ieqn<=s.model().neq;ieqn++)
		 s.data_store->col[ieqn][s.data_store->rows]=xv[ieqn];
	    s.data_store->rows++;
	    row_stored(s); /* xppautX: replay stops here, a front end shows the run grow */
	    if(s.integrator->steady?s.data_store->rows>=DataStore::start_rows:!(s.data_store->rows<s.data_store->max_rows)){
            if(s.integrator->steady){
              s.data_store->thin(s.model().neq+1);
              steady_stride*=2;
              steady_seen=0;
            }
            else {
              const auto grown=stor_full(s);
              if(!grown){stop_integration(s.parser,grown.error());failure=take_step_error(s).error();break;}
            }
            }
	    if((pflag==1)&&(s.numerics->poincare_stop)){s.integrator->stop_note=section_note(*t);break;}
	   }

out:
           if(solver.section_found()&&s.numerics->poincare_stop){s.integrator->stop_note=section_note(*t);break;}
           if(!solver.traits().dense_output||*t==tout)icount++;
           if(s.integrator->steady&&xpp::job::cancelled()){
             s.integrator->steady_result->status="stopped";
             break;
           }
           if(s.integrator->steady&&s.integrator->steady->observe(std::span(x,static_cast<std::size_t>(s.model().node+s.model().nmarkov)),*t)){
             s.integrator->steady_result->status="settled";
             break;
           }
           if(s.integrator->steady&&icount>=nit)s.integrator->steady_result->status="limit";
           if(icount>=nit&&count!=0)break;

	   /* END POST INTEGRATE ANALYSIS  */
 }
 
       s.integrator->last_time=*t;
       solver.finish();
       if(failure)return std::unexpected(std::move(*failure));
       if(s.numerics->poincare_map!=PoincareMap::none && !section_crossed &&
          s.integrator->stop_note.empty() && !xpp::job::cancelled()){
         s.integrator->stop_note=xpp::format("No crossing found ({}) in [{}, {}]; change the section or integrate longer",
             xpp::named_settings(s,{"poincare.map","poincare.variable", "poincare.plane"}),section_start,*t);
       }
       return(rval);
  }
void send_halt(xpp::Session &s, int flag)
{
  s.integrator->stop_flag=1;
  s.integrator->halt_flag=flag;
}
void send_output(xpp::Session &s, double *y,double t)
{
  std::vector<double> yy(s.model().neq);
  int i;
  for(i=0;i<s.model().node;i++)
    yy[i]=y[i];
  extra(s,yy.data(),t,s.model().node,s.model().neq);
  if((s.numerics->storflag)&&(s.data_store->rows<s.data_store->max_rows)){
    
    for(i=0;i<s.model().neq;i++)
      s.data_store->col[i+1][s.data_store->rows]=yy[i];
    s.data_store->col[0][s.data_store->rows]=t;
    s.data_store->rows++;
    row_stored(s);
  }
}

/* an invariant set's branch ends where it leaves the box (state_limit) or an
   event stops it: said once, at INFO, since drawing the set means that */
static void log_branch_end(xpp::Session &s)
{
  if(s.integrator->stop_note.empty())return;
  xpp::log(XPP_LOG_INFO,"Invariant set branch ended: {}\n",s.integrator->stop_note);
  s.integrator->stop_note.clear();
}

xpp::BranchStoreFrom::BranchStoreFrom(xpp::Session &s) : s_(s), before_(s.numerics->store_from)
{
  s_.numerics->store_from=xpp::BRANCH_START_TIME;
}

xpp::BranchStoreFrom::~BranchStoreFrom()
{
  s_.numerics->store_from=before_;
}

xpp::Result<> shoot_easy(xpp::Session &s, double *x)
{
  double t=xpp::BRANCH_START_TIME;
  int i;
  const xpp::BranchStoreFrom branch(s);
  s.integrator->suppress_bounds=1;
  const xpp::Result<int> r=integrate(s,&t,x,s.numerics->total_time,s.numerics->delta_t,1,s.numerics->store_every,&i);
  s.integrator->suppress_bounds=0;
  log_branch_end(s);
  return r.transform([](int){});
}

xpp::Result<> shoot(xpp::Session &s, double *x, double *xg, double *evec, int sgn)
{
 int i;
 double t=xpp::BRANCH_START_TIME;
 const xpp::BranchStoreFrom branch(s);
 s.integrator->suppress_bounds=1;
 for(i=0;i<s.model().node;i++)
 x[i]=xg[i]+sgn*evec[i]*s.numerics->delta_t*.1;
i=1;
 const xpp::Result<int> r=integrate(s,&t,x,s.numerics->total_time,s.numerics->delta_t,1,s.numerics->store_every,&i);
 ping();
  s.integrator->suppress_bounds=0;
 log_branch_end(s);
 return r.transform([](int){});
}

xpp::Result<> stor_full(xpp::Session &s)
{
 if(s.data_store->max_rows>xpp::DataStore::max_rows_kept)
   return xpp::fail("storage",xpp::format("Cannot allocate sufficient storage: the run has stored {} rows, the most the data store keeps is {} ({})",
                                          s.data_store->rows,xpp::DataStore::max_rows_kept,xpp::named_settings(s,{"integration.total_time","integration.store_every"})),
                    command_place());
 return s.data_store->fit(s.model().neq+1,2*s.data_store->max_rows);
}

} // namespace xpp
