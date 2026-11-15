/* The runs a command makes of the integrator and what is made of each: see runs.h. integrate() (integrate.cpp)
   integrates one run; the data table, the sweeps and Monte Carlo are here, above the integrators. */
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <new>
#include <stdexcept>

#include "adj2.h"
#include "ani_state.h"
#include "browse.h"
#include "colormap.h"
#include "del_stab.h"
#include "dae_fun.h"
#include "delay_handle.h"
#include "derived.h"
#include "equilibria.h"
#include "expr.h"
#include "flags.h"
#include "histogram.h"
#include "integrate.h"
#include "line_style.h"
#include "markov.h"
#include "model.h"
#include "model_place.h"
#include "my_rhs.h"
#include "model_native.h"
#include "numerics.h"
#include "numerics_settings.h"
#include "plot_state.h"
#include "runs.h"
#include "session.h"
#include "session_values.h"
#include "solver.h"
#include "storage.h"
#include "stochastic.h"
#include "sweep_runs.h"
#include "tabular.h"
#include "typed_values.h"
#include "value_undo.h"
#include "volterra2.h"
#include "xpp_globals.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "xpp_mem.h"
#include "xpp_path.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "struct.h"

#define PARAM 1
#define IC 2

namespace xpp {

xpp::Result<> do_monte_carlo_search(xpp::Session &s, int append, int stuffbrowse,int ishoot)
{
  xpp::FirstError failure; /* a manifold's */
  int i,j,k,m,n=s.integrator->fixptguess.n;
  int ierr,is_new=1;
  int attempted=0,converged=0;
  double sum;
  std::vector<double> x(s.model().node),er(s.model().node),em(s.model().node);
  if(append==0)
    s.integrator->fixptlist.n=0;

  if(s.integrator->fixptlist.flag==0){
    s.integrator->fixptlist.x.resize(MAXFP);
    s.integrator->fixptlist.er.resize(MAXFP);
    s.integrator->fixptlist.em.resize(MAXFP);
    for(i=0;i<MAXFP;i++){
      s.integrator->fixptlist.x[i].assign(s.model().node,0.0);
      s.integrator->fixptlist.er[i].assign(s.model().node,0.0);
      s.integrator->fixptlist.em[i].assign(s.model().node,0.0);
    }
    s.integrator->fixptlist.flag=1;
  }
  for(i=0;i<n;i++){
    if(xpp::job::cancelled())break;
    attempted++;
    ierr=1;
    for(j=0;j<s.model().node;j++){ 
      x[j]=s.random.uniform()*(s.integrator->fixptguess.xhi[j]-s.integrator->fixptguess.xlo[j])+s.integrator->fixptguess.xlo[j];
    }
    if(auto r=do_sing_info(s,x.data(),s.numerics->equilibria.jacobian_step,s.numerics->equilibria.newton_tolerance,s.numerics->equilibria.search_limit,s.numerics->equilibria.max_iterations,s.model().node,er.data(),em.data(),&ierr);!r){failure.keep(r);if(ierr!=0)break;}
    if(ierr==0){
      converged++;
      m=s.integrator->fixptlist.n;
      if(m==0){ /* first fixed point found */
	s.integrator->fixptlist.n=1;
	xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	for(j=0;j<s.model().node;j++){
	  s.integrator->fixptlist.x[0][j]=x[j];
	  s.integrator->fixptlist.er[0][j]=er[j];
	  s.integrator->fixptlist.em[0][j]=em[j];
          if(ishoot)failure.keep(shoot_this_now(s));
	  xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	}
      }
      else { /* there are others  better compare them */
	is_new=1;
	for(k=0;k<m;k++){
	  sum=0.0;
	  for(j=0;j<s.model().node;j++)
	    sum+=fabs(x[j]-s.integrator->fixptlist.x[k][j]);
	  if(sum<s.integrator->fixptguess.tol)
	    is_new=0;
	}
	if(is_new==1){
	  m=s.integrator->fixptlist.n;
	  if(m>=MAXFP){
	    failure.keep(xpp::Error{"equilibria",xpp::format("The Monte Carlo search found {} equilibria, the most it keeps: stopped after {} of {} guesses",MAXFP,i,n),command_place()});
	    break;
	  }
	  s.integrator->fixptlist.n++;
	  {
	    xpp::log(XPP_LOG_INFO, "Found: {}\n",m);
	    for(j=0;j<s.model().node;j++){
	      s.integrator->fixptlist.x[m][j]=x[j];
	      s.integrator->fixptlist.er[m][j]=er[j];
	      s.integrator->fixptlist.em[m][j]=em[j];
	      if(ishoot)failure.keep(shoot_this_now(s));
	      xpp::log(XPP_LOG_INFO, " x[{}]= {:g}   eval= {:g} + I {:g} \n",j,x[j],er[j],em[j]);
	    }
	  }
	}
      }
    }
  }
  if(stuffbrowse) {
    reset_data(s);
    s.data_store->rows=0;
    m=s.integrator->fixptlist.n;
    for(i=0;i<m;i++){
      if(s.data_store->rows==s.data_store->max_rows){
        const auto grown=stor_full(s);
        if(!grown){failure.keep(grown);break;}
      }
      s.data_store->col[0][s.data_store->rows]=i;
      for(j=0;j<s.model().node;j++)s.data_store->col[j+1][s.data_store->rows]=s.integrator->fixptlist.x[i][j];
      s.data_store->rows++;
    }
    refresh_data(s,s.data_store->rows);
  }
  bottom_msg(0,xpp::format("Monte Carlo: {} of {} guesses converged to {} distinct equilibria ({} requested; {}); {}",
      converged,attempted,s.integrator->fixptlist.n,n,
      xpp::named_settings(s,{"equilibria.max_iterations","equilibria.newton_tolerance", "equilibria.search_limit"}),
      xpp::job::cancelled()?"stopped by the user":attempted<n?"stopped early":"search finished"));
  return failure.result();
}

namespace {
/* a sweep's progress line ("p=0.3  i=4"), which the command shows */
using ShowProgress = std::function<void(const std::string &)>;

/* the progress line a range command shows */
void show_range_progress(const std::string &line)
{
  bottom_msg(2,line);
}

/* Range Equilibria's sweep over s.integrator->eq_range: each step's equilibrium (or
   Monte Carlo's) stored as a row; the first step that failed is
   returned once the sweep has ended */
xpp::Result<int> eq_range_sweep(xpp::Session &s, double *x, const xpp::RunReport &report, const ShowProgress &show_progress)
{
 double parlo,parhi,dpar,temp;
 int npar,stabcol,i,j,ierr;
 int mc;
 double stabinfo;
 xpp::FirstError failure;
 int points_found=0;
 int without_root=0; /* the values the search found no equilibrium at */
 std::string first_without;

 wipe_rep(*s.data);
 data_back(s);
 parlo=s.integrator->eq_range.plow;
 parhi=s.integrator->eq_range.phigh;
 
 npar=s.integrator->eq_range.steps;
 dpar=(parhi-parlo)/static_cast<double>(npar);
 stabcol=s.integrator->eq_range.col;
 mc=s.integrator->eq_range.mc;
 s.data_store->rows=0;
 s.parser.step_error.reset();
 s.numerics->endsing=false;
 s.numerics->par_fol=true;
 s.numerics->pauser=false;
 s.numerics->shoot=s.integrator->eq_range.shoot;
 reset_data(s);
 if(mc==1){
   s.integrator->eq_range.movie=1;
   s.numerics->shoot=false;
 }
 if(s.integrator->eq_range.movie)reset_film(s);
 /* the steps go one after another: each equilibrium search starts from the last one's (x), and a Monte Carlo step
    adds to the list of equilibria found and draws the manifolds it finds into the windows, in order */
 xpp::say_sweep_serial(s,"Range Equilibria",npar+1,mc?"each step adds to the equilibria found and draws their manifolds in order":"each search starts from the equilibrium before it");
 /* a Monte Carlo step has a seed of its own, the sweep's seed stream's, as a run of Range has (range_sweep) */
 int steps_seeded=0;
 int step_seed=s.numerics->rand_seed; /* step i's seed is the i-th of the stream (xpp::next_seed), made as the steps go: no list of them */
 for(i=0;i<=npar;i++)
   {
     if(xpp::job::cancelled())break;
     if(s.integrator->eq_range.movie)
       clear_draw_window(s);
      temp=parlo+dpar*static_cast<double>(i);
      bool found=true;
      set_val(s.parser,s.integrator->eq_range.item,temp);
      s.numerics->par_fol=true;
      {
        std::string bob=xpp::format("{}={:.16g}",s.integrator->eq_range.item,temp);
        show_progress(bob);
        evaluate_derived(s.parser);
        /*  I think  */ failure.keep(redo_all_fun_tables(s));
        if(mc) {
	  s.random.seed(step_seed);
	  s.numerics->last_seed=step_seed;
	  step_seed=xpp::next_seed(step_seed);
	  ++steps_seeded;
	  failure.keep(do_monte_carlo_search(s,0,0,1));
          points_found+=s.integrator->fixptlist.n;
        }
        else {
        const xpp::Result<> search=s.delay->flag
          ? do_delay_sing(s,x,s.numerics->equilibria.jacobian_step,s.numerics->equilibria.newton_tolerance,s.numerics->equilibria.search_limit,s.numerics->equilibria.max_iterations,
		          s.model().node,&ierr,&stabinfo)
          : do_sing(s,x,s.numerics->equilibria.jacobian_step,s.numerics->equilibria.newton_tolerance,s.numerics->equilibria.search_limit,s.numerics->equilibria.max_iterations,
		    s.model().node,&ierr,&stabinfo);
        if(!search){
          /* a value with no equilibrium has no row; the sweep goes on */
          const std::string note=xpp::format("{} = {}: no equilibrium found ({})",s.integrator->eq_range.item,xpp::number(temp),search.error().what);
          xpp::log(XPP_LOG_WARN,"Range Equilibria step {} of {}: {}\n",i+1,npar+1,note);
          if(without_root++==0)first_without=note;
          found=false;
        }
        }
        if(report.stored)failure.keep(report.stored(bob)); /* out of film, the sweep goes on without frames and tells it at the end */
      }
      if(mc==0&&found){
      if(s.data_store->rows==s.data_store->max_rows){
        const auto grown=stor_full(s);
        if(!grown){failure.keep(grown);break;}
      }
      s.data_store->col[0][s.data_store->rows]=temp;
      for(j=0;j<s.model().node;j++)s.data_store->col[j+1][s.data_store->rows]=x[j];
      for(j=s.model().node;j<s.model().node+s.model().nmarkov;j++)s.data_store->col[j+1][s.data_store->rows]=0.0;
      if(stabcol>0)s.data_store->col[stabcol-1][s.data_store->rows]=stabinfo;

      ++points_found;
      s.data_store->rows++;
      row_stored(s);}
      if(s.numerics->endsing||xpp::job::cancelled())break;
    }
    refresh_data(s,s.data_store->rows);
 if(mc){ /* the next run's seed is the one after the last step's, as a sweep of Range ends (range_sweep), however early this one did */
   for(;steps_seeded<=npar;++steps_seeded)step_seed=xpp::next_seed(step_seed);
   s.numerics->rand_seed=step_seed;
   s.random.seed(s.numerics->rand_seed);
 }
 s.numerics->par_fol=false;
 if(without_root>0)bottom_msg(0,xpp::format("{} of {} values have no equilibrium; the first: {}",without_root,npar+1,first_without));
 if(const auto result=failure.result();!result)return std::unexpected(result.error());
 return points_found;
}
}

Result<int> equilibrium_range(Session &s, const RunReport &report)
{
 return eq_range_sweep(s,s.data_store->current.data(),report,show_range_progress);
}

namespace {

/* Seeds the random generator for a run about to start (Go, usual_integrate_stuff's
   plain run; a sweep gives each of its runs its own seed, range_sweep): rand_seed is the seed shown
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
  const int seed=s.numerics->rand_seed;
  s.random.seed(seed);
  s.numerics->last_seed=seed;
  xpp::start_ou(s);
  xpp::log(XPP_LOG_INFO,"Go: seed {}\n",seed);
  s.numerics->rand_seed=xpp::next_seed(seed);
}

/* A run that ended early without failing (an event's arret, a section that
   stops the run) says why, once: its note is shown and cleared. */
void show_stop_note(xpp::Session &s)
{
  if(s.integrator->stop_note.empty())return;
  bottom_msg(0,s.integrator->stop_note);
  s.integrator->stop_note.clear();
}

/* What every run of a range reads (range_sweep): the settings made and the sweep's start. Run k is the grid point
   (i, j), k = j*(steps+1)+i (j counts the second parameter's values, i the first's): its values and its seed are
   functions of k alone, so the runs may be computed in any order, on any thread. */
struct RangeRuns {
  int type=0,type2=0,index=0,index2=0,rtype=RANGE_SINGLE,steps=0;
  std::string item,item2;
  double plow=0,dpar=0,plow2=0,dpar2=0;
  bool apply=true;   /* the runs set the values above (not autoX's range, which sets them itself) */
  bool fresh=false;  /* every run starts afresh from the initial data and the sweep's constants ("Use old I.C.s"): the
                        runs are independent, else each starts where the last ended */
  std::vector<int> seeds;                 /* the seed of each run (xpp::seed_stream) */
  std::vector<double> constants,variables; /* the parameters and variables at the sweep's start, which a fresh run begins from */

  struct Values { double p,p2; };
  Values values(int i,int j) const
  {
    Values v{plow+dpar*static_cast<double>(i),0.0};
    if(rtype==RANGE_TOGETHER)v.p2=plow2+dpar2*static_cast<double>(i);
    if(rtype==RANGE_INDEPENDENT)v.p2=plow2+dpar2*static_cast<double>(j);
    return v;
  }
};

/* What one run of a range gives the sweep's join: how it ended, what it stored and said. A run on a worker keeps its
   rows here (rows of ncol columns, column after column) for the join to take in; a run on the sweep's own Session
   stored them there already. */
struct RunOutcome {
  xpp::Result<int> run{0};            /* the integration's result */
  std::optional<xpp::Error> setup_error; /* the first error of setting the run's values (a function table that failed to rebuild) */
  bool setup_failed=false;            /* the run could not begin: setup_error is why, and it was not integrated */
  std::string unexpected;             /* an exception a worker caught, when the run did not end any other way */
  int stored=0;                       /* the rows the integration stored, the first row not counted */
  std::string stop_note,warnings;     /* why the run ended early without failing; what it logged at WARN */
  int rows=0;                         /* a worker's rows */
  std::vector<double> columns;       /* a worker's rows, column after column (a float or a double of the store held exactly) */
  double last_time=0;                 /* where the integration stopped */
};

/* One run of a range on the Session x, from the state: its seed, its values set, its start flags, its first row and
   its integration, what it gave back in o. The run on the sweep's own Session and on a worker are this one function,
   so a run's numbers do not depend on where it was computed. A fresh run reads nothing an earlier run left: the
   constants and variables are the sweep's, the state the initial data, the generator its own seed. */
void compute_run(xpp::Session &x, double *state, const RangeRuns &r, int i, int j, std::size_t k, RunOutcome &o)
{
  x.integrator->my_start=1;
  x.numerics->poiext=0;
  if(r.fresh){
    x.parser.constants=r.constants;
    x.parser.variables=r.variables;
  }
  x.random.seed(r.seeds[k]);
  xpp::start_ou(x);
  if(r.apply){
    const RangeRuns::Values v=r.values(i,j);
    if(r.fresh){
      get_ic(x,1,state);
      if(x.delay->flag){
        /* restart initial data */
        if(const xpp::Result<> d=do_init_delay(x,x.numerics->delay);!d){
          o.setup_error=d.error();
          o.setup_failed=true;
          return;
        }
      }
    }
    const auto set_value=[&](int type,int index,const std::string &item,double value){
      if(type==IC)state[index]=value;
      else {
        set_val(x.parser,item,value);
        if(auto t=redo_all_fun_tables(x);!t&&!o.setup_error)o.setup_error=t.error();
        re_evaluate_kernels(x);
      }
    };
    set_value(r.type,r.index,r.item,v.p);
    if(r.rtype!=RANGE_SINGLE)set_value(r.type2,r.index2,r.item2,v.p2);
  }
  double t=x.numerics->start_time;
  do_start_flags(x,state,&x.data_store->current_time);
  if(stored_from(x.numerics->store_from,x.data_store->current_time,x.numerics->delta_t)&&x.numerics->storflag&&x.numerics->poincare_map==PoincareMap::none)
  {
    x.data_store->col[0][x.data_store->rows]=x.data_store->current_time;
    extra(x,state,x.data_store->current_time,x.model().node,x.model().neq);
    for(int e=0;e<x.model().neq;e++)x.data_store->col[1+e][x.data_store->rows]=state[e];
    x.data_store->rows++;
  }
  /* the rows of earlier runs and the first one may fill the store (reset storage off): the integration stores only where there is room */
  if(x.data_store->rows==x.data_store->max_rows)
    if(const xpp::Result<> grown=stor_full(x);!grown){
      o.setup_error=grown.error();
      o.setup_failed=true;
      return;
    }
  const int rows_before=x.data_store->rows;
  o.run=integrate(x,&t,state,x.numerics->total_time,x.numerics->delta_t,1,x.numerics->store_every,&x.integrator->my_start);
  o.stored=x.data_store->rows-rows_before;
  o.stop_note=std::move(x.integrator->stop_note);
  x.integrator->stop_note.clear();
  o.last_time=x.integrator->last_time;
}

/* Range's sweep (one parameter or two, or autoX's range over its
   diagram when `diagram` is given), the settings made: one integration per step; -1 when one
   stopped early (the sweep ends there), else 0; or the error it failed
   with.

   Each run has a seed of its own, the sweep's seed stream's k-th (xpp::seed_stream), so a run's numbers are its own,
   whichever runs came before it or at the same time (W434). The runs that do not start where the last ended ("Use old
   I.C.s" on) and whose model allows it (sweep_serial_reason) are computed on as many worker threads as the thread
   setting gives; every run, on a worker or here, is compute_run, and what comes after a run (its rows, files, messages,
   drawing) is done here, in run order, as each run is taken in. */
xpp::Result<int> range_sweep(xpp::Session &s, double *x, const xpp::DiagramRange *diagram, const xpp::RunReport &report, const ShowProgress &show_progress, RangeProgress &progress)
{
  std::optional<xpp::Error> failure;
  bool complete=true; /* a failed capture can stop the sweep without an integration error */

  std::string bob;
 int ivar=0,ivar2=0,res=0,oldic=0;
 int nit=20,itype=0,itype2=0,cycle=0,icol=0,nit2=0;
 xpp::Color color=s.plot_windows->current->color[0];
 double dpar,plow=0.0,phigh=1.0,plow2=0.0,phigh2=0.0,dpar2=0.0;
 double temp,temp2;
 int ierr=0;
 xpp::Computation computing; /* the whole range, between its integrations too (xpp_job.h) */

 s.integrator->my_start=1;
 itype=s.integrator->range.type;
 ivar=s.integrator->range.index;

 res=s.integrator->range.reset;
 oldic=s.integrator->range.oldic;
 nit=s.integrator->range.steps;
 plow=s.integrator->range.plow;
 phigh=s.integrator->range.phigh;

 cycle=s.integrator->range.cycle;
 dpar=(phigh-plow)/static_cast<double>(nit);

nit2=0;
if(s.integrator->range.rtype==RANGE_INDEPENDENT)nit2=s.integrator->range.steps2;
 /* refused before anything below changes the Session; autoX's count (a diagram's) is its diagram's, capped by its points limit */
 if(!diagram)
   if(const Result<std::int64_t> count=sweep_run_count(s,nit,nit2); !count)return std::unexpected(count.error());
 get_ic(s,2,x);
 s.data_store->rows=0;
 s.numerics->storflag=true;
 s.numerics->pauser=false;
if(s.integrator->range.type==PARAM)get_val(s.parser,s.integrator->range.item,&temp);
 if(s.integrator->range.rtype!=RANGE_SINGLE){
 itype2=s.integrator->range.type2;
 ivar2=s.integrator->range.index2;
 plow2=s.integrator->range.plow2;
 phigh2=s.integrator->range.phigh2;
  if(s.integrator->range.rtype==RANGE_INDEPENDENT)dpar2=(phigh2-plow2)/static_cast<double>(nit2);
  else dpar2=(phigh2-plow2)/static_cast<double>(nit);
  if(s.integrator->range.type2==PARAM)get_val(s.parser,s.integrator->range.item2,&temp2);

 }

 if(s.integrator->range.movie)reset_film(s);
 if(diagram){
   nit=diagram->points;
   nit2=0;
 }
 const std::int64_t runs=(static_cast<std::int64_t>(nit)+1)*(nit2+1); /* at most MAX_RANGE_RUNS for a range (sweep_run_count) */
 const std::size_t run_count=runs>0?static_cast<std::size_t>(runs):0; /* autoX's range over an empty diagram has none */
 RangeRuns plan;
 plan.type=itype;plan.type2=itype2;plan.index=ivar;plan.index2=ivar2;
 plan.rtype=s.integrator->range.rtype;plan.steps=nit;
 plan.item=s.integrator->range.item;plan.item2=s.integrator->range.item2;
 plan.plow=plow;plan.dpar=dpar;plan.plow2=plow2;plan.dpar2=dpar2;
 plan.apply=!diagram;
 plan.fresh=oldic==1&&!diagram;
 plan.constants=s.parser.constants;
 plan.variables=s.parser.variables;
 /* run k's seed is the k-th of the stream from the seed set for the next run, and the next run's is the one after the
    last: a sweep uses up its runs' seeds as that many Go's would */
 plan.seeds=xpp::seed_stream(s.numerics->rand_seed,run_count);
 if(run_count>0){
   s.numerics->rand_seed=xpp::next_seed(plan.seeds.back());
   xpp::log(XPP_LOG_INFO,"Range: seed {} for the first of {} runs, the seed after it for the next\n",plan.seeds.front(),runs);
 }

 /* the runs side by side, when the runs are independent and the model lets them; else one after another here, and
    said why when more than one thread was there to use */
 const unsigned budget=s.thread_budget();
 unsigned threads=1;
 if(runs>1&&budget>1){
   xpp::native_ready(s); /* runs side by side call the model's native code */
   const std::string why=diagram?"the runs follow autoX's diagram"
     :!plan.fresh?"\"Use old I.C.s\" is off, so each run starts where the last ended"
     :xpp::sweep_serial_reason(s);
   if(why.empty())threads=std::min(budget,static_cast<unsigned>(runs));
   else xpp::say_sweep_serial(s,"Range",runs,why);
 }
 std::vector<std::unique_ptr<xpp::Session>> workers;
 for(unsigned w=0;threads>1&&w<threads;w++)workers.push_back(xpp::make_sweep_worker(s,s.data_store->max_rows));

 int early=0; /* the runs that ended before their end without failing */
 std::string first_early; /* the first of them, with its parameter value */

 /* run k taken in, on the main thread: what is shown before it, its outcome o (computed by `compute` here, or already
    by a worker), what is done after it. False when the sweep ends with it. */
 const auto take=[&](std::size_t k,auto &&compute)->bool{
   const int i=static_cast<int>(k%static_cast<std::size_t>(nit+1)),j=static_cast<int>(k/static_cast<std::size_t>(nit+1));
   const int run_number=static_cast<int>(k)+1;
   if(s.integrator->range.movie)clear_draw_window(s);
   if(cycle)s.plot_windows->current->color[0]=xpp::xppaut_color(icol+1);
   icol++;
   if(icol==10)icol=0;
   s.numerics->last_seed=plan.seeds[k];
   RangeRuns::Values v{0.0,0.0};
   if(!diagram){
     v=plan.values(i,j);
     if(program.interactive){
       if(plan.rtype!=RANGE_SINGLE)
	 bob=xpp::format("{}={:.16g}  {}={:.16g}",plan.item,v.p,plan.item2,v.p2);
       else
	 bob=xpp::format("{}={:.16g}  i={}",plan.item,v.p,i);
       show_progress(bob);
     }
   }  /* normal range stuff   */
   else {  /* auto range stuff */
     diagram->mark(i);
     get_ic(s,2,x);
     get_val(s.parser,diagram->parameter,&temp);
     bob=xpp::format("{:.230}={:.16g}",diagram->parameter,temp);
     show_progress(bob);
   }
   const std::string where=diagram?xpp::format("{} = {}",diagram->parameter,xpp::number(temp))
     :plan.rtype!=RANGE_SINGLE?xpp::format("{} = {}, {} = {}",plan.item,xpp::number(v.p),plan.item2,xpp::number(v.p2))
     :xpp::format("{} = {}",plan.item,xpp::number(v.p));
   RunOutcome &o=compute();
   if(o.setup_failed){
     failure=o.setup_error;
     complete=false;
     return false;
   }
   if(o.setup_error&&!failure)failure=o.setup_error;
   if(!o.warnings.empty())xpp::log(XPP_LOG_WARN,"{}",o.warnings); /* what a worker logged while it ran, here in run order */
   if(!o.unexpected.empty()){
     failure=xpp::Error{"Range",xpp::format("Range stopped at {} (run {} of {}): a run stopped unexpectedly: {}",where,run_number,runs,o.unexpected),xpp::command_place()};
     ierr=-1;
     complete=false;
     return false;
   }
   if(!workers.empty()){ /* a worker's rows into the Session's store, and where its integration stopped */
     bool room=true;
     while(room&&s.data_store->rows+o.rows>s.data_store->max_rows)
       if(const auto grown=stor_full(s);!grown){
         if(o.run)o.run=std::unexpected(grown.error()); /* the run's own failure, if it had one, is told first */
         room=false;
       }
     if(room&&o.rows>0){
       const std::size_t rows=static_cast<std::size_t>(o.rows);
       for(int c=0;c<=s.model().neq;c++)
	 s.data_store->col[c].store(s.data_store->rows,std::span(o.columns).subspan(static_cast<std::size_t>(c)*rows,rows));
       s.data_store->rows+=o.rows;
       xpp::job::report_rows(s.data_store->rows,s.data_store->col[0][s.data_store->rows-1]);
       rows_stored(s,s.data_store->rows);
     }
     s.integrator->last_time=o.last_time;
   }
   if(!o.run||*o.run==1||xpp::job::cancelled()){
     if(!o.run){
       failure=o.run.error();
       failure->what=xpp::format("Range stopped at {} (run {} of {}): {}",where,run_number,runs,failure->what);
       failure->action.reset(); /* Continue goes on from a single run, not a sweep's */
     }
     ierr=-1;
     complete=false;
     return false;
   }
   progress.runs++;
   progress.steps+=o.stored;
   if(!o.stop_note.empty()){
     const std::string note=xpp::format("{}: {}",where,o.stop_note);
     xpp::log(XPP_LOG_WARN,"Range run {} of {} ended early at {}\n",run_number,runs,note);
     if(early++==0)first_early=note;
   }
   if(s.stochastic->flag)
     append_stoch(s,i,s.data_store->rows);

   if(report.stored)
     if(const xpp::Result<> shown=report.stored(bob);!shown){ /* out of film */
       if(!failure)failure=shown.error();
       complete=false;
       return false;
     }
   refresh_data(s,s.data_store->rows);
   if(s.integrator->adj_range==1){
     bob=xpp::format("{}_{:g}",plan.item,v.p);
     data_get_mytable(s,s.data_store->rows-1);
     compute_one_period(s,static_cast<double>(s.data_store->col[0][s.data_store->rows-1]),s.last_ic.data(),bob.c_str(),report.orbit?*report.orbit:xpp::RunReport{});
   }

   do_this_liaprun(s,v.p);  /* sends parameter and index back */
   if(report.done)report.done(bob);

   if(res==1||s.stochastic->flag)
     {
       if(s.integrator->range.write_files){
         post_process_stuff(s);
         const std::string file = program.interactive
           ? xpp::include_path(s.model().this_file,xpp::files::output_name(s.model().this_file,".dat"))
           : s.batch_out_file;
         write_this_run(s,file.c_str(),j*(nit+1)+i);
         if(report.written)report.written(j*(nit+1)+i);
       }
       s.data_store->rows=0;
     }
   return true;
 };

 std::size_t taken=0;
 if(workers.empty()){
   for(std::size_t k=0;k<run_count;k++){
     RunOutcome o;
     if(!take(k,[&]()->RunOutcome &{
       compute_run(s,x,plan,static_cast<int>(k%static_cast<std::size_t>(nit+1)),static_cast<int>(k/static_cast<std::size_t>(nit+1)),k,o);
       return o;
     }))break;
     taken++;
   }
 }
 else {
   const std::size_t window=xpp::sweep_window(threads);
   std::vector<RunOutcome> slots(window);
   const auto compute_on_worker=[&](unsigned w,std::size_t k){
     RunOutcome &o=slots[k%window];
     o=RunOutcome{};
     xpp::Session &worker=*workers[w];
     try {
       std::string table; /* log_autox's, which a run never writes */
       const xpp::LogHold hold(table,o.warnings);
       worker.data_store->rows=0;
       compute_run(worker,worker.data_store->current.data(),plan,static_cast<int>(k%static_cast<std::size_t>(nit+1)),static_cast<int>(k/static_cast<std::size_t>(nit+1)),k,o);
       o.rows=worker.data_store->rows;
       const std::size_t rows=static_cast<std::size_t>(o.rows),columns=static_cast<std::size_t>(worker.model().neq)+1;
       o.columns.resize(rows*columns);
       for(std::size_t c=0;c<columns;c++)
	 for(std::size_t r=0;r<rows;r++)o.columns[c*rows+r]=worker.data_store->col[c][r];
     } catch (const std::bad_alloc &) {
       xpp::out_of_memory("a run of a sweep");
     } catch (const std::exception &e) {
       o.unexpected=e.what();
     }
   };
   const xpp::Result<bool> ran=xpp::run_ordered(run_count,threads,compute_on_worker,[&](std::size_t k){
     const bool go=take(k,[&]()->RunOutcome &{return slots[k%window];});
     if(go)taken++;
     return go;
   });
   if(!ran&&!failure){
     failure=ran.error();
     complete=false;
   }
   if(ran&&*ran){ /* the / key ended the sweep, as it ends one on a thread of its own */
     s.numerics->endsing=true;
     ierr=-1;
     complete=false;
   }
   if(complete&&taken<run_count&&xpp::job::cancelled()){ /* Stop came before the next run was taken in */
     ierr=-1;
     complete=false;
   }
 }
 workers.clear();
 if(early>0)bottom_msg(0,xpp::format("{} of {} runs ended early; the first at {}",early,runs,first_early));
 if(oldic==1)get_ic(s,1,x);
 else set_initial_data(s,{x,static_cast<size_t>(s.model().node+s.model().nmarkov)});
 if(s.integrator->range.type==PARAM)set_val(s.parser,s.integrator->range.item,temp);
 if(s.integrator->range.rtype!=RANGE_SINGLE)
   if(s.integrator->range.type2==PARAM)set_val(s.parser,s.integrator->range.item2,temp2);
 evaluate_derived(s.parser);
s.plot_windows->current->color[0]=color;
 s.numerics->inflag=true;
 /* whichever runs were computed and wherever, the generator ends seeded for the next run: the same after any thread count */
 s.random.seed(s.numerics->rand_seed);

 ping();
 s.integrator->adj_range=0;
 if(s.stochastic->flag)
   do_stats(s,ierr);

 if(failure)return std::unexpected(std::move(*failure));
 return(ierr);
}

} // namespace

namespace {
/* the sweep over the range settings made: its integrations, the movie's frames; done (when given) what they completed */
xpp::Result<int> sweep_range(xpp::Session &s, double *x, const xpp::DiagramRange *diagram, RangeProgress *done, const xpp::RunReport &report)
{
 RangeProgress progress;
 const xpp::Result<int> r=range_sweep(s,x,diagram,report,show_range_progress,progress);
 if(done)*done=progress;
 return r;
}
}

int do_range(xpp::Session &s, double *x, bool two, const xpp::RunReport &report, const xpp::DiagramRange *diagram)
{
 if(!two){
   s.integrator->range.rtype=RANGE_SINGLE;
   if(set_up_range(s)==0)return(-1);
 }
 else{
   if(set_up_range2(s)==0)return -1;
 }
 const xpp::Result<int> r=sweep_range(s,x,diagram,nullptr,report);
 if(!r){
   xpp::show_error(r.error());
   return -1;
 }
 return *r;
}

Result<> write_equilibrium(xpp::Session &s, const char *name, int shoot, bool confirm, const RunReport &report)
{
  const int n=s.model().node;
  std::vector<double> x(s.last_ic.begin(),s.last_ic.begin()+n),er(n),em(n);
  int ierr;

  const auto found=do_sing_info(s,x.data(),s.numerics->equilibria.jacobian_step,s.numerics->equilibria.newton_tolerance,s.numerics->equilibria.search_limit,s.numerics->equilibria.max_iterations,n,er.data(),em.data(),&ierr);
  if(!found)return std::unexpected(found.error());
  if(ierr!=0)return fail("equilibrium.export","Could not compute equilibrium",command_place());
  Result<> opened;
  const std::unique_ptr<xpp::Sink> w=confirm?open_sink_replacing(*s.model().fs,name,false,&opened):open_sink_asking(*s.model().fs,name,false,&opened);
  if(!opened)return opened;
  if(!w)return fail("equilibrium.export","Output was not opened",command_place());
  for(int i=0;i<n;i++)
    w->print("{:g} {:g} {:g}\n",x[i],er[i],em[i]);
  if(const auto saved=commit_save(*w);!saved)return saved;
  if(shoot)return save_batch_shoot(s,name,confirm,report);
  return {};
}
  
int write_this_run(xpp::Session &s, const char *file, int i)
{
  if(!s.integrator->suppress_out){
  std::string outfile=xpp::files::run_file_name(file,i);
  if(!save_ready(s.data->view.iend>s.data->view.istart))return -1;
  const std::unique_ptr<xpp::Sink> w=xpp::open_sink_asking(*s.model().fs,outfile,false,nullptr,false,s.output_place);
  if(!w)return -1;
  xpp::ok_or_show(xpp::save_mytable_data(s,*w));
  }
  return(1);
}
  
xpp::Result<> run_allowed(const xpp::Session &s, std::string_view command)
{
  if(s.numerics->fft||s.numerics->hist)
    return xpp::fail(std::string(command),"The data table shows a Fourier or histogram result: choose Stochastic, Data to see the trajectory again before running",xpp::command_place());
  return {};
}

namespace {

/* the state every run begins in: its first integration afresh, no range
   going, no failure kept from the one before */
void reset_run_state(xpp::Session &s)
{
  s.integrator->my_start=1;
  s.integrator->range_flag=0;
  s.parser.step_error.reset();
  reset_dae(s);
}

/* a run from the initial data empties what the last one stored */
void clear_for_run(xpp::Session &s)
{
  data_back(s);
  wipe_rep(*s.data);
  s.data_store->current_time=s.numerics->start_time;
  s.numerics->storflag=true;
  s.numerics->poiext=0;
  s.data_store->rows=0;
  reset_data(s);
}


/* Dt back as it was when the run ends, whichever way it does */
struct DtRestore {
  xpp::Session &s;
  double dt;
  explicit DtRestore(xpp::Session &session) : s(session), dt(session.numerics->delta_t) {}
  ~DtRestore(){s.numerics->delta_t=dt;}
};

/* the delay history restarted from the initial data of a model with delays */
xpp::Result<> restart_delays(xpp::Session &s)
{
  if(s.delay->flag)return do_init_delay(s,s.numerics->delay);
  return {};
}

} // namespace

xpp::Result<bool> run_from(xpp::Session &s, RunFrom from, const RunReport &report, bool reset_integrals)
{
  const int oldstart=s.integrator->my_start;
  const DtRestore dt(s);
  double *const x=&s.data_store->current[0];
  if(auto allowed=run_allowed(s,"run");!allowed)return std::unexpected(allowed.error());
  if((from==RunFrom::last||from==RunFrom::shift)&&!s.numerics->inflag)
    return xpp::fail("run","No prior solution: run once from the initial data first",xpp::command_place());
  reset_run_state(s);
  clear_for_run(s);
  switch(from){
  case RunFrom::last:
  case RunFrom::shift:
    xpp::push_value_undo(s); /* Run from last state overwrites the initial conditions */
    set_initial_data(s,{x,static_cast<size_t>(s.model().node+s.model().nmarkov)});
    if(from==RunFrom::shift){
      s.numerics->start_time=s.integrator->last_time;
      s.data_store->current_time=s.numerics->start_time;
    }
    if(xpp::solver_info(s.numerics->method).traits.integral_history&&oldstart==0&&!reset_integrals)
      s.integrator->my_start=oldstart;
    break;
  case RunFrom::old:
    get_ic(s,1,x);
    if(auto delays=restart_delays(s);!delays)return std::unexpected(delays.error());
    set_init_guess(s);
    break;
  case RunFrom::backward:
    s.numerics->delta_t=-fabs(s.numerics->delta_t);
    [[fallthrough]];
  case RunFrom::initial:
    set_init_guess(s);
    get_ic(s,2,x);
    if(auto delays=restart_delays(s);!delays)return std::unexpected(delays.error());
    break;
  }
  return integrate_from(s,x,report);
}

xpp::Result<int> run_range(xpp::Session &s, RangeProgress *done, const xpp::RunReport &report)
{
  double *const x=&s.data_store->current[0];
  if(auto allowed=run_allowed(s,"range");!allowed)return std::unexpected(allowed.error());
  reset_run_state(s);
  clear_for_run(s);
  s.integrator->range_flag=1; /* a range's runs are not stopped at the state limit */
  return sweep_range(s,x,nullptr,done,report);
}

namespace {
/* the two states a graph plots against each other, or why it has no such pair */
xpp::Result<Plane> plane_of(const xpp::Session &s, const GRAPH &graph)
{
  const int h=graph.xv[0]-1,v=graph.yv[0]-1;
  if(h<0||h>=s.model().node||v<0||v>=s.model().node||graph.grtype>=5||v==h)
    return xpp::fail("initial conditions","Not in a usable 2D plane: the window must plot two states against each other",xpp::command_place());
  return Plane{h,v};
}
}

xpp::Result<Plane> window_plane(const xpp::Session &s, int graph)
{
  return plane_of(s,s.plot_windows->graph[static_cast<std::size_t>(graph)]);
}

xpp::Result<bool> run_from_click(xpp::Session &s, Plane plane, double horizontal, double vertical, const RunReport &report)
{
  double *const x=&s.data_store->current[0];
  get_ic(s,1,x);
  x[plane.horizontal]=horizontal;
  x[plane.vertical]=vertical;
  s.last_ic[static_cast<std::size_t>(plane.horizontal)]=horizontal;
  s.last_ic[static_cast<std::size_t>(plane.vertical)]=vertical;
  if(auto delays=restart_delays(s);!delays)return std::unexpected(delays.error());
  s.integrator->my_start=1;
  s.data_store->current_time=s.numerics->start_time;
  return integrate_from(s,x,report);
}

xpp::Result<int> run_from_clicks(xpp::Session &s, Plane plane, std::span<const std::pair<double,double>> clicks, bool suppress_bounds, const RunReport &report)
{
  if(auto allowed=run_allowed(s,"initial conditions");!allowed)return std::unexpected(allowed.error());
  reset_run_state(s);
  clear_for_run(s);
  struct Unbounded {
    xpp::Session &s;
    bool on;
    Unbounded(xpp::Session &session,bool unbounded) : s(session), on(unbounded){if(on)s.integrator->suppress_bounds=1;}
    ~Unbounded(){if(on)s.integrator->suppress_bounds=0;}
  } unbounded(s,suppress_bounds);
  int done=0;
  for(const auto &[horizontal,vertical]:clicks){
    if(xpp::job::cancelled())break;
    if(auto run=run_from_click(s,plane,horizontal,vertical,report);!run)return std::unexpected(run.error());
    done++;
  }
  return done;
}

xpp::Result<bool> run_from_manifold(xpp::Session &s, int index, const RunReport &report)
{
  if(s.manifolds->ic_flag==0)
    return xpp::fail("shoot","No shooting data available: run the equilibria's shooting (Sing pts, Shoot) first",xpp::command_place());
  if(index<1||index>s.manifolds->count)
    return xpp::fail("shoot",xpp::format("Manifold {} is out of range: the shooting found {}",index,s.manifolds->count),xpp::command_place());
  if(auto allowed=run_allowed(s,"shoot"); !allowed)return std::unexpected(allowed.error());
  set_initial_data(s,std::span(s.manifolds->ic[static_cast<std::size_t>(index-1)]).first(static_cast<std::size_t>(s.model().node)));
  return run_from(s,RunFrom::old,report);
}

void run_now(xpp::Session &s, const RunReport &report)
{
  double *const x=&s.data_store->current[0];
  reset_run_state(s);
  s.data_store->current_time=s.numerics->start_time;
  get_ic(s,2,x);
  s.numerics->storflag=true;
  s.numerics->poiext=0;
  s.data_store->rows=0;
  reset_data(s);
  xpp::ok_or_show(integrate_from(s,x,report));
}
xpp::Result<bool> integrate_from(xpp::Session &s, double *x, const RunReport &report)
{
  int i;

  seed_this_run(s);
  do_start_flags(s,x,&s.data_store->current_time);
   if(stored_from(s.numerics->store_from,s.data_store->current_time,s.numerics->delta_t)&&s.numerics->storflag&&s.numerics->poincare_map==PoincareMap::none)
    {
      s.data_store->col[0][0]=s.data_store->current_time;
      extra(s,x,s.data_store->current_time,s.model().node,s.model().neq);
      for(i=0;i<s.model().neq;i++)s.data_store->col[1+i][0]=x[i];
      s.data_store->rows=1;
    }
  if(s.parser.step_error){
    refresh_data(s,s.data_store->rows);
    return take_step_error(s);
  }
 
  const xpp::Result<int> r=[&]{
    const xpp::Job job; /* Abort cancels it (xpp_job.h) */
    return integrate(s,&s.data_store->current_time,x,s.numerics->total_time,s.numerics->delta_t,1,s.numerics->store_every,&s.integrator->my_start);
  }();
  
  ping();
  s.numerics->inflag=true;
  if(report.stored)xpp::ok_or_show(report.stored({}));
  refresh_data(s,s.data_store->rows);
  const bool early=r&&(*r!=0||!s.integrator->stop_note.empty());
  show_stop_note(s);
  if(program.interactive){
  if(report.done)report.done({});
  redraw_ics();
  }
  if(!r)return std::unexpected(r.error());
  return early;
}

int usual_integrate_stuff(xpp::Session &s, double *x, const xpp::RunReport &report)
{
  return xpp::ok_or_show(integrate_from(s,x,report))?0:1;
}
Result<bool> continue_to(Session &s, double until, const RunReport &report)
{
  /* one output group: store_every steps of Dt (of 1, for a map); the run ends on a group boundary,
     so the end is rounded up to the output grid and the stride is honoured */
  const double interval=(s.integrator->solver->traits().discrete?1:s.numerics->delta_t)*s.numerics->store_every;
  const double duration=until-s.data_store->current_time;
  if(!s.numerics->inflag||s.numerics->fft||s.numerics->hist)
    return fail("continue","Needs a prior trajectory; disable FFT and histogram modes",command_place());
  if(!std::isfinite(until)||!std::isfinite(duration)||duration<=0||!std::isfinite(interval)||interval<=0||
     s.numerics->delta_t<=0||duration/interval>MAX_RUN_STEPS)
    return fail("continue","End time must be beyond the current time with a positive Dt and a step count that fits the integrator",command_place());
  /* Round up to the next grid point: never short of the end asked for. */
  const double length=std::max(std::ceil(duration/interval-CONTINUE_GRID_TOLERANCE),1.0)*interval;
  wipe_rep(*s.data);
  data_back(s);
  s.integrator->my_start=1;  /*  I know it is wasteful to restart, but lets be safe.... */
  const xpp::Result<int> r=integrate(s,&s.data_store->current_time,&s.data_store->current[0],length,s.numerics->delta_t,1,s.numerics->store_every,&s.integrator->my_start);
  ping();
  if(report.stored)xpp::ok_or_show(report.stored({}));
  refresh_data(s,s.data_store->rows);
  const bool early=r&&(*r!=0||!s.integrator->stop_note.empty());
  show_stop_note(s);
  if(!r)return std::unexpected(r.error());
  return early;
}

Result<bool> continue_for(Session &s, double extra, const RunReport &report)
{
  return continue_to(s,s.data_store->current_time+extra,report);
}

Result<bool> continue_total(Session &s, const RunReport &report)
{
  return continue_for(s,s.numerics->total_time,report);
}

Result<> run_to_steady_state(Session &s, SteadyStateSettings settings, const RunReport &report)
{
  const double interval=s.integrator->solver->traits().discrete?(s.numerics->delta_t>0?1:-1):s.numerics->delta_t;
  if(auto valid=validate_steady_state(settings,interval,s.numerics->start_time);!valid)return valid;
  if(s.model().node+s.model().nmarkov==0||s.numerics->poincare_map!=PoincareMap::none||s.numerics->fft||s.numerics->hist)
    return fail("steady","Needs model states and ordinary trajectory output (disable Poincare, FFT and histogram modes)",command_place());
  if(!s.integrator->solver->traits().fixed_step&&interval<std::fabs(s.numerics->min_step))
    return fail("steady","Dt must be at least the solver's minimum step",command_place());
  /* One core comparison per configured Dt, regardless of output stride.
     Restore the user's duration/stride before deferred settings apply. */
  struct Restore {
    Session &s;
    double duration;
    int stride;
    ~Restore(){s.numerics->total_time=duration;s.numerics->store_every=stride;s.integrator->steady.reset();}
  } restore{s,s.numerics->total_time,s.numerics->store_every};
  s.numerics->total_time=std::floor(settings.maximum/interval)*interval;
  s.numerics->store_every=1;
  if(const Result<> room=s.data_store->fit(s.model().neq+1,DataStore::start_rows);!room)return std::unexpected(room.error()); /* it thins at start_rows */
  s.integrator->steady.emplace(settings);
  s.integrator->steady_result=SteadyStateResult{"failed",settings.decimals};
  s.integrator->last_time=s.numerics->start_time;
  const Result<bool> run=run_from(s,RunFrom::initial,report);
  s.integrator->steady_result->time=s.integrator->last_time;
  if(!run)return std::unexpected(run.error());
  return {};
}

void compute_one_period(xpp::Session &s, double period,double *x,const char *name, const xpp::RunReport &report)
{
  const PoincareMap opm=s.numerics->poincare_map;
  double ot=s.numerics->store_from,ote=s.numerics->total_time;
  const auto restore = [&]() {
    s.numerics->store_from=ot;
    s.numerics->poincare_map=opm;
    s.numerics->total_time=ote;
  };
  s.numerics->store_from=0;
  s.numerics->start_time=0;
  s.data_store->current_time=0;
  s.numerics->total_time=period;
  s.numerics->poincare_map=PoincareMap::none;
  reset_data(s);

  usual_integrate_stuff(s,x,report);
  {
    const std::unique_ptr<xpp::Sink> w=xpp::ask_output_sink(s,"Save orbit",".dat",xpp::format("orbit-{}",name));
    if(w){
      if (!xpp::ok_or_show(xpp::save_mytable_data(s,*w))) { restore(); return; }
    }
    else{
      restore();
      return;
    }
  }
  if(!xpp::ok_or_show(new_adjoint(s))){restore();return;}
  {
    const std::unique_ptr<xpp::Sink> w=xpp::ask_output_sink(s,"Save adjoint",".dat",xpp::format("adjoint-{}",name));
    if(w){
      const bool saved=xpp::ok_or_show(xpp::save_mytable_data(s,*w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }
  if(!xpp::ok_or_show(new_h_fun(s,false))){restore();return;}
  {
    const std::unique_ptr<xpp::Sink> w=xpp::ask_output_sink(s,"Save hfun",".dat",xpp::format("hfun-{}",name));
    if(w){
      const bool saved=xpp::ok_or_show(xpp::save_mytable_data(s,*w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }

  reset_data(s);

  restore();

}

} // namespace xpp
