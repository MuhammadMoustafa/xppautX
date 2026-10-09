#ifndef XPP_OPEN_MODEL_H
#define XPP_OPEN_MODEL_H
/* What File > Open model accepts, said once (W232): the dialog the core asks
   (model_switch.cpp), the desktop window's native one (xpp_window.cpp) and
   the start screen's Open model button all offer these. A .ode is converted
   on opening (docs/odex-quirks.md), a .odex is the model, a .snapx a saved
   session and a .recx a recording. C++ only. */
#include <array>
#include <string>
#include <string_view>

#include "recx.h"
#include "snapx.h"
#include "odex.h"

namespace xpp {

inline constexpr std::string_view OPEN_MODEL_TITLE = "Open model";

inline constexpr std::array<std::string_view, 4> OPEN_MODEL_EXTENSIONS = {".ode", odex::extension, snapx::extension,
                                                                           recx::extension};

/* the file selector's pattern: "*.ode *.odex *.snapx *.recx" */
inline std::string open_model_wild()
{
    std::string wild;
    for (std::string_view extension : OPEN_MODEL_EXTENSIONS) {
        if (!wild.empty()) wild += ' ';
        wild += '*';
        wild += extension;
    }
    return wild;
}

} // namespace xpp

#endif
