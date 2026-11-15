/* What a command asked to load, and what a Reload carries over: see model_request.h. */
#include "model_request.h"
#include "model_method.h"

#include <utility>

#include "autox_settings.h"
#include "expr.h"
#include "model_native.h"
#include "form_ode.h"
#include "jacobian.h"
#include "model.h"
#include "model_record.h"
#include "numerics.h"
#include "session.h"
#include "session_values.h"
#include "solver.h"
#include "typed_values.h"
#include "xpp_log.h"
#include "xpp_ui.h"

namespace xpp {

/* the program's name a command line starts with: the one the model was
   loaded with */
std::string program_name(const Model &m)
{
  const std::vector<std::string> &now=m.record->command_line.arguments;
  return now.empty()?std::string("xppautX"):now[0];
}

namespace {

/* variable i's name, "T" for 0 (the Poincare section's numbering) */
std::string poincare_name(const Model &m, int i)
{
  if(i==0)return "T";
  return i>0&&i<=m.neq?m.uvar_names[i-1]:std::string();
}

/* the model's own ICs (the ODEs and Markov variables): not an aux quantity */
int ic_index(const Model &m, const std::string &name)
{
  int i=find_user_name(m,ICBOX,name);
  return i<m.node+m.nmarkov?i:-1;
}

} // namespace

ModelRequest open_request(const Session &s, std::string dir, std::string file)
{
  ModelRequest req;
  req.command_line={program_name(s.model()),file};
  req.dir=std::move(dir);
  req.file=std::move(file);
  return req;
}

ModelRequest saved_request(const Session &s, std::string dir, SavedFile f)
{
  ModelRequest req;
  req.dir=std::move(dir);
  req.file=f.manifest.model_name;
  req.command_line={program_name(s.model())};
  req.command_line.push_back(f.manifest.model_name);
  req.saved=f.model;
  req.restore=std::move(f);
  return req;
}

std::optional<ModelRequest> take_model_request(Session &s)
{
  if(!s.model_request)return std::nullopt;
  std::optional<ModelRequest> req=std::move(*s.model_request);
  s.model_request.reset();
  return req;
}

KeptValues keep_values(const Session &s)
{
  const Model &m=s.model();
  KeptValues kept;
  for(int i=0;i<m.nupar;i++){
    double z=0;
    get_val(s.parser,m.upar_names[i],&z);
    kept.pars.emplace_back(m.upar_names[i],z);
  }
  for(int i=0;i<m.node+m.nmarkov;i++)
    kept.ics.emplace_back(m.uvar_names[i],s.last_ic[i]);
  for(int i=0;i<m.node;i++)
    kept.delays.emplace_back(m.uvar_names[i],s.delay_string[i]);
  kept.numerics=*s.numerics;
  kept.autox_settings=autox_settings_now(xpp::autox_parts(s));
  kept.poincare_variable=poincare_name(m,s.numerics->poincare_variable);
  kept.saved=*s.saved_session;
  return kept;
}

void restore_values(Session &s, const KeptValues &kept)
{
  const Model &m=s.model();
  /* the numerics: the settings kept, what a run leaves (data stored, a
     range's or shooting's state, the last seed) the new session's own */
  autox_settings_reload(xpp::autox_parts(s), kept.autox_settings);
  const NumericsSettings loaded=*s.numerics;
  NumericsSettings &n=*s.numerics;
  n=kept.numerics;
  n.inflag=loaded.inflag;
  n.storflag=loaded.storflag;
  n.endsing=loaded.endsing;
  n.pauser=loaded.pauser;
  n.shoot=loaded.shoot;
  n.par_fol=loaded.par_fol;
  n.fft=loaded.fft;
  n.hist=loaded.hist;
  n.null_here=loaded.null_here;
  n.last_seed=loaded.last_seed;
  int poi=-1;
  if(kept.poincare_variable=="T")poi=0;
  else if(!kept.poincare_variable.empty()){
    const int i=find_user_name(m,ICBOX,kept.poincare_variable);
    if(i>=0)poi=i+1;
  }
  if(poi<0){
    /* its variable is gone: the section the model sets up */
    n.poincare_map=loaded.poincare_map;
    n.poincare_variable=loaded.poincare_variable;
    n.poincare_sign=loaded.poincare_sign;
    n.poincare_plane=loaded.poincare_plane;
    n.poincare_stop=loaded.poincare_stop;
  }else n.poincare_variable=poi;
  if(disc(m))n.method=pick_method(m, "Discrete", command_place()).value();
  /* the exact Jacobian kept for a model that has none: the model's own, said */
  if(n.jacobian==JACOBIAN_EXACT&&!ensure_exact_jacobian(s)){
    n.jacobian=loaded.jacobian;
    xpp::log(XPP_LOG_WARN,"Jacobian set to {}: {} has no exact Jacobian: {}\n",jacobian_name(n.jacobian),
             m.this_file,m.jacobian.unavailable);
  }
  do_meth(s); /* starts the method's solver too */
  set_delay(s);

  for(const std::pair<std::string,double> &p : kept.pars)
    if(find_user_name(m,PARAMBOX,p.first)>=0)set_val(s.parser,p.first,p.second);
  box_values_loaded(s,PARAMBOX);
  for(const std::pair<std::string,double> &v : kept.ics){
    const int i=ic_index(m,v.first);
    if(i>=0)s.last_ic[i]=v.second;
  }
  for(const std::pair<std::string,std::string> &d : kept.delays){
    const int i=ic_index(m,d.first);
    if(i>=0&&i<m.node)s.delay_string[i]=d.second;
  }
  *s.saved_session=kept.saved;
}

} // namespace xpp
