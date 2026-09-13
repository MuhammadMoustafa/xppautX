#ifndef XPP_UI_H
#define XPP_UI_H

/*
 * The seam between the numerics and whatever front end is driving them.
 *
 * Core code keeps calling the historical names (err_msg, new_int, ping,
 * refresh_browser, TwoChoice, set_color, ALINE, ...). Those are thin
 * dispatchers in xpp_ui.c that go through the XppUi table below. The
 * default table is headless: messages go to the log, prompts are declined,
 * redraws and drawing do nothing. The X11 front end installs its own table
 * (ui_x11.c) before it shows a window.
 *
 * Conventions kept from the original code:
 *   new_string / file_selector / string_box return 0 when the user cancels.
 *   yes_no_box returns 1 for yes, 0 for no.
 *   two_choice returns the chosen key character, 0 if none.
 *   check_abort returns a key code, 27 for escape, 64 when nothing happened.
 */

#include <stdio.h>

/* ids for submenu(); these are menus that the nUmerics key handler in
   numerics.c pops up */
enum {
    XPP_SUBMENU_COLOR = 0,
    XPP_SUBMENU_LOOKUP,
    XPP_SUBMENU_ADJOINT,
    XPP_SUBMENU_POINCARE,
    XPP_SUBMENU_FROZEN_CLINE,
    XPP_SUBMENU_STOCHASTIC
};

typedef struct XppUi {
    /* messages */
    void (*err_msg)(char *msg);
    void (*ping)(void);
    void (*bottom_msg)(int line, char *msg);
    void (*message_box)(char *msg);
    void (*kill_message_box)(void);
    void (*title_text)(char *s);
    void (*canvas_xy)(char *s);

    /* prompts */
    int (*new_string)(char *name, char *value);
    int (*yes_no_box)(void);
    int (*two_choice)(char *c1, char *c2, char *q, char *key);
    int (*string_box)(int n, int row, int col, char *title, char **names,
                      char values[][25], int maxchar);
    int (*file_selector)(char *title, char *file, char *wild);
    /* single-key chooser used for the integration method menu; returns the
       chosen key character, or 0 */
    int (*choose_key)(char *title, char **items, char *keys, int n, int def,
                      char **hints);
    int (*get_mouse_xy)(int *x, int *y);
    void (*edit_ics)(void); /* walk the user through every initial condition */

    /* menus driven from numerics.c */
    void (*menu_flash)(int num);
    void (*menu_help)(void);
    void (*submenu)(int id);

    /* long-running loops poll these */
    int (*check_abort)(void);
    int (*progress_begin)(void);                     /* returns bar width */
    void (*progress)(int nit, int icount, int cwidth);
    void (*flush)(void);

    /* things changed, please redraw */
    void (*redraw_params)(void);
    void (*redraw_ics)(void);
    void (*redraw_all)(void);
    void (*redraw_bcs)(void);
    void (*redraw_delays)(void);
    void (*redraw_graph)(void);
    void (*redraw_screens)(void);   /* every plot window */
    void (*clear_screens)(void);
    void (*clear_draw_window)(void);
    void (*reset_graphics)(void);
    void (*data_changed)(int length); /* browser storage grew/shrank */

    /* plot windows */
    void (*activate_graph)(int i, int flag); /* graph i became MyGraph */
    void (*create_plot_window)(void);
    void (*get_draw_size)(unsigned int *w, unsigned int *h);
    void (*draw_label)(void);
    void (*draw_freeze)(void); /* frozen curves */
    void (*blank_draw_window)(void);
    void (*put_text)(int x, int y, char *s);
    void (*small_base)(void);  /* pen selection for small text */
    void (*small_gr)(void);
    int (*film_clip)(void); /* returns 0 when the movie buffer is full */
    void (*reset_film)(void);
    void (*on_the_fly)(int task); /* animation hook during integration */
    void (*freeze_curve)(void);   /* auto-freeze after a range run */

    /* raw drawing primitives used by graphics.c when the plot format is
       the screen (PS and SVG are handled in graphics.c itself) */
    void (*draw_point)(int x, int y);
    void (*draw_line)(int x1, int y1, int x2, int y2);
    void (*draw_bead)(int x, int y);
    void (*draw_frect)(int x, int y, int w, int h);
    void (*draw_text)(int x, int y, char *s);
    void (*draw_special_text)(int x, int y, char *s, int size);
    void (*draw_linestyle)(int ls);
    void (*set_color)(int col);

    /* array plot window */
    void (*aplot_init)(void);
    void (*aplot_close_files)(void);
    void (*aplot_draw_one)(char *tag);
    void (*aplot_io)(FILE *fp, int f);

    /* AUTO bifurcation window */
    void (*auto_make_window)(char *wname, char *iname);
    void (*auto_line)(int a, int b, int c, int d);
    void (*auto_text)(int a, int b, char *c);
    void (*auto_circle)(int x, int y, int r);
    void (*auto_fill_circle)(int x, int y, int r);
    void (*auto_xor_cross)(int x, int y);
    void (*auto_line_width)(int wid);
    void (*auto_col)(int col);
    void (*auto_bw)(void);
    void (*auto_clr_stab)(void);
    void (*auto_stab_line)(int x, int y, int xp, int yp);
    void (*auto_clear_plot)(void);
    void (*auto_redraw_menus)(void);
    void (*auto_clear_info)(void);
    void (*auto_draw_info)(char *s, int x, int y);
    void (*auto_refresh)(void);
    int (*auto_check_abort)(int *iflag);
    int (*auto_rubber)(int *i1, int *j1, int *i2, int *j2, int flag);
    int (*auto_choose_key)(char *title, char **list, char *key, int n, int max,
                           int def, int x, int y, char **hints, char *httxt);
    void (*auto_scroll_window)(void);
    void (*auto_traverse_diagram)(void);

    /* misc front-end hooks called while loading an ODE file */
    void (*init_txtview)(void);
    void (*add_user_button)(char *s);

    /* equilibrium eigenvalue summary window */
    void (*show_eq_box)(int cp, int cm, int rp, int rm, int im, double *y,
                        double *ev, int n);

    /* program is quitting */
    void (*exit_program)(void);
} XppUi;

/* The active table. Never NULL; defaults to the headless implementation. */
extern XppUi xpp_ui;

void xpp_set_ui(const XppUi *ui); /* copies; missing entries keep defaults */

/* Historical names, now dispatchers. Declared here so every core file sees
   one consistent prototype. */
void err_msg(char *string);
void ping(void);
int plintf(char *fmt, ...);
void bottom_msg(int line, char *msg);
void MessageBox(char *m);
void KillMessageBox(void);
void title_text(char *s);
void canvas_xy(char *s);
int new_string(char *name, char *value);
int new_int(char *name, int *value);
int new_float(char *name, double *value);
int yes_no_box(void);
int TwoChoice(char *c1, char *c2, char *q, char *key);
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][25], int maxchar);
int file_selector(char *title, char *file, char *wild);
int GetMouseXY(int *x, int *y);
void man_ic(void);
void flash(int num);
void help(void);
void set_col_par(void);
void new_lookup(void);
void make_adj(void);
void get_pmap_pars(void);
void froz_cline_stuff(void);
void do_stochast(void);
int my_abort(void);
int get_command_width(void);
void plot_command(int nit, int icount, int cwidth);
void FlushDisplay(void);
void redraw_params(void);
void redraw_ics(void);
void redraw_all(void);
void redraw_bcs(void);
void redraw_delays(void);
void drw_all_scrns(void);
void clr_all_scrns(void);
void clear_draw_window(void);
void reset_graphics(void);
void create_a_pop(void);
void SmallBase(void);
void SmallGr(void);
void reset_film(void);
void on_the_fly(int task);
void auto_freeze_it(void);
void set_color(int col);
void init_my_aplot(void);
void close_aplot_files(void);
void draw_one_array_plot(char *bob);
void dump_aplot(FILE *fp, int f);
void make_auto(char *wname, char *iname);
void ALINE(int a, int b, int c, int d);
void ATEXT(int a, int b, char *c);
void Circle(int x, int y, int r);
void FillCircle(int x, int y, int r);
void XORCross(int x, int y);
void LineWidth(int wid);
void autocol(int col);
void autobw(void);
void clr_stab(void);
void auto_stab_line(int x, int y, int xp, int yp);
void clear_auto_plot(void);
void redraw_auto_menus(void);
void clear_auto_info(void);
void draw_auto_info(char *bob, int x, int y);
void refreshdisplay(void);
int byeauto_(int *iflag);
int auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag);
int auto_pop_up_list(char *title, char **list, char *key, int n, int max,
                     int def, int x, int y, char **hints, char *httxt);
void auto_scroll_window(void);
void traverse_diagram(void);
void init_txtview(void);
void add_user_button(char *s);
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n);
void bye_bye(void);

#endif
