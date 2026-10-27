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
#include <algorithm>
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

constexpr int PRIMARY_PRIORITY = 10; /* Primary terms and function calls bind above operators in the token table. */

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
   {"ATAN2",5,COM(FUN2TYPE,4),2,PRIMARY_PRIORITY},
   {"MAX",3,COM(FUN2TYPE,6),2,PRIMARY_PRIORITY},
   {"MIN",3,COM(FUN2TYPE,7),2,PRIMARY_PRIORITY},
   {"SIN",3,COM(FUN1TYPE,0),0,PRIMARY_PRIORITY},
   {"COS",3,COM(FUN1TYPE,1),0,PRIMARY_PRIORITY},
   {"TAN",3,COM(FUN1TYPE,2),0,PRIMARY_PRIORITY},
   {"ASIN",4,COM(FUN1TYPE,3),0,PRIMARY_PRIORITY},
   {"ACOS",4,COM(FUN1TYPE,4),0,PRIMARY_PRIORITY},
   {"ATAN",4,COM(FUN1TYPE,5),0,PRIMARY_PRIORITY},  /* 20  */
   {"SINH",4,COM(FUN1TYPE,6),0,PRIMARY_PRIORITY},
   {"TANH",4,COM(FUN1TYPE,7),0,PRIMARY_PRIORITY},
   {"COSH",4,COM(FUN1TYPE,8),0,PRIMARY_PRIORITY},
   {"ABS",3,COM(FUN1TYPE,9),0,PRIMARY_PRIORITY},
   {"EXP",3,COM(FUN1TYPE,10),0,PRIMARY_PRIORITY},
   {"LN",2,COM(FUN1TYPE,11),0,PRIMARY_PRIORITY},
   {"LOG",3,COM(FUN1TYPE,11),0,PRIMARY_PRIORITY},
   {"LOG10",5,COM(FUN1TYPE,12),0,PRIMARY_PRIORITY},
   {"SQRT",4,COM(FUN1TYPE,13),0,PRIMARY_PRIORITY},
   {"HEAV",4,COM(FUN1TYPE,16),0,PRIMARY_PRIORITY},  /*  30 */
   {"SIGN",4,COM(FUN1TYPE,17),0,PRIMARY_PRIORITY},
   {"#$%1",4,COM(USTACKTYPE,0),0,PRIMARY_PRIORITY},
   {"#$%2",4,COM(USTACKTYPE,1),0,PRIMARY_PRIORITY},
   {"#$%3",4,COM(USTACKTYPE,2),0,PRIMARY_PRIORITY},
   {"#$%4",4,COM(USTACKTYPE,3),0,PRIMARY_PRIORITY},
   {"#$%5",4,COM(USTACKTYPE,4),0,PRIMARY_PRIORITY},
   {"#$%6",4,COM(USTACKTYPE,5),0,PRIMARY_PRIORITY},
   {"#$%7",4,COM(USTACKTYPE,6),0,PRIMARY_PRIORITY},
   {"#$%8",4,COM(USTACKTYPE,7),0,PRIMARY_PRIORITY},
   {"FLR",3,COM(FUN1TYPE,18),0,PRIMARY_PRIORITY},  /*  40 */
   {"MOD",3,COM(FUN2TYPE,8),2,PRIMARY_PRIORITY}, /*  41 */
   {"DELAY",5,ENDDELAY,2,PRIMARY_PRIORITY},      /*  42 */   /*  Delay symbol */
   {"RAN",3,COM(FUN1TYPE,xpp::expr::RANDOM_UNIFORM),1,PRIMARY_PRIORITY}, /* 43 */
   {"&",1,COM(FUN2TYPE,9),0,6},  /* logical stuff  */
   {"|",1,COM(FUN2TYPE,10),0,4},
   {">",1,COM(FUN2TYPE,11),0,7},
   {"<",1,COM(FUN2TYPE,12),0,7},
   {"==",2,COM(FUN2TYPE,13),0,7},
   {">=",2,COM(FUN2TYPE,14),0,7},
   {"<=",2,COM(FUN2TYPE,15),0,7}, /*50 */
   {"IF",2,995,1,PRIMARY_PRIORITY},
   {"THEN",4,994,1,PRIMARY_PRIORITY},
   {"ELSE",4,993,1,PRIMARY_PRIORITY},
   {"!=",2,COM(FUN2TYPE,16),0,7},
   {"NOT",3,COM(FUN1TYPE,20),0,6},
   {"NORMAL",6,COM(FUN2TYPE,xpp::expr::RANDOM_NORMAL),2,PRIMARY_PRIORITY}, /* returns normally dist number */
   {"BESSELJ",7,COM(FUN2TYPE,18),2,PRIMARY_PRIORITY}, /* Bessel J   */
   {"BESSELY",7,COM(FUN2TYPE,19),2,PRIMARY_PRIORITY}, /* Bessel Y */
   {"NXXQQ",5,NUMSYM,0,PRIMARY_PRIORITY},
   {"ERF", 3, COM(FUN1TYPE,21),0,PRIMARY_PRIORITY}, /* 60 */
   {"ERFC",4,COM(FUN1TYPE,22),0,PRIMARY_PRIORITY},
   {"SUM",3,SUMSYM,2,PRIMARY_PRIORITY},
   {"OF",2,ENDSUM,0,PRIMARY_PRIORITY},
   {"SHIFT",5,ENDSHIFT,2,PRIMARY_PRIORITY},
   {"DEL_SHFT",8,ENDDELSHFT,3,PRIMARY_PRIORITY},/* 65 */
   {"HOM_BCS",7,COM(FUN1TYPE,23),0,PRIMARY_PRIORITY},
   {"ISHIFT",6,ENDISHIFT,2,PRIMARY_PRIORITY}, /* 67 */
   {"@",1,INDXCOM,0,PRIMARY_PRIORITY}, /*68 */
   {"]",1,ENDSHIFT,0,PRIMARY_PRIORITY},
   {"[",1,ENDSHIFT,0,PRIMARY_PRIORITY}, /*70 */
   {"POISSON",7,COM(FUN1TYPE,xpp::expr::RANDOM_POISSON),0,PRIMARY_PRIORITY}, /* 71 */
   {"SET",3,ENDSET,3,PRIMARY_PRIORITY}, /* 72 */
   {"ARG1",4,COM(USTACKTYPE,0),0,PRIMARY_PRIORITY}, /* ARG1..ARG20: a user function reads its arguments by these */
   {"ARG2",4,COM(USTACKTYPE,1),0,PRIMARY_PRIORITY},
   {"ARG3",4,COM(USTACKTYPE,2),0,PRIMARY_PRIORITY},
   {"ARG4",4,COM(USTACKTYPE,3),0,PRIMARY_PRIORITY},
   {"ARG5",4,COM(USTACKTYPE,4),0,PRIMARY_PRIORITY},
   {"ARG6",4,COM(USTACKTYPE,5),0,PRIMARY_PRIORITY},
   {"ARG7",4,COM(USTACKTYPE,6),0,PRIMARY_PRIORITY},
   {"ARG8",4,COM(USTACKTYPE,7),0,PRIMARY_PRIORITY},
   {"ARG9",4,COM(USTACKTYPE,8),0,PRIMARY_PRIORITY},
   {"ARG10",5,COM(USTACKTYPE,9),0,PRIMARY_PRIORITY},
   {"ARG11",5,COM(USTACKTYPE,10),0,PRIMARY_PRIORITY},
   {"ARG12",5,COM(USTACKTYPE,11),0,PRIMARY_PRIORITY},
   {"ARG13",5,COM(USTACKTYPE,12),0,PRIMARY_PRIORITY},
   {"ARG14",5,COM(USTACKTYPE,13),0,PRIMARY_PRIORITY},
   {"ARG15",5,COM(USTACKTYPE,14),0,PRIMARY_PRIORITY},
   {"ARG16",5,COM(USTACKTYPE,15),0,PRIMARY_PRIORITY},
   {"ARG17",5,COM(USTACKTYPE,16),0,PRIMARY_PRIORITY},
   {"ARG18",5,COM(USTACKTYPE,17),0,PRIMARY_PRIORITY},
   {"ARG19",5,COM(USTACKTYPE,18),0,PRIMARY_PRIORITY},
   {"ARG20",5,COM(USTACKTYPE,19),0,PRIMARY_PRIORITY},
   {"BESSELI",7,COM(FUN2TYPE,20),2,PRIMARY_PRIORITY},/* Bessel I  # 93 */
   {"LGAMMA",6,COM(FUN1TYPE,25),1,PRIMARY_PRIORITY}, /* Log Gamma  #94 */
   {"BESSELIS",8,COM(FUN2TYPE,21),2,PRIMARY_PRIORITY},/* Bessel I Scaled  # 95 */
};
static_assert(std::size(builtins)==STDSYM,"STDSYM is the number of built-in symbols");
/* Immutable, sorted once at compile time; a formula no longer rescans
   every built-in for each name it validates. */
constexpr auto sorted_builtins=[] {
  auto sorted=std::to_array(builtins);
  std::sort(sorted.begin(),sorted.end(),[](const Builtin &a,const Builtin &b) {
    return std::string_view(a.name)<std::string_view(b.name);
  });
  return sorted;
}();

const Builtin *builtin_named(std::string_view name)
{
  const auto b=std::lower_bound(sorted_builtins.begin(),sorted_builtins.end(),name,
                              [](const Builtin &entry,std::string_view key) {return entry.name<key;});
  return b!=sorted_builtins.end() && b->name==name ? &*b : nullptr;
}


}

int builtin_arity(std::string_view name)
{
  if (name != lower_case(std::string(name))) return -1;
  const std::string upper = upper_case(std::string(name));
  if (upper == "VOLTERRA") return -2; /* syntax lowered to the engine's integral */
  if (const Builtin *b=builtin_named(upper)) {
    if (b->com == SUMSYM) return -2;
    if (b->com / MAXTYPE == FUN1TYPE && b->pri == PRIMARY_PRIORITY) return 1;
    if ((b->com / MAXTYPE == FUN2TYPE && b->pri == PRIMARY_PRIORITY) || b->com == ENDDELAY || b->com == ENDSHIFT || b->com == ENDISHIFT) return 2;
    if (b->com == ENDDELSHFT || b->com == ENDSET) return 3;
  }
  return -1;
}

bool is_builtin_symbol(std::string_view name)
{
  return builtin_named(name)!=nullptr;
}

bool is_pure_builtin(std::string_view name)
{
  const Builtin *b=builtin_named(name);
  if(!b||b->pri!=PRIMARY_PRIORITY)return false;
  const std::size_t index=static_cast<std::size_t>(b->com%MAXTYPE);
  if(b->com/MAXTYPE==FUN1TYPE)return xpp::expr::fun1[index]!=nullptr;
  if(b->com/MAXTYPE==FUN2TYPE)return xpp::expr::fun2[index]!=nullptr;
  return false;
}

std::optional<double> pure_builtin_value(std::string_view name, std::span<const double> args)
{
  const std::string upper=upper_case(std::string(name));
  if(!is_pure_builtin(upper))return std::nullopt;
  const Builtin *b=builtin_named(upper);
  const std::size_t index=static_cast<std::size_t>(b->com%MAXTYPE);
  if(b->com/MAXTYPE==FUN1TYPE&&args.size()==1)return xpp::expr::fun1[index](args[0]);
  if(b->com/MAXTYPE==FUN2TYPE&&args.size()==2)return xpp::expr::fun2[index](args[0],args[1]);
  return std::nullopt;
}

namespace {

/* 1 (with an INFO) when name is already a symbol */
int duplicate_name(const ParserState &p, std::string_view junk);
/* name's symbol index in *index (-1 when none) */
void find_name(const ParserState &p, std::string_view string, int *index);

}

void ParserState::unindex_symbol(int index) const
{
  auto entry = names_.find(indexed_names_[index]);
  entry->second.erase(index);
  if (entry->second.empty()) names_.erase(entry);
}

void ParserState::forget_symbols_from(int index) const
{
  while (indexed_names_.size() > static_cast<size_t>(index)) {
    unindex_symbol(static_cast<int>(indexed_names_.size() - 1));
    indexed_names_.pop_back();
  }
}

void ParserState::sync_symbols() const
{
  forget_symbols_from(nsym);
  while (indexed_names_.size() < static_cast<size_t>(nsym)) {
    const int i = static_cast<int>(indexed_names_.size());
    indexed_names_.push_back(symbols[i].name);
    names_[symbols[i].name].insert(i);
  }
}

void ParserState::symbol_name_changed(int index)
{
  sync_symbols();
  if (indexed_names_[index] == symbols[index].name) return;
  unindex_symbol(index);
  indexed_names_[index] = symbols[index].name;
  names_[symbols[index].name].insert(index);
}

int ParserState::symbol_named(std::string_view name) const
{
  sync_symbols();
  const auto entry = names_.find(name);
  return entry == names_.end() ? -1 : *entry->second.begin();
}

int ParserState::symbol_prefix(const char *source, int &length) const
{
  sync_symbols();
  int token = nsym;
  length = 0;
  for (size_t n = 1; source[n - 1] != '\0'; ++n) {
    const std::string_view prefix(source, n);
    const auto entry = names_.lower_bound(prefix);
    if (entry == names_.end() || !entry->first.starts_with(prefix)) break;
    if (entry->first.size() == n) {
      token = *entry->second.begin(); // the original scan chose the first equal name
      length = static_cast<int>(n);
    }
  }
  return token;
}

int ParserState::arg_symbol(int i)
{
  if (i < BUILTIN_ARGS) return FIRST_ARG + i;
  const int com = COM(USTACKTYPE, i);
  for (int k = STDSYM; k < nsym; k++)
    if (symbols[k].com == com) return k;
  forget_symbols_from(nsym);
  room_for_symbol();
  ExprSymbol &s = symbols[nsym];
  s.name = xpp::format("ARG{}", i + 1);
  s.len = static_cast<int>(s.name.size());
  s.pri = PRIMARY_PRIORITY;
  s.arg = 0;
  s.com = com;
  return nsym++;
}

int ParserState::arg_count() const
{
  int n = BUILTIN_ARGS;
  for (int k = 0; k < nsym; k++)
    if (symbols[k].com / MAXTYPE == USTACKTYPE) n = std::max(n, symbols[k].com % MAXTYPE + 1);
  return n;
}

ParserState::ParserState()
{
  symbols.resize(2*STDSYM);
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
    add_con(s,"PI", pi_value);

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
    if (s.model().command_line.new_seed) {
      s.numerics.rand_seed=static_cast<int>(time(0));
      xpp::log(XPP_LOG_INFO,"--newseed: seed {}\n",s.numerics.rand_seed);
    }
    s.random.seed(s.numerics.rand_seed);
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
    xpp::log(XPP_LOG_WARN, "Empty name - remove spaces\n");
    return 1;
  }
  p.forget_symbols_from(p.nsym);
  p.room_for_symbol();
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
 if(set_symbol(p,junk,10,0,COM(CONTYPE,p.ncon-1)))return 1;
 p.nsym++;
 return(0);
}

}

int get_var_index(const xpp::Session &s, std::string_view name)
{
  if(const auto *entry=s.model().name(name);entry && entry->kind!=Model::NameKind::Set)return entry->parser_var;

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
 if(p.ncon>=MAXTYPE) /* likewise a constant (COM) */
 {
  if(p.errout)xpp::log(XPP_LOG_WARN, "a model has at most {} constants: a compiled formula names one by its place in that many\n",MAXTYPE);
  return(1);
 }
 if(p.constants.size()<=static_cast<size_t>(p.ncon))p.constants.resize(std::max<size_t>(64,2*p.constants.size()));
 p.constants[p.ncon]=value;
 p.ncon++;
 const int result=add_constant(p,name);
 if(result==0)
   if(auto it=s.model().names.find(converted(name));it!=s.model().names.end())it->second.parser_con=p.ncon-1;
 return result;
}

int add_kernel(xpp::Session &s, std::string_view name, double mu, std::string_view expr)
{
  xpp::Model &m=s.model();
  int in=-1;
  if(duplicate_name(s.parser,name)==1)return(1);
  if(mu<0||mu>=1.0){
    xpp::log(XPP_LOG_WARN, " mu must lie in [0,1.0) \n");
    return(1);
  }
  if(set_symbol(s.parser,name,10,0,COM(KERTYPE,m.nkernel)))return 1;
  std::string_view text=expr;
  size_t hash=text.rfind('#');
  if(hash!=std::string_view::npos)in=static_cast<int>(hash);
  if(in==0||in==static_cast<int>(text.size())-1){
    xpp::log(XPP_LOG_WARN, "Illegal use of convolution...\n");
    return(1);
  }
  m.kernels.resize(m.nkernel+1);
  KERNEL &k=m.kernels[m.nkernel];
  k.mu=mu;
  k.flag=0;
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
  k.where=Load::place();
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
 if(m.nvar>=MAXTYPE) /* an operand names its variable below MAXTYPE (COM) */
 {
  if(p.errout)xpp::log(XPP_LOG_WARN, "a model has at most {} variables: a compiled formula names one by its place in that many\n",MAXTYPE);
  return(1);
 }
 if(set_symbol(p,junk,10,0,COM(VARTYPE,m.nvar)))return 1;
 p.nsym++;
 if(p.variables.size()<=static_cast<size_t>(m.nvar))p.variables.resize(std::max<size_t>(64,2*p.variables.size()));
 p.variables[m.nvar]=value;
 if(auto it=m.names.find(converted(junk));it!=m.names.end())it->second.parser_var=m.nvar;
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
 xpp::log(XPP_LOG_WARN, " TWO D NOT HERE YET \n");
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
  return made;
}

namespace {

/* the symbols ARG1..ARGnarg back to their own names */
void set_old_arg_names(ParserState &p, int narg)
{
  int i;
  for(i=0;i<narg;i++){
    const int k=p.arg_symbol(i);
    ExprSymbol &s=p.symbols[k];
    s.name=xpp::format("ARG{}",i+1);
    s.len=static_cast<int>(s.name.size());
    p.symbol_name_changed(k);
  }
}

/* the symbols ARG1..ARGn stand for user function index's own argument
   names (xpp::Model ufun_args), until set_old_arg_names puts them back */
void set_ufun_arg_names(xpp::Session &session, int index)
{
  const std::vector<std::string> &args=session.model().ufun_args[index];
  for(size_t i=0;i<args.size();i++){
    const int k=session.parser.arg_symbol(static_cast<int>(i));
    ExprSymbol &s=session.parser.symbols[k];
    s.name=args[i];
    s.len=static_cast<int>(s.name.size());
    session.parser.symbol_name_changed(k);
 }
}

}

/* NEW ADD_FUN for new form_ode code  */

int add_ufun_name(xpp::Session &s, std::string_view name, int index, int narg)
{
 if(duplicate_name(s.parser,name)==1)return(1);
 xpp::log(XPP_LOG_INFO, " Added user fun {} \n",name);
  if(set_symbol(s.parser,name,10,narg,COM(UFUNTYPE, index)))return 1;
  s.parser.nsym++;
  s.model().room_for_function(index);
  s.model().ufun_names[index]=name;
  return (0);
}

namespace {

/* ends a compiled user function of narg arguments whose formula add_expr
   wrote */
void fixup_endfun(std::vector<int> &program, int narg)
{
 if(!program.empty() && program.back() == ENDEXP)
   program.pop_back();
 program.push_back(ENDFUN);
 program.push_back(narg);
 program.push_back(ENDEXP);
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
  m.ufun_programs[index].rpn.clear();
  set_ufun_def(m,index,"");
  m.ufun_args[index].assign(args.begin(),args.end());
  set_ufun_arg_names(s,index);
  if(add_expr(s,rhs,m.ufun_programs[index].rpn,&end)==0)
    {
      fixup_endfun(m.ufun_programs[index].rpn,narg);
      set_ufun_def(m,index,rhs);
      m.narg_fun[index]=narg;
      set_old_arg_names(s.parser,narg);
      return(0);
    }

  set_old_arg_names(s.parser,narg);
  if(s.parser.errout)xpp::log(XPP_LOG_WARN, " ERROR IN FUNCTION DEFINITION\n");
  return(1);
}

/* ADD_UFUN   */

int add_ufun(xpp::Session &s, std::string_view junk, std::string_view expr, int narg)
{
 xpp::Model &m=s.model();
 int i;
 int end;

 if(duplicate_name(s.parser,junk)==1)return(1);
 m.room_for_function(m.nfun);
 std::vector<int> &program=m.ufun_programs[m.nfun].rpn;
 program.clear();
 set_ufun_def(m,m.nfun,"");

 if(add_expr(s,expr,program,&end)==0)
 {
  set_symbol(s.parser,junk,10,narg,COM(UFUNTYPE, m.nfun));
  s.parser.nsym++;
  fixup_endfun(program,narg);
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
       if(s.parser.errout)xpp::log(XPP_LOG_WARN, " ERROR IN FUNCTION DEFINITION\n");
       return(1);
}

int find_lookup(const xpp::Session &s, std::string_view name)
{
 if(const auto *entry=s.model().name(name);entry && entry->kind!=Model::NameKind::Set)return entry->kind==Model::NameKind::Table?entry->index:-1;
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
  *index=p.symbol_named(converted(string));
}

}

int get_param_index(const xpp::Session &s, std::string_view name)
{
 if(const auto *entry=s.model().name(name);entry && entry->kind!=Model::NameKind::Set)return entry->parser_con;
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

std::string_view parameter_name(const xpp::Session &s, int k)
{
  const Model &m=s.model();
  for(int i=0;i<m.nupar;i++)
    if(get_param_index(s,m.upar_names[i])==k)return m.upar_names[i];
  return {};
}

namespace {

/* where name's value lives in p, a parameter's or a variable's (nullptr
   when name is neither): const as p is */
template <class Parser>
auto slot_in(const Model &m, Parser &p, std::string_view name) -> decltype(&p.constants[0])
{
  if(const Model::Name *entry=m.name(name);entry && entry->kind!=Model::NameKind::Set){
    if(entry->parser_con>=0)return &p.constants[entry->parser_con];
    if(entry->parser_var>=0)return &p.variables[entry->parser_var];
    return nullptr;
  }
  int type;
  find_name(p,name,&type);
  if(type<0)return nullptr;
  const int com=p.symbols[type].com;
  if(is_ucon(com))return &p.constants[com % MAXTYPE];
  if(is_uvar(com))return &p.variables[com % MAXTYPE];
  return nullptr;
}

}

double *value_slot(Session &s, std::string_view name) { return slot_in(s.model(),s.parser,name); }

/* GET_VAL   */

int get_val(const xpp::Session &s, std::string_view name, double *value)
{
  const double *v=slot_in(s.model(),s.parser,name);
  *value=v?*v:0.0;
  return v!=nullptr;
}

/* SET_VAL         */

int set_val(xpp::Session &s, std::string_view name, double value)
{
  double *v=slot_in(s.model(),s.parser,name);
  if(v)*v=value;
  return v!=nullptr;
}

} // namespace xpp
