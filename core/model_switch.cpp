/* File > Open model and File > Reload: see model_switch.h. */
#include "solver.h"
#include "model_switch.h"
#include "open_model.h"
#include "model.h"
#include "model_record.h"
#include "recx.h"
#include "snapx.h"
#include "model_load.h"
#include "xpp_path.h"
#include "xpp_text.h"
#include "xpp_fs.h"
#include "xpp_log.h"
#include "xpp_session.h"
#include "xpp_ui.h"
#include "typed_values.h"
#include "session_values.h"
#include "expr.h"
#include "model_native.h"
#include "form_ode.h"
#include "numerics.h"
#include "jacobian.h"
#include "session.h"
#include "comline.h"
#include "expr_program.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <expected>
#include <memory>

namespace {

/* a file a load can read: there, and not a folder (load_eqn would ask
   for another file instead) */
xpp::Result<> model_file_ok(xpp::FileSystem &fs, const std::string &file)
{
  if (file.empty()) return xpp::fail("open model", "no file name was given", xpp::command_place());
  if (!fs.exists(file)) return std::unexpected(xpp::files::open_error("open model", file));
  if (fs.is_dir(file)) return std::unexpected(xpp::files::open_error("open model", file, "a directory, not a model file"));
  return {};
}

/* the model the user opened is the newest of the recent ones; the open goes
   on when the list cannot be kept (a bad preferences.odex is shown, not changed) */
void remember_opened(xpp::FileSystem &fs, const std::string &file)
{
  if(const auto noted=xpp::recent::note(fs,fs.absolute(file));!noted)xpp::show_error(noted.error());
}

} // namespace

xpp::Result<bool> xpp_model_open(xpp::Session &s, const char *path)
{
  std::string file=path?path:"";
  if(file.empty()){
    if(!xpp::file_selector(xpp::OPEN_MODEL_TITLE,file,xpp::open_model_wild()))return false;
  }
  if (const auto checked = model_file_ok(*s.model().fs,file); !checked) return std::unexpected(checked.error());
  /* a recording: its model, in the player (W59b) */
  if(xpp::files::has_extension(file,xpp::recx::extension)){
    remember_opened(*s.model().fs,file);
    play_recording(s,file);
    return true;
  }
  /* a file that carries a model: that model, then what the file adds */
  std::optional<SavedFile> saved;
  if(xpp_saved_file_name(file)){
    saved=xpp_saved_read(*s.model().fs,file);
    if(!saved)return false; /* its reader showed why */
  }
  /* everything below replaces this session (a .snapx of this very model
     too: its values, data and diagram take the place of these), so it
     asks first, as File > Open does (W103 review) */
  if(!xpp_model_may_leave(s,file))return false;
  remember_opened(*s.model().fs,file);
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
  s.model_request=std::make_unique<xpp::ModelRequest>(std::move(req));
  return true;
}

xpp::Result<bool> xpp_model_open_example(xpp::Session &s, std::string_view name)
{
  const xpp::Result<std::string> copy=xpp::examples::install(*s.model().fs,name);
  if(!copy)return std::unexpected(copy.error());
  if(copy->empty())return false;
  xpp::bottom_msg(0,xpp::format("The example {} is copied to {}; this copy is opened, so the example stays as it was",name,*copy));
  return xpp_model_open(s,copy->c_str());
}

namespace {

/* the leave question answered key (LEAVE_KEYS's, 0 a cancel): whether the
   session may go */
bool leave_as(xpp::Session &s, int key, bool with_recording)
{
  switch(key){
  case 's':
    return xpp_session_save_here(s)&&(!with_recording||save_recording(s));
  case 'd':
    return true;
  default:
    return false;
  }
}

} // namespace

bool xpp_session_may_leave(xpp::Session &s, const std::string &question, bool with_recording)
{
  /* the start screen's session has nothing to save */
  if(!s.model().loaded())return true;
  return leave_as(s,xpp::TwoChoice(xpp::LEAVE_SAVE,xpp::LEAVE_DONT_SAVE,question,xpp::LEAVE_KEYS),with_recording);
}

bool xpp_model_may_leave(xpp::Session &s, const std::string &file)
{
  return xpp_session_may_leave(s,xpp::format("Open {}? This model's data and diagram go. Save its session first?",
                                             xpp::files::split_path(file).second),false);
}

bool xpp_session_leave_saved(xpp::Session &s, bool save, bool with_recording)
{
  return !save||leave_as(s,'s',with_recording);
}

void xpp_quit(xpp::Session &s, bool saving)
{
  const bool recording=xpp::recording_in_progress();
  if(saving?xpp_session_leave_saved(s,true,recording):xpp_session_may_leave(s,xpp::leave_question(xpp::Leave::quit,recording),recording))xpp::bye_bye();
}

xpp::Result<bool> xpp_model_reload(xpp::Session &s)
{
  const xpp::Model &m=s.model();
  if(m.record->command_line.arguments.empty()||m.this_file=="console")
    return xpp::fail("reload", "This model was not read from a file: there is nothing to reload", xpp::command_place());
  const std::string question=xpp::format("Reload {}? Its values are kept by name; this model's data and diagram go. Save its session first?",
                                         xpp::files::split_path(m.this_file).second);
  if(!xpp_session_may_leave(s,question,false))return false;
  /* a model picked at the start (no file on the command line) is loaded
     by its name this time */
  std::vector<std::string> command_line=m.record->command_line.arguments;
  if(!s.got_file)command_line={program_name(m),m.this_file};
  xpp::ModelRequest req{m.load_dir,m.this_file,std::move(command_line),true,std::nullopt,std::nullopt};
  if(!m.saved_in.empty())req.saved=xpp::SavedModel{m.saved_in,m.record->files};
  s.model_request=std::make_unique<xpp::ModelRequest>(std::move(req));
  return true;
}

namespace xpp {

Result<Session *> load_requested(const Session &now, const ModelRequest &req)
{
  xpp::FileSystem &fs=*now.model().fs;
  const std::string before=fs.working_dir(),before_file=now.model().this_file;
  if(!req.dir.empty()&&fs.change_dir(req.dir)!=0)return std::unexpected(xpp::files::open_error("open model",req.dir));
  auto back=[&before,&fs](){ if(!before.empty())fs.change_dir(before); };
  if (!req.saved) {
    const auto checked = model_file_ok(*now.model().fs,req.file);
    if (!checked) {
      back();
      return std::unexpected(checked.error());
    }
  }
  Result<CommandLine> line=parse_command_line(req.command_line);
  if(!line){
    back();
    return std::unexpected(line.error());
  }
  /* what a session file adds is read before the load keeps its
     model: a member missing or that does not read fails the open */
  std::function<std::optional<Error>(Session &)> check;
  if(req.restore || req.keep_values)check=[&req,&now](Session &fresh) -> std::optional<Error> {
    if(req.restore) {
      if(auto error=xpp_saved_check(fresh,*req.restore))return error;
    }
    if(req.keep_values) {
      const auto picked=disc(fresh.model()) ? pick_method(fresh.model(),"Discrete",command_place())
                                            : check_method(fresh.model(),now.numerics->method,command_place());
      if(!picked)return picked.error();
    }
    return std::nullopt;
  };
  Loaded loaded=load_model(fs,std::move(*line),0,req.saved?&*req.saved:nullptr,check);
  if(!loaded){
    back();
    /* at the place the load failed at */
    const Error &e=loaded.error();
    return std::unexpected(Error{"open",xpp::format("{} could not be loaded ({}){}",
                                        req.restore?req.restore->name:req.file,e.what,
                                        before_file.empty()?std::string():xpp::format("; {} is still loaded",before_file)),e.place});
  }
  return *loaded;
}

}
