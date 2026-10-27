 
/****************************************************************
 *                                                              *
 * File          : vector.h                                     *
 * Programmers   : Scott D. Cohen and Alan C. Hindmarsh @ LLNL  *
 * Last Modified : 1 September 1994                             *
 *--------------------------------------------------------------*
 *                                                              *
 * This is the header file for a generic VECTOR package. It     *
 * exports the type N_VectorContent.                                   *
 *                                                              *
 * Part I of this file contains declarations which are specific *
 * to the particular machine environment in which this version  *
 * of the vector package is to be used. This includes the       *
 * definition of N_VectorContent, as well as accessor macros    *
 * that allow the user to use efficiently the type N_VectorContent     *
 * without making explicit references to its underlying         *
 * representation. The underlying type of N_VectorContent *will always  *
 * be some pointer type.                                        *
 *                                                              *
 * Part II of this file contains the prototypes for the vector  *
 * kernels which operate on the type N_VectorContent. These prototypes *
 * are fixed for all implementations of the vector package. The *
 * definitions of the types real and integer are in the header  *
 * file llnltyps.h and these may be changed according to the    *
 * user's needs.                                                *
 *                                                              *
 * Important Note: N_VectorContent *arguments to arithmetic kernels     *
 * need not be distinct. Thus, for example, the call            *
 *                                                              *
 *         N_VLinearSum(a,x,b,y,y);    y <- ax+by               *
 *                                                              *
 * is legal.                                                    *
 *                                                              * 
 * This version of vector.h is for the ordinary sequential      *
 * machine environment. In the documentation given below, N is  *
 * the length of all N_VectorContent *parameters and x[i] denotes the   *
 * ith component of the N_VectorContent *x, where 0 <= i <= N-1.        *
 *                                                              *
 ****************************************************************/
 

#pragma once


#include "llnltyps.h"
#include <vector>
namespace xpp::cvode_solver {


/* Part I: Machine Environment-Dependent Declarations */


/* Environment: Sequential */

 
/***************************************************************
 *                                                             *
 * Type: N_VectorContent                                              *
 *-------------------------------------------------------------*
 * The type N_VectorContent pointer is an abstract vector type. The fields of *
 * its concrete representation should not be accessed          *
 * directly, but rather through the macros given below.        *
 *                                                             *
 * A user may assume that the N components of an N_VectorContent      *
 * are stored contiguously. A pointer to the first component   *
 * can be obtained via the macro N_VDATA.                      *
 *                                                             *
 ***************************************************************/

struct N_VectorContent {
  integer length;
  real   *data;
  /* N_VNew's components, which data points into (N_VFree deletes the
     whole vector). Empty for a vector made around the caller's own array
     (cvdense.cpp's jthCol, a local N_VectorContent over one column). */
  std::vector<real> storage;
};
 
 
/***************************************************************
 *                                                             *
 * Macros: N_VDATA, N_VLENGTH, N_VIth                          *
 *-------------------------------------------------------------*
 * In the descriptions below, the following user               *
 * declarations are assumed:                                   *
 *                                                             *
 * N_VectorContent *v; real *v_data, r; integer v_len, i;              *
 *                                                             *
 * (1) N_VDATA, N_VLENGTH                                      *
 *                                                             *
 *     These routines give individual access to the parts of   *
 *     an N_VectorContent.                                            *
 *                                                             *
 *     The assignment v_data=N_VDATA(v) sets v_data to be      *
 *     a pointer to the first component of v. The assignment   *
 *     N_VDATA(v)=v_data sets the component array of v to      *
 *     be v_data by storing the pointer v_data.                *  
 *                                                             *
 *     The assignment v_len=N_VLENGTH(v) sets v_len to be      *
 *     the length of v. The call N_VLENGTH(v)=len_v sets       *
 *     the length of v to be len_v.                            *
 *                                                             *
 * (2) N_VIth                                                  *
 *                                                             *
 *     In the following description, the components of an      *
 *     N_VectorContent are numbered 0..N-1, where N is the length of  *
 *     v.                                                      *
 *                                                             *
 *     The assignment r=N_VIth(v,i) sets r to be the value of  *
 *     the ith component of v. The assignment N_VIth(v,i)=r    *
 *     sets the value of the ith component of v to be r.       *
 *                                                             *
 * Notes..                                                     *
 *                                                             *
 * When looping over the components of an N_VectorContent *v, it is    *
 * more efficient to first obtain the component array via      *
 * v_data=N_VDATA(v) and then access v_data[i] within the      *
 * loop than it is to use N_VDATA(v,i) within the loop.        *
 *                                                             *
 ***************************************************************/ 

#define N_VDATA(v) (v->data)


 

/* Part II: N_VectorContent *Kernel Prototypes (Machine Environment-Independent) */

 
/***************************************************************
 *                                                             *
 * Memory Allocation and Deallocation: N_VNew, N_VFree         *
 *                                                             *
 ***************************************************************/


/***************************************************************
 *                                                             *
 * Function : N_VNew                                           *
 * Usage    : x = N_VNew(N);                          *
 *-------------------------------------------------------------*
 *                                                             *
 * Returns a new vector of length N, or NULL for a nonpositive N. *
 * Allocation failure follows the core's exception handling.      *
 *                                                             *
 ***************************************************************/

N_VectorContent *N_VNew(integer n);


/***************************************************************
 *                                                             *
 * Function : N_VFree                                          *
 * Usage    : N_VFree(x);                                      *
 *-------------------------------------------------------------*
 *                                                             *
 * Frees the N_VectorContent *x. It is illegal to use x after the call *
 * N_VFree(x).                                                 *
 *                                                             *
 ***************************************************************/

void N_VFree(N_VectorContent *x);
 
 
/***************************************************************
 *                                                             *
 * N_VectorContent *Arithmetic: N_VLinearSum, N_VConst, N_VProd,       *
 *                      N_VDiv, N_VScale, N_VAbs, N_VInv,      *
 *                      N_VAddConst                            *
 *                                                             *
 ***************************************************************/


/***************************************************************
 *                                                             *
 * Function  : N_VLinearSum                                    *
 * Operation : z = a x + b y                                   *
 *                                                             *
 ***************************************************************/

void N_VLinearSum(real a, N_VectorContent *x, real b, N_VectorContent *y, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VConst                                        *
 * Operation : z[i] = c for i=0, 1, ..., N-1                   *
 *                                                             *
 ***************************************************************/

void N_VConst(real c, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VProd                                         *
 * Operation : z[i] = x[i] * y[i] for i=0, 1, ..., N-1         *
 *                                                             *
 ***************************************************************/

void N_VProd(N_VectorContent *x, N_VectorContent *y, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VDiv                                          *
 * Operation : z[i] = x[i] / y[i] for i=0, 1, ..., N-1         *
 *                                                             *
 ***************************************************************/

void N_VDiv(N_VectorContent *x, N_VectorContent *y, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VScale                                        *
 * Operation : z = c x                                         *
 *                                                             *
 ***************************************************************/

void N_VScale(real c, N_VectorContent *x, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VAbs                                          *
 * Operation : z[i] = |x[i]|,   for i=0, 1, ..., N-1           *
 *                                                             *
 ***************************************************************/

void N_VAbs(N_VectorContent *x, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VInv                                          *
 * Operation : z[i] = 1.0 / x[i] for i = 0, 1, ..., N-1        *
 *-------------------------------------------------------------*
 *                                                             *
 * This routine does not check for division by 0. It should be *
 * called only with an N_VectorContent *x which is guaranteed to have  *
 * all non-zero components.                                    *
 *                                                             *
 ***************************************************************/

void N_VInv(N_VectorContent *x, N_VectorContent *z);


/***************************************************************
 *                                                             *
 * Function  : N_VAddConst                                     *
 * Operation : z[i] = x[i] + b   for i = 0, 1, ..., N-1        *
 *                                                             *
 ***************************************************************/

void N_VAddConst(N_VectorContent *x, real b, N_VectorContent *z);
 
 
/***************************************************************
 *                                                             *
 * N_VectorContent *Measures: N_VMaxNorm, N_VWrmsNorm, N_VMin          *
 *                                                             *
 ***************************************************************/


/***************************************************************
 *                                                             *
 * Function : N_VMaxNorm                                       *
 * Usage    : maxnorm = N_VMaxNorm(x);                         *
 *-------------------------------------------------------------*
 *                                                             *
 * Returns the maximum norm of x:                              *
 *                                                             *
 * -> max (i=0 to N-1) |x[i]|                                  *
 *                                                             *
 * Returns 0.0 if N <= 0.                                      *
 *                                                             *
 ***************************************************************/

real N_VMaxNorm(N_VectorContent *x);


/***************************************************************
 *                                                             *
 * Function : N_VWrmsNorm                                      *
 * Usage    : wrmsnorm = N_VWrmsNorm(x, w);                    *
 *-------------------------------------------------------------*
 *                                                             *
 * Returns the weighted root mean square norm of x with        *
 * weight vector w:                                            *
 *                                                             *
 * -> sqrt [(sum (i=0 to N-1) {(x[i] * w[i])^2}) / N]          *
 *                                                             *
 * Returns 0.0 if N <= 0.                                      *
 *                                                             *
 ***************************************************************/

real N_VWrmsNorm(N_VectorContent *x, N_VectorContent *w);


/***************************************************************
 *                                                             *
 * Function : N_VMin                                           *
 * Usage    : min = N_VMin(x);                                 *
 *-------------------------------------------------------------*
 *                                                             *
 * Returns min x[i] if N > 0 and returns 0.0 if N <= 0.        *
 *          i                                                  *
 *                                                             *
 ***************************************************************/

real N_VMin(N_VectorContent *x);
 
 
} // namespace xpp::cvode_solver
