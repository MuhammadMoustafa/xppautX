/* The expression engine's built-in functions (expr.h): the one- and
   two-argument functions a program calls by index (FUN1TYPE, FUN2TYPE:
   the tables below, whose order the symbol table's built-ins name), and
   what DELAY, DEL_SHFT, SHIFT and ISHIFT compute (expr_program.h). */
#include "expr_internal.h"
#include "expr_derivative.h"
#include "delay_handle.h"
#include "xpp_log.h"
#include "xpp_math.h"

#include <cmath>
#include <math.h>
#include <numbers>

namespace {

/* mod(x,y): always in [0,y) */
double pmod(double x, double y)
{
  double z=fmod(x,y);
  if(z<0)z+=y;
  return(z);
}

double max(double x, double y)
{
 return(((x>y)?x:y));
}

double min(double x, double y)
{
 return(((x<y)?x:y));
}

double neg(double z)
{
 return(-z);
}

double recip(double z)
{
 return(1.00/z);
}

double heaviside(double z)
{
 float w=1.0;
 if(z<0)w=0.0;
 return(w);
}

double signum(double z)
{
  if(z<0.0)return(-1.0);
  if(z>0.0)return(1.0);
  return(0.0);
}

/* HOM_BCS(x): deprecated, always 0 */
double hom_bcs(double x)
{
  static_cast<void>(x);
  return 0.0;
}

/*  logical stuff  */

double dnot(double x)
{
 return(static_cast<double>(x==0.0));
}
double dand(double x, double y)
{
 return(static_cast<double>(x&&y));
}
double dor(double x, double y)
{
 return(static_cast<double>(x||y));
}
double dge(double x, double y)
{
 return(static_cast<double>(x>=y));
}
double dle(double x, double y)
{
 return(static_cast<double>(x<=y));
}
double deq(double x, double y)
{
 return(static_cast<double>(x==y));
}
double dne(double x, double y)
{
 return(static_cast<double>(x!=y));
}
double dgt(double x, double y)
{
 return(static_cast<double>(x>y));
}
double dlt(double x, double y)
{
 return(static_cast<double>(x<y));
}

/* an .odex model's x/y: IEEE's, a zero divisor unguarded */
double ieee_divide(double x, double y)
{
 return x/y;
}

}

namespace xpp::expr {

/* xpp::math's functions (correctly rounded, the same on every CPU) and the
   C library's exact ones (sqrt, fabs, floor) */
const std::array<Fun1,FUN1_COUNT> fun1={
  xpp::math::sin,     /*  0 */
  xpp::math::cos,
  xpp::math::tan,
  xpp::math::asin,
  xpp::math::acos,
  xpp::math::atan,    /*  5 */
  xpp::math::sinh,
  xpp::math::tanh,
  xpp::math::cosh,
  ::fabs,
  xpp::math::exp,     /* 10 */
  xpp::math::log,
  xpp::math::log10,
  ::sqrt,
  ::neg,
  ::recip,   /* 15 */
  ::heaviside,
  ::signum,
  ::floor,
  nullptr,   /* RANDOM_UNIFORM: RANDUNI, the evaluator's */
  ::dnot,    /* 20 */
  xpp::math::erf,
  xpp::math::erfc,
  ::hom_bcs,
  nullptr,   /* RANDOM_POISSON: RANDPOI, the evaluator's */
  xpp::math::lgamma,  /* 25 */
};

/* 0-3 are + - * /, which the evaluator does itself (and IEEE_DIVIDE) */
const std::array<Fun2,FUN2_COUNT> fun2={
  nullptr, /*  0 */
  nullptr,
  nullptr,
  nullptr,
  xpp::math::atan2,
  xpp::math::pow,     /*  5 */
  ::max,
  ::min,
  ::pmod,
  ::dand,
  ::dor,     /* 10 */
  ::dgt,
  ::dlt,
  ::deq,
  ::dge,
  ::dle,     /* 15 */
  ::dne,
  nullptr,   /* RANDOM_NORMAL: RANDNORM, the evaluator's */
  xpp::bessel_j,
  xpp::bessel_y,
  xpp::bessel_i,  /* 20 */
  xpp::bessel_i_scaled,
  ::ieee_divide,   /* IEEE_DIVIDE */
};

/* The derivatives of the functions above (W260), each beside its
   function's index, for the exact Jacobian (expr_derivative.h): written
   with the functions themselves. A piecewise-constant function (heav,
   sign, flr, not, the comparisons and logic) has derivative 0 where it is
   defined, as a symbolic derivative gives; max and min take the branch
   they return, mod's quotient is a step (so d mod(x,y)/dy is
   (mod(x,y)-x)/y). A Bessel function's order is a whole number (the
   functions truncate it): no derivative in it, and n+1 and n-1 are the
   neighbouring orders. lgamma's derivative (digamma) is not one of them:
   none. */
namespace {
/* the indices of fun1 and fun2 the derivatives name */
constexpr int SIN = 0, COS = 1, SINH = 6, TANH = 7, COSH = 8, EXP = 10, LOG = 11, SQRT = 13, NEG = 14, SIGN = 17;
constexpr int PMOD = 8, GT = 11, LT = 12, GE = 14, LE = 15, BESSEL_J = 18, BESSEL_Y = 19, BESSEL_I = 20, BESSEL_I_SCALED = 21;

Term zero(Symbolic &g) { return g.number(0); }
Term one(Symbolic &g) { return g.number(1); }
/* 1/sqrt(1-x^2): asin's, -acos's */
Term arcsine(Symbolic &g, Term x) { return g.divide(one(g), g.fun1(SQRT, g.subtract(one(g), g.multiply(x, x)))); }
/* the Bessel function `bessel` of the orders either side of n at x: (f(n-1,x) + sign f(n+1,x))/2 */
Term bessel_neighbours(Symbolic &g, int bessel, Term n, Term x, double sign)
{
  const Term below = g.fun2(bessel, g.subtract(n, one(g)), x), above = g.fun2(bessel, g.add(n, one(g)), x);
  return g.multiply(g.number(0.5), g.add(below, g.multiply(g.number(sign), above)));
}
/* a quotient's derivatives, x/y in x and in y, with the quotient's own
   division (DIVIDE guards a 0 divisor, IEEE_DIVIDE does not) */
template <int divide>
constexpr Fun2Derivative quotient()
{
  return {[](Symbolic &g, Term, Term y) { return g.fun2(divide, one(g), y); },
          [](Symbolic &g, Term x, Term y) { return g.fun1(NEG, g.fun2(divide, g.fun2(divide, x, y), y)); }};
}
/* a piecewise-constant function of two values */
constexpr Fun2Derivative steps() { return {[](Symbolic &g, Term, Term) { return zero(g); }, [](Symbolic &g, Term, Term) { return zero(g); }}; }
}

constexpr std::array<Fun1Derivative,FUN1_COUNT> fun1_derivative={
  [](Symbolic &g, Term x) { return g.fun1(COS, x); },                                       /*  0 sin */
  [](Symbolic &g, Term x) { return g.fun1(NEG, g.fun1(SIN, x)); },                          /* cos */
  [](Symbolic &g, Term x) { const Term c = g.fun1(COS, x); return g.divide(one(g), g.multiply(c, c)); }, /* tan */
  arcsine,                                                                                   /* asin */
  [](Symbolic &g, Term x) { return g.fun1(NEG, arcsine(g, x)); },                           /* acos */
  [](Symbolic &g, Term x) { return g.divide(one(g), g.add(one(g), g.multiply(x, x))); },    /*  5 atan */
  [](Symbolic &g, Term x) { return g.fun1(COSH, x); },                                      /* sinh */
  [](Symbolic &g, Term x) { const Term t = g.fun1(TANH, x); return g.subtract(one(g), g.multiply(t, t)); }, /* tanh */
  [](Symbolic &g, Term x) { return g.fun1(SINH, x); },                                      /* cosh */
  [](Symbolic &g, Term x) { return g.fun1(SIGN, x); },                                      /* fabs */
  [](Symbolic &g, Term x) { return g.fun1(EXP, x); },                                       /* 10 exp */
  [](Symbolic &g, Term x) { return g.divide(one(g), x); },                                  /* log */
  [](Symbolic &g, Term x) { return g.divide(one(g), g.multiply(x, g.number(std::numbers::ln10))); }, /* log10 */
  [](Symbolic &g, Term x) { return g.divide(g.number(0.5), g.fun1(SQRT, x)); },             /* sqrt */
  [](Symbolic &g, Term) { return g.number(-1); },                                           /* neg */
  [](Symbolic &g, Term x) { return g.divide(g.number(-1), g.multiply(x, x)); },             /* 15 recip */
  [](Symbolic &g, Term) { return zero(g); },                                                /* heaviside */
  [](Symbolic &g, Term) { return zero(g); },                                                /* signum */
  [](Symbolic &g, Term) { return zero(g); },                                                /* floor */
  nullptr,                                                                                   /* RANDOM_UNIFORM */
  [](Symbolic &g, Term) { return zero(g); },                                                /* 20 not */
  [](Symbolic &g, Term x) { return g.multiply(g.number(2 * std::numbers::inv_sqrtpi), g.fun1(EXP, g.fun1(NEG, g.multiply(x, x)))); }, /* erf */
  [](Symbolic &g, Term x) { return g.multiply(g.number(-2 * std::numbers::inv_sqrtpi), g.fun1(EXP, g.fun1(NEG, g.multiply(x, x)))); }, /* erfc */
  [](Symbolic &g, Term) { return zero(g); },                                                /* hom_bcs */
  nullptr,                                                                                   /* RANDOM_POISSON */
  nullptr,                                                                                   /* 25 lgamma */
};

constexpr std::array<Fun2Derivative,FUN2_COUNT> fun2_derivative={
  Fun2Derivative{[](Symbolic &g, Term, Term) { return one(g); }, [](Symbolic &g, Term, Term) { return one(g); }},          /*  0 + */
  Fun2Derivative{[](Symbolic &g, Term, Term) { return one(g); }, [](Symbolic &g, Term, Term) { return g.number(-1); }},   /* - */
  Fun2Derivative{[](Symbolic &, Term, Term y) { return y; }, [](Symbolic &, Term x, Term) { return x; }},                   /* * */
  quotient<DIVIDE>(),                                                                                                       /* / */
  /* atan2(x,y): the angle of (y,x) */
  Fun2Derivative{[](Symbolic &g, Term x, Term y) { return g.divide(y, g.add(g.multiply(x, x), g.multiply(y, y))); },
                 [](Symbolic &g, Term x, Term y) { return g.divide(g.fun1(NEG, x), g.add(g.multiply(x, x), g.multiply(y, y))); }},
  Fun2Derivative{[](Symbolic &g, Term x, Term y) { return g.multiply(y, g.fun2(POW, x, g.subtract(y, one(g)))); },           /*  5 pow */
                 [](Symbolic &g, Term x, Term y) { return g.multiply(g.fun2(POW, x, y), g.fun1(LOG, x)); }},
  Fun2Derivative{[](Symbolic &g, Term x, Term y) { return g.fun2(GT, x, y); }, [](Symbolic &g, Term x, Term y) { return g.fun2(LE, x, y); }}, /* max */
  Fun2Derivative{[](Symbolic &g, Term x, Term y) { return g.fun2(LT, x, y); }, [](Symbolic &g, Term x, Term y) { return g.fun2(GE, x, y); }}, /* min */
  Fun2Derivative{[](Symbolic &g, Term, Term) { return one(g); },                                                           /* mod */
                 [](Symbolic &g, Term x, Term y) { return g.divide(g.subtract(g.fun2(PMOD, x, y), x), y); }},
  steps(),                                                                                                                  /* and */
  steps(),                                                                                                                  /* 10 or */
  steps(), steps(), steps(), steps(), steps(), steps(),                                                                    /* > < == >= <= != */
  Fun2Derivative{},                                                                                                         /* RANDOM_NORMAL */
  Fun2Derivative{nullptr, [](Symbolic &g, Term n, Term x) { return bessel_neighbours(g, BESSEL_J, n, x, -1); }},           /* besselj */
  Fun2Derivative{nullptr, [](Symbolic &g, Term n, Term x) { return bessel_neighbours(g, BESSEL_Y, n, x, -1); }},           /* bessely */
  Fun2Derivative{nullptr, [](Symbolic &g, Term n, Term x) { return bessel_neighbours(g, BESSEL_I, n, x, 1); }},            /* 20 besseli */
  /* exp(-|x|) I_n(x): its derivative is (I'_n(x) - sign(x) I_n(x)) exp(-|x|) */
  Fun2Derivative{nullptr, [](Symbolic &g, Term n, Term x) {
    return g.subtract(bessel_neighbours(g, BESSEL_I_SCALED, n, x, 1), g.multiply(g.fun1(SIGN, x), g.fun2(BESSEL_I_SCALED, n, x)));
  }},
  quotient<IEEE_DIVIDE>(),                                                                                                  /* IEEE_DIVIDE */
};

/*********************************************
          FANCY DELAY HERE                   *-------------------------<<<
*********************************************/

double do_shift(const xpp::Session &s, double shift, double variable)
{
  int it, in;
  int i=static_cast<int>(variable),ish=static_cast<int>(shift);

  if(i<0) return(0.0);
   it=i/MAXTYPE;
   in = (i % MAXTYPE) + ish;
  switch(it){
  case CONTYPE:
    if(in<0||static_cast<size_t>(in)>=s.parser.constants.size()) return 0.0;
    return s.parser.constants[static_cast<size_t>(in)];
  case VARTYPE:
    if(in<0||static_cast<size_t>(in)>=s.parser.variables.size()) return 0.0;
    return s.parser.variables[static_cast<size_t>(in)];
  default:
    xpp::log(XPP_LOG_WARN, "This can't happen: Invalid symbol index for SHIFT: i = {:d}\n", i);
    return 0.0;
  }
}
double do_ishift(double shift, double variable)
{

 return variable+shift;

}

double do_set(xpp::Session &s, double shift, double variable, double value)
{
  const int index=static_cast<int>(shift)+((static_cast<int>(variable)) % MAXTYPE);
  if(index>=0&&static_cast<size_t>(index)<s.parser.variables.size())s.parser.variables[index]=value;
  return value;
}

double do_random_uniform(xpp::Session &s, double x)
{
  return x*s.random.uniform();
}
double do_random_poisson(xpp::Session &s, double mean)
{
  return s.random.poisson(mean);
}
double do_random_normal(xpp::Session &s, double mean, double sd)
{
  return s.random.normal(mean,sd);
}

double do_delay_shift(xpp::Session &s, double delay, double shift, double variable)
{
 int in;
  int i=static_cast<int>(variable),ish=static_cast<int>(shift);
  if(i<0) return(0.0);
  in=(i % MAXTYPE)+ish;

  if(in<0||static_cast<size_t>(in)>=s.parser.variables.size())
    return 0.0;

  if(s.delay.stab_flag>0){
    if(s.delay.flag&&delay>0.0)
      return(get_delay(s,in-1,delay));
    return(s.parser.variables[in]);
  }

  return(delay_stab_eval(s,delay,in));

}
double do_delay(xpp::Session &s, double delay, double i)
{
  int variable;
    /* ram - this was a little weird, since i is a double... except I think it's secretely an integer */
    variable = (static_cast<int>(i)) % MAXTYPE;

  if(s.delay.stab_flag>0){
    if(s.delay.flag&&delay>0.0) {
      return(get_delay(s,variable-1,delay));
    }
    return(s.parser.variables[variable]);
  }

  return(delay_stab_eval(s,delay,static_cast<int>(variable)));

}

}
