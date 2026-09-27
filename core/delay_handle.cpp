#include "delay_handle.h"
#include "session.h"
#include "load_eqn.h"
#include "xpp_ui.h"
#include "parserslow.h"
#include "integrate.h"

#include <stdlib.h>
/*   This handles the delay stuff    */

#include <stdio.h>
#include <math.h>
#include <vector>
#include "getvar.h"
#include "form_ode.h"
#include "model.h"

namespace {
/* the stored history, NODE values per row, MaxDelay rows (a ring,
   LatestDelay its newest row) */
std::vector<double> DelayWork;
}
static int LatestDelay;
static int MaxDelay;

double delay_stab_eval(double delay, int var)  /* this returns appropriate values for delay jacobian */
{
  xpp::Session &s=xpp::session();
  int i;

  if(s.delay.stab_flag==0) /* search for all delays  */
    {
      for(i=0;i<s.delay.ndelay;i++){
	if(delay==s.delay.list[i])
	  return(GETVAR(var));
      }
      s.delay.list[s.delay.ndelay]=delay;
      s.delay.ndelay++;
      return(GETVAR(var));
    }
 /*  now we must determine the value to return  */
 /*  del_stab_flag =-1 */    
     for(i=0;i<s.delay.ndelay;i++){
       if(delay==s.delay.list[i])
	if(i==s.delay.which)
	  return s.delay.variable_shift[1][var-1];
     }
   return s.delay.variable_shift[0][var-1];
}

int alloc_delay(double big)
{
 int n;

 n=static_cast<int>(big/fabs(xpp::session().numerics.delta_t))+1;

 MaxDelay=n;
 LatestDelay=1;
 xpp::session().delay.flag=0;
 DelayWork.assign(n*(xpp::model().node ),0.0);
 xpp::session().delay.flag=1;
 xpp::session().delay.ndelay=0;
 xpp::session().delay.which=-1;
 xpp::session().delay.stab_flag=1;
 return(1);
}

void free_delay()
{
 if(xpp::session().delay.flag)DelayWork=std::vector<double>();
 xpp::session().delay.flag=0;
}

void stor_delay(double *y)
{
 int i,in;
 int nodes=xpp::model().node;
 if(xpp::session().delay.flag==0)return;
 --LatestDelay;
 if(LatestDelay<0)LatestDelay+=MaxDelay;
 in=LatestDelay*(nodes );
 for(i=0;i<(nodes );i++)DelayWork[i+in]=y[i];

}

void polint(double *xa, double *ya, int n, double x, double *y, double *dy)
{
  int i,m,ns=1;
  double den,dif,dift,h0,hp,w;
  double c[10],d[10];
  dif=fabs(x-xa[0]);
  for(i=1;i<=n;i++){
    if( (dift=fabs(x-xa[i-1]))<dif){
      ns=i;
      dif=dift;
    }
    c[i-1]=ya[i-1];
    d[i-1]=ya[i-1];
  }
  *y=ya[(ns--) -1];
  for(m=1;m<n;m++){
    for(i=1;i<=n-m;i++){
      h0=xa[i-1]-x;
      hp=xa[i+m-1]-x;
      w=c[i]-d[i-1];
      if((den=h0-hp)==0.0)return ;
      den=w/den;
      d[i-1]=hp*den;
      c[i-1]=h0*den;
    }
    *y += (*dy=(2*ns < (n-m) ? c[ns]:d[ns-- -1]));
  }
}

/* this is like get_delay but uses cubic interpolation */
double get_delay(int in, double tau)
{
 double x=tau/fabs(xpp::session().numerics.delta_t);
 double dd=fabs(xpp::session().numerics.delta_t);
 double y,ya[4],xa[4],dy;
 int n1=static_cast<int>(x);
 int n2=n1+1;
 int nodes=xpp::model().node;
 int n0=n1;
 int n3=n2+1;
 int i0,i1,i2,i3;

 if(tau<0.0||tau>xpp::session().numerics.delay){
			 err_msg("Delay negative or too large");
			stop_integration();
			return(0.0);
  			}
 if(tau==0.0) /* check fro zero delay and ignore the rest */
   return DelayWork[in+nodes*(LatestDelay%MaxDelay)];
  xa[1]=n1*dd;
  xa[0]=xa[1]-dd;
  xa[2]=xa[1]+dd;
  xa[3]=xa[2]+dd;
  i1=(n1+LatestDelay)%MaxDelay;
  i2=(n2+LatestDelay)%MaxDelay;
  i0=(n0+LatestDelay)%MaxDelay;
  i3=(n3+LatestDelay)%MaxDelay;
  if(i1<0)i1+=MaxDelay;
  if(i2<0)i2+=MaxDelay;
  if(i3<0)i3+=MaxDelay;
  if(i0<0)i0+=MaxDelay;

  ya[1]=DelayWork[in+(nodes )*i1];
  ya[2]=DelayWork[in+(nodes )*i2];
   ya[0]=DelayWork[in+(nodes )*i0];
  ya[3]=DelayWork[in+(nodes )*i3];
  polint(xa,ya,4,tau,&y,&dy);
  
  return(y);
 }

/*  Handling of the initial data  */
int do_init_delay(double big)
{
 xpp::Session &s=xpp::session();
 double t=s.numerics.t0,old_t,y[MAXODE];
 int i,nt,j;
 int len;

 /* del_form's per-node formula buffers are RAII now (std::vector), so
    every return path below frees them automatically -- no more manual
    xpp_free loops paired to each early-exit. */
 std::vector<std::vector<int>> del_form(xpp::model().node, std::vector<int>(200, 0));
 nt=static_cast<int>(big/fabs(s.numerics.delta_t));
 s.parser.ncon=xpp::model().ncon_start;
 s.parser.nsym=xpp::model().nsym_start;
 for(i=0;i<(xpp::model().node );i++){
	 if(add_expr(s.delay_string[i].c_str(),del_form[i].data(),&len)){
		err_msg("Illegal delay expression");
		 s.parser.ncon=xpp::model().ncon_start;
		s.parser.nsym=xpp::model().nsym_start;
		return(0);
		}
	 }        /*  Okay all formulas are cool... */
  LatestDelay=1;

  get_val("t",&old_t);

  for(i=nt;i>=0;i--){
	t=s.numerics.t0-fabs(s.numerics.delta_t)*i;
	set_val("t",t);
	for(j=0;j<(xpp::model().node );j++)
		y[j]=evaluate(del_form[j].data());
	stor_delay(y);
  }
   s.parser.ncon=xpp::model().ncon_start;
   s.parser.nsym=xpp::model().nsym_start;
  set_val("t",old_t);
   return(1);
 }

