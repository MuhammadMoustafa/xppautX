#ifndef _userbut_h_
#define _userbut_h_


#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
typedef struct {
  Window w;
  char bname[10];
  int com;
} USERBUT;

void user_button_events(XEvent report);
void user_button_press(Window w);
void user_button_draw(Window w);
void user_button_cross(Window w, int b);
#endif /* Xlib.h */
int get_button_info(char *s, char *bname, char *sc);
int find_kbs(char *sc);
void add_user_button(char *s);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void create_user_buttons(int x0, int y0, Window base);



#endif /* Xlib.h */
#endif
