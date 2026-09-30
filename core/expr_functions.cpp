/* The expression engine's built-in functions (expr.h): the one- and
   two-argument functions a program calls by index (FUN1TYPE, FUN2TYPE:
   the tables below, whose order the symbol table's built-ins name), and
   what DELAY, DEL_SHFT, SHIFT and ISHIFT compute (expr_program.h). */
#include "expr_internal.h"
#include "delay_handle.h"
#include "xpp_log.h"
#include "xpp_math.h"

#include <cmath>
#include <math.h>

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

double rndom(double z)
{
  return(z*ndrand48());
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

/* the C library's functions by their global names (xpp::log is not log) */
const std::array<Fun1,26> fun1={
  ::sin,     /*  0 */
  ::cos,
  ::tan,
  ::asin,
  ::acos,
  ::atan,    /*  5 */
  ::sinh,
  ::tanh,
  ::cosh,
  ::fabs,
  ::exp,     /* 10 */
  ::log,
  ::log10,
  ::sqrt,
  ::neg,
  ::recip,   /* 15 */
  ::heaviside,
  ::signum,
  ::floor,
  ::rndom,
  ::dnot,    /* 20 */
  ::erf,
  ::erfc,
  ::hom_bcs,
  ::poidev,
  ::lgamma,  /* 25 */
};

/* 0-3 are + - * /, which the evaluator does itself (and IEEE_DIVIDE) */
const std::array<Fun2,23> fun2={
  nullptr, /*  0 */
  nullptr,
  nullptr,
  nullptr,
  ::atan2,
  ::pow,     /*  5 */
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
  ::normal,
  ::xpp_bessel_j,
  ::xpp_bessel_y,
  ::xpp_bessel_i,  /* 20 */
  ::xpp_bessel_i_scaled,
  ::ieee_divide,   /* IEEE_DIVIDE */
};

/*********************************************
          FANCY DELAY HERE                   *-------------------------<<<
*********************************************/

double do_shift(const xpp::Session &s, double shift, double variable)
{
  const ParserState &p=s.parser;
  int it, in;
  int i=static_cast<int>(variable),ish=static_cast<int>(shift);

  if(i<0) return(0.0);
   it=i/MAXTYPE;
   in = (i % MAXTYPE) + ish;
  switch(it){
  case CONTYPE:
	if(in>p.ncon)
	  return 0.0;
	else
	  return p.constants[in];
	break;
  case VARTYPE:
	if(in>MAXODE)
	  return 0.0;
	else
	  return p.variables[in];
  default:
    xpp_log(XPP_LOG_WARN, "This can't happen: Invalid symbol index for SHIFT: i = %d\n", i);
    return 0.0;
  }
}
double do_ishift(double shift, double variable)
{

 return variable+shift;

}

double do_delay_shift(xpp::Session &s, double delay, double shift, double variable)
{
 int in;
  int i=static_cast<int>(variable),ish=static_cast<int>(shift);
  if(i<0) return(0.0);
  in=(i % MAXTYPE)+ish;

  if(in>MAXODE)
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
