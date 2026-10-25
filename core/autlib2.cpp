#include "xpp_mem.h"
/* translated from AUTO's autlib2.f by f2c */

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_job.h" /* xppautX: cancel */

/* The sizes follow the Fortran's macros: nrow = ndim*ncol (rows of A),
   nclm = nrow + ndim (its columns), nrc = nbc + nint + 1 (rows of C and D),
   and each array holds ntst + 1 mesh intervals (the last is a spare). */
void AutoAssemblyScratch::resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t nbc,
                                 std::ptrdiff_t nint, std::ptrdiff_t parameters)
{
  dfdp.resize(xpp::size_product(ndim, parameters));
  dfdu.resize(xpp::size_product(ndim, ndim));
  uold.resize(ndim);
  f.resize(ndim);
  u.resize(ndim);
  wploc.resize(xpp::size_product(ncol, xpp::size_sum(ncol, 1)));
  prm.resize(parameters);
  residual_parameters.resize(parameters);
  if (wi.size() != xpp::size_sum(ncol, 1)) {
    wi.resize(xpp::size_sum(ncol, 1));
    wp.resize(xpp::size_product(ncol, xpp::size_sum(ncol, 1)));
    wt.resize(xpp::size_product(ncol, xpp::size_sum(ncol, 1)));
    wint(ncol + 1, wi.data());
    genwts(ncol, ncol + 1, wt.data(), wp.data());
  }
  dicd.resize(xpp::size_product(nint, xpp::size_sum(ndim, parameters)));
  ficd.resize(nint);
  dbc.resize(xpp::size_product(nbc, xpp::size_sum(xpp::size_product(2, ndim), parameters)));
  fbc.resize(nbc);
  uic.resize(ndim);
  uio.resize(ndim);
  uid.resize(ndim);
  uip.resize(ndim);
  ubc0.resize(ndim);
  ubc1.resize(ndim);
}

void SolvbvWork::resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t ntst, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t parameters)
{
  const std::size_t nrow = xpp::size_product(ndim,ncol);
  const std::size_t nclm = xpp::size_sum(nrow,ndim), nrc = xpp::size_sum(nbc,nint,1), nodes = xpp::size_sum(ntst,1);
  a.resize(xpp::size_product(nclm, nrow, nodes));
  b.resize(xpp::size_product(parameters, nrow, nodes));
  c.resize(xpp::size_product(nclm, nrc, nodes));
  d.resize(xpp::size_product(nrc, parameters));
  a1.resize(xpp::size_product(ndim, ndim, nodes));
  a2.resize(xpp::size_product(ndim, ndim, nodes));
  s1.resize(xpp::size_product(ndim, ndim, nodes));
  s2.resize(xpp::size_product(ndim, ndim, nodes));
  bb.resize(xpp::size_product(ndim, parameters, nodes));
  cc.resize(xpp::size_product(ndim, nrc, nodes));
  c2.resize(xpp::size_product(ndim, nrc, nodes));
  dd.resize(xpp::size_product(parameters, nrc, nodes));
  faa.resize(xpp::size_product(ndim, nodes));
  fcfc.resize(xpp::size_product(nrc, nodes));
  icf.resize(xpp::size_product(nclm, nodes));
  irf.resize(xpp::size_product(nrow, nodes));
  ipr.resize(xpp::size_product(ndim, nodes));
  icf2.resize(xpp::size_product(ndim, nodes));
  ft.resize(xpp::size_product(nrow, nodes));
  const auto reduced = xpp::size_sum(ndim, nrc);
  e.resize(xpp::size_product(reduced, reduced));
  fcc.resize(xpp::size_sum(reduced, xpp::size_product(2, ndim, ndim), 1));
  sol.resize(xpp::size_product(ndim, nodes));
  xe.resize(reduced);
}

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*           Setting up of the Jacobian and right hand side */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
solvbv(integer *ifst, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rds, integer *nllv, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{

  integer ndim;
  integer ncol, nclm, nfpr, nint, nrow, ntst;

  integer nbc, iid;
  doublereal det;
  xpp::AutoProblem ips;
  integer nrc;

  /* Sets up and solves the linear equations for one Newton/Chord iteration 
   */

  /* Most of the required memory is allocated below */
  /* This is an interesting section of code.  The main point
     is that setubv and conpar only get called when ifst
     is 1.  This is a optimization since you can solve
     the system using the previously factored jacobian.
     One thing to watch out for is that two seperate calls
     of solvbv_ talk to each other through these arrays,
     so it is only safe to get rid of them when ifst is
     1 (since their entries will then be recreated in conpar
     and setubv).
  */

  if (*ifst == 1) {
    /* the arrays are sized in one place, SolvbvWork::resize */
    iap->lib->solvbv.resize(iap->ndim, iap->ncol, iap->ntst, iap->nbc, iap->nint, iap->lib->parameter_count);
  }

  auto &ft = iap->lib->solvbv.ft;

  ndim = iap->ndim;
  ips = iap->ips;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nbc = iap->nbc;
  nint = iap->nint;
  iid = iap->iid;
  nfpr = iap->nfpr;
  nrc = nbc + nint + 1;
  nrow = ndim * ncol;
  nclm = nrow + ndim;
  
  if (ntst < 1) {
    xpp::auto_fail(xpp::format("Ntst is {}, less than 1; set Ntst (Numerics) to 1 or more", ntst));
  }
  
  if (*ifst == 1) {
    setubv(ndim, ips, ntst, ncol, nbc, nint, nfpr, nrc, nrow, nclm,
	   funi, bcni, icni, *ndxloc, iap, rap, par, icp, 
	   *rds, iap->lib->solvbv.a_matrix(nclm, nrow, ntst), iap->lib->solvbv.b_matrix(nfpr, nrow, ntst), iap->lib->solvbv.c_matrix(nclm, nrc, ntst), iap->lib->solvbv.d_matrix(nfpr, nrc), ft.data(), fc, rlcur, 
	   rlold, rldot, ups, uoldps, udotps, upoldp, dups, 
	   dtm, thl, thu, p0, p1);
  } else if (*nllv >= 0 || ips == xpp::AutoProblem::Homoclinic) {
    // A cycle fold uses a fixed RHS. HomCont still needs the boundary
    // evaluations that advance its projection continuity state.
    setrhs(&ndim, &ips, &ntst, &ncol, &nbc, &nint, &
	   nfpr, &nrc, &nrow, &nclm, funi, bcni, icni,
	   ndxloc, iap, rap, par, icp, rds, ft.data(), fc, rlcur, 
	   rlold, rldot, ups, uoldps, udotps, upoldp, dups, dtm, thl, 
	   thu, p0, p1);
  }

  if (iap->lib->setubv_stop && xpp::job::cancelled()) return 0; /* xppautX: cancel */

  brbd(*iap->lib, iap->lib->fp9, ft.data(), fc, p0, p1,
       ifst, &iid, nllv, &det, &ndim, &ntst, &nbc, &nrow, &nclm, &nfpr, &nrc);
  
  faft(ft.data(), fa, &ntst, &nrow, ndxloc);

  rap->det = det;
  return 0;
} /* solvbv_ */

int 
faft(doublereal *ff, doublereal *fa, integer *ntst, integer *nrow, integer *ndxloc)
{
  integer fa_dim1, ff_dim1;

  integer i, j;

  ff_dim1 = *nrow;
  fa_dim1 = *ndxloc;
    
  for (i = 0; i < *ntst; ++i) {
    for (j = 0; j < *nrow; ++j) {
      ARRAY2D(fa, i, j) = ARRAY2D(ff, j, i);
    }
  }

  return 0;
} /* faft_ */

int 
setrhs(integer *ndim, xpp::AutoProblem *ips, integer *na, integer *ncol, integer *nbc, integer *nint, integer *ncb, integer *nrc, integer *nra, integer *nca, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), integer *ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *fa, doublereal *fc, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1)
{
  integer ups_dim1, dups_dim1, uoldps_dim1, 
    udotps_dim1, upoldp_dim1, 
    fa_dim1, wt_dim1, wp_dim1, wploc_dim1;

  integer i, j, k, l, m;
  integer i1, j1, k1, l1;

  doublereal rlsum;
  integer ib, ic, jj;
  integer ic1;

  integer jp1;
  integer ncp1;
  doublereal dt,ddt;

  iap->lib->assembly.resize(iap->ndim, iap->ncol, iap->nbc, iap->nint, iap->lib->parameter_count);
  auto &dicd = iap->lib->assembly.dicd;
  auto &ficd = iap->lib->assembly.ficd;
  auto &dfdp = iap->lib->assembly.dfdp;
  auto &dfdu = iap->lib->assembly.dfdu;
  auto &uold = iap->lib->assembly.uold;
  auto &f = iap->lib->assembly.f;
  auto &u = iap->lib->assembly.u;
  auto &wploc = iap->lib->assembly.wploc;
  auto &wi = iap->lib->assembly.wi;
  auto &wp = iap->lib->assembly.wp;
  auto &wt = iap->lib->assembly.wt;
  auto &dbc = iap->lib->assembly.dbc;
  auto &fbc = iap->lib->assembly.fbc;
  auto &uic = iap->lib->assembly.uic;
  auto &uio = iap->lib->assembly.uio;
  auto &prm = iap->lib->assembly.prm;
  auto &uid = iap->lib->assembly.uid;
  auto &uip = iap->lib->assembly.uip;
  auto &ubc0 = iap->lib->assembly.ubc0;
  auto &ubc1 = iap->lib->assembly.ubc1;

  fa_dim1 = *nra;
  dups_dim1 = *ndxloc;
  upoldp_dim1 = *ndxloc;
  udotps_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
  wt_dim1 = iap->ncol+1;
  wp_dim1 = iap->ncol+1;
  wploc_dim1 = iap->ncol+1;

  /* Initialize to zero. */
  for (i = 0; i < *nrc; ++i) {
    fc[i] = 0.;
  }

  /* Set constants. */
  ncp1 = *ncol + 1;
  for (i = 0; i < *ncb; ++i) {
    par[icp[i]] = rlcur[i];
  }

  /* Generate FA : */

  for (jj = 0; jj < *na; ++jj) {
    j = jj;
    jp1 = j + 1;
    dt = dtm[j];
    ddt = 1. / dt;
    for (ic = 0; ic < *ncol; ++ic) {
      for (ib = 0; ib < ncp1; ++ib) {
	ARRAY2D(wploc, ib, ic) = ddt * ARRAY2D(wp, ib, ic);
      }
    }
    for (ic = 0; ic < *ncol; ++ic) {
      for (k = 0; k < *ndim; ++k) {
	u[k] = ARRAY2D(wt, *ncol, ic) * ARRAY2D(ups, jp1, k);
	uold[k] = ARRAY2D(wt, *ncol, ic) * ARRAY2D(uoldps, jp1, k);
	for (l = 0; l < *ncol; ++l) {
	  l1 = l * *ndim + k;
	  u[k] += ARRAY2D(wt, l, ic) * ARRAY2D(ups, j, l1);
	  uold[k] += ARRAY2D(wt, l, ic) * ARRAY2D(uoldps, j, l1);
	}
      }
      /*     ** Time evolution computations (parabolic systems) */
      if (xpp::problem_is_parabolic_evolution(*ips)) {
	rap->tivp = rlold[0];
      }
      for (i = 0; i < iap->lib->parameter_count; ++i) {
	prm[i] = par[i];
      }
      /* The residual reads only f: no Jacobian (docs/xppaut-findings.md #39). */
      (*funi)(iap, rap, *ndim, u.data(), uold.data(), icp, prm.data(), 0, f.data(), 
	      dfdu.data(), dfdp.data());
      ic1 = ic * *ndim;
      for (i = 0; i < *ndim; ++i) {
	ARRAY2D(fa, ic1 + i, jj) = f[i] - ARRAY2D(wploc, *ncol, ic) * ARRAY2D(ups, jp1, i);
	for (k = 0; k < *ncol; ++k) {
	  k1 = k * *ndim + i;
	  ARRAY2D(fa, ic1 + i, jj) -= ARRAY2D(wploc, k, ic) * ARRAY2D(ups, j, k1);
	}
      }
      /* L1: */
    }
    /* L2: */
  }

  /*     Generate FC : */

/*     Boundary conditions : */

  if (*nbc > 0) {
    for (i = 0; i < *ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, *na, i);
    }
    (*bcni)(iap, rap, *ndim, par, icp, *nbc, ubc0.data(), ubc1.data(), 
	    fbc.data(), 2, dbc.data());
    for (i = 0; i < *nbc; ++i) {
      fc[i] = -fbc[i];
    }
    /*       Save difference : */
    for (j = 0; j < *na + 1; ++j) {
      for (i = 0; i < *nra; ++i) {
	ARRAY2D(dups, j, i) = ARRAY2D(ups, j, i) - ARRAY2D(uoldps, j, i);
      }
    }
  }

  /*     Integral constraints : */
  if (*nint > 0) {
    for (jj = 0; jj < *na; ++jj) {
      j = jj;
      jp1 = j + 1;
      for (k = 0; k < ncp1; ++k) {
	for (i = 0; i < *ndim; ++i) {
	  i1 = k * *ndim + i;
	  j1 = j;
	  if (k + 1 == ncp1) {
	    i1 = i;
	  }
	  if (k + 1 == ncp1) {
	    j1 = jp1;
	  }
	  uic[i] = ARRAY2D(ups, j1, i1);
	  uio[i] = ARRAY2D(uoldps, j1, i1);
	  uid[i] = ARRAY2D(udotps, j1, i1);
	  uip[i] = ARRAY2D(upoldp, j1, i1);
	}
	(*icni)(iap, rap, *ndim, par, icp, *nint, uic.data(), 
		uio.data(), uid.data(), uip.data(), ficd.data(), 2, dicd.data());
	for (m = 0; m < *nint; ++m) {
	  fc[*nbc + m] -= dtm[j] * wi[k] * ficd[m];
	}
      }
    }
  }

  /*     Pseudo-arclength equation : */
  rlsum = 0.;
  for (i = 0; i < *ncb; ++i) {
    rlsum += thl[icp[i]] * (rlcur[i] - rlold[i]) * rldot[i];
  }

  fc[-1 + *nrc] = *rds - rinpr(iap, ndim, ndxloc, udotps, 
			       dups, dtm, thu) - rlsum;

  return 0;
} /* setrhs_ */

namespace {
/* W474: the condensed blocks are reduced by AUTO-07p's nested dissection
   (solvebv.f90's REDUCER and BCKSUBR): blocks lo..hi split at
   mid=(lo+hi)/2, both halves are reduced, then the interface variable
   between them, x(mid+1), is eliminated. The tree depends on NTST alone,
   so the elimination order, and every bit of the solution, is the same at
   every thread count (AUTO-07p cuts its ranges by thread count, so its last
   bits change with it); it is AUTO-07p's own order on one thread. The
   subtrees at depth solve_chunk_levels are the chunks the collocation
   workers share, the merges above them run on the caller; a mesh with
   fewer intervals than chunks simply has single-block chunks, which have
   nothing to merge. Block k's equations couple x(k) and x(k+1); a reduced
   range lo..hi keeps its rows in slot hi, coupled to x(lo) through a1's slot
   lo and to x(hi+1) through a2's slot hi; the rows that eliminated x(mid+1)
   stay in slot mid (s1: x(lo), a2: x(mid+1), s2: x(hi+1)) for the back
   substitution. */
// 4 chunks, one for each of AutoParallel::max_threads, leaving three merges
// to the caller; no number depends on it (W474 measured 8 chunks 1.7% slower
// at four threads on Mu26's cycles, 1.2% faster at three).
constexpr int solve_chunk_levels = 2;
constexpr std::size_t solve_chunks = std::size_t{1} << solve_chunk_levels;
// A chunk's own subtree goes to its leaves.
constexpr int whole_subtree = std::numeric_limits<int>::max();

struct BlockRange { integer lo, hi; };
using RangeWork = std::function<void(integer lo, integer mid, integer hi)>;

/* AUTO-07p's MID=(LO+HI)/2: where lo..hi splits */
integer bisect(integer lo, integer hi) { return lo + (hi - lo) / 2; }

/* Calls work(lo,mid,hi) for the merges of lo..hi's bisection tree down to
   levels below it, children before their parent (up) or after it (down). */
void tree_up(integer lo, integer hi, int levels, const RangeWork &work)
{
  if (lo >= hi || levels == 0) return;
  const integer mid = bisect(lo, hi);
  tree_up(lo, mid, levels - 1, work);
  tree_up(mid + 1, hi, levels - 1, work);
  work(lo, mid, hi);
}

void tree_down(integer lo, integer hi, int levels, const RangeWork &work)
{
  if (lo >= hi || levels == 0) return;
  const integer mid = bisect(lo, hi);
  work(lo, mid, hi);
  tree_down(mid + 1, hi, levels - 1, work);
  tree_down(lo, mid, levels - 1, work);
}

void chunk_ranges(integer lo, integer hi, int levels, std::array<BlockRange, solve_chunks> &chunks, std::size_t &count)
{
  if (lo >= hi || levels == 0) { chunks[count++] = {lo, hi}; return; }
  const integer mid = bisect(lo, hi);
  chunk_ranges(lo, mid, levels - 1, chunks, count);
  chunk_ranges(mid + 1, hi, levels - 1, chunks, count);
}

/* Runs work on n independent blocks, on the collocation workers when there
   are some; the result does not depend on how they are shared. */
void for_blocks(AutoLib &lib, integer n, const std::function<void(integer begin, integer end)> &work)
{
  if (lib.collocation && lib.collocation->threads() > 1)
    lib.collocation->intervals(n, [&](std::ptrdiff_t begin, std::ptrdiff_t end, AutoLib &) { work(begin, end); });
  else work(0, n);
}

/* Runs walk on each chunk of blocks 0..na-1, a chunk on one thread. */
void for_chunks(AutoLib &lib, integer na, const std::function<void(integer lo, integer hi)> &walk)
{
  std::array<BlockRange, solve_chunks> chunks{};
  std::size_t count = 0;
  chunk_ranges(0, na - 1, solve_chunk_levels, chunks, count);
  for_blocks(lib, static_cast<integer>(count), [&](integer begin, integer end) {
    for (integer k = begin; k < end; ++k) walk(chunks[k].lo, chunks[k].hi);
  });
}

/* The chunks' merges, then the merges above them on the caller. */
void reduce_tree(AutoLib &lib, integer na, const RangeWork &merge)
{
  for_chunks(lib, na, [&](integer lo, integer hi) { tree_up(lo, hi, whole_subtree, merge); });
  tree_up(0, na - 1, solve_chunk_levels, merge);
}

/* The back substitution: the same tree from the root down. */
void backsubstitute_tree(AutoLib &lib, integer na, const RangeWork &solve)
{
  tree_down(0, na - 1, solve_chunk_levels, solve);
  for_chunks(lib, na, [&](integer lo, integer hi) { tree_down(lo, hi, whole_subtree, solve); });
}

/* The condensed system copycp builds, by block slot: nov x nov blocks
   (a1, a2, s1, s2: row + column*nov), nov x ncb (bb), the conditions'
   columns nov x nrc (cc: x(k), c2: x(k+1)), ncb x nrc (dd), the right-hand
   sides (faa: nov, fcfc: nrc), the row pivots and column order (ipr, icf2:
   nov). dd and fcfc gather what a range's merges subtract from the
   conditions' d and fc, so that no two chunks write the same numbers. */
struct CondensedBlocks {
  double *a1, *a2, *s1, *s2, *bb, *cc, *c2, *dd, *faa, *fcfc;
  integer *ipr, *icf2;
  integer nov, ncb, nrc, nbc;
  double *square(double *m, integer slot) const { return m + slot * nov * nov; }
  double *border(integer slot) const { return bb + slot * nov * ncb; }
  double *conditions(double *m, integer slot) const { return m + slot * nov * nrc; }
};

/* AUTO-07p's REDBLK in AUTO2000's arithmetic: eliminates x(mid+1) from
   the rows of lo..mid (slot mid) and mid+1..hi (slot hi) with complete
   pivoting over both, rows swapped physically, columns through icf2. */
void reduce_merge(const CondensedBlocks &m, integer lo, integer mid, integer hi)
{
  const integer nov = m.nov, ncb = m.ncb, nrc = m.nrc;
  double *s1l = m.square(m.s1, mid), *a2l = m.square(m.a2, mid), *s2l = m.square(m.s2, mid), *bbl = m.border(mid);
  double *a11 = m.square(m.a1, lo), *a12 = m.square(m.a1, mid + 1), *a22 = m.square(m.a2, hi), *bbr = m.border(hi);
  double *cc1 = m.conditions(m.cc, lo), *cc2 = m.conditions(m.cc, mid + 1), *cc3 = m.conditions(m.c2, hi);
  const double *c2l = m.conditions(m.c2, mid);
  double *ddr = m.dd + hi * ncb * nrc;
  const double *ddl = m.dd + mid * ncb * nrc;
  integer *perm = m.icf2 + mid * nov, *ipr = m.ipr + mid * nov;
  // every array here has the leading dimension nov: (i, j) at i + j*nov
  const auto at = [nov](double *block, integer row, integer column) -> double & { return block[row + column * nov]; };

  /* x(lo)'s columns of lo..mid's rows move to s1; a11 gathers those of
     mid+1..hi's, the merged range's coupling to x(lo) */
  for (integer k = 0; k < nov * nov; ++k) {
    s1l[k] = a11[k];
    a11[k] = 0.;
    s2l[k] = 0.;
  }
  for (integer k = 0; k < nov * nrc; ++k) cc2[k] += c2l[k];
  for (integer ir = m.nbc; ir < nrc; ++ir)
    for (integer l = 0; l < ncb; ++l) ddr[l + ir * ncb] += ddl[l + ir * ncb];
  for (integer k = 0; k < nov; ++k) perm[k] = k;

  for (integer ic = 0; ic < nov; ++ic) {
    double piv1 = 0.;
    integer ipiv1 = ic, jpiv1 = ic;
    for (integer k1 = ic; k1 < nov; ++k1)
      for (integer k2 = ic; k2 < nov; ++k2) {
        const double tpiv = std::fabs(at(a2l, k1, perm[k2]));
        if (piv1 < tpiv) { piv1 = tpiv; ipiv1 = k1; jpiv1 = k2; }
      }
    double piv2 = 0.;
    integer ipiv2 = 0, jpiv2 = ic;
    for (integer k1 = 0; k1 < nov; ++k1)
      for (integer k2 = ic; k2 < nov; ++k2) {
        const double tpiv = std::fabs(at(a12, k1, perm[k2]));
        if (piv2 < tpiv) { piv2 = tpiv; ipiv2 = k1; jpiv2 = k2; }
      }

    if (piv1 >= piv2) {
      ipr[ic] = ipiv1;
      std::swap(perm[ic], perm[jpiv1]);
      for (integer l = 0; l < nov; ++l) {
        std::swap(at(s1l, ic, l), at(s1l, ipiv1, l));
        if (l >= ic) std::swap(at(a2l, ic, perm[l]), at(a2l, ipiv1, perm[l]));
        std::swap(at(s2l, ic, l), at(s2l, ipiv1, l));
      }
      for (integer l = 0; l < ncb; ++l) std::swap(at(bbl, ic, l), at(bbl, ipiv1, l));
    } else {
      ipr[ic] = nov + ipiv2;
      std::swap(perm[ic], perm[jpiv2]);
      for (integer l = 0; l < nov; ++l) {
        if (l >= ic) std::swap(at(a2l, ic, perm[l]), at(a12, ipiv2, perm[l]));
        std::swap(at(s2l, ic, l), at(a22, ipiv2, l));
        std::swap(at(s1l, ic, l), at(a11, ipiv2, l));
      }
      for (integer l = 0; l < ncb; ++l) std::swap(at(bbl, ic, l), at(bbr, ipiv2, l));
    }

    const integer pc = perm[ic];
    const double pivot = at(a2l, ic, pc);
    for (integer ir = ic + 1; ir < nov; ++ir) {
      const double rm = at(a2l, ir, pc) / pivot;
      at(a2l, ir, pc) = rm;
      if (rm == 0.) continue;
      for (integer l = ic + 1; l < nov; ++l) at(a2l, ir, perm[l]) -= rm * at(a2l, ic, perm[l]);
      for (integer l = 0; l < nov; ++l) {
        at(s1l, ir, l) -= rm * at(s1l, ic, l);
        at(s2l, ir, l) -= rm * at(s2l, ic, l);
      }
      for (integer l = 0; l < ncb; ++l) at(bbl, ir, l) -= rm * at(bbl, ic, l);
    }
    for (integer ir = 0; ir < nov; ++ir) {
      const double rm = at(a12, ir, pc) / pivot;
      at(a12, ir, pc) = rm;
      if (rm == 0.) continue;
      for (integer l = ic + 1; l < nov; ++l) at(a12, ir, perm[l]) -= rm * at(a2l, ic, perm[l]);
      for (integer l = 0; l < nov; ++l) {
        at(a11, ir, l) -= rm * at(s1l, ic, l);
        at(a22, ir, l) -= rm * at(s2l, ic, l);
      }
      for (integer l = 0; l < ncb; ++l) at(bbr, ir, l) -= rm * at(bbl, ic, l);
    }
    for (integer ir = m.nbc; ir < nrc; ++ir) {
      const double rm = at(cc2, pc, ir) / pivot;
      at(cc2, pc, ir) = rm;
      if (rm == 0.) continue;
      for (integer l = ic + 1; l < nov; ++l) at(cc2, perm[l], ir) -= rm * at(a2l, ic, perm[l]);
      for (integer l = 0; l < nov; ++l) {
        at(cc1, l, ir) -= rm * at(s1l, ic, l);
        at(cc3, l, ir) -= rm * at(s2l, ic, l);
      }
      for (integer l = 0; l < ncb; ++l) ddr[l + ir * ncb] -= rm * at(bbl, ic, l);
    }
  }
}

/* AUTO-07p's REDRHSBLK: the right-hand side through reduce_merge's pivots and multipliers. */
void redrhs_merge(const CondensedBlocks &m, integer mid, integer hi)
{
  const integer nov = m.nov, nrc = m.nrc;
  const double *a2l = m.square(m.a2, mid), *a12 = m.square(m.a1, mid + 1), *cc2 = m.conditions(m.cc, mid + 1);
  double *faal = m.faa + mid * nov, *faar = m.faa + hi * nov, *fcr = m.fcfc + hi * nrc;
  const double *fcl = m.fcfc + mid * nrc;
  const integer *perm = m.icf2 + mid * nov, *ipr = m.ipr + mid * nov;
  for (integer ir = m.nbc; ir < nrc; ++ir) fcr[ir] += fcl[ir];
  for (integer ic = 0; ic < nov; ++ic) {
    const integer p = ipr[ic];
    std::swap(faal[ic], p < nov ? faal[p] : faar[p - nov]);
    const integer pc = perm[ic];
    for (integer ir = ic + 1; ir < nov; ++ir) faal[ir] -= a2l[ir + pc * nov] * faal[ic];
    for (integer ir = 0; ir < nov; ++ir) faar[ir] -= a12[ir + pc * nov] * faal[ic];
    for (integer ir = m.nbc; ir < nrc; ++ir) fcr[ir] -= cc2[pc + ir * nov] * faal[ic];
  }
}

/* AUTO-07p's BCKSUB1: x(mid+1) from slot mid's rows, x(lo), x(hi+1) and the parameters. */
void bcksub_merge(const CondensedBlocks &m, const double *parameters, double *sol, integer lo, integer mid, integer hi)
{
  const integer nov = m.nov, ncb = m.ncb;
  const double *s1 = m.square(m.s1, mid), *s2 = m.square(m.s2, mid), *a2 = m.square(m.a2, mid), *bb = m.border(mid);
  const double *faa = m.faa + mid * nov, *left = sol + lo * nov, *right = sol + (hi + 1) * nov;
  double *x = sol + (mid + 1) * nov;
  const integer *perm = m.icf2 + mid * nov;
  for (integer k = nov - 1; k >= 0; --k) {
    double sm = 0.;
    for (integer l = 0; l < nov; ++l) {
      sm += left[l] * s1[k + l * nov];
      sm += right[l] * s2[k + l * nov];
    }
    for (integer l = 0; l < ncb; ++l) sm += parameters[l] * bb[k + l * nov];
    for (integer l = k + 1; l < nov; ++l) sm += x[perm[l]] * a2[k + perm[l] * nov];
    const integer pk = perm[k];
    x[pk] = (faa[k] - sm) / a2[k + pk * nov];
  }
}

/* The reduction of the blocks; what it subtracts from the conditions' d is
   gathered by range and added once, at the root. */
void reduce(AutoLib &lib, const CondensedBlocks &m, integer na, double *d)
{
  std::fill_n(m.dd, na * m.ncb * m.nrc, 0.);
  reduce_tree(lib, na, [&](integer lo, integer mid, integer hi) { reduce_merge(m, lo, mid, hi); });
  const double *root = m.dd + (na - 1) * m.ncb * m.nrc;
  for (integer ir = m.nbc; ir < m.nrc; ++ir)
    for (integer l = 0; l < m.ncb; ++l) d[l + ir * m.ncb] += root[l + ir * m.ncb];
}

/* The right-hand side through the same tree; fc's share added at the root. */
void redrhs(AutoLib &lib, const CondensedBlocks &m, integer na, double *fc)
{
  std::fill_n(m.fcfc, na * m.nrc, 0.);
  reduce_tree(lib, na, [&](integer, integer mid, integer hi) { redrhs_merge(m, mid, hi); });
  const double *root = m.fcfc + (na - 1) * m.nrc;
  for (integer ir = m.nbc; ir < m.nrc; ++ir) fc[ir] += root[ir];
}

/* x(0) and x(na) from dimrge's solution fcc (and fc = fcc after x(0)),
   then every other x(k) down the tree, into sol (nov x (na+1)). */
void bcksub(AutoLib &lib, const CondensedBlocks &m, integer na, const double *fc, const double *fcc, double *sol)
{
  for (integer l = 0; l < m.nov; ++l) {
    sol[l] = fcc[l];
    sol[l + na * m.nov] = fc[l];
  }
  backsubstitute_tree(lib, na, [&](integer lo, integer mid, integer hi) { bcksub_merge(m, fc + m.nov, sol, lo, mid, hi); });
}

/* Determine the local variables by backsubstitution, blocks begin..end-1:
   no communication, so the blocks are shared among the workers. */
void infpar(const doublereal *a, const doublereal *b, doublereal *fa, const doublereal *sol, const doublereal *fc, integer begin, integer end, integer nov, integer nra, integer nca, integer ncb, const integer *irf, const integer *icf)
{
  const integer irf_dim1 = nra, fa_dim1 = nra, icf_dim1 = nca, a_dim1 = nca, a_dim2 = nra, b_dim1 = ncb, b_dim2 = nra;
  const integer nram = nra - nov;
  std::vector<doublereal> x(nra);
  for (integer i = begin; i < end; ++i) {
    const doublereal *left = sol + i * nov, *right = sol + (i + 1) * nov;
    for (integer ir = nram - 1; ir >= 0; --ir) {
      doublereal sm = 0.;
      const integer irfir = ARRAY2D(irf, ir, i) - 1;
      for (integer j = 0; j < nov; ++j) {
        sm += ARRAY3D(a, j, irfir, i) * left[j];
        sm += ARRAY3D(a, nra + j, irfir, i) * right[j];
      }
      for (integer j = 0; j < ncb; ++j) sm += ARRAY3D(b, j, irfir, i) * fc[nov + j];
      for (integer j = ir + 1; j < nram; ++j) {
        const integer icfj1 = ARRAY2D(icf, j + nov, i) - 1;
        sm += ARRAY3D(a, icfj1, irfir, i) * x[icfj1];
      }
      const integer icfnovpir = ARRAY2D(icf, nov + ir, i) - 1;
      x[icfnovpir] = (ARRAY2D(fa, irfir, i) - sm) / ARRAY3D(a, icfnovpir, irfir, i);
    }
    /* Copy x(i) and the local variables into fa */
    for (integer j = 0; j < nov; ++j) ARRAY2D(fa, j, i) = left[j];
    for (integer j = nov; j < nra; ++j) ARRAY2D(fa, j, i) = x[j];
  }
}
} // namespace

int 
brbd(AutoLib &lib, FILE *fp9, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, integer *ifst, integer *idb, integer *nllv, doublereal *det, integer *nov, integer *na, integer *nbc, integer *nra, integer *nca, integer *ncb, integer *nrc)
{
  /* the matrices and index arrays solvbv sized and kept in lib.solvbv */
  SolvbvWork &work = lib.solvbv;
  doublereal *a = work.a.data(), *b = work.b.data(), *c = work.c.data(), *d = work.d.data();
  integer *irf = work.irf.data(), *icf = work.icf.data();
  const CondensedBlocks blocks{work.a1.data(), work.a2.data(), work.s1.data(), work.s2.data(), work.bb.data(),
    work.cc.data(), work.c2.data(), work.dd.data(), work.faa.data(), work.fcfc.data(),
    work.ipr.data(), work.icf2.data(), *nov, *ncb, *nrc, *nbc};

  if (*idb > 4) {
    print1(fp9, nov, na, nra, nca, ncb, nrc, a, b, c, d, fa, fc);
  }
  if (*ifst == 1) {
    conpar(lib, nov, na, nra, nca, a, ncb, b, nbc, nrc, c, d, irf, icf);
    copycp(na, nov, nra, nca, a, ncb, b, nrc, c, blocks.a1, blocks.a2, blocks.bb, blocks.cc, blocks.c2, irf);
  }

  if (*nllv == 0) {
    conrhs(nov, na, nra, nca, a, nbc, nrc, c, fa, fc, irf, icf);
  } else {
    setzero(fa, fc, na, nra, nrc);
  }
  cpyrhs(na, nov, nra, blocks.faa, fa, irf);

  if (*ifst == 1) {
    reduce(lib, blocks, *na, d);
  }
  if (*nllv == 0) {
    redrhs(lib, blocks, *na, fc);
  }

  dimrge(lib, fp9, work.e.data(), blocks.cc, blocks.c2, d, fc, ifst, na,
	 nrc, nov, ncb, idb, nllv, work.fcc.data(), p0, p1, det, blocks.a1, blocks.a2,
	 blocks.faa, blocks.bb);

  bcksub(lib, blocks, *na, fc, work.fcc.data(), work.sol.data());

  for_blocks(lib, *na, [&](integer begin, integer end) {
    infpar(a, b, fa, work.sol.data(), fc, begin, end, *nov, *nra, *nca, *ncb, irf, icf);
  });

  return 0;
} /* brbd_ */

int 
setzero(doublereal *fa, doublereal *fc, integer *na, integer *nra, integer *nrc)
{
  integer fa_dim1;

  integer i, j;

  fa_dim1 = *nra;
    
  for (i = 0; i < *na; ++i) {
    for (j = 0; j < *nra; ++j) {
      ARRAY2D(fa, j, i) = 0.;
    }
  }

  for (i = 0; i < *nrc; ++i) {
    fc[i] = 0.;
  }

  return 0;
} /* setzero_ */

int 
conrhs(integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *nbc, integer *nrc, doublereal *c, doublereal *fa, doublereal *fc, integer *irf, integer *icf)
{
  integer icf_dim1, irf_dim1, a_dim1, a_dim2, c_dim1, c_dim2, fa_dim1;

  integer i, icfic, irfir, m2, ic, ir, irfirp, ir1, nex,
    irp;

  irf_dim1 = *nra;
  fa_dim1 = *nra;
  icf_dim1 = *nca;
  a_dim1 = *nca;
  a_dim2 = *nra;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  nex = *nca - (*nov * 2);
  if (nex == 0) {
    return 0;
  }

  /* Condensation of right hand side. */

  m2 = *nov + nex;

  for (i = 0; i < *na; ++i) {
    for (ic = *nov; ic < m2; ++ic) {
      ir1 = ic - *nov + 1;
      irp = ir1 - 1;
      irfirp = ARRAY2D(irf, irp, i);
      icfic = ARRAY2D(icf, ic, i);
      for (ir = ir1; ir < *nra; ++ir) {
	irfir = ARRAY2D(irf, ir, i);
	if (ARRAY3D(a, (icfic - 1), (irfir - 1), i) != 0.) {
	  ARRAY2D(fa, (irfir - 1), i) -= ARRAY3D(a, (icfic - 1), (irfir - 1), i) * ARRAY2D(fa, (irfirp - 1), i);
	}
      }
      for (ir = *nbc; ir < *nrc; ++ir) {
	if (ARRAY3D(c, (icfic - 1), ir, i) != 0.) {
	  fc[ir] -= ARRAY3D(c, (icfic - 1), ir, i) * ARRAY2D(fa, (irfirp - 1), i);
	}
      }
    }
  }

  return 0;
} /* conrhs_ */

int 
copycp(integer *na, integer *nov, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nrc, doublereal *c, doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, doublereal *c2, integer *irf)
{
  integer irf_dim1, a_dim1, a_dim2, b_dim1, b_dim2, c_dim1, c_dim2, a1_dim1, a1_dim2,
    a2_dim1, a2_dim2, bb_dim1, bb_dim2, cc_dim1, cc_dim2, c2_dim1, c2_dim2;

  integer i, irfir, ic, ir, ic1;

/* Copies the condensed sytem generated by CONPAR into workspace: the
   conditions' columns of x(i) to cc and of x(i+1) to c2, by block, as
   AUTO-07p keeps them (W474: reduce adds them where it eliminates x(i)). */

  a2_dim1 = *nov;
  a2_dim2 = *nov;
  a1_dim1 = *nov;
  a1_dim2 = *nov;
  irf_dim1 = *nra;
  a_dim1 = *nca;
  a_dim2 = *nra;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;
  b_dim1 = *ncb;
  b_dim2 = *nra;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;
  c2_dim1 = *nov;
  c2_dim2 = *nrc;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  for (i = 0; i < *na; ++i) {
    for (ir = 0; ir < *nov; ++ir) {
      irfir = ARRAY2D(irf, *nra - *nov + ir, i);
      for (ic = 0; ic < *nov; ++ic) {
	ic1 = *nca - *nov + ic;
	ARRAY3D(a1,  ir, ic, i) = ARRAY3D(a, ic, (irfir - 1), i);
	ARRAY3D(a2, ir, ic, i) = ARRAY3D(a, ic1, (irfir - 1), i);
      }
      for (ic = 0; ic < *ncb; ++ic) {
	ARRAY3D(bb, ir, ic, i) = ARRAY3D(b, ic, (irfir - 1), i);
      }
    }
  }

  for (i = 0; i < *na; ++i) {
    for (ir = 0; ir < *nrc; ++ir) {
      for (ic = 0; ic < *nov; ++ic) {
	ARRAY3D(cc, ic, ir, i) = ARRAY3D(c, ic, ir, i);
	ARRAY3D(c2, ic, ir, i) = ARRAY3D(c, *nra + ic, ir, i);
      }
    }
  }

  return 0;
} /* copycp_ */

int 
cpyrhs(integer *na, integer *nov, integer *nra, doublereal *faa, doublereal *fa, integer *irf)
{
  integer irf_dim1, fa_dim1, faa_dim1;

  integer i, irfir, ir;

/*     **Copy the RHS */
  faa_dim1 = *nov;
  irf_dim1 = *nra;
  fa_dim1 = *nra;

  for (i = 0; i < *na; ++i) {
    for (ir = 0; ir < *nov; ++ir) {
      irfir = ARRAY2D(irf, *nra - *nov + ir, i);
      ARRAY2D(faa, ir, i) = ARRAY2D(fa, (irfir - 1), i);
    }
  }

  return 0;
} /* cpyrhs_ */

int 
dimrge(AutoLib &lib, FILE *fp9, doublereal *e, doublereal *cc, doublereal *c2, doublereal *d, doublereal *fc, integer *ifst, integer *na, integer *nrc, integer *nov, integer *ncb, integer *idb, integer *nllv, doublereal *fcc, doublereal *p0, doublereal *p1, doublereal *det, doublereal *a1, doublereal *a2, doublereal *faa, doublereal *bb)
{

  integer e_dim1, cc_dim1, cc_dim2, c2_dim1, c2_dim2, d_dim1, 
    p0_dim1, p1_dim1, a1_dim1, a1_dim2, 
    faa_dim1, a2_dim1, a2_dim2, bb_dim1, 
    bb_dim2;

  integer i, j, k;

  integer novpi, novpj, k1, k2;

  integer novpj2, kc, kr, ncrloc;

  auto &xe = lib.solvbv.xe;

  faa_dim1 = *nov;
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  a1_dim1 = *nov;
  a1_dim2 = *nov;
  p1_dim1 = *nov;
  p0_dim1 = *nov;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;
  c2_dim1 = *nov;
  c2_dim2 = *nrc;
  e_dim1 = *nov + *nrc;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;
  d_dim1 = *ncb;
    
  ncrloc = *nrc + *nov;

  /* Copy the reduced rows: x(0)'s coupling is a1's first slot, x(na)'s a2's last (reduce) */
  for (i = 0; i < *nov; ++i) {
    for (j = 0; j < *nov; ++j) {
      novpj = *nov + j;
      ARRAY2D(e, i, j) = ARRAY3D(a1, i, j, 0);
      ARRAY2D(p0, i, j) = ARRAY3D(a1, i, j, 0);
      ARRAY2D(e, i, novpj) = ARRAY3D(a2, i, j, (*na - 1));
      ARRAY2D(p1, i, j) = ARRAY3D(a2, i, j, (*na - 1));
    }
    for (j = 0; j < *ncb; ++j) {
      novpj2 = (*nov * 2) + j;
      ARRAY2D(e, i, novpj2) = ARRAY3D(bb, i, j, (*na - 1));
    }
  }

  for (i = 0; i < *nrc; ++i) {
    novpi = *nov + i;
    for (j = 0; j < *nov; ++j) {
      novpj = *nov + j;
      ARRAY2D(e, novpi, j) = ARRAY3D(cc, j, i, 0);
      ARRAY2D(e, novpi, novpj) = ARRAY3D(c2, j, i, (*na - 1));
    }
    for (j = 0; j < *ncb; ++j) {
      novpj2 = (*nov * 2) + j;
      ARRAY2D(e, novpi, novpj2) = ARRAY2D(d, j, i);
    }
  }

  for (i = 0; i < *nov; ++i) {
    xe[i] = ARRAY2D(faa, i, (*na - 1));
  }

  for (i = 0; i < *nrc; ++i) {
    novpi = *nov + i;
    xe[novpi] = fc[i];
  }

  if (*idb >= 3) {
    xpp::print(fp9," Residuals of reduced system:\n");	
	  
    xpp::print(fp9," ");
    for (i = 0; i < ncrloc; ++i) {
      xpp::print(fp9,"{:11.3E}",xe[i]);	
      if((i+ 1)%10==0)
	xpp::print(fp9,"\n ");
	    
    }
    xpp::print(fp9,"\n");	
  }

  if (*idb >= 4) {

    xpp::print(fp9," Reduced Jacobian matrix:\n");	
	      
    for (i = 0; i < ncrloc; ++i) {
      int total_printed = 0;
      for (j = 0; j < ncrloc; ++j) {
	if((total_printed != 0)&&(total_printed % 10 == 0))
	  xpp::print(fp9,"\n");	
	xpp::print(fp9," {:11.3E}",ARRAY2D(e, i, j));	
	total_printed++;
      }
      xpp::print(fp9,"\n");	
    }
  }

  /* Solve for FCC */
  if (*nllv == 0) {
    xpp::ge(fp9, *idb, ncrloc, ncrloc, e, 1, ncrloc, fcc,
       ncrloc, xe.data(), det);
  } else if (*nllv > 0) {
    xpp::nlvc(fp9, ncrloc, ncrloc, *nllv, e, fcc);
  } else {
    for (i = 0; i < ncrloc - 1; ++i) {
      xe[i] = 0.;
    }
    xe[-1 + ncrloc] = 1.;
    xpp::ge(fp9, *idb, ncrloc, ncrloc, e, 1, ncrloc, fcc,
       ncrloc, xe.data(), det);
  }
  if (*idb >= 4) {
    xpp::print(fp9," Solution vector:\n");	
	  
    for (i = 0; i < ncrloc; ++i) {
      if((i!=0)&&(i%auto_solution_columns==0))
	xpp::print(fp9,"\n");	
      xpp::print(fp9," {:11.3E}",fcc[i]);	
    }
    xpp::print(fp9,"\n");	
  }

  k1 = ncrloc;
  /* Computing 2nd power */
  k2 = k1 + (*nov) * (*nov);
  for (kr = 0; kr < *nov; ++kr) {
    for (kc = 0; kc < *nov; ++kc) {
      k = kr * *nov + kc;
      fcc[k1 + k] = ARRAY2D(p0, kr, kc);
      fcc[k2 + k] = ARRAY2D(p1, kr, kc);
    }
  }
  /* Computing 2nd power */
  fcc[ncrloc + ((*nov) * (*nov) * 2)] = *det;

  for (i = 0; i < *nrc; ++i) {
    fc[i] = fcc[*nov + i];
  }

  return 0;
} /* dimrge_ */

int 
print1(FILE *fp9, integer *nov, integer *na, integer *nra, integer *nca, integer *ncb, integer *nrc, doublereal *a, doublereal *b, doublereal *c, doublereal *d, doublereal *fa, doublereal *fc)
{

  integer a_dim1, a_dim2, b_dim1, b_dim2, c_dim1, 
    c_dim2, d_dim1, fa_dim1;

  integer i, ic, ir;

  fa_dim1 = *nra;
  a_dim1 = *nca;
  a_dim2 = *nra;
  d_dim1 = *ncb;
  b_dim1 = *ncb;
  b_dim2 = *nra;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  xpp::print(fp9,"AA , BB , FA (Full dimension) :\n");	
  /* should be 10.3f*/
  for (i = 0; i < *na; ++i) {
    xpp::print(fp9,"I={:3}\n",i + 1);
    for (ir = 0; ir < *nra; ++ir) {
      int total_written = 0;
      for (ic = 0; ic < *nca; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(a, ic, ir, i));
	total_written++;
      }
      for (ic = 0; ic < *ncb; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(b, ic, ir, i));	
	total_written++;
      }
      if((total_written != 0) && (total_written%12 == 0))
	xpp::print(fp9,"\n");
      xpp::print(fp9," {:10.3E}",ARRAY2D(fa, ir, i));	
      xpp::print(fp9,"\n");	
    }
  }

  xpp::print(fp9,"CC (Full dimension) :\n");	

  for (i = 0; i < *na; ++i) {
    xpp::print(fp9,"I={:3}\n",i + 1);	
    for (ir = 0; ir < *nrc; ++ir) {
      int total_written = 0;
      for (ic = 0; ic < *nca; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(c, ic, ir, i));	
	total_written++;
      }
      xpp::print(fp9,"\n");	
    }
  }

  xpp::print(fp9,"DD , FC\n");	

  for (ir = 0; ir < *nrc; ++ir) {
    int total_written = 0;
    for (ic = 0; ic < *ncb; ++ic) {
      if((total_written != 0) && (total_written%12 == 0))
	xpp::print(fp9,"\n");
      xpp::print(fp9," {:10.3E}",ARRAY2D(d, ic, ir));	
      total_written++;
    }
    xpp::print(fp9," {:10.3E}\n",fc[ir]);	
  }

  return 0;
} /* print1_ */
