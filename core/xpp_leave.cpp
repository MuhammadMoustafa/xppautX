#include "xpp_leave.h"

#include <string_view>

#include "xpp_text.h"

namespace xpp {

std::string leave_question(Leave leave, bool recording)
{
  const std::string_view head=leave==Leave::restart?"Restart xppautX to use the new version?"
                             :leave==Leave::install?"Install the update and quit xppautX?"
                                                    :"Quit xppautX?";
  return xpp::format("{} {}",head,recording?"Save this session, and the recording in progress, first?":"Save this session first?");
}

} // namespace xpp
