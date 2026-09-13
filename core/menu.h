#ifndef _xppmenu_h_
#define _xppmenu_h_



void flash(int num);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void add_menu(Window base, int j, int n, char **names, char *key, char **hint);
void create_the_menus(Window base);
#endif /* Xlib.h */
void show_menu(int j);
void unshow_menu(int j);
void help(void);
void help_num(void);
void help_file(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void menu_crossing(Window win, int yn);
void menu_expose(Window win);
void menu_button(Window win);
#endif /* Xlib.h */
void draw_help(void);

#endif
