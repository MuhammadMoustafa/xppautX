#pragma once
#include "xpp_error.h"
namespace xpp { class Sink; struct Picture; }


namespace xpp {

/* the picture as an SVG into out; my_svg.cpp */
Result<> write_svg(const Picture &picture, Sink &out);

} // namespace xpp
