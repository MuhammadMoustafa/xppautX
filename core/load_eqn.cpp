#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
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

/* the @ option lines of .xpprc, each whole, until
   set_internopts_xpprc_and_comline applies them (the model's own are
   xpp::Model's options) */
std::vector<std::string> interopt;

/* how many of the model's options set_internopts has applied: each call
   applies those the parser added since the call before (xpp::load_model's,
   after the parse, applies them all; set_all_vals' own finds none new) */
std::size_t options_applied=0;

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
    name = bob.substr(0, k);
    value = bob.substr(k + 1);
  }
}

/* every name=value of an option line (its first token, the @ or $,
   skipped; then tokens split at delims) to set(name, value), those with
   an empty name or value left out; first: the delimiters of the first
   token */
template <class F>
void each_option(std::string_view line, std::string_view first, std::string_view delims, F set)
{
  xpp::Tokens tok(line);
  if (!tok.next(first)) return;
  std::string name, value;
  while (std::optional<std::string_view> t = tok.next(delims)) {
    split_apart(*t, name, value);
    if (!name.empty() && !value.empty()) set(name, value);
  }
}

} // namespace



/*   this file has all of the phaseplane parameters defined   
     and created.  All other files should use external stuff
    to use them. (Except eqn forming stuff)
 */





void load_eqn(xpp::Session &s)
{
 int okay=0;
 int std=0;
 options_applied=0;
 for(int i=0;i<MAXODE;i++)
 {
  s.itor[i]=0;
  s.delay_string[i]="0.0";
 }
 std::string &this_file=s.model().this_file;
 if(this_file=="/dev/stdin")std=1;
 if (s.got_file==1&&(std==0)&&xpp::files::is_dir(this_file.c_str()))
 {
   xpp::files::change_dir(this_file.c_str());
   make_eqn(s);
   return;
 }
 /* the model's file: text (a zip or another binary file is refused, its
    bytes never shown as a parse error's line), and for a saved model its
    saved copy (model_files.h) */
 if(s.got_file==1&&std==0)
 {
   std::string bytes;
   const bool read=xpp::read_model_file(s.model(),this_file,bytes);
   if(read&&!xpp::is_model_text(bytes))
   {
     xpp::log(XPP_LOG_ERROR, "{} is not a model: {}\n",this_file,
              xpp::zip::is_zip(bytes)?"it is a zip file (an AUTO file is a .autox, a session file a .snapx)":"it is a binary file");
     model_failed();
   }
   if(!read&&!s.model().saved_in.empty())
   {
     xpp::log(XPP_LOG_ERROR, "{} is not saved in {}\n",this_file,s.model().saved_in);
     model_failed();
   }
 }
 /* an .odex model: its own reader, then the same builder (odex.h) */
 if(s.got_file==1&&std==0&&xpp::odex::is_odex(this_file))
 {
   okay=xpp::odex::load(s,this_file);
   if(okay==1)return;
 }
 if(s.got_file==1)
 {
   xpp::UniqueFile fptr=std==1?xpp::open_read(this_file.c_str()):xpp::open_model_file(s.model(),this_file);
   if(fptr)
   {
     if(std==1)this_file="console";
     okay=get_eqn(s,fptr.get());
     if(okay==1)return;
   }
 }
 while(okay==0)
 {
   const char *start=getenv("XPPSTART");
   if (start!=NULL && xpp::files::is_dir(start))
     xpp::files::change_dir(start);
   okay=make_eqn(s);
 }
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

  xpp::chk_volterra(s);  

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
 xpp::log_printf(XPP_LOG_INFO, " added %s doing %s \n",
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
std::string intern_item_problem(xpp::Session &s, const std::string &name1, const std::string &value)
{
  const std::string name=converted(name1);
  double z=0;
  if(find_user_name(s.model(),ICBOX,name)>-1||find_user_name(s.model(),PARAMBOX,name)>-1)
    return xpp::parse_number(xpp::trim_blanks(value),z)?std::string():"not a number";
  const char *why=option_problem(s,name,value);
  return why?why:"";
}

} // namespace

xpp::Result<> extract_action(xpp::Session &s, std::string_view ptr, const xpp::Place &where)
{
  /* every item checked before one is applied: a bad one leaves s as it was */
  std::optional<xpp::Error> bad;
  each_option(ptr," "," ,;\n",[&](const std::string &name,const std::string &value){
    if(bad)return;
    if(const std::string why=intern_item_problem(s,name,value);!why.empty())
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
  return extract_action(s,set.does,set.place);
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
  
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set)
{
  std::vector<std::pair<std::string, std::string>> out;
  auto keep=[&out](const std::string &name,const std::string &value){ out.emplace_back(name,value); };
  if(set)each_option(line," "," ,;\n",keep);
  else each_option(line," ,"," ,\n\r",keep);
  return out;
}

void set_internopts(xpp::Session &s, const OptionsSet *mask)
{
  const std::vector<xpp::Model::OptionLine> &options=s.model().options;
  if(options_applied>=options.size())return;
  for(;options_applied<options.size();options_applied++){
    /* a value an option refuses is reported at its line */
    const xpp::Place &at=options[options_applied].where;
    xpp::Load::at(at.file.empty()?s.model().this_file:at.file,at.line,at.col);
    each_option(options[options_applied].text," ,"," ,\n\r",[&s,mask](const std::string &name,const std::string &value){
      set_option(s,name,value,0,mask);
    });
  }
  xpp::Load::at(s.model().this_file);
}

void set_internopts_xpprc_and_comline(xpp::Session &s)
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

  /*
  We leave a fresh start for options specified in the ODE file.
  */
  interopt.clear();
}

void check_for_xpprc()
{
  const char *home=getenv("HOME");
  if(home==NULL)return;
  xpp::LineReader lr((std::string(home)+"/.xpprc").c_str());
  if(!lr)return;
  while(std::optional<std::string_view> line=lr.next()){
    if(!line->empty()&&(*line)[0]=='@')
      stor_internopts(*line);
  }
}

void stor_internopts(std::string_view s1)
{
  store_option(interopt,std::string(s1),s1);
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
  if(refused)return refuse_compiled_functions(refused);
  store_option(m.options,xpp::Model::OptionLine{std::string(s1),xpp::Load::place()},s1);
  return 0;
}

} // namespace xpp
