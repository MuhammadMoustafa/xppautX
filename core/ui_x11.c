/* X11 front end's implementation of the XppUi seam (see xpp_ui.h).
   Installed by init_X() in main.c. */
#include <X11/Xlib.h>
#include "xpp_ui.h"
#include "browse.h"
#include "ggets.h"
#include "pop_list.h"
#include "init_conds.h"
#include "many_pops.h"
#include "graphics.h"
#include "kinescope.h"
#include "eig_list.h"

extern int Xup;
extern Window draw_win, main_win, info_pop;
extern int DCURY;
extern char *info_message;
extern BROWSER my_browser;

/* the renamed X11 originals */
void x11_err_msg(char *string);
void x11_ping(void);
int x11_new_string(char *name, char *value);
int x11_yes_no_box(void);
int x11_do_string_box(int n, int row, int col, char *title, char **names,
                      char values[][25], int maxchar);
int x11_file_selector(char *title, char *file, char *wild);
void x11_redraw_params(void);
void x11_redraw_ics(void);
void x11_redraw_all(void);
void x11_redraw_bcs(void);
void x11_redraw_delays(void);
void redraw_the_graph(void);
void x11_create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                       double *ev, int n);

static int x11_choose_key(char *title, char **items, char *keys, int n,
                          int def, char **hints)
{
    Window temp = main_win;
    return pop_up_list(&temp, title, items, keys, n, n, def, 10, DCURY + 8,
                       hints, info_pop, info_message);
}

static void x11_data_changed(int length)
{
    (void)length;
    if (Xup && my_browser.xflag == 1) draw_data(my_browser);
}

static void x11_draw_label(void) { draw_label(draw_win); }
static void x11_blank_draw_window(void) { blank_screen(draw_win); }

static const XppUi x11_ui = {
    x11_err_msg, x11_ping,
    x11_new_string, x11_yes_no_box, x11_do_string_box, x11_file_selector,
    x11_choose_key,
    x11_redraw_params, x11_redraw_ics, x11_redraw_all,
    x11_redraw_bcs, x11_redraw_delays, redraw_the_graph, x11_data_changed,
    x11_draw_label, x11_blank_draw_window, put_text_x11, film_clip,
    x11_create_eq_box,
};

void xpp_install_x11_ui(void) { xpp_set_ui(&x11_ui); }
