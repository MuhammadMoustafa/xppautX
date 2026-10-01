/* f2c.h  --  Standard Fortran to C header file */

/**  barf  [ba:rf]  2.  "He suggested using FORTRAN, and everybody barfed."

	- From The Shogakukan DICTIONARY OF NEW ENGLISH (Second edition) */

#ifndef F2C_INCLUDE
#define F2C_INCLUDE

#include <stdlib.h>
#include <math.h>

typedef long int integer;
typedef float real;
typedef double doublereal;
typedef struct { doublereal r, i; } doublecomplex;
typedef integer logical;

#define TRUE_ (1)
#define FALSE_ (0)

#define ARRAY2D(array,i,j) array[(i) + (j) * array ## _dim1]
#define ARRAY3D(array,i,j,k) array[(i) + ((j)  + (k) * array ## _dim2) * array ## _dim1]

/* f2c's libF77 helpers (f2c_helpers.cpp) */
double d_imag(const doublecomplex *z);
double d_lg10(const doublereal *x);
double d_sign(doublereal a, doublereal b);
integer i_dnnt(const doublereal *x);
integer i_nint(const real *x);
double pow_dd(const doublereal *ap, const doublereal *bp);
double pow_di(const doublereal *ap, const integer *bp);
integer pow_ii(integer ap, integer bp);
double r_lg10(real x);
double z_abs(const doublecomplex *z);
void z_exp(doublecomplex *r, const doublecomplex *z);
void z_log(doublecomplex *r, const doublecomplex *z);

/* f2c's abs/fabs/min/max macros, as functions (W33c): the macros broke
   every C++ standard header included after this one. Same types (the
   usual arithmetic conversions of the macros' ?:) and the same results,
   which the standard ones do not always give: f2c::abs keeps -0.0 and a
   NaN's sign flips where std::fabs clears the sign bit, and std::min/max
   pick the other operand of an unordered (NaN) pair. The arguments are
   evaluated once where the macros evaluated the chosen one twice; every
   argument in AUTO's code is free of side effects. */
namespace f2c {
template <class T>
constexpr auto abs(T x) noexcept
{
  return x >= 0 ? x : -x;
}
template <class A, class B>
constexpr auto min(A a, B b) noexcept
{
  return a <= b ? a : b;
}
template <class A, class B>
constexpr auto max(A a, B b) noexcept
{
  return a >= b ? a : b;
}
} // namespace f2c
#endif
