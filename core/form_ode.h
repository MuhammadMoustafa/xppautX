#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include "shoot.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAXVNAM (XPP_NAME_MAX+1)
#define MAXLINES 5000

/* The model as the parser leaves it (form_ode.cpp), read by the rest of
   the core as C tables: the names of the variables (ODEs, Markov, aux)
   and parameters, each variable's formula as typed (ode_names) and
   compiled (my_ode), the source's lines (save_eqn), the boundary
   conditions, and the columns an "only" statement keeps (plotlist) */
extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern char upar_names[MAXPAR][XPP_NAME_MAX+1];
extern char *ode_names[MAXODE];
extern int *my_ode[MAXODE];
extern char *save_eqn[MAXLINES];
extern BC_STRUCT my_bc[MAXODE];
extern int *plotlist,N_plist;
extern int EqType[MAXODE];

int make_eqn(void);
void strip_saveqn(void);
int disc(const char *string);
int get_eqn(FILE *fptr);
/* strtok's tokens of string (get_first) and of the rest of it (get_next):
   the tokenizer aniparse, auto_nox, do_fit, load_eqn and simplenet share */
char *get_first(char *string, const char *src);
char *get_next(const char *src);
void create_plot_list(void);
int find_char(const char *s1, const char *s2, int i0, int *i1);

/* the model's comments (the ones with an action run it when picked):
   C text, kept by form_ode.cpp */
typedef struct {
  char *text,*action;
  int aflag;
} ACTION;

extern ACTION comments[];
extern int n_comments;

#ifdef __cplusplus
}

#include <array>
#include <optional>
#include <string>
#include <string_view>

/* strtok's tokens (get_first/get_next) without writing into the text:
   next(delims) passes over the delimiters, returns the text up to the
   next one and passes over that one too, each call naming its own
   delimiters as strtok's did; nullopt once nothing is left. rest() is
   what follows the last token returned. */
class Tokens {
public:
  explicit Tokens(std::string_view text):rest_(text){}
  std::optional<std::string_view> next(std::string_view delims)
  {
    size_t b=rest_.find_first_not_of(delims);
    if(b==std::string_view::npos){
      rest_={};
      return std::nullopt;
    }
    rest_.remove_prefix(b);
    size_t e=rest_.find_first_of(delims);
    std::string_view tok=rest_.substr(0,e);
    rest_.remove_prefix(e==std::string_view::npos?rest_.size():e+1);
    return tok;
  }
  /* the next token as text, "" when there is none */
  std::string text(std::string_view delims)
  {
    return std::string{next(delims).value_or(std::string_view())};
  }
  std::string_view rest() const { return rest_; }
private:
  std::string_view rest_;
};

/* a fixed variable's name and formula as typed (lunch-new.cpp writes
   them): FIX_VAR of them */
struct FIXINFO {
  std::string name,value;
};
extern std::array<FIXINFO,MAXODE> fixinfo;

/* formula i (ode_names[i]) becomes text */
void set_ode_name(int i, std::string_view text);
/* old with its array range x[i..j] made x[j] (i1, i2 the range; flag 1,
   or 2 for a %[i..j] for loop): newstr. 0 (newstr old) when the range
   is malformed. A line of initial data x[..](0)=... goes to
   extract_ic_data, which may rewrite old. */
int search_array(char *old, std::string &newstr, int *i1, int *i2, int *flag);
/* big with its subscripts worked out for index k */
void subsk(const char *big, std::string &newstr, int k, int flag);
#endif
#endif
