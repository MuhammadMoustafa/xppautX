/* The Model builder (form_ode.h, odex.h): a model's statements to the
   xpp::Model, whichever reader made them, the .ode reader (ode_read.cpp)
   or the .odex reader (odex_load.cpp): one route after the reading, so a
   fix in how a statement becomes the Model holds for both formats
   (docs/odex.md question 10). An old-style model (the number of
   equations on its first line) is built here line by line as it is read
   (compiler, build_old_style), with the same pieces, during conversion. */
#include <new>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <functional>
#include <map>
#include <set>

#include "xpp_util.h"
#include "session.h"
#include "form_ode.h"
#include "ode_read.h"
#include "model.h"
#include "model_files.h"
#include "xpp_log.h"

#include "expr.h"
#include "xpp_io.h"
#include "markov.h"
#include "xpp_files.h"
#include "load_eqn.h"
#include <unistd.h>
#include "flags.h"

#include "dae_fun.h"
#include "derived.h"
#include "browse.h"
#include "simplenet.h"
#include "integrate.h"
#include "xpp_ui.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>


#include "xpp_batch.h"
#include "xpp_globals.h"
#include "comline.h"

using xpp::odex::Binding;
using xpp::odex::Expr;
using xpp::odex::Parsed;
using xpp::odex::Statement;

namespace xpp {

#define MAXONLY 1000
#define MAXCOMMENTS 500






int ConvertStyle=0;



namespace {
/* The storage behind the C tables above, which the rest of the core
   reads (and a few write into, so each keeps its old size): each entry
   points into one of these, set with the functions below */


} // namespace

namespace {

/* program i: MAXEXPLEN commands, zeroed */
int *new_program(xpp::Model &m, int i)
{
  std::vector<int> &program=m.programs[i].rpn;
  program.assign(MAXEXPLEN,0);
  return program.data();
}

/* a boundary condition's text becomes string, cut to its buffer */
void put_bc_text(std::vector<char> &text, std::string_view string)
{
  if(string.size()>=text.size()){
    xpp::log(XPP_LOG_WARN, "boundary condition cut to {} characters: {}\n",text.size()-1,string);
    string=string.substr(0,text.size()-1);
  }
  std::fill(text.begin(),text.end(),'\0');
  std::copy(string.begin(),string.end(),text.begin());
}

/* the model's boundary condition i is 0=string (at most 255 bytes of it) */
void set_bc(xpp::Model &m, int i, std::string_view string)
{
  xpp::Model::BoundaryCondition &b=m.bcs[i];
  b.com.rpn.assign(200,0);
  b.string.assign(256,'\0');
  b.name.assign(10,'\0');
  put_bc_text(b.string,string);
  std::string_view name="0=";
  std::copy(name.begin(),name.end(),b.name.begin());
}

} // namespace

/* the Session's boundary condition i is 0=string */
void set_bc_formula(xpp::Session &s, int i, std::string_view string)
{
  put_bc_text(s.bcs[i].string,string);
}

namespace {

/* a table the load could not make fails it: at the table's file and line
   when the error names one, else at the model line that makes the table
   (a file that cannot be read included: the model line names it) */
[[noreturn]] void table_failed(xpp::Error e)
{
  if(e.place.line<=0){
    if(!e.place.file.empty())e.what=xpp::format("{}: {}",e.place.file,e.what);
    e.place=xpp::Load::place();
  }
  model_failed(std::move(e));
}

/* s after a C function wrote into s.data(): cut at its NUL */
void c_resync(std::string &s)
{
  s.resize(strlen(s.c_str()));
}
} // namespace

void refuse_compiled_functions(std::string_view what)
{
  model_failed(xpp::format("{}: compiled functions are not supported", what));
}

void set_ode_name(xpp::Model &m, int i, std::string_view text)
{
  m.formulas[i]=text;
}

namespace {

void welcome()
{
 xpp::log(XPP_LOG_INFO, "\n The commands are: \n");
 xpp::log(XPP_LOG_INFO, " P(arameter) -- declare parameters <name1>=<value1>,<name2>=<value2>,...\n");
 xpp::log(XPP_LOG_INFO, " F(ixed)     -- declare fixed variables\n");
 xpp::log(XPP_LOG_INFO, " V(ariables) -- declare ode variables \n");
 xpp::log(XPP_LOG_INFO, " U(ser)      -- declare user functions <name> <nargs> <formula>\n");
 xpp::log(XPP_LOG_INFO, " C(hange)    -- change option file   <filename>\n");
 xpp::log(XPP_LOG_INFO, " O(de)       -- declare RHS for equations\n");
 xpp::log(XPP_LOG_INFO, " D(one)      -- finished compiling formula\n");
 xpp::log(XPP_LOG_INFO, " H(elp)      -- this menu                 \n");
 xpp::log(XPP_LOG_INFO, " S(ymbols)   -- Valid functions and symbols\n");
 xpp::log(XPP_LOG_INFO, " I(ntegral)  -- rhs for integral eqn\n");
 xpp::log(XPP_LOG_INFO, " K(ernel)    -- declare kernel for integral eqns\n");
 xpp::log(XPP_LOG_INFO, " T(able)     -- lookup table\n");
 xpp::log(XPP_LOG_INFO, " A(ux)       -- name auxiliary variable\n");
 xpp::log(XPP_LOG_INFO, " N(umbers)   --  hidden parameters\n");
 xpp::log(XPP_LOG_INFO, " M(arkov)    --  Markov variables \n");
 xpp::log(XPP_LOG_INFO, " W(iener)    -- Wiener parameter \n");
 xpp::log(XPP_LOG_INFO, "_________________________________________________________________________\n");

}

void show_syms()
{
 xpp::log(XPP_LOG_INFO, "(    ,    )    +    -      *    ^    **    / \n");
 xpp::log(XPP_LOG_INFO, "sin  cos  tan  atan  atan2 acos asin\n");
 xpp::log(XPP_LOG_INFO, "exp  ln   log  log10 tanh  cosh sinh \n");
 xpp::log(XPP_LOG_INFO, "max  min  heav flr   mod   sign sqrt \n");
 xpp::log(XPP_LOG_INFO, "t    pi   ran  \n");
}

/* string with its integral operators int[mu]{form} (or int{form}) made
   the kernel's name, K##n (a kernel added for each) */
void find_ker(xpp::Session &s, std::string &string, int *alt)
{
  std::string newstr,form;
  double mu=0.0;
  bool fflag=false;
  size_t i=0;
  size_t n=string.size();
  char ch;
  *alt=0;
  /* string[n] is its '\0' */
  while(i<n){
    ch=string[i];
    if(ch=='['){
      newstr.resize(newstr.size()>=3?newstr.size()-3:0); /* the "int" */
      std::string num;
      i++;
      while(i<n&&(ch=string[i])!=']'){
	num+=ch;
	i++;
      }
      mu=atof(num.c_str());
      fflag=true;
      *alt=1;
      form.clear();
      i+=2;
      continue;
    }
    if(ch=='{'){
      newstr.resize(newstr.size()>=3?newstr.size()-3:0); /* the "int" */
      fflag=true;
      form.clear();
      *alt=1;
      i++;
      continue;
    }
    if(ch=='}'){
      std::string name=xpp::format("K##{}",s.model().nkernel);
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",mu,name,form);
      if(add_kernel(s,name,mu,form))model_failed();
      newstr+=name;
      mu=0.0;
      form.clear();
      fflag=false;
      i++;
      continue;
    }
    if(fflag)
      form+=ch;
    else
      newstr+=ch;
    i++;
  }
  string=std::move(newstr);
}

/* a load's start: the parser's symbols and the counts from nothing, t
   the first variable */
void begin_model(xpp::Session &s)
{
  xpp::Model &m=s.model();
  init_rpn(s);
  s.parser.build.in_vars=0;
  s.parser.build.naux=0;
  s.parser.build.aux_names.assign(MAXODE,std::string());
  m.node=0;
  s.model().bc_defined=0;
  m.nupar=0;
  m.nwiener=0;
  add_var(s,"t",0.0);
}

/* ---- what a statement or an old-style line does (compiler uses them
   too) ---- */
void add_parameter(xpp::Session &s, const std::string &name, double value)
{
  xpp::Model &m=s.model();
  if(add_con(s,name,value))
    model_failed(xpp::format("{} is a name already, or one parameter too many",name));
  m.default_val[m.nupar]=value;
  m.upar_con[m.nupar]=s.parser.ncon-1; /* add_con's */
  m.upar_names[m.nupar++]=name;
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
}

/* a constant (.ode's number), or with wiener a Wiener parameter */
void add_constant(xpp::Session &s, const std::string &name, double value, bool wiener)
{
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
  if(add_con(s,name,value))
    model_failed(xpp::format("{} is a name already, or one parameter too many",name));
  if(wiener)xpp::add_wiener(s,s.parser.ncon-1);
}

/* XPPAUT's options file (an "option <file>" line, else default.opt) never
   took effect there (docs/xppaut-findings.md 4), so a model that names one
   is refused rather than silently ignored */
[[noreturn]] void refuse_options_file(const std::string &name)
{
  model_failed(xpp::Error{"model",xpp::format("the options file <{}> is not supported: write its settings as @ lines, in the model or in a file it includes (#include opts.inc)",name),xpp::Load::place()});
}

void add_boundary(xpp::Model &m, std::string_view formula)
{
  set_bc(m,m.bc_defined,formula);
  xpp::log(XPP_LOG_DEBUG, "|{}| |{}| \n",m.bcs[m.bc_defined].name.data(),m.bcs[m.bc_defined].string.data());
  m.bc_defined++;
}

void add_flag(xpp::Session &s, const std::string &cond, int sign, const std::vector<FlagEvent> &events)
{
  xpp::log(XPP_LOG_DEBUG, " GLOBAL: sign ={} condition = {} \n",sign,cond);
  if(add_global(s,cond.c_str(),sign,events)){
    xpp::log(XPP_LOG_WARN, "Bad global !! \n");
    model_failed();
  }
}

void add_only(xpp::Model &m, std::string_view s)
{
  if(s.empty())return;
  std::vector<std::string> &only=m.only;
  if(only.size()>=MAXONLY)return;
  only.emplace_back(s);
}

/* One line of an old-style model (a command letter and its arguments),
   built as it is read. 0 at "done". */
int compiler(xpp::Session &s, const std::string &bob, FILE *fptr)
{
  xpp::Model &m=s.model();
  double value,xlo,xhi;
  int narg,done,nn,iflg=0,VFlag=0,nstates,alt,index,sign;
  int len; /* a program's length, from add_expr */
  std::string name,formula,condition;
  /* the fixed variables' names, for a converted file */
  std::vector<std::string> &fixname=s.parser.build.fixname;
  if(fixname.empty())fixname.resize(MAXODE1);
  int nlin,i;
  done=1;
  if(bob[0]=='@'){
    if(add_model_option(m,bob)<0)
      model_failed(); /* the option said why, at this line */
    if(ConvertStyle)
      xpp::print(s.parser.convert.file(),"{}\n",bob.c_str());
    return(done);
  }
  xpp::Tokens tokens(bob);
  std::string command=tokens.text(" ,");
  xpp::to_lower(command.data());
  /* the "name=value"s after the command */
  std::string_view values=tokens.rest();
  switch(command[0])
    {
    case 'd': done=0;
      break;
    case 's': show_syms();
      break;
    case 'h': welcome();
      break;
    case 'x':
      condition=tokens.text("{ ");
      formula=tokens.text("}\n");
      add_intern_set(m,condition,formula);
      break;
    case 'w':  /*  Make a Wiener (heh heh) constants  */
    case 'n':
      xpp::log(XPP_LOG_INFO, "{}", command[0]=='w'?"Wiener constants\n":" Hidden params:\n");
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"{}",command[0]=='w'?"wiener ":"number ");
      for(const OdeItem &item : ode_items(values))
	{
	  if(ConvertStyle){
	    if(command[0]=='w')
	      xpp::print(s.parser.convert.file(),"{}  ",item.name);
	    else
	      xpp::print(s.parser.convert.file(),"{}={:g}  ",item.name,item.value);
	  }
	  add_constant(s,item.name,item.value,command[0]=='w');
	}
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"\n");
      xpp::log(XPP_LOG_DEBUG, "\n");
      break;
    case 'g': { /* global */
      sign=atoi(tokens.text("{ ").c_str());
      condition=tokens.text("{}");
      formula=tokens.text("\n");
      xpp::log(XPP_LOG_DEBUG, " events={} \n",formula);
      std::vector<FlagEvent> events;
      if(split_events(condition.c_str(),formula.c_str(),events)){
	xpp::log(XPP_LOG_WARN, "Bad global !! \n");
	model_failed();
      }
      add_flag(s,condition,sign,events);
      if(ConvertStyle){
	xpp::print(s.parser.convert.file(),"global {} {{{}}} {}\n",sign,condition,formula);
      }
      break;
    }
    case 'p':
      xpp::log(XPP_LOG_INFO, "Parameters:\n");
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"par ");
      for(const OdeItem &item : ode_items(values))
	{
	  add_parameter(s,item.name,item.value);
	  if(ConvertStyle)
	    xpp::print(s.parser.convert.file(),"{}={:g}  ",item.name,item.value);
	}
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"\n");
      xpp::log(XPP_LOG_DEBUG, "\n");
      break;
    case 'c':
      refuse_options_file(tokens.text(" \n"));
    case 'f':iflg=0;
      xpp::log(XPP_LOG_INFO, "\nFixed variables:\n");
      goto vrs;
    case 'm': /* Markov variable  */
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      nstates=atoi(tokens.text(" \n").c_str());
      if(add_var(s,name,value))
	model_failed(xpp::format("{} is a name already, or one variable too many",name));
      m.uvar_names[s.parser.build.in_vars+m.nmarkov]=name;
      s.last_ic[s.parser.build.in_vars+m.nmarkov]=value;
      m.default_ic[s.parser.build.in_vars+m.nmarkov]=value;
      xpp::log(XPP_LOG_INFO, " Markov variable {}={:f} has {} states \n",name,value,nstates);
      xpp::add_markov(s,nstates,name.c_str());
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"{}(0)={:g}\n",name,value);
      break;
    case 'r': /* state table for Markov variables  */
      name=tokens.text("\n");
      nlin=m.nlines();
      index=xpp::old_build_markov(s,fptr,name.c_str());
      set_ode_name(m,s.parser.build.in_vars+index,xpp::format("{{ {} ... }}",m.source[nlin]));
      break;
    case 'v':
      iflg=1;
      xpp::log(XPP_LOG_INFO, "\nVariables:\n");
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"init ");
    vrs:
      if(m.nmarkov>0) {
	xpp::log(XPP_LOG_WARN, " Error at line {:d} \n Must declare Markov variables after fixed and regular variables\n",m.nlines());
	model_failed();
      }
      for(const OdeItem &item : ode_items(values))
	{
	  if((s.parser.build.in_vars>m.neq)||(s.parser.build.in_vars==MAXODE))
	    {
	      model_failed("too many variables");
	    }
	  name=item.name;
	  value=item.value;
	  if(add_var(s,name,value))
	    model_failed(xpp::format("{} is a name already, or one variable too many",name));
	  if(iflg)
	    {
	      m.uvar_names[s.parser.build.in_vars]=name;
	      s.last_ic[s.parser.build.in_vars]=value;
              m.default_ic[s.parser.build.in_vars]=value;
	      s.parser.build.in_vars++;
	      if(ConvertStyle)
		xpp::print(s.parser.convert.file(),"{}={:g}  ",name,value);
	    }
	  else {
	    if(ConvertStyle)
	      fixname[m.fix_var]=name;
	    m.fix_var++;

	  }
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",name);

	}
      xpp::log(XPP_LOG_DEBUG, " \n");
      if(iflg&&ConvertStyle)
	xpp::print(s.parser.convert.file(),"\n");
      break;
    case 'b':
      add_boundary(m,tokens.text("\n"));
      if(ConvertStyle)
	xpp::print(s.parser.convert.file(),"bndry {}\n",m.bcs[s.model().bc_defined-1].string.data());
      break;
    case 'k':
      if(ConvertStyle)
	xpp::log(XPP_LOG_WARN, " Warning  kernel declaration cannot be converted \n");
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      formula=tokens.text("$");
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",value,name,formula);
      if(add_kernel(s,name,value,formula)){
	xpp::log(XPP_LOG_WARN, "ERROR at line {:d}\n",m.nlines());
	model_failed();
      }
      break;
    case 't':
      if(s.ntable>=MAX_TAB)
	{
	  if(s.parser.errout)xpp::log(XPP_LOG_WARN, "too many tables !!\n");
	  model_failed();
	}
      name=tokens.text(" ");
      formula=tokens.text(" \n");
      if(formula[0]=='%') {
	xpp::log(XPP_LOG_INFO, " Function form of table....\n");
	nn=atoi(tokens.text(" ").c_str());
	xlo=atof(tokens.text(" ").c_str());
	xhi=atof(tokens.text(" ").c_str());
	formula=tokens.text("\n");
	xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",
	       name,nn,xlo,xhi,formula);
	add_table_name(s,s.ntable,name);

	if(auto t=add_form_table(s,s.ntable,nn,xlo,xhi,formula);!t)
	  table_failed(t.error());

	if(ConvertStyle)
	  xpp::print(s.parser.convert.file(),"table {} % {} {:g} {:g} {}\n",
		  name,nn,xlo,xhi,formula);
	s.ntable++;
	xpp::log(XPP_LOG_INFO, " NTable = {:d} \n",s.ntable);

      }
      else
	if(formula[0]=='@'){
	  xpp::log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	  formula=tokens.text(" ");
	  xpp::log(XPP_LOG_INFO, " {} = {} \n",name,formula);
	  if(add_2d_table(name,formula))
	    model_failed(); /* add_2d_table said why */
	}
	else
	  {
	    xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",name,formula);
            add_table_name(s,s.ntable,name);
	    if(auto t=add_file_table(s,s.ntable,formula);!t)
	      table_failed(t.error());
	    if(ConvertStyle)
	      xpp::print(s.parser.convert.file(),"table {} {}\n",
		      name,formula);
	    s.ntable++;
	  }
      break;

    case 'u':
      name=tokens.text(" ");
      narg=atoi(tokens.text(" ").c_str());
      formula=tokens.text("$");
      xpp::log(XPP_LOG_INFO, "{} {} :\n",name,narg);
      if(ConvertStyle){
	xpp::print(s.parser.convert.file(),"{}(",name);
	for(i=0;i<narg;i++){
	  xpp::print(s.parser.convert.file(),"arg{}",i+1);
	  if(i<(narg-1))
	    xpp::print(s.parser.convert.file(),",");
	}
	xpp::print(s.parser.convert.file(),")={}",formula);
      }
      if(add_ufun(s,name,formula,narg)){
	xpp::log(XPP_LOG_WARN, "ERROR at line {:d}\n",m.nlines());
	model_failed();
      }

      xpp::log(XPP_LOG_INFO, "user {} = {}\n",name,formula);
      break;
    case 'i': VFlag=1;
      [[fallthrough]];
    case 'o':
      if(m.node>=(m.neq+m.fix_var-m.nmarkov))
	{
	  done=0;
	  break;
	}
      formula=tokens.text("\n");
      new_program(m,m.node);

      if(m.node<s.parser.build.in_vars)
	{
	  set_ode_name(m,m.node,formula);
	  if(ConvertStyle){
	    if(VFlag)
	      xpp::print(s.parser.convert.file(),"volt {}={}\n",m.uvar_names[m.node],formula);
	    else
	      xpp::print(s.parser.convert.file(),"{}'={}\n",m.uvar_names[m.node],formula);
	  }
	  find_ker(s,formula,&alt);

	  m.eq_type[m.node]=VFlag;

	  VFlag=0;
	}
      if(m.node>=s.parser.build.in_vars&&m.node<(s.parser.build.in_vars+m.fix_var))
	{
	  if(ConvertStyle)
	    xpp::print(s.parser.convert.file(),"{}={}\n",fixname[m.node-s.parser.build.in_vars],formula);
	  find_ker(s,formula,&alt);

	}

      if(m.node>=(s.parser.build.in_vars+m.fix_var))
	{
	  i=m.node-(s.parser.build.in_vars+m.fix_var);
	  set_ode_name(m,m.node-m.fix_var+m.nmarkov,formula);
	  if(ConvertStyle){
	    if(i<s.parser.build.naux)
	      xpp::print(s.parser.convert.file(),"aux {}={}\n",s.parser.build.aux_names[i],formula);
	    else
	      xpp::print(s.parser.convert.file(),"aux aux{}={}\n",i+1,formula);
	  }
	}
      xpp::log(XPP_LOG_INFO, "RHS({})={}\n",m.node,formula);
      if(add_expr(s,formula,m.programs[m.node].rpn.data(),&len)){
	xpp::log(XPP_LOG_WARN, "ERROR at line {:d}\n",m.nlines());
	model_failed();
      }
      m.node++;
      break;

    case 'a':   /* name auxiliary variables */
      xpp::log(XPP_LOG_INFO, "Auxiliary variables:\n");
      for(std::optional<std::string_view> tok;(tok=tokens.next(" ,\n"));)
	{
	  std::string aux(*tok);
	  s.parser.build.aux_names[s.parser.build.naux]=aux;
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",s.parser.build.aux_names[s.parser.build.naux]);
	  s.parser.build.naux++;
	};
      xpp::log(XPP_LOG_DEBUG, "\n");
      break;

    default:
      if(ConvertStyle) {
	xpp::print(s.parser.convert.file(),"{} {}\n",command,tokens.text("\n"));
      }
      break;
    }

  return(done);
}

void add_comment(xpp::Model &m, std::string_view line)
{
  std::vector<xpp::Model::Comment> &comments=m.comments;
  if(comments.size()>=MAXCOMMENTS)return;
  xpp::Model::Comment c;
  std::string &text=c.text;
  size_t open=line.find('{');
  if(open==std::string_view::npos){
    text=line.empty()?std::string_view():line.substr(1);
    c.aflag=0;
  }
  else {
    std::string &action=c.action;
    action="$ ";
    size_t j1=open+1;
    for(size_t i=open+1;i<line.size();i++){
      char ch=line[i];
      if(ch==','){
        action+=' ';
        continue;
      }
      if(ch=='}'){
        action+=' ';
        j1=i+1;
        break;
      }
      action+=ch;
    }
    text="* ";
    text+=line.substr(j1);
    c.aflag=1;
  }
 xpp::log(XPP_LOG_DEBUG, "text={} \n",text);
 if(c.aflag==1)
   xpp::log(XPP_LOG_DEBUG, "action={} \n",c.action);
 comments.push_back(std::move(c));
}

/* a load's end, whichever the model's form: the counts checked, the
   boundary conditions filled up, the formulas and names in upper case,
   the primed variables, Markov chains, flags and aux quantities added */
void finish_model(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i;
 if((m.node+m.nmarkov)==0){
   model_failed("Must have at least one equation: probably not an ODE file");
 }
  if(s.model().bc_defined>s.parser.build.in_vars ){
    model_failed("Too many boundary conditions");
  }
  if(s.model().bc_defined<s.parser.build.in_vars ){
    if(s.model().bc_defined>0)xpp::log(XPP_LOG_WARN, "Warning: Too few boundary conditions\n");
    for(i=s.model().bc_defined;i<s.parser.build.in_vars ;i++){
      set_bc(m,i,"0");
    }
  }
  s.numerics.bvp_flag=1;

  if(m.node!=m.neq+m.fix_var-m.nmarkov)
    {
      model_failed("Too many/few equations");
    }
  if(s.parser.build.in_vars>m.neq)
    {
      model_failed("Too many variables");
    }
  m.node=s.parser.build.in_vars;

  std::array<std::string,MAXODE> &uvar_names=m.uvar_names;
  for(i=0; i<s.parser.build.naux; i++)
    uvar_names[i+m.node+m.nmarkov]=s.parser.build.aux_names[i];

  for(i=m.node+m.nmarkov+s.parser.build.naux;i<m.neq;i++)
    {
      uvar_names[i]=xpp::format("AUX{}",i-m.node-m.nmarkov+1);
    }

  for(i=0;i<m.neq;i++)
      {
	xpp::to_upper(uvar_names[i].data());
	std::string formula=m.formulas[i];
	xpp::to_upper(formula.data());
	de_space(formula);
	set_ode_name(m,i,formula);
      }
  /*
     add primed variables                              */
  m.prime_start=m.nvar;
  if(m.nvar<MAXPRIMEVAR){
  add_var(s,"t'",0.0);
  for(i=0;i<m.node ;i++)
    add_var(s,xpp::format("{}'",uvar_names[i]),0.0);
}
  else {
    xpp::log(XPP_LOG_WARN, " Warning: primed variables not added must have < {:d} variables\n",
     MAXPRIMEVAR);
    xpp::log(XPP_LOG_WARN, " Averaging and boundary value problems cannot be done\n");
  }
  if(m.nmarkov>0)
    xpp::compile_all_markov(s);
  if(compile_flags(s)==1){
    model_failed("Error in compiling a flag");
  }
  /*  add auxiliary variables   */
  for(i=m.node+m.nmarkov;i<m.neq;i++)add_var(s,uvar_names[i],0.0);
  m.ncon_start=s.parser.ncon;
  m.nsym_start=s.parser.nsym;
  program.version_major=static_cast<float>(MYSTR1);
  program.version_minor=static_cast<float>(MYSTR2);
  xpp::log(XPP_LOG_INFO, "Used {:d} constants and {:d} symbols \n",s.parser.ncon,s.parser.nsym);
  xpp::log(XPP_LOG_INFO, "XPPAUT {:g}.{:g} Copyright (C) 2002-now  Bard Ermentrout \n",program.version_major,program.version_minor);
}

} // namespace

void strip_saveqn(xpp::Model &m)
{
  for(std::string &line : m.source)
    for(char &c : line)
      if(c<32)
	c=32;
}

int disc(const xpp::Model &m)
{
  if(m.is_a_map==1)return(1);
  std::string_view s=m.this_file;
  /* what follows the first '.' */
  size_t dot=s.find('.');
  std::string_view end=dot==std::string_view::npos?std::string_view():s.substr(dot+1);
  return end=="dis"||end=="dif";
}

void build_old_style(xpp::Session &s, int neq, FILE *fptr, const std::function<bool(std::string &)> &next_line)
{
  begin_model(s);
  s.model().neq=neq;
  xpp::log(XPP_LOG_INFO, "NEQ={:d}\n",neq);
  if(ConvertStyle){
    const std::string &this_file=s.model().this_file;
    std::string filename=this_file.empty()?std::string("convert.ode"):this_file+".new";
    s.parser.convert=xpp::Writer(filename.c_str());
    if(s.parser.convert.file()==NULL){
      xpp::log(XPP_LOG_WARN, "{}; no conversion done\n",xpp::files::open_error("conversion",filename).text());
      ConvertStyle=0;
    }
    xpp::print(s.parser.convert.file(),"# converted {} \n",this_file);
  }
  std::string bob;
  for(int done=1;done&&next_line(bob);)
    done=compiler(s,bob,fptr);
  if(ConvertStyle){
    xpp::print(s.parser.convert.file(),"done\n");
    if(const xpp::Result<> saved=s.parser.convert.commit();!saved)xpp::show_error(saved.error());
  }
  finish_model(s);
}

/* ---- the Model builder: a model's statements to the Model ---- */

namespace {

/* ---- derived quantities (docs/odex.md question 9) ----
   What a formula reads, to tell whether it reads only parameters, consts
   and pure functions: the formula as the expression engine's text (an
   .ode's as written, an .odex's engine_text), its words in upper case, as
   the engine reads them. */
class Purity {
public:
  /* the parameters and consts (their values are set before anything is
     worked out, whatever their order), the derived quantities so far,
     the tables (data), the user functions */
  std::set<std::string> constants,derived,tables;
  std::map<std::string,const Statement *> functions;

  /* text reads only what a quantity worked out when a parameter changes
     may read: the constants, the derived quantities before it (with
     derived), a function's own arguments (args), numbers, the built-in
     functions that are pure, the tables and the pure user functions; never
     t, a variable, a primed name, a Volterra kernel, ran, normal or
     poisson, a network, delay or shift */
  bool reads(const std::string &text, const std::vector<std::string> &args, bool with_derived)
  {
    if(text.find_first_of("'#")!=std::string::npos)return false;
    for(const std::string &w : xpp::words_of(text)){
      const std::string u=xpp::upper_case(w);
      if(std::find(args.begin(),args.end(),u)!=args.end())continue;
      if(constants.count(u)||tables.count(u)||builtin(u))continue;
      if(with_derived&&derived.count(u))continue;
      auto f=functions.find(u);
      if(f!=functions.end()&&function(u,*f->second))continue;
      return false;
    }
    return true;
  }

private:
  static bool builtin(const std::string &u)
  {
    static const std::set<std::string> pure={
      "SIN","COS","TAN","ASIN","ACOS","ATAN","ATAN2","SINH","COSH","TANH","EXP","LN","LOG","LOG10","SQRT",
      "HEAV","SIGN","FLR","ABS","MAX","MIN","BESSELJ","BESSELY","BESSELI","BESSELIS","ERF","ERFC","LGAMMA",
      "MOD","PI","IF","THEN","ELSE","NOT","AND","OR"};
    return pure.count(u)>0;
  }

  /* a user function is pure when its formula reads only its arguments,
     the constants and pure functions (never a derived quantity, which a
     call may read before it is worked out) */
  bool function(const std::string &u, const Statement &f)
  {
    auto known=pure_.find(u);
    if(known!=pure_.end())return known->second;
    pure_[u]=false; /* a function that calls itself is not pure */
    std::vector<std::string> args;
    for(const std::string &a : f.names)args.push_back(xpp::upper_case(a));
    const std::string body=f.expr.kind==Expr::Kind::Text?f.expr.text:xpp::odex::engine_text(f.expr);
    return pure_[u]=reads(body,args,false);
  }

  std::map<std::string,bool> pure_;
};

/* the names a formula (the engine's text) needs to be variables: the
   first argument of delay, shift, ishift and del_shft, and a Volterra
   kernel's variable (#name) */
void needs_variables(const std::string &text, std::set<std::string> &out)
{
  const std::string t=xpp::upper_case(text);
  auto word_at=[&t,&out](size_t i){
    while(i<t.size()&&t[i]==' ')i++;
    size_t b=i;
    while(i<t.size()&&xpp::is_word_char(t[i]))i++;
    if(i>b)out.insert(t.substr(b,i-b));
  };
  for(std::string_view f : {"DELAY(","SHIFT(","DEL_SHFT("})
    for(size_t p=t.find(f);p!=std::string::npos;p=t.find(f,p+1))word_at(p+f.size());
  for(size_t p=t.find('#');p!=std::string::npos;p=t.find('#',p+1))word_at(p+1);
}

/* OdeAsOdex's count (form_ode.h): more than 0 while --convert's check
   builds an .ode as an .odex */
int ode_as_odex=0;

/* ---- the builder ---- */
class Builder {
public:
  Builder(xpp::Session &s, Parsed &p) : s_(s), m_(s.model()), p_(p) {}

  void run()
  {
    xpp::Model &m=m_;
    m.ieee_division=p_.ieee_division;
    ConvertStyle=0;
    begin_model(s_);
    find_derived(p_.derived||ode_as_odex>0);
    for(Statement &s : p_.statements){
      at(s);
      declare(s);
    }
    /* what follows is the whole model's */
    xpp::Load::at(m_.this_file);
    add_names();
    /* A formula table evaluates while it compiles, so every function it
       can call must already have its program (including converter guards). */
    for(Statement &s : p_.statements) if(s.kind==Statement::Kind::Fun){
      at(s);
      compile(s);
    }
    for(Statement &s : p_.statements) if(s.kind!=Statement::Kind::Fun){
      at(s);
      compile(s);
    }
    xpp::Load::at(m_.this_file);
    compile_derived(s_);
    xpp::compile_svars(s_);
    evaluate_derived(s_);
    xpp::log(XPP_LOG_INFO, " All formulas are valid!!\n");
    m.node=nvar_+naux_+nfix_;
    xpp::log(XPP_LOG_INFO, " nvar={:d} naux={:d} nfix={:d} nmark={:d} NEQ={:d} NODE={:d} \n",
	   nvar_,naux_,nfix_,nmark_,m.neq,m.node);
    m.statements=std::move(p_.statements);
    m.statement_files=p_.files;
    finish_model(s_);
  }

private:
  /* the file of a place in the model */
  const std::string &file(const xpp::odex::Pos &pos) const
  {
    return pos.file<static_cast<int>(p_.files.size())?p_.files[pos.file]:m_.this_file;
  }

  /* where a statement's binding is, for an error */
  xpp::Place where(const xpp::odex::Pos &pos) const
  {
    return xpp::Place{file(pos),pos.line,pos.col};
  }

  /* the load is at statement s: a problem there is reported at it */
  void at(const Statement &s) const
  {
    xpp::Load::at(file(s.pos),s.pos.line,s.pos.col);
  }

  /* a declaration of name (as written) at pos, kept for the message of a
     clash: the name, where, and the array it is a member of (""
     when it is not) */
  void note(const std::string &name,const xpp::odex::Pos &pos,const Statement &s)
  {
    origins_[converted(name)].push_back(Origin{s.written.empty()?name:s.written,pos,s.array.written});
  }

  /* the load fails: the name in upper case (as the tables hold it) is
     declared twice. The later declaration is the one the error is at:
     the name as its author wrote it, and, when either is a member of an
     array, the array. Names match without case (.ode's), said when the
     two were written differently. */
  [[noreturn]] void clash(const std::string &upper) const
  {
    auto it=origins_.find(upper);
    if(it==origins_.end()||it->second.size()<2)
      model_failed(xpp::format("Duplicate name {}",it==origins_.end()?upper:it->second.back().written));
    const Origin &now=it->second.back(),&before=it->second[it->second.size()-2];
    xpp::Load::at(file(now.pos),now.pos.line,now.pos.col);
    std::string at=xpp::format("line {}",before.pos.line);
    if(file(before.pos)!=file(now.pos))at+=xpp::format(" of {}",file(before.pos));
    if(now.array.empty()&&before.array.empty()){
      std::string what=xpp::format("Duplicate name {}",now.written);
      if(now.written!=before.written)
	what+=xpp::format(" (names match without case: {} is declared at {})",before.written,at);
      model_failed(what);
    }
    model_failed(xpp::odex::name_clash(now.written,now.array,before.array,at));
  }

  /* e as the text the expression engine compiles: an .ode formula as
     written; an .odex one with the parentheses it needs, in upper case
     where the Model keeps .ode's so (upper) */
  static std::string text(const Expr &e, bool upper=false)
  {
    if(e.kind==Expr::Kind::Text)return e.text;
    std::string t=xpp::odex::engine_text(e);
    if(upper)xpp::to_upper(t.data());
    return t;
  }

  /* every formula of s, as the expression engine's text */
  static std::vector<std::string> formulas(const Statement &s)
  {
    std::vector<std::string> out={text(s.expr,true)};
    for(const Binding &b : s.bindings)out.push_back(text(b.value,true));
    for(const Expr &c : s.cells)out.push_back(text(c,true));
    return out;
  }

  /* The derived quantities (docs/odex.md question 9). With fixed (an
     .odex model), a fixed variable whose formula reads only parameters,
     consts, pure functions and the derived quantities before it is worked
     out only when a parameter changes, as .ode's !name = expr is (a
     slider, an event setting a parameter, each AUTO evaluation): the same
     numbers, worked out less often. One an aux quantity records under its
     name, delay, shift or a Volterra kernel needs as a variable, or a
     network or a vector may read (any name a word of theirs starts:
     p{1-4} reads p1 ... p4) stays one. An .ode's fixed quantities stay
     fixed, as XPPAUT's, and its !name = expr stays what it is;
     parameters_only says whether it reads only those (--convert refuses
     one that does not). */
  void find_derived(bool fixed)
  {
    Purity pure;
    std::set<std::string> keep,prefixes;
    for(const Statement &s : p_.statements){
      switch(s.kind){
      case Statement::Kind::Par:
      case Statement::Kind::Const:
	for(const Binding &b : s.bindings)pure.constants.insert(xpp::upper_case(b.name));
	break;
      case Statement::Kind::Fun: pure.functions[xpp::upper_case(s.name)]=&s; break;
      case Statement::Kind::Table: pure.tables.insert(xpp::upper_case(s.name)); break;
      case Statement::Kind::Aux:
	for(const Binding &b : s.bindings)keep.insert(xpp::upper_case(b.name));
	break;
      case Statement::Kind::Network:
      case Statement::Kind::Vector:
	for(const std::string &w : xpp::words_of(s.text))prefixes.insert(xpp::upper_case(w));
	break;
      default: break;
      }
      for(const std::string &f : formulas(s))needs_variables(f,keep);
    }
    auto kept=[&keep,&prefixes](const std::string &name){
      const std::string u=xpp::upper_case(name);
      if(keep.count(u))return true;
      for(const std::string &p : prefixes)
	if(u.starts_with(p))return true;
      return false;
    };
    const std::vector<std::string> none;
    for(Statement &s : p_.statements){
      if(s.kind==Statement::Kind::Derived){
	s.parameters_only=true;
	for(const Binding &b : s.bindings)
	  s.parameters_only=s.parameters_only&&pure.reads(text(b.value,true),none,true);
	for(const Binding &b : s.bindings)pure.derived.insert(xpp::upper_case(b.name));
      }
      else if(fixed&&s.kind==Statement::Kind::Fixed&&!kept(s.name)&&
	      pure.reads(text(s.expr,true),none,true)){
	Binding b;
	b.name=s.name;
	b.pos=s.name_pos;
	b.value=std::move(s.expr);
	s.expr=Expr();
	s.kind=Statement::Kind::Derived;
	s.bindings.assign(1,std::move(b));
	s.parameters_only=true;
	pure.derived.insert(xpp::upper_case(s.name));
      }
    }
  }

  /* first, the names: every statement's that the formulas compiled
     after them may read, and what does not wait for them */
  void declare(Statement &s)
  {
    xpp::Model &m=m_;
    switch(s.kind){
    case Statement::Kind::Options:
      if(add_model_option(m,s.text)<0){
	model_failed(xpp::format("Error in parsing {}",s.text));
      }
      break;
    case Statement::Kind::Comment: add_comment(m,"\""+s.text); break;
    case Statement::Kind::Only:
      for(const std::string &n : s.names)add_only(m,n);
      break;
    case Statement::Kind::Markov: {
      note(s.name,s.pos,s);
      xpp::add_markov(s_,s.count,s.name.c_str());
      std::vector<std::string> cells;
      for(const Expr &c : s.cells)cells.push_back(text(c));
      xpp::build_markov(s_,cells,s.name.c_str());
      /* a second chain of the same name has no variable of its own */
      std::string name=converted(s.name);
      if(std::find(mnames_.begin(),mnames_.end(),name)==mnames_.end())mnames_.push_back(std::move(name));
      break;
    }
    case Statement::Kind::Par:
      xpp::log(XPP_LOG_INFO, "Parameters:\n");
      for(const Binding &b : s.bindings){
	note(b.name,b.pos.line?b.pos:s.pos,s);
	add_parameter(s_,b.name,b.value.value);
      }
      xpp::log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::Wiener:
    case Statement::Kind::Const:
      xpp::log(XPP_LOG_INFO, "{}", s.kind==Statement::Kind::Wiener?"Wiener constants\n":" Hidden params:\n");
      for(const Binding &b : s.bindings){
	note(b.name,b.pos.line?b.pos:s.pos,s);
	add_constant(s_,b.name,b.value.value,s.kind==Statement::Kind::Wiener);
      }
      xpp::log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::OptionFile: refuse_options_file(s.text);
    case Statement::Kind::Set: add_intern_set(m,s.name,s.text); break;
    case Statement::Kind::Boundary: add_boundary(m,text(s.expr)); break;
    case Statement::Kind::Event: {
      std::vector<FlagEvent> events;
      for(const Binding &b : s.bindings)events.push_back({b.name,text(b.value)});
      add_flag(s_,text(s.expr),s.count,events);
      break;
    }
    case Statement::Kind::Ode:
    case Statement::Kind::Map:
    case Statement::Kind::Volterra: {
      std::string name=converted(s.name);
      note(s.name,s.pos,s);
      if(std::find(vnames_.begin(),vnames_.end(),name)!=vnames_.end())clash(name);
      vnames_.push_back(std::move(name));
      break;
    }
    case Statement::Kind::Vector: add_vectorizer_name(s_,s.name,s.text); break;
    case Statement::Kind::Network:
      add_special_name(s_,s.name,s.text.data());
      c_resync(s.text);
      break;
    case Statement::Kind::Solv:
      xpp::add_svar(s_,s.name.c_str(),text(s.expr).c_str());
      break;
    case Statement::Kind::Aux:
      for(const Binding &b : s.bindings){
	anames_.push_back(converted(b.name));
	xpp::log(XPP_LOG_INFO, "{} = {} \n",anames_.back(),text(b.value));
      }
      break;
    case Statement::Kind::Derived:
      for(const Binding &b : s.bindings){
	note(b.name,b.pos.line?b.pos:s.pos,s);
	if(add_derived(s_,xpp::upper_case(b.name),text(b.value,true))==1)
	  clash(converted(b.name));
      }
      break;
    case Statement::Kind::Fixed: {
      const int k=static_cast<int>(fnames_.size());
      note(s.name,s.pos,s);
      m.fixinfo[k].name=xpp::upper_case(s.name);
      m.fixinfo[k].value=text(s.expr,true);
      fnames_.push_back(converted(s.name));
      xpp::log(XPP_LOG_INFO, "{} = {} \n",fnames_.back(),m.fixinfo[k].value);
      break;
    }
    case Statement::Kind::Table: {
      const std::string name=converted(s.name);
      if(add_table_name(s_,ntab_,name)==1){
	model_failed(xpp::format("{} is a duplicate name",name));
      }
      xpp::log(XPP_LOG_DEBUG, "added name {:d}\n",ntab_);
      ntab_++;
      break;
    }
    case Statement::Kind::Fun: {
      const std::string name=converted(s.name);
      if(add_ufun_name(s_,name,nufun_,static_cast<int>(s.names.size()))==1){
	model_failed(xpp::format("Duplicate name or too many functions for {}",name));
      }
      nufun_++;
      break;
    }
    default: break;
    }
  }

  /* the variables' names, the fixed variables', the Markov variables',
     the aux quantities', the algebraic variables': the indices the
     formulas are compiled at */
  void add_names()
  {
    xpp::Model &m=m_;
    xpp::Session &s=s_;
    const int nvar=static_cast<int>(vnames_.size());
    for(int i=0;i<nvar;i++){
      if(add_var(s_,vnames_[i].c_str(),0.0))clash(vnames_[i]);
      m.uvar_names[i]=vnames_[i];
      s.last_ic[i]=0.0;
      m.default_ic[i]=0.0;
    }
    for(const std::string &f : fnames_)
      if(add_var(s_,f.c_str(),0.0))clash(f);
    for(size_t i=0;i<mnames_.size();i++){
      if(add_var(s_,mnames_[i].c_str(),0.0))clash(mnames_[i]);
      m.uvar_names[i+nvar]=mnames_[i];
      s.last_ic[i+nvar]=0.0;
      m.default_ic[i+nvar]=0.0;
    }
    for(size_t i=0;i<anames_.size();i++)
      s_.parser.build.aux_names[i]=anames_[i];
    xpp::add_svar_names(s);
    s_.parser.build.in_vars=nvar;
    s_.parser.build.naux=static_cast<int>(anames_.size());
    m.neq=nvar+m.nmarkov+s_.parser.build.naux;
    m.fix_var=static_cast<int>(fnames_.size());
    s.ntable=ntab_;
    m.nfun=nufun_;
    ntab_=0;
    nufun_=0;
  }

  /* where name is among the variables (then the Markov variables, their
     index after the variables'): -1 when it is neither */
  int variable(const std::string &name, bool &markov) const
  {
    auto v=std::find(vnames_.begin(),vnames_.end(),name);
    markov=false;
    if(v!=vnames_.end())return static_cast<int>(v-vnames_.begin());
    auto k=std::find(mnames_.begin(),mnames_.end(),name);
    if(k==mnames_.end())return -1;
    markov=true;
    return s_.parser.build.in_vars+static_cast<int>(k-mnames_.begin());
  }

  void initial_value(const Binding &b)
  {
    const std::string name=converted(b.name);
    bool markov;
    const int in=variable(name,markov);
    if(in<0){
      model_failed(xpp::format("In initial value statement no variable {}",name));
    }
    const double z=b.value.value;
    s_.last_ic[in]=z;
    m_.default_ic[in]=z;
    set_val(s_,name.c_str(),z);
    xpp::log(XPP_LOG_INFO, " {} {}(0)={:g}\n",markov?"Markov":"Initial",name,z);
  }

  void history(const Binding &b)
  {
    const std::string name=converted(b.name);
    bool markov;
    const int in=variable(name,markov);
    if(in<0){
      model_failed(xpp::format("In initial value statement no variable {}",name));
    }
    if(!markov)s_.delay_string[in]=text(b.value,true);
  }

  /* then, in order, what reads the names: the formulas compiled */
  void compile(Statement &s)
  {
    xpp::Model &m=m_;
    int alt,len;
    switch(s.kind){
    case Statement::Kind::InitNumbers:
      for(const Binding &b : s.bindings)initial_value(b);
      break;
    case Statement::Kind::Init:
      /* evaluated in order with every parameter set, once the model is
         set up (set_initial_values) */
      for(const Binding &b : s.bindings)
	m.initial_values.push_back({b.name,text(b.value),where(b.value.pos)});
      break;
    case Statement::Kind::History:
      for(const Binding &b : s.bindings)history(b);
      break;
    case Statement::Kind::Ode:
    case Statement::Kind::Map:
    case Statement::Kind::Volterra: {
      std::string rhs=text(s.expr,s.kind!=Statement::Kind::Volterra);
      m.eq_type[nvar_]=s.kind==Statement::Kind::Volterra;
      set_ode_name(m,nvar_,rhs);
      new_program(m,nvar_);
      find_ker(s_,rhs,&alt);
      if(add_expr(s_,rhs,m.programs[nvar_].rpn.data(),&len)){
	model_failed(xpp::format("ERROR compiling {}'",s.name));
      }
      if(s.kind==Statement::Kind::Map){
	xpp::log(XPP_LOG_INFO, "{}(t+1)={}\n",s.name,rhs);
	m.is_a_map=1;
      }
      if(s.kind==Statement::Kind::Volterra)
	xpp::log(XPP_LOG_INFO, "{}(t)={}\n",s.name,rhs);
      if(s.kind==Statement::Kind::Ode)
	xpp::log(XPP_LOG_INFO, "{}:d{}/dt={}\n",nvar_,s.name,rhs);
      nvar_++;
      break;
    }
    case Statement::Kind::Fixed: {
      std::string rhs=text(s.expr,true);
      find_ker(s_,rhs,&alt);
      new_program(m,nfix_+s_.parser.build.in_vars);
      if(add_expr(s_,rhs,m.programs[nfix_+s_.parser.build.in_vars].rpn.data(),&len)!=0){
	model_failed(xpp::format("Error allocating or compiling {}",s.name));
      }
      nfix_++;
      xpp::log(XPP_LOG_INFO, "{}={}\n",s.name,rhs);
      break;
    }
    case Statement::Kind::Dae: {
      const std::string rhs=text(s.expr,true);
      xpp::add_aeqn(s_,rhs.c_str());
      xpp::log(XPP_LOG_INFO, " DAE eqn: {}=0 \n",rhs);
      break;
    }
    case Statement::Kind::Aux:
      for(const Binding &b : s.bindings){
	const std::string rhs=text(b.value);
	const int in1=s_.parser.build.in_vars+m.nmarkov+naux_,in2=s_.parser.build.in_vars+m.fix_var+naux_;
	set_ode_name(m,in1,rhs);
	new_program(m,in2);
	if(add_expr(s_,rhs,m.programs[in2].rpn.data(),&len)){
	  model_failed(xpp::format("ERROR compiling {}",b.name));
	}
	naux_++;
	xpp::log(XPP_LOG_INFO, "{}={}\n",b.name,rhs);
      }
      break;
    case Statement::Kind::Vector: {
      const int ok=add_vectorizer(s_,s.name,s.text.data());
      c_resync(s.text);
      if(ok==0){
	model_failed(xpp::format("Illegal vector {}",s.text));
      }
      break;
    }
    case Statement::Kind::Network: {
      const int ok=add_spec_fun(s_,s.name,s.text.data());
      c_resync(s.text);
      if(ok==0){
	model_failed(xpp::format("Illegal special function {}",s.text));
      }
      break;
    }
    case Statement::Kind::Markov:
      set_ode_name(m,s_.parser.build.in_vars+nmark_,"...many states..");
      nmark_++;
      xpp::log(XPP_LOG_INFO, "{}: ...many states..",s.name);
      break;
    case Statement::Kind::Fun: {
      const std::string rhs=text(s.expr,true);
      /* the arguments as the formula reads them: an .odex formula is in
         upper case, an .ode one as its reader keeps it */
      std::vector<std::string> args=s.names;
      if(s.expr.kind!=Expr::Kind::Text)
	for(std::string &a : args)xpp::to_upper(a.data());
      if(add_ufun_new(s_,nufun_,rhs,args)!=0){
	model_failed(xpp::format("Function {} messed up",s.name));
      }
      nufun_++;
      xpp::log(XPP_LOG_INFO, "{}({}",s.name,s.names.empty()?std::string():s.names[0]);
      for(size_t a=1;a<s.names.size();a++)
	xpp::log(XPP_LOG_INFO, ",{}",s.names[a]);
      xpp::log(XPP_LOG_INFO, ")={}\n",rhs);
      break;
    }
    case Statement::Kind::Table: table(s); break;
    default: break;
    }
  }

  void table(const Statement &s)
  {
    switch(s.table_kind){
    case Statement::TableKind::Formula: {
      const std::string formula=text(s.expr);
      xpp::log(XPP_LOG_INFO, " Function form of table....\n");
      xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",s.name,s.count,s.lo,s.hi,formula);
      if(auto t=add_form_table(s_,ntab_,s.count,s.lo,s.hi,formula);!t){
	table_failed(t.error());
      }
      ntab_++;
      break;
    }
    case Statement::TableKind::TwoD:
      xpp::log(XPP_LOG_INFO, " Two-dimensional array: \n ");
      xpp::log(XPP_LOG_INFO, " {} = {} \n",s.name,s.text);
      if(add_2d_table(s.name,s.text)){
	model_failed(); /* add_2d_table said why */
      }
      break;
    case Statement::TableKind::File:
      xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",s.name,s.text);
      if(auto t=add_file_table(s_,ntab_,s.text);!t){
	table_failed(t.error());
      }
      ntab_++;
      break;
    }
  }

  xpp::Session &s_;
  xpp::Model &m_;
  Parsed &p_;
  /* the names of the variables, the Markov variables, the fixed
     variables and the aux quantities (converted: blanks removed, upper
     case), in order */
  std::vector<std::string> vnames_,mnames_,fnames_,anames_;
  /* every name declared so far, by its upper case form, in order */
  struct Origin {
    std::string written;
    xpp::odex::Pos pos;
    std::string array;
  };
  std::map<std::string,std::vector<Origin>> origins_;
  /* how many of each the compiling has met */
  int nvar_=0,nfix_=0,naux_=0,nmark_=0,ntab_=0,nufun_=0;
};

} // namespace

void build_model(xpp::Session &s, Parsed p)
{
  Builder(s,p).run();
}

OdeAsOdex::OdeAsOdex()
{
  ode_as_odex++;
}

OdeAsOdex::~OdeAsOdex()
{
  ode_as_odex--;
}

void set_initial_values(xpp::Session &s)
{
  xpp::Model &m=s.model();
  for(const xpp::Model::InitialValue &init : m.initial_values){
    int ok=0;
    const double z=calculate(s,init.formula,&ok);
    if(!ok){
      xpp::model_failed(xpp::Error{"model",xpp::format("the initial value of {} does not evaluate",init.name),init.where});
    }
    const int i=find_user_name(m,ICBOX,init.name);
    s.last_ic[i]=z;
    m.default_ic[i]=z;
    set_val(s,converted(init.name),z);
  }
}

void create_plot_list(xpp::Session &s)
{
  int k;
  s.plot_list.clear();
  for(const std::string &name : s.model().only){
    find_variable(s,name.c_str(),&k);
    if(k>=0)s.plot_list.push_back(k);
  }
}

} // namespace xpp
