 
/******************************************************************
 *                                                                *
 * File          : dense.h                                        *
 * Programmers   : Scott D. Cohen and Alan C. Hindmarsh @ LLNL    *
 * Last Modified : 1 September 1994                               *
 *----------------------------------------------------------------*
 * This is the header file for a generic DENSE linear solver      *
 * package. There are two sets of dense solver routines listed in *
 * this file: one set uses type DenseMatRec defined below and the    *
 * other set uses the type real ** for dense matrix arguments.    *
 * The two sets of dense solver routines make it easy to work     *
 * with two types of dense matrices:                              *
 *                                                                *
 * (1) The DenseMatRec *type is intended for use with large dense     *
 *     matrices whose elements/columns may be stored in           *
 *     non-contiguous memory locations or even distributed into   *
 *     different processor memories. This type may be modified to *
 *     include such distribution information. If this is done,    *
 *     then all the routines that use DenseMatRec must be modified   *
 *     to reflect the new data structure.                         *
 *                                                                *
 * (2) The set of routines that use real ** (and NOT the DenseMatRec *
 *     type) is intended for use with small matrices which can    *
 *     easily be allocated within a contiguous block of memory    *
 *     on a single processor.                                     *
 *                                                                *
 * Routines that work with the type DenseMatRec begin with "Dense".  *
 * The DenseAllocMat function allocates a dense matrix for use in *
 * the other DenseMatRec *routines listed in this file. Matrix        *
 * storage details are given in the documentation for the type    *
 * DenseMatRec *; DenseFreeMat frees it. The LU factorization and
 * its solve are BlockLu's (xpp_math.h), which gathers a matrix's
 * columns as DenseMatRec stores them.                            *
 *                                                                * 
 * Routines that work with real ** begin with "den". The             *
 * underlying matrix storage is described in                      *
 * the documentation for the type DenseMatRec.                       *
 *                                                                *
 ******************************************************************/
 

#pragma once


#include "llnltyps.h"
#include "vector.h"
#include <vector>
namespace xpp::cvode_solver {

 
/******************************************************************
 *                                                                *
 * Type: DenseMatRec                                                 *
 *----------------------------------------------------------------*
 * The type DenseMatRec is defined to be a pointer to a structure    *
 * with a size and a data field. The size field indicates the     *
 * number of columns (== number of rows) of a dense matrix, while *
 * the data field is a two dimensional array used for component   *
 * storage. The elements of a dense matrix are stored columnwise  *
 * (i.e columns are stored one on top of the other in memory). If *
 * A is of type DenseMatRec *, then the (i,j)th element of A (with     *
 * 0 <= i,j <= size-1) is given by the expression (A->data)[j][i] *
 * or by the expression (A->data)[0][j*n+i]. The macros below     *
 * allow a user to access efficiently individual matrix           *
 * elements without writing out explicit data structure           *
 * references and without knowing too much about the underlying   *
 * element storage. The only storage assumption needed is that    *
 * elements are stored columnwise and that a pointer to the jth   *
 * column of elements can be obtained via the DENSE_COL macro.    *
 * Users should use these macros whenever possible.               *
 *                                                                *
 ******************************************************************/

struct DenseMatRec {
  integer size;
  real  **data;
  /* the storage data points into, freed with the matrix by DenseFreeMat:
     values holds the size*size elements column by column, columns each
     column's first element (data is columns.data()), so data[0] points
     to all size*size of them and data[j] to the jth column */
  std::vector<real> values;
  std::vector<real *> columns;
};
 

/* DenseMatRec accessor macros */

 
/******************************************************************
 *                                                                *
 * Macro : DENSE_ELEM                                             *
 * Usage : DENSE_ELEM(A,i,j) = a_ij;  OR                          *
 *         a_ij = DENSE_ELEM(A,i,j);                              *
 *----------------------------------------------------------------*
 * DENSE_ELEM(A,i,j) references the (i,j)th element of the N by N *
 * DenseMatRec *A, 0 <= i,j <= N-1.                                   *
 *                                                                *
 ******************************************************************/



/******************************************************************
 *                                                                *
 * Macro : DENSE_COL                                              *
 * Usage : col_j = DENSE_COL(A,j);                                *
 *----------------------------------------------------------------*
 * DENSE_COL(A,j) references the jth column of the N by N         *
 * DenseMatRec *A, 0 <= j <= N-1. The type of the expression          *
 * DENSE_COL(A,j) is real *. After the assignment in the usage    *
 * above, col_j may be treated as an array indexed from 0 to N-1. *
 * The (i,j)th element of A is referenced by col_j[i].            *
 *                                                                *
 ******************************************************************/

#define DENSE_COL(A,j) ((A->data)[j])
 

/* Functions that use the DenseMatRec representation for a dense matrix */

 
/******************************************************************
 *                                                                *
 * Function : DenseAllocMat                                       *
 * Usage    : A = DenseAllocMat(N);                               *
 *            if (A == NULL) ... memory request failed            *
 *----------------------------------------------------------------*
 * DenseAllocMat allocates memory for an N by N dense matrix and  *
 * returns the storage allocated (type DenseMatRec). DenseAllocMat   *
 * returns NULL if the request for matrix storage cannot be       *
 * satisfied. See the above documentation for the type DenseMatRec   *
 * for matrix storage details.                                    * 
 *                                                                * 
 ******************************************************************/

DenseMatRec *DenseAllocMat(integer N);


/******************************************************************
 *                                                                *
 * Function : DenseZero                                           *
 * Usage    : DenseZero(A);                                       *
 *----------------------------------------------------------------*
 * DenseZero sets all the elements of the N by N matrix A to 0.0. *
 *                                                                *
 ******************************************************************/

void DenseZero(DenseMatRec *A);


/******************************************************************
 *                                                                *
 * Function : DenseCopy                                           *
 * Usage    : DenseCopy(A, B);                                    *
 *----------------------------------------------------------------*
 * DenseCopy copies the contents of the N by N matrix A into the  *
 * N by N matrix B.                                               *
 *                                                                *
 ******************************************************************/

void DenseCopy(DenseMatRec *A, DenseMatRec *B);


/******************************************************************
 *                                                                *
 * Function: DenseScale                                           *
 * Usage   : DenseScale(c, A);                                    *
 *----------------------------------------------------------------*
 * DenseScale scales the elements of the N by N matrix A by the   *
 * constant c and stores the result back in A.                    *
 *                                                                *
 ******************************************************************/

void DenseScale(real c, DenseMatRec *A);


/******************************************************************
 *                                                                *
 * Function : DenseAddI                                           *
 * Usage    : DenseAddI(A);                                       *
 *----------------------------------------------------------------*
 * DenseAddI adds the identity matrix to A and stores the result  *
 * back in A.                                                     *
 *                                                                *
 ******************************************************************/

void DenseAddI(DenseMatRec *A);


/******************************************************************
 *                                                                *
 * Function : DenseFreeMat                                        *
 * Usage    : DenseFreeMat(A);                                    *
 *----------------------------------------------------------------*
 * DenseFreeMat frees the memory allocated by DenseAllocMat for   *
 * the N by N matrix A.                                           *
 *                                                                *
 ******************************************************************/

void DenseFreeMat(DenseMatRec *A);


/* Functions that use the real ** representation for a dense matrix */

 
/******************************************************************
 *                                                                *
 * Function : denzero                                             *
 * Usage    : denzero(a,n);                                       *
 *----------------------------------------------------------------*
 * denzero(a,n) sets all the elements of the n by n dense matrix  *
 * a to be 0.0.                                                   *
 *                                                                *
 ******************************************************************/

void denzero(real **a, integer n);


/******************************************************************
 *                                                                *
 * Function : dencopy                                             *
 * Usage    : dencopy(a,b,n);                                     *
 *----------------------------------------------------------------*
 * dencopy(a,b,n) copies the n by n dense matrix a into the       *
 * n by n dense matrix b.                                         *
 *                                                                *
 ******************************************************************/

void dencopy(real **a, real **b, integer n);


/******************************************************************
 *                                                                *
 * Function : denscale                                            *
 * Usage    : denscale(c,a,n);                                    *
 *----------------------------------------------------------------*
 * denscale(c,a,n) scales every element in the n by n dense       *
 * matrix a by c.                                                 *
 *                                                                *
 ******************************************************************/

void denscale(real c, real **a, integer n);


/******************************************************************
 *                                                                *
 * Function : denaddI                                             *
 * Usage    : denaddI(a,n);                                       *
 *----------------------------------------------------------------*
 * denaddI(a,n) increments the n by n dense matrix a by the       *
 * identity matrix.                                               *
 *                                                                *
 ******************************************************************/

void denaddI(real **a, integer n);


} // namespace xpp::cvode_solver
