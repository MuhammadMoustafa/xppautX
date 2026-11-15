#pragma once
/* Text only, split from model_switch.h at W737 so the front end and the commands name the leave question without the program's
   model switch. */
#include <string>

namespace xpp {

/* The leave question's answers and their keys (W59d), and Quit's
   wording: the one source of the core's question (xpp_session_may_leave,
   xpp_quit) and of the page's own, which hello's `quit` carries (W110:
   the page asks it while a computation runs, docs/protocol.md "quit") */
inline constexpr const char *LEAVE_SAVE = "Save session";
inline constexpr const char *LEAVE_DONT_SAVE = "Don't save";
inline constexpr const char *LEAVE_KEYS = "sd";
/* What the leave question is asked for: Quit, or an update's restart or install (W691) */
enum class Leave { quit, restart, install };
/* The question for `leave`: "Quit xppautX? Save this session first?", "Restart xppautX to use the new version? Save this session
   first?", "Install the update and quit xppautX? Save this session first?"; naming the recording in progress when recording */
std::string leave_question(Leave leave, bool recording);

} // namespace xpp
