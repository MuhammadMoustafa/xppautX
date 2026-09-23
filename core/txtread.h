#ifndef _txtread_h
#define _txtread_h
#ifdef __cplusplus
extern "C" {
#endif



#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void txt_view_events(XEvent ev);
void txtview_keypress(XEvent ev);
void enter_txtview(Window w, int val);
#endif /* Xlib.h */
void do_txt_action(char *s);
void resize_txtview(int w, int h);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void txtview_press(Window w, int x, int y);
void redraw_txtview(Window w);
#endif /* Xlib.h */
void redraw_txtview_text(void);
void init_txtview(void);
void make_txtview(void);


#ifdef __cplusplus
}
#endif
#endif
