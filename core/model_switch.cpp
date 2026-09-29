/* File > Open model and File > Reload: see model_switch.h. */
#include "model_switch.h"
#include "session.h"
#include "model.h"
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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

/* the program's name a command line starts with: the one the model was
   loaded with */
std::string program_name()
{
  const std::vector<std::string> &now=xpp::model().command_line;
  return now.empty()?std::string("xppautX"):now[0];
}

/* a file a load can read: there, and not a folder (load_eqn would ask
   for another file instead) */
bool model_file_ok(const std::string &file)
{
  return !file.empty()&&xpp_files_exists(file.c_str())&&!xpp_files_is_dir(file.c_str());
}

/* variable i's name, "T" for 0 (the Poincare section's numbering) */
std::string poincare_name(int i)
{
  const xpp::Model &m=xpp::model();
  if(i==0)return "T";
  return i>0&&i<=m.neq?m.uvar_names[i-1]:std::string();
}

/* the model's own ICs (the ODEs and Markov variables): not an aux quantity */
int ic_index(const std::string &name)
{
  const xpp::Model &m=xpp::model();
  int i=find_user_name(ICBOX,name);
  return i<m.node+m.nmarkov?i:-1;
}

} // namespace

void xpp_model_open(const char *path)
{
  std::string file=path?path:"";
  if(file.empty()){
    file=xpp_files_working_dir();
    if(file.empty()||file.back()!='/')file+='/';
    if(!file_selector("Open model",file,"*.ode*"))return;
  }
  if(!model_file_ok(file)){
    err_msg(xpp::format("Cannot open {}",file).c_str());
    return;
  }
  std::pair<std::string,std::string> where=xpp_files_split_path(file);
  const std::string question=xpp::format("Open {}? This model's data and diagram go. Save its session first?",
                                         where.second);
  switch(TwoChoice("Save first","Don't save",question.c_str(),"sd")){
  case 's':
    if(!xpp_session_save(nullptr))return;
    break;
  case 'd':
    break;
  default:
    return;
  }
  /* loaded from its own folder, as a double-click starts it: the folder
     the page's files are (xpp_files.h) */
  xpp::session().model_request=xpp::ModelRequest{where.first,where.second,{program_name(),where.second},false};
}

void xpp_model_reload(void)
{
  const xpp::Model &m=xpp::model();
  if(m.command_line.empty()||m.this_file=="console"){
    err_msg("This model was not read from a file: there is nothing to reload");
    return;
  }
  /* a model picked at the start (no file on the command line) is loaded
     by its name this time */
  std::vector<std::string> command_line=m.command_line;
  if(!xpp::session().got_file)command_line={program_name(),m.this_file};
  xpp::session().model_request=xpp::ModelRequest{m.load_dir,m.this_file,std::move(command_line),true};
}

namespace xpp {

std::optional<ModelRequest> take_model_request()
{
  std::optional<ModelRequest> req=std::move(session().model_request);
  session().model_request.reset();
  return req;
}

KeptValues keep_values()
{
  const Model &m=model();
  const Session &s=session();
  KeptValues kept;
  for(int i=0;i<m.nupar;i++){
    double z=0;
    get_val(m.upar_names[i],&z);
    kept.pars.emplace_back(m.upar_names[i],z);
  }
  for(int i=0;i<m.node+m.nmarkov;i++)
    kept.ics.emplace_back(m.uvar_names[i],s.last_ic[i]);
  for(int i=0;i<m.node;i++)
    kept.delays.emplace_back(m.uvar_names[i],s.delay_string[i]);
  kept.numerics=s.numerics;
  kept.poivar=poincare_name(s.numerics.poivar);
  return kept;
}

void restore_values(const KeptValues &kept)
{
  const Model &m=model();
  Session &s=session();
  /* the numerics: the settings kept, what a run leaves (data stored, a
     range's or shooting's state, the last seed) the new session's own */
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
    const int i=find_user_name(ICBOX,kept.poivar);
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
  if(disc(m.this_file))n.method=0;
  do_meth(); /* starts the method's solver too */
  set_delay();

  for(const std::pair<std::string,double> &p : kept.pars)
    if(find_user_name(PARAMBOX,p.first)>=0)set_val(p.first,p.second);
  box_values_loaded(PARAMBOX);
  for(const std::pair<std::string,double> &v : kept.ics){
    const int i=ic_index(v.first);
    if(i>=0)s.last_ic[i]=v.second;
  }
  for(const std::pair<std::string,std::string> &d : kept.delays){
    const int i=ic_index(d.first);
    if(i>=0&&i<m.node)s.delay_string[i]=d.second;
  }
}

bool load_requested(const ModelRequest &req)
{
  const std::string before=xpp_files_working_dir(),before_file=model().this_file;
  if(!req.dir.empty()&&xpp_files_change_dir(req.dir.c_str())!=0){
    err_msg(xpp::format("Cannot open the folder {}",req.dir).c_str());
    return false;
  }
  auto back=[&before](){ if(!before.empty())xpp_files_change_dir(before.c_str()); };
  if(!model_file_ok(req.file)){
    back();
    err_msg(xpp::format("Cannot open {}",req.file).c_str());
    return false;
  }
  /* load_model takes argv as main has it: writable, NULL after the last */
  std::vector<std::string> args=req.command_line;
  std::vector<char *> argv;
  for(std::string &a : args)argv.push_back(a.data());
  argv.push_back(nullptr);
  if(std::optional<Diagnostic> failed=load_model(static_cast<int>(args.size()),argv.data(),0)){
    back();
    err_msg(xpp::format("{} could not be loaded ({}); {} is still loaded",req.file,failed->text(),before_file).c_str());
    return false;
  }
  return true;
}

}
