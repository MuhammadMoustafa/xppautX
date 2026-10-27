/* The exact Jacobian (W260, jacobian.h, expr_derivative.h): each built-in
   operation's derivative, in each argument, against a central difference
   of the operation as the model evaluates it; the constants' and the
   time's columns; an operation with no derivative leaves the model
   without one; and the exact Jacobian of whole models (an equilibrium
   model, a stiff one, a map) against xpp::jacobian's forward differences,
   within their error. */
#include "load_args.h"
#include "xpptest.h"
#include "expr.h"
#include "expr_derivative.h"
#include "expr_native.h"
#include "expr_program.h"
#include "jacobian.h"
#include "model.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_globals.h"
#include "derived.h"
#include "auto_c.h"
#include "auto_unknowns.h"
#include "auto_nox.h"
#include "xpp_math.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<int> number(double x)
{
    const auto halves = xpp::expr::number_halves(x);
    return {NUMSYM, halves[1], halves[0]};
}

/* two numbers agree to the central difference's error */
bool close(double exact, double difference)
{
    return std::fabs(exact - difference) <= 1e-6 * std::max(1.0, std::fabs(exact));
}

/* A model of two equations, x' = the program under test and y' = 0, its
   variables x and y and one constant, compiled whole for each program. */
struct Fixture {
    std::unique_ptr<xpp::Model> model = std::make_unique<xpp::Model>();
    std::unique_ptr<xpp::Session> session = std::make_unique<xpp::Session>(*model);
    int par = 0; /* the constant's index */

    Fixture()
    {
        xpp::Session &s = *session;
        xpp::init_rpn(s);
        CHECK(xpp::add_var(s, "t", 0.0) == 0);
        CHECK(xpp::add_var(s, "x", 0.0) == 0);
        CHECK(xpp::add_var(s, "y", 0.0) == 0);
        par = s.parser.ncon;
        CHECK(xpp::add_con(s, "par", 0.6) == 0);
        model->node = 2;
        s.size_variables(2);
        model->uvar_names[0] = "x";
        model->uvar_names[1] = "y";
        std::vector<int> zero = number(0);
        zero.push_back(ENDEXP);
        model->programs[1].rpn = zero;
    }

    /* x' = rpn (ENDEXP added), compiled with its exact Jacobian */
    bool compile(std::vector<int> rpn)
    {
        rpn.push_back(ENDEXP);
        model->programs[0].rpn = std::move(rpn);
        CHECK(xpp::compile_model(*session).has_value());
        return xpp::ensure_exact_jacobian(*session);
    }

    /* x' at (t, x, y) */
    double f(double t, double x, double y)
    {
        xpp::Session &s = *session;
        s.parser.variables[0] = t;
        s.parser.variables[1] = x;
        s.parser.variables[2] = y;
        return xpp::evaluate(s, model->programs[0]);
    }

    /* d x'/dx and d x'/dy, exactly and by central differences, agree */
    bool agrees(double x, double y, const char *what)
    {
        xpp::Session &s = *session;
        const double point[2] = {x, y};
        double exact[4];
        const double fxy[2] = {f(0, x, y), 0};
        xpp::exact_jacobian(s, s.jacobian_scratch, s.parser.constants.data(), 0.0, point, fxy, 2, {xpp::JacobianLayout::RowMajor}, exact);
        const double hx = 1e-6 * std::max(1.0, std::fabs(x)), hy = 1e-6 * std::max(1.0, std::fabs(y));
        const double dx = (f(0, x + hx, y) - f(0, x - hx, y)) / (2 * hx), dy = (f(0, x, y + hy) - f(0, x, y - hy)) / (2 * hy);
        const bool ok = close(exact[0], dx) && close(exact[1], dy) && exact[2] == 0 && exact[3] == 0;
        if (!ok) std::printf("%s: exact %.12g %.12g, differences %.12g %.12g\n", what, exact[0], exact[1], dx, dy);
        return ok;
    }
};

void check_operations()
{
    Fixture fx;
    const int x = COM(VARTYPE, 1), y = COM(VARTYPE, 2);
    for (std::size_t i = 0; i < xpp::expr::FUN1_COUNT; ++i) {
        if (!xpp::expr::fun1[i]) continue;
        const bool available = fx.compile({x, COM(FUN1TYPE, static_cast<int>(i))});
        CHECK(available == (xpp::expr::fun1_derivative[i] != nullptr));
        if (available) CHECK(fx.agrees(0.3, 0.7, ("fun1 " + std::to_string(i)).c_str()));
    }
    /* the Bessel functions are differentiated in x, their whole order a number */
    const std::vector<int> bessel{18, 19, 20, 21};
    for (std::size_t i = 0; i < xpp::expr::FUN2_COUNT; ++i) {
        if (i > xpp::expr::DIVIDE && !xpp::expr::fun2[i]) continue;
        const int op = COM(FUN2TYPE, static_cast<int>(i));
        const std::string what = "fun2 " + std::to_string(i);
        if (std::ranges::find(bessel, static_cast<int>(i)) != bessel.end()) {
            for (double order : {0.0, 1.0, 2.0, -1.0}) {
                std::vector<int> p = number(order);
                p.insert(p.end(), {y, op});
                CHECK(fx.compile(p));
                CHECK(fx.agrees(0.3, 1.7, what.c_str()));
                if (i != 19) CHECK(fx.agrees(0.3, -1.7, what.c_str())); /* bessely is real for x > 0 only */
            }
            CHECK(!fx.compile({x, y, op})); /* no derivative in the order */
            continue;
        }
        CHECK(fx.compile({x, y, op}));
        /* mod's quotient is not 0 here, and pow's base is above 0 */
        CHECK(fx.agrees(2.3, 0.7, what.c_str()));
        CHECK(fx.agrees(0.3, 0.7, what.c_str()));
    }
    /* x^n for a literal whole n (W471): its derivative n x^(n-1) multiplied out as
       the interpreter multiplies, x^1 being x */
    for (int n = 2; n <= xpp::math::MAX_INTEGER_POWER; ++n) {
        std::vector<int> p{x};
        for (int k : number(n)) p.push_back(k);
        p.push_back(COM(FUN2TYPE, xpp::expr::INTEGER_POWER));
        CHECK(fx.compile(p));
        CHECK(fx.agrees(1.3, 0.7, ("x^" + std::to_string(n)).c_str()));
        xpp::Session &s = *fx.session;
        const double point[2] = {-0.83, 0.7}, fxy[2] = {fx.f(0, point[0], point[1]), 0};
        double exact[4];
        xpp::exact_jacobian(s, s.jacobian_scratch, s.parser.constants.data(), 0.0, point, fxy, 2, {xpp::JacobianLayout::RowMajor}, exact);
        CHECK(exact[0] == n * (n == 2 ? point[0] : xpp::math::integer_power(point[0], n - 1)));
    }
    /* if, a constant, the time */
    /* if(x>0.5)then(x*x)else(-x): MYIF skips the then part and MYTHEN, MYTHEN the else part */
    std::vector<int> branch{x};
    for (int k : number(0.5)) branch.push_back(k);
    branch.insert(branch.end(), {COM(FUN2TYPE, 11), MYIF, 5, x, x, COM(FUN2TYPE, xpp::expr::MULTIPLY), MYTHEN, 3, MYELSE, x, COM(FUN1TYPE, 14)});
    CHECK(fx.compile(branch));
    CHECK(fx.agrees(0.9, 0.0, "if, then"));
    CHECK(fx.agrees(0.2, 0.0, "if, else"));
    CHECK(fx.compile({COM(CONTYPE, fx.par), x, COM(FUN2TYPE, xpp::expr::MULTIPLY), COM(VARTYPE, 0), COM(FUN1TYPE, 0), COM(FUN2TYPE, xpp::expr::ADD)}));
    xpp::Session &s = *fx.session;
    const double point[2] = {0.25, 0.0};
    double column[2] = {-1, -1};
    xpp::exact_constant_derivative(s, s.jacobian_scratch, s.parser.constants.data(), fx.par, 0.4, point, point, 2, column);
    CHECK(column[0] == 0.25 && column[1] == 0);
    xpp::exact_time_derivative(s, s.jacobian_scratch, s.parser.constants.data(), 0.4, point, point, 2, column);
    CHECK(close(column[0], std::cos(0.4)) && column[1] == 0);
    xpp::exact_constant_derivative(s, s.jacobian_scratch, s.parser.constants.data(), fx.par + 1, 0.4, point, point, 2, column);
    CHECK(column[0] == 0 && column[1] == 0);
    /* sqrt(x) at x=0: an exact derivative not finite where f is stops the
       computation, the equation's line named, no difference instead */
    CHECK(fx.compile({x, COM(FUN1TYPE, 13)}));
    {
        const double at[2] = {0, 0}, f0[2] = {fx.f(0, 0, 0), 0};
        double out[4];
        bool stopped = false;
        try {
            xpp::exact_jacobian(s, s.jacobian_scratch, s.parser.constants.data(), 0.0, at, f0, 2, {xpp::JacobianLayout::RowMajor}, out);
        } catch (const xpp::JacobianFailed &failed) {
            stopped = failed.error.where == xpp::JACOBIAN_FAILED
                && failed.error.what == "the exact Jacobian's dx'/dx is inf at t=0 (x=0, y=0), where x' is 0: switch the Numerics' Jacobian to difference";
            if (!stopped) std::printf("%s\n", failed.error.text().c_str());
        }
        CHECK(stopped);
    }
    /* random numbers have no derivative; the model then says why */
    CHECK(!fx.compile({x, RANDUNI}));
    CHECK(fx.model->jacobian.unavailable.find("uses random numbers") != std::string::npos);
    /* --no-compile: none */
    program.compile = false;
    CHECK(!fx.compile({x}));
    CHECK(fx.model->jacobian.unavailable == "the model is interpreted (--no-compile)");
    program.compile = true;
}

/* the model file's exact Jacobian at its initial point against central
   differences, and (forward) against forward differences of step eps in the
   form the integrators use */
void check_model(const char *path, bool map, bool forward = true)
{
    std::string file = path;
    char name[] = "test_exact_jacobian";
    char *argv[] = {name, file.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    const xpp::Model &m = s.model();
    CHECK(xpp::ensure_exact_jacobian(s));
    if (!m.jacobian.available()) return;
    const int n = m.node;
    std::vector<double> x(s.last_ic.begin(), s.last_ic.begin() + n), f(static_cast<std::size_t>(n));
    /* away from a symmetric start, and from 0, where a forward difference steps by eps^2 */
    for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] += 0.13 * (i + 1);
    const double t = 0.3;
    s.integrator.rhs(t, x.data(), f.data(), n);
    if (map)
        for (int i = 0; i < n; ++i) f[static_cast<std::size_t>(i)] -= x[static_cast<std::size_t>(i)];
    const xpp::JacobianForm form{xpp::JacobianLayout::RowMajor, 0, 0, map};
    std::vector<double> differences(static_cast<std::size_t>(n) * n), exact(differences.size()), dt(static_cast<std::size_t>(n)),
        dt_exact(dt.size());
    /* (a map's time derivative is no Jacobian's: none asked) */
    s.numerics.jacobian = xpp::JACOBIAN_DIFFERENCE;
    xpp::jacobian(s, t, x.data(), f.data(), n, 1e-8, form, differences.data(), map ? nullptr : dt.data());
    s.numerics.jacobian = xpp::JACOBIAN_EXACT;
    xpp::jacobian(s, t, x.data(), f.data(), n, 1e-8, form, exact.data(), map ? nullptr : dt_exact.data());
    double worst = 0;
    for (std::size_t k = 0; k < exact.size(); ++k) worst = std::max(worst, std::fabs(exact[k] - differences[k]) / std::max(1.0, std::fabs(exact[k])));
    /* every column again against a central difference of step 1e-5*max(1,|x|)
       (good to about 1e-9): a forward difference is only as good as its step,
       and on motoneuron6c's and mu26's gates and concentrations near zero the
       one above is off by more than 1e-5 (W455) */
    double central = 0;
    {
        std::vector<double> xp = x, fp(f.size()), fm(f.size());
        for (int j = 0; j < n; ++j) {
            const auto sj = static_cast<std::size_t>(j);
            const double h = 1e-5 * std::max(1.0, std::fabs(x[sj]));
            xp[sj] = x[sj] + h;
            s.integrator.rhs(t, xp.data(), fp.data(), n);
            xp[sj] = x[sj] - h;
            s.integrator.rhs(t, xp.data(), fm.data(), n);
            xp[sj] = x[sj];
            for (int i = 0; i < n; ++i) {
                const auto si = static_cast<std::size_t>(i);
                const double d = (fp[si] - fm[si] - (map ? 2 * h * (j == i) : 0)) / (2 * h);
                central = std::max(central, std::fabs(exact[si * static_cast<std::size_t>(n) + sj] - d) / std::max(1.0, std::fabs(d)));
            }
        }
    }
    std::printf("%s: exact against central differences, largest relative difference %.3g\n", path, central);
    CHECK(central < 1e-6);
    for (std::size_t k = 0; k < dt.size(); ++k) worst = std::max(worst, std::fabs(dt_exact[k] - dt[k]) / std::max(1.0, std::fabs(dt_exact[k])));
    /* each parameter's column against a central difference, the derived
       parameters worked out again from it as AUTO does */
    std::vector<double> fp(f.size()), fm(f.size()), column(f.size());
    for (int p = 0; p < m.nupar; ++p) {
        const int k = xpp::get_param_index(s, m.upar_names[static_cast<std::size_t>(p)]);
        const double c = s.parser.constants[static_cast<std::size_t>(k)], h = 1e-6 * std::max(1.0, std::fabs(c));
        s.parser.constants[static_cast<std::size_t>(k)] = c + h;
        xpp::evaluate_derived(s);
        s.integrator.rhs(t, x.data(), fp.data(), n);
        s.parser.constants[static_cast<std::size_t>(k)] = c - h;
        xpp::evaluate_derived(s);
        s.integrator.rhs(t, x.data(), fm.data(), n);
        s.parser.constants[static_cast<std::size_t>(k)] = c;
        xpp::evaluate_derived(s);
        xpp::exact_constant_derivative(s, s.jacobian_scratch, s.parser.constants.data(), k, t, x.data(), f.data(), n, column.data());
        for (int i = 0; i < n; ++i) {
            const double d = (fp[static_cast<std::size_t>(i)] - fm[static_cast<std::size_t>(i)]) / (2 * h);
            worst = std::max(worst, std::fabs(column[static_cast<std::size_t>(i)] - d) / std::max(1.0, std::fabs(d)));
        }
    }
    std::printf("%s: exact against differences, largest relative difference %.3g\n", path, worst);
    /* a forward difference of step 1e-8 is good to about 1e-7 on a model of
       moderate scale; the two models of W455 (gates and concentrations near zero)
       have only the central difference above to check against */
    if (forward) CHECK(worst < 1e-5);
}

/* AUTO's func with ijac 2 (JAC=1): dfdu and the continuation parameters'
   dfdp columns, exactly, against central differences of func itself; a
   parameter of AUTO's own (the period, 10) has a column of 0; a map
   applied store_every times, through the chain rule */
void check_auto_func(const char *path, double u0, double u1, int store_every = 1)
{
    std::string file = path;
    char name[] = "test_exact_jacobian";
    char *argv[] = {name, file.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    xpp::init_auto_win(s);
    CHECK(xpp::ensure_exact_jacobian(s));
    s.numerics.jacobian = xpp::JACOBIAN_EXACT;
    s.numerics.store_every = store_every;
    constexpr integer n = 2;
    const integer continued=s.auto_state.parameter_layout.solver_index(0);
    std::vector<double> par(s.auto_lib.parameter_count, 0.0);
    for (int k = 0; k < s.model().nupar; ++k) par[s.auto_state.parameter_layout.solver_index(k)] = s.parser.constants[static_cast<std::size_t>(s.model().upar_con[k])];
    par[xpp::auto_unknown::period] = 6.3;
    const integer icp[2] = {continued, xpp::auto_unknown::period};
    const double u[n] = {u0, u1};
    double f[n], dfdu[n * n];
    std::vector<double> dfdp(n * s.auto_lib.parameter_count);
    std::fill_n(dfdp.data(), n * s.auto_lib.parameter_count, -1.0);
    func(s, n, u, icp, 2, par.data(), 2, f, dfdu, dfdp.data());
    double worst = 0, fp[n], fm[n];
    std::vector<double> unused(n * s.auto_lib.parameter_count);
    for (integer j = 0; j < n; ++j) {
        double up[n] = {u[0], u[1]}, um[n] = {u[0], u[1]};
        up[j] += 1e-6;
        um[j] -= 1e-6;
        func(s, n, up, icp, 2, par.data(), 0, fp, unused.data(), unused.data());
        func(s, n, um, icp, 2, par.data(), 0, fm, unused.data(), unused.data());
        for (integer i = 0; i < n; ++i) {
            const double d = (fp[i] - fm[i]) / 2e-6;
            worst = std::max(worst, std::fabs(dfdu[i + j * n] - d) / std::max(1.0, std::fabs(d)));
        }
    }
    par[continued] += 1e-6;
    func(s, n, u, icp, 2, par.data(), 0, fp, unused.data(), unused.data());
    par[continued] -= 2e-6;
    func(s, n, u, icp, 2, par.data(), 0, fm, unused.data(), unused.data());
    for (integer i = 0; i < n; ++i) {
        const double d = (fp[i] - fm[i]) / 2e-6;
        worst = std::max(worst, std::fabs(dfdp[continued*n+i] - d) / std::max(1.0, std::fabs(d)));
    }
    std::printf("%s: AUTO's func, exact against central differences, largest difference %.3g\n", path, worst);
    CHECK(worst < 1e-7);
    CHECK(dfdp[xpp::auto_unknown::period * n] == 0 && dfdp[xpp::auto_unknown::period * n + 1] == 0);
}

} // namespace

int main()
{
    check_operations();
    check_model("examples/ode/lecar.odex", false);   /* an equilibrium model: user functions, a fixed variable */
    check_model("examples/ode/fieldnoy.odex", false); /* stiff: the Oregonator, CVODE's */
    check_model("examples/ode/cobweb2.odex", true);   /* a map, its time in mod and an if */
    check_model("tools/models/compile_derived.odex", false); /* a derived parameter */
    check_model("tools/models/exact_jacobian.odex", false);  /* derived, a user function, a fixed variable */
    check_model("examples/ode/mu26.odex", false, false);          /* W455: 15 states, gates near zero; the T1 model */
    check_model("examples/ode/motoneuron6c.odex", false, false);  /* W455: 25 states, concentrations near zero; the P2/E2 model */
    check_auto_func("tools/models/exact_jacobian.odex", 0.3, -0.2);
    check_auto_func("examples/ode/lecar.odex", -0.144, 0.03);
    check_auto_func("examples/ode/cobweb2.odex", 0.4, 0.3, 3); /* a map, three steps a point */
    TEST_REPORT("exact jacobian");
}
