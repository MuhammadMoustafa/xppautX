#pragma once

#include "expr_program.h"
#include "xpp_error.h"

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

/* the array IC newic[j1..j2]: the one already used for it, else the first
   free one (the first slot when none is free) made into it */
inline ArrayIc &array_ic_for(ArrayIcs &ics, const char *newic, int j1, int j2)
{
  int ihot=-1;
  int ifree=-1;
  for(int i=0;i<NAR_IC;i++){
    if(ics.ics[i].index0==-1&&ifree==-1&&ics.ics[i].type==0)
      ifree=i;
    if(ics.ics[i].var==newic&&ics.ics[i].j1==j1&&ics.ics[i].j2==j2)
      ihot=i;
  }
  if(ihot==-1){
    ihot=ifree==-1?0:ifree;
    ics.ics[ihot].var=newic;
    ics.ics[ihot].type=2;
    ics.ics[ihot].j1=j1;
    ics.ics[ihot].j2=j2;
  }
  return ics.ics[ihot];
}

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
