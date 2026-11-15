/* The session list and a load's swap of its client's Model and Session
   (session.h). */
#include "expr_tables.h"
#include "session.h"
#include "model_request.h"
#include "model.h"
#include "xpp_mem.h"
#include "xpp_ui.h"
#include "adj2.h"
#include "ani_state.h"
#include "autox_state.h"
#include "browse.h"
#include "dae_fun.h"
#include "delay_handle.h"
#include "numerics.h"
#include "diagram.h"
#include "display_state.h"
#include "do_fit.h"
#include "equilibria.h"
#include "plot_settings.h"
#include "plot_state.h"
#include "histogram.h"
#include "integrate.h"
#include "jacobian.h"
#include "plot_state.h"
#include "stochastic.h"
#include "picture_state.h"
#include "pp_shoot.h"
#include "simplenet.h"
#include "storage.h"
#include "value_undo.h"
#include "volterra.h"
#include "volterra2.h"
#include "my_rhs.h"
#include "autox_codes.h"
#include "tabular.h"
#include "userbut.h"

#include <memory>
#include <new>
#include <utility>

namespace xpp {

namespace {

/* the run's functions the compiled formulas call (ExprHost, expr.h): the delays, integral kernels, networks and the
   right-hand side's purity, which the expressions never include */
const ExprHost run_host{
  .delay = do_delay,
  .delay_shift = do_delay_shift,
  .kernel_value = ker_val,
  .network_value = network_value,
  .vector_value = vector_value,
  .pure_rhs_ready = pure_rhs_ready,
};

}

/* The parts are made default-initialised, as when they were members, except
   the plot windows and frozen curves, which were value-initialised. */
Session::Session(Model &m)
  :     data_store(std::make_unique_for_overwrite<DataStore>()),
    plot_windows(std::make_unique<XppPlotWindows>()),
    tools_hidden(std::make_unique_for_overwrite<ToolWindowsHidden>()),
    frozen_curves(std::make_unique<XppFrozenCurves>()),
    movie_3d(std::make_unique_for_overwrite<Mov3d>()),
    phase_shown(std::make_unique_for_overwrite<PhaseShown>()),
    marks_shown(std::make_unique_for_overwrite<MarksShown>()),
    autox_view(std::make_unique_for_overwrite<AutoxView>()),
    integrator(std::make_unique_for_overwrite<IntegratorState>()),
    solver_work(std::make_unique_for_overwrite<SolverWork>()),
    jacobian_scratch(std::make_unique_for_overwrite<JacobianScratch>()),
    autox_state(std::make_unique_for_overwrite<AutoxState>()),
    diagram(std::make_unique_for_overwrite<AutoxDiagram>()),
    data(std::make_unique_for_overwrite<DataState>()),
    kinescope(std::make_unique_for_overwrite<XppKinescope>()),
    numerics(std::make_unique_for_overwrite<NumericsSettings>()),
    plot_settings(std::make_unique_for_overwrite<PlotSettings>()),
    delay(std::make_unique_for_overwrite<DelayState>()),
    histogram(std::make_unique_for_overwrite<HistogramState>()),
    adjoint(std::make_unique_for_overwrite<AdjointState>()),
    stochastic(std::make_unique_for_overwrite<StochasticState>()),
    nullclines(std::make_unique_for_overwrite<NullclineSettings>()),
    volterra(std::make_unique_for_overwrite<VolterraState>()),
    fit(std::make_unique_for_overwrite<FitInfo>()),
    dae(std::make_unique_for_overwrite<DaeState>()),
    nullcline_state(std::make_unique_for_overwrite<NullclineState>()),
    shoot_range(std::make_unique_for_overwrite<ShootRange>()),
    marker(std::make_unique_for_overwrite<MarkInfo>()),
    manifolds(std::make_unique_for_overwrite<ManifoldShots>()),
    drawing(std::make_unique_for_overwrite<DrawingState>()),
    plot_file(std::make_unique_for_overwrite<PlotFileState>()),
    array_plot(std::make_unique_for_overwrite<ArrayPlotState>()),
    animation(std::make_unique_for_overwrite<AnimationState>()),
    ani_shown(std::make_unique_for_overwrite<AniShown>()),
    saved_session(std::make_unique_for_overwrite<SavedSession>()),
    value_undo(std::make_unique_for_overwrite<ValueUndo>()),
    option_parts{*this,m,numerics.get(),plot_settings.get(),autox_state.get(),integrator.get(),delay.get(),histogram.get(),adjoint.get(),
                 nullclines.get(),shoot_range.get(),animation.get(),drawing.get(),plot_windows.get(),array_plot.get(),data->added_columns,
                 sliders,user_buttons,tables,run_sets,itor,batch_out_file,picture_file,after_run_file,colormap,output_place,options_set,random},
    model_(&m)
{
  parser.host=&run_host;
  parser.compiler=&m.compiler;
  parser.tables=&tables;
  parser.random=&random;
  parser.owner=this;
  selected_window=plot_windows->graph[0].name;
  plot_display.resize(1);
}

Session::~Session()=default;

void Session::size_variables(int n)
{
  const std::size_t k=static_cast<std::size_t>(n);
  model_->size_variables(n);
  if(last_ic.size()>=k)return;
  last_ic.resize(k);
  delay_string.resize(k,"0.0");
  itor.resize(k);
  bcs.resize(k);
}

namespace {

/* the session list's one client: its Model and the Session that runs it
   (session.h client_session), the Session declared last so that it goes
   before the Model it points to */
struct Client {
  std::unique_ptr<Model> model;
  std::unique_ptr<Session> session;
  /* the file system the last Load was given: a Model made while no load stands (the start screen's, after a
     load that found no model) opens its first file through it */
  FileSystem *fs=nullptr;
};

/* The session list, kept for the program's life: never destroyed, as a
   union member is not. exit() comes from inside a command (Quit) whose
   Session is still in use, and what the process holds then is the
   system's to reclaim (AGENTS.md "Memory"). */
union SessionList {
  Client entry; /* its one entry */
  constexpr SessionList() : entry{} {}
  ~SessionList() {}
  SessionList(const SessionList &)=delete;
  SessionList &operator=(const SessionList &)=delete;
};
constinit SessionList sessions;

}

Session &client_session()
{
  Client &c=sessions.entry;
  if(!c.session)[[unlikely]]{
    c.model=std::make_unique<Model>();
    c.model->fs=c.fs;
    c.session=std::make_unique<Session>(*c.model);
  }
  return *c.session;
}

Load::Load(FileSystem &fs)
{
  /* made before anything changes hands: an allocation that fails leaves
     the client as it was */
  std::unique_ptr<Model> model=std::make_unique<Model>();
  model->fs=&fs;
  std::unique_ptr<Session> fresh=std::make_unique<Session>(*model);
  Client &c=sessions.entry;
  c.fs=&fs;
  /* a Model no load made (the start screen's, made before the first load) takes the file system of the load
     that finds no model to open, so that File > Open model's load has one to pass on */
  if(c.model&&!c.model->fs)c.model->fs=&fs;
  /* the process's autoX scratch folder (xppautx_main.cpp makes it before
     the first load) stays the same across loads */
  if(c.session)fresh->autox_state->dir=c.session->autox_state->dir;
  session_=fresh.get();
  previous_session=std::exchange(c.session,std::move(fresh));
  previous_model=std::exchange(c.model,std::move(model));
}

Load::~Load()
{
  write_kept(); /* what a failed load's error did not take */
  leave();
  if(committed)return;
  /* the failed Session, then its Model, give way to the ones before */
  Client &c=sessions.entry;
  c.session=std::move(previous_session);
  c.model=std::move(previous_model);
}

void Load::locate(Error &e)
{
  Place &p=e.place;
  if(p.line<=0||!p.source.empty()||p.file.empty())return;
  std::string text;
  if(read_model_file(model(),p.file,text))p.source=std::string(nth_line(text,p.line));
}

void Load::arrive(std::string_view file, int line, int col)
{
  write_kept();
  where=Place{std::string(file),line,col};
}

std::string Load::kept() const
{
  std::string out;
  std::string_view text=messages.text();
  while(!text.empty()){
    const size_t eol=text.find('\n');
    std::string_view line=text.substr(0,eol);
    text=eol==std::string_view::npos?std::string_view():text.substr(eol+1);
    while(!line.empty()&&(line.back()==' '||line.back()=='\r'||line.back()=='\t'))line.remove_suffix(1);
    if(line.empty())continue;
    if(!out.empty())out+='\n';
    out+=line;
  }
  return out;
}

void Load::write_kept()
{
  try{
    const std::string what=kept();
    messages.write(what.empty()?std::string():Error{"load",what,where}.text()+'\n');
  }catch(const std::bad_alloc &){
    out_of_memory("a load's messages");
  }
}

Error Load::describe(std::string_view also)
{
  Error e{"load",kept(),where};
  messages.clear();
  if(!also.empty()){
    if(!e.what.empty())e.what+='\n';
    e.what+=also;
  }
  if(e.what.empty())e.what="the model does not load";
  return e;
}

void Load::commit()
{
  committed=true;
  previous_session.reset(); /* before the Model it points to */
  previous_model.reset();
}

void init_alloc_info(Session &s)
{
  SolverWork &w=*s.solver_work;
  w.xpv.node=s.model().node+s.model().nmarkov;
  w.xpv.nvec=0; /* this is just for now */
  /* called again once the model's options are read: a fresh zeroed block */
  try {
    w.state.assign(w.xpv.nvec+w.xpv.node,0.0);
  } catch (const std::bad_alloc &) {
    xpp::out_of_memory("the state vector");
  }
  w.xpv.x=w.state.data();
}

void storage_precision_changed(Session &s)
{
  const StoragePrecision wanted=s.numerics->storage_precision;
  const Result<> converted=s.data_store->set_precision(wanted);
  if(!converted){
    s.numerics->storage_precision=s.data_store->precision();
    ok_or_show(converted);
    return;
  }
  s.histogram->hist_columns.retype(*s.data_store);
  s.histogram->four_columns.retype(*s.data_store);
  s.adjoint->adjoint.retype(*s.data_store);
  s.adjoint->h_function.retype(*s.data_store);
  s.adjoint->transposed.retype(*s.data_store);
  retype_stochastic(s);
}

void apply_option_effect(Session &s, OptionEffect effect)
{
  switch(effect){
  case OptionEffect::none: break;
  case OptionEffect::step_size: dt_changed(s); break;
  case OptionEffect::delay: chk_delay(s); break;
  case OptionEffect::storage_precision: storage_precision_changed(s); break;
  }
}

}

/* the array plot's movie is a Writer (the files module's), which the plot state's header does not see: the Session, which holds
   the state, is where it is made and destroyed */
ArrayPlotState::ArrayPlotState()=default;
ArrayPlotState::~ArrayPlotState()=default;
