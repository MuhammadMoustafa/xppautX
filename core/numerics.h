#ifndef _numerics_h_
#define _numerics_h_

namespace xpp {

struct Session; /* session.h */
struct Model; /* model.h */

void check_pos(int *j);

void chk_volterra(Session &s);
void quick_num(Session &s, int com);
/* why method m cannot integrate model (Volterra without integral
   equations, Symplectic with an odd dimension), NULL when it can */
const char *method_refusal(const Model &model, int m);
/* what a new delta_t needs: the delays' and the integrals' memory again */
void dt_changed(Session &s);
void set_total(Session &s, double total);
void get_num_par(Session &s, char ch);
void chk_delay(Session &s);
void set_delay(Session &s);
void ruelle(Session &s);
void compute_one_period(Session &s, double period,double *x, const char *name);
void get_pmap_pars_com(Session &s, int l);
void get_method(Session &s);
void user_set_color_par(Session &s, int flag,const char *via,double lo,double hi);
void set_col_par_com(Session &s, int i);
/* applies numerics.method (Volterra when the model has integrals): its
   settings, then a fresh solver (xpp::start_solver) */
void do_meth(Session &s);

} // namespace xpp
#endif
