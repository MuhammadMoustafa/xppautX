#ifndef XPP_ODE_READ_H
#define XPP_ODE_READ_H
/* The .ode reader (ode_read.cpp): an .ode file's lines to the statement
   list (odex.h) the Model builder (form_ode.h) makes the Model from. */
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* the first character of s2 found in s1 from i0 on: its index in s2, *i1
   where it is; -1 when there is none */
int find_char(const char *s1, const char *s2, int i0, int *i1);

#ifdef __cplusplus
}

#include <string>
#include <string_view>
#include <vector>

namespace xpp {
struct Session; /* session.h */
}

/* the .ode model fptr reads into the loading Session s's Model (its
   this_file the path): read, then built; 1 (a model that does not load
   fails the load, xpp_model_failed) */
int get_eqn(xpp::Session &s, FILE *fptr);

/* the name=value items of a par, init, number or wiener line as the
   reader splits them (get_next2, take_apart): each name, its value's
   text and the number atof reads from it */
struct OdeItem {
  std::string name, text;
  double value = 0;
};
std::vector<OdeItem> ode_items(std::string_view rhs);
/* old with its array range x[i..j] made x[j] (i1, i2 the range; flag 1,
   or 2 for a %[i..j] for loop): newstr. 0 (newstr old) when the range
   is malformed. A line of initial data x[..](0)=... goes to
   extract_ic_data, which may rewrite old. */
int search_array(char *old, std::string &newstr, int *i1, int *i2, int *flag);
/* big with its subscripts worked out for index k */
void subsk(const char *big, std::string &newstr, int k, int flag);
#endif
#endif
