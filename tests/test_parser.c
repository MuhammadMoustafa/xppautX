/* The expression parser: every .ode file goes through it, and a wrong answer
   here is a wrong answer everywhere, reported as "the model is different"
   rather than as a parser bug. add_expr() compiles to an int program that
   evaluate() runs, which is what the right-hand sides do at each step. */
#include "xpptest.h"
#include <math.h>
#include "parserslow.h"

static double calc(char *expr, int *ok)
{
    int command[256], length = 0;
    char buf[256];
    sprintf(buf, "%s", expr); /* add_expr writes into what it is given */
    *ok = (add_expr(buf, command, &length) == 0);
    if (!*ok) return 0.0;
    return evaluate(command);
}

static int close_to(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

int main(void)
{
    int ok;
    init_rpn();

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

    /* a name that no model defines must be refused, not silently zero */
    calc("nosuchthing+1", &ok);
    CHECK(!ok);
    calc("2+", &ok);
    CHECK(!ok);

    TEST_REPORT("parser");
}
