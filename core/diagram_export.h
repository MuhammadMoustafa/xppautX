#pragma once
/* The diagram as a picture (PDF or SVG): the export module's, which reads the diagram autoX stored (diagram.h). */

#include <optional>
#include <string>
#include "xpp_error.h"

namespace xpp {
struct Session; /* session.h */

/* the diagram as a picture, in picture.h's IMAGE_FORMAT_PS or IMAGE_FORMAT_SVG (W53, issue #101; GIF has no vector picture to
   draw here), of the autoX view `view` when given, in the file given or asked for; nothing when the dialog is declined */
Result<> export_autox_picture(Session &s, int fmt, const std::optional<std::string> &file, std::optional<int> view = {});

} // namespace xpp
