#ifndef _eig_list_h_
#define _eig_list_h_
#ifdef __cplusplus
extern "C" {
#endif


void create_eq_list(void);
void eq_list_up(void);
void eq_list_down(void);
void eq_box_import(void);
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);

#ifdef __cplusplus
}
#endif
#endif
