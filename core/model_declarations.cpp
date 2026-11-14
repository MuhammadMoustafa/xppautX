#include "model_declarations.h"

#include <utility>

#include "model.h"
#include "ode_read.h"
#include "xpp_log.h"

namespace xpp {

int split_events(const char *cond, const char *rest, std::vector<FlagEvent> &events)
{
  std::string temp,name;
  events.clear();
  for(const char *p=rest;*p;p++){
    char ch=*p;
    if(ch=='{'||ch==' ')continue;
    if(ch=='}'||ch==';'){
      if(static_cast<int>(events.size())==max_events){
	xpp::log(XPP_LOG_WARN, " Too many events per flag \n");
	return(1);
      }
      if(name.empty()){
	xpp::log(XPP_LOG_WARN, " No event variable named for {} \n",temp);
	return(1);
      }
      events.push_back({name,temp});
      name.clear();
      temp.clear();
      if(ch=='}')break;
      continue;
    }
    if(ch=='='){
      name=temp;
      temp.clear();
      continue;
    }
    temp+=ch;
  }
  if(events.empty()){
    xpp::log(XPP_LOG_WARN, " No events for condition {} \n",cond);
    return(1);
  }
  return(0);
}

std::vector<ArrayInitialValue> array_initial_values(const Model &m)
{
  std::vector<ArrayInitialValue> out;
  if(m.array_ics.defined==0)return out;
  int group=0;
  for(const ArrayIc &ic : m.array_ics.ics){
    if(ic.type!=2)continue;
    group++;
    for(int j=ic.j1;j<=ic.j2;j++){
      ArrayInitialValue v;
      subsk(ic.var,v.var,j,1);
      subsk(ic.formula,v.formula,j,1);
      v.j=j;
      v.group=group;
      out.push_back(std::move(v));
    }
  }
  return out;
}

} // namespace xpp
