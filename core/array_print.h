#ifndef _array_print_h_
#define _array_print_h_
#include "xpp_error.h"

namespace xpp {

/* The four PostScript colour scales; these are also the form's allowed bounds. */
enum ArrayRenderType { ARRAY_GREYSCALE=-1, ARRAY_REDBLUE=0, ARRAY_ROYGBIV=1, ARRAY_PERIODIC=2 };

struct ArrayPicture;

/* array_print.cpp: Print arrayplot's PostScript file */
Result<> array_print(const ArrayPicture &picture);

} // namespace xpp
#endif
