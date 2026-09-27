#ifndef _numerics_h_
#define _numerics_h_
#ifdef __cplusplus
extern "C" {
#endif

/*       Numerics.h   */

/* CVODE's banded Jacobian (the nUmerics menu's Stiff settings) */
extern int cv_bandflag,cv_bandupper,cv_bandlower;

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
void user_set_color_par(int flag,const char *via,double lo,double hi);
void compute_one_period(double period,double *x, const char *name);

#ifdef __cplusplus
}
#endif
#endif
