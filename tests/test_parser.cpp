/* The expression parser: every .ode file goes through it, and a wrong answer
   here is a wrong answer everywhere, reported as "the model is different"
   rather than as a parser bug. add_expr() compiles to an int program that
   evaluate() runs, which is what the right-hand sides do at each step. */
#include "xpptest.h"
#include <math.h>
#include <string>
#include <vector>
#include "expr.h"
#include "markov.h"
#include "session.h"
#include "tabular.h"

static double calc(std::string_view expr, int *ok)
{
    std::vector<int> command;
    int length = 0;
    *ok = (xpp::add_expr(xpp::client_session(), expr, command, &length) == 0);
    if (!*ok) return 0.0;
    return xpp::evaluate(xpp::client_session(), command.data());
}

static int close_to(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

int main(void)
{
    int ok;
    xpp::init_rpn(xpp::client_session());

    CHECK(close_to(calc("1+2", &ok), 3.0) && ok);
    CHECK(close_to(calc("2*3+4", &ok), 10.0) && ok);
    CHECK(close_to(calc("2+3*4", &ok), 14.0) && ok);   /* precedence */
    CHECK(close_to(calc("(2+3)*4", &ok), 20.0) && ok);
    /* XPP's ^ groups to the LEFT: 2^3^2 is (2^3)^2 = 64, where most maths
       notation means 2^(3^2) = 512. Upstream has always done this and models
       are written against it, so it is pinned here rather than corrected. */
    CHECK(close_to(calc("2^3^2", &ok), 64.0) && ok);
    CHECK(close_to(calc("-2^2", &ok), -4.0) && ok); /* but unary minus is weaker */
    CHECK(close_to(calc("-3+1", &ok), -2.0) && ok);
    CHECK(close_to(calc("10/4", &ok), 2.5) && ok);

    CHECK(close_to(calc("sin(0)", &ok), 0.0) && ok);
    CHECK(close_to(calc("exp(0)", &ok), 1.0) && ok);
    CHECK(close_to(calc("max(2,7)", &ok), 7.0) && ok);
    CHECK(close_to(calc("heav(-1)", &ok), 0.0) && ok);
    CHECK(close_to(calc("abs(-2.5)", &ok), 2.5) && ok);

    /* comparisons and logic: the if/then and boolean flags models use */
    CHECK(close_to(calc("1<2", &ok), 1.0) && ok);
    CHECK(close_to(calc("2<=1", &ok), 0.0) && ok);
    CHECK(close_to(calc("if(1>0)then(5)else(9)", &ok), 5.0) && ok);
    CHECK(close_to(calc("if(1<0)then(5)else(9)", &ok), 9.0) && ok);
    CHECK(close_to(calc("2*if(1)then(3)else(4)+1", &ok), 7.0) && ok);

    /* sum(low,high)of(...) evaluates its part once per index i', on the
       same stack as what surrounds it */
    CHECK(close_to(calc("sum(1,4)of(i')", &ok), 10.0) && ok);
    CHECK(close_to(calc("1+sum(1,3)of(i'*2)*2", &ok), 25.0) && ok);
    CHECK(close_to(calc("sum(3,1)of(i')", &ok), 0.0) && ok);

    /* mod is always in [0,y); a zero divisor becomes 2.23e-15 */
    CHECK(close_to(calc("mod(-1,3)", &ok), 2.0) && ok);
    CHECK(fabs(calc("1/0", &ok) - 1/2.23e-15) < 1 && ok);
    CHECK(close_to(calc("0/0", &ok), 0.0) && ok);

    /* a name that no model defines must be refused, not silently zero */
    calc("nosuchthing+1", &ok);
    CHECK(!ok);
    calc("2+", &ok);
    CHECK(!ok);

    /* Long formula (> 2000 terms): previously overflowed tokstak[500], loopstk[100], and MAXEXPLEN */
    std::string long_expr = "1";
    for (int i = 0; i < 2000; ++i) long_expr += "+1";
    CHECK(close_to(calc(long_expr, &ok), 2001.0) && ok);

    /* Table formula with 300 terms: previously overflowed tabular.cpp command[200] */
    xpp::add_var(xpp::client_session(), "t", 0.0);
    std::string table_expr = "t";
    for (int i = 0; i < 300; ++i) table_expr += "+0.01";
    std::vector<double> table_y(10);
    auto table_res = xpp::eval_fun_table(xpp::client_session(), 0, 10, 0.0, 1.0, table_expr, table_y.data());
    CHECK(table_res.has_value());
    CHECK(close_to(table_y[0], 3.0));

    /* Markov chain with > 50 states: previously overflowed markov.cpp st[50] */
    xpp::add_markov(xpp::client_session(), 60, "m60");
    const int m_idx = xpp::client_session().model().nmarkov - 1;
    CHECK(xpp::client_session().model().markov[m_idx].nstates == 60);
    CHECK(xpp::client_session().model().markov[m_idx].states.size() == 60);
    CHECK(close_to(xpp::client_session().model().markov[m_idx].states[59], 59.0));

    /* Stiff solver pivots with > 700 variables: previously overflowed indx[700] */
    int *pivots = xpp::client_session().solver_work.size_pivots(750);
    CHECK(pivots != nullptr);

    TEST_REPORT("parser");
}
