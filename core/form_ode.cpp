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

#include "xpp_util.h"
#include "session.h"
#include "form_ode.h"
#include "ode_read.h"
#include "model.h"
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
int *new_program(int i)
{
  std::vector<int> &program=xpp::model().programs[i];
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
void set_bc(int i, std::string_view string)
{
  xpp::Model::BoundaryCondition &b=xpp::model().bcs[i];
  b.com.assign(200,0);
  b.string.assign(256,'\0');
  b.name.assign(10,'\0');
  put_bc_text(b.string,string);
  std::string_view name="0=";
  std::copy(name.begin(),name.end(),b.name.begin());
}

} // namespace

/* the Session's boundary condition i is 0=string */
void set_bc_formula(int i, std::string_view string)
{
  put_bc_text(xpp::session().bcs[i].string,string);
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

void set_ode_name(int i, std::string_view text)
{
  xpp::model().formulas[i]=text;
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

int read_eqn()
{
  std::string wild="*.ode",string;
  get_a_filename(string,wild);
  xpp::UniqueFile fptr=xpp::open_read(string.c_str());
  if(!fptr)
   {
    xpp::log(XPP_LOG_WARN, "\n Cannot open {} \n",string);
    return(0);
   }
   xpp::model().this_file=string;
   clrscr();
   return(get_eqn(fptr.get()));
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
void find_ker(std::string &string, int *alt)
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
      std::string name=xpp::format("K##{}",xpp::model().nkernel);
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",mu,name,form);
      if(add_kernel(name.c_str(),mu,form.c_str()))xpp_model_failed();
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
void begin_model()
{
  init_rpn();
  IN_VARS=0;
  xpp::model().node=0;
  BVP_N=0;
  xpp::model().nupar=0;
  xpp::model().nwiener=0;
  xpp::model().options_file="default.opt";
  add_var("t",0.0);
}

/* ---- what a statement or an old-style line does (compiler uses them
   too) ---- */
void add_parameter(const std::string &name, double value)
{
  if(add_con(name.c_str(),value)){
    xpp::log(XPP_LOG_ERROR, "{} is a name already, or one parameter too many\n",name);
    xpp_model_failed();
  }
  xpp::model().default_val[xpp::model().nupar]=value;
  xpp::model().upar_names[xpp::model().nupar++]=name;
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
}

/* a constant (.ode's number), or with wiener a Wiener parameter */
void add_constant(const std::string &name, double value, bool wiener)
{
  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
  if(add_con(name.c_str(),value)){
    xpp::log(XPP_LOG_ERROR, "{} is a name already, or one parameter too many\n",name);
    xpp_model_failed();
  }
  if(wiener)add_wiener(xpp::session().parser.ncon-1);
}

void add_options_file(const std::string &name)
{
  xpp::model().options_file=name;
  xpp::log(XPP_LOG_INFO, " Loading new options file:<{}>\n",name);
}

void add_boundary(std::string_view formula)
{
  set_bc(BVP_N,formula);
  xpp_log(XPP_LOG_DEBUG, "|%s| |%s| \n",xpp::model().bcs[BVP_N].name.data(),xpp::model().bcs[BVP_N].string.data());
  BVP_N++;
}

void add_flag(const std::string &cond, int sign, const std::vector<FlagEvent> &events)
{
  xpp::log(XPP_LOG_DEBUG, " GLOBAL: sign ={} condition = {} \n",sign,cond);
  if(add_global(cond.c_str(),sign,events)){
    xpp_log(XPP_LOG_WARN, "Bad global !! \n");
    xpp_model_failed();
  }
}

void add_only(std::string_view s)
{
  if(s.empty())return;
  std::vector<std::string> &only=xpp::model().only;
  if(only.size()>=MAXONLY)return;
  only.emplace_back(s);
}

/* One line of an old-style model (a command letter and its arguments),
   built as it is read. 0 at "done". */
int compiler(const std::string &bob, FILE *fptr)
{
  xpp::Session &s=xpp::session();
  double value,xlo,xhi;
  int narg,done,nn,iflg=0,VFlag=0,nstates,alt,index,sign;
  int len; /* a program's length, from add_expr */
  std::string name,formula,condition;
  /* the fixed variables' names, for a converted file */
  static std::array<std::string,MAXODE1> fixname;
  int nlin,i;
  done=1;
  if(bob[0]=='@'){
    if(add_model_option(bob.c_str())<0){
      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
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
      add_intern_set(condition.c_str(),formula.c_str());
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
	  add_constant(item.name,item.value,command[0]=='w');
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
      add_flag(condition,sign,events);
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
	  add_parameter(item.name,item.value);
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={:g}  ",item.name,item.value);
	}
      if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'c':
      add_options_file(tokens.text(" \n"));
      if(ConvertStyle)
	xpp::print(convertf,"option {}\n",xpp::model().options_file);
      break;
    case 'f':iflg=0;
      xpp_log(XPP_LOG_INFO, "\nFixed variables:\n");
      goto vrs;
    case 'm': /* Markov variable  */
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      nstates=atoi(tokens.text(" \n").c_str());
      if(add_var(name,value)){
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
	xpp_model_failed();
      }
      xpp::model().uvar_names[IN_VARS+xpp::model().nmarkov]=name;
      s.last_ic[IN_VARS+xpp::model().nmarkov]=value;
      xpp::model().default_ic[IN_VARS+xpp::model().nmarkov]=value;
      xpp::log(XPP_LOG_INFO, " Markov variable {}={:f} has {} states \n",name,value,nstates);
      add_markov(nstates,name.c_str());
      if(ConvertStyle)
	xpp::print(convertf,"{}(0)={:g}\n",name,value);
      break;
    case 'r': /* state table for Markov variables  */
      name=tokens.text("\n");
      nlin=xpp::model().nlines();
      index=old_build_markov(fptr,name.c_str());
      set_ode_name(IN_VARS+index,xpp::format("{{ {} ... }}",xpp::model().source[nlin]));
      break;
    case 'v':
      iflg=1;
      xpp_log(XPP_LOG_INFO, "\nVariables:\n");
      if(ConvertStyle)
	xpp::print(convertf,"init ");
    vrs:
      if(xpp::model().nmarkov>0) {
	xpp_log(XPP_LOG_WARN, " Error at line %d \n Must declare Markov variables after fixed and regular variables\n",xpp::model().nlines());
	xpp_model_failed();
      }
      for(const OdeItem &item : ode_items(values))
	{
	  if((IN_VARS>xpp::model().neq)||(IN_VARS==MAXODE))
	    {
	      xpp_log(XPP_LOG_ERROR, " too many variables at line %d\n",xpp::model().nlines());
	      xpp_model_failed();
	    }
	  name=item.name;
	  value=item.value;
	  if(add_var(name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
	    xpp_model_failed();
	  }
	  if(iflg)
	    {
	      xpp::model().uvar_names[IN_VARS]=name;
	      s.last_ic[IN_VARS]=value;
              xpp::model().default_ic[IN_VARS]=value;
	      IN_VARS++;
	      if(ConvertStyle)
		xpp::print(convertf,"{}={:g}  ",name,value);
	    }
	  else {
	    if(ConvertStyle)
	      fixname[xpp::model().fix_var]=name;
	    xpp::model().fix_var++;

	  }
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",name);

	}
      xpp_log(XPP_LOG_DEBUG, " \n");
      if(iflg&&ConvertStyle)
	xpp::print(convertf,"\n");
      break;
    case 'b':
      add_boundary(tokens.text("\n"));
      if(ConvertStyle)
	xpp::print(convertf,"bndry {}\n",xpp::model().bcs[BVP_N-1].string.data());
      break;
    case 'k':
      if(ConvertStyle)
	xpp_log(XPP_LOG_WARN, " Warning  kernel declaration cannot be converted \n");
      name=tokens.text(" ");
      value=atof(tokens.text(" ").c_str());
      formula=tokens.text("$");
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",value,name,formula);
      if(add_kernel(name.c_str(),value,formula.c_str())){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",xpp::model().nlines());
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
	add_table_name(s.ntable,name.c_str());

	if(add_form_table(s.ntable,nn,xlo,xhi,formula.c_str())){
	  xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
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
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
	    xpp_model_failed();
	  }
	}
	else
	  {
	    xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",name,formula);
            add_table_name(s.ntable,name.c_str());
	    if(add_file_table(s.ntable,formula.c_str())){
	      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
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
      if(add_ufun(name.c_str(),formula.c_str(),narg)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",xpp::model().nlines());
	xpp_model_failed();
      }

      xpp::log(XPP_LOG_INFO, "user {} = {}\n",name,formula);
      break;
    case 'i': VFlag=1;
      [[fallthrough]];
    case 'o':
      if(xpp::model().node>=(xpp::model().neq+xpp::model().fix_var-xpp::model().nmarkov))
	{
	  done=0;
	  break;
	}
      formula=tokens.text("\n");
      new_program(xpp::model().node);

      if(xpp::model().node<IN_VARS)
	{
	  set_ode_name(xpp::model().node,formula);
	  if(ConvertStyle){
	    if(VFlag)
	      xpp::print(convertf,"volt {}={}\n",xpp::model().uvar_names[xpp::model().node],formula);
	    else
	      xpp::print(convertf,"{}'={}\n",xpp::model().uvar_names[xpp::model().node],formula);
	  }
	  find_ker(formula,&alt);

	  xpp::model().eq_type[xpp::model().node]=VFlag;

	  VFlag=0;
	}
      if(xpp::model().node>=IN_VARS&&xpp::model().node<(IN_VARS+xpp::model().fix_var))
	{
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={}\n",fixname[xpp::model().node-IN_VARS],formula);
	  find_ker(formula,&alt);

	}

      if(xpp::model().node>=(IN_VARS+xpp::model().fix_var))
	{
	  i=xpp::model().node-(IN_VARS+xpp::model().fix_var);
	  set_ode_name(xpp::model().node-xpp::model().fix_var+xpp::model().nmarkov,formula);
	  if(ConvertStyle){
	    if(i<Naux)
	      xpp::print(convertf,"aux {}={}\n",aux_names[i],formula);
	    else
	      xpp::print(convertf,"aux aux{}={}\n",i+1,formula);
	  }
	}
      xpp::log(XPP_LOG_INFO, "RHS({})={}\n",xpp::model().node,formula);
      if(add_expr(formula.c_str(),xpp::model().programs[xpp::model().node].data(),&len)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",xpp::model().nlines());
	xpp_model_failed();
      }
      xpp::model().node++;
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

void add_comment(std::string_view line)
{
  std::vector<xpp::Model::Comment> &comments=xpp::model().comments;
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
void finish_model()
{
  int i;
 if((xpp::model().node+xpp::model().nmarkov)==0){
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
      set_bc(i,"0");
    }
  }
  xpp::session().numerics.bvp_flag=1;

  if(xpp::model().node!=xpp::model().neq+xpp::model().fix_var-xpp::model().nmarkov)
    {
      xpp_log(XPP_LOG_ERROR, " Too many/few equations\n");
      xpp_model_failed();
    }
  if(IN_VARS>xpp::model().neq)
    {
      xpp_log(XPP_LOG_ERROR, " Too many variables\n");
	xpp_model_failed();
    }
  xpp::model().node=IN_VARS;

  std::array<std::string,MAXODE> &uvar_names=xpp::model().uvar_names;
  for(i=0; i<Naux; i++)
    uvar_names[i+xpp::model().node+xpp::model().nmarkov]=aux_names[i];

  for(i=xpp::model().node+xpp::model().nmarkov+Naux;i<xpp::model().neq;i++)
    {
      uvar_names[i]=xpp::format("AUX{}",i-xpp::model().node-xpp::model().nmarkov+1);
    }

  for(i=0;i<xpp::model().neq;i++)
      {
	xpp::to_upper(uvar_names[i].data());
	std::string formula=xpp::model().formulas[i];
	xpp::to_upper(formula.data());
	de_space(formula);
	set_ode_name(i,formula);
      }
  /*
     add primed variables                              */
  xpp::model().prime_start=xpp::model().nvar;
  if(xpp::model().nvar<MAXPRIMEVAR){
  add_var("t'",0.0);
  for(i=0;i<xpp::model().node ;i++)
    add_var(xpp::format("{}'",uvar_names[i]),0.0);
}
  else {
    xpp_log(XPP_LOG_WARN, " Warning: primed variables not added must have < %d variables\n",
     MAXPRIMEVAR);
    xpp_log(XPP_LOG_WARN, " Averaging and boundary value problems cannot be done\n");
  }
  if(xpp::model().nmarkov>0)
    compile_all_markov();
  if(compile_flags()==1){
    xpp_log(XPP_LOG_ERROR, " Error in compiling a flag \n");
    xpp_model_failed();
  }
  /*  add auxiliary variables   */
  for(i=xpp::model().node+xpp::model().nmarkov;i<xpp::model().neq;i++)add_var(uvar_names[i],0.0);
  xpp::model().ncon_start=xpp::session().parser.ncon;
  xpp::model().nsym_start=xpp::session().parser.nsym;
  program.version_major=static_cast<float>(cstringmaj);
  program.version_minor=static_cast<float>(cstringmin);
  xpp_log(XPP_LOG_INFO, "Used %d constants and %d symbols \n",xpp::session().parser.ncon,xpp::session().parser.nsym);
  xpp_log(XPP_LOG_INFO, "XPPAUT %g.%g Copyright (C) 2002-now  Bard Ermentrout \n",program.version_major,program.version_minor);
}

} // namespace

int make_eqn()
{
   xpp::model().neq=2;
   xpp::model().fix_var=0;
   xpp::model().nmarkov=0;
   return(read_eqn());
}

void strip_saveqn()
{
  for(std::string &line : xpp::model().source)
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

void build_old_style(int neq, FILE *fptr, const std::function<bool(std::string &)> &next_line)
{
  begin_model();
  xpp::model().neq=neq;
  xpp_log(XPP_LOG_INFO, "NEQ=%d\n",neq);
  if(ConvertStyle){
    const std::string &this_file=xpp::model().this_file;
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
    done=compiler(bob,fptr);
  if(ConvertStyle){
    xpp::print(convertf,"done\n");
    convert_writer.commit();
    convertf=NULL;
  }
  finish_model();
}

/* ---- the Model builder: a model's statements to the Model ---- */

namespace {

using xpp::odex::Binding;
using xpp::odex::Expr;
using xpp::odex::Parsed;
using xpp::odex::Statement;

/* ---- the builder ---- */
class Builder {
public:
  explicit Builder(Parsed &p) : p_(p) {}

  void run()
  {
    xpp::Model &m=xpp::model();
    m.ieee_division=p_.ieee_division;
    evaluate_parameters();
    ConvertStyle=0;
    begin_model();
    for(Statement &s : p_.statements){
      at(s);
      declare(s);
    }
    /* what follows is the whole model's */
    xpp::Load::at(xpp::model().this_file);
    add_names();
    for(Statement &s : p_.statements){
      at(s);
      compile(s);
    }
    xpp::Load::at(xpp::model().this_file);
    if(compile_derived()==1)
      xpp_model_failed();
    if(compile_svars()==1)
      xpp_model_failed();
    evaluate_derived();
    xpp_log(XPP_LOG_INFO, " All formulas are valid!!\n");
    m.node=nvar_+naux_+nfix_;
    xpp_log(XPP_LOG_INFO, " nvar=%d naux=%d nfix=%d nmark=%d NEQ=%d NODE=%d \n",
	   nvar_,naux_,nfix_,nmark_,m.neq,m.node);
    m.statements=std::move(p_.statements);
    finish_model();
  }

private:
  /* the file of a place in the model */
  const std::string &file(const xpp::odex::Pos &pos) const
  {
    return pos.file<static_cast<int>(p_.files.size())?p_.files[pos.file]:xpp::model().this_file;
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

  /* each parameter's value: a number, or an expression of numbers, pi
     and the parameters before it, evaluated in order, the model's
     divisions its own */
  void evaluate_parameters()
  {
    std::map<std::string,std::string> values;
    for(Statement &s : p_.statements){
      if(s.kind!=Statement::Kind::Par)continue;
      for(Binding &b : s.bindings){
	if(b.value.kind!=Expr::Kind::Number){
	  /* calculate() rolls the symbol table back to the Model's own:
	     here, before the build, the built-ins */
	  xpp::model().ncon_start=0;
	  xpp::model().nsym_start=STDSYM;
	  const std::string formula=xpp::odex::engine_text(b.value,&values);
	  int ok=0;
	  const double z=calculate(formula.c_str(),&ok);
	  if(!ok)throw xpp::odex::error_at(p_.files[b.value.pos.file],b.value.pos,
					    xpp::format("the value of {} does not evaluate",b.name));
	  Expr number;
	  number.kind=Expr::Kind::Number;
	  number.pos=b.value.pos;
	  number.value=z;
	  number.text=xpp::odex::print_number(z);
	  b.value=std::move(number);
	}
	values[b.name]=xpp::odex::print_number(b.value.value);
      }
    }
  }

  /* first, the names: every statement's that the formulas compiled
     after them may read, and what does not wait for them */
  void declare(Statement &s)
  {
    xpp::Model &m=xpp::model();
    switch(s.kind){
    case Statement::Kind::Options:
      if(add_model_option(s.text.c_str())<0){
	xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",s.text);
	xpp_model_failed();
      }
      break;
    case Statement::Kind::Comment: add_comment("\""+s.text); break;
    case Statement::Kind::Only:
      for(const std::string &n : s.names)add_only(n);
      break;
    case Statement::Kind::Markov: {
      add_markov(s.count,s.name.c_str());
      std::vector<std::string> cells;
      for(const Expr &c : s.cells)cells.push_back(text(c));
      build_markov(cells,s.name.c_str());
      /* a second chain of the same name has no variable of its own */
      std::string name=converted(s.name);
      if(std::find(mnames_.begin(),mnames_.end(),name)==mnames_.end())mnames_.push_back(std::move(name));
      break;
    }
    case Statement::Kind::Par:
      xpp_log(XPP_LOG_INFO, "Parameters:\n");
      for(const Binding &b : s.bindings)add_parameter(b.name,b.value.value);
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::Wiener:
    case Statement::Kind::Const:
      xpp_log(XPP_LOG_INFO, s.kind==Statement::Kind::Wiener?"Wiener constants\n":" Hidden params:\n");
      for(const Binding &b : s.bindings)add_constant(b.name,b.value.value,s.kind==Statement::Kind::Wiener);
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case Statement::Kind::OptionFile: add_options_file(s.text); break;
    case Statement::Kind::Set: add_intern_set(s.name.c_str(),s.text.c_str()); break;
    case Statement::Kind::Boundary: add_boundary(text(s.expr)); break;
    case Statement::Kind::Event: {
      std::vector<FlagEvent> events;
      for(const Binding &b : s.bindings)events.push_back({b.name,text(b.value)});
      add_flag(text(s.expr),s.count,events);
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
    case Statement::Kind::Vector: add_vectorizer_name(s.name.c_str(),s.text.c_str()); break;
    case Statement::Kind::Network:
      add_special_name(s.name.c_str(),s.text.data());
      c_resync(s.text);
      break;
    case Statement::Kind::Solv:
      if(add_svar(s.name.c_str(),text(s.expr).c_str())==1)
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
	if(add_derived(xpp::upper_case(b.name).c_str(),text(b.value,true).c_str())==1)
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
      if(add_table_name(ntab_,name.c_str())==1){
	xpp::log(XPP_LOG_ERROR, " {} is duplicate name \n",name);
	xpp_model_failed();
      }
      xpp_log(XPP_LOG_DEBUG, "added name %d\n",ntab_);
      ntab_++;
      break;
    }
    case Statement::Kind::Fun: {
      const std::string name=converted(s.name);
      if(add_ufun_name(name.c_str(),nufun_,static_cast<int>(s.names.size()))==1){
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
    xpp::Model &m=xpp::model();
    xpp::Session &s=xpp::session();
    const int nvar=static_cast<int>(vnames_.size());
    for(int i=0;i<nvar;i++){
      if(add_var(vnames_[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",vnames_[i]);
	xpp_model_failed();
      }
      m.uvar_names[i]=vnames_[i];
      s.last_ic[i]=0.0;
      m.default_ic[i]=0.0;
    }
    for(const std::string &f : fnames_)
      if(add_var(f.c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",f);
	xpp_model_failed();
      }
    for(size_t i=0;i<mnames_.size();i++){
      if(add_var(mnames_[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",mnames_[i]);
	xpp_model_failed();
      }
      m.uvar_names[i+nvar]=mnames_[i];
      s.last_ic[i+nvar]=0.0;
      m.default_ic[i+nvar]=0.0;
    }
    for(size_t i=0;i<anames_.size();i++)
      aux_names[i]=anames_[i];
    add_svar_names();
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
    xpp::session().last_ic[in]=z;
    xpp::model().default_ic[in]=z;
    set_val(name.c_str(),z);
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
    if(!markov)xpp::session().delay_string[in]=text(b.value,true);
  }

  /* then, in order, what reads the names: the formulas compiled */
  void compile(Statement &s)
  {
    xpp::Model &m=xpp::model();
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
      set_ode_name(nvar_,rhs);
      new_program(nvar_);
      find_ker(rhs,&alt);
      if(add_expr(rhs.c_str(),m.programs[nvar_].data(),&len)){
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
      find_ker(rhs,&alt);
      new_program(nfix_+IN_VARS);
      if(add_expr(rhs.c_str(),m.programs[nfix_+IN_VARS].data(),&len)!=0){
	xpp::log(XPP_LOG_ERROR, " Error allocating or compiling {}\n",s.name);
	xpp_model_failed();
      }
      nfix_++;
      xpp::log(XPP_LOG_INFO, "{}={}\n",s.name,rhs);
      break;
    }
    case Statement::Kind::Dae: {
      const std::string rhs=text(s.expr,true);
      if(add_aeqn(rhs.c_str())==1)
	xpp_model_failed();
      xpp::log(XPP_LOG_INFO, " DAE eqn: {}=0 \n",rhs);
      break;
    }
    case Statement::Kind::Aux:
      for(const Binding &b : s.bindings){
	const std::string rhs=text(b.value);
	const int in1=IN_VARS+m.nmarkov+naux_,in2=IN_VARS+m.fix_var+naux_;
	set_ode_name(in1,rhs);
	new_program(in2);
	if(add_expr(rhs.c_str(),m.programs[in2].data(),&len)){
	  xpp::log(XPP_LOG_ERROR, "ERROR compiling {} \n",b.name);
	  xpp_model_failed();
	}
	naux_++;
	xpp::log(XPP_LOG_INFO, "{}={}\n",b.name,rhs);
      }
      break;
    case Statement::Kind::Vector: {
      const int ok=add_vectorizer(s.name.c_str(),s.text.data());
      c_resync(s.text);
      if(ok==0){
	xpp::log(XPP_LOG_ERROR, " Illegal vector  {} \n",s.text);
	xpp_model_failed();
      }
      break;
    }
    case Statement::Kind::Network: {
      const int ok=add_spec_fun(s.name.c_str(),s.text.data());
      c_resync(s.text);
      if(ok==0){
	xpp::log(XPP_LOG_ERROR, " Illegal special function {} \n",s.text);
	xpp_model_failed();
      }
      break;
    }
    case Statement::Kind::Markov:
      set_ode_name(IN_VARS+nmark_,"...many states..");
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
      if(add_ufun_new(nufun_,rhs.c_str(),args)!=0){
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
      if(add_form_table(ntab_,s.count,s.lo,s.hi,formula.c_str())){
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
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",xpp::model().nlines());
	xpp_model_failed();
      }
      break;
    case Statement::TableKind::File:
      xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",s.name,s.text);
      if(add_file_table(ntab_,s.text.c_str())){
	xpp::log(XPP_LOG_ERROR, "ERROR computing {}",s.name);
	xpp_model_failed();
      }
      ntab_++;
      break;
    }
  }

  Parsed &p_;
  /* the names of the variables, the Markov variables, the fixed
     variables and the aux quantities (converted: blanks removed, upper
     case), in order */
  std::vector<std::string> vnames_,mnames_,fnames_,anames_;
  /* how many of each the compiling has met */
  int nvar_=0,nfix_=0,naux_=0,nmark_=0,ntab_=0,nufun_=0;
};

} // namespace

void build_model(Parsed p)
{
  Builder(p).run();
}

void set_initial_values()
{
  xpp::Model &m=xpp::model();
  for(const xpp::Model::InitialValue &init : m.initial_values){
    int ok=0;
    const double z=calculate(init.formula.c_str(),&ok);
    if(!ok){
      xpp::Diagnostic d=init.where;
      d.cause=xpp::format("the initial value of {} does not evaluate",init.name);
      xpp::log(XPP_LOG_ERROR, "{} {}\n",init.where.text(),d.cause);
      xpp::model_failed(std::move(d));
    }
    const int i=find_user_name(ICBOX,init.name);
    xpp::session().last_ic[i]=z;
    m.default_ic[i]=z;
    set_val(converted(init.name),z);
  }
}

void create_plot_list()
{
  int k;
  const std::vector<std::string> &only=xpp::model().only;
  if(only.empty())return;
  plot_columns.assign(only.size()+1,0);
  plotlist=plot_columns.data();
  N_plist=0;
  for(const std::string &name : only){
    find_variable(name.c_str(),&k);
    if(k>=0){
      plotlist[N_plist]=k;
      N_plist++;
    }
  }
}
