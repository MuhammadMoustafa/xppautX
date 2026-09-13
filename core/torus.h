#ifndef _torus_h_
#define _torus_h_


void do_torus_com(int c);
void draw_tor_var(int i);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_torus_box(Window win);
#endif /* Xlib.h */
void choose_torus(void);
void make_tor_box(char *title);
void do_torus_events(void);




#endif
