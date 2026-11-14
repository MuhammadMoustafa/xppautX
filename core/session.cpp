/* The session list and a load's swap of its client's Model and Session
   (session.h). */
#include "session.h"
#include "model.h"
#include "model_files.h"
#include "xpp_mem.h"
#include "adj2.h"
#include "ani_data.h"
#include "aniparse.h"
#include "arrayplot.h"
#include "autox_state.h"
#include "browse.h"
#include "dae_fun.h"
#include "delay_handle.h"
#include "diagram.h"
#include "display_state.h"
#include "do_fit.h"
#include "equilibria.h"
#include "graf_par.h"
#include "graphics.h"
#include "grobs.h"
#include "histogram.h"
#include "integrate.h"
#include "jacobian.h"
#include "kinescope.h"
#include "load_eqn.h"
#include "many_pops.h"
#include "markov.h"
#include "marks_data.h"
#include "model_switch.h"
#include "nullcline.h"
#include "phase_data.h"
#include "picture.h"
#include "pp_shoot.h"
#include "simplenet.h"
#include "storage.h"
#include "value_undo.h"
#include "volterra.h"
#include "xpp_session.h"
#include "autox_codes.h"
#include "struct.h"
#include "tabular.h"
#include "userbut.h"
#include "xpp_io.h"

#include <memory>
#include <new>
#include <utility>

namespace xpp {

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
    model_(&m)
{
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
/* the Load in progress */
constinit Load *loading=nullptr;

}

Session &client_session()
{
  Client &c=sessions.entry;
  if(!c.session)[[unlikely]]{
    c.model=std::make_unique<Model>();
    c.session=std::make_unique<Session>(*c.model);
  }
  return *c.session;
}

Load::Load()
{
  /* made before anything changes hands: an allocation that fails leaves
     the client as it was */
  std::unique_ptr<Model> model=std::make_unique<Model>();
  std::unique_ptr<Session> fresh=std::make_unique<Session>(*model);
  Client &c=sessions.entry;
  /* the process's autoX scratch folder (xppautx_main.cpp makes it before
     the first load) stays the same across loads */
  if(c.session)fresh->autox_state->dir=c.session->autox_state->dir;
  session_=fresh.get();
  previous_session=std::exchange(c.session,std::move(fresh));
  previous_model=std::exchange(c.model,std::move(model));
  loading=this;
}

Load::~Load()
{
  write_kept(); /* what a failed load's error did not take */
  loading=nullptr;
  if(committed)return;
  /* the failed Session, then its Model, give way to the ones before */
  Client &c=sessions.entry;
  c.session=std::move(previous_session);
  c.model=std::move(previous_model);
}

bool Load::running() noexcept
{
  return loading!=nullptr;
}

void Load::add_source(Error &e)
{
  Place &p=e.place;
  if(!loading||p.line<=0||!p.source.empty()||p.file.empty())return;
  p.source=model_file_lines(loading->model(),p.file).line(p.line);
}

void Load::at(std::string_view file, int line, int col)
{
  if(!loading)return;
  loading->write_kept();
  loading->where=Place{std::string(file),line,col};
}

Place Load::place()
{
  return loading?loading->where:Place();
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

Error Load::error(std::string_view also)
{
  Load &load=*loading;
  Error e{"load",load.kept(),load.where};
  load.messages.clear();
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

}
