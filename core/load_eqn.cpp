#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "my_ps.h"
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
#include "form_ode.h"
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
 if(s.got_file)xpp::files::change_dir(file.c_str());
 const char *start=getenv("XPPSTART");
 if(start&&xpp::files::is_dir(start))xpp::files::change_dir(start);
 file=xpp::files::working_dir()+"/";
 if(batch_options.enabled||!file_selector("Open model",file,"*.ode*"))
   model_failed(Error{"open","model open cancelled or no model named",Place{file}});
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
 xpp::odex::load(s,file);
}

void set_all_vals(xpp::Session &s)
{
 s.delay.stab_flag=DFNORMAL;
 s.data_store.rows=0;
 s.numerics.storflag=0;
 s.numerics.inflag=0;
 /* the parser set it for the boundary conditions it read; no boundary
    value problem has been set up yet (pp_shoot.cpp) */
 s.numerics.bvp_flag=0;
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

void add_intern_set(xpp::Model &m, std::string_view name, std::string_view does)
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
  sets.push_back({std::string(name),bob,xpp::Load::place()});
 xpp::log(XPP_LOG_INFO, " added {} doing {} \n",
	 sets.back().name.c_str(),sets.back().does.c_str());
}

std::string intern_set_default_name(const xpp::Model &m)
{
  const std::vector<xpp::Model::InternalSet> &sets=m.intern_sets;
  for(std::size_t n=1;;n++){
    std::string name=xpp::format("set{}",n);
    bool taken=false;
    for(const auto &s : sets)
      if(xpp::equal_ignoring_case(s.name,name))taken=true;
    if(!taken)return name;
  }
}

std::string intern_set_name_problem(const xpp::Model &m, std::string_view name)
{
  if(!xpp::odex::is_name(name)||xpp::odex::is_reserved(name))
    return xpp::format("{} is not a valid name for a set",name);
  for(const auto &set : m.intern_sets)
    if(xpp::equal_ignoring_case(set.name,name))
      return xpp::format("{} is already a set of the model",name);
  return "";
}

std::string intern_set_line(const xpp::Session &s, std::string_view name)
{
  const xpp::Model &m=s.model();
  std::string line=xpp::format("set {} {{",name);
  const char *sep="";
  for(int i=0;i<m.nupar;i++){
    double z=0;
    get_val(s,m.upar_names[i],&z);
    line+=xpp::format("{}{}={}",sep,m.upar_names[i],xpp::number(z));
    sep=",";
  }
  for(int i=0;i<m.node+m.nmarkov;i++){
    line+=xpp::format("{}{}={}",sep,m.uvar_names[i],xpp::number(s.last_ic[i]));
    sep=",";
  }
  return line+"}";
}

namespace {

/* why name=value cannot be applied (an initial condition or a parameter
   whose value is not a number, an option that does not take it), "" when
   it can */
std::string intern_item_problem(xpp::Session &s, const std::string &name1, const std::string &value, bool xppaut_names)
{
  const std::string name=converted(name1);
  double z=0;
  if(find_user_name(s.model(),ICBOX,name)>-1||find_user_name(s.model(),PARAMBOX,name)>-1)
    return xpp::parse_number(xpp::trim_blanks(value),z)?std::string():"not a number";
  const auto why=option_problem(s,name,value,xppaut_names);
  return why.value_or("");
}

} // namespace

xpp::Result<> extract_action(xpp::Session &s, std::string_view ptr, const xpp::Place &where, bool xppaut_names)
{
  /* every item checked before one is applied: a bad one leaves s as it was */
  std::optional<xpp::Error> bad;
  each_option(ptr," "," ,;\n",[&](const std::string &name,const std::string &value){
    if(bad)return;
    if(const std::string why=intern_item_problem(s,name,value,xppaut_names);!why.empty())
      bad=xpp::Error{"internal set",xpp::format("{}={}: {}",name,value,why),where};
  });
  if(bad)return std::unexpected(std::move(*bad));
  each_option(ptr," "," ,;\n",[&s](const std::string &name,const std::string &value){
    do_intern_set(s,name,value);
  });
  return {};
}

xpp::Result<> extract_internset(xpp::Session &s, int j)
{
  const xpp::Model::InternalSet &set=s.model().intern_sets[j];
  return extract_action(s,set.does,set.place,false);
}

void do_intern_set(xpp::Session &s, std::string_view name1, std::string_view value_text)
{
  const std::string name=converted(name1);
  const std::string value(value_text);
  /* extract_action checked it: a variable or parameter takes a number,
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
    /* a .odex value ends at a comma (`@ meth=Mod. Euler`); a .ode's, as
       XPPAUT reads it, at a space too (`@ parmin=-.2 parmax=.5`) */
    const bool odex=xpp::odex::is_odex(file);
    each_option(options[s.options_applied].text," ,",odex?",\n\r":" ,\n\r",[&s,mask](const std::string &name,const std::string &value){
      set_option(s,name,value,0,mask);
    });
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
  store_option(m.options,xpp::Model::OptionLine{std::string(s1),xpp::Load::place()},s1);
  return 0;
}

} // namespace xpp
