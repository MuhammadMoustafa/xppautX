
#ifndef _dialog_box_h
#define _dialog_box_h

#include "struct.h"

int dialog_event_loop(DIALOG *d, int max, int *pos, int *col);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
int x11_get_dialog(char *wname, char *name, char *value, char *ok, char *cancel, int max);
void display_dialog(Window w, DIALOG d, int pos, int col);


#endif /* Xlib.h */
#endif
