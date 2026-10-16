/* The model's Jacobian by forward differences: one owner (W255) of what
   gear.cpp, odesol2.cpp, stiff.cpp, del_stab.cpp and AUTO's func each
   wrote for themselves; or the exact one, from the model's compiled
   derivatives (W260). */
#include "jacobian.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "model.h"
#include "model_files.h"
#include "session.h"
#include "solver.h"
#include "xpp_io.h"
#include "expr.h"
#include "xpp_mem.h"

namespace xpp {

namespace {

/* the values of p's entries at (t, x) with the constants c, each finite
   where the right-hand side f is, else JacobianFailed; wrt(k) names the
   variable entry k differentiates by */
template <class By>
std::span<const double> entries(const Session &s, JacobianScratch &scratch, const JacobianProgram &p, const double *c, double t, const double *x, const double *f, int n, By &&wrt)
{
  const Model &m = s.model();
  auto &v = scratch.point;
  auto &w = scratch.workspace;
  v.resize(xpp::size_sum(m.node, 1));
  w.resize(m.jacobian.workspace);
  if (scratch.values.size() < p.rows.size()) scratch.values.resize(p.rows.size());
  const std::span<double> out(scratch.values.data(), p.rows.size());
  v[0] = t;
  std::copy_n(x, m.node, v.begin() + 1);
  /* the helpers only read the Session (its tables) */
  p.function(c, v.data(), const_cast<Session *>(&s), w.data(), out.data());
  for (size_t k = 0; k < out.size(); k++) {
    const int row = p.rows[k];
    if (std::isfinite(out[k]) || row >= n || !std::isfinite(f[row])) continue;
    std::string state;
    for (int i = 0; i < m.node; i++) state += xpp::format("{}{}={}", i ? ", " : "", m.uvar_names[i], x[i]);
    const std::string &name = m.uvar_names[row];
    throw JacobianFailed{Error{std::string(JACOBIAN_FAILED),
                               xpp::format("the exact Jacobian's d{}'/d{} is {} at t={} ({}), where {}' is {}: switch the Numerics' "
                                           "Jacobian to difference",
                                           name, wrt(k), out[k], t, state, name, f[row]),
                               model_place(m, name)}};
  }
  return out;
}

/* a column's entries into out[0..n), by the variable by() names */
template <class By>
void column(const Session &s, JacobianScratch &scratch, const JacobianProgram &p, const double *c, double t, const double *x, const double *f, int n, By &&by,
            double *out)
{
  const auto e = entries(s, scratch, p, c, t, x, f, n, [&by](size_t) { return by(); });
  std::fill_n(out, n, 0.0);
  for (size_t k = 0; k < e.size(); k++)
    if (p.rows[k] < n) out[p.rows[k]] = e[k];
}

} // namespace

ErrorAction switch_to_differences(std::string resume)
{
  return ErrorAction{"Switch to differences and continue",
                     {xpp::format(R"({{"cmd":"set","kind":"num","name":"{}","text":"{}"}})", JACOBIAN_KEY, jacobian_names[JACOBIAN_DIFFERENCE]),
                      std::move(resume)}};
}

void exact_jacobian(const Session &s, JacobianScratch &scratch, const double *c, double t, const double *x, const double *f, int n, const JacobianForm &form, double *out)
{
  const Model &m = s.model();
  const JacobianProgram &p = m.jacobian.states;
  const auto e = entries(s, scratch, p, c, t, x, f, n, [&](size_t k) -> std::string_view { return m.uvar_names[p.columns[k]]; });
  const int mt = form.lower + form.upper + 1;
  /* where d f_row / d x_col goes in the form, -1 outside a band */
  auto at = [&](int row, int col) -> long {
    switch (form.layout) {
    case JacobianLayout::RowMajor: return static_cast<long>(row) * n + col;
    case JacobianLayout::ColumnMajor: return row + static_cast<long>(n) * col;
    case JacobianLayout::Banded: break;
    }
    const int d = col - row;
    return d < -form.lower || d > form.upper ? -1 : static_cast<long>(row) * mt + d + form.lower;
  };
  std::fill_n(out, jacobian_entries(n, form), 0.0);
  for (size_t k = 0; k < e.size(); k++) {
    if (p.rows[k] >= n || p.columns[k] >= n) continue;
    if (const long i = at(p.rows[k], p.columns[k]); i >= 0) out[i] = e[k];
  }
  if (form.discrete_map)
    for (int i = 0; i < n; i++) out[at(i, i)] -= 1;
}

void exact_constant_derivative(const Session &s, JacobianScratch &scratch, const double *c, int k, double t, const double *x, const double *f, int n, double *out)
{
  const auto &columns = s.model().jacobian.by_constant;
  const auto it = std::ranges::lower_bound(columns, k, {}, [](const auto &e) { return e.first; });
  if (it == columns.end() || it->first != k) std::fill_n(out, n, 0.0);
  else column(s, scratch, it->second, c, t, x, f, n, [&]() -> std::string_view {
    /* the parameter constant k is: AUTO's are the model's parameters */
    const Model &m = s.model();
    for (int i = 0; i < m.nupar; i++)
      if (get_param_index(s, m.upar_names[i]) == k) return m.upar_names[i];
    return "a constant";
  }, out);
}

void exact_time_derivative(const Session &s, JacobianScratch &scratch, const double *c, double t, const double *x, const double *f, int n, double *out)
{
  column(s, scratch, s.model().jacobian.time, c, t, x, f, n, []() -> std::string_view { return "t"; }, out);
}

double difference_step(double eps, double x)
{
  return eps * std::max(eps, std::fabs(x));
}

JacobianForm model_jacobian_form(const Session &s, int n)
{
  if (!s.numerics.cv_bandflag) return {};
  return {JacobianLayout::Banded, std::min(s.numerics.cv_bandlower, n - 1), std::min(s.numerics.cv_bandupper, n - 1)};
}

int jacobian_entries(int n, const JacobianForm &form)
{
  return form.layout == JacobianLayout::Banded ? n * (form.lower + form.upper + 1) : n * n;
}

void jacobian(const JacobianEvaluate &evaluate, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out)
{
  const bool banded = form.layout == JacobianLayout::Banded;
  const int mt = form.lower + form.upper + 1;
  std::vector<double> f1(n);
  if (banded) std::fill_n(out, static_cast<size_t>(n) * mt, 0.0);
  for (int i = 0; i < n; i++) {
    const double r = difference_step(eps, x[i]);
    evaluate(i, x[i] + r, f1.data());
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

void jacobian(Session &s, double t, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out, double *dfdt)
{
  if (s.numerics.jacobian == JACOBIAN_EXACT) {
    exact_jacobian(s, s.jacobian_scratch, s.parser.constants.data(), t, x, f0, n, form, out);
    if (dfdt) exact_time_derivative(s, s.jacobian_scratch, s.parser.constants.data(), t, x, f0, n, dfdt);
    return;
  }
  std::vector<double> xp(x, x + n);
  if (dfdt) {
    std::vector<double> f1(n);
    const double r = difference_step(eps, t);
    s.integrator.rhs(t + r, xp.data(), f1.data(), n);
    for (int i = 0; i < n; i++) dfdt[i] = (f1[i] - f0[i]) / r;
  }
  auto evaluate = [&](int i, double xi, double *f) {
    xp[i] = xi;
    s.integrator.rhs(t, xp.data(), f, n);
    if (form.discrete_map)
      for (int j = 0; j < n; j++) f[j] -= xp[j];
    xp[i] = x[i];
  };
  jacobian(evaluate, x, f0, n, eps, form, out);
}

} // namespace xpp
