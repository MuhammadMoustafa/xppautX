#include "xpp_math.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include <array>
#include <vector>

namespace xpp {
namespace {
/* Complete-pivot search shared by ge and nlvc. Retain row-then-column scan,
   strict > ties and the zero initial magnitude, including all-zero/NaN input. */
void complete_pivot(integer n, integer leading, const double *a,
                    const std::vector<integer> &ir, const std::vector<integer> &ic,
                    integer first, integer &ipiv, integer &jpiv, double &piv)
{
    ipiv = first;
    jpiv = first;
    piv = 0.;
    for (integer i = first; i < n; ++i) {
        for (integer j = first; j < n; ++j) {
            const double p = f2c::abs(a[ir[i] + ic[j] * leading]);
            if (p > piv) {
                piv = p;
                ipiv = i;
                jpiv = j;
            }
        }
    }
}

/* One trailing row, shared without reordering ge's subsequent RHS updates.
   The ratio is computed once, columns updated in permutation order, and zero
   multipliers skip every update exactly as in both pre-W337 routines. */
double complete_eliminate_row(integer n, integer leading, double *a,
                              const std::vector<integer> &ir, const std::vector<integer> &ic,
                              integer first, integer row)
{
    const double rm = a[ir[row] + ic[first] * leading] / a[ir[first] + ic[first] * leading];
    if (rm != 0.) {
        for (integer i = first + 1; i < n; ++i)
            a[ir[row] + ic[i] * leading] -= rm * a[ir[first] + ic[i] * leading];
    }
    return rm;
}
} // namespace

/* W337: AUTO eigenvalues: balanced EISPACK rg (elementary Hessenberg reduction,
   machine-precision QR), no eigenvectors; preserve branch diagnostics.
   Gear uses unbalanced Householder reduction and fixed 1e-10 QR instead. */
/* Subroutine */ int 
eig(iap_type *iap, integer *ndim, integer *m1a, doublereal *a, doublecomplex *ev, integer *ier)
{
    /* Local variables */
  integer matz, ntop, ntot, i;

  integer ibr;

  std::vector<doublereal> z__(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> wi(iap->ndim);
  std::vector<doublereal> wr(iap->ndim);
  std::vector<doublereal> fv1(iap->ndim);
  std::vector<integer> iv1(iap->ndim);

  /* This subroutine uses the EISPACK subroutine RG to compute the */
  /* eigenvalues of the general real matrix A. */
  /* NDIM is the dimension of A. */
  /* M1A is the first dimension of A as in the DIMENSION statement. */
  /* The eigenvalues are to be returned in the complex vector EV. */

/* Local */

  /* Parameter adjustments */
  /*--ev;*/

  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  *ier = 0;
  matz = 0;

  rg(*m1a, *ndim, a, wr.data(), wi.data(), matz, z__.data(), iv1.data(), fv1.data(), ier);

  for (i = 0; i < *ndim; ++i) {
    ev[i].r = wr[i], ev[i].i = wi[i];
  }

  if (*ier != 0) {
    *ier = 1;
  }
  if (*ier == 1) {
    xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Error return from EISPACK routine RG\n",ibr,ntop);
    xpp::auto_fail("Error return from EISPACK routine RG");
  }

  return 0;
} /* eig_ */

/* W337: Complete-pivot null vector: the same strict > search as ge, but only n-k
   elimination stages and its own free-component/backsubstitution order.
   Share only the exact pivot search and trailing-row updates with ge; retain
   NLVC's free components, singular divisions and diagnostics separately. */
/* Subroutine */ int 
nlvc(FILE *fp9, integer n, integer m, integer k, doublereal *a, doublereal *u)
{
  /* System generated locals */
  integer a_dim1;

    /* Local variables */
  integer ipiv, jpiv, i, j, l;
  integer i1, jj, kk;
  doublereal sm;
  integer ip1, nmk;
  doublereal piv;
  integer jjp1;

  std::vector<integer> ir(n);
  std::vector<integer> ic(n);

/* Finds a null-vector of a singular matrix A. */
/* The null space of A is assumed to be K-dimensional. */

/* Parameters : */

/*     N : number of equations, */
/*     M : first dimension of A from DIMENSION statement, */
/*     K : dimension of nullspace, */
/*     A : N * N matrix of coefficients, */
/*     U : on exit U contains the null vector, */
/* IR,IC : integer arrays of dimension at least N. */

  /* Parameter adjustments */
  /*--u;*/
  /*--ir;*/
  /*--ic;*/
  a_dim1 = m;
  
  for (i = 0; i < n; ++i) {
    ic[i] = i;
    ir[i] = i;
  }

  /*   Elimination. */

  nmk = n - k;

  for (jj = 0; jj < nmk; ++jj) {
    complete_pivot(n, a_dim1, a, ir, ic, jj, ipiv, jpiv, piv);
    if (piv < auto_rsmall) {
      xpp::print(fp9,"        NOTE:Pivot {:3} < {:10.3E}  in NLVC : A null space may be multi-dimensional\n",jj,auto_rsmall);	

    }

    kk = ir[jj];
    ir[jj] = ir[ipiv];
    ir[ipiv] = kk;

    kk = ic[jj];
    ic[jj] = ic[jpiv];
    ic[jpiv] = kk;

    jjp1 = jj + 1;
    for (l = jjp1; l < n; ++l) {
      complete_eliminate_row(n, a_dim1, a, ir, ic, jj, l);
    }
  }

  /*   Backsubstitution : */

  for (i = 0; i < k; ++i) {
    u[ic[-1 + n - i]] = 1.;
  }

  for (i1 = 0 ; i1 < nmk; ++i1) {
    i = nmk - i1 - 1;
    sm = 0.;
    ip1 = i + 1;
    for (j = ip1; j <  n; ++j) {
      sm += ARRAY2D(a, ir[i], ic[j]) * u[ic[j]];
    }
    u[ic[i]] = -sm / ARRAY2D(a, ir[i], ic[i]);
  }

  /* Since the pointers for ir and ic were adjusted we need to put the
       back here. */
  /*ir \+= 1;*/
  /*ic \+= 1;*/
  return 0;
} /* nlvc_ */

/* W337: Complete pivoting: row-major search over permutation indices, strict > ties,
   unchanged determinant product/sign order and fort.9 pivot diagnostics.
   LINPACK sgefa uses partial pivoting and updates stored multipliers instead;
   substituting it changes both rounding and AUTO diagnostics. */
/* Subroutine */ int 
ge(FILE *fp9, integer iid, integer n, integer m1a, doublereal *a, integer nrhs, integer ndxloc, doublereal *u, integer m1f, doublereal *f, doublereal *det)
{
  /* System generated locals */
  integer a_dim1, u_dim1, f_dim1;

  /* Local variables */
  integer ipiv, jpiv, i, j, k, l;
  integer i1, jj;
  doublereal rm, sm;
  integer ip1, irh;
  doublereal piv;
  integer jjp1;

  std::vector<integer> ic(n);
  std::vector<integer> ir(n);

/* Solves the linear system  A U = F by Gauss elimination */
/* with complete pivoting. */

/* Parameters : */

/*   N   : number of equations, */
/*   M1A : first dimension of A from DIMENSION statement, */
/*   A   : N * N matrix of coefficients, */
/*   NRHS: 0   if no right hand sides (determinant only), */
/*         >0   if there are NRHS right hand sides, */
/*   ndxloc : first dimension of U from DIMENSION statement, */
/*   U   : on exit U contains the solution vector(s), */
/*   M1F : first dimension of F from DIMENSION statement, */
/*   F   : right hand side vector(s), */
/*  IR,IC: integer vectors of dimension at least N. */

/* The input matrix A is overwritten. */

  /* Parameter adjustments */
  /*--ir;*/
  /*--ic;*/
  a_dim1 = m1a;
  u_dim1 = ndxloc;
  f_dim1 = m1f;
  
  for (i = 0; i < n; ++i) {
    ic[i] = i;
    ir[i] = i;
  }

  /*   Elimination. */

  *det = 1.;

  for (jj = 0; jj < n - 1; ++jj) {
    complete_pivot(n, a_dim1, a, ir, ic, jj, ipiv, jpiv, piv);
    *det *= ARRAY2D(a, ir[ipiv], ic[jpiv]);
    if (iid >= auto_ge_pivot_iid) {
      if(jj==0) xpp::print(fp9,"\n Pivots in GE");
      if((jj%6)==0) xpp::print(fp9,"\n");
      xpp::print(fp9," {:4} {:12.3e} ",jj,f2c::abs(ARRAY2D(a, ir[ipiv], ic[jpiv])));
    }
    if (ipiv != jj) {
      *det = -(*det);
    }
    if (jpiv != jj) {
      *det = -(*det);
    }

    if (iid >= auto_ge_note_iid && piv < auto_rsmall) {
      xpp::print(fp9,"         NOTE:Pivot {:3} < {:10.3E}, in GE\n",jj,auto_rsmall);	
    }

    k = ir[jj];
    ir[jj] = ir[ipiv];
    ir[ipiv] = k;

    k = ic[jj];
    ic[jj] = ic[jpiv];
    ic[jpiv] = k;

    jjp1 = jj + 1;
    for (l = jjp1; l < n; ++l) {
      
      rm = complete_eliminate_row(n, a_dim1, a, ir, ic, jj, l);
      if (rm != 0.) {
	if (nrhs != 0) {
	  for (irh = 0; irh < nrhs; ++irh) {
	    ARRAY2D(f, ir[l], irh) -= rm * ARRAY2D(f, ir[jj], irh);
	  }
	}
      }
    }
  }
  *det *= ARRAY2D(a, ir[n - 1], ic[n - 1]);
  if (iid >= auto_ge_pivot_iid) {
    if((jj%6)==0) xpp::print(fp9,"\n");
    xpp::print(fp9," {:4} {:12.3e} \n",n-1,ARRAY2D(a, ir[n - 1], ic[n - 1]));
  }

  if (nrhs == 0) {
    /* Since the pointers for ir and ic were adjusted we need to put the
       back here. */
    /*ir \+= 1;*/
    /*ic \+= 1;*/
    return 0;
  }

  /*   Backsubstitution : */

  for (irh = 0; irh < nrhs; ++irh) {
    if(ARRAY2D(a, ir[n - 1], ic[n - 1]) == 0) {
      xpp::auto_fail("division by zero: the linear system of a Newton step is singular");
    }
    ARRAY2D(u, ic[n - 1], irh) = ARRAY2D(f, ir[n - 1], irh) / ARRAY2D(a, ir[n - 1], ic[n - 1]);
    for (i1 = 0; i1 < n - 1; ++i1) {
      i = n - (i1 + 1) - 1;
      sm = 0.;
      ip1 = i + 1;
      for (j = ip1; j < n; ++j) {
	sm += ARRAY2D(a, ir[i], ic[j]) * ARRAY2D(u, ic[j], irh);
      }
      ARRAY2D(u, ic[i], irh) = (ARRAY2D(f, ir[i], irh) - sm) / ARRAY2D(a, ir[i], ic[i]);
    }
  }

  /* Since the pointers for ir and ic were adjusted we need to put the
       back here. */
  /*ir \+= 1;*/
  /*ic \+= 1;*/

  return 0;
} /* ge_ */

/* autlib4.f -- translated by f2c (version 19970805).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/



/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*    Floquet Multiplier Computation (Tom Fairgrieve, U. of Toronto) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  References: */
/*  T. F. Fairgrieve, PhD Thesis, University of Toronto, 1994. */
/*  T. F. Fairgrieve, A. D. Jepson, O.K. Floquet multipliers, */
/*  SIAM J. Numer. Anal. 28. No. 5, 1991, 1446-1462. */

/*  Please inform Tom Fairgrieve (tff@na.utoronto.ca) of any */
/*  modifications to or errors in these routines. */
/*  Mailing Address: T.F. Fairgrieve, Department of Computer Science, */
/*  University of Toronto, Toronto, Ontario, CANADA  M5S 1A4C */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Floquet helper routines retained from autlib4: */

/*  subroutine flowkm : new routine to compute floquet multipliers */
/*  subroutine dhhpr  : compute a Householder matrix */
/*  subroutine dhhap  : appy a Householder matrix */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Required library routines (included in the file eispack.f) : */

/* subroutine qzhes  : QZ reduction to Hessenberg form             (EISPACK)*/
/* subroutine qzit   : QZ reduction to quasi-upper triangular form (EISPACK)*/
/* subroutine qzval  : QZ calculation of eigenvalues               (EISPACK)*/
/* function   epslon : machine constant routine                    (EISPACK)*/
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/* function   dnrm2  : compute l2-norm of a vector                 (BLAS-1)*/
/* function   ddot   : dot product of two vectors                  (BLAS-1)*/
/* subroutine dscal  : scale a vector by a constant                (BLAS-1)*/
/* function   idamax : find index of element with max abs value    (BLAS-1)*/
/* subroutine daxpy  : constant times a vector plus a vector       (BLAS-1)*/
/* subroutine drot   : apply a plane rotation                      (BLAS-1)*/
/* subroutine dswap  : swap two vectors                            (BLAS-1)*/
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  subroutine dgemc  : matrix-matrix copy */
/* subroutine xerbla : BLAS error handling routine                 (BLAS-2)*/
/* function   lsame  : compare character strings                   (BLAS-2)*/
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/* subroutine dgemm  : matrix-matrix multiply                      (BLAS-3)*/
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  subroutines ezsvd, ndrotg, ndsvd, prse, sig22, sigmin, sndrtg : */
/*                      Demmel-Kahan svd routines */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* W337: Floquet multipliers: Fairgrieve SVD deflation followed by generalized QZ,
   retaining reflector/rotation order, multiplier ratios and diagnostics.
   Ordinary rg or a matrix inverse followed by QR changes these operations. */
/* Subroutine */ int 
flowkm(FILE *fp9, integer *ndim, doublereal *c0, doublereal *c1, integer *iid, doublereal *rwork, doublecomplex *ev, bool deflate)
{
    

  /* System generated locals */
  integer c0_dim1, c1_dim1, rwork_dim1;

  /* Local variables */
  doublereal beta;
  std::array<doublereal, 1> svdu;


  integer i, j;


  logical infev;

  doublereal const__;

  integer ndimm1;
  doublereal nrmc0x, nrmc1x;
  integer svdinf;
  integer qzierr;
  std::array<doublereal, 1> qzz;

  std::vector<doublereal> svde(*ndim);
  std::vector<doublereal> svds(xpp::size_sum(*ndim, 1));
  std::vector<doublereal> svdv(xpp::size_product(*ndim, *ndim));
  std::vector<doublereal> v(*ndim);
  std::vector<doublereal> x(*ndim);
  std::vector<doublereal> qzalfi(*ndim);
  std::vector<doublereal> qzbeta(*ndim);
  std::vector<doublereal> qzalfr(*ndim);
  std::vector<doublereal> svdwrk(*ndim);

  /*  Subroutine to compute Floquet multipliers via the "deflated circuit */
  /*  pencil" method. This routine is called by the AUTO routine FNSPBV */

  /*  storage for SVD computations */

  /*  compute right singular vectors only */

  /*  storage for generalized eigenvalue computations */

  /*      LOGICAL           QZMATZ */
  /*  don't want to accumulate the transforms --- vectors not needed */

  /*  BLAS routines */


  /*  routines from EISPACK */


  /*  own routines */


  /*  Jim Demmel's svd routine  (demmel@nyu.edu) */


  /*  builtin F77 functions */

  /* xx   DOUBLE COMPLEX    DCMPLX */

  /*  Make sure that you have enough local storage. */

  /* Parameter adjustments */
  /*--ev;*/
  rwork_dim1 = *ndim;
  c1_dim1 = *ndim;
  c0_dim1 = *ndim;

  /* Change sign of P1 so that we get the sign of the multipliers right. */

  for (j = 0; j < *ndim; ++j) {
    for (i = 0; i < *ndim; ++i) {
      ARRAY2D(c1, i, j) = -ARRAY2D(c1, i, j);
    }
  }

  // A restored endpoint can be a constant solution, without the +1
  // multiplier of a nontrivial periodic orbit. Use its full pencil.
  if (!deflate) goto eigenvalues;

  /*  Print the undeflated circuit pencil (C0, C1). */

  if (*iid > 4) {
    xpp::print(fp9," Undeflated circuit pencil (C0, C1) \n");	

    xpp::print(fp9,"   C0 : \n");	

    for (i = 0; i < *ndim; ++i) {
      for (j = 0; j < *ndim; ++j) {
	xpp::print(fp9," {:23.16f}",ARRAY2D(c0, i, j));	
      }
      xpp::print(fp9,"\n");	

    }
    xpp::print(fp9,"   C1 : \n");	

    for (i = 0; i < *ndim; ++i) {
      for (j = 0; j < *ndim; ++j) {
	xpp::print(fp9," {:23.16f}",ARRAY2D(c1, i, j));
      }
      xpp::print(fp9,"\n");	

    }
  }

  /*  PART I: */
  /*  ======= */

  /*  Deflate the Floquet multiplier at +1.0 so that the deflated */
  /*  circuit pencil is not defective at periodic branch turning points. */

  /* The matrix (C0 - C1) should be (nearly) singular.  Find an approximatio
     n*/
  /*  to the right null vector (call it X).  This will be our approximation 
   */
  /*  to the eigenvector corresponding to the fixed multiplier at +1.0. */

  /*  There are many ways to get this approximation.  We could use */
  /*    1) p'(0) = f(p(0)) */
  /*    2) AUTO'86 routine NLVC applied to C0-C1 */
  /*    3) the right singular vector corresponding to the smallest */
  /*       singular value of C0-C1 */

  /*  I've chosen option 3) because it should introduce as little roundoff 
   */
  /* error as possible.  Although it is more expensive, this is insignifican
     t*/
  /* relative to the rest of the AUTO computations. Also, the SVD does give 
     a*/
  /*  version of the Householder matrix which we would have to compute */
  /* anyways.  But note that it gives V = ( X perp | X ) and not (X | Xperp)
     ,*/
  /* which the Householder routine would give.  This will permute the deflat
     ed*/
  /*  circuit pencil, so that the part to be deflated is in the last column,
   */
  /*  not it the first column, as was shown in the paper. */

  for (j = 0; j < *ndim; ++j) {
    for (i = 0; i < *ndim; ++i) {
      ARRAY2D(rwork, i, j) = ARRAY2D(c0, i, j) - ARRAY2D(c1, i, j);
    }
  }
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    doublereal tmp_tol = 1.0E-16;
    ezsvd(rwork, ndim, ndim, ndim, svds.data(), svde.data(), svdu.data(), &tmp, 
	  svdv.data(), ndim, svdwrk.data(), &tmp, &svdinf, &tmp_tol);
  }
  if (svdinf != 0) {
    xpp::print(fp9," NOTE : Warning from subroutine FLOWKM SVD routine returned SVDINF = {:4}        Floquet multiplier calculations may be wrong\n",svdinf);	

  }

  /*  Apply a Householder matrix (call it H1) based on the null vector */
  /*  to (C0, C1) from the right.  H1 = SVDV = ( Xperp | X ), where X */
  /*  is the null vector. */

  {          
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    doublereal tmp1 = 1.0;
    doublereal tmp0 = 0.0;
    logical tmp_false = FALSE_;

    dgemm("n", "n", ndim, ndim, ndim, &tmp1, c0, ndim, svdv.data(), 
	  ndim, &tmp0, rwork, ndim, 1L, 1L);
    dgemc(ndim, ndim, rwork, ndim, c0, ndim, &tmp_false);
    dgemm("n", "n", ndim, ndim, ndim, &tmp1, c1, ndim, svdv.data(), 
	  ndim, &tmp0, rwork, ndim, 1L, 1L);
    dgemc(ndim, ndim, rwork, ndim, c1, ndim, &tmp_false);
  }
  /*  Apply a Householder matrix (call it H2) based on */
  /*  (C0*X/||C0*X|| + C1*X/||C1*X||) / 2 */
  /*  to (C0*H1, C1*H1) from the left. */

  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    nrmc0x = dnrm2(ndim, &ARRAY2D(c0, 0, (*ndim - 1)), &tmp);
    nrmc1x = dnrm2(ndim, &ARRAY2D(c1, 0, (*ndim - 1)), &tmp);
  }
  for (i = 0; i < *ndim; ++i) {
    x[i] = (ARRAY2D(c0, i, (*ndim - 1)) / nrmc0x + ARRAY2D(c1, i, (*ndim - 1)) / nrmc1x) / 2.;
  }
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    integer tmp_left = auto_householder_left; 
    dhhpr(&tmp, ndim, ndim, x.data(), &tmp, &beta, v.data());
    dhhap(&tmp, ndim, ndim, ndim, &beta, v.data(), &tmp_left, c0, ndim);
    dhhap(&tmp, ndim, ndim, ndim, &beta, v.data(), &tmp_left, c1, ndim);
  }

  /* Rescale so that (H2^T)*C0*(H1)(1,NDIM) ~= (H2^T)*C1*(H1)(1,NDIM) ~= 1.0
   */

  /* Computing MAX */
  const__ = f2c::max(f2c::abs(ARRAY2D(c0, 0, (*ndim - 1))),f2c::abs(ARRAY2D(c1, 0, (*ndim - 1))));
  for (j = 0; j < *ndim; ++j) {
    for (i = 0; i < *ndim; ++i) {
      ARRAY2D(c0, i, j) /= const__;
      ARRAY2D(c1, i, j) /= const__;
    }
  }

  /*  Finished the deflation process! Print the deflated circuit pencil. */

  if (*iid > 4) {
    xpp::print(fp9," Deflated cicuit pencil (H2^T)*(C0, C1)*(H1) \n");	

    xpp::print(fp9,"   (H2^T)*C0*(H1) : \n");	

    for (i = 0; i < *ndim; ++i) {
      for (j = 0; j < *ndim; ++j) {
	xpp::print(fp9," {:23.16f}",ARRAY2D(c0, i, j));
      }
      xpp::print(fp9,"\n");	
    }
    xpp::print(fp9,"   (H2^T)*C1*(H1) : \n");	

    for (i = 0; i < *ndim; ++i) {
      for (j = 0; j < *ndim; ++j) {
	xpp::print(fp9," {:23.16f}",ARRAY2D(c1, i, j));
      }
      xpp::print(fp9,"\n");	

    }
  }

  /*  At this point we have */

  /*     (C0Bar, C1Bar) */
  /* ::= (H2^T)*(C0, C1)*(H1). */

  /*     (( B0^T     | Beta0  )  ( B1^T     | Beta1  ))  1 */
  /*   = (( ----------------- ), ( ----------------- )) */
  /*     (( C0BarDef | Delta0 )  ( C1BarDef | Delta1 )) NDIM-1 */

  /*         NDIM-1      1          NDIM-1      1 */

  /*  and approximations to the Floquet multipliers are */
  /*  (Beta0/Beta1) union the eigenvalues of the deflated pencil */
  /*  (C0BarDef, C1BarDef). */

  /*  PART II: */
  /*  ======== */

  /*  Compute the eigenvalues of the deflated circuit pencil */
  /*  (C0BarDef, C1BarDef) */
  /*  by using the QZ routines from EISPACK. */

eigenvalues:
  ndimm1 = deflate ? *ndim - 1 : *ndim;
  const integer offset = deflate ? 1 : 0;

  /*  reduce the generalized eigenvalue problem to a simpler form */
  /*   (C0BarDef,C1BarDef) = (upper hessenberg, upper triangular) */


  qzhes(*ndim, ndimm1, &c0[offset], &c1[offset], FALSE_ , qzz.data());

  /*  now reduce to an even simpler form */
  /*   (C0BarDef,C1BarDef) = (quasi-upper triangular, upper triangular) */

  qzit(*ndim, ndimm1, &c0[offset], &c1[offset], auto_qzeps1, FALSE_ ,
       qzz.data(), &qzierr);
  if (qzierr != 0) {
    xpp::print(fp9," NOTE : Warning from subroutine FLOWKM : QZ routine returned QZIERR = {:4}        Floquet multiplier calculations may be wrong \n",qzierr);	

  }

  /*  compute the generalized eigenvalues */

  qzval(*ndim, ndimm1, &c0[offset], &c1[offset], qzalfr.data(), qzalfi.data(),
	qzbeta.data(), FALSE_, qzz.data());

  /*  Pack the eigenvalues into complex form. */
  if (deflate) {
    ev[0].r = ARRAY2D(c0, 0, (*ndim - 1)) / ARRAY2D(c1, 0, (*ndim - 1));
    ev[0].i = 0.;
  }
  infev = FALSE_;
  for (j = 0; j < ndimm1; ++j) {
    if (qzbeta[j] != 0.) {
      ev[j + offset].r = qzalfr[j] / qzbeta[j];
      ev[j + offset].i = qzalfi[j] / qzbeta[j];
    } else {
      ev[j + offset].r = 1e30, ev[j + offset].i = 1e30;
      infev = TRUE_;
    }
  }
  if (infev) {
    xpp::print(fp9," NOTE : Warning from subroutine FLOWKM : Infinite Floquet multiplier represented by CMPLX( 1.0D+30, 1.0D+30 )\n");	

  }


  return 0;

} /* flowkm_ */


/* ************************** */
/* *  Householder routines  * */
/* ************************** */


/*  Subroutines for performing Householder plane rotations. */

/*  DHHPR: for computing Householder transformations and */
/*  DHHAP: for applying them. */

/*  Ref: Golub and van Loan, Matrix Calcualtions, */
/*       First Edition, Pages 38-43 */

/* Subroutine */ int 
dhhpr(integer *k, integer *j, integer *n, doublereal *x, integer *incx, doublereal *beta, doublereal *v)
{

  /* Local variables */
  integer iend, jmkp1;

  integer i, l;
  doublereal m, alpha;

  integer istart;

    


  /*     IMPLICIT UNDEFINED (A-Z,a-z) */
  /*     .. Scalar Arguments .. */
  /*     .. Array Arguments .. */
  /*     .. */

  /*  Purpose */
  /*  ======= */

  /*  DHHPR  computes a Householder Plane Rotation (G&vL Alg. 3.3-1) */
  /*  defined by v and beta. */
  /*  (I - beta v vt) * x is such that x_i = 0 for i=k+1 to j. */

  /*  Parameters */
  /*  ========== */

  /*  K      - INTEGER. */
  /*           On entry, K specifies that the K+1st entry of X */
  /*           be the first to be zeroed. */
  /*           K must be at least one. */
  /*           Unchanged on exit. */

  /*  J      - INTEGER. */
  /*           On entry, J specifies the last entry of X to be zeroed. */
  /*           J must be >= K and <= N. */
  /*           Unchanged on exit. */

  /*  N      - INTEGER. */
  /*           On entry, N specifies the (logical) length of X. */
  /*           Unchanged on exit. */

  /*  X      - DOUBLE PRECISION array of DIMENSION at least */
  /*           ( 1 + ( N - 1 )*abs( INCX ) ). */
  /*           On entry, X specifies the vector to be (partially) zeroed. */
  /*           Unchanged on exit. */

  /*  INCX   - INTEGER. */
  /*           On entry, INCX specifies the increment for the elements of */
  /*           X. INCX must be > zero. If X represents part of a matrix, */
  /*           then use INCX = 1 if a column vector is being zeroed and */
  /*           INCX = NDIM if a row vector is being zeroed. */
  /*           Unchanged on exit. */

  /*  BETA   - DOUBLE PRECISION. */
  /*           BETA specifies the scalar beta. (see pg. 40 of G and v.L.) */

  /*  V      - DOUBLE PRECISION array of DIMENSION at least n. */
  /*           Is updated to be the appropriate Householder vector for */
  /*           the given problem. (Note: space for the implicit zeroes is */
  /*          assumed to be present. Will save on time for index translation
	      .)*/

  /*  -- Written by Tom Fairgrieve, */
  /*                Department of Computer Science, */
  /*                University of Toronto, */
  /*                Toronto, Ontario CANADA  M5S 1A4 */

  /*     .. Local Scalars .. */
  /*     .. External Functions from BLAS .. */
  /*     .. External Subroutines from BLAS .. */
  /*     .. Intrinsic Functions .. */

  /*     .. Executable Statements .. */

  /*  Test the input parameters. */

  /* Parameter adjustments */
  /*--v;*/
  /*--x;*/

    
  if (*k < 1 || *k > *j) {
    xpp::auto_fail("domain error for K in DHHPR");
  }
  if (*j > *n) {
    xpp::auto_fail("domain error for J in DHHPR");
  }
  if (*incx < 1) {
    xpp::auto_fail("domain error for INCX in DHHPR");
  }

  /*  Number of potential non-zero elements in V. */

  jmkp1 = *j - *k + 1;

  /*  Find M := max{ |x_k|, ... , |x_j| } */

  m = f2c::abs(x[-1 + idamax(&jmkp1, &x[-1 + *k], incx)]);

  /*  alpha := 0 */
  /*  For i = k to j */
  /*      v_i = x_i / m */
  /*      alpha := alpha + v_i^2    (i.e. alpha = vtv) */
  /*  End For */
  /*  alpha :=  sqrt( alpha ) */

  /*  Copy X(K)/M, ... , X(J)/M to V(K), ... , V(J) */

  if (*incx == 1) {
    for (i = *k - 1; i < *j; ++i) {
      v[i] = x[i] / m;
    }
  } else {
    iend = jmkp1 * *incx;
    istart = (*k - 1) * *incx + 1;
    l = *k;
    for (i = istart; *incx < 0 ? i >= iend : i <= iend; i += *incx) 
      {
	v[-1 + l] = x[-1 + i] / m;
	++l;
      }
  }

  /*  Compute alpha */
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    alpha = dnrm2(&jmkp1, &v[-1 + *k], &tmp);
  }
  /*  beta := 1/(alpha(alpha + |V_k|)) */

  *beta = 1. / (alpha * (alpha + f2c::abs(v[-1 + *k])));

  /*  v_k := v_k + sign(v_k)*alpha */

  v[-1 + *k] += d_sign(1.0, v[-1 + *k]) * alpha;

  /*  Done ! */

  return 0;

  /*     End of DHHPR. */

} /* dhhpr_ */

/* Subroutine */ int 
dhhap(integer *k, integer *j, integer *n, integer *q, doublereal *beta, doublereal *v, integer *job, doublereal *a, integer *lda)
{
  /* System generated locals */
  integer a_dim1;

    

    /* Local variables */

  integer jmkp1;
  doublereal s;
  integer col, row;

    


  /*     IMPLICIT LOGICAL (A-Z) */
  /*     .. Scalar Arguments .. */
  /*     .. Array Arguments .. */
  /*     .. */

  /*  Purpose */
  /*  ======= */

  /*  DHHAP applies a Householder Plane Rotation defined by v and beta */
  /*  to the matrix A.  If JOB = 1 then A := (I - beta*v*vt)A and if */
  /*  JOB = 2 then A := A(I - beta*v*vt). (See Golub and van Loan */
  /*  Alg. 3.3-2.) */

  /*  Parameters */
  /*  ========== */

  /*  K      - INTEGER. */
  /*           On entry, K specifies that the V(K) may be the first */
  /*           non-zero entry of V. */
  /*           K must be at least one. */
  /*           Unchanged on exit. */

/*  J      - INTEGER. */
/*           On entry, J specifies the last non-zero entry of V. */
/*           J must be >= K and <= N. */
/*           Unchanged on exit. */

/*  N      - INTEGER. */
/*           On entry, N specifies the row dimension of A. */
/*           Unchanged on exit. */

/*  Q      - INTEGER. */
/*           On entry, Q specifies the column dimension of A. */
/*           Unchanged on exit. */

/*  BETA   - DOUBLE PRECISION. */
/*           BETA specifies the scalar beta. (see pg. 40 of G and v.L.) */
/*           Unchanged on exit. */

/*  V      - DOUBLE PRECISION array of DIMENSION at least n. */
/*           Householder vector v. */
/*           Unchanged on exit. */

/*  JOB    - INTEGER. */
/*          On entry, JOB specifies the order of the Householder applicati
on.*/
/*           If JOB = 1 then A := (I - beta*v*vt)A and if JOB = 2 then */
/*           A := A(I - beta*v*vt) */
/*           Unchanged on exit. */

/*  A      - DOUBLE PRECISION array of DIMENSION at least */
/*           ( LDA, Q ). */
/*           On entry, A specifies the matrix to be transformed. */
/*           On exit, A specifies the transformed matrix. */

/*  LDA    - INTEGER. */
/*           On entry, LDA specifies the declared leading dimension of A. 
*/
/*           Unchanged on exit. */

/*  -- Written by Tom Fairgrieve, */
/*                Department of Computer Science, */
/*                University of Toronto, */
/*                Toronto, Ontario CANADA  M5S 1A4 */

/*     .. Local Scalars .. */
/*     .. External Functions from BLAS .. */

/*     .. Executable Statements .. */

/*  Test the input parameters. */

    /* Parameter adjustments */
    /*--v;*/
  a_dim1 = *lda;
    
  if (*job != 1 && *job != 2) {
    xpp::auto_fail("domain error for JOB in DHHAP");
  }
  if (*k < 1 || *k > *j) {
    xpp::auto_fail("domain error for K in DHHAP");
  }
  if (*job == 1) {
    if (*j > *n) {
      xpp::auto_fail("domain error for J in DHHAP");
    }
  } else {
    if (*j > *q) {
      xpp::auto_fail("domain error for J in DHHAP");
    }
  }

  /*  Minimum {row,col} dimension of update. */

  jmkp1 = *j - *k + 1;

  /*  If (JOB = 1) then */
  /*      For p = 1, ... , q */
  /*          s := beta*(v_k*a_k,p + ... + v_j*a_j,p) */
  /*          For i = k, ..., j */
  /*              a_i,p := a_i,p - s*v_i */
  /*          End For */
  /*      End For */
  /*  Else % JOB=2 */
  /*      For p = 1, ... , n */
  /*          s := beta*(v_k*a_p,k + ... + v_j*a_p,j) */
  /*          For i = k, ..., j */
  /*              a_p,i := a_p,i - s*v_i */
  /*          End For */
  /*      End For */
  /*  End If */

  if (*job == 1) {
    for (col = 0; col < *q; ++col) {
      {
	/* This is here since I don't want to change the calling sequence of the
	   BLAS routines. */
	integer tmp = 1;
	s = *beta * ddot(&jmkp1, &v[-1 + *k], &tmp, &ARRAY2D(a, -1 + *k, col), &tmp);
      }
      for (row = *k - 1; row < *j; ++row) {
	ARRAY2D(a, row, col) -= s * v[row];
      }
    }
  } else {
    for (row = 0; row < *n; ++row) {
      {
	/* This is here since I don't want to change the calling sequence of the
	   BLAS routines. */
	integer tmp = 1;
	s = *beta * ddot(&jmkp1, &v[-1 + *k], &tmp, &ARRAY2D(a, row, (*k - 1)), lda);
      }
      for (col = *k - 1; col < *j; ++col) {
	ARRAY2D(a, row, col) -= s * v[col];
      }
    }
  }
    
  /*  Done ! */

  return 0;

  /*     End of DHHAP. */

} /* dhhap_ */















/* W337: HomCont eigenvectors: rg with vectors, optional transpose, ascending real
   roots, conjugate-column unpacking and previous-vector sign continuity.
   Gear computes no vectors; its fixed-tolerance QR cannot replace this path. */
/* Subroutine */ int 
eigho(const iap_type *iap, integer *isign, integer *itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm, doublereal *dfdu, doublereal *dfdp, doublereal *zz)
{
  /* System generated locals */
  integer dfdu_dim1, zz_dim1;

  /* Local variables */

  integer i, j, k, ifail;
  doublereal vdot;

  std::vector<doublereal> f(*ndm);
  std::vector<doublereal> ridum(*ndm);
  std::vector<doublereal> vidum(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> rrdum(*ndm);
  std::vector<doublereal> vrdum(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> vi(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> vr(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> fv1(*ndm);
  std::vector<integer> iv1(*ndm);

  /* Uses EISPACK routine RG to calculate the eigenvalues/eigenvectors */
  /* of the linearization matrix a (obtained from DFHO) and orders them */
  /* according to their real parts. Simple continuity with respect */
  /* previous call with same value of ITRANS. */

  /* 	input variables */
  /* 		ISIGN  = 1 => left-hand endpoint */
  /*       	       = 2 => right-hand endpoint */
  /*               ITRANS = 1 use transpose of A */
  /*                      = 2 otherwise */

/*       output variables */
/* 		RR,RI real and imaginary parts of eigenvalues, ordered w.r.t */
/* 	           real parts (largest first) */
/* 	        VRET the rows of which are real parts of corresponding */
/*                  eigenvectors */

/* Local */

    /* Parameter adjustments */
    /*--rr;*/
    /*--ri;*/
    /*--xequib;*/
    /*--icp;*/
    /*--par;*/
  vret -= ((*ndm)+1);
  zz_dim1 = *ndm;
  dfdu_dim1 = *ndm;
    
  ifail = 0;

  func(*iap->lib->session, *ndm, xequib, icp, iap->nfpr, par, 1, f.data(), dfdu, 
       dfdp);

  if (*itrans == 1) {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	vrdum[i + j * (*ndm)] = ARRAY2D(dfdu, j, i);
      }
    }
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	ARRAY2D(dfdu, i, j) = vrdum[i + j * (*ndm)];
      }
    }
  }

  /* EISPACK call for eigenvalues and eigenvectors */
  rg(*ndm, *ndm, dfdu, rr, ri, 1, zz, 
     iv1.data(), fv1.data(), &ifail);

  if (ifail != 0) {
    xpp::print(iap->lib->fp9,"EISPACK EIGENVALUE ROUTINE FAILED !\n");	
  }

  for (j = 0; j < *ndm; ++j) {
    if (ri[j] > 0.) {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, j);
	vi[i + j * (*ndm)] = ARRAY2D(zz, i, (j + 1));
      }
    } else if (ri[j] < 0.) {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, (j - 1));
	vi[i + j * (*ndm)] = -ARRAY2D(zz, i, j);
      }
    } else {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, j);
	vi[i + j * (*ndm)] = 0.;
      }
    }
  }
  /*Order the eigenvectors/values according size of real part of eigenvalue.
*/
/*     (smallest first) */

  for (i = 0; i < *ndm - 1; ++i) {
    for (j = i + 1; j < *ndm; ++j) {
      if (rr[i] > rr[j]) {
	rrdum[i] = rr[i];
	ridum[i] = ri[i];
	rr[i] = rr[j];
	rr[j] = rrdum[i];
	ri[i] = ri[j];
	ri[j] = ridum[i];
	for (k = 0; k < *ndm; ++k) {
	  vrdum[k + i * (*ndm)] = vr[k + i * (*ndm)];
	  vr[k + i * (*ndm)] = vr[k + j * (*ndm)];
	  vr[k + j * (*ndm)] = vrdum[k + i * (*ndm)];
	  vidum[k + i * (*ndm)] = vi[k + i * (*ndm)];
	  vi[k + i * (*ndm)] = vi[k + j * (*ndm)];
	  vi[k + j * (*ndm)] = vidum[k + i * (*ndm)];
	}
      }
    }
  }

  /* Choose sign of real part of eigenvectors to be */
  /* commensurate with that of the corresponding eigenvector */
  /* from the previous call with the same value of ISIGN */

  if (iap->lib->homcont.blhme.ieigc[*itrans - 1] == 0) {
    for (j = 0; j < *ndm; ++j) {
      for (i = 0; i < *ndm; ++i) {
	iap->lib->homcont.blhme.vrprev[*itrans + (i * 2 + j * (*ndm) * 2) - 1] = vr[i + j * (*ndm)];
      }
    }
    iap->lib->homcont.blhme.ieigc[*itrans - 1] = 1;
  }
  for (i = 0; i < *ndm; ++i) {
    vdot = 0.;
    {
      integer tmp;
      tmp = *ndm;
      for (j = 0; j < tmp; ++j) {
	vdot += vr[j + i * tmp] * iap->lib->homcont.blhme.vrprev[*itrans + (j * 2 + i * tmp * 2) - 1];
      }
    }
    if (vdot < 0.) {
      for (j = 0; j < *ndm; ++j) {
	vr[j + i * (*ndm)] = -vr[j + i * (*ndm)];
	/*               VI(J,I)=-VI(J,I) */
      }
    }
    for (j = 0; j < *ndm; ++j) {
      iap->lib->homcont.blhme.vrprev[*itrans + (j * 2 + i * (*ndm) * 2) - 1] = vr[j + i * (*ndm)];
    }
  }

  /* Send back the transpose of the matrix of real parts of eigenvectors */
  for (i = 0; i < *ndm; ++i) {
    for (j = 0; j < *ndm; ++j) {
      vret[(i + 1) + (j + 1) * (*ndm)] = vr[j + i * (*ndm)];
    }
  }

  return 0;
} /* eigho_ */

/* W337: HomCont projections: Householder basis then ordered real Schur hqr3lc,
   caller compzero tolerance and Beyn continuity via complete-pivot ge.
   Keep basis, dot-product and solve order: rg eigenvectors are not this basis. */
/* Subroutine */ int 
prjctn(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, integer *imfd, integer *is, integer *itrans, integer *ndm, doublereal *dfdu, doublereal *dfdp)
{
  /* System generated locals */
  integer dfdu_dim1;

    /* Local variables */
  integer i, j, k;
  integer mcond, k1, k2, m0;

  doublereal det, eps;

  std::vector<doublereal> fdum(*ndm);
  std::vector<doublereal> cnow(xpp::size_product(*ndm, *ndm));
  std::vector<integer> type__(*ndm);
  std::vector<doublereal> a(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> d(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> v(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> ei(*ndm);
  std::vector<doublereal> er(*ndm);
  std::vector<doublereal> ort(*ndm);
  std::vector<doublereal> dum1(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> dum2(xpp::size_product(*ndm, *ndm));

  /* Compute NUNSTAB (or NSTAB) projection boundary condition functions */
  /*onto to the UNSTABLE (or STABLE) manifold of the appropriate equilibrium
   */

/*    IMFD   = -1 stable eigenspace */
/*           =  1 unstable eigenspace */
/*    ITRANS =  1 use transpose of A */
/*           =  2 otherwise */
/*    IS     =  I (1 or 2) implies use the ith equilibrium in XEQUIB */

/* Use the normalization in Beyn 1990 (4.4) to ensure continuity */
/* w.r.t parameters. */
/* For the purposes of this routine the "previous point on the */
/* branch" is at the values of PAR at which the routine was last */
/* called with the same values of IS and ITRANS. */

/* Local */

    /* Parameter adjustments */
    /*--xequib;*/
    /*--icp;*/
    /*--par;*/
  bound -= ((*ndm)+1);
  dfdu_dim1 = *ndm;
  
  func(*iap->lib->session, *ndm, xequib, icp, iap->nfpr, par, 1, fdum.data(), dfdu, dfdp);

  /* Compute transpose of A if ITRANS=1 */
  if (*itrans == 1) {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	a[i + j * (*ndm)] = ARRAY2D(dfdu, j, i);
      }
    }
  } else {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	a[i + j * (*ndm)] = ARRAY2D(dfdu, i, j);
      }
    }
  }

  /* Compute basis V to put A in upper Hessenberg form */
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    orthes((ndm), ndm, &tmp, ndm, a.data(), ort.data());
    ortran((ndm), ndm, &tmp, ndm, a.data(), ort.data(), v.data());
  }

  /* Force A to be upper Hessenberg */
  if (*ndm > 2) {
    for (i = 2; i < *ndm; ++i) {
      for (j = 0; j < i - 1; ++j) {
	a[i + j * (*ndm)] = 0.;
      }
    }
  }

  /* Computes basis to put A in "Quasi Upper-Triangular form" */
  /* with the positive (negative) eigenvalues first if IMFD =-1 (=1) */
  eps = iap->lib->homcont.blhma.compzero;
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    hqr3lc(a.data(), v.data(), ndm, &tmp, ndm, &eps, er.data(), ei.data(), type__.data(), (ndm), (ndm),
	   imfd);
  }
  /* Put the basis in the appropriate part of the matrix CNOW */
  if (*imfd == 1) {
    k1 = *ndm - iap->lib->homcont.blhom.nunstab + 1;
    k2 = *ndm;
  } else {
    k1 = 1;
    k2 = iap->lib->homcont.blhom.nstab;
  }
  mcond = k2 - k1 + 1;
  m0 = k1 - 1;

  for (i = k1 - 1; i < k2; ++i) {
    for (j = 0; j < *ndm; ++j) {
      cnow[i + j * (*ndm)] = v[j + (i - k1 + 1) * (*ndm)];
    }
  }

  /* Set previous matrix to be the present one if this is the first call */

    /* Note by Randy Paffenroth:  There is a slight problem here
       in that this array is used before it is assigned to,
       hence its value is, in general, undefined.  It has
       worked because the just happened to be filled
       with zeros, even though this is not guaranteed.*/
  if (iap->lib->homcont.beyn.iflag[*is + (*itrans *2 ) - 3] == 0) {
    for (i = k1 - 1; i < k2; ++i) {
      for (j = 0; j < *ndm; ++j) {
	iap->lib->homcont.beyn.cprev[i + (j + ((*is - 1) + ((*itrans - 1) * 2)) * (*ndm)) * (*ndm)] = cnow[i + j * (*ndm)];
	bound[(i + 1) + (j + 1) * (*ndm)] = cnow[i + j * (*ndm)];
      }
    }
    iap->lib->homcont.beyn.iflag[*is + (*itrans * 2) - 3] = 1;
    return 0;
  }

  /* Calculate the (transpose of the) BEYN matrix D and hence BOUND */
  for (i = 0; i < mcond; ++i) {
    for (j = 0; j < mcond; ++j) {
      dum1[i + j * (*ndm)] = 0.;
      dum2[i + j * (*ndm)] = 0.;
      {
	integer tmp;
	tmp = *ndm;
	for (k = 0; k < tmp; ++k) {
	  dum1[i + j * (tmp)] += iap->lib->homcont.beyn.cprev[i + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)] 
	    * cnow[j + m0 + k * (tmp)];
	  dum2[i + j * (tmp)] += iap->lib->homcont.beyn.cprev[i + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)] 
	    * iap->lib->homcont.beyn.cprev[j + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)];
	}
      }
    }
  }

  if (mcond > 0) {
    ge(iap->lib->fp9, iap->iid, mcond, *ndm, dum1.data(), mcond, *ndm, d.data(), *ndm,
       dum2.data(), &det);
  }

  for (i = 0; i < mcond; ++i) {
    for (j = 0; j < *ndm; ++j) {
      bound[(i + 1) + m0 + (j + 1) * (*ndm)] = 0.;
      for (k = 0; k < mcond; ++k) {
	bound[(i + 1) + m0 + (j + 1) * (*ndm)] += d[k + i * (*ndm)] * 
	  cnow[k + m0 + j * (*ndm)];
      }
    }
  }

  for (i = k1 - 1; i < k2; ++i) {
    for (j = 0; j < *ndm; ++j) {
      iap->lib->homcont.beyn.cprev[i + (j + ((*is - 1) + ((*itrans - 1) * 2)) * (*ndm)) * (*ndm)] = bound[(i + 1) + (j + 1) * (*ndm)];
    }
  }

  return 0;
} /* prjctn_ */
} // namespace xpp
