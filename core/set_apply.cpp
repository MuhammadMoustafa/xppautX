/* A model's sets and the settings applied to the Session as patches (set_apply.h): the model's own values back, a set applied, a
   group of settings read, written or captured for a save. All or nothing, one undo step. */
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
#include "xpp_ui.h"
#include "xpp_util.h"
#include "session_values.h"
#include "typed_values.h"
#include "xpp_text.h"

#include "model_place.h"
#include "plot_settings.h"
#include "plot_state.h"
#include "load_eqn.h"
#include "numerics_settings.h"
#include "value_undo.h"

#include "numerics.h"

#include "storage.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <cstdint>
#include <expected>
#include <unordered_map>
#include "model_failed.h"
#include "xpp_log.h"
#include "session.h"

#include "set_apply.h"

namespace xpp {

namespace {

/* a variable's name as its equation spells it (the Model keeps them in upper case) */
std::string declared_spelling(const xpp::Model &m, const std::string &name)
{
  if(const auto *entry=m.record->name(name);entry && !entry->written.empty())return entry->written;
  return name;
}

} // namespace

namespace {

/* why name=value cannot be applied (an initial condition or a parameter
   whose value is not a number, an option that does not take it), "" when
   it can */
std::string intern_item_problem(xpp::Session &s, const std::string &name1, const std::string &value, bool xppaut_names)
{
  double z=0;
  if(value_kind(s.model(),name1)!=odex::ValueKind::other)
    return xpp::parse_number(xpp::trim_blanks(value),z)?std::string():"not a number";
  const auto why=option_problem(s,converted(name1),value,xppaut_names);
  return why.value_or("");
}

} // namespace

namespace {

/* the scope an item belongs to: parameters, initial (a state) or its
   option's group ("" for a name that is none of them: only `all` takes it) */
std::string_view item_scope(const xpp::Session &s, const SetItem &item)
{
  if(!item.setting){
    const odex::ValueKind kind=value_kind(s.model(),item.name);
    if(kind==odex::ValueKind::state)return set_scope::initial;
    if(kind==odex::ValueKind::parameter)return set_scope::parameters;
  }
  int index=0;
  const OptionRow *row=find_option(item.name,index,false);
  return row?row->group:std::string_view();
}

} // namespace

std::vector<SetItem> scoped_items(const xpp::Session &s, std::span<const SetItem> items, std::string_view scope)
{
  std::vector<SetItem> out;
  for(const SetItem &item : items)
    if(scope==set_scope::all||item_scope(s,item)==scope)out.push_back(item);
  return out;
}

namespace {

/* every item checked before one is applied: the first bad one is the error,
   at its own line when it has one, else at where, and s is as it was */
xpp::Result<> check_set_items(xpp::Session &s, std::span<const SetItem> items, const xpp::Place &where, bool xppaut_names, std::string_view command)
{
  for(const SetItem &item : items){
    const std::string why=item.setting?option_problem(s,item.name,item.value,xppaut_names).value_or(""):
                                       intern_item_problem(s,item.name,item.value,xppaut_names);
    if(!why.empty()){
      xpp::Place at=item.at;
      if(at.line>0&&at.file.empty())at.file=where.file; /* an item of the set's own file names none */
      return std::unexpected(xpp::Error{std::string(command),xpp::format("{}={}: {}",item.name,item.value,why),at.line>0?at:where});
    }
  }
  return {};
}

/* the checked items applied, in order */
void apply_set_items(xpp::Session &s, std::span<const SetItem> items)
{
  for(const SetItem &item : items){
    if(item.setting){
      set_option(s,item.name,item.value,1,NULL);
      /* what a change of the setting needs besides its member (the step's recount, the stored rows' precision, the delay's storage) */
      int index=0;
      if(const OptionRow *row=find_option(item.name,index,false))apply_option_effect(s,row->effect);
      xpp::do_meth(s);
    }
    else do_intern_set(s,item.name,item.value);
  }
}

std::vector<SetItem> set_items(std::string_view text)
{
  std::vector<SetItem> items;
  for(auto &[name,value] : option_items(text,true))items.push_back({std::move(name),std::move(value),false});
  return items;
}

std::vector<SetItem> set_items(const xpp::Model::InternalSet &set)
{
  std::vector<SetItem> items=set_items(set.does);
  for(const auto &[name,value] : set.settings)items.push_back({name,value,true});
  return items;
}

/* the open graph `graph` names, the active one when none; the error otherwise */
xpp::Result<int> target_graph(const xpp::Session &s, std::optional<int> graph, std::string_view command)
{
  if(!graph)return s.plot_windows->active;
  if(*graph<0||*graph>=static_cast<int>(s.plot_windows->graph.size())||!s.plot_windows->graph[*graph].Use)
    return std::unexpected(xpp::Error{std::string(command),xpp::format("{}: window {} is not an open plot window",command,*graph+1),xpp::command_place()});
  return *graph;
}

/* a graph is a target of the scopes all and plot; naming one for any other
   scope is a mistake */
xpp::Result<> graph_fits_scope(std::optional<int> graph, std::string_view scope, std::string_view command)
{
  if(graph&&scope!=set_scope::all&&scope!=option_group::plot)
    return std::unexpected(xpp::Error{std::string(command),xpp::format("{}: a graph is a target of scope all or plot, not {}",command,scope),xpp::command_place()});
  return {};
}

/* the plot settings put in graph `graph`, which the front end then shows again */
void put_plot_settings(xpp::Session &s, int graph)
{
  xpp::reset_graph(*s.plot_settings,s.plot_windows->graph[static_cast<std::size_t>(graph)]);
  ui.graph_settings_changed(s,graph);
}

/* the target graph's settings in the plot settings the options write; the destructor puts the settings of the graph that is
   active (the window the user works in, which stays) in the plot settings again */
class GraphTarget {
public:
  GraphTarget(xpp::Session &s, int graph, bool used) : s_(s), graph_(graph), previous_(s.plot_windows->active), used_(used)
  {
    if(used_)xpp::get_graph(*s_.plot_settings,s_.plot_windows->graph[static_cast<std::size_t>(graph_)]);
  }
  /* the plot settings put in the graph */
  void store() const { if(used_)put_plot_settings(s_,graph_); }
  ~GraphTarget()
  {
    if(!used_||graph_==previous_)return;
    xpp::get_graph(*s_.plot_settings,s_.plot_windows->graph[static_cast<std::size_t>(previous_)]);
  }
  GraphTarget(const GraphTarget &)=delete;
  GraphTarget &operator=(const GraphTarget &)=delete;
private:
  xpp::Session &s_;
  int graph_, previous_;
  bool used_;
};

/* what the session shows once values of scope changed */
void values_changed(xpp::Session &s, std::string_view scope)
{
  if(scope==set_scope::all||scope==set_scope::parameters)redraw_params();
  if(scope==set_scope::all||scope==set_scope::initial)redraw_ics();
  if(scope!=set_scope::parameters&&scope!=set_scope::initial)xpp::chk_delay(s);
}

/* the model's own option lines over what restoring the rows left, in the
   file's order: only the lines of those rows */
void replay_model_options(xpp::Session &s, std::span<const OptionRow *const> restored)
{
  for(const xpp::Model::OptionLine &line : s.model().options)
    for(const auto &[name,value] : line.items){
      int index=0;
      const OptionRow *row=find_option(name,index,true);
      if(row&&std::ranges::find(restored,row)!=restored.end())set_option(s,name,value,1,NULL);
    }
}

/* the values a Session starts with, for the option rows that have a member
   and no reset: their built-in value */
class BuiltIns {
public:
  BuiltIns() : model_(std::make_unique<xpp::Model>()), session_(std::make_unique<xpp::Session>(*model_)) {}
  const xpp::Session &session() const { return *session_; }
private:
  std::unique_ptr<xpp::Model> model_;
  std::unique_ptr<xpp::Session> session_;
};

/* the row back to its built-in value */
void restore_row(xpp::Session &s, const OptionRow &row, const BuiltIns &built_in)
{
  if(row.reset)row.reset(s);
  else if(row.real)row.real(s)=row.real(built_in.session());
  else if(row.whole)row.whole(s)=row.whole(built_in.session());
  else if(row.boolean)row.boolean(s)=row.boolean(built_in.session());
  else if(row.index)row.index.write(s,row.index.read(built_in.session()));
  else if(row.text)row.text(s)=row.text(built_in.session());
  apply_option_effect(s,row.effect);
}

/* a row has a built-in value of its own: a reset, or a member */
bool has_built_in(const OptionRow &row) { return row.reset||row.real||row.whole||row.boolean||row.index||row.text; }

/* a row with a member of its own holds a value to read; the others (numbered curves, ...) are set by their text only */
bool has_member(const OptionRow &row) { return row.real||row.whole||row.boolean||row.index||row.text||row.show; }

/* which settings of a group are wanted: a panel's (all that have a member), a set's (what a set holds: no `wide` row, a path left out
   none) or the model's file (all that have a member, as the file names them: a path from the model's folder, one left out none, W680) */
enum class Rows { panel, set, model };

/* the settings of group now, as their text: a row of the group with a member that the session's method and model use; the diagram's
   variable and axes are the open view's (the model's values only start a new one) */
std::vector<std::pair<const OptionRow *,std::string>> group_settings(xpp::Session &s, std::string_view group, Rows wanted)
{
  std::vector<std::pair<const OptionRow *,std::string>> out;
  for(const OptionRow &row : option_rows()){
    if(row.group!=group||(wanted==Rows::set&&!set_holds(row))||(wanted==Rows::model&&row.top_level)||!has_member(row)||!setting_applies(s,row,s.numerics->method))continue;
    std::string text=autox_view_row(row)?autox_view_text(s,row):setting_text(s,row);
    if(row.kind==OptionKind::path&&wanted!=Rows::panel){
      if(text.empty())continue; /* a path left out: none */
      if(wanted==Rows::model)text=relative_to_model(s.model().this_file,text);
    }
    out.emplace_back(&row,std::move(text));
  }
  return out;
}

} // namespace

std::vector<std::string> set_scopes(bool model_wide)
{
  std::vector<std::string> scopes{std::string(set_scope::all),std::string(set_scope::parameters),std::string(set_scope::initial)};
  for(std::string_view group : set_groups)scopes.emplace_back(group);
  if(model_wide)for(std::string_view group : model_groups)scopes.emplace_back(group);
  return scopes;
}

std::string set_scope_problem(std::string_view scope, bool model_wide)
{
  const std::vector<std::string> scopes=set_scopes(model_wide);
  if(std::ranges::find(scopes,scope)!=scopes.end())return "";
  std::string all;
  for(const std::string &name : scopes)all+=(all.empty()?"":", ")+name;
  return xpp::format("{} is not a scope (one of {})",scope,all);
}

std::vector<std::string_view> scopes_held(const xpp::Session &s, std::size_t set)
{
  std::vector<std::string_view> scopes;
  for(const SetItem &item : set_items(s.model().intern_sets[set])){
    const std::string_view scope=item_scope(s,item);
    if(!scope.empty()&&std::ranges::find(scopes,scope)==scopes.end())scopes.push_back(scope);
  }
  return scopes;
}

int intern_set_index(const xpp::Model &m, std::string_view name)
{
  if(const auto *entry=m.record->name(name))
    if(const auto set=entry->set_spellings.find(std::string(name));set!=entry->set_spellings.end())return set->second;
  return -1;
}

xpp::Result<> extract_action(xpp::Session &s, std::string_view ptr, const xpp::Place &where, bool xppaut_names, bool graph_settings)
{
  const std::vector<SetItem> items=set_items(ptr);
  if(xpp::Result<> r=check_set_items(s,items,where,xppaut_names,"internal set");!r)return r;
  if(graph_settings)xpp::get_graph(*s.plot_settings,*s.plot_windows->current);
  apply_set_items(s,items);
  return {};
}

Result<> do_txt_action(Session &s, std::string_view action)
{
  if(auto applied=extract_action(s,action,command_place(),false,true); !applied)return applied;
  ping();
  chk_delay(s);
  redraw_params();
  redraw_ics();
  put_plot_settings(s,s.plot_windows->active);
  return {};
}

xpp::Result<> apply_values(xpp::Session &s, std::span<const SetItem> items, const xpp::Place &where, std::string_view scope,
                           std::optional<int> graph, std::string_view command)
{
  if(const std::string why=set_scope_problem(scope);!why.empty())return std::unexpected(xpp::Error{std::string(command),why,xpp::command_place()});
  if(xpp::Result<> r=graph_fits_scope(graph,scope,command);!r)return r;
  const xpp::Result<int> target=target_graph(s,graph,command);
  if(!target)return std::unexpected(target.error());
  const std::vector<SetItem> chosen=scoped_items(s,items,scope);
  /* all of it or nothing (W125): a bad item is the error, at its line */
  if(xpp::Result<> r=check_set_items(s,chosen,where,false,command);!r)return r;
  xpp::push_value_undo(s);
  {
    const GraphTarget on(s,*target,scope==set_scope::all||scope==option_group::plot);
    apply_set_items(s,chosen);
    on.store();
  }
  values_changed(s,scope);
  return {};
}

xpp::Result<> apply_named_set(xpp::Session &s, std::string_view name, std::string_view scope, std::optional<int> graph)
{
  const int j=intern_set_index(s.model(),name);
  if(j<0)return std::unexpected(xpp::Error{"apply set",xpp::format("the model has no set {}",name),xpp::command_place()});
  const xpp::Model::InternalSet &set=s.model().intern_sets[static_cast<std::size_t>(j)];
  if(xpp::Result<> r=apply_values(s,set_items(set),set.place,scope,graph,"apply set");!r)return r;
  return {};
}

xpp::Result<> restore_defaults(xpp::Session &s, std::string_view scope, std::string_view field, std::optional<int> graph)
{
  const xpp::Place at=xpp::command_place();
  const auto fail=[&at](std::string what){ return std::unexpected(xpp::Error{"restore defaults",std::move(what),at}); };
  if(const std::string why=set_scope_problem(scope);!why.empty())return fail(why);
  if(scope==set_scope::all&&!field.empty())return fail("restoring all takes no field");
  if(xpp::Result<> r=graph_fits_scope(graph,scope,"restore defaults");!r)return r;
  const xpp::Result<int> target=target_graph(s,graph,"restore defaults");
  if(!target)return std::unexpected(target.error());
  const xpp::Model &m=s.model();
  const bool all=scope==set_scope::all,single=!field.empty();
  /* what is restored, found before anything changes */
  std::optional<int> parameter,state;
  std::vector<const OptionRow *> rows;
  if(scope==set_scope::parameters&&single){
    const int i=find_user_name(m,PARAMBOX,converted(field));
    if(i<0)return fail(xpp::format("the model has no parameter {}",field));
    parameter=i;
  }
  else if(scope==set_scope::initial&&single){
    const int i=find_user_name(m,ICBOX,converted(field));
    if(i<0)return fail(xpp::format("the model has no state {}",field));
    state=i;
  }
  else if(single){
    int index=0;
    const OptionRow *row=find_odex_option(scope,field,index);
    if(!row||row->wide)return fail(xpp::format("{} is not a setting of {}",field,scope));
    if(!has_built_in(*row))return fail(xpp::format("{} has no default of its own: restore the {} settings together",field,scope));
    rows.push_back(row);
  }
  else for(const OptionRow &row : option_rows())
    if(has_built_in(row)&&!row.wide&&std::ranges::find(set_groups,row.group)!=set_groups.end()&&(all||row.group==scope))rows.push_back(&row);
  xpp::push_value_undo(s);
  if(all||scope==set_scope::parameters)set_default_params(s,parameter);
  if(all||scope==set_scope::initial)set_default_ics(s,state);
  if(!rows.empty()){
    const BuiltIns built_in;
    const GraphTarget on(s,*target,all||scope==option_group::plot);
    for(const OptionRow *row : rows)restore_row(s,*row,built_in);
    replay_model_options(s,rows);
    xpp::do_meth(s);
    on.store();
  }
  values_changed(s,scope);
  return {};
}

xpp::Result<odex::Values> captured_values(xpp::Session &s, std::string_view scope, std::optional<int> graph, bool last_state, bool model_wide)
{
  const xpp::Place at=xpp::command_place();
  const auto fail=[&at](std::string what){ return std::unexpected(xpp::Error{"save values",std::move(what),at}); };
  if(const std::string why=set_scope_problem(scope,model_wide);!why.empty())return fail(why);
  if(xpp::Result<> r=graph_fits_scope(graph,scope,"save values");!r)return std::unexpected(r.error());
  const xpp::Result<int> target=target_graph(s,graph,"save values");
  if(!target)return std::unexpected(target.error());
  const xpp::Model &m=s.model();
  const bool all=scope==set_scope::all;
  odex::Values values;
  if(all||scope==set_scope::parameters){
    values.parameters=true;
    for(int i=0;i<m.nupar;i++){
      double z=0;
      get_val(s.parser,m.upar_names[i],&z);
      values.parameter_values.emplace_back(declared_spelling(m,m.upar_names[i]),odex::print_number(z));
    }
  }
  if(all||scope==set_scope::initial){
    if(last_state&&s.data_store->rows<=0)return fail("no run has reached a state yet: there is no last state to save");
    values.states=true;
    for(int i=0;i<m.node+m.nmarkov;i++){
      const std::size_t k=static_cast<std::size_t>(i);
      values.state_values.emplace_back(declared_spelling(m,m.uvar_names[i]),
                                       odex::print_number(last_state?s.data_store->current[k]:s.last_ic[k]));
    }
  }
  const GraphTarget on(s,*target,all||scope==option_group::plot);
  for(const std::string_view group : option_groups){
    const bool held=std::ranges::find(set_groups,group)!=set_groups.end(),own=model_wide&&std::ranges::find(model_groups,group)!=model_groups.end();
    if((!held&&!own)||(!all&&scope!=group))continue;
    values.groups.emplace_back(group);
    for(const auto &[row,text] : group_settings(s,group,model_wide?Rows::model:Rows::set)){
      for(std::string &value : option_values(*row,text))values.settings.push_back({std::string(group),odex_key(*row,0),std::move(value)});
    }
  }
  return values;
}

xpp::Result<std::vector<std::pair<const OptionRow *,std::string>>> option_group_values(xpp::Session &s, std::string_view group, std::optional<int> graph, std::string_view command)
{
  const xpp::Result<int> target=target_graph(s,graph,command);
  if(!target)return std::unexpected(target.error());
  const GraphTarget on(s,*target,group==option_group::plot);
  return group_settings(s,group,Rows::panel);
}

xpp::Result<> apply_option_group(xpp::Session &s, std::string_view group, std::span<const SetItem> items, const xpp::Place &where, std::optional<int> graph, std::string_view command)
{
  if(std::ranges::find(set_groups,group)!=set_groups.end())return apply_values(s,items,where,group,graph,command);
  if(graph)return std::unexpected(xpp::Error{std::string(command),xpp::format("the {} settings belong to no graph",group),xpp::command_place()});
  /* a group of the session as a whole: the same check of every item, then the same write; the diagram's variable and axes go to
     the open view, the rest of its rows as any */
  std::vector<SetItem> own;
  std::vector<std::pair<const OptionRow *,std::string>> view;
  std::vector<const OptionRow *> lists;
  for(const SetItem &item : items){
    int index=0;
    const OptionRow *row=find_option(item.name,index,false);
    /* a sets row's value is the list of names, a line each: one item a name, as a .odex line is, so each is checked as one */
    if(row&&row->names_sets){
      lists.push_back(row);
      if(!item.value.empty())for(const auto name : std::string_view(item.value)|std::views::split('\n'))
        own.push_back({item.name,std::string(name.begin(),name.end()),item.setting,item.at});
      continue;
    }
    if(row&&group!=option_group::diagram)row=nullptr;
    if(row&&autox_view_row(*row))view.emplace_back(row,item.value);
    else own.push_back(item);
  }
  if(xpp::Result<> r=check_set_items(s,own,where,false,command);!r)return r;
  if(xpp::Result<> r=autox_view_apply(s,view,true);!r)return r;
  xpp::push_value_undo(s);
  /* the sets row's list was split into one item a name above: what the session held goes first, even when the list is empty */
  for(const OptionRow *row : lists)row->reset(s);
  apply_set_items(s,own);
  if(!view.empty())return autox_view_apply(s,view,false);
  return {};
}

xpp::Result<> check_intern_sets(xpp::Session &s)
{
  /* an .ode's sets are XPPAUT's flat ones, which only the converter reads */
  if(!xpp::odex::is_odex(s.model().this_file))return {};
  for(const xpp::Model::InternalSet &set : s.model().intern_sets)
    if(xpp::Result<> r=check_set_items(s,set_items(set),set.place,false,"internal set");!r)return r;
  return {};
}

void do_intern_set(xpp::Session &s, std::string_view name1, std::string_view value_text)
{
  const std::string name=converted(name1);
  const std::string value(value_text);
  /* the items were checked: a variable or parameter takes a number,
     all of it (parse_number), anything else is an option */
  double number=0.0;
  const int ic=find_user_name(s.model(),ICBOX,name);
  const int par=ic>-1?-1:find_user_name(s.model(),PARAMBOX,name);
  if(ic>-1||par>-1){
    parse_number(trim_blanks(value),number);
    if(ic>-1)s.last_ic[ic]=number;
    else set_val(s.parser,name,number);
  }
  else set_option(s,name,value,1,NULL);
  xpp::do_meth(s);
}

} // namespace xpp
