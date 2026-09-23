#ifndef _calc_h_
#define _calc_h_
#ifdef __cplusplus
extern "C" {
#endif



#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_calc(Window w);
#endif /* Xlib.h */
void make_calc(double z);
void quit_calc(void);
void ini_calc_string(char *name, char *value, int *pos, int *col);
void q_calc(void);
int do_calc(char *temp, double *z);
int has_eq(char *z, char *w, int *where);
double calculate(char *expr, int *ok);


#ifdef __cplusplus
}
#endif
#endif
