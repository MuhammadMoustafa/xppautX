/* The lookup tables a formula reads (expr_tables.h). */
#include "expr_tables.h"

namespace xpp {

namespace {

/* a table's value at a point and the slope of its interpolation there
   (W260: the exact Jacobian differentiates a lookup by it), each piece's
   value computed as XPPAUT computes it */
struct Interpolated {
  double value, slope;
};

/* the cubic through y[i-1..i+2] (from xlo, step h) at x */
Interpolated tab_interp(double xlo, double h, double x, const double *y, int i)
{
  const double ym=y[i-1],y0=y[i],y1=y[i+1],y2=y[i+2];
  const double d=y0;
  const double b=.5*(y1+ym-2*y0);
  const double a=(3*(y0-y1)+y2-ym)/6;
  const double c=(6*y1-y2-3*y0-2*ym)/6;
  const double tt=(x-xlo)/h-i;
  return {d+tt*(c+tt*(b + tt*a)),(c+tt*(2*b+3*tt*a))/h};
}

Interpolated interpolate(const xpp::TABULAR &t, double x)
{
  const double xlo=t.xlo,xhi=t.xhi,dx=t.dx;
  const double *y=t.y;
  const int n=t.n;
  if(t.flag==0)return {0.0,0.0}; /* Not defined   */
  const int i1=static_cast<int>((x-xlo)/dx);   /* (int)floor(x) instead of (int)x ??? */
  if(t.interp==2&&i1>0&&i1<(n-2))
    return tab_interp(xlo,dx,x,y,i1); /* if it is on the edge - use linear */
  const int i2=i1+1;
  if(i1>-1&&i2<n){
    const double x1=dx*i1+xlo,y1=y[i1],y2=y[i2];
    if (t.interp==0||t.interp==2)
      return {y1+(y2-y1)*(x-x1)/dx,(y2-y1)/dx};
    return {y1,0.0};
  }
  if(i1<0)return {y[0]+(y[1]-y[0])*(x-xlo)/dx,(y[1]-y[0])/dx};
  if(i2>=n)return {y[n-1]+(y[n-1]-y[n-2])*(x-xhi)/dx,(y[n-1]-y[n-2])/dx};
  return {0.0,0.0};
}

}

double lookup(const TABULAR &table, double x)
{
  return interpolate(table,x).value;
}

double lookup_slope(const TABULAR &table, double x)
{
  return interpolate(table,x).slope;
}

} // namespace xpp
