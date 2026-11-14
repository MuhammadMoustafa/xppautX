#pragma once

#include <string>
#include <vector>

/* A model's declarations read as data: what an .ode global's events text says, and the array initial values
   element by element. Neither needs a run, so they sit with the Model, below the integrators that apply them (W736). */
namespace xpp {

struct Model; /* model.h */

/* one event of a flag: the name it sets, and its value's formula */
struct FlagEvent {
  std::string name,formula;
};
/* an .ode global's events, rest "{name=formula;name=formula;...}" (blanks
   and braces dropped, each name the text before its event's last =): 0,
   or 1 (said why, cond the flag's condition) when one names nothing,
   there are none or too many */
int split_events(const char *cond, const char *rest, std::vector<FlagEvent> &events);

/* the model's array initial values (x[j1..j2](0)=formula, applied by
   arr_ic_start when set_all_vals sets the model up), element by element:
   each variable's name and formula with the index worked out, the index
   j, and which of the model's array initial values it is (group, from 1) */
struct ArrayInitialValue {
  std::string var, formula;
  int j = 0, group = 0;
};
std::vector<ArrayInitialValue> array_initial_values(const Model &m);

} // namespace xpp
