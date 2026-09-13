#ifndef _eig_list_h_
#define _eig_list_h_


#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_eq_list(Window w);
#endif /* Xlib.h */
void create_eq_list(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void eq_list_keypress(XEvent ev, int *used);
void enter_eq_stuff(Window w, int b);
void eq_list_button(XEvent ev);
#endif /* Xlib.h */
void eq_list_up(void);
void eq_list_down(void);
void eq_box_import(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void get_new_size(Window win, unsigned int *wid, unsigned int *hgt);
void resize_eq_list(Window win);
void eq_box_button(Window w);
#endif /* Xlib.h */
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_eq_box(Window w);


#endif /* Xlib.h */
#endif
