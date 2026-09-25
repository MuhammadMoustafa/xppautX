#ifndef _init_conds_h_
#define _init_conds_h_

#include "xpplim.h"
#include "read_dir.h"
#ifdef __cplusplus
extern "C" {
#endif



#define FILESELNWIN 10
void c_hints(void);
void clone_ode(void);
int find_user_name(int type, const char *oname);
void resize_par_slides(int h);
void redraw_directory(void);
void redraw_file_list(void);
void new_wild(void);
void fs_scroll(int i);
int do_file_select_events(void);
void create_file_selector(const char *title, const char *file, const char *wild);
void stringintersect(const char *target, const char *sother);
void destroy_selector(void);
int file_selector(const char *title, char *file, const char *wild);
void reset_sliders(void);
void make_new_ic_box(void);
void make_new_bc_box(void);
void make_new_delay_box(void);
void make_new_param_box(void);
void initialize_box(void);
void get_nrow_from_hgt(int h, int *n, int *w);
void redraw_params(void);
void redraw_ics(void);
void set_up_xvt(void);
void set_up_pp(void);
void set_up_arry(void);
void man_ic(void);
void new_parameter(void);
void redo_stuff(void);
void set_default_params(void);
void check_box_cursor(void);
void prt_focus(void);

#ifdef __cplusplus
}
#endif
#endif
