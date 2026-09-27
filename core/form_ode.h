#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include "shoot.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAXLINES 5000

/* The model as the parser leaves it (form_ode.cpp), read by the rest of
   the core as C tables (the names of the variables and parameters are
   xpp::Model's, model.h): each variable's formula as typed (ode_names) and
   compiled (my_ode), the source's lines (save_eqn), the boundary
   conditions, and the columns an "only" statement keeps (plotlist) */
extern char *ode_names[MAXODE];
extern int *my_ode[MAXODE];
extern char *save_eqn[MAXLINES];
extern BC_STRUCT my_bc[MAXODE];
extern int *plotlist,N_plist;
/* the parameters' and variables' values as the model gives them */
extern double default_val[MAXPAR];
extern double default_ic[MAXODE];
/* the source's line count (the model's other counts are xpp::Model's) */
extern int NLINES;
/* an old-style file being rewritten in the new syntax (-convert):
   ConvertStyle set, convertf the new file (markov.cpp writes it too) */
extern int ConvertStyle;
extern FILE *convertf;

int make_eqn(void);
void strip_saveqn(void);
int get_eqn(FILE *fptr);
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
#include <vector>

/* a fixed variable's name and formula as typed (lunch-new.cpp writes
   them): FIX_VAR of them */
struct FIXINFO {
  std::string name,value;
};
extern std::array<FIXINFO,MAXODE> fixinfo;

/* 1 when the model is a map: is_a_map, or file ends in .dis or .dif */
int disc(std::string_view file);
/* variable i's compiled formula (my_ode[i]) becomes program */
void set_program(int i, std::vector<int> program);
/* boundary condition i's formula (my_bc[i].string, at most 255 bytes
   of it) becomes string; set_bc makes the condition first */
void set_bc_formula(int i, std::string_view string);
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
