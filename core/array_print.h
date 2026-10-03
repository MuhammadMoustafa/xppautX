#ifndef _array_print_h_
#define _array_print_h_
#include "xpp_error.h"

namespace xpp {

struct ArrayPicture;

/* array_print.cpp: Print arrayplot's PostScript file */
Result<> array_print(const ArrayPicture &picture);

} // namespace xpp
#endif
