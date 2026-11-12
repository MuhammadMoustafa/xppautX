double N[29];
static double dv(double a,double b){if(b==0.0)b=N[0];return a/b;}
extern double f1_9(double);
extern double f1_10(double);
extern double f1_14(double);
extern double f2_12(double,double);
extern double h_arg(void*,int);
extern double h_end(void*,double,int);
double u0(double *c,double *v,void *S,double a0);
double u1(double *c,double *v,void *S,double a0);
double u0(double *c,double *v,void *S,double a0){double t0,t1,t2,t3;t0=f1_9(a0);t1=f2_12(t0,N[1]);if(t1==0.0){t2=u1(c,v,S,a0);t3=((f1_10(a0)-N[2])/t2);}else{t3=(((a0*(a0*a0))/N[5])+(((a0*a0)/N[4])+((a0/N[3])+N[2])));}return t3;}
double u1(double *c,double *v,void *S,double a0){return (a0==0.0?N[6]:a0);}
double p_eq0(double *c,double *v,void *S,const int *R){return (((((((c[6]-v[24])-v[25])-v[26])-v[27])-v[28])-v[29])/(c[7]==0.0?N[6]:c[7]));}
double p_eq1(double *c,double *v,void *S,const int *R){return ((v[16]-v[2])*((v[11]+v[10])*c[18]));}
double p_eq2(double *c,double *v,void *S,const int *R){return ((v[17]-v[3])*((v[13]+v[12])*c[18]));}
double p_eq3(double *c,double *v,void *S,const int *R){return ((v[18]-v[4])*((v[15]+v[14])*c[18]));}
double p_eq4(double *c,double *v,void *S,const int *R){return ((v[19]-v[5])/(v[20]==0.0?N[6]:v[20]));}
double p_eq5(double *c,double *v,void *S,const int *R){return ((v[21]-v[6])/N[3]);}
double p_eq6(double *c,double *v,void *S,const int *R){return ((v[22]-v[7])/N[7]);}
double p_eq7(double *c,double *v,void *S,const int *R){return ((v[23]-v[8])/N[8]);}
double p_eq8(double *c,double *v,void *S,const int *R){return (v[1]-c[17]);}
double p_eq9(double *c,double *v,void *S,const int *R){double t0;t0=u0(c,v,S,((N[10]-v[9])/N[11]));return (N[9]/(t0==0.0?N[6]:t0));}
double p_eq10(double *c,double *v,void *S,const int *R){double t0;t0=u0(c,v,S,((v[9]-N[13])/N[8]));return (N[12]/(t0==0.0?N[6]:t0));}
double p_eq11(double *c,double *v,void *S,const int *R){double t0;t0=f1_10(((N[15]-v[9])/N[16]));return (t0*N[14]);}
double p_eq12(double *c,double *v,void *S,const int *R){double t0;t0=f1_10(((N[13]-v[9])/N[8]));return (N[11]/(t0+N[2]));}
double p_eq13(double *c,double *v,void *S,const int *R){double t0;t0=u0(c,v,S,((N[18]-v[9])/N[8]));return (N[17]/(t0==0.0?N[6]:t0));}
double p_eq14(double *c,double *v,void *S,const int *R){double t0;t0=f1_10(((N[20]-v[9])/N[13]));return (t0*N[19]);}
double p_eq15(double *c,double *v,void *S,const int *R){double t0;t0=(v[11]+v[10]);return (v[10]/(t0==0.0?N[6]:t0));}
double p_eq16(double *c,double *v,void *S,const int *R){double t0;t0=(v[13]+v[12]);return (v[12]/(t0==0.0?N[6]:t0));}
double p_eq17(double *c,double *v,void *S,const int *R){double t0;t0=(v[15]+v[14]);return (v[14]/(t0==0.0?N[6]:t0));}
double p_eq18(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14((N[21]+v[1]));t1=f1_10((t0/N[20]));return (N[2]/(t1+N[2]));}
double p_eq19(double *c,double *v,void *S,const int *R){double t0,t1,t2,t3;t0=f1_10(((N[21]+v[1])/N[7]));t1=f1_14((N[21]+v[1]));t2=f1_10((t1/N[7]));t3=(t2+(t0*N[22]));return ((c[20]/(t3==0.0?N[6]:t3))/(c[19]==0.0?N[6]:c[19]));}
double p_eq20(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14((N[23]+v[1]));t1=f1_10((t0/N[7]));return (N[2]/(t1+N[2]));}
double p_eq21(double *c,double *v,void *S,const int *R){double t0;t0=f1_10(((N[24]+v[1])/N[4]));return (N[2]/(t0+N[2]));}
double p_eq22(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14((N[25]+v[1]));t1=f1_10((t0/N[26]));return (N[2]/(t1+N[2]));}
double p_eq23(double *c,double *v,void *S,const int *R){double t0,t1;t0=(v[2]*v[2]);t1=(v[2]*t0);return ((v[1]-c[15])*(v[3]*(t1*(c[8]*N[27]))));}
double p_eq24(double *c,double *v,void *S,const int *R){double t0,t1;t0=(v[4]*v[4]);t1=(t0*t0);return ((v[1]-c[16])*(t1*(c[9]*N[27])));}
double p_eq25(double *c,double *v,void *S,const int *R){return ((v[1]-c[14])*(c[10]*N[27]));}
double p_eq26(double *c,double *v,void *S,const int *R){return ((v[1]-c[16])*(v[5]*(c[11]*N[27])));}
double p_eq27(double *c,double *v,void *S,const int *R){double t0,t1;t0=(v[6]*v[6]);t1=(v[6]*t0);return ((v[1]-c[16])*(v[7]*(t1*(c[12]*N[27]))));}
double p_eq28(double *c,double *v,void *S,const int *R){return ((v[1]-c[15])*(v[8]*(c[13]*N[27])));}
double p_eq29(double *c,double *v,void *S,const int *R){return (v[29]+(v[28]+(v[27]+(v[26]+(v[25]+v[24])))));}
double p_eq30(double *c,double *v,void *S,const int *R){return v[5];}
double p_fun0(double*c,double*v,void*S,const int*R){double z=u0(c,v,S,h_arg(S,0));return h_end(S,z,1);}
double p_fun1(double*c,double*v,void*S,const int*R){double z=u1(c,v,S,h_arg(S,0));return h_end(S,z,1);}
void p_rhs(double *c,double *v,void *S,double *o){double t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t20,t21,t22,t23,t24;v[9]=(v[1]-c[17]);t0=u0(c,v,S,((N[10]-v[9])/N[11]));v[10]=(N[9]/(t0==0.0?N[6]:t0));t1=u0(c,v,S,((v[9]-N[13])/N[8]));v[11]=(N[12]/(t1==0.0?N[6]:t1));t2=f1_10(((N[15]-v[9])/N[16]));v[12]=(t2*N[14]);t3=f1_10(((N[13]-v[9])/N[8]));v[13]=(N[11]/(t3+N[2]));t4=u0(c,v,S,((N[18]-v[9])/N[8]));v[14]=(N[17]/(t4==0.0?N[6]:t4));t5=f1_10(((N[20]-v[9])/N[13]));v[15]=(t5*N[19]);t6=(v[11]+v[10]);v[16]=(v[10]/(t6==0.0?N[6]:t6));t7=(v[13]+v[12]);v[17]=(v[12]/(t7==0.0?N[6]:t7));t8=(v[15]+v[14]);v[18]=(v[14]/(t8==0.0?N[6]:t8));t9=f1_14((N[21]+v[1]));t10=f1_10((t9/N[20]));v[19]=(N[2]/(t10+N[2]));t11=f1_10(((N[21]+v[1])/N[7]));t12=f1_10((t9/N[7]));t13=(t12+(t11*N[22]));v[20]=((c[20]/(t13==0.0?N[6]:t13))/(c[19]==0.0?N[6]:c[19]));t14=f1_14((N[23]+v[1]));t15=f1_10((t14/N[7]));v[21]=(N[2]/(t15+N[2]));t16=f1_10(((N[24]+v[1])/N[4]));v[22]=(N[2]/(t16+N[2]));t17=f1_14((N[25]+v[1]));t18=f1_10((t17/N[26]));v[23]=(N[2]/(t18+N[2]));t19=(v[2]*v[2]);t20=(v[2]*t19);v[24]=((v[1]-c[15])*(v[3]*(t20*(c[8]*N[27]))));t21=(v[4]*v[4]);t22=(t21*t21);v[25]=((v[1]-c[16])*(t22*(c[9]*N[27])));v[26]=((v[1]-c[14])*(c[10]*N[27]));v[27]=((v[1]-c[16])*(v[5]*(c[11]*N[27])));t23=(v[6]*v[6]);t24=(v[6]*t23);v[28]=((v[1]-c[16])*(v[7]*(t24*(c[12]*N[27]))));v[29]=((v[1]-c[15])*(v[8]*(c[13]*N[27])));if(!o)return;o[0]=(((((((c[6]-v[24])-v[25])-v[26])-v[27])-v[28])-v[29])/(c[7]==0.0?N[6]:c[7]));o[1]=((v[16]-v[2])*((v[11]+v[10])*c[18]));o[2]=((v[17]-v[3])*((v[13]+v[12])*c[18]));o[3]=((v[18]-v[4])*((v[15]+v[14])*c[18]));o[4]=((v[19]-v[5])/(v[20]==0.0?N[6]:v[20]));o[5]=((v[21]-v[6])/N[3]);o[6]=((v[22]-v[7])/N[7]);o[7]=((v[23]-v[8])/N[8]);}
