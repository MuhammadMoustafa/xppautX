/* X11 front end's implementation of the XppUi seam (see xpp_ui.h).
   Installed by init_X() in main.c. */
#include <X11/Xlib.h>
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "browse.h"
#include "ggets.h"
#include "pop_list.h"
#include "init_conds.h"
#include "many_pops.h"
#include "graphics.h"
#include "kinescope.h"
#include "eig_list.h"

extern Window draw_win, main_win, info_pop;
extern int DCURY;
extern char *info_message;
extern BROWSER my_browser;

/* the renamed X11 originals */
void x11_err_msg(char *string);
void x11_ping(void);
void x11_bottom_msg(int line, char *msg);
void x11_MessageBox(char *m);
void x11_KillMessageBox(void);
void x11_title_text(char *s);
void x11_canvas_xy(char *s);
int x11_new_string(char *name, char *value);
int x11_yes_no_box(void);
int x11_TwoChoice(char *c1, char *c2, char *q, char *key);
int x11_do_string_box(int n, int row, int col, char *title, char **names,
                      char values[][25], int maxchar);
int x11_file_selector(char *title, char *file, char *wild);
int x11_GetMouseXY(int *x, int *y);
int x11_my_abort(void);
int x11_get_command_width(void);
void x11_plot_command(int nit, int icount, int cwidth);
void x11_FlushDisplay(void);
void x11_redraw_params(void);
void x11_redraw_ics(void);
void x11_redraw_all(void);
void x11_redraw_bcs(void);
void x11_redraw_delays(void);
void redraw_the_graph(void);
void x11_drw_all_scrns(void);
void x11_clr_all_scrns(void);
void x11_clear_draw_window(void);
void x11_get_draw_size(unsigned int *w, unsigned int *h);
void x11_SmallBase(void);
void x11_SmallGr(void);
void x11_reset_film(void);
void x11_on_the_fly(int task);
void x11_set_color(int col);
void x11_create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                       double *ev, int n);
void x11_bye_bye(void);

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

static void x11_activate_graph(int i, int flag)
{
    (void)i;
    draw_win = MyGraph->w;
    get_draw_area_flag(flag);
}

static void x11_draw_label(void) { draw_label(draw_win); }
static void x11_blank_draw_window(void) { blank_screen(draw_win); }

static const XppUi x11_ui = {
    /* messages */
    x11_err_msg, x11_ping, x11_bottom_msg, x11_MessageBox, x11_KillMessageBox,
    x11_title_text, x11_canvas_xy,
    /* prompts */
    x11_new_string, x11_yes_no_box, x11_TwoChoice, x11_do_string_box,
    x11_file_selector, x11_choose_key, x11_GetMouseXY,
    /* polling */
    x11_my_abort, x11_get_command_width, x11_plot_command, x11_FlushDisplay,
    /* redraw */
    x11_redraw_params, x11_redraw_ics, x11_redraw_all, x11_redraw_bcs,
    x11_redraw_delays, redraw_the_graph, x11_drw_all_scrns, x11_clr_all_scrns,
    x11_clear_draw_window, x11_data_changed,
    /* plot windows */
    x11_activate_graph, x11_get_draw_size, x11_draw_label,
    x11_blank_draw_window, put_text_x11, x11_SmallBase, x11_SmallGr,
    film_clip, x11_reset_film, x11_on_the_fly,
    /* drawing */
    point_x11, line_x11, bead_x11, rect_x11, put_text_x11,
    special_put_text_x11, set_line_style_x11, x11_set_color,
    /* misc */
    x11_create_eq_box,
    x11_bye_bye,
};

void xpp_install_x11_ui(void) { xpp_set_ui(&x11_ui); }
