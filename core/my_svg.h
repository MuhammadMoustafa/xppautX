#pragma once

#include "picture.h"

namespace xpp {

/* the picture as an SVG into out; my_svg.cpp */
Result<> write_svg(const Picture &picture, Writer &out);

} // namespace xpp
