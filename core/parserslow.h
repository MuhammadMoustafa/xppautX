#ifndef _parserslow_h_
#define _parserslow_h_

#include "volterra.h"
#include "xpplim.h"
#ifdef __cplusplus
extern "C" {
#endif

#define FUN1TYPE 9
#define FUN2TYPE 1
#define VARTYPE 3  /* standard variable */
#define CONTYPE 2  /* standard parameter */
#define UFUNTYPE   24
#define SVARTYPE 4  /* shifted variable */
#define SCONTYPE 32  /* shifted constant  */
#define NETTYPE 6
#define TABTYPE 7
#define USTACKTYPE 8
#define KERTYPE 10
#define VECTYPE 13  /* for vectorized stuff */
#define EVECTYPE 14 /* treat vector like a function */
#define MAXTYPE 20000000  /* this is the maximum number of named stuff */ 



#define COM(a,b) ((a)*MAXTYPE+(b))


#define MAXARG 20
#define NEGATE 9
#define MINUS 4
#define LPAREN 0
#define RPAREN 1
#define COMMA  2
#define STARTTOK 10
#define ENDTOK 11

#define ENDEXP 999
#define ENDFUN 998
#define STARTDELAY 980
#define DELSYM  42
#define ENDDELAY 996
#define MYIF  995
#define MYELSE 993
#define MYTHEN 994
#define SUMSYM 990
#define ENDSUM 991
#define SHIFTSYM 64
#define ISHIFTSYM 67
#define ENDSHIFT 988
#define SUMINDEX 989
#define LASTTOK MAX_SYMBS-2
#define NUMSYM 987
#define NUMTOK 59
#define CONV 2
#define FIRST_ARG 73
#define ENDDELSHFT 986
#define DELSHFTSYM 65
#define ENDISHIFT 985
#define SETSYM  72
#define ENDSET 981
#define INDX 68
#define INDXVAR 984

/*#define STDSYM 95
*/
#define STDSYM 96

#define INDXCOM 922
#define STARTINDX 70
#define ENDINDX 69



/* longest symbol name: a model's names, and the primed name X' that
   form_ode.c adds for each variable X */
#define MXLEN (XPP_NAME_MAX+1)

/* the longest program add_expr writes (the callers' command arrays) */
#define MAXEXPLEN 1024

/* a user function's argument names (xpp_util.cpp and edit_rhs.cpp read
   them as C text) */
typedef struct {
  int narg;
  char args[MAXARG][XPP_NAME_MAX+1];
} UFUN_ARG;

/* the user functions (NFUN of them): names, argument counts and names,
   and their definitions as C text (set_ufun_def keeps it) */
extern char ufun_names[MAXUFUN][XPP_NAME_MAX+1];
extern int narg_fun[MAXUFUN];
extern UFUN_ARG ufun_arg[MAXUFUN];
extern char *ufun_def[MAXUFUN];

/* the parser's counts and values (parserslow2.cpp's): the model's
   constants (parameters and numbers) and variables as the compiled
   programs read them, how many of each kind of symbol there are, the
   delays, kernels, tables and user functions, and the Volterra grid's
   length (MaxPoints); ERROUT: report parse errors; RandSeed: the random
   numbers' seed */
extern double constants[MAXPAR];
extern double variables[MAXODE1];
extern int NCON,NVAR,NFUN,NSYM,NDELAYS,NKernel,NTable,MaxPoints;
extern int ERROUT,RandSeed;


#define VECT_ROOT 500 




void init_rpn(void);
int duplicate_name(const char *junk);
int name_too_long(const char *name);
int add_constant(const char *junk);
int get_var_index(const char *name);
int add_con(const char *name, double value);
int add_kernel(const char *name, double mu, const char *expr);
int add_var(const char *junk, double value);
int add_expr(const char *expr, int *command, int *length);
int add_net_name(int index, const char *name);
int add_vector_name(int index, const char *name);
int add_2d_table(const char *name, const char *file);
int add_file_table(int index, const char *file);
int add_table_name(int index, const char *name);
int add_form_table(int index, int nn, double xlo, double xhi, const char *formula);
void set_old_arg_names(int narg);
/* the symbols ARG1..ARGn stand for user function index's own argument
   names (ufun_arg), until set_old_arg_names puts them back */
void set_ufun_arg_names(int index);
int add_ufun_name(const char *name, int index, int narg);
void fixup_endfun(int *u, int l, int narg);
int add_ufun(const char *junk, const char *expr, int narg);
int is_ufun(int x);
int is_ucon(int x);
int is_uvar(int x);
int isvar(int y);
int iscnst(int y);
int isker(int y);
int is_lookup(int x);
int find_lookup(const char *name);
void find_name(const char *string, int *index);
int get_param_index(const char *name);
int get_val(const char *name, double *value);
int set_val(const char *name, double value);
void set_ivar(int i, double value);
double get_ivar(int i);
int alg_to_rpn(int *toklist, int *command);
void show_where(const char *string, int index);
int function_sym(int token);
int unary_sym(int token);
int binary_sym(int token);
int pure_number(int token);
int gives_number(int token);
int check_syntax(int oldtoken, int newtoken);
int make_toks(const char *dest, int *my_token);
void tokeninfo(int tok);
int do_num(const char *source, char *num, double *value, int *ind);
void convert(const char *source, char *dest);
void find_tok(const char *source, int *index, int *tok);
double pmod(double x, double y);
void two_args(void);
double do_shift(double shift, double variable);
double do_ishift(double shift, double variable);
double do_delay_shift(double delay, double shift, double variable);
double do_delay(double delay, double i);
void one_arg(void);
double max(double x, double y);
double min(double x, double y);

double neg(double z);
double recip(double z);
double heaviside(double z);
double rndom(double z);
double signum(double z);
double dnot(double x);
double dand(double x, double y);
double dor(double x, double y);
double dge(double x, double y);
double dle(double x, double y);
double deq(double x, double y);
double dne(double x, double y);
double dgt(double x, double y);
double dlt(double x, double y);
double evaluate(int *equat);
double eval_rpn(int *equat);

/*  STRING STUFF  */
#ifdef _WIN32
/* the Windows C library has its own char *strupr/strlwr; use private names */
#ifdef __cplusplus
}
#endif
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define strupr xpp_strupr
#define strlwr xpp_strlwr
#endif
#ifndef STRUPR
void strupr(char *s);
void strlwr(char *s);
#endif

/*****************************************************/






#ifdef __cplusplus
}

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/* the user functions' programs (MAXEXPLEN commands each) */
extern std::array<std::vector<int>,MAXUFUN> ufun;
/* user function index's definition becomes def (ufun_def[index] its text) */
void set_ufun_def(int index, std::string_view def);
/* user function index with the arguments args and the formula rhs */
int add_ufun_new(int index, const char *rhs, std::span<const std::string> args);
/* name as the symbol table keeps it: blanks removed, upper case */
std::string converted(std::string_view name);
#endif
#endif
