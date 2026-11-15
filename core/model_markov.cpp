#include "model_failed.h"
#include "model_markov.h"

#include "model.h"
#include "model_record.h"
#include "model_place.h"
#include "session.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace xpp {

void add_wiener(xpp::Session &s, int index)
{
  s.model().wiener.push_back(index);
  s.model().nwiener++;
}

void add_markov(xpp::Session &s, int nstate, const char *name)
{
  std::vector<double> st(nstate);
  for(int i=0;i<nstate;i++)st[i]=static_cast<double>(i);
  create_markov(s,nstate,st.data(),0,name);
}

/* the Markov variable a transition table names (the longest name that
   is a prefix of it); a model naming none fails to load. Writes the
   table's header line when converting. */
static int markov_named(xpp::Session &s, const char *name)
{
  int index=-1;
  const std::string upper=xpp::upper_case(name);
  const auto &lengths=s.model().record->markov_name_lengths;
  for(auto length=lengths.rbegin();length!=lengths.rend();++length){
    if(*length>upper.size())continue;
    const auto it=s.model().record->names.find(upper.substr(0,*length));
    if(it!=s.model().record->names.end() && it->second.markov_declared){
      index=it->second.index;
      break;
    }
  }
  if(index==-1){
    model_failed(xpp::format("Markov variable {} not found",name));
  }
  return index;
}

int build_markov(xpp::Session &s, std::span<const std::string> cells, const char *name)
{
 int index=markov_named(s,name);
 int nstates=s.model().markov[index].nstates;
 xpp::log(XPP_LOG_INFO, " Building {} {:d} states...\n",name,nstates);
 for(int i=0;i<nstates;i++){
   for(int j=0;j<nstates;j++){
     const std::string &expr=cells[static_cast<size_t>(i*nstates+j)];
     xpp::log(XPP_LOG_INFO, "{} ",expr.c_str());
     add_markov_entry(s,index,i,j,expr.c_str());
   }
   xpp::log(XPP_LOG_INFO, "\n");
 }
 return index;
}

int old_build_markov(xpp::Session &s, const std::function<bool(std::string &)> &next_line, const char *name)
{
  int istart;

 int i,j;
 int index=markov_named(s,name);
 int nstates=s.model().markov[index].nstates;
 xpp::log(XPP_LOG_INFO, " Building {} ...\n",name);
 {
   /* a whole line at a time, from the same source as the model's other lines */
   std::string line;
   for(i=0;i<nstates;i++){
    if(!next_line(line)){
      model_failed(xpp::format("Unexpected end of file building Markov variable {}",name));
    }

   istart=0;
     for(j=0;j<nstates;j++){
       std::string expr = markov_cell(line.c_str(),&istart);
       xpp::log(XPP_LOG_INFO, "{} ",expr.c_str());
       add_markov_entry(s,index,i,j,expr.c_str());
     }
   xpp::log(XPP_LOG_INFO, "\n");
   }
 }
 return index;
}
  
std::string markov_cell(const char *source, int *i0)
{
 std::string dest;
 char ch;
 int flag=0;
 while(1)
   {
     ch=source[*i0];
     *i0=*i0+1;
     if(ch=='}')break;
     if(ch=='{')flag=1;
     else {
       if(flag){
	 dest.push_back(ch);
       }
     }
   }
   return dest;
}

void create_markov(xpp::Session &s, int nstates, double *st, int type, const char *name)
{
  int n2=nstates*nstates;
  int j=s.model().nmarkov;
  s.model().markov.resize(j+1);

  s.model().markov[j].nstates=nstates;
  s.model().markov[j].states.assign(st, st+nstates);
  if(type==0){
    s.model().markov[j].trans.assign(n2, std::string());
    s.model().markov[j].command.assign(n2, Program());
  }
  else {
    s.model().markov[j].fixed.assign(n2, 0.0);
  }
  auto &entry=s.model().record->names[xpp::converted(name)];
  if(!entry.markov_declared){
    entry.markov_declared=true;
    entry.kind=xpp::ModelRecord::NameKind::Markov;
    entry.index=j;
    entry.written=name;
    entry.pos={0,xpp::LoadSite::place().line,xpp::LoadSite::place().col};
  }
  s.model().record->markov_name_lengths.insert(std::string_view(name).size());
  s.model().markov[j].name = name;
  s.model().nmarkov++;

}

void add_markov_entry(xpp::Session &s, int index, int j, int k, const char *expr)
{
  
  int l0=s.model().markov[index].nstates*j+k;
  int type=s.model().markov[index].type;
  if(type==0){
  s.model().markov[index].trans[l0]=expr;
  /*  compilation step -- can be delayed */
  /*  end of compilation   */
  
}
  else {
    s.model().markov[index].fixed[l0]=atof(expr);
  }
}

void compile_all_markov(xpp::Session &s)
{
  int index,j,k,ns,l0;
  if(s.model().nmarkov==0)return;
  for(index=0;index<s.model().nmarkov;index++){
    ns=s.model().markov[index].nstates;
    for(j=0;j<ns;j++){
      for(k=0;k<ns;k++){
	l0=ns*j+k;
	if(compile_markov(s,index,j,k)==-1){
	  const xpp::Model &m=s.model();
	  model_failed(xpp::Error{"markov",xpp::format("Bad expression {}[{}][{}] = {}",m.markov[index].name,j,k,
	                                               m.markov[index].trans[l0]),xpp::model_place(m,m.markov[index].name)});
	}
      }
    }
  }
}

int compile_markov(xpp::Session &s, int index, int j, int k)
{
  const char *expr;
  int l0=s.model().markov[index].nstates*j+k,leng;
  expr=s.model().markov[index].trans[l0].c_str();

  if(add_expr(s.parser,expr,s.model().markov[index].command[l0].rpn,&leng))
    return -1;
  /* zero-padded by two */
  s.model().markov[index].command[l0].rpn.resize(leng+2, 0);
  
  return 1;
}

} // namespace xpp
