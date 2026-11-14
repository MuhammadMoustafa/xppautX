#include <algorithm>
#include <vector> /* first: autox_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include <atomic>
#include <deque>
#include "autox_jobs.h"
#include "expr_native.h"
#include "autox_queue.h"
#include "session.h"
#include "autox_state.h"
#include "expr.h"
#include "autox_data.h"
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
#include "model.h"
#include "autox_codes.h"
#include "autox_ends.h"
#include "autox_nox.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <cstddef>
#include <exception>
#include <expected>
#include <span>
#include <utility>

namespace xpp {

namespace {

/* How a job's branch and label numbers move to follow the jobs before it: a number above what the diagram and fort.3
   held when the jobs started (the base new_restart_label counts from, AutoxRun::base_branch) is a number the job made, and shifts
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
      if(rows_left>0){ /* a solution's rows are copied as they are, or left out with their header */
        rows_left--;
        if(keeping)out.write_line(*line);
        continue;
      }
      if(kind==RunFile::diagnostics){
        out.write_line(autox_renumbered_line(*line,branch,label));
        continue;
      }
      const std::string text(*line);
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
        out.write_line(text);
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

/* The equilibrium start Newton's method refines when the initial conditions are not a steady state (autox_evd.cpp), done
   once here, on the Session, before the jobs begin: they all start from it. The error is the run's refusal; no value, a
   start that needed none (or is not an equilibrium start). */
Result<std::optional<std::vector<double>>> refined_equilibrium_start(Session &s)
{
  const AutoxState &state=*s.autox_state;
  const std::size_t n=static_cast<std::size_t>(s.model().node);
  AutoxRun probe;
  autox_bind_session(probe,s);
  probe.parameter_count=state.parameter_layout.size();
  iap_type iap{};
  iap.run=&probe;
  iap.irs=state.bifur.irs;
  iap.ips=state.bifur.ips;
  iap.ndm=iap.ndim=s.model().node;
  iap.nfpr=1;
  rap_type rap{};
  rap.epsu=state.bifur.epsu;
  std::vector<std::ptrdiff_t> icp{state.parameter_layout.solver_index(state.axes().icp1)};
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

/* the kind of calculation a job's points are stored under: what the start of such a run records (autox_run.cpp) */
AutoxCalculation autox_job_calculation(AutoxProblem ips)
{
  return ips==AutoxProblem::Periodic?PE1:ips==AutoxProblem::DiscreteMap?DI1:EQ1;
}

/* one job: a branch run from a start (autox_queue.h gives the start and the order) */
struct QueuedJob {
  AutoxJobKey key;
  int id=0; /* what its live points and the autox.jobs event call it: its place among the jobs made, from 1 */
  AutoxJobStart start; /* what it continues: the problem, the direction and, for a found start, the point */
  QueuedJob *parent=nullptr; /* the job that found its start */
  QueuedJob *twin=nullptr; /* a root's: the root of the other direction, which comes before it (a found start's other direction is autox_start_reversed's) */
  QueuedJob *hopf_before=nullptr; /* the Hopf point before its own on the branch that found both (the pair rule) */
  std::unique_ptr<AutoxRun> run=std::make_unique<AutoxRun>();
  std::optional<Error> failed;
  std::string table,warnings; /* its table lines and its warnings, kept while it runs, for the join to write */
  std::vector<AutoxBufferedPoint> unseen; /* its points stored since the main thread last drew them: the thread records, only the main thread draws (q.mutex) */
  std::size_t drawn=0; /* its points drawn live so far, which autox.jobs announces: never ahead of what the client was sent (the main thread) */
  std::optional<DIAGRAM> latest; /* the last point drawn of it: what the circle shows while it is the earliest running (the main thread) */
  std::vector<AutoxJobStart> made; /* once done: what it made besides its start (autox_end_claims) */
  bool closed=false; /* once done: its branch ended back at its start */
  std::optional<int> first_step; /* a Hopf point's cycles: the sign of the parameter's first step away from the point */
  bool skipped=false; /* never run: its direction leaves the parameter window at the start, or an earlier job made it */
  bool started=false,done=false,expanded=false;
  bool left_out=false; /* the join dropped it: it repeats an earlier job's */
  std::optional<bool> kept; /* once it and every job before it are settled (settled): whether it is one the join keeps (repeats) */
  bool announced=false; /* once done: its early end, if it had one, was shown */
  xpp::job::BranchJob ended; /* once the jobs ended: how it did (ended_as) */
  bool ended_early=false; /* once the jobs ended: it ended with an error of its own (early_end), which the others did not share */
  bool never=false; /* the run was stopped while it queued, so it never started (or, in a replay, it did not in the recording) */
  Renumber numbering; /* the join's, for the jobs that start from this one's labels */
};

/* the jobs, and what running them shares */
struct Queue {
  Session &s;
  std::deque<QueuedJob> jobs; /* only the main thread adds to it, with the mutex held; a job never moves */
  std::mutex mutex; /* guards the jobs' fields the threads write, and the jobs' list */
  std::condition_variable changed;
  std::atomic<std::size_t> running{0};
  bool isolated=false; /* the jobs evaluate on copies of the model's constants (else on the Session's own, in turn) */
  bool at_once=false; /* one thread each, at most `budget` together */
  unsigned budget=1;
  std::atomic<bool> fatal{false}; /* a job ended with an error of the whole run: every job is stopped, as Stop does */
  std::atomic<std::size_t> points_held{0}; /* every job's points together, which with the diagram's own are the run's */
  std::size_t diagram_base=0; /* the diagram's points when the jobs began */
  std::vector<xpp::job::BranchJob> plan; /* a recording's replay: how each job ended (xpp_job.h's stop_jobs); empty otherwise */
  std::map<std::vector<long>,const xpp::job::BranchJob *> plan_of; /* plan by job key: a recording may list a great many jobs */
  XppAutoxJobs shown_counts{-1,-1,-1,{}}; /* the last autox.jobs event sent; a count is never negative, so the first is always sent */
  double live_last=0,counts_last=0; /* when the live points were last drawn and the jobs' points last announced (xpp::every) */
  std::vector<double> constants,variables; /* the Session's, when the run began */
  double epss=0,ds=0; /* special_point_tolerance, the size of step_size */
  explicit Queue(Session &session) : s(session) {}
  void restore_constants() { s.parser.constants=constants; s.parser.variables=variables; }
};

/* a new job at the end of the queue (the mutex held when the jobs are running) */
QueuedJob &make_job(Queue &q)
{
  QueuedJob &job=q.jobs.emplace_back();
  job.id=static_cast<int>(q.jobs.size());
  return job;
}

/* a job another has no use for: not run, dropped as a repeat, or stopped by the queue */
bool unmade(const QueuedJob &job) { return job.skipped||job.left_out||job.run->dropped; }

/* the recording's entry for a job, none when it holds none (or there is no recording) */
const xpp::job::BranchJob *planned(const Queue &q, const AutoxJobKey &key)
{
  const auto at=q.plan_of.find(key.path);
  return at==q.plan_of.end()?nullptr:at->second;
}

/* a job whose fate is final: it ended (or never started) and what it found is queued, so a job after it in the join's order
   does not wait on it. The mutex is held. */
bool settled(const QueuedJob &job) { return job.never||(job.done&&job.expanded); }

/* whether one of the jobs the join keeps (in their order) already made a job's start: its twin's loop closed, or a loop closed
   at the point the job starts from in the other direction, or the start or an end of one made it (autox_queue.h) */
bool made_by(const std::vector<const QueuedJob *> &kept, const AutoxJobStart &start, const QueuedJob *twin, double epss)
{
  for(const QueuedJob *other:kept){
    if(other->closed&&(other==twin||autox_start_reversed(other->start,start,epss)))return true;
    if(autox_start_made(other->start,start,epss))return true;
    for(const AutoxJobStart &claim:other->made)
      if(autox_start_made(claim,start,epss))return true;
  }
  return false;
}

/* whether a job with this key and start is already made by a job the join keeps before it (autox_queue.h). The join's rule is
   a function of the jobs before a job, so a job before the first one not yet settled counts and a later one does not: what it
   would make is not known, and a job another has dropped makes nothing. The answer is the same whatever the order the jobs
   finish in (a job that says no may still be dropped at the join, once the jobs before it are settled; one that says yes is
   dropped by it). The mutex is held when the jobs are running. */
bool repeats(std::deque<QueuedJob> &jobs, const AutoxJobKey &key, const AutoxJobStart &start, const QueuedJob *twin, double epss)
{
  std::vector<QueuedJob *> before;
  for(QueuedJob &other:jobs)
    if(autox_job_before(other.key,key))before.push_back(&other);
  std::sort(before.begin(),before.end(),[](const QueuedJob *a,const QueuedJob *b){return autox_job_before(a->key,b->key);});
  std::vector<const QueuedJob *> kept;
  for(QueuedJob *other:before){
    if(!settled(*other))break;
    if(!other->kept)other->kept=!unmade(*other)&&(!other->parent||other->parent->kept.value_or(false))&&!made_by(kept,other->start,other->twin,epss);
    if(*other->kept)kept.push_back(other);
  }
  return made_by(kept,start,twin,epss);
}

/* a direction that leaves the parameter window at the start is not run (it says so, once, in the job's own output) */
void skip_if_away(Queue &q, QueuedJob &job, double start)
{
  const AutoxState &state=*q.s.autox_state;
  const double ds=job.run->params.ds;
  if(!autox_start_points_away(start,state.bifur.rl0,state.bifur.rl1,ds))return;
  job.skipped=true;
  const int principal=state.axes().icp1;
  const LogHold hold(job.table,job.warnings);
  autox_stop_note(*job.run,xpp::format("step_size = {} is not followed: the start {} = {} is at or outside parameter_min = {} and parameter_max = {}, where that direction leaves the window",
                                       xpp::number(ds),q.s.model().upar_names[principal],xpp::number(start),xpp::number(state.bifur.rl0),xpp::number(state.bifur.rl1)));
}

/* XPP_AUTOX_FAIL_CHILD_AT=N makes every job that starts at what another found fail when it holds N points, so servercheck
   reaches a failing job without a model that fails (the way XPP_STORAGE_FAIL_GROW reaches a full store) */
std::optional<std::size_t> child_failure_point()
{
  const char *at=std::getenv("XPP_AUTOX_FAIL_CHILD_AT");
  if(!at)return std::nullopt;
  return static_cast<std::size_t>(std::strtoull(at,nullptr,10));
}

/* the job's direction, evaluation and threads, once it is about to run */
void configure(Queue &q, QueuedJob &job)
{
  AutoxRun &run=*job.run;
  run.job=true;
  run.on_thread=q.at_once;
  if(job.parent)run.fail_after_points=child_failure_point();
  run.params.mxbf=0; /* the queue follows the branch points: autoX's own switching at them is off */
  run.params.ds=job.start.sign.value_or(1)*q.ds;
  if(const xpp::job::BranchJob *entry=planned(q,job.key);entry&&entry->state==xpp::job::JobState::stopped)
    run.stop_after_points=static_cast<std::size_t>(entry->point); /* the recording stopped it there */
  if(q.at_once)run.collocation_threads=std::min<unsigned>(AutoxParallel::max_threads,std::max<unsigned>(1,q.budget/static_cast<unsigned>(q.running+1)));
  if(q.isolated){
    run.pure_rhs=true;
    run.rhs_constants=q.constants;
    run.rhs_variables=q.variables;
  }
}

/* a job that starts at what another found: its folder, and its fort.3 the point's solution out of the finding job's fort.8
   (an equilibrium through an end of cycles starts from its state, as a run from the initial conditions does) */
Result<> set_up_child(Queue &q, QueuedJob &job)
{
  Session &s=q.s;
  AutoxRun &run=*job.run;
  const AutoxJobStart &start=job.start;
  AutoxFoundStart found{start.ips,start.isw,s.autox_state->suppress_bp?0:start.ips==AutoxProblem::Periodic?2:1,start.label,nullptr,{}};
  if(start.label==0){
    if(auto ready=autox_set_up_run(s,run,NO_OPEN_3,&found);!ready)return ready;
    run.orbit_start=false;
    run.refined_start=start.equilibrium;
    return {};
  }
  const AutoxRun &finder=*job.parent->run;
  auto solution=autox_solution_block(finder.fort8,start.label,finder.parameter_count);
  const auto point=std::find_if(finder.points.begin(),finder.points.end(),[&](const AutoxBufferedPoint &held){return held.point.lab==start.label;});
  if(!solution||point==finder.points.end())
    return fail("autox",xpp::format("the solution of label {} is not in the job that found it",start.label),Place{finder.fort8,0,0,{}});
  found.point=&point->point;
  found.solution=std::move(*solution);
  return autox_set_up_run(s,run,NO_OPEN_3,&found);
}

/* a job ended (the mutex held): what it made, and whether it closed on itself */
void finish(Queue &q, QueuedJob &job)
{
  job.made=autox_end_claims(job.start.ips,job.run->points);
  job.closed=!job.run->points.empty()&&job.run->points.back().point.itp.kind==AutoxPointKind::BranchClosed;
  job.done=true;
  q.changed.notify_all();
}

/* waits, the mutex held through `lock`, until done() holds; false when the run was stopped meanwhile */
template<class Done> bool wait_until(Queue &q, std::unique_lock<std::mutex> &lock, Done done)
{
  while(!done()){
    if(xpp::job::cancelled())return false;
    q.changed.wait_for(lock,std::chrono::duration<double>(INPUT_POLL_SECONDS));
  }
  return true;
}

/* The pair rule (#662 corner case 1), on the job's thread when it has taken its first step: two neighbouring Hopf points of
   one branch whose cycles grow towards each other are two ends of one family, so the later waits until the earlier has
   ended, and goes on only if the earlier has not made it (a cycle branch that ended at its Hopf point has). The family is
   the same whatever the order they finish in; the join drops the repeat in any case, this only saves the work. */
void hold_for_neighbour(Queue &q, QueuedJob &job)
{
  const double from=job.start.id->parameters[0];
  const double step=job.run->points[1].point.par[0]-from;
  std::unique_lock lock(q.mutex);
  if(step!=0)job.first_step=step>0?1:-1;
  q.changed.notify_all();
  QueuedJob &before=*job.hopf_before;
  if(!job.first_step||!wait_until(q,lock,[&]{return before.first_step||before.done;})||!before.first_step)return;
  const bool to_the_right=from>before.start.id->parameters[0]; /* this Hopf point is above the other's */
  if(*before.first_step!=(to_the_right?1:-1)||*job.first_step!=-*before.first_step)return; /* not growing towards each other */
  if(!wait_until(q,lock,[&]{return before.done;}))return;
  if(repeats(q.jobs,job.key,job.start,job.twin,q.epss))job.run->dropped=true;
}

/* the job's numerics on a thread of its own (or here), then what it made; q.mutex is not held */
void run_job(Queue &q, QueuedJob &job)
{
  {
    const LogHold hold(job.table,job.warnings);
    job.failed=run_numerics(*job.run);
    job.run->fp8.reset();
  }
  const std::lock_guard lock(q.mutex);
  if(job.failed&&job.run->run_wide_error)q.fatal=true;
  finish(q,job);
}

/* the autox.jobs event, when how many jobs run, wait or have ended changed, or (no more often than the progress events)
   how many points a running job has */
void announce_counts(Queue &q)
{
  XppAutoxJobs counts{0,0,0,{}};
  std::vector<const QueuedJob *> running;
  {
    const std::lock_guard lock(q.mutex);
    for(const QueuedJob &job:q.jobs){
      if(job.skipped||job.never)continue;
      if(job.done)counts.done++;
      else if(job.started){
        counts.running++;
        running.push_back(&job);
      }
      else counts.queued++;
    }
    std::sort(running.begin(),running.end(),[](const QueuedJob *a,const QueuedJob *b){return autox_job_before(a->key,b->key);});
    for(const QueuedJob *job:running)counts.each.push_back({job->id,job->key.path,static_cast<int>(job->drawn)});
  }
  const bool same=counts.running==q.shown_counts.running&&counts.queued==q.shown_counts.queued&&counts.done==q.shown_counts.done;
  if(same&&(counts.each==q.shown_counts.each||!xpp::every(q.counts_last,xpp::PROGRESS_SECONDS)))return;
  q.shown_counts=counts;
  autox_jobs(counts);
}

/* Draws the points the jobs stored since it last did, each as its job's (autox_show_live_point), and the stability circle of the
   earliest job running (the join's order); at most as often as the progress events. Only the main thread draws: the jobs'
   threads record (q.mutex) and wake it. */
void show_live(Queue &q)
{
  if(!xpp::every(q.live_last,xpp::PROGRESS_SECONDS))return;
  std::vector<std::pair<QueuedJob *,std::vector<AutoxBufferedPoint>>> fresh;
  QueuedJob *earliest=nullptr;
  {
    const std::lock_guard lock(q.mutex);
    for(QueuedJob &job:q.jobs)
      if(!job.unseen.empty()){
        fresh.emplace_back(&job,std::move(job.unseen));
        job.unseen.clear();
      }
  }
  for(auto &[job,points]:fresh){
    for(const AutoxBufferedPoint &held:points)autox_show_live_point(q.s,held.point,job->id,held.new_segment);
    job->drawn+=points.size();
    job->latest=std::move(points.back().point);
  }
  {
    const std::lock_guard lock(q.mutex);
    for(QueuedJob &job:q.jobs)
      if(job.started&&!job.done&&job.latest&&(!earliest||autox_job_before(job.key,earliest->key)))earliest=&job;
  }
  if(earliest){
    const DIAGRAM &point=*earliest->latest;
    autox_data_stab(q.s,point.evr,point.evi,point_is_periodic(get_bif_type(point.ibr,point.ntot)));
  }
  refreshdisplay(q.s);
}

/* sets a queued job up and starts it, on a thread or here (the jobs in turn); not started when an earlier job made it, or when
   it cannot be set up (the join shows that) */
void begin(Queue &q, QueuedJob &job, std::vector<std::thread> &threads)
{
  {
    const std::lock_guard lock(q.mutex);
    if((!q.plan.empty()&&!planned(q,job.key))||repeats(q.jobs,job.key,job.start,job.twin,q.epss)){
      job.skipped=job.done=job.expanded=true;
      q.changed.notify_all();
      return;
    }
  }
  if(!q.at_once)q.restore_constants();
  if(job.parent)
    if(auto ready=set_up_child(q,job);!ready){
      const std::lock_guard lock(q.mutex);
      job.failed=ready.error();
      job.done=true;
      q.changed.notify_all();
      return;
    }
  configure(q,job);
  AutoxRun &run=*job.run;
  if(job.parent&&job.start.label==0){ /* an equilibrium: the parameter value it starts at */
    const int slot=q.s.model().upar_con[q.s.autox_state->axes().icp1];
    (run.pure_rhs?run.rhs_constants:q.s.parser.constants)[static_cast<std::size_t>(slot)]=job.start.parameter;
  }
  run.point_stored=[&q,&job](std::size_t points){
    {
      const std::lock_guard lock(q.mutex);
      job.unseen.push_back(job.run->points.back());
    }
    if(autox_diagram_full(q.diagram_base+q.points_held.fetch_add(1))){ /* more points than the diagram holds: the run is over */
      q.fatal=true;
      if(!q.at_once)xpp::job::cancel_current();
    }
    if(q.at_once)q.changed.notify_all(); /* the main thread draws it */
    else{
      show_live(q);
      announce_counts(q);
    }
    if(q.at_once&&job.hopf_before&&points==2)hold_for_neighbour(q,job);
  };
  job.started=true;
  announce_counts(q); /* before a job in turn computes: the main thread is busy until it ends */
  if(!q.at_once){
    run_job(q,job);
    return;
  }
  {
    const std::lock_guard lock(q.mutex);
    q.running++;
  }
  try {
    threads.emplace_back([&q,&job]{
      run_job(q,job);
      const std::lock_guard lock(q.mutex);
      q.running--;
      q.changed.notify_all();
    });
  } catch (const std::system_error &e) {
    const std::lock_guard lock(q.mutex);
    job.failed=Error{"autoX",xpp::format("cannot start a thread for a job: {}",e.what()),command_place()};
    finish(q,job);
    q.running--;
  }
}

/* the jobs a job that ended starts (autox_found_starts), those not already made by the jobs before them */
void make_found_jobs(Queue &q, QueuedJob &job)
{
  /* a run over (stopped, or fatal: a point at the diagram's limit sets it before its job can end) makes no more jobs, so the
     jobs made never exceed autox_recorded_jobs_limit, whatever the threads stored after it */
  if(xpp::job::cancelled()||q.fatal||job.run->dropped)return;
  if(const xpp::job::BranchJob *entry=planned(q,job.key);entry&&entry->state!=xpp::job::JobState::finished)return; /* a stopped job's points started nothing */
  QueuedJob *last_hopf=nullptr;
  for(AutoxJobStart &start:autox_found_starts(job.start.ips,job.run->points,job.run->stop)){
    const bool negative=start.sign&&*start.sign<0;
    const AutoxJobKey key=autox_child_key(job.key,start.point,negative);
    const xpp::job::BranchJob *entry=q.plan.empty()?nullptr:planned(q,key);
    if(!q.plan.empty()&&!entry)continue; /* the recorded run never made it */
    const std::lock_guard lock(q.mutex);
    if(repeats(q.jobs,key,start,nullptr,q.epss))continue;
    QueuedJob &child=make_job(q);
    child.key=key;
    child.parent=&job;
    const double parameter=start.id->parameters[0];
    child.start=std::move(start);
    child.run->params.ds=child.start.sign.value_or(1)*q.ds;
    if(child.start.ips==AutoxProblem::Periodic&&child.start.isw==1){ /* a Hopf point's cycles */
      child.hopf_before=last_hopf;
      last_hopf=&child;
    }
    skip_if_away(q,child,parameter);
    if(child.skipped||(entry&&entry->state==xpp::job::JobState::never_started)){
      child.never=!child.skipped;
      child.done=child.expanded=true;
    }
  }
}

/* a job that ended: its jobs are made, and the jobs after it in the join's order no longer wait on it (settled) */
void expand(Queue &q, QueuedJob &job)
{
  make_found_jobs(q,job);
  const std::lock_guard lock(q.mutex);
  job.expanded=true;
}

/* the queued job with the least key, none once the run was stopped */
QueuedJob *next_queued(Queue &q)
{
  if(xpp::job::cancelled())return nullptr;
  QueuedJob *next=nullptr;
  for(QueuedJob &job:q.jobs)
    if(!job.started&&!job.done&&(!next||autox_job_before(job.key,next->key)))next=&job;
  return next;
}

/* what a job ended early with: its failure, or the exact Jacobian's (autox_pp.cpp); not an error of the whole run, which the join
   shows once (the mutex held, the job done) */
std::optional<Error> early_end(const QueuedJob &job)
{
  if(job.run->run_wide_error)return std::nullopt;
  if(job.failed)return job.failed;
  return job.run->jacobian_failure;
}

/* which job, for a message made while the jobs run, before the join numbers them: what it continues and from where */
std::string job_text(const Queue &q, const QueuedJob &job)
{
  const AutoxJobStart &start=job.start;
  const std::string from=job.parent?xpp::format("the point at {} = {}",q.s.model().upar_names[q.s.autox_state->axes().icp1],xpp::number(start.id->parameters[0])):"the start";
  const std::string direction=start.sign?(*start.sign>0?", +step_size":", -step_size"):"";
  return xpp::format("the job of {} from {}{}",start.ips==AutoxProblem::Periodic?"cycles":"steady states",from,direction);
}

/* a job ended early, without the others: shown at once, in autoX's output and as a message; the run goes on (the job's points are kept) */
void show_early_end(const Queue &q, const QueuedJob &job, Error error)
{
  error.what=xpp::format("{} ended early after {} points: {}",job_text(q,job),job.run->points.size(),error.what);
  if(error.place.file.empty()&&error.place.line==0)error.place=command_place();
  xpp::log_autox("{}\n",error.text());
  show_error(error);
}

/* how a job ended, for a stopped run to be replayed from (xpp_job.h): stopped when the user's Stop (or the replay's) ended its
   branch, never started when it did not start or stopped before storing a point (which leaves nothing in the diagram either) */
xpp::job::BranchJob ended_as(const QueuedJob &job)
{
  xpp::job::BranchJob end{job.key.path,xpp::job::JobState::finished,0};
  const AutoxRun &run=*job.run;
  if(job.never||(job.started&&run.points.empty()&&!job.failed))end.state=xpp::job::JobState::never_started;
  else if(job.started&&run.stop.last_why==AUTOX_STOP_USER){
    end.state=xpp::job::JobState::stopped;
    end.point=static_cast<long>(run.points.size());
  }
  return end;
}

/* a recording's stop replayed: every job ended as recorded, so the run ends stopped as it did; else the stop stays armed, which the
   player says (it never got to its recorded interruption) */
void finish_replay(const Queue &q)
{
  if(q.plan.empty())return;
  bool stopped=false;
  for(const xpp::job::BranchJob &entry:q.plan)stopped|=entry.state!=xpp::job::JobState::finished;
  std::size_t matched=0;
  bool every=true;
  for(const QueuedJob &job:q.jobs){
    const xpp::job::BranchJob *entry=planned(q,job.key);
    if(!entry)continue;
    matched++;
    every=every&&job.ended.state==entry->state&&job.ended.point==entry->point;
  }
  if(!every||matched!=q.plan_of.size())return;
  xpp::job::jobs_reached();
  if(stopped)xpp::job::cancel_current();
}

/* Runs the jobs and what they find until none is left: each when a thread is free (at once: the main thread waits and polls the
   front end for Abort), or one after another on the main thread. Jobs start in their order. */
void run_queue(Queue &q)
{
  std::vector<std::thread> threads;
  for(;;){
    if(q.fatal&&!xpp::job::cancelled())xpp::job::cancel_current(); /* an error of the whole run stops every job, as Stop does */
    for(std::size_t i=0;i<q.jobs.size();i++){
      QueuedJob &job=q.jobs[i];
      bool ended;
      std::optional<Error> early;
      {
        const std::lock_guard lock(q.mutex);
        ended=job.done&&!job.expanded;
        if(job.done&&!job.announced){
          job.announced=true;
          early=early_end(job);
        }
      }
      if(early)show_early_end(q,job,std::move(*early));
      if(ended)expand(q,job);
    }
    show_live(q);
    announce_counts(q);
    while(QueuedJob *next=next_queued(q)){
      if(q.at_once?q.running>=q.budget:q.running>0)break;
      begin(q,*next,threads);
      if(!q.at_once)break; /* the job ended; its jobs are queued before the next starts */
    }
    std::unique_lock lock(q.mutex);
    if(q.running==0&&!next_queued(q)&&std::all_of(q.jobs.begin(),q.jobs.end(),[](const QueuedJob &j){return !j.done||j.expanded;}))break;
    if(!q.at_once)continue;
    q.changed.wait_for(lock,std::chrono::duration<double>(INPUT_POLL_SECONDS));
    lock.unlock();
    int stop;
    byeautox_(&stop); /* the front end's Abort, which cancels every job */
  }
  for(std::thread &thread:threads)thread.join();
  show_live(q); /* what the threads stored last */
  for(QueuedJob &job:q.jobs)
    if(!job.started&&!job.done&&!job.skipped)job.never=true; /* what a stop left queued */
  announce_counts(q);
}

Result<> autox_run_jobs(Session &s, int iold, int isave)
{
  if(auto prepared=autox_run_prologue(s,isave);!prepared)return prepared;
  const AutoxDepth depth(s);
  create_autox_file_name(s);
  /* the jobs evaluate on copies of the model's constants when the model allows it; else on the Session's own, one at a
     time, each from the constants the run began with, which are the Session's again when the run ends */
  Queue q(s);
  const AutoxState &state=*s.autox_state;
  q.isolated=native_ready(s)&&autox_parallel_ready(s); /* the jobs on threads call the native code: built first (W715) */
  q.constants=s.parser.constants;
  q.variables=s.parser.variables;
  q.epss=state.bifur.epss;
  q.ds=std::abs(state.bifur.ds);
  q.budget=s.thread_budget();
  q.at_once=q.isolated&&q.budget>1;
  q.diagram_base=s.diagram->points.size();
  if(const std::vector<xpp::job::BranchJob> *armed=xpp::job::armed_jobs())q.plan=*armed; /* a recording's stopped run: each job ends as it did */
  for(const xpp::job::BranchJob &entry:q.plan)q.plan_of.emplace(entry.key,&entry);
  for(const int sign:{1,-1}){ /* the roots: both directions of the start */
    QueuedJob &root=make_job(q);
    root.key.path={sign>0?0L:1L};
    root.start.ips=state.bifur.ips;
    root.start.isw=state.bifur.isw;
    root.start.sign=sign;
  }
  q.jobs[1].twin=&q.jobs[0];
  std::optional<std::vector<double>> refined;
  {
    const xpp::Job job; /* Abort cancels it (xpp_job.h) */
    const Computation computing; /* what Abort stops (xpp_job.h) */
    auto started=refined_equilibrium_start(s);
    q.restore_constants();
    if(!started)return std::unexpected(started.error());
    refined=std::move(*started);
  }
  for(QueuedJob &root:q.jobs){
    if(auto ready=autox_set_up_run(s,*root.run,iold);!ready)return ready;
    root.run->params.ds=root.start.sign.value()*q.ds;
    root.run->refined_start=refined;
  }

  /* a direction that leaves the parameter window at the start is not run (it says so); when both do, only the first is
     run, which is refused as a single run is, once */
  const int principal=state.axes().icp1;
  const double start=q.constants[s.model().upar_con[principal]];
  if(autox_start_points_away(start,state.bifur.rl0,state.bifur.rl1,q.ds)&&autox_start_points_away(start,state.bifur.rl0,state.bifur.rl1,-q.ds))
    q.jobs[1].skipped=true;
  else for(QueuedJob &root:q.jobs)skip_if_away(q,root,start);
  for(QueuedJob &root:q.jobs){
    const xpp::job::BranchJob *entry=q.plan.empty()?nullptr:planned(q,root.key);
    root.never=!root.skipped&&entry&&entry->state==xpp::job::JobState::never_started;
    root.done=root.expanded=root.skipped||root.never;
  }
  bool stopped=false; /* the run was stopped (Stop, or an error of the whole run) */
  const AutoxCalculation calculation=s.autox_state->type_of_calc; /* the roots' kind of calculation, which each job stores its points under */
  {
    const xpp::Job job;
    const Computation computing;
    run_queue(q);
    for(QueuedJob &queued:q.jobs){
      queued.ended=ended_as(queued);
      queued.ended_early=early_end(queued).has_value();
    }
    finish_replay(q);
    stopped=xpp::job::cancelled();
  }
  autox_live_drop(s); /* the join stores the diagram's own points in place of the live ones */
  /* the join's order is the jobs' keys (a queue served in turn would run them so) */
  std::vector<QueuedJob *> order;
  for(QueuedJob &job:q.jobs)order.push_back(&job);
  std::sort(order.begin(),order.end(),[](const QueuedJob *a,const QueuedJob *b){return autox_job_before(a->key,b->key);});
  /* the Session's constants end as an ordinary run leaves them, at the last evaluated point of the last job that ran (a
     job that evaluated on the Session's own left them there) */
  const auto last=std::find_if(order.rbegin(),order.rend(),[](const QueuedJob *j){return j->started;});
  if(last==order.rend())q.restore_constants();
  else{
    if(q.isolated){
      s.parser.constants=(*last)->run->rhs_constants;
      s.parser.variables=(*last)->run->rhs_variables;
    }
    std::vector<double> before(static_cast<std::size_t>(s.model().nupar)); /* the start's parameters, as do_autox keeps them */
    for(int k=0;k<s.model().nupar;k++)before[static_cast<std::size_t>(k)]=q.constants[s.model().upar_con[k]];
    autox_restore_finite_pars(s,before.data()); /* leave no NaN parameter behind (QA SCI-001) */
  }

  /* the join: each job in its order, numbered as a run after the ones before it; a repeat of an earlier job's is dropped */
  std::optional<Error> reported;
  Renumber moved;
  const auto first=std::find_if(order.begin(),order.end(),[](const QueuedJob *j){return j->started;});
  if(first!=order.end()){
    moved.base_branch=(*first)->run->base_branch;
    moved.base_label=(*first)->run->base_label;
  }
  const std::string limit_text=autox_diagram_limit_message();
  bool diagram_full=false,wide_shown=false;
  int job_number=0; /* the jobs that keep their points, in the join's order: what the diagram's points name */
  std::vector<XppJoinedJob> joined; /* the same jobs with the branch their points carry: the autox.joined event */
  std::optional<std::pair<int,int>> last_stored; /* the last point of the diagram, which a stopped run reports */
  for(QueuedJob *queued:order){
    QueuedJob &job=*queued;
    AutoxRun &run=*job.run;
    if(job.parent&&job.parent->left_out){
      job.left_out=true;
      continue;
    }
    if(!job.started){
      if(!job.warnings.empty())xpp::log(XPP_LOG_WARN,"{}",job.warnings);
      autox_stop_merge(s.autox_state->stop,std::move(run.stop));
      continue;
    }
    if(run.dropped||repeats(q.jobs,job.key,job.start,job.twin,q.epss)){
      job.left_out=true;
      continue;
    }
    if(job.failed&&run.run_wide_error){ /* an error of the whole run (the diagram's limit, memory) is one error, shown once */
      if(wide_shown)job.failed.reset();
      wide_shown=true;
    }
    const int saving=s.autox_state->diag_flag&&isave==OVERWRITE?APPEND:isave;
    const Renumber mine=moved;
    job.numbering=mine;
    const Renumber from=job.parent?job.parent->numbering:mine; /* its first point's `from` label is the finding job's */
    int branches=moved.base_branch,labels=moved.base_label;
    StoredPoints stored;
    bool cut=diagram_full; /* some of this job's points were not stored */
    s.autox_state->type_of_calc=autox_job_calculation(job.start.ips);
    job_number++;
    for(AutoxBufferedPoint &held:run.points){
      DIAGRAM &p=held.point;
      p.ibr=mine.branch(p.ibr);
      p.lab=mine.label(p.lab);
      p.from=from.label(p.from);
      p.job=job_number;
      branches=std::max(branches,std::abs(p.ibr));
      labels=std::max(labels,p.lab);
      if(!diagram_full&&autox_diagram_full(s.diagram->points.size())){ /* the limit is the whole run's: every job's points and the diagram's */
        diagram_full=cut=true;
        if(!wide_shown&&!job.failed){
          job.failed=Error{"autoX",limit_text,command_place()};
          run.run_wide_error=true;
        }
        wide_shown=true;
      }
      if(diagram_full)continue;
      stored.insert({std::abs(p.ibr),std::abs(p.ntot)});
      autox_store_point(s,p,held.new_segment);
      last_stored=std::pair<int,int>(std::abs(p.ibr),std::abs(p.ntot));
    }
    for(const auto &[file,kind]:{std::pair{run.fort7,RunFile::branches},std::pair{run.fort8,RunFile::solutions},std::pair{run.fort9,RunFile::diagnostics}})
      if(auto numbered=renumber_file(run,file,kind,mine,kind==RunFile::diagnostics||!cut?nullptr:&stored);!numbered&&!reported)reported=numbered.error();
    if(!stored.empty())joined.push_back({job.key.path,stored.begin()->first}); /* one run, one branch: every point stored has it */
    /* the next job numbers after this one's */
    moved.branch_offset=branches-moved.base_branch;
    moved.label_offset=labels-moved.base_label;
    if(!job.table.empty())xpp::log_autox("{}",renumbered_table(job.table,mine));
    if(!job.warnings.empty())xpp::log(XPP_LOG_WARN,"{}",renumbered_table(job.warnings,mine));
    if(job.failed)job.failed->what=autox_renumbered_line(job.failed->what,[&](int value){return mine.branch(value);},[&](int value){return mine.label(value);}); /* "Branch N point M: nonfinite" */
    run.stop.last_br=mine.branch(static_cast<int>(run.stop.last_br)); /* why this job's branches ended, as the diagram numbers them */
    for(std::string &note:run.stop.notes)
      note=autox_renumbered_line(note,[&](int value){return mine.branch(value);},[&](int value){return mine.label(value);});
    /* a job that ended early alone was shown when it did; the run's own error is the command's */
    std::optional<Error> wide_error;
    if(run.run_wide_error)wide_error=std::move(job.failed);
    if(auto ended=autox_end_run(s,run,std::move(wide_error),saving);ended&&!reported&&run.run_wide_error)reported=std::move(ended);
  }
  autox_joined(joined);
  std::vector<xpp::job::BranchJob> ends; /* how the jobs that count ended, and the counts a stop or an early end reports */
  int finished=0,stopped_jobs=0,never_started=0,early=0;
  for(const QueuedJob *queued:order){
    if(unmade(*queued))continue;
    ends.push_back(queued->ended);
    if(queued->ended_early)early++;
    else switch(queued->ended.state){
      case xpp::job::JobState::finished: finished++; break;
      case xpp::job::JobState::stopped: stopped_jobs++; break;
      case xpp::job::JobState::never_started: never_started++; break;
    }
  }
  std::vector<std::string> notes; /* the jobs' counts, in autoX's output and the info strip's notes */
  if(stopped)notes.push_back(xpp::format("autoX stopped: {} of {} jobs finished, {} stopped, {} never started",finished,ends.size(),stopped_jobs,never_started));
  if(early)notes.push_back(xpp::format("autoX: {} of {} jobs ended early (the messages above say why)",early,ends.size()));
  for(std::string &note:notes){
    xpp::log_autox("{}\n",note);
    s.autox_state->stop.notes.push_back(std::move(note));
  }
  s.autox_state->type_of_calc=calculation;
  if(last_stored)xpp::job::report_point(last_stored->first,last_stored->second);
  xpp::job::report_jobs(std::move(ends));
  ping();
  redraw_params();
  if(reported)return std::unexpected(std::move(*reported));
  return {};
}

} // namespace

Result<> autox_run_from_current_state(Session &s, int iold, int isave)
{
  return s.autox_state->follow_all_branches?autox_run_jobs(s,iold,isave):do_autox(s,iold,isave,s.autox_state->bifur.itp);
}

} // namespace xpp
