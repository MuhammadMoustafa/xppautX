/******************************************************************
 *                                                                *
 * File          : dense.c                                        *
 * Programmers   : Scott D. Cohen and Alan C. Hindmarsh @ LLNL    *
 * Last Modified : 1 September 1994                               *
 *----------------------------------------------------------------*
 * This is the implementation file for a generic DENSE linear     *
 * solver package.                                                *
 *                                                                *
 ******************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include "dense.h"

namespace xpp::cvode_solver {


#define ZERO 0.0
#define ONE  1.0


/* Implementation */


DenseMatRec *DenseAllocMat(integer N)
{
  if (N <= 0) return(NULL);

  /* The solver's raw handle (DenseFreeMat deletes it), its elements in
     its own std::vectors, zeroed: data[0] points to all N*N, data[j] to the jth column. */
  DenseMatRec *A = new DenseMatRec();
  A->values.assign(N * N, ZERO);
  A->columns.resize(N);
  for (integer j=0; j < N; j++) A->columns[j] = A->values.data() + j * N;
  A->data = A->columns.data();

  A->size = N;

  return(A);
}


void DenseZero(DenseMatRec *A)
{
  denzero(A->data, A->size);
}

void DenseCopy(DenseMatRec *A, DenseMatRec *B)
{
  dencopy(A->data, B->data, A->size);
}

void DenseScale(real c, DenseMatRec *A)
{
  denscale(c, A->data, A->size);
}

void DenseAddI(DenseMatRec *A)
{
  denaddI(A->data, A->size);
}

void DenseFreeMat(DenseMatRec *A)
{
  delete A;
}


void denzero(real **a, integer n)
{
  integer i, j;
  real *col_j;

  for (j=0; j < n; j++) {
    col_j = a[j];
    for (i=0; i < n; i++)
      col_j[i] =  ZERO;
  }
}

void dencopy(real **a, real **b, integer n)
{
  integer i, j;
  real *a_col_j, *b_col_j;

  for (j=0; j < n; j++) {
    a_col_j = a[j];
    b_col_j = b[j];
    for (i=0; i < n; i++)
      b_col_j[i] = a_col_j[i];
  }

}

void denscale(real c, real **a, integer n)
{
  integer i, j;
  real *col_j;

  for (j=0; j < n; j++) {
    col_j = a[j];
    for (i=0; i < n; i++)
      col_j[i] *= c;
  }
}

void denaddI(real **a, integer n)
{
  integer i;
  
  for (i=0; i < n; i++) a[i][i] += ONE;
}

} // namespace xpp::cvode_solver
