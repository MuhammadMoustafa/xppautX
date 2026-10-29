#pragma once
#include "xpp_error.h"

namespace xpp {

struct ArrayPicture;
struct Session; /* session.h */

/* array_print.cpp: Print arrayplot's picture, a PDF: the cells, the colour scale and the labels, built as a
   Picture (picture.h) the way the plot windows' pictures are; false when replacing an existing file was declined */
Result<bool> array_print(const Session &s, const ArrayPicture &picture);

} // namespace xpp
