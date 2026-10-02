/* C23's roundeven (round to nearest, ties to even), for systems whose C
   library lacks it: MinGW's. Written for xppautX, built on Windows only.
   CORE-MATH's x86 objects call __builtin_roundeven, which is the SSE4.1
   instruction on a build that targets it (the FMA copies) and a call to this
   function on one that does not (the plain copies, which run on any x86-64).
   The sum below rounds to nearest even in the default rounding mode, the
   only one the program runs in; a number of 2^52 or more is integral already
   (and infinities and NaNs fall out of the comparison). */
#include <math.h>

double roundeven(double x)
{
    const double two52 = 4503599627370496.0;
    double a = fabs(x);
    if (!(a < two52)) return x;
    a = (a + two52) - two52;
    return copysign(a, x);
}
