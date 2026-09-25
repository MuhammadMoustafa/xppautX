#ifndef _numerics_h_
#define _numerics_h_
#ifdef __cplusplus
extern "C" {
#endif

/*       Numerics.h   */

extern double DELTA_T,TEND,T0,TRANS,NULL_ERR,EVEC_ERR,NEWT_ERR;
extern double BOUND,DELAY,TOLER,HMIN,HMAX;
extern double POIPLN;

extern int NMESH,NJMP,METHOD;
extern int EVEC_ITER,FOREVER;

extern int POIMAP,POIVAR,POISGN,SOS;

extern int HIST;

extern int XSHFT,YSHFT,ZSHFT;

void chk_volterra(void);
void check_pos(int *j);
void quick_num(int com);
void get_num_par(char ch);
void chk_delay(void);
void set_delay(void);
void ruelle(void);
void get_pmap_pars_com(int l);
void get_method(void);
void set_col_par_com(int i);
void do_meth(void);
void set_total(double total);
void user_set_color_par(int flag,char *via,double lo,double hi);
void compute_one_period(double period,double *x, char *name);

#ifdef __cplusplus
}
#endif
#endif
