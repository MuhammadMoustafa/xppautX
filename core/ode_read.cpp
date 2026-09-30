/* The .ode reader (ode_read.h, odex.h): an .ode file's lines to the
   statement list the Model builder (form_ode.cpp's build_model) makes the
   Model from, the builder an .odex model's statements go through too
   (docs/odex.md question 10). Every quirk of .ode's reading is here and
   kept, since an .ode gives XPPAUT's numbers: its lines joined at a
   backslash, #include, arrays and for loops worked out, a line told apart
   by its shape (parse_a_string) and a command by its first letters, a
   list of name=value items read with get_next2 and atof, a global's
   events split at every = and ;, x(0)=formula the number at the
   formula's front (or 0) as the initial value and the formula as the
   history. A formula stays .ode text (Expr::Kind::Text), which the
   expression engine compiles as written, .ode's precedence and all. An
   old-style model (the number of equations on its first line) is read
   and built line by line (form_ode.cpp's build_old_style). */
#include "ode_read.h"
#include "comline.h"
#include "expr.h"
#include "flags.h"
#include "form_ode.h"
#include "integrate.h"
#include "markov.h"
#include "model.h"
#include "model_files.h"
#include "newpars.h"
#include "odex.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using xpp::odex::Binding;
using xpp::odex::Expr;
using xpp::odex::Parsed;
using xpp::odex::Statement;

namespace {

/* the files being included: more than 0 inside one */
int IN_INCLUDED_FILE=0;

/* one line of a model as parse_a_string splits it: its kind (newpars.h),
   the text left and right of its '=' and a function's argument names */
struct VAR_INFO {
  int type=0;
  std::string lhs,rhs;
  std::vector<std::string> args;
};

/* where a model's lines come from: the file, its name and its index in
   Parsed::files, how many lines were read from it and the line the last
   logical line (read_a_line) began at; and the loading Model, whose
   files an include reads and whose source keeps the lines (save_line) */
struct LineSource {
  FILE *fp=nullptr;
  xpp::Model *model=nullptr;
  std::string file;
  int index=0;
  int lines=0;
  int line=0;
};


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

} // namespace

std::vector<OdeItem> ode_items(std::string_view rhs)
{
  std::vector<OdeItem> out;
  for(std::optional<std::string> tok;(tok=get_next2(rhs));){
    OdeItem item;
    item.name=take_apart(*tok,&item.value);
    size_t k=tok->find('=');
    if(k!=std::string::npos)item.text=trimmed(std::string_view(*tok).substr(k+1));
    out.push_back(std::move(item));
  }
  return out;
}

namespace {

/* the model's source line: fgets without its size, the line with its
   '\n' ("" at the end of the file); false once the end was met, what
   feof(fp) says after it */
bool read_raw_line(LineSource &src, std::string &line)
{
  line.clear();
  FILE *fp=src.fp;
  int c;
  while((c=getc(fp))!=EOF){
    line+=static_cast<char>(c);
    if(c=='\n'){
      src.lines++;
      return true;
    }
  }
  if(!line.empty())src.lines++;
  return false;
}

/* keeps one line of the model's source in source (Model::source), up
   to a NUL (the front ends read it as text) */
void save_line(std::vector<std::string> &source, const std::string &line)
{
  if (source.size()>=MAXLINES) {
    xpp_log(XPP_LOG_ERROR, "The model has more than %d lines\n", MAXLINES);
    xpp_model_failed();
  }
  source.push_back(line.substr(0,line.find('\0')));
}

/* The next logical line: a line ending in a backslash goes on in the
   next one (the text from the first backslash on is dropped); the
   line's end becomes a blank and one more blank follows it. false once
   the end of the file was met. */
bool read_a_line(LineSource &src, std::string &s)
{
  bool more=true,in_file=true;
  s.clear();
  src.line=src.lines+1;
  while(more){
    std::string temp;
    in_file=read_raw_line(src,temp)&&in_file;
    save_line(src.model->source,temp);
    size_t hat=temp.find('\\');
    more=hat!=std::string::npos;
    if(more)temp.resize(hat);
    s+=temp;
  }
  if(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.back()=' '; /* empty at the end of the file */
  s+=' ';
  return in_file;
}

/* "#include file": the file's name, blanks removed, into nf */
bool if_include_file(const std::string &old, std::string &nf)
{
  std::string_view s(old);
  if(!s.starts_with("#include"))return false;
  size_t blank=s.find(' ');
  if(blank==std::string_view::npos)return false;
  nf=s.substr(blank+1);
  de_space(nf);
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

int parse_model(LineSource &src, const std::string &first, int nnn, bool at_end, Parsed &p);

/* no exception crosses into C: the only one parse_model() can throw is
   std::bad_alloc, and running out of memory ends the program, as
   xpp_malloc() does */
int do_new_parser(LineSource &src, const std::string &first, int nnn, bool at_end, Parsed &p)
{
  try {
    return parse_model(src, first, nnn, at_end, p);
  } catch (const std::bad_alloc &) {
    xpp::log(XPP_LOG_ERROR, "out of memory reading {}\n", first);
    exit(1);
  }
}

/* an only line's names: the words between blanks and commas */
std::vector<std::string> break_up_list(std::string_view rhs)
{
  std::vector<std::string> names;
  std::string s;
  for(char c : rhs){
    if(c=='\0')break;
    if(c==' '||c==','){
      if(!s.empty())names.push_back(s);
      s.clear();
    }
    else
      s+=c;
  }
  if(!s.empty())names.push_back(s);
  return names;
}

/* this code checks if the right-hand side for an initial
   condition is a formula (for delays) or a number
*/
int formula_or_number(const char *expr,double *z)
{
  std::array<char,40> num{}; /* do_num's 40 bytes */
  int flag,i=0;
  *z=0.0; /* initial it to 0 */
  const std::string form=converted(expr);
  flag=do_num(form.c_str(),num.data(),z,&i,0); /* a formula is no error here */
  if(i<static_cast<int>(form.size()))flag=1;
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

/* the names in s1 from i0 up to its ')' into args; *ie where the
   formula after the '=' starts. 0 when they are not there. */
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

/* The line s1 (made upper case, its leading blanks removed) as v: 1, 2
   for "done", 0 for a line that is not one (a comment or an option,
   their statements added to out as they are), -1 when it cannot be read */
int parse_a_string(std::string &s1, VAR_INFO &v, std::vector<Statement> &out)
{
  int i0=0,i1,i2,i3;
  std::string lhs,rhs;
  std::vector<std::string> args;
  int type,type2;
  if(char_at(s1,0)=='"'||char_at(s1,0)=='@'){
    Statement s;
    s.kind=s1[0]=='"'?Statement::Kind::Comment:Statement::Kind::Options;
    s.text=s1[0]=='"'?s1.substr(1):s1;
    out.push_back(std::move(s));
    return 0;
  }
  remove_blanks(s1);

  const std::string s1old=s1;
  xpp::to_upper(s1.data());
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

/* ---- a line's statements ---- */

Expr text_expr(std::string text)
{
  Expr e;
  e.kind=Expr::Kind::Text;
  e.text=std::move(text);
  return e;
}

Binding text_binding(std::string name, std::string text)
{
  Binding b;
  b.name=std::move(name);
  b.value=text_expr(std::move(text));
  return b;
}

/* the name=value items after a command's first word in big, each value
   the number atof reads from it, its text as written */
std::vector<Binding> number_items(const std::string &big)
{
  xpp::Tokens tokens(big);
  tokens.next(" ,");
  std::vector<Binding> out;
  for(OdeItem &item : ode_items(tokens.rest())){
    Binding b;
    b.name=std::move(item.name);
    b.value.kind=Expr::Kind::Number;
    b.value.value=item.value;
    b.value.text=std::move(item.text);
    out.push_back(std::move(b));
  }
  return out;
}

/* a command line (its first word v.lhs, in upper case; the rest v.rhs as
   typed), told apart by its first letters, as XPP does: par, wiener,
   number, init, the options file, set, bdry and global; any other is
   nothing */
void command(const VAR_INFO &v, std::vector<Statement> &out)
{
  const char c0=char_at(v.lhs,0),c1=char_at(v.lhs,1);
  Statement s;
  if(c0=='P'||c0=='W'||c0=='N'||c0=='I'){
    s.kind=c0=='P'?Statement::Kind::Par:c0=='W'?Statement::Kind::Wiener:
      c0=='N'?Statement::Kind::Const:Statement::Kind::InitNumbers;
    s.bindings=number_items("x "+v.rhs+" \n");
  }
  else if(c0=='O'){
    s.kind=Statement::Kind::OptionFile;
    const std::string big="c "+v.rhs+" \n";
    xpp::Tokens tokens(big);
    tokens.next(" ,");
    s.text=tokens.text(" \n");
  }
  else if(c0=='S'&&c1=='E'){
    s.kind=Statement::Kind::Set;
    const std::string big="x "+v.rhs+"\n";
    xpp::Tokens tokens(big);
    tokens.next(" ,");
    s.name=tokens.text("{ ");
    s.text=tokens.text("}\n");
  }
  else if(c0=='B'){
    s.kind=Statement::Kind::Boundary;
    const std::string big="b "+v.rhs+" \n";
    xpp::Tokens tokens(big);
    tokens.next(" ,");
    s.expr=text_expr(tokens.text("\n"));
  }
  else if(c0=='G'){
    /* global sign {condition} {name=formula;...} */
    s.kind=Statement::Kind::Event;
    const std::string big="g "+v.rhs+" \n";
    xpp::Tokens tokens(big);
    tokens.next(" ,");
    s.count=atoi_of(tokens.text("{ "));
    const std::string cond=tokens.text("{}");
    const std::string events=tokens.text("\n");
    std::vector<FlagEvent> split;
    if(split_events(cond.c_str(),events.c_str(),split)){
      xpp_log(XPP_LOG_WARN, "Bad global !! \n");
      xpp_model_failed();
    }
    s.expr=text_expr(cond);
    for(FlagEvent &e : split)s.bindings.push_back(text_binding(std::move(e.name),std::move(e.formula)));
  }
  else
    return;
  out.push_back(std::move(s));
}

/* x(0)=formula: x's initial value, the number at the formula's front
   (0 when there is none, a sign and a digit read as atof reads them),
   and x's history, the formula */
void initial(const VAR_INFO &v, std::vector<Statement> &out)
{
  double z;
  if(formula_or_number(v.rhs.c_str(),&z)==1&&char_at(v.rhs,0)=='-'&&
     (isdigit(static_cast<unsigned char>(char_at(v.rhs,1)))||char_at(v.rhs,1)=='.'))
    z=atof(v.rhs.c_str());
  Statement init;
  init.kind=Statement::Kind::InitNumbers;
  init.text=v.rhs;
  Binding b;
  b.name=v.lhs;
  b.value.kind=Expr::Kind::Number;
  b.value.value=z;
  b.value.text=v.rhs;
  init.bindings.push_back(std::move(b));
  out.push_back(std::move(init));
  Statement history;
  history.kind=Statement::Kind::History;
  history.bindings.push_back(text_binding(v.lhs,v.rhs));
  out.push_back(std::move(history));
}

/* table name file, table name % n lo hi formula, table name @ file */
void table(const VAR_INFO &v, Statement &s)
{
  s.kind=Statement::Kind::Table;
  s.name=v.lhs;
  const std::string big="t "+v.lhs+" "+v.rhs+" ";
  xpp::Tokens tokens(big);
  tokens.next(" ,");
  tokens.next(" ");
  std::string formula=tokens.text(" \n");
  if(formula[0]=='%'){
    s.table_kind=Statement::TableKind::Formula;
    s.count=atoi_of(tokens.text(" "));
    s.lo=atof_of(tokens.text(" "));
    s.hi=atof_of(tokens.text(" "));
    s.expr=text_expr(tokens.text("\n"));
  }
  else if(formula[0]=='@'){
    s.table_kind=Statement::TableKind::TwoD;
    s.text=tokens.text(" ");
  }
  else
    s.text=formula;
}

/* the line v, as parse_model made it, into out */
void add_statement(const VAR_INFO &v, std::vector<Statement> &out)
{
  Statement s;
  switch(v.type){
  case COMMAND: command(v,out); return;
  case IC: initial(v,out); return;
  case ODE:
  case MAP:
  case VEQ:
    s.kind=v.type==ODE?Statement::Kind::Ode:v.type==MAP?Statement::Kind::Map:Statement::Kind::Volterra;
    s.name=v.lhs;
    s.expr=text_expr(v.rhs);
    break;
  case FIXED:
  case SOL_VAR:
  case DAE:
    s.kind=v.type==FIXED?Statement::Kind::Fixed:v.type==SOL_VAR?Statement::Kind::Solv:Statement::Kind::Dae;
    s.name=v.lhs;
    s.expr=text_expr(v.rhs);
    break;
  case FUNCTION:
    s.kind=Statement::Kind::Fun;
    s.name=v.lhs;
    s.names=v.args;
    s.expr=text_expr(v.rhs);
    break;
  case AUX_VAR:
  case DERIVE_PAR:
    s.kind=v.type==AUX_VAR?Statement::Kind::Aux:Statement::Kind::Derived;
    s.bindings.push_back(text_binding(v.lhs,v.rhs));
    break;
  case TABLE: table(v,s); break;
  case SPEC_FUN:
  case VECTOR:
    s.kind=v.type==SPEC_FUN?Statement::Kind::Network:Statement::Kind::Vector;
    s.name=v.lhs;
    s.text=v.rhs;
    break;
  case ONLY:
    s.kind=Statement::Kind::Only;
    s.names=break_up_list(v.rhs);
    break;
  case GROUP: s.kind=Statement::Kind::Group; break;
  default: return; /* a Markov variable's: parse_model added it */
  }
  out.push_back(std::move(s));
}

int parse_model(LineSource &src, const std::string &first, int nnn, bool at_end, Parsed &p)
{
 std::vector<Statement> &out=p.statements;
 VAR_INFO v;
 std::vector<std::string> strings; /* this line, or a for loop's lines */
 std::vector<int> string_lines; /* where each of strings is in the file */
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
 auto next_line=[&src,&at_end](std::string &s){
   if(!read_a_line(src,s))at_end=true;
 };
 while(notdone){
   strings.clear();
   string_lines.clear();
   if(start||nnn==1){
     next_line(old);
   }
   else {
        if(loadincludefile)
	{
		loadincludefile=0;/*Only do this once*/
		for (const std::string &inc : include_files)
		{
			xpp::UniqueFile fnew=xpp::open_model_file(*src.model,inc);
      			if(!fnew){
         		  xpp::log(XPP_LOG_ERROR, "Can't open include file <{}>\n",inc);
			  xpp_model_failed();
       			}
      			xpp::log(XPP_LOG_INFO, "Including {} \n",inc);
			IN_INCLUDED_FILE++;
			LineSource inc_src;
			inc_src.fp=fnew.get();
			inc_src.model=src.model;
			inc_src.file=inc;
			inc_src.index=static_cast<int>(p.files.size());
			p.files.push_back(inc);
       			do_new_parser(inc_src,inc,1,false,p);
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
      xpp::UniqueFile fnew=xpp::open_model_file(*src.model,newfile);
      if(!fnew){
         xpp::log(XPP_LOG_WARN, "Cant open include file <{}>\n",newfile);
         continue;
       }
       xpp::log(XPP_LOG_INFO, "Including {}...\n",newfile);
       IN_INCLUDED_FILE++;
       LineSource inc_src;
       inc_src.fp=fnew.get();
       inc_src.model=src.model;
       inc_src.file=newfile;
       inc_src.index=static_cast<int>(p.files.size());
       p.files.push_back(newfile);
       do_new_parser(inc_src,newfile,1,false,p);
       fnew.reset();
       if (IN_INCLUDED_FILE <= 0)
             continue;
    }

    xpp::Load::at(src.file,src.line);
    search_array(old.data(),newstr,&jj1,&jj2,&is_array);
   jj=jj1;
   jjsgn=1;
   if(jj2<jj1)jjsgn=-1;

   switch(is_array){
     case 0:  /*  not a for loop so */
     case 1:
           strings.assign(1,newstr);
           string_lines.assign(1,src.line);
           break;
      case 2: /*  a for loop, so we will ignore the first line */
            while(1){
             next_line(old);
             if(old[0]=='%')
               break;
             strings.push_back(old);
             string_lines.push_back(src.line);
             if(strings.size()>255)break;
             }

            break;
       }

   /* each line's copies are one array's (Statement::array), numbered by
      where the first of them is in out */
   std::vector<int> groups(strings.size(),0);
   while(1){
      for(ns=0;ns<static_cast<int>(strings.size());ns++){
      xpp::Load::at(src.file,string_lines[ns]);
      const size_t first_new=out.size();
      subsk(strings[ns].c_str(),big,jj,is_array);

   done=parse_a_string(big,v,out);

   if(done==-1){
     xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",big.c_str());
     return -1;
   }
   if(done==1){
     if(v.type==COMMAND)xpp::to_upper(v.lhs.data());
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
       if(jj==jj1) {  /* test to see if this is the first one */
	 markov_states.assign(nstates,std::string());
	 for(istates=0;istates<nstates;istates++){
           if(is_array==2)
	     markov_states[istates]=strings[ns+1+istates];
	   else
	     next_line(markov_states[istates]);
	 }
       }

       /*  now we clean up these arrays: each row's cells, {formula} */
       Statement markov;
       markov.kind=Statement::Kind::Markov;
       markov.name=name;
       markov.count=nstates;
       markov_states2.assign(nstates,std::string());
       for(istates=0;istates<nstates;istates++){
	 subsk(markov_states[istates].c_str(),markov_states2[istates],jj,is_array);
	 int istart=0;
	 for(int k=0;k<nstates;k++)
	   markov.cells.push_back(text_expr(markov_cell(markov_states2[istates].c_str(),&istart)));
       }
       out.push_back(std::move(markov));
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

/*   export {inputs} {outputs} called a compiled library's function   */
     if(v.type==COMMAND && char_at(v.lhs,0)=='E' && char_at(v.lhs,1)=='X'){
       refuse_compiled_functions("export");
       xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",big.c_str());
       return -1;
     }

/*  ONLY save options  */

    if(v.type==COMMAND && char_at(v.lhs,0)=='O' && char_at(v.lhs,1)=='N')
    {
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

    add_statement(v,out);
      }
      /* the statements the line made are at it */
      for(size_t k=first_new;k<out.size();k++)
        if(out[k].pos.line==0)out[k].pos=xpp::odex::Pos{src.index,string_lines[ns],0};
      if(is_array&&out.size()>first_new){
        if(groups[ns]==0)groups[ns]=static_cast<int>(first_new)+1;
        for(size_t k=first_new;k<out.size();k++)
          out[k].array=xpp::odex::ArrayCopy{groups[ns],"j",jj,jj1,jj2,jjsgn,is_array==2&&strings.size()>1};
      }
   } /* end loop for the strings */
   if(done==2)notdone=0;
   if(at_end)
   	notdone=0;

   if(jj==jj2)break;

     jj+=jjsgn;

   }

   /* a Markov line's states (its statement copied them) */
   markov_states.clear();
   markov_states2.clear();

 }
 return 1;

}

} // namespace

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

int get_eqn(xpp::Session &s, FILE *fptr)
{
  xpp::Model &m=s.model();
  LineSource src;
  src.fp=fptr;
  src.file=m.this_file;
  src.model=&m;
  std::string first;
  m.source.clear();
  bool in_file=read_raw_line(src,first);
  src.line=1;
  save_line(m.source,first);
  const int neq=atoi(first.c_str());
  if(neq>0){ /* an old-style model: each line built as it is read */
    build_old_style(s,neq,fptr,[&src](std::string &line){
      read_raw_line(src,line);
      if(line.empty())return false;
      save_line(src.model->source,line);
      xpp::Load::at(src.file,src.lines);
      return true;
    });
    return 1;
  }
  Parsed p;
  p.files.push_back(src.file);
  if(do_new_parser(src,first,0,!in_file,p)<0)xpp_model_failed();
  build_model(s,std::move(p));
  return 1;
}
