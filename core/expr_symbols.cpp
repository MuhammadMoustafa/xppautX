/* The expression engine's symbol table (expr.h): the names a formula may
   use, what each compiles to, and the parameters' and variables' values
   by name. The table is a Session's (ParserState::symbols):
   its first STDSYM entries are the built-ins below, then come the
   model's names as the load adds them. */
#include "expr_internal.h"
#include "model.h"
#include "comline.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_math.h"
#include "tabular.h"
#include "getvar.h"

#include <cctype>
#include <cmath>
#include <ctime>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using xpp::expr::is_ucon;
using xpp::expr::is_uvar;
using xpp::expr::is_lookup;

namespace xpp {

#ifndef M_PI
# define M_PI	3.14159265358979323846264338327950288
#endif


namespace {

/* a built-in symbol: its name, length, what it compiles to, number of
   arguments and priority (ExprSymbol's fields); its index is its token */
struct Builtin {
  const char *name;
  int len;
  int com;
  int arg;
  int pri;
};

constexpr Builtin builtins[]=
{
   {"(",1,999,0,1},      /*  0   */
   {")",1,999,0,2},
   {",",1,999,0,3},
   {"+",1,COM(FUN2TYPE,0),0,4},
   {"-",1,COM(FUN2TYPE,1),0,4},
   {"*",1,COM(FUN2TYPE,2),0,6},
   {"/",1,COM(FUN2TYPE,3),0,6},
   {"^",1,COM(FUN2TYPE,5),0,7},
   {"**",2,COM(FUN2TYPE,5),0,7},
   {"~",1,COM(FUN1TYPE,14),0,6},
   {"START",5,-1,0,0},  /* 10  */
   {"END",3,999,0,-1},
   {"ATAN2",5,COM(FUN2TYPE,4),2,10},
   {"MAX",3,COM(FUN2TYPE,6),2,10},
   {"MIN",3,COM(FUN2TYPE,7),2,10},
   {"SIN",3,COM(FUN1TYPE,0),0,10},
   {"COS",3,COM(FUN1TYPE,1),0,10},
   {"TAN",3,COM(FUN1TYPE,2),0,10},
   {"ASIN",4,COM(FUN1TYPE,3),0,10},
   {"ACOS",4,COM(FUN1TYPE,4),0,10},
   {"ATAN",4,COM(FUN1TYPE,5),0,10},  /* 20  */
   {"SINH",4,COM(FUN1TYPE,6),0,10},
   {"TANH",4,COM(FUN1TYPE,7),0,10},
   {"COSH",4,COM(FUN1TYPE,8),0,10},
   {"ABS",3,COM(FUN1TYPE,9),0,10},
   {"EXP",3,COM(FUN1TYPE,10),0,10},
   {"LN",2,COM(FUN1TYPE,11),0,10},
   {"LOG",3,COM(FUN1TYPE,11),0,10},
   {"LOG10",5,COM(FUN1TYPE,12),0,10},
   {"SQRT",4,COM(FUN1TYPE,13),0,10},
   {"HEAV",4,COM(FUN1TYPE,16),0,10},  /*  30 */
   {"SIGN",4,COM(FUN1TYPE,17),0,10},
   {"#$%1",4,COM(USTACKTYPE,0),0,10},
   {"#$%2",4,COM(USTACKTYPE,1),0,10},
   {"#$%3",4,COM(USTACKTYPE,2),0,10},
   {"#$%4",4,COM(USTACKTYPE,3),0,10},
   {"#$%5",4,COM(USTACKTYPE,4),0,10},
   {"#$%6",4,COM(USTACKTYPE,5),0,10},
   {"#$%7",4,COM(USTACKTYPE,6),0,10},
   {"#$%8",4,COM(USTACKTYPE,7),0,10},
   {"FLR",3,COM(FUN1TYPE,18),0,10},  /*  40 */
   {"MOD",3,COM(FUN2TYPE,8),2,10}, /*  41 */
   {"DELAY",5,ENDDELAY,2,10},      /*  42 */   /*  Delay symbol */
   {"RAN",3,COM(FUN1TYPE,19),1,10}, /* 43 */
   {"&",1,COM(FUN2TYPE,9),0,6},  /* logical stuff  */
   {"|",1,COM(FUN2TYPE,10),0,4},
   {">",1,COM(FUN2TYPE,11),0,7},
   {"<",1,COM(FUN2TYPE,12),0,7},
   {"==",2,COM(FUN2TYPE,13),0,7},
   {">=",2,COM(FUN2TYPE,14),0,7},
   {"<=",2,COM(FUN2TYPE,15),0,7}, /*50 */
   {"IF",2,995,1,10}, 
   {"THEN",4,994,1,10},
   {"ELSE",4,993,1,10},
   {"!=",2,COM(FUN2TYPE,16),0,7},
   {"NOT",3,COM(FUN1TYPE,20),0,6},
   {"NORMAL",6,COM(FUN2TYPE,17),2,10}, /* returns normally dist number */
   {"BESSELJ",7,COM(FUN2TYPE,18),2,10}, /* Bessel J   */
   {"BESSELY",7,COM(FUN2TYPE,19),2,10}, /* Bessel Y */
   {"NXXQQ",5,NUMSYM,0,10},  
   {"ERF", 3, COM(FUN1TYPE,21),0,10}, /* 60 */
   {"ERFC",4,COM(FUN1TYPE,22),0,10},
   {"SUM",3,SUMSYM,2,10},
   {"OF",2,ENDSUM,0,10},
   {"SHIFT",5,ENDSHIFT,2,10}, 
   {"DEL_SHFT",8,ENDDELSHFT,3,10},/* 65 */
   {"HOM_BCS",7,COM(FUN1TYPE,23),0,10},
   {"ISHIFT",6,ENDISHIFT,2,10}, /* 67 */
   {"@",1,INDXCOM,0,10}, /*68 */
   {"]",1,ENDSHIFT,0,10},
   {"[",1,ENDSHIFT,0,10}, /*70 */
   {"POISSON",7,COM(FUN1TYPE,24),0,10}, /* 71 */
   {"SET",3,ENDSET,3,10}, /* 72 */
   {"ARG1",4,COM(USTACKTYPE,0),0,10}, /*  FIXXX ????  */
   {"ARG2",4,COM(USTACKTYPE,1),0,10},
   {"ARG3",4,COM(USTACKTYPE,2),0,10},
   {"ARG4",4,COM(USTACKTYPE,3),0,10},
   {"ARG5",4,COM(USTACKTYPE,4),0,10},
   {"ARG6",4,COM(USTACKTYPE,5),0,10},
   {"ARG7",4,COM(USTACKTYPE,6),0,10},
   {"ARG8",4,COM(USTACKTYPE,7),0,10},
   {"ARG9",4,COM(USTACKTYPE,8),0,10},
   {"ARG10",5,COM(USTACKTYPE,9),0,10},
   {"ARG11",5,COM(USTACKTYPE,10),0,10},
   {"ARG12",5,COM(USTACKTYPE,11),0,10},
   {"ARG13",5,COM(USTACKTYPE,12),0,10},
   {"ARG14",5,COM(USTACKTYPE,13),0,10},
   {"ARG15",5,COM(USTACKTYPE,14),0,10},
   {"ARG16",5,COM(USTACKTYPE,15),0,10},
   {"ARG17",5,COM(USTACKTYPE,16),0,10},
   {"ARG18",5,COM(USTACKTYPE,17),0,10},
   {"ARG19",5,COM(USTACKTYPE,18),0,10},
   {"ARG20",5,COM(USTACKTYPE,19),0,10},  
   {"BESSELI",7,COM(FUN2TYPE,20),2,10},/* Bessel I  # 93 */
   {"LGAMMA",6,COM(FUN1TYPE,25),1,10}, /* Log Gamma  #94 */
   {"BESSELIS",8,COM(FUN2TYPE,21),2,10},/* Bessel I Scaled  # 95 */
};
static_assert(std::size(builtins)==STDSYM,"STDSYM is the number of built-in symbols");

}

bool is_builtin_symbol(std::string_view name)
{
  for(const Builtin &b : builtins)
    if(name==b.name)return true;
  return false;
}

namespace {

/* 1 (with an INFO) when name is already a symbol */
int duplicate_name(const ParserState &p, std::string_view junk);
/* name's symbol index in *index (-1 when none) */
void find_name(const ParserState &p, std::string_view string, int *index);

}

ParserState::ParserState()
{
  for(int i=0;i<STDSYM;i++){
    const Builtin &b=builtins[i];
    symbols[i]={b.name,b.len,b.com,b.arg,b.pri};
  }
}

/*****************************
*
*     parses any algebraic expression
*     and converts to an integer array
*     to be interpreted by the rpe_val
*     function.
*
*     the main data structure is a contiguous
*     list of symbols with their priorities
*     and their symbol value
*
*  6/95  stuff added to add names to namelist without compilation
*************************************************************/

/* INIT_RPN    */
void init_rpn(xpp::Session &s)
{

    s.parser.errout = 1;
    s.parser.ncon = 0;
    s.model().nfun = 0;
    s.model().nvar = 0;
    s.model().nkernel=0;

    s.numerics.max_points=4000;
    s.parser.nsym = STDSYM;
    add_con(s,"PI", M_PI);

    /* I', the constant SUM sets: xpp::expr::SUM_INDEX */
        add_con(s,"I'",0.0);
    /*   This is going to be for interacting with the
         animator */
        add_con(s,"mouse_x",0.0);
        add_con(s,"mouse_y",0.0);
        add_con(s,"mouse_vx",0.0);
        add_con(s,"mouse_vy",0.0);

    /* end animator stuff */

    init_table(s);
    if (newseed==1) {
      s.numerics.rand_seed=static_cast<int>(time(0));
      xpp::log(XPP_LOG_INFO,"-newseed: seed {}\n",s.numerics.rand_seed);
    }
    xpp::nsrand48(s.numerics.rand_seed);
}

namespace {

int duplicate_name(const ParserState &p, std::string_view junk)
{
  int i;
  find_name(p,junk,&i);
  if(i>=0){
    /* INFO, not WARN: a plain "name=expr" followed by "aux name=name" (a
       common, intentional pattern -- lecar.ode does exactly this for ica)
       hits this every time and is not a mistake the user needs to act on;
       WARN would make --verbose-off runs noisy for a routine model shape. */
    if(p.errout)xpp::log(XPP_LOG_INFO, "{} is a duplicate name\n",junk);
    return(1);
  }
  return(0);
}

}

/* name as convert makes it: blanks removed, upper case (never longer) */
std::string converted(std::string_view name)
{
  std::string s;
  for(char ch : name){
    if(ch=='\0')break;
    if(!isspace(static_cast<unsigned char>(ch)))s+=ch;
  }
  xpp::to_upper(s.data());
  return s;
}

namespace {

/* the next symbol of the table: name (without blanks, in upper case, of
   any length), priority, number of arguments, what it compiles to.
   Returns 1 (and says why) when name is empty. The table's count (nsym)
   is the caller's to raise. */
int set_symbol(ParserState &p, std::string_view name, int pri, int arg, int com)
{
  std::string string=converted(name);
  int len=static_cast<int>(string.size());
  if(len<1){
    xpp::log_printf(XPP_LOG_WARN, "Empty name - remove spaces\n");
    return 1;
  }
  ExprSymbol &s=p.symbols[p.nsym];
  s.name=std::move(string);
  s.len=len;
  s.pri=pri;
  s.arg=arg;
  s.com=com;
  return 0;
}

}

namespace {

/*  ADD_CONSTANT: the name of the last constant added */
int add_constant(ParserState &p, std::string_view junk)
{
 if(duplicate_name(p,junk)==1)return(1);
 if(p.ncon>=MAXPAR)
 {
  if(p.errout)xpp::log_printf(XPP_LOG_WARN, "too many constants !!\n");
  return(1);
 }
 if(set_symbol(p,junk,10,0,COM(CONTYPE,p.ncon-1)))return 1;
 p.nsym++;
 return(0);
}

}

int get_var_index(const xpp::Session &s, std::string_view name)
{

  int type,com;
  find_name(s.parser,name,&type);
  if(type<0)return -1;
  com=s.parser.symbols[type].com;
  if(is_uvar(com))
  {
      return(com%MAXTYPE);
  }
  return(-1);
}

/*   ADD_CON      */

int add_con(xpp::Session &s, std::string_view name, double value)
{
 ParserState &p=s.parser;
 if(p.ncon>=MAXPAR)
 {
  if(p.errout)xpp::log_printf(XPP_LOG_WARN, "too many constants !!\n");
  return(1);
 }
 p.constants[p.ncon]=value;
 p.ncon++;
 return(add_constant(p,name));
}

int add_kernel(xpp::Session &s, std::string_view name, double mu, std::string_view expr)
{
  xpp::Model &m=s.model();
  int in=-1;
  if(duplicate_name(s.parser,name)==1)return(1);
  if(m.nkernel==MAXKER){
    xpp::log_printf(XPP_LOG_WARN, "Too many kernels..\n");
    return(1);
  }
  if(mu<0||mu>=1.0){
    xpp::log_printf(XPP_LOG_WARN, " mu must lie in [0,1.0) \n");
    return(1);
  }
  if(set_symbol(s.parser,name,10,0,COM(KERTYPE,m.nkernel)))return 1;
  KERNEL &k=m.kernels[m.nkernel];
  k.mu=mu;
  k.flag=0;
  std::string_view text=expr;
  size_t hash=text.rfind('#');
  if(hash!=std::string_view::npos)in=static_cast<int>(hash);
  if(in==0||in==static_cast<int>(text.size())-1){
    xpp::log_printf(XPP_LOG_WARN, "Illegal use of convolution...\n");
    return(1);
  }
  if(in>0){
    k.flag=CONV;
    /* split at the # */
    k.kerexpr=text.substr(0,in);
    k.expr=text.substr(in+1);
    xpp::log(XPP_LOG_INFO, "Convolving {} with {}\n",k.kerexpr,k.expr);
  }
  else {
    k.expr=text;
  }
  k.name=name;
  s.parser.nsym++;
  m.nkernel++;
  return(0);
}

/*  ADD_VAR          */

int add_var(xpp::Session &s, std::string_view junk, double value)
{
 ParserState &p=s.parser;
 xpp::Model &m=s.model();
 if(duplicate_name(p,junk)==1)return(1);
 if(m.nvar>=MAXODE1)
 {
  if(p.errout)xpp::log_printf(XPP_LOG_WARN, "too many variables !!\n");
  return(1);
 }
 if(set_symbol(p,junk,10,0,COM(VARTYPE,m.nvar)))return 1;
 p.nsym++;
 p.variables[m.nvar]=value;
 m.nvar++;
 return(0);
}

int add_net_name(xpp::Session &s, int index, std::string_view name, int vectorizer)
{
  xpp::log(XPP_LOG_INFO, " Adding {} {} {} \n",vectorizer?"vectorizer":"net",name,index);
  if(duplicate_name(s.parser,name)==1)return(1);
  if(set_symbol(s.parser,name,10,1,COM(vectorizer?VECTYPE:NETTYPE,index)))return 1;
  s.parser.nsym++;
  return(0);
}

/* ADD LOOKUP TABLE   */

int add_2d_table(std::string_view name, std::string_view file)
{
 xpp::log_printf(XPP_LOG_WARN, " TWO D NOT HERE YET \n");
 return(1);
}

xpp::Result<> add_file_table(xpp::Session &s, int index, std::string_view file)
{
  /* the name's printable characters */
  std::string file2;
  for(char c:file)
    if(c>31&&c<127)
      file2+=c;
  auto loaded=load_table(s,file2,index,1);
  if(!loaded&&s.parser.errout)xpp::log_printf(XPP_LOG_WARN, "Problem with creating table !!\n");
  return loaded;
}

int add_table_name(xpp::Session &s, int index, std::string_view name)
{
     if(duplicate_name(s.parser,name)==1)return(1);
     if(set_symbol(s.parser,name,10,1,COM(TABTYPE, index)))return 1;
     set_table_name(s,name,index);
     s.parser.nsym++;
     return(0);
   }
/* ADD LOOKUP TABLE   */

xpp::Result<> add_form_table(xpp::Session &s, int index, int nn, double xlo, double xhi, std::string_view formula)
{

  auto made=create_fun_table(s,nn,xlo,xhi,formula,index);
  if(!made&&s.parser.errout)xpp::log_printf(XPP_LOG_WARN, "Problem with creating table !!\n");
  return made;
}

namespace {

/* the symbols ARG1..ARGnarg back to their own names */
void set_old_arg_names(ParserState &p, int narg)
{
  int i;
  for(i=0;i<narg;i++){
    ExprSymbol &s=p.symbols[FIRST_ARG+i];
    s.name=xpp::format("ARG{}",i+1);
    s.len=static_cast<int>(s.name.size());
  }
}

/* the symbols ARG1..ARGn stand for user function index's own argument
   names (xpp::Model ufun_args), until set_old_arg_names puts them back */
void set_ufun_arg_names(xpp::Session &session, int index)
{
  const std::vector<std::string> &args=session.model().ufun_args[index];
  for(size_t i=0;i<args.size();i++){
    ExprSymbol &s=session.parser.symbols[FIRST_ARG+i];
    s.name=args[i];
    s.len=static_cast<int>(s.name.size());
 }
}

}

/* NEW ADD_FUN for new form_ode code  */

int add_ufun_name(xpp::Session &s, std::string_view name, int index, int narg)
{
 if(duplicate_name(s.parser,name)==1)return(1);
 if(index>=MAXUFUN)
 {
  if(s.parser.errout)xpp::log_printf(XPP_LOG_WARN, "too many functions !!\n");
  return(1);
 }
  xpp::log(XPP_LOG_INFO, " Added user fun {} \n",name);
  if(set_symbol(s.parser,name,10,narg,COM(UFUNTYPE, index)))return 1;
  s.parser.nsym++;
  s.model().ufun_names[index]=name;
  return (0);
}

namespace {

/* ends a compiled user function of narg arguments whose formula add_expr
   wrote in l commands */
void fixup_endfun(int *u, int l, int narg)
{
 u[l-1]=ENDFUN;
 u[l]=narg;
 u[l+1]=ENDEXP;
}

/* user function index's definition becomes def (Model::ufun_defs) */
void set_ufun_def(xpp::Model &m, int index, std::string_view def)
{
  m.ufun_defs[index]=def;
}

}

int add_ufun_new(xpp::Session &s, int index, std::string_view rhs, std::span<const std::string> args)
{
  xpp::Model &m=s.model();
  int end;
  int narg=static_cast<int>(args.size());
   if(narg>MAXARG){
    xpp::log_printf(XPP_LOG_WARN, "Maximal arguments exceeded \n");
    return(1);
  }
  /* add_expr compiles into it in place: MAXEXPLEN commands */
  m.ufun_programs[index].assign(MAXEXPLEN,0);
  set_ufun_def(m,index,"");
  m.ufun_args[index].assign(args.begin(),args.end());
  set_ufun_arg_names(s,index);
  if(add_expr(s,rhs,m.ufun_programs[index].data(),&end)==0)
    {
      fixup_endfun(m.ufun_programs[index].data(),end,narg);
      set_ufun_def(m,index,rhs);
      m.narg_fun[index]=narg;
      set_old_arg_names(s.parser,narg);
      return(0);
    }

  set_old_arg_names(s.parser,narg);
  if(s.parser.errout)xpp::log_printf(XPP_LOG_WARN, " ERROR IN FUNCTION DEFINITION\n");
  return(1);
}

/* ADD_UFUN   */

int add_ufun(xpp::Session &s, std::string_view junk, std::string_view expr, int narg)
{
 xpp::Model &m=s.model();
 int i;
 int end;

 if(duplicate_name(s.parser,junk)==1)return(1);
 if(m.nfun>=MAXUFUN)
 {
  if(s.parser.errout)xpp::log_printf(XPP_LOG_WARN, "too many functions !!\n");
  return(1);
 }
 std::vector<int> &program=m.ufun_programs[m.nfun];
 program.assign(MAXEXPLEN,0);
 set_ufun_def(m,m.nfun,"");

 if(add_expr(s,expr,program.data(),&end)==0)
 {
  set_symbol(s.parser,junk,10,narg,COM(UFUNTYPE, m.nfun));
  s.parser.nsym++;
  fixup_endfun(program.data(),end,narg);
  /* the definition without its last character */
  std::string_view def=expr;
  if(!def.empty())def.remove_suffix(1);
  set_ufun_def(m,m.nfun,def);
  m.ufun_names[m.nfun]=junk;
  m.narg_fun[m.nfun]=narg;
  std::vector<std::string> &arg_names=m.ufun_args[m.nfun];
  arg_names.clear();
  for(i=0;i<narg;i++)arg_names.push_back(xpp::format("ARG{}",i+1));
  m.nfun++;
  return(0);
 }
       if(s.parser.errout)xpp::log_printf(XPP_LOG_WARN, " ERROR IN FUNCTION DEFINITION\n");
       return(1);
}

int find_lookup(const xpp::Session &s, std::string_view name)
{
 int index,com;
 find_name(s.parser,name,&index);
  if(index==-1)return(-1);
  com=s.parser.symbols[index].com;
  if(is_lookup(com))return(com%MAXTYPE);
  return(-1);
}

namespace {

/* FIND_NAME    */

void find_name(const ParserState &p, std::string_view string, int *index)
{
  const std::array<ExprSymbol,MAX_SYMBS> &table=p.symbols;
  const int nsym=p.nsym;
  int i;
  std::string junk=converted(string);
  int len=static_cast<int>(junk.size());
  for(i=0;i<nsym;i++)
  {
   if(len==table[i].len)
    if(table[i].name.compare(0,len,junk)==0)break;
  }
   if(i<nsym)
    *index=i;
   else *index=-1;
}

}

int get_param_index(const xpp::Session &s, std::string_view name)
{
 int type,com;
  find_name(s.parser,name,&type);
  if(type<0)return(-1);
  com=s.parser.symbols[type].com;
  if(is_ucon(com))
  {
      return(com % MAXTYPE);

  }
    return(-1);
}

/* GET_VAL   */

int get_val(const xpp::Session &s, std::string_view name, double *value)
{
  int type,com;
  *value=0.0;
  find_name(s.parser,name,&type);
  if(type<0)return(0);
  com=s.parser.symbols[type].com;
  if(is_ucon(com))
  {
   *value=s.parser.constants[com % MAXTYPE];
   return(1);
  }
  if(is_uvar(com))
  {
      *value=s.parser.variables[com % MAXTYPE];
   return(1);
  }
  return(0);
}

/* SET_VAL         */

int set_val(xpp::Session &s, std::string_view name, double value)
{
  int type,com;
  find_name(s.parser,name,&type);
  if(type<0)return(0);
  com=s.parser.symbols[type].com;
  if(is_ucon(com))
  {
         s.parser.constants[com % MAXTYPE]=value;

    return(1);
  }
  if(is_uvar(com))
  {

      s.parser.variables[com % MAXTYPE]=value;
    return(1);
  }
  return(0);
}

} // namespace xpp
