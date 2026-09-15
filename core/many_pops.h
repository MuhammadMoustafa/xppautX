#ifndef _many_pops_h
#define _many_pops_h


int select_table(void);
void get_intern_set(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void make_icon(char *icon, int wid, int hgt, Window w);
#endif /* Xlib.h */
void title_text(char *string);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void gtitle_text(char *string, Window win);
#endif /* Xlib.h */
void restore_off(void);
void restore_on(void);
#include "grobs.h"
void destroy_a_pop(void);
void init_grafs(int x, int y, int w, int h);
void ps_restore(void);
void svg_restore(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
int rotate3dcheck(XEvent ev);
void do_motion_events(XEvent ev);
void do_expose(XEvent ev);
#endif /* Xlib.h */
void resize_all_pops(int wid, int hgt);
void kill_all_pops(void);
void create_a_pop(void);
void GrCol(void);
void BaseCol(void);
void SmallGr(void);
void SmallBase(void);
void change_plot_vars(int k);
int check_active_plot(int k);
int graph_used(int i);
void make_active(int i,int flag);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void select_window(Window w);
#endif /* Xlib.h */
void set_gr_fore(void);
void set_gr_back(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void hi_lite(Window wi);
void lo_lite(Window wi);
void select_sym(Window w);
#endif /* Xlib.h */
void canvas_xy(char *buf);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void check_draw_button(XEvent ev);
#endif /* Xlib.h */
void set_active_windows();


#endif
