/* The session list and a load's swap of its client's Model and Session
   (session.h). */
#include "session.h"
#include "model_files.h"
#include "xpp_mem.h"

#include <memory>
#include <new>
#include <utility>

namespace xpp {

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
   system's to reclaim (CLAUDE.md "Memory"). */
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
  /* the process's AUTO scratch folder (xppautx_main.cpp makes it before
     the first load) stays the same across loads */
  if(c.session)fresh->auto_state.dir=c.session->auto_state.dir;
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

Error Load::error()
{
  Load &load=*loading;
  Error e{"load",load.kept(),load.where};
  load.messages.clear();
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
