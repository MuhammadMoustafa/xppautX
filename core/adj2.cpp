#include "model.h"
#include "session.h"
#include "adj2.h"
#include "storage.h"
#include "odesol2.h"
#include "jacobian.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "my_rhs.h"
#include "browse.h"
#include "do_fit.h"
#include "integrate.h"
#include "expr.h"
#include "getvar.h"
#include "xpp_ui.h"

#include <string>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <vector>
/*
  this has a bunch of numerical routines
  averaging
  adjoints
  transpose
  maximal liapunov exponent
  
*/

#include <stdio.h>
#include <math.h>

#include "histogram.h"
#include "load_eqn.h"

namespace xpp {


namespace {
constexpr double ADJ_EPS=1.e-8;
}

/* extern Window main_win; */

void init_trans(xpp::Session &s)
{
  s.adjoint.transpose.firstcol=s.model().uvar_names[0];
  s.adjoint.transpose.ncol=2;
  s.adjoint.transpose.nrow=1;
  s.adjoint.transpose.rowskip=1;
  s.adjoint.transpose.colskip=1;
  s.adjoint.transpose.row0=1;
  s.adjoint.transpose.col0=2;
}


int do_transpose(xpp::Session &s)
{
 int i,status;
 static const char *const n[]={"*0Column 1","NCols","ColSkip","Row 1","NRows","RowSkip"};
 std::array<std::string, 6> values;
 values[0] = s.adjoint.transpose.firstcol;
 values[1] = xpp::format("{:d}", s.adjoint.transpose.ncol);
 values[2] = xpp::format("{:d}", s.adjoint.transpose.colskip);
 values[3] = xpp::format("{:d}", s.adjoint.transpose.row0);
 values[4] = xpp::format("{:d}", s.adjoint.transpose.nrow);
 values[5] = xpp::format("{:d}", s.adjoint.transpose.rowskip);
 AdjointState &a=s.adjoint;
 if(a.transposed_here){
   a.transposed.release();
   a.transposed_here=false;
   data_back(s);
 }
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,
                           XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
 status=do_string_box_of(6,1,"Transpose Data",n,values,kinds);
 if(status!=0){
   find_variable(s,values[0].c_str(),&i);
   if(i>-1)
     s.adjoint.transpose.col0=i+1;
   else
     {
       command_error("adjoint", "No such columns");
       return 0;
     }
   s.adjoint.transpose.firstcol=values[0];
   i=atoi(values[4].c_str());
   if(i>=s.model().neq)i=s.model().neq-1;
   s.adjoint.transpose.nrow=i;
   s.adjoint.transpose.ncol=atoi(values[1].c_str());
   s.adjoint.transpose.colskip=atoi(values[2].c_str());
   s.adjoint.transpose.row0=atoi(values[3].c_str());
   s.adjoint.transpose.rowskip=atoi(values[5].c_str());
   return (create_transpose(s));
 }
 return 0; 

}
 
int create_transpose(xpp::Session &s)
{
  int i,j;
  int inrow,incol;
  float **data=s.adjoint.transposed.make(s.data_store,s.adjoint.transpose.nrow+1,s.adjoint.transpose.ncol,s.model().neq);
  for(j=0;j<s.adjoint.transpose.ncol;j++)
    data[0][j]=j+1;

  for(i=0;i<s.adjoint.transpose.ncol;i++){
    incol=s.adjoint.transpose.col0-1+i*s.adjoint.transpose.colskip;
    if(incol>s.model().neq)
      incol=s.model().neq;
    for(j=0;j<s.adjoint.transpose.nrow;j++){
      inrow=s.adjoint.transpose.row0+j*s.adjoint.transpose.rowskip;
      if(inrow>s.data_store.rows)
	inrow=s.data_store.rows;
      data[j+1][i]=s.data_store.col[incol][inrow];
    }
  }
  
  new_browse_dat(s,data,s.adjoint.transpose.ncol);
   s.adjoint.transposed_here=true;
   return 1;
}

void alloc_h_stuff(xpp::Session &s)
{
 s.adjoint.coup_fun.assign(s.model().node,{});
 s.adjoint.coup_string.assign(s.model().node,"0");
}

void data_back(xpp::Session &s)
{
 s.histogram.four_here=0;
 new_browse_dat(s,s.data_store.col,s.data_store.rows);
}

void adj_back(xpp::Session &s)
{
 AdjointState &a=s.adjoint;
 if(a.adjoint_here)new_browse_dat(s,a.adjoint.view(s.data_store),a.adjoint_rows);
}

void h_back(xpp::Session &s)
{
 AdjointState &a=s.adjoint;
 if(a.h_here)new_browse_dat(s,a.h_function.view(s.data_store),a.h_rows);
}
/*  Here is how to do the range over adjoints and h functions
    unfortunately, h functions are always computed even if you dont want them 
    they will just be zeros

    Step 1. Compute a singel orbit, adjoint, and H function (to load the 
    program with the correct right-hand sides for H function. Or just load in
    set file where it was done
    Step 2.  Set transient to some reasonable number to assure convergence 
    onto the limit cycle as you change parameters and total to be at least
    2 periods beyond the transient
    Step 3. Se up Poincare map - period - stop on section. This lets you
    get the period
    Step 4. In numerics averaging - click on adjrange
    Step 5. Initconds range over the parameter. It should find the periodic
    orbit, adjoint, and H function and save. Files are of the form
    orbit.parname_parvalue.dat etc
*/
void make_adj_com(xpp::Session &s, int com)
{
static const char *const key="nmaohpr";
 switch(key[com]){
 case 'n': 
   new_adjoint(s);
   break;
 case 'm':
   new_h_fun(s,0);
   break;
 case 'a':
   adj_back(s);
   break;
 case 'o':
   data_back(s);
   break;
 case 'h':
   h_back(s);
   break;
 case 'p':
   adjoint_parameters(s);
   break;
 case 'r':
   s.integrator.adj_range=1;
   break;
 
 }
}

void adjoint_parameters(xpp::Session &s)
{
  new_int("Maximum iterates :",&s.adjoint.maxit);
  new_float(s,"Adjoint error tolerance :",&s.adjoint.err);
}

void new_h_fun(xpp::Session &s, int silent)
{

 int n=2;
 AdjointState &a=s.adjoint;
 if(!a.adjoint_here){
   command_error("adjoint", "Must compute adjoint first!");
   return;
 }
  if(s.data_store.rows!=a.adjoint_rows){
     command_error("adjoint", "incompatible data and adjoint");
     return;
   }
 if(a.h_here){
   a.h_function.release();
   a.h_here=false;
   a.h_odd_even=false;
 }
   if(s.model().neq>2){
     a.h_odd_even=true;
     n=4;
   }
   a.h_rows=s.data_store.rows;
   data_back(s); 
   float **h=a.h_function.make(s.data_store,n,a.h_rows,s.model().neq);
   if(make_h(s,s.data_store.col,a.adjoint.table(),h,a.h_rows,s.numerics.delta_t*s.numerics.store_every,s.model().node,silent )){
     a.h_here=true;
     h_back(s);
   }
 ping();
  
}


int make_h(xpp::Session &s, float **orb, float **adj, float **h, int nt, double dt, int node, int silent)
{

 int i,j,rval=0;
 float sum;
 double z;
 int n0=node+1+s.model().fix_var,k2,k;
 if(silent==0){
   for(i=0;i<s.model().node ;i++){
     std::string name=xpp::format("Coupling for {} eqn:",s.model().uvar_names[i]);
     new_string_of(name,s.adjoint.coup_string[i],XPP_FIELD_EXPRESSION);
     if(add_expr(s,s.adjoint.coup_string[i],s.adjoint.coup_fun[i].data(),&j)){
       command_error("adjoint", xpp::format("Illegal formula {}",s.adjoint.coup_string[i]));
       goto bye;
     }
   }
 }
 /*  formulae are fine .. lets do it ... */
 for(j=0;j<nt;j++)  /* j is phi variable  */
   {
     sum=0.0;
     
     for(k=0;k<nt;k++){
         k2=k+j;
       if(k2>=nt)k2=k2-nt+1;
       for(i=0;i<node;i++){
	 setvar(s,i+1,static_cast<double>(orb[i+1][k]));
         setvar(s,i+n0+1,static_cast<double>(orb[i+1][k2]));
       }
       z=0.0;
       update_based_on_current(s); 

       for(i=0;i<node;i++){
	
	 z=evaluate(s,s.adjoint.coup_fun[i].data());
	
	 sum=sum+static_cast<float>(z)*adj[i+1][k];
       }
	
     }
     h[0][j]=orb[0][j];
     h[1][j]=sum/static_cast<double>(nt);
   }
   if(s.adjoint.h_odd_even){
     for(k=0;k<nt;k++){
       k2=nt-k-1;
       h[2][k]=.5*(h[1][k]-h[1][k2]);
       h[3][k]=.5*(h[1][k]+h[1][k2]);
     }
   }
   rval=1;
      
 bye:
  s.parser.nsym=s.model().nsym_start;
  s.parser.ncon=s.model().ncon_start;
  return(rval);

}

void new_adjoint(xpp::Session &s)
{
 int n=s.model().node +1;
 AdjointState &a=s.adjoint;
 if(a.adjoint_here){
   data_back(s);
   a.adjoint.release();
   a.adjoint_here=false;
 }
 a.adjoint_rows=s.data_store.rows;
 float **adj=a.adjoint.make(s.data_store,n,a.adjoint_rows,s.model().neq);
 auto done=adjoint(s,s.data_store.col,adj,a.adjoint_rows,s.numerics.delta_t*s.numerics.store_every,ADJ_EPS,s.adjoint.err,s.adjoint.maxit,s.model().node );
 if(done){
   a.adjoint_here=true;
 adj_back(s);
 }
 else xpp::show_error(done.error());
 ping();
}
/* this computes the periodic orbit and stores it in 
   the usual place  given initial data and period */

/*    ADJOINT ROUTINE
 *
 *
      This assumes that you have already computed the periodic orbit
	and have stored in in an array **orbit
      including time in the first column

      The righthand sides of the equations are
	rhs(t,y,yp,n)
      and the coupling function for ``H'' functions is
	couple(y,yhat,f,n)

	where yhat is presynaptic and y is postynaptic
     variable.  f returns the coupling vector.

    adjoint is the same size as orbit and when returned has 
    t in the first column.
  */

xpp::Result<> adjoint(xpp::Session &s, float **orbit, float **adjnt, int nt, double dt, double eps, double minerr, int maxit, int node)
{
  double ytemp;
  double t,prod;
  int i,j,k,l,k2;
  xpp::Result<> rval;
  int n2=node*node;
  double error;

   std::vector<double> work_v(static_cast<size_t>(n2)+4*node);
   std::vector<double> yprime_v(node), yold_v(node), fold_v(node), fdev_v(node), dfdy_v(n2);
   double *work=work_v.data();
   double *yprime=yprime_v.data(), *yold=yold_v.data(), *fold=fold_v.data(), *fdev=fdev_v.data(), *dfdy=dfdy_v.data();
  std::vector<std::vector<double>> jac_store(n2);
  std::vector<double *> jac_v(n2);

  for(i=0;i<n2;i++)
  {
   jac_store[i].assign(nt, 0.0);
   jac_v[i]=jac_store[i].data();
   }
  double **jac=jac_v.data();

  /*  Now we compute the
	transpose time reversed jacobian  --  this is complex !! */
  for(k=0;k<nt;k++){
	l=nt-1-k;  /* reverse the limit cycle  */
	for(i=0;i<node;i++)yold[i]=static_cast<double>(orbit[i+1][l]);
        s.integrator.rhs(0.0,yold,fold,node);
	xpp::jacobian(s,0.0,yold,fold,node,eps,xpp::JacobianForm{},dfdy);
	for(i=0;i<node;i++)
	 for(j=0;j<node;j++)
	  jac[i+node*j][k]=dfdy[i*node+j];

 
      }
 
  /* now we iterate to get a good adjoint using implicit Euler's method */
  ytemp=0.0;
 for(i=0;i<node;i++){
	yold[i]=1.+.01*(s.random.uniform()-.5); /* random initial data */
	
	ytemp+=fabs(yold[i]);
	}
  for(i=0;i<node;i++){
    yold[i]=yold[i]/ytemp;
    fdev[i]=yold[i];
  }
	
  xpp::log(XPP_LOG_DEBUG, "{:f} {:f} \n",yold[0],yold[1]);

 for(l=0;l<maxit;l++){
	for(k=0;k<nt-1;k++){
		k2=k+1;
		if(k2>=nt)k2=k2-nt;
		if(auto e=step_eul(jac,k,k2,yold,work,node,dt);!e){
		  rval=e;
		  goto bye;
		} 
	      }
	ytemp=0.0;
	error=0.0;
	
         for(i=0;i<node;i++){
	  if(fabs(yold[i])>s.numerics.bound){
	    
	  rval=xpp::fail("adjoint","Out of bounds",command_place());
	  goto bye;
	  }
	error+=fabs(yold[i]-fdev[i]);
	ytemp+=fabs(yold[i]);
	
	}

        for(i=0;i<node;i++){ yold[i]=yold[i]/ytemp;
			     fdev[i]=yold[i];
			   }
	xpp::log(XPP_LOG_DEBUG, "{:f} {:f} \n",yold[0],yold[1]);
        xpp::log(XPP_LOG_DEBUG, "err={:f} \n",error);
	if(error<minerr)break; /*  exit if error small   */
 }
 /*  onelast time to compute the adjoint  */
 prod=0.0;   /* for normalization   */
 t=0.0;
 for(k=0;k<nt;k++){
        l=nt-k-1;
	t+=dt; 
        for(i=0;i<node;i++)fdev[i]=static_cast<double>(orbit[i+1][l]);
	s.integrator.rhs(0.0,fdev,yprime,node);
	for(j=0;j<node;j++){
	adjnt[j+1][l]=static_cast<float>(yold[j]);
	prod+=yold[j]*yprime[j]*dt;
      }
	k2=k+1;
	if(k2>=nt)k2-=nt;
	 if(auto e=step_eul(jac,k,k2,yold,work,node,dt);!e){
	  rval=e;
	  goto bye;
	    }	 

      }

	prod=prod/t;
  xpp::log(XPP_LOG_INFO, " Multiplying the adjoint by 1/{:g} to normalize\n",prod);
  for(k=0;k<nt;k++){
     for(j=0;j<node;j++)adjnt[j+1][k]=adjnt[j+1][k]/static_cast<float>(prod);
     adjnt[0][k]=orbit[0][k];
   }

 bye:
   return(rval);
 }

xpp::Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt)
{

int j,i,n2=node*node,info;
int ipvt[MAXODE];
double *mat,*fold;
fold=work;
mat=work+node;

  for(j=0;j<node;j++){
    fold[j]=0.0;
    for(i=0;i<node;i++)
      fold[j]=fold[j]+jac[i+j*node][k]*yold[i];
  }
  for(j=0;j<node;j++)yold[j]=yold[j]+.5*dt*fold[j];
  for(i=0;i<n2;i++)mat[i]=-jac[i][k2]*dt*.5;
  for(i=0;i<node;i++)mat[i+i*node]=1.+mat[i+i*node];
  xpp::sgefa(mat,node,node,ipvt,&info);
if(info!=-1){
  
  return xpp::fail("adjoint","Uninvertible Jacobian",command_place());
}
xpp::sgesl(mat,node,node,ipvt,yold);
return {};
}

/* this is some code for the maximal liapunov exponent
   I assume you have computed an orbit and it is in storage
   
   at each time point, I use y+dy as an initial condition
   I then integrate for one time step 
   I subtract this from y(t+dt) and divide by the norm of dy.
   I take the log of this and sum up the logs dividing by Ndt
   to get an approximation
*/

void do_liapunov(xpp::Session &s)
{
  int i;
  double *x;
  new_int("Range over parameters?(0/1)",&s.adjoint.liap_flag);
  if(s.adjoint.liap_flag!=1){
    auto z=hrw_liapunov(s,s.numerics.singpt_jacobian_epsilon);
    if(z)bottom_msg(0,xpp::format("Maximal exponent is {:g}",*z));
    else xpp::show_error(z.error());
    return;
  }
  x=&s.data_store.current[0];
  do_range(s,x,0); 
  /* done the range */
  for(i=0;i<s.adjoint.liap_i;i++){
    s.data_store.col[0][i]=s.adjoint.liap[0][i];
    s.data_store.col[1][i]=s.adjoint.liap[1][i];
  }
  s.data_store.rows=s.adjoint.liap_i;
  refresh_browser(s,s.data_store.rows);
  s.adjoint.liap_flag=0;
  for(auto &c : s.adjoint.liap)c.clear();
}

void alloc_liap(xpp::Session &s, int n)
{
  if(s.adjoint.liap_flag==0)return;
  for(auto &c : s.adjoint.liap)c.assign(n+1,0.0f);
  s.adjoint.liap_i=0;
}

void do_this_liaprun(xpp::Session &s, int i,double p)
{
 if(s.adjoint.liap_flag==0)return;
 s.adjoint.liap[0][i]=p;
 /* a sweep's step that fails is not shown: its point is 0 */
 s.adjoint.liap[1][i]=static_cast<float>(hrw_liapunov(s,s.numerics.singpt_jacobian_epsilon).value_or(0.0));
 s.adjoint.liap_i++;
}

void norm_vec(std::span<double> v, double &mu)
{
  double sum=0.0;
  for(double vi : v)
    sum+=(vi*vi);
  sum=sqrt(sum);
  if(sum>0)
    for(double &vi : v)
      vi=vi/sum;
  mu=sum;
}

xpp::Result<double> hrw_liapunov(xpp::Session &s, double eps)
{
 double y[MAXODE];
 double yp[MAXODE],nrm,dy[MAXODE];
 double t0,t1;
 double sum=0.0;
 int istart=1;
 int i,j;
  if(s.data_store.rows<2){
   return xpp::fail("Liapunov","You need to compute an orbit first",command_place());
 }

 /* lets make an initial random perturbation */
   for(i=0;i<s.model().node;i++)
      dy[i]=0; 
   dy[0]=eps;
   
   for(j=0;j<(s.data_store.rows-1);j++){
     t0=s.data_store.col[0][j];
     t1=s.data_store.col[0][j+1];
     istart=1;
     for(i=0;i<s.model().node;i++)
       y[i]=s.data_store.col[i+1][j]+dy[i];
     if(auto st=one_step_int(s,y,t0,t1,&istart);!st)return std::unexpected(st.error());
     for(i=0;i<s.model().node;i++)
       yp[i]=(y[i]-s.data_store.col[i+1][j+1]);
     norm_vec(std::span(yp,s.model().node),nrm);
     nrm=nrm/eps;
     if(nrm==0.0){
       return xpp::fail("Liapunov","Liapunov: -infinity exponent!",command_place()); /* something wrong here */
     }
     sum=sum+xpp::math::log(nrm);
    for(i=0;i<s.model().node;i++)
      dy[i]=eps*yp[i];

   }
   t1=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
   if(t1!=0)
     sum=sum/t1;

 return sum; /*  success !! */
}

} // namespace xpp
