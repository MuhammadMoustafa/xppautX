#include "model_flags.h"

#include "model.h"
#include "session.h"
#include "typed_values.h"
#include "expr.h"
#include "xpp_log.h"

#include <strings.h>

#define IC 2
#define PARAM 1

int add_global(xpp::Session &s, const char *cond, int sign, const std::vector<xpp::FlagEvent> &events)
{
  xpp::Model &m=s.model();
  if(static_cast<int>(events.size())>xpp::max_events){
    xpp::log(XPP_LOG_WARN, " Too many events per flag \n");
    return(1);
  }
  xpp::Model::GlobalFlag &f=m.flags.emplace_back();
  f.cond=cond;
  for(size_t e=0;e<events.size();e++){
    f.lhsname[e]=events[e].name;
    f.rhs[e]=events[e].formula;
  }
  f.sign=sign;
  f.nevents=static_cast<int>(events.size());
  return(0);
}

/* expr compiled (add_expr), its ENDEXP included; false if it does not
   parse */
static bool compile(xpp::Session &s, const std::string &expr, std::vector<int> &out)
{
  int nc;
  if(xpp::add_expr(s.parser,expr,out,&nc))return false;
  return true;
}

int compile_flags(xpp::Session &s)
{
  std::vector<xpp::Model::GlobalFlag> &flags=s.model().flags;
  int j;
  int i,index;
  if(s.model().flag_count()==0)return(0);
  for(j=0;j<s.model().flag_count();j++){
    if(!compile(s,flags[j].cond,flags[j].comcond.rpn)){
      xpp::log(XPP_LOG_WARN, "Illegal global condition:  {}\n",flags[j].cond);
      return(1);
    }
    flags[j].anypars=0;
    flags[j].nointerp=0;
    for(i=0;i<flags[j].nevents;i++){
      const char *name=flags[j].lhsname[i].c_str();
      index=xpp::find_user_name(s.model(),IC,name);
      if(index<0){
	index=xpp::find_user_name(s.model(),PARAM,name);
	if(index<0){
	  if(strcasecmp(name,"out_put")==0)
	    {
	      flags[j].type[i]=xpp::Model::EventTarget::output;
	      flags[j].lhs[i]=0;
	    }
	  else {
	    if(strcasecmp(name,"arret")==0)
	      {
		flags[j].type[i]=xpp::Model::EventTarget::halt;
		flags[j].lhs[i]=0;

	      }
	    else {
	      if(strcasecmp(name,"no_interp")==0)
		{
		  flags[j].nointerp=1;
                  flags[j].type[i]=xpp::Model::EventTarget::variable;
		  flags[j].lhs[i]=0;
		}

	    else {
	      xpp::log(XPP_LOG_WARN, " <{}> is not a valid variable/parameter name \n",
		     name);
	      return(1);
	    }
	    }
	  }
	}
	else{
	  flags[j].lhs[i]=index;
	  flags[j].type[i]=xpp::Model::EventTarget::parameter;
          flags[j].anypars=1;
	}
      }
      else {
	flags[j].lhs[i]=index;
	flags[j].type[i]=xpp::Model::EventTarget::variable;
      }
      if(!compile(s,flags[j].rhs[i],flags[j].comrhs[i].rpn)){
	xpp::log(XPP_LOG_WARN, "Illegal event {} for global {}\n",
	       flags[j].rhs[i],flags[j].cond);
      return(1);
      }
    }
  }
  return(0);
}
