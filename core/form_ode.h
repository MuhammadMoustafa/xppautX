#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAXLINES 5000

/* the columns a batch run writes (plotlist, N_plist of them): an "only"
   statement's (create_plot_list), or post-processing's; what the model
   itself defines is xpp::Model's (model.h) */
extern int *plotlist,N_plist;
/* an old-style file being rewritten in the new syntax (-convert):
   ConvertStyle set, convertf the new file (markov.cpp writes it too) */
extern int ConvertStyle;
extern FILE *convertf;

int make_eqn(void);
void strip_saveqn(void);
int get_eqn(FILE *fptr);
void create_plot_list(void);
int find_char(const char *s1, const char *s2, int i0, int *i1);
/* a model statement that calls compiled C code (export, dll_lib, dll_fun,
   a network's import; W55 removed them): logs why the line is refused
   and returns -1, the parsers' "this line does not parse" */
int refuse_compiled_functions(const char *what);


#ifdef __cplusplus
}

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* the name=value items of a par, init, number or wiener line as the
   reader splits them (get_next2, take_apart): each name, its value's
   text and the number atof reads from it */
struct OdeItem {
  std::string name, text;
  double value = 0;
};
std::vector<OdeItem> ode_items(std::string_view rhs);
/* get_eqn, the model's lines made already (an .odex model's: odex_load.cpp)
   rather than read from its file */
int get_eqn_lines(const std::vector<std::string> &lines);
/* 1 when the model is a map: is_a_map, or file ends in .dis or .dif */
int disc(std::string_view file);
/* boundary condition i's formula (Model::bcs[i].string, at most 255 bytes
   of it) becomes string; set_bc makes the condition first */
void set_bc_formula(int i, std::string_view string);
/* formula i (Model::formulas) becomes text */
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
