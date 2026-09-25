#include <stdlib.h> 
#include "auto_f2c.h" 
#include "odesol2.h"
#include "auto_nox.h"
#include "autlim.h"
#include "derived.h"
#include "pp_shoot.h"
#include "xAuto.h"

void redo_all_fun_tables();
void getjactrans(double *x,double *y,double *yp,double *xp, double eps, double *d, int n);
extern XAUTO xAuto;

/*    Hooks to xpp RHS     */

extern double constants[],last_ic[];

extern int Auto_index_to_array[8];
extern int NewPeriodFlag;
extern int AutoTwoParam,NAutoPar;
extern int HomoFlag;
extern double homo_l[100],homo_r[100];
extern int METHOD,NJMP;
extern double outperiod[];
extern int UzrPar[],NAutoUzr;

extern double NEWT_ERR;
int func(integer ndim, double *u, integer *icp, double *par, integer ijac, double *f, double *dfdu, double *dfdp)
{
   int i,j;
   double zz[NAUTO];
   double  y[NAUTO],yp[NAUTO],xp[NAUTO];
   for(i=0;i<NAutoPar;i++){
     constants[Auto_index_to_array[i]]=par[i];
     
   }
   evaluate_derived();
   redo_all_fun_tables();
   rhs(0.0,u,f,ndim);
   if(ijac==1){
     getjactrans(u,y,yp,xp,NEWT_ERR,dfdu,ndim);
   }
   if(METHOD>0||NJMP==1)return 0;
   for(i=1;i<NJMP;i++){
     for(j=0;j<ndim;j++)
       zz[j]=f[j];
     rhs(0.0,zz,f,ndim);
   }

   return 0;





} /* func_ */


int stpnt(integer ndim, doublereal t, doublereal *u, doublereal *par)
{
  int i;

  double p;

  for(i=0;i<NAutoPar;i++)
    par[i] = constants[Auto_index_to_array[i]];

  if(NewPeriodFlag==0){  
    for(i=0;i<ndim;i++)
      u[i]=last_ic[i];
    return 0;
  }

  get_start_period(&p);
  par[10]=p;
  if(HomoFlag!=1)get_start_orbit(u,t,p,ndim);
  /*  printf("%d %d %g %g %g %g \n",ndim,HomoFlag,t,u[0],u[1],p); */
  if(HomoFlag==1){

    get_shifted_orbit(u,t,p,ndim);
    for(i=0;i<ndim;i++){
      par[11+i]=homo_l[i];

    }
  }
  if(HomoFlag==2){ /* heteroclinic */
    for(i=0;i<ndim;i++){
      par[11+i]=homo_l[i];
      par[11+i+ndim]=homo_r[i];

    }

  }
  return 0;

} /* stpnt_ */







/* Subroutine */ int bcnd(integer ndim, double *par, integer *icp, integer nbc, double *u0, double *u1, integer ijac, double *fb, double *dbc)
{
 int i;
/* Hooks to the XPP bc parser!! */

 for(i=0;i<NAutoPar;i++){
     constants[Auto_index_to_array[i]]=par[i];
 }

 
 evaluate_derived();
 redo_all_fun_tables();
 do_bc(u0,0.0,u1,1.0,fb,nbc);

    return 0;
} /* bcnd_ */

/* AUTO's user routines that xppautX does not supply: stubs, with
   auto_c.h's prototypes (which AUTO calls them through) */
/* Subroutine */ int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint,
	 const doublereal *u, const doublereal *uold, const doublereal *udot,
	 const doublereal *upold, integer ijac,
	 doublereal *fi, doublereal *dint)
{
   /*
   int i;
   double dum=0.0;
  for(i=0;i<Homo_n;i++)
    dum+=upold[i]*(u[i]-uold[i]);
  fi[0]=dum;
   */
    return 0;
} /* icnd_ */

/* Subroutine */ int fopt(integer ndim, const doublereal *u, const integer *icp,
	 const doublereal *par, integer ijac,
	 doublereal *fs, doublereal *dfdu, doublereal *dfdp)
{
/*     ---------- ---- */
    return 0;
} /* fopt_ */

/*  Not sure what to do here; I think  do nothing  since IEQUIB is always
    -2 
*/
int pvls (integer ndim, const doublereal *u,
          doublereal *par)
{
  return 0;
}


