double N[47];
static double dv(double a,double b){if(b==0.0)b=N[0];return a/b;}
extern double f1_10(double);
extern double f1_14(double);
extern double f2_5(double,double);
extern double f2_6(double,double);
extern double h_arg(void*,int);
extern double h_end(void*,double,int);
double u0(double *c,double *v,void *S,double a0);
double u0(double *c,double *v,void *S,double a0){return (a0==0.0?N[1]:a0);}
double p_eq0(double *c,double *v,void *S,const int *R){return ((v[11]-v[1])/(v[12]==0.0?N[1]:v[12]));}
double p_eq1(double *c,double *v,void *S,const int *R){return ((v[17]-v[2])/(v[18]==0.0?N[1]:v[18]));}
double p_eq2(double *c,double *v,void *S,const int *R){return ((v[23]-v[3])/(v[24]==0.0?N[1]:v[24]));}
double p_eq3(double *c,double *v,void *S,const int *R){double t0,t1,t2;t0=((c[43]/(c[44]==0.0?N[1]:c[44]))+N[2]);t1=f1_14(v[25]);t2=(c[46]*(c[45]*(N[4]*N[3])));return (((t1/(t2==0.0?N[1]:t2))-((v[4]-c[41])*c[42]))*(N[2]/(t0==0.0?N[1]:t0)));}
double p_eq4(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(v[6]);return ((c[52]+(((((t0-v[7])-v[13])-v[19])-v[25])-v[26]))/(c[7]==0.0?N[1]:c[7]));}
double p_eq5(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[5]);return ((v[5]-t0)*c[11]);}
double p_eq6(double *c,double *v,void *S,const int *R){double t0,t1,t2,t3;t0=f1_10(((v[5]-c[13])/(c[14]==0.0?N[1]:c[14])));t1=(N[2]/(t0+N[2]));t2=(t1*t1);t3=(t1*t2);return ((v[5]-N[7])*(v[1]*(t3*c[12])));}
double p_eq7(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14(((v[5]-c[16])/(c[17]==0.0?N[1]:c[17])));t1=f1_10(t0);return (t1*c[15]);}
double p_eq8(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_10(((v[5]-c[19])/(c[20]==0.0?N[1]:c[20])));t1=(N[2]-t0);return (((v[5]-c[19])*c[18])/(t1==0.0?N[1]:t1));}
double p_eq9(double *c,double *v,void *S,const int *R){double t0;t0=f2_6((v[9]+v[8]),N[8]);return t0;}
double p_eq10(double *c,double *v,void *S,const int *R){return (v[8]/(v[10]==0.0?N[1]:v[10]));}
double p_eq11(double *c,double *v,void *S,const int *R){return (N[2]/(v[10]==0.0?N[1]:v[10]));}
double p_eq12(double *c,double *v,void *S,const int *R){double t0,t1,t2;t0=(v[2]*v[2]);t1=(t0*t0);t2=f1_14(N[10]);return ((v[5]-t2)*(t1*c[21]));}
double p_eq13(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_10(((v[5]-c[23])/(c[24]==0.0?N[1]:c[24])));t1=(N[2]-t0);return (((v[5]-c[23])*c[22])/(t1==0.0?N[1]:t1));}
double p_eq14(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14((v[5]-c[26]));t1=f1_10((t0/(c[27]==0.0?N[1]:c[27])));return (t1*c[25]);}
double p_eq15(double *c,double *v,void *S,const int *R){double t0;t0=f2_6((v[15]+v[14]),N[8]);return t0;}
double p_eq16(double *c,double *v,void *S,const int *R){return (v[14]/(v[16]==0.0?N[1]:v[16]));}
double p_eq17(double *c,double *v,void *S,const int *R){return (N[2]/(v[16]==0.0?N[1]:v[16]));}
double p_eq18(double *c,double *v,void *S,const int *R){double t0,t1;t0=(v[3]*v[3]);t1=f1_14(N[10]);return ((v[5]-t1)*(t0*c[28]));}
double p_eq19(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_10(((v[5]-c[30])/(c[31]==0.0?N[1]:c[31])));t1=(N[2]-t0);return (((v[5]-c[30])*c[29])/(t1==0.0?N[1]:t1));}
double p_eq20(double *c,double *v,void *S,const int *R){double t0,t1;t0=f1_14((v[5]-c[33]));t1=f1_10((t0/(c[34]==0.0?N[1]:c[34])));return (t1*c[32]);}
double p_eq21(double *c,double *v,void *S,const int *R){double t0;t0=f2_6((v[21]+v[20]),N[8]);return t0;}
double p_eq22(double *c,double *v,void *S,const int *R){return (v[20]/(v[22]==0.0?N[1]:v[22]));}
double p_eq23(double *c,double *v,void *S,const int *R){return (N[2]/(v[22]==0.0?N[1]:v[22]));}
double p_eq24(double *c,double *v,void *S,const int *R){double t0,t1,t2;t0=f1_10(((v[5]-c[36])/(c[37]==0.0?N[1]:c[37])));t1=(N[2]/(t0+N[2]));t2=(t1*t1);return ((v[5]-N[11])*(t2*c[35]));}
double p_eq25(double *c,double *v,void *S,const int *R){double t0,t1,t2;t0=f2_5((v[4]/(c[39]==0.0?N[1]:c[39])),c[40]);t1=(t0+N[2]);t2=f1_14(N[10]);return ((v[5]-t2)*((t0/(t1==0.0?N[1]:t1))*c[38]));}
double p_fun0(double*c,double*v,void*S,const int*R){double z=u0(c,v,S,h_arg(S,0));return h_end(S,z,1);}
double p_derived0(double *c,double *v,void *S,const int *R){return N[12];}
double p_derived1(double *c,double *v,void *S,const int *R){return N[13];}
double p_derived2(double *c,double *v,void *S,const int *R){return N[13];}
double p_derived3(double *c,double *v,void *S,const int *R){return N[14];}
double p_derived4(double *c,double *v,void *S,const int *R){return N[15];}
double p_derived5(double *c,double *v,void *S,const int *R){return N[16];}
double p_derived6(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[17]);return t0;}
double p_derived7(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[18]);return t0;}
double p_derived8(double *c,double *v,void *S,const int *R){return N[19];}
double p_derived9(double *c,double *v,void *S,const int *R){return N[20];}
double p_derived10(double *c,double *v,void *S,const int *R){return N[21];}
double p_derived11(double *c,double *v,void *S,const int *R){return N[22];}
double p_derived12(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[23]);return t0;}
double p_derived13(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[24]);return t0;}
double p_derived14(double *c,double *v,void *S,const int *R){return N[25];}
double p_derived15(double *c,double *v,void *S,const int *R){return N[26];}
double p_derived16(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[12]);return t0;}
double p_derived17(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[27]);return t0;}
double p_derived18(double *c,double *v,void *S,const int *R){return N[28];}
double p_derived19(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[29]);return t0;}
double p_derived20(double *c,double *v,void *S,const int *R){return N[30];}
double p_derived21(double *c,double *v,void *S,const int *R){return N[31];}
double p_derived22(double *c,double *v,void *S,const int *R){return N[32];}
double p_derived23(double *c,double *v,void *S,const int *R){return N[33];}
double p_derived24(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[34]);return t0;}
double p_derived25(double *c,double *v,void *S,const int *R){return N[35];}
double p_derived26(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[36]);return t0;}
double p_derived27(double *c,double *v,void *S,const int *R){return N[37];}
double p_derived28(double *c,double *v,void *S,const int *R){return N[15];}
double p_derived29(double *c,double *v,void *S,const int *R){return N[6];}
double p_derived30(double *c,double *v,void *S,const int *R){double t0;t0=f1_14(N[38]);return t0;}
double p_derived31(double *c,double *v,void *S,const int *R){return N[21];}
double p_derived32(double *c,double *v,void *S,const int *R){return N[39];}
double p_derived33(double *c,double *v,void *S,const int *R){return N[40];}
double p_derived34(double *c,double *v,void *S,const int *R){return N[41];}
double p_derived35(double *c,double *v,void *S,const int *R){return N[42];}
double p_derived36(double *c,double *v,void *S,const int *R){return N[43];}
double p_derived37(double *c,double *v,void *S,const int *R){return N[44];}
double p_derived38(double *c,double *v,void *S,const int *R){return N[45];}
double p_derived39(double *c,double *v,void *S,const int *R){return N[44];}
double p_derived40(double *c,double *v,void *S,const int *R){return N[14];}
double p_derived41(double *c,double *v,void *S,const int *R){return N[46];}
double p_derived42(double *c,double *v,void *S,const int *R){return N[2];}
double p_derived43(double *c,double *v,void *S,const int *R){return N[13];}
double p_derived44(double *c,double *v,void *S,const int *R){return (c[48]+((c[50]+c[48])*(c[49]-N[2])));}
double p_derived45(double *c,double *v,void *S,const int *R){return c[6];}
void p_rhs(double *c,double *v,void *S,double *o){double t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t30,t31,t32;t0=f1_14(N[5]);v[6]=((v[5]-t0)*c[11]);t1=f1_10(((v[5]-c[13])/(c[14]==0.0?N[1]:c[14])));t2=(N[2]/(t1+N[2]));t3=(t2*t2);t4=(t2*t3);v[7]=((v[5]-N[7])*(v[1]*(t4*c[12])));t5=f1_14(((v[5]-c[16])/(c[17]==0.0?N[1]:c[17])));t6=f1_10(t5);v[8]=(t6*c[15]);t7=f1_10(((v[5]-c[19])/(c[20]==0.0?N[1]:c[20])));t8=(N[2]-t7);v[9]=(((v[5]-c[19])*c[18])/(t8==0.0?N[1]:t8));t9=f2_6((v[9]+v[8]),N[8]);v[10]=t9;v[11]=(v[8]/(v[10]==0.0?N[1]:v[10]));v[12]=(N[2]/(v[10]==0.0?N[1]:v[10]));t10=(v[2]*v[2]);t11=(t10*t10);t12=f1_14(N[10]);v[13]=((v[5]-t12)*(t11*c[21]));t13=f1_10(((v[5]-c[23])/(c[24]==0.0?N[1]:c[24])));t14=(N[2]-t13);v[14]=(((v[5]-c[23])*c[22])/(t14==0.0?N[1]:t14));t15=f1_14((v[5]-c[26]));t16=f1_10((t15/(c[27]==0.0?N[1]:c[27])));v[15]=(t16*c[25]);t17=f2_6((v[15]+v[14]),N[8]);v[16]=t17;v[17]=(v[14]/(v[16]==0.0?N[1]:v[16]));v[18]=(N[2]/(v[16]==0.0?N[1]:v[16]));t18=(v[3]*v[3]);v[19]=((v[5]-t12)*(t18*c[28]));t19=f1_10(((v[5]-c[30])/(c[31]==0.0?N[1]:c[31])));t20=(N[2]-t19);v[20]=(((v[5]-c[30])*c[29])/(t20==0.0?N[1]:t20));t21=f1_14((v[5]-c[33]));t22=f1_10((t21/(c[34]==0.0?N[1]:c[34])));v[21]=(t22*c[32]);t23=f2_6((v[21]+v[20]),N[8]);v[22]=t23;v[23]=(v[20]/(v[22]==0.0?N[1]:v[22]));v[24]=(N[2]/(v[22]==0.0?N[1]:v[22]));t24=f1_10(((v[5]-c[36])/(c[37]==0.0?N[1]:c[37])));t25=(N[2]/(t24+N[2]));t26=(t25*t25);v[25]=((v[5]-N[11])*(t26*c[35]));t27=f2_5((v[4]/(c[39]==0.0?N[1]:c[39])),c[40]);t28=(t27+N[2]);v[26]=((v[5]-t12)*((t27/(t28==0.0?N[1]:t28))*c[38]));if(!o)return;o[0]=((v[11]-v[1])/(v[12]==0.0?N[1]:v[12]));o[1]=((v[17]-v[2])/(v[18]==0.0?N[1]:v[18]));o[2]=((v[23]-v[3])/(v[24]==0.0?N[1]:v[24]));t29=((c[43]/(c[44]==0.0?N[1]:c[44]))+N[2]);t30=f1_14(v[25]);t31=(c[46]*(c[45]*(N[4]*N[3])));o[3]=(((t30/(t31==0.0?N[1]:t31))-((v[4]-c[41])*c[42]))*(N[2]/(t29==0.0?N[1]:t29)));t32=f1_14(v[6]);o[4]=((c[52]+(((((t32-v[7])-v[13])-v[19])-v[25])-v[26]))/(c[7]==0.0?N[1]:c[7]));}
