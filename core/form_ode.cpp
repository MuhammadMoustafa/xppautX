#include <new>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include "xpp_util.h"
#include "form_ode.h"
#include "xpp_log.h"

#include "parserslow.h"
#include "markov.h"
#include "read_dir.h"
#include <unistd.h>
#include "flags.h"

#include "dae_fun.h"
#include "derived.h"
#include "extra.h"
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

int IN_INCLUDED_FILE=0;
char uvar_names[MAXODE][XPP_NAME_MAX+1];
char *ode_names[MAXODE];
char upar_names[MAXPAR][XPP_NAME_MAX+1];
char *save_eqn[MAXLINES];
double default_val[MAXPAR];

int *my_ode[MAXODE];
int leng[MAXODE];

int *plotlist;
int N_plist;

ACTION comments[MAXCOMMENTS];
int n_comments=0;
BC_STRUCT my_bc[MAXODE];

double default_ic[MAXODE];
int NODE,NUPAR,NLINES;
int PrimeStart;
int NCON_START,NSYM_START;
int BVP_NL,BVP_NR,BVP_N;

#define cstringmaj MYSTR1
#define cstringmin MYSTR2

int ConvertStyle=0;
FILE *convertf;
int IN_VARS;
int NMarkov;
int FIX_VAR;
int NEQ_MIN;
int EqType[MAXODE];
std::array<FIXINFO,MAXODE> fixinfo;

extern int NWiener;

namespace {
/* the lines of the model being read, in order: do_new_parser() adds them,
   compile_em() compiles them, free_varinfo() lets them go */
std::vector<VAR_INFO> model_lines;

/* an old-style file rewritten in the new syntax (ConvertStyle): convertf
   is its FILE *, which markov.cpp's old_build_markov writes into too */
xpp::Writer convert_writer;

/* The storage behind the C tables above, which the rest of the core
   reads (and a few write into, so each keeps its old size): each entry
   points into one of these, set with the functions below */
std::array<std::string,MAXODE> ode_text;          /* ode_names[] */
std::array<std::vector<int>,MAXODE> ode_program;  /* my_ode[] */
std::array<std::string,MAXLINES> line_text;       /* save_eqn[] */
std::array<std::string,MAXCOMMENTS> comment_text,comment_action; /* comments[] */
std::vector<int> plot_columns;                    /* plotlist */
struct BcText {
  std::vector<int> com;     /* the compiled condition: 200 commands */
  std::vector<char> string; /* the condition: 256 bytes (lunch-new reads a .set's into it) */
  std::vector<char> name;   /* "0=": 10 bytes (pp_shoot writes its side into it) */
};
std::array<BcText,MAXODE> bc_text;                /* my_bc[] */

/* the names an "only" statement keeps */
std::vector<std::string> onlylist;
/* the model's named auxiliary variables */
std::array<std::string,MAXODE> aux_names;
int Naux=0;
int OldStyle=1;
int is_a_map=0;

/* my_ode[i]: MAXEXPLEN commands, zeroed */
int *new_program(int i)
{
  ode_program[i].assign(MAXEXPLEN,0);
  return my_ode[i]=ode_program[i].data();
}

/* boundary condition i is 0=string (at most 255 bytes of it) */
void set_bc(int i, std::string_view string)
{
  BcText &b=bc_text[i];
  b.com.assign(200,0);
  b.string.assign(256,'\0');
  b.name.assign(10,'\0');
  if(string.size()>=b.string.size()){
    xpp::log(XPP_LOG_WARN, "boundary condition cut to {} characters: {}\n",b.string.size()-1,string);
    string=string.substr(0,b.string.size()-1);
  }
  std::copy(string.begin(),string.end(),b.string.begin());
  std::string_view name="0=";
  std::copy(name.begin(),name.end(),b.name.begin());
  my_bc[i].com=b.com.data();
  my_bc[i].string=b.string.data();
  my_bc[i].name=b.name.data();
}

/* p's text, "" for none (a missing token) */
const char *text_of(const char *p)
{
  return p?p:"";
}

/* s after a C function wrote into s.data(): cut at its NUL */
void c_resync(std::string &s)
{
  s.resize(strlen(s.c_str()));
}
} // namespace

void set_ode_name(int i, std::string_view text)
{
  xpp::keep_c_text(ode_text[i],ode_names[i],text);
}

namespace {
int do_new_parser(FILE *fp, const std::string &first, int nnn, bool at_end);
}

namespace {

bool is_space(char c)
{
  return isspace(static_cast<unsigned char>(c))!=0;
}

/* s without its leading and trailing white space */
std::string trimmed(std::string_view s)
{
  size_t b=0,e=s.size();
  while(b<e&&is_space(s[b]))b++;
  while(e>b&&is_space(s[e-1]))e--;
  return std::string(s.substr(b,e-b));
}

int atoi_of(std::string_view s)
{
  return atoi(std::string(s).c_str());
}

double atof_of(std::string_view s)
{
  return atof(std::string(s).c_str());
}

/* the name of "name=value" (blanks around both trimmed) and its value
   (0 without a '=') */
std::string take_apart(std::string_view bob, double *value)
{
  size_t k=bob.find('=');
  if(k==std::string_view::npos){
    *value=0.0;
    return trimmed(bob);
  }
  /* the number after the '=', whatever its length */
  *value=atof_of(bob.substr(k+1));
  return trimmed(bob.substr(0,k));
}

/* old's first length characters, a final comma dropped */
std::string new_string2(std::string_view old)
{
  std::string s(old);
  if (!s.empty() && s.back() == ',')
    s.pop_back();
  return s;
}

/* The next "name=value" of tokens (blanks around the = allowed, or a
   name alone), ending at a blank or a comma, which it passes over;
   nullopt when no text is left */
std::optional<std::string> get_next2(std::string_view &tokens)
{
    size_t start=0;
    while (start < tokens.size() && is_space(tokens[start])) start++;
    tokens.remove_prefix(start);
    if (tokens.empty()) return std::nullopt;
    size_t len = tokens.size();
    size_t i;
    /* advance past space/the equal sign/comma */
    bool success = false;
    for (i = 1; i < len; i++) {
        if (tokens[i] == '=' || is_space(tokens[i]) || tokens[i] == ',') {
            success = true;
            break;
        }
    }
    auto take=[&tokens](size_t n, size_t next){
      std::string s=new_string2(tokens.substr(0,n));
      tokens.remove_prefix(next);
      return s;
    };
    if (!success) /* this is either a variable alone or a syntax error */
        return take(len,len);
    /* advance past any spaces */
    success = false;
    for (; i < len; i++) {
        if (!is_space(tokens[i])) {
            success = true;
            break;
        }
    }
    if (!success) /* this is either a variable alone or a syntax error */
        return take(len,len);
    if (tokens[i] != '=')
        return take(i,tokens[i]==','?i+1:i);
    /* advance until the first non-space */
    success = false;
    for (i = i + 1; i < len; i++) {
        if (!is_space(tokens[i])) {
            success = true;
            break;
        }
    }
    if (!success) /* also a syntax error */
        return take(len,len);
    /* advance past the nonspaces and non-commas */
    for (; i < len; i++) {
        if (is_space(tokens[i]) || tokens[i] == ',') break;
    }
    /* advance past any spaces */
    for (; i < len; i++) {
        if (!is_space(tokens[i])) {
            break;
        }
    }
    /* advance past a comma, if any */
    if (i < len && tokens[i] == ',') i++;
    return take(i,i);
}

/* the model's source line: fgets without its size, the line with its
   '\n' ("" at the end of the file); false once the end was met, what
   feof(fp) says after it */
bool read_raw_line(FILE *fp, std::string &line)
{
  line.clear();
  int c;
  while((c=getc(fp))!=EOF){
    line+=static_cast<char>(c);
    if(c=='\n')return true;
  }
  return false;
}

/* keeps one line of the model's source in save_eqn (C text: strip_saveqn
   and the front ends read it) */
void save_line(const std::string &line)
{
  if (NLINES>=MAXLINES) {
    xpp_log(XPP_LOG_ERROR, "The model has more than %d lines\n", MAXLINES);
    exit(1);
  }
  xpp::keep_c_text(line_text[NLINES],save_eqn[NLINES],line);
  NLINES++;
}

/* The next logical line: a line ending in a backslash goes on in the
   next one (the text from the first backslash on is dropped); the
   line's end becomes a blank and one more blank follows it. false once
   the end of the file was met. */
bool read_a_line(FILE *fp, std::string &s)
{
  bool more=true,in_file=true;
  s.clear();
  while(more){
    std::string temp;
    in_file=read_raw_line(fp,temp)&&in_file;
    save_line(temp);
    size_t hat=temp.find('\\');
    more=hat!=std::string::npos;
    if(more)temp.resize(hat);
    s+=temp;
  }
  if(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.back()=' '; /* empty at the end of the file */
  s+=' ';
  return in_file;
}

/* the first n names of list: where name is, -1 when it is not */
int find_the_name(const std::vector<std::string> &list, int n, std::string_view name)
{
  int m=std::min(n,static_cast<int>(list.size()));
  for(int i=0;i<m;i++)
    if(list[i]==name)
      return(i);
  return(-1);
}

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
  get_directory(cur_dir);
  xpp_log(XPP_LOG_INFO, "%s: \n",cur_dir);
  std::vector<std::string> dirs,files;
  if(!list_folder(wild,cur_dir,dirs,files))return;
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
	   change_directory(string.c_str());
	   list_em(wild.c_str());
	 }
        }
    }
  }
  }
  else
  {
    std::string dir=current_directory();
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
   XPP_FORMAT_TO_BUF(this_file,"{}",string);
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
      std::string name=xpp::format("K##{}",NKernel);
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

/* One line of the old syntax (a command letter and its arguments), and
   the lines compile_em makes of the new one's commands. 0 at "done". */
int compiler(const std::string &bob, FILE *fptr)
{
  double value,xlo,xhi;
  int narg,done,nn,iflg=0,VFlag=0,nstates,alt,index,sign;
  std::string name,formula,condition;
  /* the fixed variables' names, for a converted file */
  static std::array<std::string,MAXODE1> fixname;
  int nlin,i;
  done=1;
  if(bob[0]=='@'){
    stor_internopts(bob.c_str());
    if(ConvertStyle)
      xpp::print(convertf,"{}\n",bob.c_str());
    return(done);
  }
  xpp::Tokens tokens(bob);
  std::string command=tokens.text(" ,");
  strlwr(command.data());
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
      xpp_log(XPP_LOG_INFO, "Wiener constants\n");
      if(ConvertStyle)
	xpp::print(convertf,"wiener ");
      for(std::optional<std::string> tok;(tok=get_next2(values));)
	{
	  name=take_apart(*tok,&value);
	  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
	  if(ConvertStyle)
	    xpp::print(convertf,"{}  ",name);
	  if(add_con(name.c_str(),value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    xpp_model_failed();
	  }
	  add_wiener(NCON-1);

	}
      if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
           break;
    case 'n':
      xpp_log(XPP_LOG_INFO, " Hidden params:\n");
      if(ConvertStyle)
	xpp::print(convertf,"number ");

      for(std::optional<std::string> tok;(tok=get_next2(values));)
	{
	  name=take_apart(*tok,&value);
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={:g}  ",name,value);

	  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);
	  if(add_con(name.c_str(),value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    xpp_model_failed();
	  }

	}
       if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'g': /* global */
      sign=atoi_of(tokens.text("{ "));
      xpp_log(XPP_LOG_DEBUG, " GLOBAL: sign =%d \n",sign);
      condition=tokens.text("{}");
      xpp::log(XPP_LOG_DEBUG, " condition = {} \n",condition);
      formula=tokens.text("\n");
      xpp::log(XPP_LOG_DEBUG, " events={} \n",formula);
      if(add_global(condition.c_str(),sign,formula.c_str())){
	xpp_log(XPP_LOG_WARN, "Bad global !! \n");
	xpp_model_failed();
      }
      if(ConvertStyle){
	xpp::print(convertf,"global {} {{{}}} {}\n",sign,condition,formula);
      }
      break;
    case 'p':
      xpp_log(XPP_LOG_INFO, "Parameters:\n");
      if(ConvertStyle)
	xpp::print(convertf,"par ");

      for(std::optional<std::string> tok;(tok=get_next2(values));)
	{

	  name=take_apart(*tok,&value);
	  if(add_con(name.c_str(),value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    xpp_model_failed();
	  }
	  default_val[NUPAR]=value;
	  XPP_FORMAT_TO_BUF(upar_names[NUPAR++],"{}",name);
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={:g}  ",name,value);
	  xpp::log(XPP_LOG_DEBUG, "|{}|={:f} ",name,value);

	}
      if(ConvertStyle)
	xpp::print(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'c':
      options_file=tokens.text(" \n");
      xpp::log(XPP_LOG_INFO, " Loading new options file:<{}>\n",options_file);
      if(ConvertStyle)
	xpp::print(convertf,"option {}\n",options_file);
      break;
    case 'f':iflg=0;
      xpp_log(XPP_LOG_INFO, "\nFixed variables:\n");
      goto vrs;
    case 'm': /* Markov variable  */
      name=tokens.text(" ");
      value=atof_of(tokens.text(" "));
      nstates=atoi_of(tokens.text(" \n"));
      if(name_too_long(name.c_str())||add_var(name.c_str(),value)){
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	xpp_model_failed();
      }
      XPP_FORMAT_TO_BUF(uvar_names[IN_VARS+NMarkov],"{}",name);
      last_ic[IN_VARS+NMarkov]=value;
      default_ic[IN_VARS+NMarkov]=value;
      xpp::log(XPP_LOG_INFO, " Markov variable {}={:f} has {} states \n",name,value,nstates);
      if(OldStyle)add_markov(nstates,name.c_str());
      if(ConvertStyle)
	xpp::print(convertf,"{}(0)={:g}\n",name,value);
      break;
    case 'r': /* state table for Markov variables  */
      name=tokens.text("\n");
      nlin=NLINES;
      index=old_build_markov(fptr,name.c_str());
      set_ode_name(IN_VARS+index,xpp::format("{{ {} ... }}",save_eqn[nlin]));
      break;
    case 'v':
      iflg=1;
      xpp_log(XPP_LOG_INFO, "\nVariables:\n");
      if(ConvertStyle)
	xpp::print(convertf,"init ");
    vrs:
      if(NMarkov>0&&OldStyle) {
	xpp_log(XPP_LOG_WARN, " Error at line %d \n Must declare Markov variables after fixed and regular variables\n",NLINES);
	xpp_model_failed();
      }
      for(std::optional<std::string> tok;(tok=get_next2(values));)
	{
	  if((IN_VARS>NEQ)||(IN_VARS==MAXODE))
	    {
	      xpp_log(XPP_LOG_ERROR, " too many variables at line %d\n",NLINES);
	      xpp_model_failed();
	    }
	  name=take_apart(*tok,&value);
	  if(name_too_long(name.c_str())||add_var(name.c_str(),value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    xpp_model_failed();
	  }
	  if(iflg)
	    {
	      XPP_FORMAT_TO_BUF(uvar_names[IN_VARS],"{}",name);
	      last_ic[IN_VARS]=value;
              default_ic[IN_VARS]=value;
	      IN_VARS++;
	      if(ConvertStyle)
		xpp::print(convertf,"{}={:g}  ",name,value);
	    }
	  else {
	    if(ConvertStyle)
	      fixname[FIX_VAR]=name;
	    FIX_VAR++;

	  }
	  xpp::log(XPP_LOG_DEBUG, "|{}| ",name);

	}
      xpp_log(XPP_LOG_DEBUG, " \n");
      if(iflg&&ConvertStyle)
	xpp::print(convertf,"\n");
      break;
    case 'b':
      set_bc(BVP_N,tokens.text("\n"));
      if(ConvertStyle)
	xpp::print(convertf,"bndry {}\n",my_bc[BVP_N].string);
      xpp_log(XPP_LOG_DEBUG, "|%s| |%s| \n",my_bc[BVP_N].name,my_bc[BVP_N].string);
      BVP_N++;
      break;
    case 'k':
      if(ConvertStyle)
	xpp_log(XPP_LOG_WARN, " Warning  kernel declaration cannot be converted \n");
      name=tokens.text(" ");
      value=atof_of(tokens.text(" "));
      formula=tokens.text("$");
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",value,name,formula);
      if(add_kernel(name.c_str(),value,formula.c_str())){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	xpp_model_failed();
      }
      break;
    case 't':
      if(NTable>=MAX_TAB)
	{
	  if(ERROUT)xpp_log(XPP_LOG_WARN, "too many tables !!\n");
	  xpp_model_failed();
	}
      name=tokens.text(" ");
      formula=tokens.text(" \n");
      if(formula[0]=='%') {
	xpp_log(XPP_LOG_INFO, " Function form of table....\n");
	nn=atoi_of(tokens.text(" "));
	xlo=atof_of(tokens.text(" "));
	xhi=atof_of(tokens.text(" "));
	formula=tokens.text("\n");
	xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",
	       name,nn,xlo,xhi,formula);
	add_table_name(NTable,name.c_str());

	if(add_form_table(NTable,nn,xlo,xhi,formula.c_str())){
	  xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	  xpp_model_failed();
	}

	if(ConvertStyle)
	  xpp::print(convertf,"table {} % {} {:g} {:g} {}\n",
		  name,nn,xlo,xhi,formula);
	NTable++;
	xpp_log(XPP_LOG_INFO, " NTable = %d \n",NTable);

      }
      else
	if(formula[0]=='@'){
	  xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	  formula=tokens.text(" ");
	  xpp::log(XPP_LOG_INFO, " {} = {} \n",name,formula);
	  if(add_2d_table(name.c_str(),formula.c_str())){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    xpp_model_failed();
	  }
	}
	else
	  {
	    xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",name,formula);
            add_table_name(NTable,name.c_str());
	    if(add_file_table(NTable,formula.c_str())){
	      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	      xpp_model_failed();
	    }
	    if(ConvertStyle)
	      xpp::print(convertf,"table {} {}\n",
		      name,formula);
	    NTable++;
	  }
      break;

    case 'u':
      name=tokens.text(" ");
      narg=atoi_of(tokens.text(" "));
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
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	xpp_model_failed();
      }

      xpp::log(XPP_LOG_INFO, "user {} = {}\n",name,formula);
      break;
    case 'i': VFlag=1;
      [[fallthrough]];
    case 'o':
      if(NODE>=(NEQ+FIX_VAR-NMarkov))
	{
	  done=0;
	  break;
	}
      formula=tokens.text("\n");
      new_program(NODE);

      if(NODE<IN_VARS)
	{
	  set_ode_name(NODE,formula);
	  if(ConvertStyle){
	    if(VFlag)
	      xpp::print(convertf,"volt {}={}\n",uvar_names[NODE],formula);
	    else
	      xpp::print(convertf,"{}'={}\n",uvar_names[NODE],formula);
	  }
	  find_ker(formula,&alt);

	  EqType[NODE]=VFlag;

	  VFlag=0;
	}
      if(NODE>=IN_VARS&&NODE<(IN_VARS+FIX_VAR))
	{
	  if(ConvertStyle)
	    xpp::print(convertf,"{}={}\n",fixname[NODE-IN_VARS],formula);
	  find_ker(formula,&alt);

	}

      if(NODE>=(IN_VARS+FIX_VAR))
	{
	  i=NODE-(IN_VARS+FIX_VAR);
	  set_ode_name(NODE-FIX_VAR+NMarkov,formula);
	  if(ConvertStyle){
	    if(i<Naux)
	      xpp::print(convertf,"aux {}={}\n",aux_names[i],formula);
	    else
	      xpp::print(convertf,"aux aux{}={}\n",i+1,formula);
	  }
	}
      xpp::log(XPP_LOG_INFO, "RHS({})={}\n",NODE,formula);
      if(add_expr(formula.c_str(),my_ode[NODE],&leng[NODE])){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	xpp_model_failed();
      }
      NODE++;
      break;

    case 'a':   /* name auxiliary variables */
      xpp_log(XPP_LOG_INFO, "Auxiliary variables:\n");
      for(std::optional<std::string_view> tok;(tok=tokens.next(" ,\n"));)
	{
	  std::string aux(*tok);
	  if(name_too_long(aux.c_str()))xpp_model_failed();
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

} // namespace

int make_eqn()
{
   NEQ=2;
   FIX_VAR=0;
   NMarkov=0;
   return(read_eqn());
}

void strip_saveqn()
{
  for(int i=0;i<NLINES;i++)
    for(char *c=save_eqn[i];*c;c++)
      if(*c<32)
	*c=32;
}

int disc(const char *string)
{
  if(is_a_map==1)return(1);
  /* what follows the first '.' */
  std::string_view s(string);
  size_t dot=s.find('.');
  std::string_view end=dot==std::string_view::npos?std::string_view():s.substr(dot+1);
  return end=="dis"||end=="dif";
}

int get_eqn(FILE *fptr)
{
  std::string bob;
  int done=1,i;
  int flag;
  init_rpn();
  NLINES=0;
  IN_VARS=0;
  NODE=0;
  BVP_N=0;
  BVP_NL=0;
  BVP_NR=0;
  NUPAR=0;
  NWiener=0;
  /*check_for_xpprc();  This is now done just once and in do_vis_env()
  */
  options_file="default.opt";
  add_var("t",0.0);
  bool in_file=read_raw_line(fptr,bob);
  save_line(bob);
  i=atoi(bob.c_str());
  if(i<=0) { /* New parser ---   */

    OldStyle=0;
    ConvertStyle=0;
    flag=do_new_parser(fptr,bob,0,!in_file);
    if(flag<0) xpp_model_failed();
  }
  else{
    OldStyle=1;
    NEQ=i;
    xpp_log(XPP_LOG_INFO, "NEQ=%d\n",NEQ);
    if(ConvertStyle){
      std::string filename=this_file[0]==0?std::string("convert.ode"):std::string(this_file)+".new";
      convert_writer=xpp::Writer(filename.c_str());
      convertf=convert_writer.file();
      if(convertf==NULL){
	xpp::log(XPP_LOG_WARN, " Cannot open {} - no conversion done \n",filename);
	ConvertStyle=0;
      }
      xpp::print(convertf,"# converted {} \n",this_file);
    }
    while(done)
      {
	read_raw_line(fptr,bob);
	if(bob.empty())break;
	save_line(bob);
	done=compiler(bob,fptr);
      }
    if(ConvertStyle){
      xpp::print(convertf,"done\n");
      convert_writer.commit();
      convertf=NULL;
    }
  }
 if((NODE+NMarkov)==0){
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
  BVP_FLAG=1;

  if(NODE!=NEQ+FIX_VAR-NMarkov)
    {
      xpp_log(XPP_LOG_ERROR, " Too many/few equations\n");
      xpp_model_failed();
    }
  if(IN_VARS>NEQ)
    {
      xpp_log(XPP_LOG_ERROR, " Too many variables\n");
	xpp_model_failed();
    }
  NODE=IN_VARS;

  for(i=0; i<Naux; i++)
    XPP_FORMAT_TO_BUF(uvar_names[i+NODE+NMarkov],"{}",aux_names[i]);

  for(i=NODE+NMarkov+Naux;i<NEQ;i++)
    {
      XPP_FORMAT_TO_BUF(uvar_names[i],"AUX{}",i-NODE-NMarkov+1);
    }

  for(i=0;i<NEQ;i++)
      {
	strupr(uvar_names[i]);
	std::string formula=text_of(ode_names[i]);
	strupr(formula.data());
        de_space(formula.data());
	c_resync(formula);
	set_ode_name(i,formula);
      }
  /*
     add primed variables                              */
  PrimeStart=NVAR;
  if(NVAR<MAXPRIMEVAR){
  add_var("t'",0.0);
  for(i=0;i<NODE ;i++)
    add_var(xpp::format("{}'",uvar_names[i]).c_str(),0.0);
}
  else {
    xpp_log(XPP_LOG_WARN, " Warning: primed variables not added must have < %d variables\n",
     MAXPRIMEVAR);
    xpp_log(XPP_LOG_WARN, " Averaging and boundary value problems cannot be done\n");
  }
  if(NMarkov>0)
    compile_all_markov();
  if(compile_flags()==1){
    xpp_log(XPP_LOG_ERROR, " Error in compiling a flag \n");
    xpp_model_failed();
  }
  /*  add auxiliary variables   */
  for(i=NODE+NMarkov;i<NEQ;i++)add_var(uvar_names[i],0.0);
  NCON_START=NCON;
  NSYM_START=NSYM;
  NEQ_MIN=NEQ;
  program.version_major=static_cast<float>(cstringmaj);
  program.version_minor=static_cast<float>(cstringmin);
  xpp_log(XPP_LOG_INFO, "Used %d constants and %d symbols \n",NCON,NSYM);
  xpp_log(XPP_LOG_INFO, "XPPAUT %g.%g Copyright (C) 2002-now  Bard Ermentrout \n",program.version_major,program.version_minor);
    return(1);
}

char *get_first(char *string, const char *src)
{
 return strtok(string,src);
}
char *get_next(const char *src)
{
 return strtok(NULL,src);
}

namespace {

/* "#include file": the file's name, blanks removed, into nf */
bool if_include_file(const std::string &old, std::string &nf)
{
  std::string_view s(old);
  if(!s.starts_with("#include"))return false;
  size_t blank=s.find(' ');
  if(blank==std::string_view::npos)return false;
  nf=s.substr(blank+1);
  de_space(nf.data());
  c_resync(nf);
  return true;
}

bool if_end_include(std::string_view old)
{
  if (IN_INCLUDED_FILE>0)
  {
  	if(old.starts_with("#done"))return true;
  	if(old.starts_with("done"))return true;
	/*Note that the end of an included file
	 is also possible but that condition is checked
	elsewhere (currently near the bottom of do_new_parser)
	*/
  }
  return false;
}

/* how far s's text (up to a NUL a C function wrote into it) matches the
   character at i ('\0' past its end) */
char char_at(const std::string &s, size_t i)
{
  return i<s.size()?s[i]:'\0';
}

/* "name = rest" of v.rhs: v.lhs the name, v.rhs the rest (the text
   around the character at i1) */
void split_rhs(VAR_INFO &v, size_t name_end, size_t rest_start)
{
  std::string big=v.rhs;
  v.lhs=big.substr(0,name_end);
  v.rhs=rest_start<big.size()?big.substr(rest_start):std::string();
}

int parse_model(FILE *fp, const std::string &first, int nnn, bool at_end);

/* no exception crosses into C: the only one parse_model() can throw is
   std::bad_alloc, and running out of memory ends the program, as
   xpp_malloc() does */
int do_new_parser(FILE *fp, const std::string &first, int nnn, bool at_end)
{
  try {
    return parse_model(fp, first, nnn, at_end);
  } catch (const std::bad_alloc &) {
    xpp::log(XPP_LOG_ERROR, "out of memory reading {}\n", first);
    exit(1);
  }
}

void add_only(std::string_view s)
{
  if(s.empty())return;
  if(onlylist.size()>=MAXONLY)return;
  onlylist.emplace_back(s);
}

void break_up_list(std::string_view rhs)
{
  /* the names between blanks and commas */
  std::string s;
  for(char c : rhs){
    if(c=='\0')break;
    if(c==' '||c==','){
      add_only(s);
      s.clear();
    }
    else
      s+=c;
  }
  add_only(s);
}

/* the line v, kept for compile_em */
void add_varinfo(const VAR_INFO &v)
{
  try {
    model_lines.push_back(v);
  } catch (const std::bad_alloc &) { /* no exception crosses into C */
    xpp_log(XPP_LOG_ERROR, "out of memory: the model's line %d\n", static_cast<int>(model_lines.size()) + 1);
    exit(1);
  }
}

/* compiled: the lines and their memory go */
void free_varinfo()
{
  std::vector<VAR_INFO>().swap(model_lines);
}

/* this code checks if the right-hand side for an initial
   condition is a formula (for delays) or a number
*/
int formula_or_number(const char *expr,double *z)
{
  std::array<char,40> num{}; /* do_num's 40 bytes */
  int flag,i=0;
  int olderr=ERROUT;
  ERROUT=0;
  *z=0.0; /* initial it to 0 */
  /* convert only drops blanks: never longer than expr */
  std::string form(expr);
  convert(expr,form.data());
  c_resync(form);
  flag=do_num(form.c_str(),num.data(),z,&i);
  if(i<static_cast<int>(form.size()))flag=1;
  ERROUT=olderr;
  if(flag==0)
    return 0; /* 0 is a number */
  return 1; /* 1 is a formula */
}

int extract_ode(const char *s1, int *ie, int i1)  /* name is char 1-i1  ie is start of rhs */
{
  int i=0,n=strlen(s1);

  i=i1;
  while(i<n){
    if(s1[i]=='='){
      *ie=i+1;
      return 1;
    }
    i++;
  }
  return 0;
}

int strparse(const char *s1, const char *s2, int i0, int *i1)
{
  int i=i0;
  int n=strlen(s1);
  int m=strlen(s2);
  int j=0;
  char ch;
  int start=0;

  while(i<n){
    ch=s1[i];
    if(start==1){

      if(ch==s2[j]|| ch==' '){
        if(ch==s2[j])j++;
        i++;
	if(j==m){
	  *i1=i;
	  return(1);
	}
      }
      else
	{
	  start=0;
	  j=0;
	}
    }
    else /* just starting */
      {

	if(ch==s2[0]){
	  j++;
	  i++;
	  start=1;
	  if(j==m){  /* only one char */
	    *i1=i;
	    return(1);
	  }
	}
      else
	i++;
      }

  }
  return(0);
}

/* the names in s1 from i0 up to its ')', each at most NAMLEN
   characters, into args; *ie where the formula after the '=' starts.
   0 when they are not there. */
int extract_args(const char *s1, int i0, int *ie, std::vector<std::string> &args)
{
  int i=i0,n=strlen(s1);
  int type,i1;
  args.clear();
  while(i<n){
    type=find_char(s1,",)",i,&i1);
    if(type<0)break;
    if(static_cast<int>(args.size())>=MAXARG){
      xpp_log(XPP_LOG_ERROR, "More than %d arguments\n",MAXARG);
      return 0;
    }
    if(i1-i>NAMLEN){
      xpp_log(XPP_LOG_ERROR, "Argument name longer than %d characters\n",NAMLEN);
      return 0;
    }
    args.emplace_back(s1+i,s1+i1);
    i=i1+1;
    if(type==1){
      find_char(s1,"=",i,&i1);
      *ie=i1+1;
      return 1;
    }
  }
  return(0);
}

int next_nonspace(const char *s1, int i0, int *i1)
{
  int i=i0;
  int n=strlen(s1);
  char ch;
  *i1=n-1;
  while(i<n){
    ch=s1[i];
    if(ch!=' '){
      *i1=i;
      return(static_cast<int>(ch));
    }
    i++;
  }
  return(-1);
}

/* removes starting blanks from s  */
void remove_blanks(std::string &s)
{
  size_t i=0;
  while(i<s.size()&&is_space(s[i]))i++;
  s.erase(0,i);
}

int check_if_ic(const char *big)
{
  char c;
  int n=strlen(big);
  int j;
  j=0;
  while(1){
    c=big[j];
    if(c==']'){
      if((big[j+1]=='(') && (big[j+2]=='0') && (big[j+3]==')')){
	return 1;

      }
    }
    j++;
    if(j>=n)break;
  }
  return 0;
}

int not_ker(const char *s, int i) /* returns 1 if string is not 'int[' */
{
  if(i<3)return 1;
  if(s[i-3]=='i'&&s[i-2]=='n'&&s[i-1]=='t')return 0;
  return 1;
}

int is_comment(const char *s)
{
  int n=strlen(s);
  int i=0;
  char c;
  while(1) {
    c=s[i];
    if(c=='#')return 1;
    if(isspace(c)){
      i++;

      if(i>=n)return 0;
    }
    else
      return 0;
  }
}

/* A " line: its text, or with {name=value,...} an action ("$ name=value
   ...") and the text after the braces ("* text") */
void add_comment(std::string_view line)
{
  if(n_comments>=MAXCOMMENTS)return;
  ACTION &c=comments[n_comments];
  std::string &text=comment_text[n_comments];
  size_t open=line.find('{');
  if(open==std::string_view::npos){
    text=line.empty()?std::string_view():line.substr(1);
    c.aflag=0;
  }
  else {
    std::string &action=comment_action[n_comments];
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
    c.action=action.data();
    c.aflag=1;
  }
  c.text=text.data();
 xpp::log(XPP_LOG_DEBUG, "text={} \n",text);
 if(c.aflag==1)
   xpp_log(XPP_LOG_DEBUG, "action=%s \n",c.action);
 n_comments++;
}

/* The line s1 (made upper case, its leading blanks removed) as v: 1, 2
   for "done", 0 for a line that is not one (a comment, an option), -1
   when it cannot be read */
int parse_a_string(std::string &s1, VAR_INFO &v)
{
  int i0=0,i1,i2,i3;
  std::string lhs,rhs;
  std::vector<std::string> args;
  int type,type2;
  if(char_at(s1,0)=='"'){
    add_comment(s1);
    return 0;
  }
  if(char_at(s1,0)=='@') {
    stor_internopts(s1.c_str());
    return 0;
  }
  remove_blanks(s1);

  const std::string s1old=s1;
  strupr(s1.data());
  const char *s=s1.c_str();
  if(s1.empty()){
    return 0;
  }
  if(s1[0]=='0'&&char_at(s1,1)=='='){
   type2=DAE;
   lhs="0=";
   rhs=s1.substr(2);
   goto good_type;
  }
  if(s1[0]=='#'){
    return 0;
  }

  type=find_char(s," =/'(",i0,&i1);
  switch(type){
  case 0:
    i0=i1;
    switch(next_nonspace(s,i0,&i2)){
    case '=' :
      if(s1[0]=='!'){
	lhs=s1.substr(1,i1-1);
	rhs=s1.substr(i2+1);
	type2=DERIVE_PAR;
	break;
      }
      lhs=s1.substr(0,i1);
      rhs=s1.substr(i2+1);
      type2=FIXED;
      break;
    default:
      type2=COMMAND;
      lhs=s1.substr(0,i1);
      rhs=s1old.substr(i2);
      break;
    }
    break;
  case 1:
    if(s1[0]=='!'){
      lhs=s1.substr(1,i1-1);
      rhs=s1.substr(i1+1);
      type2=DERIVE_PAR;
      break;
    }

    type2=FIXED;
    lhs=s1.substr(0,i1);
    rhs=s1.substr(i1+1);
    break;
  case 2:
    if(s1[0]!='D')return -1;
    if(extract_ode(s,&i2,i1)){
      lhs=s1.substr(1,i1-1);
      rhs=s1.substr(i2);
      type2=ODE;
    }
    else
      return -1;
    break;
  case 3:
    if(extract_ode(s,&i2,i1)){
      lhs=s1.substr(0,i1);
      rhs=s1.substr(i2);
      type2=ODE;
    }
    else
      return -1;
    break;

  case 4:
    i0=i1;
    if(strparse(s,"T+1)=",i0,&i2)){
      type2=MAP;
      is_a_map=1;
      lhs=s1.substr(0,i1);
      rhs=s1.substr(i2);
      break;
    }
    if(strparse(s,"(0)=",i0-1,&i2)){

      type2=IC;
      lhs=s1.substr(0,i1);
      rhs=s1.substr(i2);
      break;
     }
    if(strparse(s,"T)=",i0,&i2)){

      if(strparse(s,"INT{",0,&i3)==1||
	 strparse(s,"INT[",0,&i3)==1){
	type2=VEQ;
	lhs=s1.substr(0,i1);
	rhs=s1.substr(i2);
	break;
      }
      else {
	type2=FUNCTION;
        if(extract_args(s,i0+1,&i2,args)==0)return -1;
	lhs=s1.substr(0,i0);
	rhs=s1.substr(i2);
	break;
      }
    }
    i0++;
    if(extract_args(s,i0,&i2,args)==0)return -1;
    type2=FUNCTION;
    lhs=s1.substr(0,i0-1);
    rhs=s1.substr(i2);
    break;
  default:
    return -1;
  }

good_type:
  v.type=type2;
  v.lhs=std::move(lhs);
  v.rhs=std::move(rhs);
  v.args=std::move(args);

  if(char_at(v.lhs,0)=='D'&&type2==COMMAND)
    return 2;
  return 1;
}

void compile_em() /* Now we try to keep track of markov, fixed, etc as
		well as their names  */
{
 std::vector<std::string> vnames,fnames,anames,mnames;
 double z,xlo,xhi;
 std::string tmp,formula;
 int nmark=0,nfix=0,naux=0,nvar=0,nn,alt,in,i,ntab=0,nufun=0;
 int in1,in2,iflag,ok;
 int fon;
 FILE *fp=NULL;
 /* v.lhs/v.rhs after a C function wrote into them: cut at their NUL */
 auto resync=[](VAR_INFO &v){
   c_resync(v.lhs);
   c_resync(v.rhs);
 };

 /* On this first pass through, all the variable names
    are kept as well as fixed declarations, boundary conds,
    and parameters, functions and tables.  Once this pass is
    completed all the names will be known to the compiler.
 */
 for(VAR_INFO &v : model_lines)
   {
    const char *lhs=v.lhs.c_str(),*rhs=v.rhs.c_str();
    if(v.type==COMMAND && lhs[0]=='P')
      compiler("par "+v.rhs+" \n",fp);
    if(v.type==COMMAND && lhs[0]=='W')
      compiler("wie "+v.rhs+" \n",fp);
    if(v.type==COMMAND && lhs[0]=='N')
      compiler("num "+v.rhs+" \n",fp);
    if(v.type==COMMAND && lhs[0]=='O')
      compiler("c "+v.rhs+" \n",fp);
    if(v.type==COMMAND && lhs[0]=='S' && lhs[1]=='E')
      compiler("x "+v.rhs+"\n",fp);
    if(v.type==COMMAND && lhs[0]=='B')
      compiler("b "+v.rhs+" \n",fp);
    if(v.type==COMMAND && lhs[0]=='G')
      compiler("g "+v.rhs+" \n",fp);
    if(v.type==MAP||v.type==ODE||v.type==VEQ){
      tmp=converted(v.lhs);
      if(name_too_long(tmp.c_str()))xpp_model_failed();
      if(find_the_name(vnames,nvar,tmp)<0){
	vnames.push_back(tmp);
	nvar++;
      }
      else
	{
	  xpp::log(XPP_LOG_ERROR, " {} is a duplicate name \n",tmp);
	  xpp_model_failed();
	}
    }

    if(v.type==MARKOV_VAR){
      tmp=converted(v.lhs);
      if(name_too_long(tmp.c_str()))xpp_model_failed();
      if(find_the_name(mnames,nmark,tmp)<0){
	mnames.push_back(tmp);
	nmark++;
      }
    }
    if(v.type==EXPORT){
      add_export_list(lhs,rhs);
    }
    if(v.type==VECTOR){
      add_vectorizer_name(lhs,rhs);

    }
    if(v.type==SPEC_FUN){
      add_special_name(lhs,v.rhs.data());
      resync(v);
    }
    if(v.type==SOL_VAR){
       if(add_svar(lhs,rhs)==1)
	 xpp_model_failed();
    }

    if(v.type==AUX_VAR){
      tmp=converted(v.lhs);
      if(name_too_long(tmp.c_str()))xpp_model_failed();
      anames.push_back(tmp);
      naux++;
      xpp::log(XPP_LOG_INFO, "{} = {} \n",anames[naux-1],v.rhs);
    }
    if(v.type==DERIVE_PAR){
      if(add_derived(lhs,rhs)==1)
	xpp_model_failed();
    }
    if(v.type==FIXED){
      fixinfo[nfix].name=v.lhs;
      fixinfo[nfix].value=v.rhs;
      tmp=converted(v.lhs);
      if(name_too_long(tmp.c_str()))xpp_model_failed();
      fnames.push_back(tmp);
      nfix++;
     xpp::log(XPP_LOG_INFO, "{} = {} \n",fnames[nfix-1],v.rhs);
    }

    if(v.type==TABLE){
      tmp=converted(v.lhs);
      if(add_table_name(ntab,tmp.c_str())==1){
	xpp::log(XPP_LOG_ERROR, " {} is duplicate name \n", tmp);
	xpp_model_failed();
      }
      xpp_log(XPP_LOG_DEBUG, "added name %d\n",ntab);
      ntab++;
    }

    if(v.type==FUNCTION){
      tmp=converted(v.lhs);
      if(add_ufun_name(tmp.c_str(),nufun,static_cast<int>(v.args.size()))==1){
	xpp::log(XPP_LOG_ERROR, "Duplicate name or too many functions for {} \n",tmp);
	xpp_model_failed();
      }

      nufun++;
    }
   }

 /* now we add all the names of the variables and the
    fixed stuff
 */
 for(i=0;i<nvar;i++){
      if(add_var(vnames[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",vnames[i]);
	xpp_model_failed();
      }
      XPP_FORMAT_TO_BUF(uvar_names[i],"{}",vnames[i]);
      last_ic[i]=0.0;
      default_ic[i]=0.0;
    }
 for(i=0;i<nfix;i++){
   if(add_var(fnames[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",fnames[i]);
	xpp_model_failed();
      }
 }
 for(i=0;i<nmark;i++){
   if(add_var(mnames[i].c_str(),0.0)){
	xpp::log(XPP_LOG_ERROR, " Duplicate name {} \n",mnames[i]);
	xpp_model_failed();
      }
   XPP_FORMAT_TO_BUF(uvar_names[i+nvar],"{}",mnames[i]);
   last_ic[i+nvar]=0.0;
   default_ic[i+nvar]=0.0;
 }
 for(i=0;i<naux;i++)
   aux_names[i]=anames[i];
 add_svar_names();

/* NODE = nvars ; Naux = naux ; NEQ = NODE+NMarkov+Naux ; FIX_VAR = nfix; */

 IN_VARS=nvar;
 Naux=naux;
 NEQ=nvar+NMarkov+Naux;
 FIX_VAR=nfix;
 NTable=ntab;
 NFUN=nufun;

/* Reset all this stuff so we align the indices correctly */

 nvar=0;
 naux=0;
 ntab=0;
 nufun=0;
 nfix=0;
 nmark=0;

 for(VAR_INFO &v : model_lines)
   {
     if(v.type==COMMAND && v.lhs[0]=='I'){
      std::string big="i "+v.rhs+" \n";
      xpp::Tokens tokens(big);
      tokens.next(" ,");
      std::string_view values=tokens.rest();
      for(std::optional<std::string> tok;(tok=get_next2(values));)
	{
	   tmp=converted(take_apart(*tok,&z));
	   in=find_the_name(vnames,IN_VARS,tmp);
	   if(in>=0){
	     last_ic[in]=z;
	     default_ic[in]=z;
	     set_val(tmp.c_str(),z);
	     xpp::log(XPP_LOG_INFO, " Initial {}(0)={:g}\n",tmp,z);
	   }
	   else {
	     in=find_the_name(mnames,NMarkov,tmp);
	     if(in>=0){
	       last_ic[in+IN_VARS]=z;
               default_ic[in+IN_VARS]=z;
	       set_val(tmp.c_str(),z);
	       xpp::log(XPP_LOG_INFO, " Markov {}(0)={:g}\n",tmp,z);
	     }
	     else
	       {
		 xpp::log(XPP_LOG_ERROR, "In initial value statement no variable {} \n",
			tmp);
		 xpp_model_failed();
	       }
	   }
	 } /* end take apart */
     }  /* end  init  command    */
     if(v.type==IC){
       tmp=converted(v.lhs);
       fon=formula_or_number(v.rhs.c_str(),&z);

	  if(fon==1){

	 if(char_at(v.rhs,0)=='-'&&(isdigit(char_at(v.rhs,1))||(char_at(v.rhs,1)=='.')))
	   {

	     z=atof(v.rhs.c_str());

	   }
       }

       in=find_the_name(vnames,IN_VARS,tmp);
       if(in>=0){
	 last_ic[in]=z;
         default_ic[in]=z;
	 set_val(tmp.c_str(),z);
	   delay_string[in]=v.rhs;

	 xpp::log(XPP_LOG_INFO, " Initial {}(0)={}\n",tmp,v.rhs);
       }
       else {
	 in=find_the_name(mnames,NMarkov,tmp);
	 if(in>=0){
	   last_ic[in+IN_VARS]=z;
           default_ic[in+IN_VARS]=z;
	   set_val(tmp.c_str(),z);
	   xpp::log(XPP_LOG_INFO, " Markov {}(0)={:g}\n",tmp,z);
	 }
	 else
	   {
	     xpp::log(XPP_LOG_ERROR, "In initial value statement no variable {} \n",
		    tmp);
	     xpp_model_failed();
	   }
       }
     } /* end IC stuff  */

 /*   all that is left is the right-hand sides !!   */
     iflag=0;
     switch(v.type){
     case VEQ:
       iflag=1;
       [[fallthrough]];
     case ODE:
     case MAP:
       EqType[nvar]=iflag;
       set_ode_name(nvar,v.rhs);
       new_program(nvar);
       find_ker(v.rhs,&alt);
       if(add_expr(v.rhs.c_str(),my_ode[nvar],&leng[nvar])){
	 xpp::log(XPP_LOG_ERROR, "ERROR compiling {}' \n",v.lhs);
	 xpp_model_failed();
       }
       if(v.type==MAP){
	 xpp::log(XPP_LOG_INFO, "{}(t+1)={}\n",v.lhs,v.rhs);
	 is_a_map=1;
       }
       if(v.type==VEQ)
	 xpp::log(XPP_LOG_INFO, "{}(t)={}\n",v.lhs,v.rhs);
       if(v.type==ODE)
	 xpp::log(XPP_LOG_INFO, "{}:d{}/dt={}\n",nvar,v.lhs,v.rhs);
       nvar++;
       break;
      case FIXED:
       find_ker(v.rhs,&alt);
       new_program(nfix+IN_VARS);
       if(add_expr(v.rhs.c_str(),my_ode[nfix+IN_VARS],&leng[IN_VARS+nfix])!=0){
	 xpp::log(XPP_LOG_ERROR, " Error allocating or compiling {}\n",v.lhs);
	 xpp_model_failed();
       }
       nfix++;
       xpp::log(XPP_LOG_INFO, "{}={}\n",v.lhs,v.rhs);
       break;
     case DAE:
       if(add_aeqn(v.rhs.c_str())==1)
	 xpp_model_failed();
       xpp::log(XPP_LOG_INFO, " DAE eqn: {}=0 \n",v.rhs);
       break;

     case  AUX_VAR:
       in1=IN_VARS+NMarkov+naux;
       in2=IN_VARS+FIX_VAR+naux;
       set_ode_name(in1,v.rhs);
       new_program(in2);
       if(add_expr(v.rhs.c_str(),my_ode[in2],&leng[in2])){
	 xpp::log(XPP_LOG_ERROR, "ERROR compiling {} \n",v.lhs);
	 xpp_model_failed();
       }
       naux++;
       xpp::log(XPP_LOG_INFO, "{}={}\n",v.lhs,v.rhs);
       break;
     case VECTOR:
       ok=add_vectorizer(v.lhs.c_str(),v.rhs.data());
       resync(v);
       if(ok==0){
	 xpp::log(XPP_LOG_ERROR, " Illegal vector  {} \n",v.rhs);
	 xpp_model_failed();
       }

       break;
     case SPEC_FUN:
       ok=add_spec_fun(v.lhs.c_str(),v.rhs.data());
       resync(v);
       if(ok==0){
	 xpp::log(XPP_LOG_ERROR, " Illegal special function {} \n",v.rhs);
	 xpp_model_failed();
       }
       break;
     case MARKOV_VAR:
       set_ode_name(IN_VARS+nmark,v.rhs);
       nmark++;
       xpp::log(XPP_LOG_INFO, "{}: {}",v.lhs,v.rhs);
       break;
     case  FUNCTION:
       if(add_ufun_new(nufun,v.rhs.c_str(),v.args)!=0){
	 xpp::log(XPP_LOG_ERROR, " Function {} messed up \n",v.lhs);
	 xpp_model_failed();
       }
       nufun++;
       xpp::log(XPP_LOG_INFO, "{}({}",v.lhs,v.args.empty()?std::string():v.args[0]);
       for(size_t a=1;a<v.args.size();a++)
	 xpp::log(XPP_LOG_INFO, ",{}",v.args[a]);
       xpp::log(XPP_LOG_INFO, ")={}\n",v.rhs);
       break;

     case TABLE:
       {
       std::string big="t "+v.lhs+" "+v.rhs+" ";
       xpp::Tokens tokens(big);
       tokens.next(" ,");
       tokens.next(" ");
       formula=tokens.text(" \n");
       if(formula[0]=='%') {
	 xpp_log(XPP_LOG_INFO, " Function form of table....\n");
	 nn=atoi_of(tokens.text(" "));
	 xlo=atof_of(tokens.text(" "));
	 xhi=atof_of(tokens.text(" "));
	 formula=tokens.text("\n");
	 xpp::log(XPP_LOG_INFO, " {} has {} pts from {:f} to {:f} = {}\n",
		v.lhs,nn,xlo,xhi,formula);
	 if(add_form_table(ntab,nn,xlo,xhi,formula.c_str())){
	   xpp::log(XPP_LOG_ERROR, "ERROR computing {}\n",v.lhs);
	   xpp_model_failed();
	 }
	 ntab++;
       }
       else
	 if(formula[0]=='@'){
	   xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	   formula=tokens.text(" ");
	   xpp::log(XPP_LOG_INFO, " {} = {} \n",v.lhs,formula);
	   if(add_2d_table(v.lhs.c_str(),formula.c_str())){
	     xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	     xpp_model_failed();
	   }
	 }
	 else
	   {
	     xpp::log(XPP_LOG_INFO, "Lookup table {} = {} \n",v.lhs,formula);

	     if(add_file_table(ntab,formula.c_str())){
	       xpp::log(XPP_LOG_ERROR, "ERROR computing {}",v.lhs);
	       xpp_model_failed();
	     }
	     ntab++;
	   }
       }
       break;
     }
   }
 if(compile_derived()==1)
   xpp_model_failed();
 if(compile_svars()==1)
   xpp_model_failed();
 evaluate_derived();
 do_export_list();
 xpp_log(XPP_LOG_INFO, " All formulas are valid!!\n");
 NODE=nvar+naux+nfix;
 xpp_log(XPP_LOG_INFO, " nvar=%d naux=%d nfix=%d nmark=%d NEQ=%d NODE=%d \n",
	nvar,naux,nfix,nmark,NEQ,NODE);

}

int parse_model(FILE *fp, const std::string &first, int nnn, bool at_end)
{
 VAR_INFO v;
 std::vector<std::string> strings; /* this line, or a for loop's lines */
 int ns;
 int done=0,start=0,i1,i2,istates;
 int jj1=0,jj2=0,jj,notdone=1,jjsgn=1;
 std::string name;
 int nstates=0;
 std::string newfile;
 /* the line read, with its array range worked out, and one of its
    strings with its subscripts worked out (parse_a_string edits it in
    place, never longer) */
 std::string old,newstr,big;
 /* a Markov line's states as read and with their subscripts worked out */
 std::vector<std::string> markov_states,markov_states2;
 int is_array=0;
 /* the next line of fp into s; at_end once the file's end was met */
 auto next_line=[fp,&at_end](std::string &s){
   if(!read_a_line(fp,s))at_end=true;
 };
 if(nnn==0){model_lines.clear();}
 while(notdone){
   strings.clear();
   if(start||nnn==1){
     next_line(old);
   }
   else {
        if(loadincludefile)
	{
		loadincludefile=0;/*Only do this once*/
		for (const std::string &inc : include_files)
		{
			xpp::UniqueFile fnew=xpp::open_read(inc.c_str());
      			if(!fnew){
         		  xpp::log(XPP_LOG_ERROR, "Can't open include file <{}>\n",inc);
			  exit(-1);
       			}
      			xpp::log(XPP_LOG_INFO, "Including {} \n",inc);
			IN_INCLUDED_FILE++;
       			do_new_parser(fnew.get(),inc,1,false);
		}
	}

     old=first; /* pass the first line ....  */
     start=1;
   }
   if (IN_INCLUDED_FILE > 0)
    {
	    if (if_end_include(old) || at_end)
	    {
	    	xpp::log(XPP_LOG_INFO, "Completed include of file {}\n",first);
	    	IN_INCLUDED_FILE--;
	    	return 1;
	    }
    }
    if(if_include_file(old,newfile)){
      xpp::UniqueFile fnew=xpp::open_read(newfile.c_str());
      if(!fnew){
         xpp::log(XPP_LOG_WARN, "Cant open include file <{}>\n",newfile);
         continue;
       }
       xpp::log(XPP_LOG_INFO, "Including {}...\n",newfile);
       IN_INCLUDED_FILE++;
       do_new_parser(fnew.get(),newfile,1,false);
       fnew.reset();
       if (IN_INCLUDED_FILE <= 0)
             continue;
    }

    search_array(old.data(),newstr,&jj1,&jj2,&is_array);
   jj=jj1;
   jjsgn=1;
   if(jj2<jj1)jjsgn=-1;

   switch(is_array){
     case 0:  /*  not a for loop so */
     case 1:
           strings.assign(1,newstr);
           break;
      case 2: /*  a for loop, so we will ignore the first line */
            while(1){
             next_line(old);
             if(old[0]=='%')
               break;
             strings.push_back(old);
             if(strings.size()>255)break;
             }

            break;
       }

   while(1){
      for(ns=0;ns<static_cast<int>(strings.size());ns++){
      subsk(strings[ns].c_str(),big,jj,is_array);

   done=parse_a_string(big,v);

   if(done==-1){
     xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",big.c_str());
     return -1;
   }
   if(done==1){
     if(v.type==COMMAND)strupr(v.lhs.data());
     if(v.type==COMMAND && char_at(v.lhs,0)=='G' && char_at(v.lhs,1)=='R') {
       xpp::Tokens tokens(v.rhs);
       name=tokens.text(" ");
       std::optional<std::string_view> parts=tokens.next(" \n");
       nstates=parts?atoi_of(*parts):0;
       if(nstates<1){
	 xpp::log(XPP_LOG_ERROR, "Group {}  must have at least 1 part \n",name);
	 return -1;
       }
       xpp::log(XPP_LOG_INFO, "Group {} has {} parts\n",name,nstates);
       for(istates=0;istates<nstates;istates++){
	 next_line(old);
	 xpp::log(XPP_LOG_DEBUG, "part {} is {} \n",istates,old);
       }

       v.type=GROUP;
     }
   /* check for Markov to get rid of extra lines */

     if(v.type==COMMAND && char_at(v.lhs,0)=='M' && char_at(v.lhs,1)=='A'){
       xpp::Tokens tokens(v.rhs);
       name=tokens.text(" ");
       std::optional<std::string_view> count=tokens.next(" \n");
       nstates=count?atoi_of(*count):0;
       if(nstates<2){
	 xpp::log(XPP_LOG_ERROR, "Markov variable {}  must have at least 2 states \n",name);
	 return -1;
       }
       if(name_too_long(name.c_str()))return -1;
       add_markov(nstates,name.c_str());
       if(jj==jj1) {  /* test to see if this is the first one */
	 markov_states.assign(nstates,std::string());
	 for(istates=0;istates<nstates;istates++){
           if(is_array==2)
	     markov_states[istates]=strings[ns+1+istates];
	   else
	     next_line(markov_states[istates]);
	 }
       }

       /*  now we clean up these arrays */
       markov_states2.assign(nstates,std::string());
       std::vector<const char *> states(nstates);
       for(istates=0;istates<nstates;istates++){
	 subsk(markov_states[istates].c_str(),markov_states2[istates],jj,is_array);
	 states[istates]=markov_states2[istates].c_str();
       }

       build_markov(states.data(),name.c_str());
       v.type=MARKOV_VAR;
       v.lhs=name;
       v.rhs="...many states..";
     }

        /* take care of special form for SOLVE-VARIABLE */
          if(v.type==COMMAND && char_at(v.lhs,0)=='S' && char_at(v.lhs,1)=='O'){
           if(find_char(v.rhs.c_str(),"=",0,&i1)<0){
             v.lhs=v.rhs;
             v.rhs="0";
            }
          else
	    split_rhs(v,i1,i1+1);
          v.type=SOL_VAR;
     }

   /* take care of special form for auxiliary */
     if(v.type==COMMAND && char_at(v.lhs,0)=='A' && char_at(v.lhs,1)=='U'){
       if(find_char(v.rhs.c_str(),"=",0,&i1)>=0)
	 split_rhs(v,i1,i1+1);
       v.type=AUX_VAR;
     }

     /* take care of special form for vector */
     if(v.type==COMMAND && char_at(v.lhs,0)=='V' && char_at(v.lhs,1)=='E' && char_at(v.lhs,5)=='R')
     {
      if(find_char(v.rhs.c_str(),"=",0,&i1)>=0)
	split_rhs(v,i1,i1+1);
       v.type=VECTOR;
     }
        /* take care of special form for special */
     if(v.type==COMMAND && char_at(v.lhs,0)=='S'&&char_at(v.lhs,1)=='P'&&char_at(v.lhs,5)=='A'){
       if(find_char(v.rhs.c_str(),"=",0,&i1)>=0)
	 split_rhs(v,i1,i1+1);
       v.type=SPEC_FUN;
     }

/*   import-export to external C program   */
     if(v.type==COMMAND && char_at(v.lhs,0)=='E' && char_at(v.lhs,1)=='X'){
       v.type=EXPORT;
       if(find_char(v.rhs.c_str(),"}",0,&i1)>=0)
	 split_rhs(v,i1+1,i1+1);
    }

/*  ONLY save options  */

    if(v.type==COMMAND && char_at(v.lhs,0)=='O' && char_at(v.lhs,1)=='N')
    {
      break_up_list(v.rhs);
      v.type=ONLY;
     }

 /*  forced integral equation form */
     if(v.type==COMMAND && char_at(v.lhs,0)=='V'){
       if(find_char(v.rhs.c_str(),"=",0,&i1)>=0)
	 split_rhs(v,i1,i1+1);
       v.type=VEQ;
     }
    /* take care of tables   */

     if(v.type==COMMAND && char_at(v.lhs,0)=='T' && char_at(v.lhs,1)=='A'){
      int i0=0;
      next_nonspace(v.rhs.c_str(),i0,&i1);
      i0=i1;
      i2=find_char(v.rhs.c_str()," ",i0,&i1);
      if(i2!=0){
	xpp::log(XPP_LOG_WARN, " Illegal definition of table {} \n",v.rhs);
	xpp_model_failed();
      }
      std::string rest=v.rhs;
      v.lhs=rest.substr(i0,i1-i0);
      v.rhs=rest.substr(i1+1);
      v.type=TABLE;
    }

    add_varinfo(v);
      }
   } /* end loop for the strings */
   if(done==2)notdone=0;
   if(at_end)
   	notdone=0;

   if(jj==jj2)break;

     jj+=jjsgn;

   }

   /* a Markov line's states (build_markov copied them) */
   markov_states.clear();
   markov_states2.clear();

 }
 compile_em();

 free_varinfo();
 return 1;

}

} // namespace

void create_plot_list()
{
  int k;
  if(onlylist.empty())return;
  plot_columns.assign(onlylist.size()+1,0);
  plotlist=plot_columns.data();
  N_plist=0;
  for(const std::string &name : onlylist){
    find_variable(name.c_str(),&k);
    if(k>=0){
      plotlist[N_plist]=k;
      N_plist++;
    }
  }
}

int find_char(const char *s1, const char *s2, int i0, int *i1)
{
  int m=strlen(s2),n=strlen(s1);
  int i=i0;
  char ch;
  int j;
  while(i<n){
    ch=s1[i];
    for(j=0;j<m;j++){
      if(ch==s2[j]){
	*i1=i;
	return(j);
      }
    }
    i++;
  }
  return(-1);
}

/* old with its array range x[i..j] made x[j] (i1, i2 the range; flag 1,
   or 2 for a %[i..j] for loop): newstr. 0 (newstr old) when the range
   is malformed. A line of initial data x[..](0)=... goes to
   extract_ic_data, which may rewrite old. */
int search_array(char *old, std::string &newstr, int *i1, int *i2, int *flag)
{
  int i,j;
  int ileft,iright;
  int n=strlen(old);
  std::string num1="0",num2="0";
  char ch,chp;
  ileft=n-1;
  iright=-1;
  *i1=0;
  *i2=0;
  *flag=0;
  if(old[0]=='#'||(n>0&&old[1]=='#')) {  /* check for comments */
    newstr=old;
    return 1;
  }
  if(check_if_ic(old)==1){
    extract_ic_data(old);
    newstr=old;
    return 1;
  }
  for(i=0;i<n;i++){
    ch=old[i];
    chp=old[i+1];
    if(ch=='.'&&chp=='.'){
      j=0;
      *flag=1;
      if(old[0]=='%')
	*flag=2;   /*   FOR LOOP CONSTRUCTION  */
      while(1){
	ch=old[i+j];
	if(ch=='['){
	  ileft=i+j;
	  num1.assign(old+i+j+1,old+i);
	  break;
	}
	j--;
	if((i+j)<=0){
	  *i1=0;
          *i2=0;
	  newstr=old;
          xpp_log(XPP_LOG_WARN, " Possible error in array %s -- ignoring it \n",old);
	  return(0); /* error in array  */
	}
      }
      j=2;
      while(1){
	ch=old[i+j];
	if(ch==']'){
	  iright=i+j;
	  num2.assign(old+i+2,old+i+j);
	  break;
	}
	j++;
	if((i+j)>=n) {
	  *i1=0;
          *i2=0;
	  newstr=old;
          xpp_log(XPP_LOG_WARN, " Possible error in array  %s -- ignoring it \n",old);
	  return(0); /* error again   */
	}
      }
    }
  }
  *i1=atoi(num1.c_str());
  *i2=atoi(num2.c_str());
  /* now we have the numbers and will get rid of the junk inbetween */
  newstr.assign(old,old+ileft+1);
  if(iright>0){
    newstr+='j';
    newstr.append(old+iright,old+n);
  }
  return 1;
}

/* big with its subscripts worked out for index k: [n] becomes n, [j+n]
   k+n, [j-n] k-n, [j*n] k*n ([j] only in an array line, flag nonzero) */
void subsk(const char *big, std::string &newstr, int k, int flag)
{
  int n=strlen(big),i=0,add,isign,multflag=0;
  bool ok;
  char ch,chp;
  std::string num;
  newstr.clear();
  if(is_comment(big)){
    newstr=big;
    return;
  }
  /* the subscript's text runs to its ']' */
  auto unterminated=[big](){
    xpp_log(XPP_LOG_ERROR, "Error in %s The expression does not terminate. Perhaps a ] is missing.\n",big);
    xpp_model_failed();
  };
  while(i<n){
    ch=big[i];
    chp=big[i+1];
    if(ch=='['&&chp != 'j'&&not_ker(big,i)){
      ok=true;
      num.clear();
      i++;
      while(ok){
	if(i>=n)unterminated();
	ch=big[i];
	i++;
	if(ch==']'){
	  add=atoi(num.c_str());
	  newstr+=std::to_string(add);
	  ok=false;
	}
	else
	  num+=ch;
      }
    }
    else if(ch=='['&&chp=='j'){
      if(flag==0){
	xpp_log(XPP_LOG_WARN, " Illegal use of [j] at %s \n",big);
	xpp_model_failed();
      }
      num.clear();
      isign=1;
      i+=2;
      ok=true;
      while(ok){
	if(i>=n)unterminated();
	ch=big[i];
	switch(ch){
	case '+':
	  isign=1;
	  i++;
	  break;
	case '-':
	  isign=-1;
	  i++;
	  break;
	case '*':
	  i++;
	  isign=1;
	  multflag=1;
	  break;
	case ']':
	  i++;
	  if(multflag==0){
	    add=atoi(num.c_str())*isign+k;
	  }
	  else {
	    add=atoi(num.c_str())*k;
	    multflag=0;
	  }
	  newstr+=std::to_string(add);
	  ok=false;
	  break;
	default:
	  i++;
	  num+=ch;
	  break;
	}
      }
    }
    else {
      newstr+=ch;
      i++;
    }
  }
}

