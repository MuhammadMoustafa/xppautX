#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "nullcline.h"
#include "colormap.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "markov.h"
#include "expr.h"
#include "xpp_io.h"

#include "xpp_files.h"
#include "xpp_zip.h"
#include "model_files.h"
#include "load_eqn.h"
#include "value_undo.h"
#include "form_ode.h"
#include "numerics_settings.h"
#include "odex.h"

#include "browse.h"
#include "numerics.h"
#include "integrate.h"
#include "adj2.h"
#include "arrayplot.h"
#include "graphics.h"

#include "userbut.h"
#include "volterra2.h"
#include "storage.h"
#include "histogram.h"
#include "tabular.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include "xpp_batch.h"
#include "xpp_log.h"
#include "graf_par.h"
#include "xpp_globals.h"
#include "delay_handle.h"

namespace xpp {


#define DFNORMAL 1
#define MAXOPT 1000


namespace {

/* line appended to options, unless they already hold MAXOPT */
template <class Line>
void store_option(std::vector<Line> &options, Line line, std::string_view s1)
{
  if(options.size()>=MAXOPT){
   xpp::log(XPP_LOG_WARN, "to many options set {} ignored\n",s1);
    return;
  }
  options.push_back(std::move(line));
}

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





void choose_model_file(xpp::Session &s)
{
 std::string &file=s.model().this_file;
 if(s.got_file&&!xpp::files::is_dir(file))return;
 /* no file, or a folder: the start screen's Open model asks, in that folder
    (xppautx_main.cpp); the dialog is File > Open model's, not a second one */
 if(s.got_file)xpp::files::change_dir(file.c_str());
 const char *start=getenv("XPPSTART");
 if(start&&xpp::files::is_dir(start))xpp::files::change_dir(start);
 model_failed(Error{std::string(xpp::NO_MODEL_WHERE),"no model file was named",Place{xpp::files::working_dir()}});
}

void load_eqn(xpp::Session &s)
{
 const std::string &file=s.model().this_file;
 xpp::Load::at(file);
 if(!xpp::odex::is_odex(file))
   model_failed(Error{"model","the loader reads .odex only; open a .ode to convert it",Place{file}});
 std::string bytes;
 if(!xpp::read_model_file(s.model(),file,bytes))
   model_failed(Error{"model",s.model().saved_in.empty()?"cannot be read":xpp::format("is not saved in {}",s.model().saved_in),Place{file}});
 if(!xpp::is_model_text(bytes))
   model_failed(Error{"model",xpp::zip::is_zip(bytes)?"is not a model: it is a zip file (a session file is a .snapx)":"is not a model: it is a binary file",Place{file}});
 xpp::odex::load(s,file,std::move(bytes));
}

void set_all_vals(xpp::Session &s)
{
 s.delay.stab_flag=DFNORMAL;
 s.data_store.rows=0;
 s.numerics.storflag=false;
 s.numerics.inflag=false;
 /* the parser set it for the boundary conditions it read; no boundary
    value problem has been set up yet (pp_shoot.cpp) */
 s.numerics.bvp_flag=false;
 s.plot_settings.plot_3d=0;
 xpp::set_option_defaults(s);
 /* internal options go here  */
 set_internopts(s,NULL);

 xpp::init_range(s);
 xpp::init_trans(s);
 init_my_aplot(s);
 init_txtview();



/*                           */

 if(s.plot_settings.izplt>s.model().neq)s.plot_settings.izplt=s.model().neq;
 if(s.plot_settings.iyplt>s.model().neq)s.plot_settings.iyplt=s.model().neq;
 if(s.plot_settings.ixplt==0||s.plot_settings.iyplt==0)
   s.plot_settings.timplot=1;
 else 
   s.plot_settings.timplot=0;
 if(s.plot_settings.x_3d[0]>=s.plot_settings.x_3d[1]){
   s.plot_settings.x_3d[0]=-1;
   s.plot_settings.x_3d[1]=1;
 }
if(s.plot_settings.y_3d[0]>=s.plot_settings.y_3d[1]){
   s.plot_settings.y_3d[0]=-1;
   s.plot_settings.y_3d[1]=1;
 }
if(s.plot_settings.z_3d[0]>=s.plot_settings.z_3d[1]){
   s.plot_settings.z_3d[0]=-1;
   s.plot_settings.z_3d[1]=1;
 }
 if(s.plot_settings.my_xlo>=s.plot_settings.my_xhi){
   s.plot_settings.my_xlo=-2.0;
   s.plot_settings.my_xhi=2.0;
 }
if(s.plot_settings.my_ylo>=s.plot_settings.my_yhi){
   s.plot_settings.my_ylo=-2.0;
   s.plot_settings.my_yhi=2.0;
 }
 if(s.plot_settings.axes<5){
   s.plot_settings.x_3d[0]=s.plot_settings.my_xlo;
   s.plot_settings.y_3d[0]=s.plot_settings.my_ylo;
   s.plot_settings.x_3d[1]=s.plot_settings.my_xhi;
   s.plot_settings.y_3d[1]=s.plot_settings.my_yhi;
 } 
 s.data_store.allocate(s.data_store.max_rows,s.model().neq+1);
 if(s.plot_settings.axes>=5)s.plot_settings.plot_3d=1;
 xpp::chk_delay(s); /* check for delay allocation */
 xpp::alloc_h_stuff(s);

 xpp::alloc_v_memory(s);  /* allocate stuff for volterra equations */
 xpp::start_solver(s);
 set_initial_values(s); /* the initial values given as formulas */
 xpp::arr_ic_start(s); /* take care of all predefined array ics */

}

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
  if(sets.size()>=MAX_INTERN_SET){
   xpp::log(XPP_LOG_WARN, " {} not added -- too many must be less than {} \n",
	   name,MAX_INTERN_SET);
    return;
  }
  /* "$ " then does without its braces, commas as spaces */
  std::string bob="$ ";
  for(char c:does){
    if(c=='}'||c=='{')
      continue;
    bob+=c==','?' ':c;
  }
  auto [it,inserted]=m.names.try_emplace(xpp::converted(name));
  if(inserted){
    it->second.kind=xpp::Model::NameKind::Set;
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
  add_set_at(m,name,does,std::move(settings),xpp::Load::place());
}

void replace_intern_sets(xpp::Model &m, const xpp::odex::Parsed &parsed)
{
  for(auto &[key,entry] : m.names){
    entry.set=-1;
    entry.set_spellings.clear();
  }
  std::erase_if(m.names,[](const auto &entry){ return entry.second.kind==xpp::Model::NameKind::Set; });
  m.intern_sets.clear();
  for(const xpp::odex::Statement &st : parsed.statements){
    if(st.kind!=xpp::odex::Statement::Kind::Set)continue;
    std::vector<std::pair<std::string,std::string>> settings;
    for(const xpp::odex::Option &o : st.options)settings.emplace_back(o.name,o.value);
    add_set_at(m,st.name,st.text,std::move(settings),xpp::Place{parsed.files.at(static_cast<std::size_t>(st.pos.file)),st.pos.line,st.pos.col});
  }
}

xpp::Result<> check_model_limits(const xpp::odex::Parsed &parsed)
{
  std::size_t options=0,sets=0;
  for(const xpp::odex::Statement &st : parsed.statements){
    if(st.kind==xpp::odex::Statement::Kind::Options)options+=st.options.size();
    if(st.kind==xpp::odex::Statement::Kind::Set)sets++;
  }
  if(options>MAXOPT)return xpp::fail("save values",xpp::format("a model may set at most {} options",MAXOPT),xpp::Place{parsed.files.front()});
  if(sets>MAX_INTERN_SET)return xpp::fail("save values",xpp::format("a model may have at most {} sets",MAX_INTERN_SET),xpp::Place{parsed.files.front()});
  return {};
}

void replace_model_options(xpp::Model &m, const xpp::odex::Parsed &parsed)
{
  m.options.clear();
  for(const xpp::odex::Statement &st : parsed.statements)
    if(st.kind==xpp::odex::Statement::Kind::Options&&!st.options.empty())add_model_options(m,st.options,parsed.files);
}

std::string intern_set_default_name(const xpp::Model &m)
{
  for(std::size_t n=1;;n++){
    std::string name=xpp::format("set{}",n);
    const auto *entry=m.name(name);
    if(!entry || entry->set<0)return name;
  }
}

namespace {

/* a variable's name as its equation spells it (the Model keeps them in upper case) */
std::string declared_spelling(const xpp::Model &m, const std::string &name)
{
  if(const auto *entry=m.name(name);entry && !entry->written.empty())return entry->written;
  return name;
}

} // namespace

namespace {

/* what a name of a set's entry is in the model: a state, a parameter or neither (a state wins) */
enum class ModelValue { none, state, parameter };

ModelValue model_value_of(const xpp::Model &m, const std::string &name)
{
  const std::string upper=converted(name);
  if(find_user_name(m,ICBOX,upper)>-1)return ModelValue::state;
  if(find_user_name(m,PARAMBOX,upper)>-1)return ModelValue::parameter;
  return ModelValue::none;
}

/* why name=value cannot be applied (an initial condition or a parameter
   whose value is not a number, an option that does not take it), "" when
   it can */
std::string intern_item_problem(xpp::Session &s, const std::string &name1, const std::string &value, bool xppaut_names)
{
  double z=0;
  if(model_value_of(s.model(),name1)!=ModelValue::none)
    return xpp::parse_number(xpp::trim_blanks(value),z)?std::string():"not a number";
  const auto why=option_problem(s,converted(name1),value,xppaut_names);
  return why.value_or("");
}

} // namespace

namespace {

/* one name=value of a set: a model name's (a parameter's or a state's) or a
   setting's, which no model name takes the place of */
struct SetItem {
  std::string name,value;
  bool setting;
};

/* the scope an item belongs to: parameters, initial (a state) or its
   option's group ("" for a name that is none of them: only `all` takes it) */
std::string_view item_scope(const xpp::Session &s, const SetItem &item, bool xppaut_names)
{
  if(!item.setting){
    const ModelValue kind=model_value_of(s.model(),item.name);
    if(kind==ModelValue::state)return set_scope::initial;
    if(kind==ModelValue::parameter)return set_scope::parameters;
  }
  int index=0;
  const OptionRow *row=find_option(xpp::upper_case(item.name),index,xppaut_names);
  return row?row->group:std::string_view();
}

/* the items of scope, in order */
std::vector<SetItem> scoped(const xpp::Session &s, std::span<const SetItem> items, std::string_view scope, bool xppaut_names)
{
  std::vector<SetItem> out;
  for(const SetItem &item : items)
    if(scope==set_scope::all||item_scope(s,item,xppaut_names)==scope)out.push_back(item);
  return out;
}

/* every item checked before one is applied: the first bad one is the error,
   at where, and s is as it was */
xpp::Result<> check_set_items(xpp::Session &s, std::span<const SetItem> items, const xpp::Place &where, bool xppaut_names)
{
  for(const SetItem &item : items){
    const std::string why=item.setting?option_problem(s,item.name,item.value,xppaut_names).value_or(""):
                                       intern_item_problem(s,item.name,item.value,xppaut_names);
    if(!why.empty())return std::unexpected(xpp::Error{"internal set",xpp::format("{}={}: {}",item.name,item.value,why),where});
  }
  return {};
}

/* the checked items applied, in order */
void apply_set_items(xpp::Session &s, std::span<const SetItem> items)
{
  for(const SetItem &item : items){
    if(item.setting){
      set_option(s,item.name,item.value,1,NULL);
      xpp::do_meth(s);
    }
    else do_intern_set(s,item.name,item.value);
  }
}

std::vector<SetItem> set_items(std::string_view text)
{
  std::vector<SetItem> items;
  each_option(text," "," ,;\n",[&items](const std::string &name,const std::string &value){
    items.push_back({name,value,false});
  });
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
  if(!graph)return s.plot_windows.active;
  if(*graph<0||*graph>=static_cast<int>(s.plot_windows.graph.size())||!s.plot_windows.graph[*graph].Use)
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

/* the target graph made the active one, with its settings in the plot
   settings the options write; the destructor puts the graph that was active
   back, with its own settings in the plot settings again */
class GraphTarget {
public:
  GraphTarget(xpp::Session &s, int graph, bool used) : s_(s), previous_(s.plot_windows.active), used_(used)
  {
    if(!used_)return;
    if(graph!=previous_)xpp::make_active(s_,graph,1);
    get_graph(s_);
  }
  /* the plot settings put in the graph */
  void store() const { if(used_)reset_graph(s_); }
  ~GraphTarget()
  {
    if(!used_||s_.plot_windows.active==previous_)return;
    xpp::make_active(s_,previous_,1);
    get_graph(s_);
  }
  GraphTarget(const GraphTarget &)=delete;
  GraphTarget &operator=(const GraphTarget &)=delete;
private:
  xpp::Session &s_;
  int previous_;
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
      const OptionRow *row=find_option(xpp::upper_case(name),index,true);
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
  else if(row.color)row.color(s)=row.color(built_in.session());
  else if(row.text)row.text(s)=row.text(built_in.session());
  if(row.applied)row.applied(s);
}

/* a row has a built-in value of its own: a reset, or a member */
bool has_built_in(const OptionRow &row) { return row.reset||row.real||row.whole||row.boolean||row.index||row.color||row.text; }

} // namespace

std::vector<std::string> set_scopes()
{
  std::vector<std::string> scopes{std::string(set_scope::all),std::string(set_scope::parameters),std::string(set_scope::initial)};
  for(std::string_view group : set_groups)scopes.emplace_back(group);
  return scopes;
}

std::string set_scope_problem(std::string_view scope)
{
  const std::vector<std::string> scopes=set_scopes();
  if(std::ranges::find(scopes,scope)!=scopes.end())return "";
  std::string all;
  for(const std::string &name : scopes)all+=(all.empty()?"":", ")+name;
  return xpp::format("{} is not a scope (one of {})",scope,all);
}

int intern_set_index(const xpp::Model &m, std::string_view name)
{
  if(const auto *entry=m.name(name))
    if(const auto set=entry->set_spellings.find(std::string(name));set!=entry->set_spellings.end())return set->second;
  return -1;
}

xpp::Result<> extract_action(xpp::Session &s, std::string_view ptr, const xpp::Place &where, bool xppaut_names)
{
  const std::vector<SetItem> items=set_items(ptr);
  if(xpp::Result<> r=check_set_items(s,items,where,xppaut_names);!r)return r;
  apply_set_items(s,items);
  return {};
}

xpp::Result<> apply_named_set(xpp::Session &s, std::string_view name, std::string_view scope, std::optional<int> graph)
{
  const xpp::Place at=xpp::command_place();
  if(const std::string why=set_scope_problem(scope);!why.empty())return std::unexpected(xpp::Error{"apply set",why,at});
  const int j=intern_set_index(s.model(),name);
  if(j<0)return std::unexpected(xpp::Error{"apply set",xpp::format("the model has no set {}",name),at});
  if(xpp::Result<> r=graph_fits_scope(graph,scope,"apply set");!r)return r;
  const xpp::Result<int> target=target_graph(s,graph,"apply set");
  if(!target)return std::unexpected(target.error());
  const xpp::Model::InternalSet &set=s.model().intern_sets[static_cast<std::size_t>(j)];
  const std::vector<SetItem> items=scoped(s,set_items(set),scope,false);
  /* all of it or nothing (W125): a bad item is the error, at the set's line */
  if(xpp::Result<> r=check_set_items(s,items,set.place,false);!r)return r;
  xpp::push_value_undo(s);
  {
    const GraphTarget on(s,*target,scope==set_scope::all||scope==option_group::plot);
    apply_set_items(s,items);
    on.store();
  }
  values_changed(s,scope);
  s.this_internset="_"+set.name;
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
    const OptionRow *row=find_odex_option(field,index);
    if(!row||row->group!=scope||row->wide)return fail(xpp::format("{} is not a setting of {}",field,scope));
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

odex::ValueKind value_kind(const xpp::Model &m, std::string_view name)
{
  switch(model_value_of(m,std::string(name))){
    case ModelValue::state: return odex::ValueKind::state;
    case ModelValue::parameter: return odex::ValueKind::parameter;
    case ModelValue::none: break;
  }
  return odex::ValueKind::other;
}

xpp::Result<odex::Values> captured_values(xpp::Session &s, std::string_view scope, std::optional<int> graph, bool last_state)
{
  const xpp::Place at=xpp::command_place();
  const auto fail=[&at](std::string what){ return std::unexpected(xpp::Error{"save values",std::move(what),at}); };
  if(const std::string why=set_scope_problem(scope);!why.empty())return fail(why);
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
      get_val(s,m.upar_names[i],&z);
      values.parameter_values.emplace_back(declared_spelling(m,m.upar_names[i]),odex::print_number(z));
    }
  }
  if(all||scope==set_scope::initial){
    if(last_state&&s.data_store.rows<=0)return fail("no run has reached a state yet: there is no last state to save");
    values.states=true;
    for(int i=0;i<m.node+m.nmarkov;i++){
      const std::size_t k=static_cast<std::size_t>(i);
      values.state_values.emplace_back(declared_spelling(m,m.uvar_names[i]),
                                       odex::print_number(last_state?s.data_store.current[k]:s.last_ic[k]));
    }
  }
  const GraphTarget on(s,*target,all||scope==option_group::plot);
  for(const std::string_view group : set_groups){
    if(!all&&scope!=group)continue;
    values.groups.emplace_back(group);
    for(const OptionRow &row : option_rows()){
      /* a row with a member of its own holds a value to read; the others (numbered curves, ...) are set by their text only */
      if(row.group!=group||!set_holds(row)||!(row.real||row.whole||row.boolean||row.index||row.color||row.text)||!setting_applies(s,row,s.numerics.method))continue;
      const std::string text=setting_text(s,row);
      if(row.kind==OptionKind::path&&text.empty())continue; /* a path left out: none */
      const bool quoted=option_written_quoted(row,text);
      values.settings.push_back({std::string(group),odex_key(row,0),quoted?odex::quote_string(text):text});
    }
  }
  return values;
}

xpp::Result<> check_intern_sets(xpp::Session &s)
{
  /* an .ode's sets are XPPAUT's flat ones, which only the converter reads */
  if(!xpp::odex::is_odex(s.model().this_file))return {};
  for(const xpp::Model::InternalSet &set : s.model().intern_sets)
    if(xpp::Result<> r=check_set_items(s,set_items(set),set.place,false);!r)return r;
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
    else set_val(s,name,number);
  }
  else set_option(s,name,value,1,NULL);
  xpp::do_meth(s);
}
/*  ODE options stuff  here !!   */

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

void set_internopts(xpp::Session &s, const OptionsSet *mask)
{
  const std::vector<xpp::Model::OptionLine> &options=s.model().options;
  if(s.options_applied>=options.size())return;
  for(;s.options_applied<options.size();s.options_applied++){
    /* a value an option refuses is reported at its line */
    const xpp::Place &at=options[s.options_applied].where;
    const std::string &file=at.file.empty()?s.model().this_file:at.file;
    xpp::Load::at(file,at.line,at.col);
    for(const auto &[name,value] : options[s.options_applied].items)
      set_option(s,name,value,0,mask);
  }
  xpp::Load::at(s.model().this_file);
}

void set_internopts_xpprc_and_comline(xpp::Session &s, std::span<const std::string> interopt)
{
  if(interopt.empty())return;
  /* QUIET and LOGFILE first */
  for(const std::string &opt : interopt){
    xpp::Tokens tok(opt);
    if(!tok.next(" ,"))continue;
    std::string name,value;
    while(std::optional<std::string_view> t=tok.next(" ,\n\r")){
      split_apart(*t,name,value);
      name=xpp::upper_case(name);
      if(name=="QUIET"||name=="LOGFILE")
        set_option(s,name,value,0,NULL);
    }
  }

  /*We make a BOOLEAN MASK using the current OptionsSet*/
  /*This allows options to be overwritten multiple times within .xpprc
  but prevents overwriting across comline, .xpprc etc.
  */
  const OptionsSet mask = s.options_set;
  for(const std::string &opt : interopt)
    each_option(opt," ,"," ,\n\r",[&s,&mask](const std::string &name,const std::string &value){
      set_option(s,name,value,0,&mask);
    });

}

std::vector<std::string> check_for_xpprc()
{
  std::vector<std::string> interopt;
  const char *home=getenv("HOME");
  if(home==NULL)return interopt;
  xpp::LineReader lr((std::string(home)+"/.xpprc").c_str());
  if(!lr)return interopt;
  while(std::optional<std::string_view> line=lr.next()){
    if(!line->empty()&&(*line)[0]=='@')
      store_option(interopt,std::string(*line),*line);
  }
  return interopt;
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
  store_option(m.options,xpp::Model::OptionLine{std::string(s1),option_items(s1,false),xpp::Load::place()},s1);
  return 0;
}

void add_model_options(xpp::Model &m, std::span<const xpp::odex::Option> entries, const std::vector<std::string> &files)
{
  for(const xpp::odex::Option &o : entries){
    if(m.options.size()>=MAXOPT)
      model_failed(xpp::format("A model may set at most {} options",MAXOPT));
    m.options.push_back(xpp::Model::OptionLine{"",{{o.name,o.value}},
      xpp::Place{files.at(static_cast<std::size_t>(o.pos.file)),o.pos.line,o.pos.col}});
  }
}

} // namespace xpp
