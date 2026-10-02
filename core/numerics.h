#ifndef _numerics_h_
#define _numerics_h_

namespace xpp {

struct Session; /* session.h */
void quick_num(Session &s, int com);

/* what a new delta_t needs: the delays' and the integrals' memory again */
void dt_changed(Session &s);
void set_total(Session &s, double total);
void get_num_par(Session &s, char ch);
void chk_delay(Session &s);
void set_delay(Session &s);
void ruelle(Session &s);
void compute_one_period(Session &s, double period,double *x, const char *name);
void get_pmap_pars_com(Session &s, int l);
/* the method picked from the Method menu (the one in use when none) */
int chosen_method(const Session &s);
void user_set_color_par(Session &s, int flag,const char *via,double lo,double hi);
void set_col_par_com(Session &s, int i);
/* applies the checked numerics.method: its
   settings, then a fresh solver (xpp::start_solver) */
void do_meth(Session &s);

} // namespace xpp
#endif
