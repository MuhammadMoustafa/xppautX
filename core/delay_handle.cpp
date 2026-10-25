#include "delay_handle.h"
#include "session.h"
#include "load_eqn.h"
#include "xpp_ui.h"
#include "xpp_job.h"
#include "expr.h"
#include "integrate.h"

#include <stdlib.h>
/*   This handles the delay stuff    */

#include <stdio.h>
#include <math.h>
#include <array>
#include <vector>
#include "getvar.h"
#include "form_ode.h"
#include "model.h"
#include "model_files.h"
#include "xpp_mem.h"
#include <limits>
#include <cmath>

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
      if(s.delay.list.size()<=static_cast<std::size_t>(s.delay.ndelay))s.delay.list.resize(s.delay.ndelay+1);
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

Result<> alloc_delay(xpp::Session &s, double big)
{
 const double ratio=big/fabs(s.numerics.delta_t);
 const auto failed=[&]{return xpp::fail("delay",
   xpp::format("Cannot allocate delay history for delay={} and dt={}",big,s.numerics.delta_t),
   model_place(s.model(),"delay"));};
 if(!std::isfinite(ratio)||ratio<0 ||
    ratio>=static_cast<double>(std::numeric_limits<std::size_t>::max()))return failed();
 std::size_t n;
 try {
   n=xpp::size_sum(static_cast<std::size_t>(ratio),1);
   std::vector<double> work(xpp::size_product(n,s.model().node),0.0);
   s.delay.work.swap(work);
 } catch(const std::length_error &){return failed();}
   catch(const std::bad_alloc &){return failed();}
 s.delay.rows=n;
 s.delay.latest=1;
 s.delay.flag=1;
 s.delay.ndelay=0;
 s.delay.which=-1;
 s.delay.stab_flag=1;
 return {};
}

void free_delay(xpp::Session &s)
{
 if(s.delay.flag)s.delay.work=std::vector<double>();
 s.delay.flag=0;
}

void stor_delay(xpp::Session &s, double *y)
{
 int i;
 int nodes=s.model().node;
 if(s.delay.flag==0)return;
 if(s.delay.latest==0)s.delay.latest=s.delay.rows;
 --s.delay.latest;
 const std::size_t in=s.delay.latest*static_cast<std::size_t>(nodes);
 for(i=0;i<(nodes );i++)s.delay.work[i+in]=y[i];

}

void polint(std::span<const double, polint_points> xa, std::span<const double, polint_points> ya, double x, double &y, double &dy)
{
  const int n=static_cast<int>(xa.size());
  int i,m,ns=1;
  double den,dif,dift,h0,hp,w;
  std::array<double, polint_points> c{}, d{};
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
 double y,dy;
 std::array<double, polint_points> ya{}, xa{};
 const std::size_t nodes=s.model().node;

 if(tau<0.0||tau>s.numerics.delay){
			stop_integration(s,{"delay","Delay negative or too large"});
			return(0.0);
  			}
 if(tau==0.0) /* check fro zero delay and ignore the rest */
   return s.delay.work[in+nodes*(s.delay.latest%s.delay.rows)];
  const std::size_t n1=static_cast<std::size_t>(x),n2=n1+1,n0=n1,n3=n2+1;
  xa[1]=n1*dd;
  xa[0]=xa[1]-dd;
  xa[2]=xa[1]+dd;
  xa[3]=xa[2]+dd;
  const std::size_t i1=(n1+s.delay.latest)%s.delay.rows;
  const std::size_t i2=(n2+s.delay.latest)%s.delay.rows;
  const std::size_t i0=(n0+s.delay.latest)%s.delay.rows;
  const std::size_t i3=(n3+s.delay.latest)%s.delay.rows;

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
 double t=s.numerics.start_time,old_t;
 std::vector<double> y(s.model().node);
 int i,j;
 int len;

 /* del_form's per-node formula buffers are std::vectors: every return
    path below frees them. */
 std::vector<std::vector<int>> del_form(s.model().node);
 /* Allocation proves the ratio and all history indices representable. */
 if(!s.delay.flag)return xpp::fail("delay","Delay history is not allocated",model_place(s.model(),"delay"));
 const double ratio=big/fabs(s.numerics.delta_t);
 if(!std::isfinite(ratio)||ratio<0||ratio>=static_cast<double>(s.delay.rows))
   return xpp::fail("delay","Delay history does not fit its allocation",model_place(s.model(),"delay"));
 const std::size_t nt=static_cast<std::size_t>(ratio);
 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;
 for(i=0;i<(s.model().node );i++){
	 if(add_expr(s,s.delay_string[i],del_form[i],&len)){
		 s.parser.ncon=s.model().ncon_start;
		s.parser.nsym=s.model().nsym_start;
		return xpp::fail("delay",xpp::format("Illegal delay expression {} for {}",s.delay_string[i],s.model().uvar_names[i]),
		                 command_place());
		}
	 }        /*  Okay all formulas are cool... */
  s.delay.latest=1;

  get_val(s,"t",&old_t);

  for(std::size_t row=nt+1;row-->0;){
	if(xpp::job::cancelled())break;
	t=s.numerics.start_time-fabs(s.numerics.delta_t)*row;
	set_val(s,"t",t);
	for(j=0;j<(s.model().node );j++)
		y[j]=evaluate(s,del_form[j].data());
	stor_delay(s,y.data());
  }
   s.parser.ncon=s.model().ncon_start;
   s.parser.nsym=s.model().nsym_start;
  set_val(s,"t",old_t);
   return {};
 }

} // namespace xpp
