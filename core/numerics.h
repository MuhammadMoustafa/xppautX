#ifndef _numerics_h_
#define _numerics_h_
#ifdef __cplusplus
extern "C" {
#endif

/*       Numerics.h   */


void check_pos(int *j);

#ifdef __cplusplus
}


namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */
}

void chk_volterra(xpp::Session &s);
void quick_num(xpp::Session &s, int com);
/* why method m cannot integrate model (Volterra without integral
   equations, Symplectic with an odd dimension), NULL when it can */
const char *method_refusal(const xpp::Model &model, int m);
/* what a new delta_t needs: the delays' and the integrals' memory again */
void dt_changed(xpp::Session &s);
void set_total(xpp::Session &s, double total);
void get_num_par(xpp::Session &s, char ch);
void chk_delay(xpp::Session &s);
void set_delay(xpp::Session &s);
void ruelle(xpp::Session &s);
void compute_one_period(xpp::Session &s, double period,double *x, const char *name);
void get_pmap_pars_com(xpp::Session &s, int l);
void get_method(xpp::Session &s);
void user_set_color_par(xpp::Session &s, int flag,const char *via,double lo,double hi);
void set_col_par_com(xpp::Session &s, int i);
/* applies numerics.method (Volterra when the model has integrals): its
   settings, then a fresh solver (xpp::start_solver) */
void do_meth(xpp::Session &s);
#endif
#endif
