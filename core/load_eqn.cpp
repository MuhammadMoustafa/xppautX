#include <algorithm>
#include <ranges>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "model.h"
#include "model_record.h"
#include "model_failed.h"
#include "expr.h"
#include "xpp_ui.h"
#include "typed_values.h"

#include "model_place.h"
#include "plot_settings.h"
#include "load_eqn.h"
#include "form_ode.h"



#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <cstdint>
#include <expected>
#include <unordered_map>

namespace xpp {




namespace {

/* "name=value" split at its first '='; value "" when there is none */
void split_apart(std::string_view bob, std::string &name, std::string &value)
{
  size_t k = bob.find('=');
  if (k == std::string_view::npos) {
    name = bob;
    value.clear();
  } else {
    name = trim_blanks(bob.substr(0, k));
    value = trim_blanks(bob.substr(k + 1));
  }
}

/* every name=value of an option line (its first token, the @ or $,
   skipped; then tokens split at delims) to set(name, value), those with
   an empty name or value left out; first: the delimiters of the first
   token */
template <class F>
void each_option(std::string_view line, std::string_view first, std::string_view delims, F set,
                 const std::function<void(std::string_view)> &ignored = {})
{
  xpp::Tokens tok(line);
  if (!tok.next(first)) return;
  std::string name, value;
  while (std::optional<std::string_view> t = tok.next(delims)) {
    split_apart(*t, name, value);
    if (!name.empty() && !value.empty()) set(name, value);
    else if(ignored) ignored(*t);
  }
}

} // namespace



/*   this file has all of the phaseplane parameters defined   
     and created.  All other files should use external stuff
    to use them. (Except eqn forming stuff)
 */





/* here is some new code for internal set files:
   format of the file is a long string of the form:
   { x=y, z=w, q=p , .... }
*/

namespace {

/* the set added at where (the line of its statement) */
void add_set_at(xpp::Model &m, std::string_view name, std::string_view does,
                std::vector<std::pair<std::string,std::string>> settings, const xpp::Place &where)
{
  std::vector<xpp::Model::InternalSet> &sets=m.intern_sets;
  /* "$ " then does without its braces, commas as spaces */
  std::string bob="$ ";
  for(char c:does){
    if(c=='}'||c=='{')
      continue;
    bob+=c==','?' ':c;
  }
  auto [it,inserted]=m.record->names.try_emplace(xpp::converted(name));
  if(inserted){
    it->second.kind=xpp::ModelRecord::NameKind::Set;
    it->second.index=static_cast<int>(sets.size());
    it->second.written=name;
    it->second.pos={0,where.line,where.col};
  }
  if(it->second.set<0)it->second.set=static_cast<int>(sets.size());
  it->second.set_spellings.try_emplace(std::string(name),static_cast<int>(sets.size()));
  sets.push_back({std::string(name),bob,std::move(settings),where});
 xpp::log(XPP_LOG_INFO, " added {} doing {} \n",
	 sets.back().name.c_str(),sets.back().does.c_str());
}

} // namespace

void add_intern_set(xpp::Model &m, std::string_view name, std::string_view does,
                    std::vector<std::pair<std::string,std::string>> settings)
{
  add_set_at(m,name,does,std::move(settings),xpp::LoadSite::place());
}

void replace_intern_sets(xpp::Model &m, const xpp::odex::Parsed &parsed)
{
  for(auto &[key,entry] : m.record->names){
    entry.set=-1;
    entry.set_spellings.clear();
  }
  std::erase_if(m.record->names,[](const auto &entry){ return entry.second.kind==xpp::ModelRecord::NameKind::Set; });
  m.intern_sets.clear();
  for(const xpp::odex::Statement &st : parsed.statements){
    if(st.kind!=xpp::odex::Statement::Kind::Set)continue;
    std::vector<std::pair<std::string,std::string>> settings;
    for(const xpp::odex::Option &o : st.more().options)settings.emplace_back(xpp::odex_option_setting(o.group,o.name),o.value);
    add_set_at(m,st.name,st.text,std::move(settings),xpp::Place{parsed.files.at(static_cast<std::size_t>(st.pos.file)),st.pos.line,st.pos.col});
  }
}

void replace_model_options(xpp::Model &m, const xpp::odex::Parsed &parsed)
{
  m.options.clear();
  for(const xpp::odex::Statement &st : parsed.statements)
    if(st.kind==xpp::odex::Statement::Kind::Options&&!st.more().options.empty())add_model_options(m,st.more().options,parsed.files);
}

odex::ValueKind value_kind(const xpp::Model &m, std::string_view name)
{
  const std::string upper=converted(name);
  if(find_user_name(m,ICBOX,upper)>-1)return odex::ValueKind::state;
  if(find_user_name(m,PARAMBOX,upper)>-1)return odex::ValueKind::parameter;
  return odex::ValueKind::other;
}


int msc(std::string_view s1, std::string_view s2)
{
 /* s2 starts with s1 */
 return s2.starts_with(s1);
}  
  
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set,
                                                              std::vector<std::string> *ignored)
{
  std::vector<std::pair<std::string, std::string>> out;
  auto keep=[&out](const std::string &name,const std::string &value){ out.emplace_back(name,value); };
  auto drop=[ignored](std::string_view token){ if(ignored) ignored->emplace_back(token); };
  if(set)each_option(line," "," ,;\n",keep,drop);
  else each_option(line," ,"," ,\n\r",keep,drop);
  return out;
}

int add_model_option(xpp::Model &m, std::string_view s1)
{
  /* dll_lib= and dll_fun= named a compiled library and its function */
  const char *refused=nullptr;
  each_option(s1," ,"," ,\n\r",[&refused](const std::string &name,const std::string &){
    if(refused)return;
    std::string upper=xpp::upper_case(name);
    if(msc("DLL_LIB",upper))refused="dll_lib";
    else if(msc("DLL_FUN",upper))refused="dll_fun";
  });
  if(refused)refuse_compiled_functions(refused);
  /* an .ode's value ends at a space as well as a comma, as XPPAUT reads it
     (`@ parmin=-.2 parmax=.5`) */
  m.options.push_back(xpp::Model::OptionLine{std::string(s1),option_items(s1,false),xpp::LoadSite::place()});
  return 0;
}

void add_model_options(xpp::Model &m, std::span<const xpp::odex::Option> entries, const std::vector<std::string> &files)
{
  for(const xpp::odex::Option &o : entries){
    m.options.push_back(xpp::Model::OptionLine{"",{{xpp::odex_option_setting(o.group,o.name),o.value}},
      xpp::Place{files.at(static_cast<std::size_t>(o.pos.file)),o.pos.line,o.pos.col}});
  }
}

} // namespace xpp
