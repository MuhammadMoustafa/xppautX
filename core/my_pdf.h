#pragma once
#include "xpp_error.h"
namespace xpp { class Writer; struct Picture; }


namespace xpp {

/* the picture as a one-page PDF into out (a binary Writer); my_pdf.cpp */
Result<> write_pdf(const Picture &picture, Writer &out);

} // namespace xpp
