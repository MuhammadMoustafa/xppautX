/* Undo and redo of the Values edits: see value_undo.h. */
#include "value_undo.h"
#include "session.h"
#include "xpp_ui.h"
#include <utility>

namespace xpp {

namespace {

/* a stack's top becomes the values in use, `other` keeps the values it replaces */
bool step(Session &s, std::deque<KeptValues> &from, std::deque<KeptValues> &other)
{
  if (from.empty()) return false;
  KeptValues target = std::move(from.back());
  from.pop_back();
  other.push_back(keep_values(s));
  if (other.size() > MAX_VALUE_UNDO) other.pop_front();
  target.saved = s.saved_session; /* the session file is no value: an undo leaves it */
  restore_values(s, target);
  s.value_undo.dragging.clear();
  return true;
}

} // namespace

void push_value_undo(Session &s, std::string_view drag)
{
  if (!drag.empty() && s.value_undo.dragging == drag) return;
  s.value_undo.dragging = drag;
  s.value_undo.undo.push_back(keep_values(s));
  if (s.value_undo.undo.size() > MAX_VALUE_UNDO) s.value_undo.undo.pop_front();
  s.value_undo.redo.clear();
}

/* the step's outcome as a command's value: nothing to take or put back is its error */
static Result<> step_result(bool stepped, const char *command, const char *nothing)
{
  if (!stepped) return fail(command, nothing, command_place());
  return {};
}

bool undo_values(Session &s)
{
  return step(s, s.value_undo.undo, s.value_undo.redo);
}

bool redo_values(Session &s)
{
  return step(s, s.value_undo.redo, s.value_undo.undo);
}

Result<> undo_value_edit(Session &s) { return step_result(undo_values(s), "undo", "No value edit to undo"); }

Result<> redo_value_edit(Session &s) { return step_result(redo_values(s), "redo", "No value edit to redo"); }

}
