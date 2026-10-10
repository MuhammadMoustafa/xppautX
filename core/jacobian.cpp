/* The model's Jacobian by forward differences: one owner (W255) of what
   gear.cpp, odesol2.cpp, stiff.cpp, del_stab.cpp and AUTO's func each
   wrote for themselves. */
#include "jacobian.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "session.h"
#include "solver.h"

namespace xpp {

void jacobian(Session &s, double t, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out, double *dfdt)
{
  const bool banded = form.layout == JacobianLayout::Banded;
  const int mt = form.lower + form.upper + 1;
  std::vector<double> xp(x, x + n), f1(n);
  if (banded) std::fill_n(out, static_cast<size_t>(n) * mt, 0.0);
  if (dfdt) {
    const double r = eps * std::max(eps, std::fabs(t));
    s.integrator.rhs(t + r, xp.data(), f1.data(), n);
    for (int i = 0; i < n; i++) dfdt[i] = (f1[i] - f0[i]) / r;
  }
  for (int i = 0; i < n; i++) {
    const double r = eps * std::max(eps, std::fabs(x[i]));
    xp[i] = x[i] + r;
    s.integrator.rhs(t, xp.data(), f1.data(), n);
    if (form.discrete_map)
      for (int j = 0; j < n; j++) f1[j] -= xp[j];
    xp[i] = x[i];
    auto entry = [&](int j) { return (f1[j] - f0[j]) / r; };
    switch (form.layout) {
    case JacobianLayout::RowMajor:
      for (int j = 0; j < n; j++) out[static_cast<size_t>(j) * n + i] = entry(j);
      break;
    case JacobianLayout::ColumnMajor:
      for (int j = 0; j < n; j++) out[j + static_cast<size_t>(n) * i] = entry(j);
      break;
    case JacobianLayout::Banded:
      for (int d = -form.lower; d <= form.upper; d++) {
        const int k = i - d;
        if (k < 0 || k > n - 1) continue;
        out[static_cast<size_t>(k) * mt + d + form.lower] = entry(k);
      }
      break;
    }
  }
}

} // namespace xpp
