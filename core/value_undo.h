#ifndef XPP_VALUE_UNDO_H
#define XPP_VALUE_UNDO_H
/* Undo and redo of the Values edits (W210, docs/command-design.md): a
   bounded stack of snapshots of the parameters, the initial conditions
   (and a delay equation's history) and the numerics, the things model_switch.h's
   KeptValues carries over a Reload. Not an undo of runs, plots, files or
   data. An edit pushes the values it is about to change; Run from last
   state pushes the initial conditions it overwrites. A new edit forgets
   what could be redone. A Session's (session.h); a load starts a new one. */

#include "model_switch.h"
#include <cstddef>
#include <deque>

namespace xpp {

struct Session;

/* the most snapshots each stack keeps: a long editing session (a slider drag
   edits at every step) without the snapshots, a few kilobytes each, growing
   without end; the oldest goes first */
inline constexpr std::size_t MAX_VALUE_UNDO = 100;

struct ValueUndo {
  std::deque<KeptValues> undo, redo;
};

/* about to edit the values: remember them, and forget what could be redone */
void push_value_undo(Session &s);
/* the values before the last edit come back in one step (the values now go
   to the redo stack); false, with nothing changed, when there is none */
bool undo_values(Session &s);
bool redo_values(Session &s);

}
#endif
