#include "delay_handle.h"
#include "session.h"
#include "load_eqn.h"
#include "xpp_ui.h"
#include "expr.h"
#include "integrate.h"

#include <stdlib.h>
/*   This handles the delay stuff    */

#include <stdio.h>
#include <math.h>
#include <vector>
#include "getvar.h"
#include "form_ode.h"
#include "model.h"

namespace xpp {


double delay_stab_eval(xpp::Session &s, double delay, int var)  /* this returns appropriate values for delay jacobian */
{
  int i;

  if(s.delay.stab_flag==0) /* search for all delays  */
    {
      for(i=0;i<s.delay.ndelay;i++){
	if(delay==s.delay.list[i])
	  return(getvar(s,var));
      }
      s.delay.list[s.delay.ndelay]=delay;
      s.delay.ndelay++;
      return(getvar(s,var));
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

int alloc_delay(xpp::Session &s, double big)
{
 int n;

 n=static_cast<int>(big/fabs(s.numerics.delta_t))+1;

 s.delay.rows=n;
 s.delay.latest=1;
 s.delay.flag=0;
 s.delay.work.assign(n*(s.model().node ),0.0);
 s.delay.flag=1;
 s.delay.ndelay=0;
 s.delay.which=-1;
 s.delay.stab_flag=1;
 return(1);
}

void free_delay(xpp::Session &s)
{
 if(s.delay.flag)s.delay.work=std::vector<double>();
 s.delay.flag=0;
}

void stor_delay(xpp::Session &s, double *y)
{
 int i,in;
 int nodes=s.model().node;
 if(s.delay.flag==0)return;
 --s.delay.latest;
 if(s.delay.latest<0)s.delay.latest+=s.delay.rows;
 in=s.delay.latest*(nodes );
 for(i=0;i<(nodes );i++)s.delay.work[i+in]=y[i];

}

void polint(std::span<const double> xa, std::span<const double> ya, double x, double &y, double &dy)
{
  const int n=static_cast<int>(xa.size());
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
  y=ya[(ns--) -1];
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
    y += (dy=(2*ns < (n-m) ? c[ns]:d[ns-- -1]));
  }
}

/* this is like get_delay but uses cubic interpolation */
double get_delay(xpp::Session &s, int in, double tau)
{
 double x=tau/fabs(s.numerics.delta_t);
 double dd=fabs(s.numerics.delta_t);
 double y,ya[4],xa[4],dy;
 int n1=static_cast<int>(x);
 int n2=n1+1;
 int nodes=s.model().node;
 int n0=n1;
 int n3=n2+1;
 int i0,i1,i2,i3;

 if(tau<0.0||tau>s.numerics.delay){
			stop_integration(s,{"delay","Delay negative or too large"});
			return(0.0);
  			}
 if(tau==0.0) /* check fro zero delay and ignore the rest */
   return s.delay.work[in+nodes*(s.delay.latest%s.delay.rows)];
  xa[1]=n1*dd;
  xa[0]=xa[1]-dd;
  xa[2]=xa[1]+dd;
  xa[3]=xa[2]+dd;
  i1=(n1+s.delay.latest)%s.delay.rows;
  i2=(n2+s.delay.latest)%s.delay.rows;
  i0=(n0+s.delay.latest)%s.delay.rows;
  i3=(n3+s.delay.latest)%s.delay.rows;
  if(i1<0)i1+=s.delay.rows;
  if(i2<0)i2+=s.delay.rows;
  if(i3<0)i3+=s.delay.rows;
  if(i0<0)i0+=s.delay.rows;

  ya[1]=s.delay.work[in+(nodes )*i1];
  ya[2]=s.delay.work[in+(nodes )*i2];
   ya[0]=s.delay.work[in+(nodes )*i0];
  ya[3]=s.delay.work[in+(nodes )*i3];
  polint(xa,ya,tau,y,dy);
  
  return(y);
 }

/*  Handling of the initial data  */
xpp::Result<> do_init_delay(xpp::Session &s, double big)
{
 double t=s.numerics.start_time,old_t,y[MAXODE];
 int i,nt,j;
 int len;

 /* del_form's per-node formula buffers are RAII now (std::vector), so
    every return path below frees them automatically -- no more manual
    xpp_free loops paired to each early-exit. */
 std::vector<std::vector<int>> del_form(s.model().node, std::vector<int>(200, 0));
 nt=static_cast<int>(big/fabs(s.numerics.delta_t));
 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;
 for(i=0;i<(s.model().node );i++){
	 if(add_expr(s,s.delay_string[i],del_form[i].data(),&len)){
		 s.parser.ncon=s.model().ncon_start;
		s.parser.nsym=s.model().nsym_start;
		return xpp::fail("delay",xpp::format("Illegal delay expression {} for {}",s.delay_string[i],s.model().uvar_names[i]),
		                 command_place());
		}
	 }        /*  Okay all formulas are cool... */
  s.delay.latest=1;

  get_val(s,"t",&old_t);

  for(i=nt;i>=0;i--){
	t=s.numerics.start_time-fabs(s.numerics.delta_t)*i;
	set_val(s,"t",t);
	for(j=0;j<(s.model().node );j++)
		y[j]=evaluate(s,del_form[j].data());
	stor_delay(s,y);
  }
   s.parser.ncon=s.model().ncon_start;
   s.parser.nsym=s.model().nsym_start;
  set_val(s,"t",old_t);
   return {};
 }

} // namespace xpp
