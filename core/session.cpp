/* A load's swap of the current Model and Session (session.h). */
#include "session.h"

namespace xpp {

Load::Load()
  : previous_model(detail::current_slot<Model>()),
    previous_session(detail::current_slot<Session>())
{
  Session *fresh=new Session();
  /* the process's AUTO scratch folder (xppautx_main.cpp makes it before
     the first load) stays the same across loads */
  if(previous_session)fresh->auto_state.dir=previous_session->auto_state.dir;
  detail::current_slot<Model>()=new Model();
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

void Load::commit()
{
  committed=true;
  delete previous_model;
  delete previous_session;
  previous_model=nullptr;
  previous_session=nullptr;
}

}
