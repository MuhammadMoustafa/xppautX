# Reference-LAPACK (vendored, for the LINALG=lapack comparison build only)

The routines AUTO's eigenvalue code would call if it used LAPACK, unchanged,
from Reference-LAPACK 3.12.1 (the GitHub tag `v3.12.1` archive,
`lapack-3.12.1.tar.gz`, SHA-256
`2ca6407a001a474d4d4d35f3a61550156050c48016d949f0da0529c0aa052422`), modified BSD
licence (`LICENSE`).

- Taken by `tools/lapack/extract.py` (rerunnable; it checks the tarball's hash): `DGEEV`
  (eigenvalues and eigenvectors of a general real matrix, EISPACK's `RG`'s job) and
  `DGGEV` (generalized eigenvalues, EISPACK's `QZHES`/`QZIT`/`QZVAL`'s), with everything
  they call: LAPACK's own routines (`SRC/`), the reference BLAS they use (`BLAS/SRC/`)
  and the machine constants (`INSTALL/dlamch.f`).
- Built by the Makefile only under `LINALG=lapack`, with gfortran, into the build
  folder named by `BUILDDIR`; the default build and every release never contain it.
  Its adapter, which gives `rg`, `qzhes`, `qzit` and `qzval` the old EISPACK signatures, is
  `tools/lapack-adapter/lapack_adapter.cpp`.
- Why: the comparison of W358 (#420): which eigenvalue library is faster and more accurate
  for AUTO's Jacobians and monodromy matrices (docs/studies/w358-auto-profile.md).
