#pragma once

#include <string_view>

namespace xpp {

/* The kind of an action (W95), one letter: what the page may still do while
   a computation runs (docs/protocol.md "Action kinds"). A control action
   (Abort, Quit, an answer) always goes; a view action only changes what is
   shown and goes during a computation too (the core runs it after the
   computation); a setting (W106: a parameter, an initial or boundary
   condition, a delay, the numerics, AUTO's forms) goes during a computation
   too and is applied when it ends, never to the run in progress; a data
   action (saving, loading) and a computation (anything that starts one)
   are refused during one. */
#define XPP_KIND_CONTROL 'c'
#define XPP_KIND_VIEW 'v'
#define XPP_KIND_SETTING 's'
#define XPP_KIND_DATA 'd'
#define XPP_KIND_COMPUTE 'x'

/* The group a registry command (core/command_def.h) is filed under in the protocol's tables and the keymap editor.
   Panels are the commands the page opens from a panel or the title bar (the Model panel's Source, the Values
   panel's sets, Help's Keyboard shortcuts, Record and Play): they cannot be pinned. */
enum class CommandCategory { Run, Files, Analysis, Plot, Tools, Panels };

constexpr std::string_view category_id(CommandCategory category)
{
  switch (category) {
  case CommandCategory::Run: return "run";
  case CommandCategory::Files: return "files";
  case CommandCategory::Analysis: return "analysis";
  case CommandCategory::Plot: return "plot";
  case CommandCategory::Tools: return "tools";
  case CommandCategory::Panels: return "panels";
  }
  return {};
}

}  // namespace xpp
