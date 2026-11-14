#pragma once
#include "xpp_error.h"
namespace xpp { class Sink; struct Picture; }


namespace xpp {

/* the picture as a one-page PDF into out (a binary Sink); my_pdf.cpp */
Result<> write_pdf(const Picture &picture, Sink &out);

} // namespace xpp
