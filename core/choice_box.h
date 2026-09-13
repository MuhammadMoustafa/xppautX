#ifndef _choice_box_h_
#define _choice_box_h_

#include "struct.h"

void destroy_choice(CHOICE_BOX p);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void display_choice(Window w, CHOICE_BOX p);
#endif /* Xlib.h */
void do_checks(CHOICE_BOX p);
void base_choice(char *wname, int n, int mcc, char **names, int *check, int type);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
int do_choice_box(Window root, char *wname, int n, int mcc, char **names, int *check, int type);
#endif /* Xlib.h */
int choice_box_event_loop(CHOICE_BOX p);

#endif
