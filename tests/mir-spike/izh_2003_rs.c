double N[8];
static double dv(double a,double b){if(b==0.0)b=N[0];return a/b;}
extern double f1_16(double);
double p_eq0(double *c,double *v,void *S,const int *R){double t0;t0=(v[1]*v[1]);return (v[5]+((N[4]+((v[1]*N[3])+(t0*N[1])))-v[2]));}
double p_eq1(double *c,double *v,void *S,const int *R){return (((v[1]*c[7])-v[2])*c[6]);}
double p_eq2(double *c,double *v,void *S,const int *R){return N[5];}
double p_eq3(double *c,double *v,void *S,const int *R){return N[5];}
double p_eq4(double *c,double *v,void *S,const int *R){double t0;t0=f1_16((v[0]-c[11]));return (t0*c[10]);}
double p_eq5(double *c,double *v,void *S,const int *R){return v[5];}
double p_condition0(double *c,double *v,void *S,const int *R){return (v[1]-N[6]);}
double p_event0_0(double *c,double *v,void *S,const int *R){return c[8];}
double p_event0_1(double *c,double *v,void *S,const int *R){return (c[9]+v[2]);}
double p_event0_2(double *c,double *v,void *S,const int *R){return (N[7]+v[3]);}
double p_event0_3(double *c,double *v,void *S,const int *R){return v[0];}
void p_rhs(double *c,double *v,void *S,double *o){double t0,t1;t0=f1_16((v[0]-c[11]));v[5]=(t0*c[10]);if(!o)return;t1=(v[1]*v[1]);o[0]=(v[5]+((N[4]+((v[1]*N[3])+(t1*N[1])))-v[2]));o[1]=(((v[1]*c[7])-v[2])*c[6]);o[2]=N[5];o[3]=N[5];}
