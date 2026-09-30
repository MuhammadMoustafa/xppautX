/* The Model builder (form_ode.h, odex.h): a model's statements to the
   xpp::Model, whichever reader made them, the .ode reader (ode_read.cpp)
   or the .odex reader (odex_load.cpp): one route after the reading, so a
   fix in how a statement becomes the Model holds for both formats
   (docs/odex.md question 10). An old-style model (the number of
   equations on its first line) is built here line by line as it is read
   (compiler, build_old_style), with the same pieces. And the file
   selector's choice of a model to load (make_eqn). */
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

#define MAXONLY 1000
#define MAXCOMMENTS 500


int *plotlist;
int N_plist;


/* the boundary conditions' count */
static int BVP_N;

#define cstringmaj MYSTR1
#define cstringmin MYSTR2

int ConvertStyle=0;
FILE *convertf;
static int IN_VARS;



namespace {
/* an old-style file rewritten in the new syntax (ConvertStyle): convertf
   is its FILE *, which markov.cpp's old_build_markov writes into too */
xpp::Writer convert_writer;

/* The storage behind the C tables above, which the rest of the core
   reads (and a few write into, so each keeps its old size): each entry
   points into one of these, set with the functions below */
std::vector<int> plot_columns;                    /* plotlist */

/* the model's named auxiliary variables */
std::array<std::string,MAXODE> aux_names;
int Naux=0;
int is_a_map=0;

} // namespace

namespace {

/* program i: MAXEXPLEN commands, zeroed */
int *new_program(xpp::Model &m, int i)
{
  std::vector<int> &program=m.programs[i];
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
  b.com.assign(200,0);
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

/* s after a C function wrote into s.data(): cut at its NUL */
void c_resync(std::string &s)
{
  s.resize(strlen(s.c_str()));
}
} // namespace

int refuse_compiled_functions(const char *what)
{
  xpp::log(XPP_LOG_ERROR, " {}: compiled functions are not supported\n", what);
  return -1;
}

void set_ode_name(xpp::Model &m, int i, std::string_view text)
{
  m.formulas[i]=text;
}

namespace {

void format_list(const std::vector<std::string> &s)
{
 int n=static_cast<int>(s.size());
 int i,ip;
 int ncol;
 int k,j;
 int lmax=0,l=0;
 for(i=0;i<n;i++){
   l=static_cast<int>(s[i].size());
   if(lmax<l)lmax=l;
 }
 ncol=80/(lmax+2);
 if(ncol<1)ncol=1;
 if(ncol>8)ncol=8;
 k=n/ncol;
 j=n-ncol*k;
 /* each name right-aligned in lmax+2 columns */
 for(ip=0;ip<k;ip++){
   for(i=0;i<ncol;i++)
     xpp::log(XPP_LOG_INFO, "{:>{}}",s[ip*ncol+i],lmax+2);
   xpp_log(XPP_LOG_INFO, "\n");
 }
  for(i=0;i<j;i++)
     xpp::log(XPP_LOG_INFO, "{:>{}}",s[k*ncol+i],lmax+2);
  xpp_log(XPP_LOG_INFO, "\n");
}

void list_em(const char *wild)
{
  xpp_files_refresh_cur_dir();
  xpp_log(XPP_LOG_INFO, "%s: \n",xpp_files_cur_dir());
  std::vector<std::string> dirs,files;
  if(!xpp_files_list_matching(wild,xpp_files_cur_dir(),dirs,files))return;
  xpp_log(XPP_LOG_INFO, "DIRECTORIES:\n");
  format_list(dirs);
  xpp_log(XPP_LOG_INFO, "FILES OF TYPE %s:\n",wild);
  format_list(files);
}

/* the next whitespace-separated word typed on stdin (what scanf("%s")
   read, of any length); false at the end of input */
bool read_word(std::string &word)
{
  int c;
  word.clear();
  while((c=getchar())!=EOF&&isspace(c)){}
  if(c==EOF)return false;
  do word+=static_cast<char>(c);
  while((c=getchar())!=EOF&&!isspace(c));
  return true;
}

/* The ODE file to read: asked on the console in batch mode ((r)un,
   (c)d, (l)ist wild), else with the file selector. 0 on cancel. */
int get_a_filename(std::string &filename,std::string &wild)
{
 if(batch_options.enabled)
 {
  std::string string;
  list_em(wild.c_str());
  while(1){
  xpp_log(XPP_LOG_INFO, "(r)un (c)d (l)ist ");
  if(!read_word(string))return 0;
  if(string[0]=='r'){
    xpp_log(XPP_LOG_INFO, "Run file: ");
    if(!read_word(filename))return 0;
    xpp::log(XPP_LOG_INFO, "Loading {}\n ",filename);
    return 1;
  }
  else
    {
      if(string[0]=='l'){
        xpp_log(XPP_LOG_INFO, "List files of type: ");
        if(!read_word(wild))return 0;
        list_em(wild.c_str());
      }
      else
        {
 	 if(string[0]=='c'){
	   xpp_log(XPP_LOG_INFO, "Change to directory: ");
	   if(!read_word(string))return 0;
	   xpp_files_change_dir(string.c_str());
	   list_em(wild.c_str());
	 }
        }
    }
  }
  }
  else
  {
    std::string dir=xpp_files_working_dir();
    if (dir.empty() || dir.back() != '/')
      dir += '/';
    if (file_selector ("Select an ODE file", dir, wild.c_str()) == 0) {
      bye_bye ();
      return 0;
    }
    filename = dir;
    return 1;
  }
  return(0);
}

void clrscr()
{
 if(system("clear")){}
}

int read_eqn(xpp::Session &s)
{
  std::string wild="*.ode",string;
  get_a_filename(string,wild);
  xpp::UniqueFile fptr=xpp::open_model_file(s.model(),string);
  if(!fptr)
   {
    xpp::log(XPP_LOG_WARN, "\n Cannot open {} \n",string);
    return(0);
   }
   s.model().this_file=string;
   clrscr();
   return(get_eqn(s,fptr.get()));
}

void welcome()
{
 xpp_log(XPP_LOG_INFO, "\n The commands are: \n");
 xpp_log(XPP_LOG_INFO, " P(arameter) -- declare parameters <name1>=<value1>,<name2>=<value2>,...\n");
 xpp_log(XPP_LOG_INFO, " F(ixed)     -- declare fixed variables\n");
 xpp_log(XPP_LOG_INFO, " V(ariables) -- declare ode variables \n");
 xpp_log(XPP_LOG_INFO, " U(ser)      -- declare user functions <name> <nargs> <formula>\n");
 xpp_log(XPP_LOG_INFO, " C(hange)    -- change option file   <filename>\n");
 xpp_log(XPP_LOG_INFO, " O(de)       -- declare RHS for equations\n");
 xpp_log(XPP_LOG_INFO, " D(one)      -- finished compiling formula\n");
 xpp_log(XPP_LOG_INFO, " H(elp)      -- this menu                 \n");
 xpp_log(XPP_LOG_INFO, " S(ymbols)   -- Valid functions and symbols\n");
 xpp_log(XPP_LOG_INFO, " I(ntegral)  -- rhs for integral eqn\n");
 xpp_log(XPP_LOG_INFO, " K(ernel)    -- declare kernel for integral eqns\n");
 xpp_log(XPP_LOG_INFO, " T(able)     -- lookup table\n");
 xpp_log(XPP_LOG_INFO, " A(ux)       -- name auxiliary variable\n");
 xpp_log(XPP_LOG_INFO, " N(umbers)   --  hidden parameters\n");
 xpp_log(XPP_LOG_INFO, " M(arkov)    --  Markov variables \n");
 xpp_log(XPP_LOG_INFO, " W(iener)    -- Wiener parameter \n");
 xpp_log(XPP_LOG_INFO, "_________________________________________________________________________\n");

}

void show_syms()
{
 xpp_log(XPP_LOG_INFO, "(    ,    )    +    -      *    ^    **    / \n");
 xpp_log(XPP_LOG_INFO, "sin  cos  tan  atan  atan2 acos asin\n");
 xpp_log(XPP_LOG_INFO, "exp  ln   log  log10 tanh  cosh sinh \n");
 xpp_log(XPP_LOG_INFO, "max  min  heav flr   mod   sign sqrt \n");
 xpp_log(XPP_LOG_INFO, "t    pi   ran  \n");
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
      if(add_kernel(s,name.c_str(),mu,form.c_str()))xpp_model_failed();
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
  IN_VARS=0;
  Naux=0;
  m.node=0;
  BVP_N=0;
  m.nupar=0;
  m.nwiener=0;
  m.options_file="default.opt";
  add_var(s,"t",0.0);
}

/* ---- what a statement or an old-style line does (compiler uses them
   too) ---- */
void add_parameter(xpp::Session &s, const std::string &name, double value)
{
  xpp::Model &m=s.model();
  if(add_con(s,name.c_str(),value)){
    xpp::log(XPP_LOG_ERROR, "{} is a name already, or one parameter too many\n",name);
    xpp_model_failed();
  }
  m.default_val[m.nupar]=value;
  m.upar_names[m.nupar++]=name;
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
}

/* a constant (.ode's number), or with wiener a Wiener parameter */
void add_constant(xpp::Session &s, const std::string &name, double value, bool wiener)
{
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
  if(add_con(s,name.c_str(),value)){
    xpp::log(XPP_LOG_ERROR, "{} is a name already, or one parameter too many\n",name);
    xpp_model_failed();
  }
  if(wiener)add_wiener(s,s.parser.ncon-1);
}

void add_options_file(xpp::Model &m, const std::string &name)
{
  m.options_file=name;
  xpp::log(XPP_LOG_INFO, " Loading new options file:<{}>\n",name);
}

void add_boundary(xpp::Model &m, std::string_view formula)
{
  set_bc(m,BVP_N,formula);
  xpp_log(XPP_LOG_DEBUG, "|%s| |%s| \n",m.bcs[BVP_N].name.data(),m.bcs[BVP_N].string.data());
  BVP_N++;
  m.bc_defined=BVP_N;
}

void add_flag(xpp::Session &s, const std::string &cond, int sign, const std::vector<FlagEvent> &events)
{
  xpp::log(XPP_LOG_DEBUG, " GLOBAL: sign ={} condition = {} \n",sign,cond);
  if(add_global(s,cond.c_str(),sign,events)){
    xpp_log(XPP_LOG_WARN, "Bad global !! \n");
    xpp_model_failed();
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
  static std::array<std::string,MAXODE1> fixname;
  int nlin,i;
  done=1;
  if(bob[0]=='@'){
    if(add_model_option(m,bob.c_str())<0){
      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
      xpp_model_failed();
    }
    if(ConvertStyle)
      xpp::print(convertf,"{}\n",bob.c_str());
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
      add_intern_set(m,condition.c_str(),formula.c_str());
      break;
    case 'w':  /*  Make a Wiener (heh heh) constants  */
    case 'n':
      xpp_log(XPP_LOG_INFO, command[0]=='w'?"Wiener constants\n":" Hidden params:\n");
      if(ConvertStyle)
	xpp::print(convertf,"{}",command[0]=='w'?"wiener ":"number ");
      for(const OdeItem &item : ode_items(values))
	{
	  if(ConvertStyle){
	    if(command[0]=='w')
	      xpp::print(convertf,"{}  ",item.name);
	    else
	      xpp::print(convertf,"{}={:g}  ",item.name,item.value);
	  }
	  add_constant(s,item.name,item.value,command[0]=='w');
	}
      if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'g': { /* global */
      sign=atoi(tokens.text("{ ").c_str());
      condition=tokens.text("{}");
      formula=tokens.text("\n");
      xpp::log(XPP_LOG_DEBUG, " events={} \n",formula);
      std::vector<FlagEvent> events;
      if(split_events(condition.c_str(),formula.c_str(),events)){
	xpp_log(XPP_LOG_WARN, "Bad global !! \n");
	xpp_model_failed();
      }
      add_flag(s,condition,sign,events);
      if(ConvertStyle){
	xpp::print(convertf,"global {} {{{}}} {}\n",sign,condition,formula);
      }
      break;
    }
    case 'p':
      xpp_log(XPP_LOG_INFO, "Parameters:\n");
      if(ConvertStyle)
	xpp::print(convertf,"par ");
      for(const OdeItem &item : ode_items(values))
	{
	  add_parameter(s,item.name,item.value);
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={:g}  ",item.name,item.value);
	}
      if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'c':
      add_options_file(m,tokens.text(" \n"));
      if(ConvertStyle)
	xpp::print(convertf,"option {}\n",m.options_file);
      break;
    case 'f':iflg=0;
      xpp_log(XPP_LOG_INFO, "\nFixed variables:\n");
      goto vrs;
    case 'm': /* Markov variable  */
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      nstates=atoi(tokens.text(" \n").c_str());
      if(add_var(s,name,value)){
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
	xpp_model_failed();
      }
      m.uvar_names[IN_VARS+m.nmarkov]=name;
      s.last_ic[IN_VARS+m.nmarkov]=value;
      m.default_ic[IN_VARS+m.nmarkov]=value;
      xpp::log(XPP_LOG_INFO, " Markov variable {}={:f} has {} states \n",name,value,nstates);
      add_markov(s,nstates,name.c_str());
      if(ConvertStyle)
	xpp::print(convertf,"{}(0)={:g}\n",name,value);
      break;
    case 'r': /* state table for Markov variables  */
      name=tokens.text("\n");
      nlin=m.nlines();
      index=old_build_markov(s,fptr,name.c_str());
      set_ode_name(m,IN_VARS+index,xpp::format("{{ {} ... }}",m.source[nlin]));
      break;
    case 'v':
      iflg=1;
      xpp_log(XPP_LOG_INFO, "\nVariables:\n");
      if(ConvertStyle)
	xpp::print(convertf,"init ");
    vrs:
      if(m.nmarkov>0) {
	xpp_log(XPP_LOG_WARN, " Error at line %d \n Must declare Markov variables after fixed and regular variables\n",m.nlines());
	xpp_model_failed();
      }
      for(const OdeItem &item : ode_items(values))
	{
	  if((IN_VARS>m.neq)||(IN_VARS==MAXODE))
	    {
	      xpp_log(XPP_LOG_ERROR, " too many variables at line %d\n",m.nlines());
	      xpp_model_failed();
	    }
	  name=item.name;
	  value=item.value;
	  if(add_var(s,name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
	    xpp_model_failed();
	  }
	  if(iflg)
	    {
	      m.uvar_names[IN_VARS]=name;
	      s.last_ic[IN_VARS]=value;
              m.default_ic[IN_VARS]=value;
	      IN_VARS++;
	      if(ConvertStyle)
		xpp::print(convertf,"{}={:g}  ",name,value);
	    }
	  else {
	    if(ConvertStyle)
	      fixname[m.fix_var]=name;
	    m.fix_var++;

	  }
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",name);

	}
      xpp_log(XPP_LOG_DEBUG, " \n");
      if(iflg&&ConvertStyle)
	xpp::print(convertf,"\n");
      break;
    case 'b':
      add_boundary(m,tokens.text("\n"));
      if(ConvertStyle)
	xpp::print(convertf,"bndry {}\n",m.bcs[BVP_N-1].string.data());
      break;
    case 'k':
      if(ConvertStyle)
	xpp_log(XPP_LOG_WARN, " Warning  kernel declaration cannot be converted \n");
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      formula=tokens.text("$");
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",value,name,formula);
      if(add_kernel(s,name.c_str(),value,formula.c_str())){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",m.nlines());
	xpp_model_failed();
      }
      break;
    case 't':
      if(s.ntable>=MAX_TAB)
	{
	  if(s.parser.errout)xpp_log(XPP_LOG_WARN, "too many tables !!\n");
	  xpp_model_failed();
	}
      name=tokens.text(" ");
      formula=tokens.text(" \n");
      if(formula[0]=='%') {
	xpp_log(XPP_LOG_INFO, " Function form of table....\n");
	nn=atoi(tokens.text(" ").c_str());
	xlo=atof(tokens.text(" ").c_str());
	xhi=atof(tokens.text(" ").c_str());
	formula=tokens.text("\n");
	xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",
	       name,nn,xlo,xhi,formula);
	add_table_name(s,s.ntable,name.c_str());

	if(auto t=add_form_table(s,s.ntable,nn,xlo,xhi,formula.c_str());!t){
	  xpp::show_error(t.error());
	  xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
	  xpp_model_failed();
	}

	if(ConvertStyle)
	  xpp::print(convertf,"table {} % {} {:g} {:g} {}\n",
		  name,nn,xlo,xhi,formula);
	s.ntable++;
	xpp_log(XPP_LOG_INFO, " NTable = %d \n",s.ntable);

      }
      else
	if(formula[0]=='@'){
	  xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	  formula=tokens.text(" ");
	  xpp::log(XPP_LOG_INFO, " {} = {} \n",name,formula);
	  if(add_2d_table(name.c_str(),formula.c_str())){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
	    xpp_model_failed();
	  }
	}
	else
	  {
	    xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",name,formula);
            add_table_name(s,s.ntable,name.c_str());
	    if(auto t=add_file_table(s,s.ntable,formula.c_str());!t){
	      xpp::show_error(t.error());
	      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m.nlines());
	      xpp_model_failed();
	    }
	    if(ConvertStyle)
	      xpp::print(convertf,"table {} {}\n",
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
	xpp::print(convertf,"{}(",name);
	for(i=0;i<narg;i++){
	  xpp::print(convertf,"arg{}",i+1);
	  if(i<(narg-1))
	    xpp::print(convertf,",");
	}
	xpp::print(convertf,")={}",formula);
      }
      if(add_ufun(s,name.c_str(),formula.c_str(),narg)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",m.nlines());
	xpp_model_failed();
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

      if(m.node<IN_VARS)
	{
	  set_ode_name(m,m.node,formula);
	  if(ConvertStyle){
	    if(VFlag)
	      xpp::print(convertf,"volt {}={}\n",m.uvar_names[m.node],formula);
	    else
	      xpp::print(convertf,"{}'={}\n",m.uvar_names[m.node],formula);
	  }
	  find_ker(s,formula,&alt);

	  m.eq_type[m.node]=VFlag;

	  VFlag=0;
	}
      if(m.node>=IN_VARS&&m.node<(IN_VARS+m.fix_var))
	{
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={}\n",fixname[m.node-IN_VARS],formula);
	  find_ker(s,formula,&alt);

	}

      if(m.node>=(IN_VARS+m.fix_var))
	{
	  i=m.node-(IN_VARS+m.fix_var);
	  set_ode_name(m,m.node-m.fix_var+m.nmarkov,formula);
	  if(ConvertStyle){
	    if(i<Naux)
	      xpp::print(convertf,"aux {}={}\n",aux_names[i],formula);
	    else
	      xpp::print(convertf,"aux aux{}={}\n",i+1,formula);
	  }
	}
      xpp::log(XPP_LOG_INFO, "RHS({})={}\n",m.node,formula);
      if(add_expr(s,formula.c_str(),m.programs[m.node].data(),&len)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",m.nlines());
	xpp_model_failed();
      }
      m.node++;
      break;

    case 'a':   /* name auxiliary variables */
      xpp_log(XPP_LOG_INFO, "Auxiliary variables:\n");
      for(std::optional<std::string_view> tok;(tok=tokens.next(" ,\n"));)
	{
	  std::string aux(*tok);
	  aux_names[Naux]=aux;
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",aux_names[Naux]);
	  Naux++;
	};
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;

    default:
      if(ConvertStyle) {
	xpp::print(convertf,"{} {}\n",command,tokens.text("\n"));
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
   xpp_log(XPP_LOG_ERROR, " Must have at least one equation! \n Probably not an ODE file.\n");
   xpp_model_failed();
 }
  if(BVP_N>IN_VARS ){
    xpp_log(XPP_LOG_ERROR, "Too many boundary conditions\n");
    xpp_model_failed();
  }
  if(BVP_N<IN_VARS ){
    if(BVP_N>0)xpp_log(XPP_LOG_WARN, "Warning: Too few boundary conditions\n");
    for(i=BVP_N;i<IN_VARS ;i++){
      set_bc(m,i,"0");
    }
  }
  s.numerics.bvp_flag=1;

  if(m.node!=m.neq+m.fix_var-m.nmarkov)
    {
      xpp_log(XPP_LOG_ERROR, " Too many/few equations\n");
      xpp_model_failed();
    }
  if(IN_VARS>m.neq)
    {
      xpp_log(XPP_LOG_ERROR, " Too many variables\n");
	xpp_model_failed();
    }
  m.node=IN_VARS;

  std::array<std::string,MAXODE> &uvar_names=m.uvar_names;
  for(i=0; i<Naux; i++)
    uvar_names[i+m.node+m.nmarkov]=aux_names[i];

  for(i=m.node+m.nmarkov+Naux;i<m.neq;i++)
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
    xpp_log(XPP_LOG_WARN, " Warning: primed variables not added must have < %d variables\n",
     MAXPRIMEVAR);
    xpp_log(XPP_LOG_WARN, " Averaging and boundary value problems cannot be done\n");
  }
  if(m.nmarkov>0)
    compile_all_markov(s);
  if(compile_flags(s)==1){
    xpp_log(XPP_LOG_ERROR, " Error in compiling a flag \n");
    xpp_model_failed();
  }
  /*  add auxiliary variables   */
  for(i=m.node+m.nmarkov;i<m.neq;i++)add_var(s,uvar_names[i],0.0);
  m.ncon_start=s.parser.ncon;
  m.nsym_start=s.parser.nsym;
  program.version_major=static_cast<float>(cstringmaj);
  program.version_minor=static_cast<float>(cstringmin);
  xpp_log(XPP_LOG_INFO, "Used %d constants and %d symbols \n",s.parser.ncon,s.parser.nsym);
  xpp_log(XPP_LOG_INFO, "XPPAUT %g.%g Copyright (C) 2002-now  Bard Ermentrout \n",program.version_major,program.version_minor);
}

} // namespace

int make_eqn(xpp::Session &s)
{
   s.model().neq=2;
   s.model().fix_var=0;
   s.model().nmarkov=0;
   return(read_eqn(s));
}

void strip_saveqn(xpp::Model &m)
{
  for(std::string &line : m.source)
    for(char &c : line)
      if(c<32)
	c=32;
}

int disc(std::string_view s)
{
  if(is_a_map==1)return(1);
  /* what follows the first '.' */
  size_t dot=s.find('.');
  std::string_view end=dot==std::string_view::npos?std::string_view():s.substr(dot+1);
  return end=="dis"||end=="dif";
}

void build_old_style(xpp::Session &s, int neq, FILE *fptr, const std::function<bool(std::string &)> &next_line)
{
  begin_model(s);
  s.model().neq=neq;
  xpp_log(XPP_LOG_INFO, "NEQ=%d\n",neq);
  if(ConvertStyle){
    const std::string &this_file=s.model().this_file;
    std::string filename=this_file.empty()?std::string("convert.ode"):this_file+".new";
    convert_writer=xpp::Writer(filename.c_str());
    convertf=convert_writer.file();
    if(convertf==NULL){
      xpp::log(XPP_LOG_WARN, " Cannot open {} - no conversion done \n",filename);
      ConvertStyle=0;
    }
    xpp::print(convertf,"# converted {} \n",this_file);
  }
  std::string bob;
  for(int done=1;done&&next_line(bob);)
    done=compiler(s,bob,fptr);
  if(ConvertStyle){
    xpp::print(convertf,"done\n");
    convert_writer.commit();
    convertf=NULL;
  }
  finish_model(s);
}

/* ---- the Model builder: a model's statements to the Model ---- */

namespace {

using xpp::odex::Binding;
using xpp::odex::Expr;
using xpp::odex::Parsed;
using xpp::odex::Statement;

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
    for(Statement &s : p_.statements){
      at(s);
      compile(s);
    }
    xpp::Load::at(m_.this_file);
    if(compile_derived(s_)==1)
      xpp_model_failed();
    if(compile_svars(s_)==1)
      xpp_model_failed();
    evaluate_derived(s_);
    xpp_log(XPP_LOG_INFO, " All formulas are valid!!\n");
    m.node=nvar_+naux_+nfix_;
    xpp_log(XPP_LOG_INFO, " nvar=%d naux=%d nfix=%d nmark=%d NEQ=%d NODE=%d \n",
	   nvar_,naux_,nfix_,nmark_,m.neq,m.node);
    m.statements=std::move(p_.statements);
    finish_model(s_);
  }

private:
  /* the file of a place in the model */
  const std::string &file(const xpp::odex::Pos &pos) const
  {
    return pos.file<static_cast<int>(p_.files.size())?p_.files[pos.file]:m_.this_file;
  }

  /* where a statement's binding is, for an error */
  xpp::Diagnostic where(const xpp::odex::Pos &pos) const
  {
    return xpp::odex::error_at(file(pos),pos,"");
  }

  /* the load is at statement s: a problem there is reported at it */
  void at(const Statement &s) const
  {
    xpp::Load::at(file(s.pos),s.pos.line,s.pos.col);
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
      if(add_model_option(m,s.text.c_str())<0){
	xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",s.text);
	xpp_model_failed();
      }
      break;
    case Statement::Kind::Comment: add_comment(m,"\""+s.text); break;
    case Statement::Kind::Only:
      for(const std::string &n : s.names)add_only(m,n);
      break;
    case Statement::Kind::Markov: {
      add_markov(s_,s.count,s.name.c_str());
      std::vector<std::string> cells;
      for(const Expr &c : s.cells)cells.push_back(text(c));
      build_markov(s_,cells,s.name.c_str());
      /* a second chain of the same name has no variable of its own */
      std::string name=converted(s.name);
      if(std::find(mnames_.begin(),mnames_.end(),name)==mnames_.end())mnames_.push_back(std::move(name));
      break;
    }
    case Statement::Kind::Par:
      xpp_log(XPP_LOG_INFO, "Parameters:\n");
      for(const Binding &b : s.bindings)add_parameter(s_,b.name,b.value.value);
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::Wiener:
    case Statement::Kind::Const:
      xpp_log(XPP_LOG_INFO, s.kind==Statement::Kind::Wiener?"Wiener constants\n":" Hidden params:\n");
      for(const Binding &b : s.bindings)add_constant(s_,b.name,b.value.value,s.kind==Statement::Kind::Wiener);
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::OptionFile: add_options_file(m,s.text); break;
    case Statement::Kind::Set: add_intern_set(m,s.name.c_str(),s.text.c_str()); break;
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
      if(std::find(vnames_.begin(),vnames_.end(),name)!=vnames_.end()){
	xpp::log(XPP_LOG_ERROR, " {} is a duplicate name \n",name);
	xpp_model_failed();
      }
      vnames_.push_back(std::move(name));
      break;
    }
    case Statement::Kind::Vector: add_vectorizer_name(s_,s.name.c_str(),s.text.c_str()); break;
    case Statement::Kind::Network:
      add_special_name(s_,s.name.c_str(),s.text.data());
      c_resync(s.text);
      break;
    case Statement::Kind::Solv:
      if(add_svar(s_,s.name.c_str(),text(s.expr).c_str())==1)
	xpp_model_failed();
      break;
    case Statement::Kind::Aux:
      for(const Binding &b : s.bindings){
	anames_.push_back(converted(b.name));
	xpp::log(XPP_LOG_INFO, "{} = {} \n",anames_.back(),text(b.value));
      }
      break;
    case Statement::Kind::Derived:
      for(const Binding &b : s.bindings)
	if(add_derived(s_,xpp::upper_case(b.name).c_str(),text(b.value,true).c_str())==1)
	  xpp_model_failed();
      break;
    case Statement::Kind::Fixed: {
      const int k=static_cast<int>(fnames_.size());
      m.fixinfo[k].name=xpp::upper_case(s.name);
      m.fixinfo[k].value=text(s.expr,true);
      fnames_.push_back(converted(s.name));
      xpp::log(XPP_LOG_INFO, "{} = {} \n",fnames_.back(),m.fixinfo[k].value);
      break;
    }
    case Statement::Kind::Table: {
      const std::string name=converted(s.name);
      if(add_table_name(s_,ntab_,name.c_str())==1){
	xpp::log(XPP_LOG_ERROR, " {} is duplicate name \n",name);
	xpp_model_failed();
      }
      xpp_log(XPP_LOG_DEBUG, "added name %d\n",ntab_);
      ntab_++;
      break;
    }
    case Statement::Kind::Fun: {
      const std::string name=converted(s.name);
      if(add_ufun_name(s_,name.c_str(),nufun_,static_cast<int>(s.names.size()))==1){
	xpp::log(XPP_LOG_ERROR, "Duplicate name or too many functions for {} \n",name);
	xpp_model_failed();
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
      if(add_var(s_,vnames_[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",vnames_[i]);
	xpp_model_failed();
      }
      m.uvar_names[i]=vnames_[i];
      s.last_ic[i]=0.0;
      m.default_ic[i]=0.0;
    }
    for(const std::string &f : fnames_)
      if(add_var(s_,f.c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",f);
	xpp_model_failed();
      }
    for(size_t i=0;i<mnames_.size();i++){
      if(add_var(s_,mnames_[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",mnames_[i]);
	xpp_model_failed();
      }
      m.uvar_names[i+nvar]=mnames_[i];
      s.last_ic[i+nvar]=0.0;
      m.default_ic[i+nvar]=0.0;
    }
    for(size_t i=0;i<anames_.size();i++)
      aux_names[i]=anames_[i];
    add_svar_names(s);
    IN_VARS=nvar;
    Naux=static_cast<int>(anames_.size());
    m.neq=nvar+m.nmarkov+Naux;
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
    return IN_VARS+static_cast<int>(k-mnames_.begin());
  }

  void initial_value(const Binding &b)
  {
    const std::string name=converted(b.name);
    bool markov;
    const int in=variable(name,markov);
    if(in<0){
      xpp::log(XPP_LOG_ERROR, "In initial value statement no variable {} \n",name);
      xpp_model_failed();
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
      xpp::log(XPP_LOG_ERROR, "In initial value statement no variable {} \n",name);
      xpp_model_failed();
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
      if(add_expr(s_,rhs.c_str(),m.programs[nvar_].data(),&len)){
	xpp::log(XPP_LOG_ERROR, "ERROR compiling {}' \n",s.name);
	xpp_model_failed();
      }
      if(s.kind==Statement::Kind::Map){
	xpp::log(XPP_LOG_INFO, "{}(t+1)={}\n",s.name,rhs);
	is_a_map=1;
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
      new_program(m,nfix_+IN_VARS);
      if(add_expr(s_,rhs.c_str(),m.programs[nfix_+IN_VARS].data(),&len)!=0){
	xpp::log(XPP_LOG_ERROR, " Error allocating or compiling {}\n",s.name);
	xpp_model_failed();
      }
      nfix_++;
      xpp::log(XPP_LOG_INFO, "{}={}\n",s.name,rhs);
      break;
    }
    case Statement::Kind::Dae: {
      const std::string rhs=text(s.expr,true);
      if(add_aeqn(s_,rhs.c_str())==1)
	xpp_model_failed();
      xpp::log(XPP_LOG_INFO, " DAE eqn: {}=0 \n",rhs);
      break;
    }
    case Statement::Kind::Aux:
      for(const Binding &b : s.bindings){
	const std::string rhs=text(b.value);
	const int in1=IN_VARS+m.nmarkov+naux_,in2=IN_VARS+m.fix_var+naux_;
	set_ode_name(m,in1,rhs);
	new_program(m,in2);
	if(add_expr(s_,rhs.c_str(),m.programs[in2].data(),&len)){
	  xpp::log(XPP_LOG_ERROR, "ERROR compiling {} \n",b.name);
	  xpp_model_failed();
	}
	naux_++;
	xpp::log(XPP_LOG_INFO, "{}={}\n",b.name,rhs);
      }
      break;
    case Statement::Kind::Vector: {
      const int ok=add_vectorizer(s_,s.name.c_str(),s.text.data());
      c_resync(s.text);
      if(ok==0){
	xpp::log(XPP_LOG_ERROR, " Illegal vector  {} \n",s.text);
	xpp_model_failed();
      }
      break;
    }
    case Statement::Kind::Network: {
      const int ok=add_spec_fun(s_,s.name.c_str(),s.text.data());
      c_resync(s.text);
      if(ok==0){
	xpp::log(XPP_LOG_ERROR, " Illegal special function {} \n",s.text);
	xpp_model_failed();
      }
      break;
    }
    case Statement::Kind::Markov:
      set_ode_name(m,IN_VARS+nmark_,"...many states..");
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
      if(add_ufun_new(s_,nufun_,rhs.c_str(),args)!=0){
	xpp::log(XPP_LOG_ERROR, " Function {} messed up \n",s.name);
	xpp_model_failed();
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
      xpp_log(XPP_LOG_INFO, " Function form of table....\n");
      xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",s.name,s.count,s.lo,s.hi,formula);
      if(auto t=add_form_table(s_,ntab_,s.count,s.lo,s.hi,formula.c_str());!t){
	xpp::show_error(t.error());
	xpp::log(XPP_LOG_ERROR, "ERROR computing {}\n",s.name);
	xpp_model_failed();
      }
      ntab_++;
      break;
    }
    case Statement::TableKind::TwoD:
      xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
      xpp::log(XPP_LOG_INFO, " {} = {} \n",s.name,s.text);
      if(add_2d_table(s.name.c_str(),s.text.c_str())){
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",m_.nlines());
	xpp_model_failed();
      }
      break;
    case Statement::TableKind::File:
      xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",s.name,s.text);
      if(auto t=add_file_table(s_,ntab_,s.text.c_str());!t){
	xpp::show_error(t.error());
	xpp::log(XPP_LOG_ERROR, "ERROR computing {}",s.name);
	xpp_model_failed();
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
    const double z=calculate(s,init.formula.c_str(),&ok);
    if(!ok){
      xpp::Diagnostic d=init.where;
      d.cause=xpp::format("the initial value of {} does not evaluate",init.name);
      xpp::log(XPP_LOG_ERROR, "{} {}\n",init.where.text(),d.cause);
      xpp::model_failed(std::move(d));
    }
    const int i=find_user_name(m,ICBOX,init.name);
    s.last_ic[i]=z;
    m.default_ic[i]=z;
    set_val(s,converted(init.name),z);
  }
}

void create_plot_list(const xpp::Session &s)
{
  int k;
  const std::vector<std::string> &only=s.model().only;
  /* a model loaded before may have had its own */
  N_plist=0;
  if(only.empty())return;
  plot_columns.assign(only.size()+1,0);
  plotlist=plot_columns.data();
  for(const std::string &name : only){
    find_variable(s,name.c_str(),&k);
    if(k>=0){
      plotlist[N_plist]=k;
      N_plist++;
    }
  }
}
