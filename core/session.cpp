/* A load's swap of the current Model and Session (session.h). */
#include "session.h"

#include <memory>

namespace xpp {

Load::Load()
  : previous_model(detail::current_slot<Model>()),
    previous_session(detail::current_slot<Session>())
{
  std::unique_ptr<Model> model=std::make_unique<Model>();
  Session *fresh=new Session(*model);
  /* the process's AUTO scratch folder (xppautx_main.cpp makes it before
     the first load) stays the same across loads */
  if(previous_session)fresh->auto_state.dir=previous_session->auto_state.dir;
  detail::current_slot<Model>()=model.release();
  detail::current_slot<Session>()=fresh;
  detail::current_slot<Load>()=this;
}

Load::~Load()
{
  detail::current_slot<Load>()=nullptr;
  if(committed)return;
  delete detail::current_slot<Model>();
  delete detail::current_slot<Session>();
  detail::current_slot<Model>()=previous_model;
  detail::current_slot<Session>()=previous_session;
}

void Load::at(std::string_view file, int line, int col)
{
  Load *load=detail::current_slot<Load>();
  if(!load)return;
  load->where.file=file;
  load->where.line=line;
  load->where.col=col;
  load->messages.clear();
}

Diagnostic Load::diagnostic()
{
  const Load &load=*detail::current_slot<Load>();
  Diagnostic d=load.where;
  /* the messages without the blank lines around them, each line without
     the blanks at its end (a caret line keeps those in front) */
  std::string_view text=load.messages.text();
  while(!text.empty()){
    const size_t eol=text.find('\n');
    std::string_view line=text.substr(0,eol);
    text=eol==std::string_view::npos?std::string_view():text.substr(eol+1);
    while(!line.empty()&&(line.back()==' '||line.back()=='\r'||line.back()=='\t'))line.remove_suffix(1);
    if(line.empty())continue;
    if(!d.cause.empty())d.cause+='\n';
    d.cause+=line;
  }
  if(d.cause.empty())d.cause="the model does not load";
  return d;
}

void Load::commit()
{
  committed=true;
  delete previous_model;
  delete previous_session;
  previous_model=nullptr;
  previous_session=nullptr;
}

}
