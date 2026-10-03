/* File > Open model and File > Reload: see model_switch.h. */
#include "solver.h"
#include "model_switch.h"
#include "session.h"
#include "model.h"
#include "recx.h"
#include "snapx.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_session.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "expr.h"
#include "form_ode.h"
#include "numerics.h"
#include "storage.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

/* the program's name a command line starts with: the one the model was
   loaded with */
std::string program_name(const xpp::Model &m)
{
  const std::vector<std::string> &now=m.command_line;
  return now.empty()?std::string("xppautX"):now[0];
}

/* a file a load can read: there, and not a folder (load_eqn would ask
   for another file instead) */
xpp::Result<> model_file_ok(const std::string &file)
{
  if (file.empty()) return xpp::fail("open model", "no file name was given", xpp::command_place());
  if (!xpp::files::exists(file)) return std::unexpected(xpp::files::open_error("open model", file));
  if (xpp::files::is_dir(file)) return std::unexpected(xpp::files::open_error("open model", file, "a directory, not a model file"));
  return {};
}

/* variable i's name, "T" for 0 (the Poincare section's numbering) */
std::string poincare_name(const xpp::Model &m, int i)
{
  if(i==0)return "T";
  return i>0&&i<=m.neq?m.uvar_names[i-1]:std::string();
}

/* the model's own ICs (the ODEs and Markov variables): not an aux quantity */
int ic_index(const xpp::Model &m, const std::string &name)
{
  int i=xpp::find_user_name(m,ICBOX,name);
  return i<m.node+m.nmarkov?i:-1;
}

} // namespace

void xpp_model_open(xpp::Session &s, const char *path)
{
  std::string file=path?path:"";
  if(file.empty()){
    if(!xpp::file_selector("Open model",file,"*.ode* *" + std::string(xpp::snapx::extension) + " *" + std::string(xpp::recx::extension)))return;
  }
  if (const auto checked = model_file_ok(file); !checked) {
    xpp::show_error(checked.error());
    return;
  }
  /* a recording: its model, in the player (W59b) */
  if(xpp::snapx::has_extension(file,xpp::recx::extension)){
    play_recording(s,file);
    return;
  }
  /* a file that carries a model: that model, then what the file adds */
  std::optional<SavedFile> saved;
  if(xpp_saved_file_name(file)){
    saved=xpp_saved_read(file);
    if(!saved)return;
  }
  /* everything below replaces this session (a .snapx of this very model
     too: its values, data and diagram take the place of these), so it
     asks first, as File > Open does (W103 review) */
  if(!xpp_model_may_leave(s,file))return;
  /* loaded from its own folder, as a double-click starts it: the folder
     the page's files are (xpp_files.h); a saved model from the folder of
     the file it is saved in, which its outputs go to */
  xpp::ModelRequest req;
  if(saved){
    req=xpp::saved_request(s,xpp::files::split_path(saved->path).first,std::move(*saved));
  }else{
    const std::pair<std::string,std::string> where=xpp::files::split_path(file);
    req=xpp::open_request(s,where.first,where.second);
  }
  s.model_request=std::move(req);
}

namespace {

/* the leave question answered key (LEAVE_KEYS's, 0 a cancel): whether the
   session may go */
bool leave_as(xpp::Session &s, int key, bool with_recording)
{
  switch(key){
  case 's':
    return xpp_session_save(s,nullptr,-1)&&(!with_recording||save_recording(s));
  case 'd':
    return true;
  default:
    return false;
  }
}

} // namespace

bool xpp_session_may_leave(xpp::Session &s, const std::string &question, bool with_recording)
{
  return leave_as(s,xpp::TwoChoice(xpp::LEAVE_SAVE,xpp::LEAVE_DONT_SAVE,question,xpp::LEAVE_KEYS),with_recording);
}

bool xpp_model_may_leave(xpp::Session &s, const std::string &file)
{
  return xpp_session_may_leave(s,xpp::format("Open {}? This model's data and diagram go. Save its session first?",
                                             xpp::files::split_path(file).second),false);
}

void xpp_quit(xpp::Session &s, bool saving)
{
  const bool recording=xpp::recording_in_progress();
  if(saving?leave_as(s,'s',recording):xpp_session_may_leave(s,xpp::quit_question(recording),recording))xpp::bye_bye();
}

void xpp_model_reload(xpp::Session &s)
{
  const xpp::Model &m=s.model();
  if(m.command_line.empty()||m.this_file=="console"){
    xpp::command_error("reload", "This model was not read from a file: there is nothing to reload");
    return;
  }
  const std::string question=xpp::format("Reload {}? Its values are kept by name; this model's data and diagram go. Save its session first?",
                                         xpp::files::split_path(m.this_file).second);
  if(!xpp_session_may_leave(s,question,false))return;
  /* a model picked at the start (no file on the command line) is loaded
     by its name this time */
  std::vector<std::string> command_line=m.command_line;
  if(!s.got_file)command_line={program_name(m),m.this_file};
  xpp::ModelRequest req{m.load_dir,m.this_file,std::move(command_line),true,std::nullopt,std::nullopt};
  if(!m.saved_in.empty())req.saved=xpp::SavedModel{m.saved_in,m.files};
  s.model_request=std::move(req);
}

namespace xpp {

ModelRequest open_request(const Session &s, std::string dir, std::string file)
{
  ModelRequest req;
  req.command_line={program_name(s.model()),file};
  req.dir=std::move(dir);
  req.file=std::move(file);
  return req;
}

const char *quit_question(bool recording)
{
  return recording?"Quit xppautX? Save this session, and the recording in progress, first?"
                  :"Quit xppautX? Save this session first?";
}

ModelRequest saved_request(const Session &s, std::string dir, SavedFile f)
{
  ModelRequest req;
  req.dir=std::move(dir);
  req.file=f.manifest.model_name;
  req.command_line={program_name(s.model())};
  for(std::string &a : xpp_saved_args(f))req.command_line.push_back(std::move(a));
  req.saved=f.model;
  req.restore=std::move(f);
  return req;
}

std::optional<ModelRequest> take_model_request(Session &s)
{
  std::optional<ModelRequest> req=std::move(s.model_request);
  s.model_request.reset();
  return req;
}

KeptValues keep_values(const Session &s)
{
  const Model &m=s.model();
  KeptValues kept;
  for(int i=0;i<m.nupar;i++){
    double z=0;
    get_val(s,m.upar_names[i],&z);
    kept.pars.emplace_back(m.upar_names[i],z);
  }
  for(int i=0;i<m.node+m.nmarkov;i++)
    kept.ics.emplace_back(m.uvar_names[i],s.last_ic[i]);
  for(int i=0;i<m.node;i++)
    kept.delays.emplace_back(m.uvar_names[i],s.delay_string[i]);
  kept.numerics=s.numerics;
  kept.auto_settings=auto_settings_now(s);
  kept.poivar=poincare_name(m,s.numerics.poivar);
  return kept;
}

void restore_values(Session &s, const KeptValues &kept)
{
  const Model &m=s.model();
  /* the numerics: the settings kept, what a run leaves (data stored, a
     range's or shooting's state, the last seed) the new session's own */
  auto_settings_reload(s, kept.auto_settings);
  const NumericsSettings loaded=s.numerics;
  NumericsSettings &n=s.numerics;
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
  if(kept.poivar=="T")poi=0;
  else if(!kept.poivar.empty()){
    const int i=find_user_name(m,ICBOX,kept.poivar);
    if(i>=0)poi=i+1;
  }
  if(poi<0){
    /* its variable is gone: the section the model sets up */
    n.poimap=loaded.poimap;
    n.poivar=loaded.poivar;
    n.poisgn=loaded.poisgn;
    n.poipln=loaded.poipln;
    n.sos=loaded.sos;
  }else n.poivar=poi;
  if(disc(m))n.method=pick_method(m, "Discrete", command_place()).value();
  do_meth(s); /* starts the method's solver too */
  set_delay(s);

  for(const std::pair<std::string,double> &p : kept.pars)
    if(find_user_name(m,PARAMBOX,p.first)>=0)set_val(s,p.first,p.second);
  box_values_loaded(s,PARAMBOX);
  for(const std::pair<std::string,double> &v : kept.ics){
    const int i=ic_index(m,v.first);
    if(i>=0)s.last_ic[i]=v.second;
  }
  for(const std::pair<std::string,std::string> &d : kept.delays){
    const int i=ic_index(m,d.first);
    if(i>=0&&i<m.node)s.delay_string[i]=d.second;
  }
}

Session *load_requested(const Session &now, const ModelRequest &req)
{
  const std::string before=xpp::files::working_dir(),before_file=now.model().this_file;
  if(!req.dir.empty()&&xpp::files::change_dir(req.dir.c_str())!=0){
    xpp::show_error(xpp::files::open_error("open model",req.dir));
    return nullptr;
  }
  auto back=[&before](){ if(!before.empty())xpp::files::change_dir(before.c_str()); };
  if (!req.saved) {
    const auto checked = model_file_ok(req.file);
    if (!checked) {
      back();
      xpp::show_error(checked.error());
      return nullptr;
    }
  }
  /* load_model takes argv as main has it: writable, NULL after the last */
  std::vector<std::string> args=req.command_line;
  std::vector<char *> argv;
  for(std::string &a : args)argv.push_back(a.data());
  argv.push_back(nullptr);
  /* what a session file adds is read before the load keeps its
     model: a member missing or that does not read fails the open */
  std::function<std::optional<Error>(Session &)> check;
  if(req.restore || req.keep_values)check=[&req,&now](Session &fresh) -> std::optional<Error> {
    if(req.restore) {
      if(auto error=xpp_saved_check(fresh,*req.restore))return error;
    }
    if(req.keep_values) {
      const auto picked=disc(fresh.model()) ? pick_method(fresh.model(),"Discrete",command_place())
                                            : check_method(fresh.model(),now.numerics.method,command_place());
      if(!picked)return picked.error();
    }
    return std::nullopt;
  };
  Loaded loaded=load_model(static_cast<int>(args.size()),argv.data(),0,req.saved?&*req.saved:nullptr,check);
  if(!loaded){
    back();
    /* at the place the load failed at */
    const Error &e=loaded.error();
    show_error(Error{"open",xpp::format("{} could not be loaded ({}); {} is still loaded",
                                        req.restore?req.restore->name:req.file,e.what,before_file),e.place});
    return nullptr;
  }
  return *loaded;
}

}
