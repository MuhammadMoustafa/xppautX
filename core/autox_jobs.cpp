#include "autox_unknowns.h"
#include <algorithm>
#include <vector> /* first: autox_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autox_jobs.h"
#include "session.h"
#include "autox_evd.h"
#include "autox_c.h"
#include "autox_parallel.h"
#include "autox_stop.h"
#include "diagram.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_ui.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace xpp {

namespace {

/* How a job's branch and label numbers move to follow the jobs before it: a number above what the diagram and fort.3
   held when the jobs started (the base newlab counts from, AutoxRun::base_branch) is a number the job made, and shifts
   by offset. */
struct Renumber {
  int base_branch=0,base_label=0;
  int branch_offset=0,label_offset=0;
  bool identity() const { return branch_offset==0&&label_offset==0; }
  static int moved(int value, int base, int offset)
  {
    const int magnitude=std::abs(value);
    return magnitude>base?(value<0?-(magnitude+offset):magnitude+offset):value;
  }
  int branch(int value) const { return moved(value,base_branch,branch_offset); }
  int label(int value) const { return moved(value,base_label,label_offset); }
};

/* the four integers a row of fort.7 (.b) starts with, and where the fourth ends: the row's branch, point, type and
   label as autox_branch_row_key prints them */
struct Key {
  long field[4]{};
  std::size_t end=0;
};

std::optional<Key> read_branch_row_key(std::string_view line)
{
  Key key;
  std::size_t at=0;
  for(long &value:key.field){
    const auto number=autox_integer_at(line,at);
    if(!number)return std::nullopt;
    value=number->first;
    at=number->second;
  }
  key.end=at;
  return key;
}

/* the points a job stored, as (branch, point) pairs, once its numbers moved: what its .b and .s records may name when the
   diagram's limit cut it short */
using StoredPoints=std::set<std::pair<int,int>>;

/* Which file a renumbering reads: .s (fort.8: a header line then its rows, which are counted), .b (fort.7: a record per
   line, headers and column titles among them) or .d (fort.9: lines of text, autox_renumbered_line's). */
enum class RunFile { solutions, branches, diagnostics };

/* the file of a job with its branch and label numbers moved by r, and, when only some of the job's points were stored
   (the diagram's limit), with the records of the others left out of the .b and .s */
Result<> renumber_file(const AutoxRun &run, const std::string &path, RunFile kind, const Renumber &r, const StoredPoints *stored)
{
  if((r.identity()&&!stored)||!files::exists(path))return {};
  xpp::Writer out(path);
  if(!out)return fail("autox","cannot write autoX's file",Place{path,0,0,{}});
  { /* the reader is closed before the rename into place (Windows refuses to replace an open file) */
    xpp::LineReader in(path);
    if(!in)return fail("autox","cannot read autoX's file to number its branches and labels after those before it",Place{path,0,0,{}});
    const auto branch=[&](int value){return r.branch(value);};
    const auto label=[&](int value){return r.label(value);};
    const auto kept=[&](long point_branch,long point){
      return !stored||stored->contains({static_cast<int>(std::abs(point_branch)),static_cast<int>(std::abs(point))});
    };
    int line_number=0;
    long rows_left=0;
    bool keeping=true; /* the rows of the solution whose header was last read */
    while(const auto line=in.next()){
      line_number++;
      const std::string text(*line);
      if(rows_left>0){ /* a solution's rows are copied as they are, or left out with their header */
        rows_left--;
        if(keeping)out.print("{}\n",text);
        continue;
      }
      if(kind==RunFile::diagnostics){
        out.print("{}\n",autox_renumbered_line(text,branch,label));
        continue;
      }
      if(kind==RunFile::solutions){
        TokenReader fields=TokenReader::of_text(text);
        AutoxSolutionHeader header;
        if(!read_autox_solution_header(fields,header,run.parameter_count))
          return fail("autox","not a solution's header line (autoX's file is damaged)",Place{path,line_number,0,text});
        header.ibr=r.branch(static_cast<int>(header.ibr));
        header.lab=r.label(static_cast<int>(header.lab));
        rows_left=header.nskip;
        keeping=kept(header.ibr,header.ntot);
        if(keeping)out.print("{}",autox_solution_header_text(header));
        continue;
      }
      const std::optional<Key> key=read_branch_row_key(text);
      if(!key||key->field[0]==0||key->end>=text.size()){ /* a header or a column title */
        out.print("{}\n",text);
        continue;
      }
      const long moved=r.branch(static_cast<int>(key->field[0]));
      if(kept(moved,key->field[1]))
        out.print("{}{}\n",autox_branch_row_key(moved,key->field[1],key->field[2],r.label(static_cast<int>(key->field[3]))),text.substr(key->end));
    }
  }
  return out.commit();
}

/* a job's table lines with their branch and label numbers moved, as its .d is */
std::string renumbered_table(const std::string &table, const Renumber &r)
{
  const auto branch=[&](int value){return r.branch(value);};
  const auto label=[&](int value){return r.label(value);};
  std::string out;
  std::size_t at=0;
  while(at<table.size()){
    const std::size_t end=table.find('\n',at);
    const bool complete=end!=std::string::npos;
    out+=autox_renumbered_line(std::string_view(table).substr(at,(complete?end:table.size())-at),branch,label);
    if(complete)out+='\n';
    at=complete?end+1:table.size();
  }
  return out;
}

/* one direction of the run: a job */
struct Direction {
  double sign=1; /* the sign of step_size */
  std::unique_ptr<AutoxRun> run=std::make_unique<AutoxRun>();
  std::optional<Error> failed;
  std::string table,warnings; /* its table lines and its warnings, kept while it runs, for the join to write */
  bool skipped=false; /* its direction leaves the parameter window at the start: not run */
  bool started=false;
};

/* the numerics of a job, whatever it throws */
std::optional<Error> run_numerics(AutoxRun &run) noexcept
{
  try {
    return autox_numerics(run);
  } catch (const std::exception &e) {
    return Error{"autoX",xpp::format("a job stopped unexpectedly: {}",e.what()),command_place()};
  } catch (...) {
    return Error{"autoX","a job stopped unexpectedly",command_place()};
  }
}

/* Runs the jobs not skipped: at once, one thread each (the main thread waits and polls the front end for Abort), or one
   after another on the main thread, before_each called before each. A job whose turn comes after the run was stopped is
   never started (`started` stays false). */
void run_jobs(std::vector<Direction> &jobs, bool at_once, const std::function<void()> &before_each)
{
  if(!at_once){
    for(Direction &job:jobs){
      if(job.skipped||xpp::job::cancelled())continue;
      before_each();
      job.started=true;
      const AutoxLogHold hold(job.table,job.warnings);
      job.failed=run_numerics(*job.run);
      job.run->fp8.reset();
    }
    return;
  }
  std::mutex mutex;
  std::condition_variable ended;
  std::size_t running=0;
  std::vector<std::thread> threads;
  threads.reserve(jobs.size());
  for(Direction &job:jobs){
    if(job.skipped||xpp::job::cancelled())continue;
    job.started=true;
    {
      const std::lock_guard lock(mutex);
      running++;
    }
    try {
      threads.emplace_back([&mutex,&ended,&running,&job]{
        {
          const AutoxLogHold hold(job.table,job.warnings);
          job.failed=run_numerics(*job.run);
          job.run->fp8.reset();
        }
        const std::lock_guard lock(mutex);
        running--;
        ended.notify_one();
      });
    } catch (const std::system_error &e) {
      job.failed=Error{"autoX",xpp::format("cannot start a thread for a job: {}",e.what()),command_place()};
      const std::lock_guard lock(mutex);
      running--;
    }
  }
  std::unique_lock lock(mutex);
  while(running>0){
    ended.wait_for(lock,std::chrono::duration<double>(INPUT_POLL_SECONDS));
    lock.unlock();
    int stop;
    byeautox_(&stop); /* the front end's Abort, which cancels every job */
    lock.lock();
  }
  lock.unlock();
  for(std::thread &thread:threads)thread.join();
}

/* The equilibrium start Newton's method refines when the initial conditions are not a steady state (autox_evd.cpp), done
   once here, on the Session, before the jobs begin: they all start from it. The error is the run's refusal; no value, a
   start that needed none (or is not an equilibrium start). */
Result<std::optional<std::vector<double>>> refined_equilibrium_start(Session &s)
{
  const AutoxState &state=s.autox_state;
  const std::size_t n=static_cast<std::size_t>(s.model().node);
  AutoxRun probe;
  probe.session=&s;
  probe.parameter_count=state.parameter_layout.size();
  iap_type iap{};
  iap.run=&probe;
  iap.irs=state.bifur.irs;
  iap.ips=state.bifur.ips;
  iap.ndm=iap.ndim=s.model().node;
  iap.nfpr=1;
  rap_type rap{};
  rap.epsu=state.bifur.epsu;
  std::vector<integer> icp{state.parameter_layout.solver_index(state.axes().icp1)};
  std::vector<double> par(static_cast<std::size_t>(probe.parameter_count));
  autox_fill_parameters(probe,par.data());
  std::vector<double> u(s.last_ic.begin(),s.last_ic.begin()+static_cast<std::ptrdiff_t>(n));
  try {
    autox_check_start_equilibrium(iap,rap,u.data(),par.data(),icp.data());
  } catch (const AutoxFailed &e) {
    return std::unexpected(autox_shown_failure(e.error));
  }
  if(!probe.stop.start_refined)return std::optional<std::vector<double>>();
  return std::optional<std::vector<double>>(std::move(u));
}

Result<> autox_run_directions(Session &s, int iold, int isave)
{
  if(auto prepared=autox_run_prologue(s,isave);!prepared)return prepared;
  const AutoxDepth depth(s);
  create_autox_file_name(s);
  /* the jobs evaluate on copies of the model's constants when the model allows it; else on the Session's own, one at a
     time, each from the constants the run began with, which are the Session's again when the run ends */
  const bool isolated=autox_parallel_ready(s);
  const std::vector<double> constants=s.parser.constants,variables=s.parser.variables;
  const auto restore_constants=[&]{
    s.parser.constants=constants;
    s.parser.variables=variables;
  };
  std::vector<Direction> jobs(2);
  jobs[0].sign=1;
  jobs[1].sign=-1;
  std::optional<std::vector<double>> refined;
  {
    const xpp::Job job; /* Abort cancels it (xpp_job.h) */
    const Computation computing; /* what Abort stops (xpp_job.h) */
    auto started=refined_equilibrium_start(s);
    restore_constants();
    if(!started)return std::unexpected(started.error());
    refined=std::move(*started);
  }
  for(Direction &job:jobs){
    if(auto ready=autox_set_up_run(s,*job.run,iold);!ready)return ready;
    job.run->params.ds=job.sign*std::abs(s.autox_state.bifur.ds);
    job.run->refined_start=refined;
  }

  /* a direction that leaves the parameter window at the start is not run (it says so); when both do, only the first is
     run, which is refused as a single run is, once */
  const AutoxState &state=s.autox_state;
  const int principal=state.axes().icp1;
  const double start=constants[s.model().upar_con[principal]];
  for(Direction &job:jobs)job.skipped=autox_start_points_away(start,state.bifur.rl0,state.bifur.rl1,job.run->params.ds);
  if(jobs[0].skipped&&jobs[1].skipped)jobs[0].skipped=false;
  else for(Direction &job:jobs)
    if(job.skipped)
      autox_stop_note(*job.run,xpp::format("step_size = {} is not followed: the start {} = {} is at or outside parameter_min = {} and parameter_max = {}, where that direction leaves the window",
                                           xpp::number(job.run->params.ds),s.model().upar_names[principal],xpp::number(start),xpp::number(state.bifur.rl0),xpp::number(state.bifur.rl1)));

  const std::size_t wanted=static_cast<std::size_t>(std::count_if(jobs.begin(),jobs.end(),[](const Direction &j){return !j.skipped;}));
  const unsigned budget=state.threads?state.threads:AutoxParallel::physical_threads();
  const bool at_once=isolated&&budget>1&&wanted>1;
  for(Direction &job:jobs){
    AutoxRun &run=*job.run;
    run.job=true;
    run.on_thread=at_once;
    if(at_once)run.collocation_threads=std::min<unsigned>(AutoxParallel::max_threads,std::max<unsigned>(1,budget/static_cast<unsigned>(wanted)));
    if(isolated){
      run.pure_rhs=true;
      run.rhs_constants=constants;
      run.rhs_variables=variables;
    }
  }
  {
    const xpp::Job job;
    const Computation computing;
    run_jobs(jobs,at_once,restore_constants);
  }
  /* the Session's constants end as an ordinary run leaves them, at the last evaluated point of the last job that ran (a
     job that evaluated on the Session's own left them there) */
  const auto last=std::find_if(jobs.rbegin(),jobs.rend(),[](const Direction &j){return j.started;});
  if(last==jobs.rend())restore_constants();
  else{
    if(isolated){
      s.parser.constants=last->run->rhs_constants;
      s.parser.variables=last->run->rhs_variables;
    }
    std::vector<double> before(static_cast<std::size_t>(s.model().nupar)); /* the start's parameters, as do_autox keeps them */
    for(int k=0;k<s.model().nupar;k++)before[static_cast<std::size_t>(k)]=constants[s.model().upar_con[k]];
    autox_restore_finite_pars(s,before.data()); /* leave no NaN parameter behind (QA SCI-001) */
  }

  /* the join: +DS, then -DS, each numbered as a run after the ones before it */
  std::optional<Error> reported;
  Renumber moved;
  const auto first=std::find_if(jobs.begin(),jobs.end(),[](const Direction &j){return j.started;});
  if(first!=jobs.end()){
    moved.base_branch=first->run->base_branch;
    moved.base_label=first->run->base_label;
  }
  const std::string limit_text=autox_diagram_limit_message();
  bool diagram_full=false,limit_shown=false;
  std::optional<std::pair<int,int>> last_stored; /* the last point of the diagram, which a stopped run reports */
  for(Direction &job:jobs){
    AutoxRun &run=*job.run;
    if(!job.started){
      autox_stop_merge(s.autox_state.stop,std::move(run.stop));
      continue;
    }
    if(job.failed&&job.failed->what==limit_text){ /* the diagram's limit is one error for the run, shown once */
      if(limit_shown)job.failed.reset();
      limit_shown=true;
    }
    const int saving=s.autox_state.diag_flag&&isave==OVERWRITE?APPEND:isave;
    const Renumber mine=moved;
    int branches=moved.base_branch,labels=moved.base_label;
    StoredPoints stored;
    bool cut=diagram_full; /* some of this job's points were not stored */
    for(AutoxBufferedPoint &held:run.points){
      DIAGRAM &p=held.point;
      p.ibr=mine.branch(p.ibr);
      p.lab=mine.label(p.lab);
      p.from=mine.label(p.from);
      branches=std::max(branches,std::abs(p.ibr));
      labels=std::max(labels,p.lab);
      if(!diagram_full&&autox_diagram_full(s.diagram.points.size())){ /* the limit is the whole run's: both jobs' points and the diagram's */
        diagram_full=cut=true;
        if(!limit_shown&&!job.failed)job.failed=Error{"autoX",limit_text,command_place()};
        limit_shown=true;
      }
      if(diagram_full)continue;
      stored.insert({std::abs(p.ibr),std::abs(p.ntot)});
      autox_store_point(s,p,held.new_segment);
      last_stored=std::pair<int,int>(std::abs(p.ibr),std::abs(p.ntot));
    }
    for(const auto &[file,kind]:{std::pair{run.fort7,RunFile::branches},std::pair{run.fort8,RunFile::solutions},std::pair{run.fort9,RunFile::diagnostics}})
      if(auto numbered=renumber_file(run,file,kind,mine,kind==RunFile::diagnostics||!cut?nullptr:&stored);!numbered&&!reported)reported=numbered.error();
    /* the next job numbers after this one's */
    moved.branch_offset=branches-moved.base_branch;
    moved.label_offset=labels-moved.base_label;
    if(!job.table.empty())xpp::log_autox("{}",renumbered_table(job.table,mine));
    if(!job.warnings.empty())xpp::log(XPP_LOG_WARN,"{}",renumbered_table(job.warnings,mine));
    if(job.failed)job.failed->what=autox_renumbered_line(job.failed->what,[&](int value){return mine.branch(value);},[&](int value){return mine.label(value);}); /* "Branch N point M: nonfinite" */
    run.stop.last_br=mine.branch(static_cast<int>(run.stop.last_br)); /* why this job's branches ended, as the diagram numbers them */
    for(std::string &note:run.stop.notes)
      note=autox_renumbered_line(note,[&](int value){return mine.branch(value);},[&](int value){return mine.label(value);});
    if(auto ended=autox_end_run(s,run,std::move(job.failed),saving);ended&&!reported)reported=std::move(ended);
  }
  if(last_stored)xpp::job::report_point(last_stored->first,last_stored->second);
  ping();
  redraw_params();
  if(reported)return std::unexpected(std::move(*reported));
  return {};
}

} // namespace

Result<> autox_run_from_current_state(Session &s, int iold, int isave)
{
  return s.autox_state.follow_all_branches?autox_run_directions(s,iold,isave):do_autox(s,iold,isave,s.autox_state.bifur.itp);
}

} // namespace xpp
