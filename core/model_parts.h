#pragma once

#include "expr_program.h"

#include <array>
#include <string>
#include <vector>

/* The pieces of a Model that a run's state (IntegratorState, Session) also
   holds by value, so integrate.h and session.h need them without model.h
   (a change to the Model then rebuilds only the files that read it). */
namespace xpp {

/* the most events a global flag sets (Model::GlobalFlag, IntegratorState's FlagState) */
constexpr int max_events = 20;

/* the array initial values: each one's variable x and range j1..j2 and
   its formula (type 2 when in use), at most NAR_IC of them; defined once
   the model gave one. The model's (Model::array_ics), and the run's own
   copy (IntegratorState), which Initialconds edits. */
constexpr int NAR_IC = 50;
struct ArrayIc {
  int index0 = -1, type = 0;
  std::string formula;
  int n = 0;
  std::string var;
  int j1 = 0, j2 = 0;
};
struct ArrayIcs {
  std::array<ArrayIc, NAR_IC> ics{};
  int defined = 0;
};

/* a boundary condition, 0=string: string (at most 255 bytes, the rest
   NUL), com its compiled form (200 commands), name "0=" (10 bytes;
   pp_shoot writes its side into it). C buffers: the shooting code and
   the dialogs write into them. The model's (Model::bcs) and the ones in
   use (Session::bcs). */
struct BoundaryCondition {
  Program com;
  std::vector<char> string;
  std::vector<char> name;
};

}
