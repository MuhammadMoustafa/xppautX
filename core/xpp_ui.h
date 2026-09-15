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
 *   menu_choose returns the chosen key character, 0 or 27 if none.
 *   check_abort returns a key code, 27 for escape, 64 when nothing happened.
 */

#include <stdio.h>

struct XppMenu; /* menus.h */

#define XPP_AUTO_CLICK 1000

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
    int (*two_choice)(char *c1, char *c2, char *q, char *key, char *title);
    void (*respond_box)(char *button, char *message); /* alert with one button */
    /* toggle a set of flags (1/0) by name; flags are edited in place and
       restored on cancel. Returns 1 for done, 0 for cancel. */
    int (*checklist)(char *title, char **names, int *flags, int n);
    int (*string_box)(int n, int row, int col, char *title, char **names,
                      char values[][25], int maxchar);
    int (*file_selector)(char *title, char *file, char *wild);
    /* one-line text entry with named buttons; returns 0 on cancel */
    int (*dialog)(char *title, char *name, char *value, char *ok, char *cancel,
                  int max);
    /* like string_box but for n long strings (MAX_LEN_EBOX); returns 0 on
       cancel */
    int (*edit_box)(int n, char *title, char **names, char **values);
    int (*get_mouse_xy)(int *x, int *y);

    /* menus. show_menu makes MAIN_MENU, FILE_MENU or NUM_MENU (menus.h) the
       main-window menu; core sets help_menu and dispatches keys itself
       (commander in commands.c). menu_choose pops up a menu, def is the
       highlighted item. */
    void (*menu_flash)(int num);
    void (*show_menu)(int which);
    void (*redraw_menu)(void);
    int (*menu_choose)(const struct XppMenu *m, int def);

    /* long-running loops poll these */
    int (*check_abort)(void);
    int (*progress_begin)(void);                     /* returns bar width */
    void (*progress)(int nit, int icount, int cwidth);
    void (*flush)(void);

    /* things changed, please redraw */
    void (*redraw_params)(void);
    /* parameter i now has text value s; redraw that one entry */
    void (*param_box_set)(int i, char *s);
    void (*param_box_redraw)(int i);
    /* the same for initial condition i */
    void (*ic_box_set)(int i, char *s);
    void (*ic_box_redraw)(int i);
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
    void (*browser_redraw)(int full); /* my_browser: 1 columns too, 0 data */

    /* plot windows */
    void (*activate_graph)(int i, int flag); /* graph i became MyGraph */
    void (*create_plot_window)(void);
    void (*destroy_plot_window)(void); /* the active one; not the main window */
    void (*kill_plot_windows)(void);   /* all but the main window */
    void (*lower_plot_window)(void);   /* put the active one at the bottom */
    void (*gr_col)(void);   /* pen: graph colours (GrCol) */
    void (*base_col)(void); /* pen: window colours (BaseCol) */
    /* Text,etc (T)ext: ask for a label, place it with the mouse, draw it
       and add_label() it */
    void (*cput_text)(void);
    void (*get_draw_size)(unsigned int *w, unsigned int *h);
    void (*draw_freeze)(void); /* frozen curves */
    void (*blank_draw_window)(void);
    void (*put_text)(int x, int y, char *s);
    void (*small_base)(void);  /* pen selection for small text */
    void (*small_gr)(void);
    int (*film_clip)(void); /* returns 0 when the movie buffer is full */
    void (*reset_film)(void);
    /* kinescope: the captured frames live in the front end */
    void (*movie_play_back)(void);  /* step through frames with keys/mouse */
    void (*movie_auto_play)(void);  /* ks_ncycle cycles, ks_speed ms apart */
    void (*movie_save)(char *basename, int fmat); /* 1 ppm, 2 gif */
    void (*movie_make_anigif)(void);

    /* mouse interaction in the plot window. rubber_band returns 1 and the
       corners in pixels when the user drew a box (flag RUBBOX) or line
       (RUBLINE), 0 when cancelled. scroll_window lets the user drag the
       view until a key is pressed (calling update_view). */
    int (*rubber_band)(int *i1, int *j1, int *i2, int *j2, int flag);
    void (*scroll_window)(void);

    /* colormap changed (custom_color); X11 reallocates its colours */
    void (*new_colormap)(int type);

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

    /* array plot window (arrayplot.h: aplot) */
    void (*aplot_make)(char *name); /* open the array plot window */
    void (*aplot_redraw)(void);
    void (*aplot_reset_axes)(void);  /* its title and z range labels */
    void (*aplot_draw_one)(char *tag); /* redraw, tag and save a range frame */

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
    /* Grab: wait for a key (returns its code, mykeydef.h) or a click on
       the diagram (returns XPP_AUTO_CLICK with the pixel in x,y) */
    int (*auto_grab_event)(int *x, int *y);
    void (*auto_show_hint)(void); /* Auto.hinttxt changed */

    /* animation (toon) window. Frames are drawn off screen, vcr.wid by
       vcr.hgt pixels (aniparse.h), then ani_show puts one on screen. */
    void (*new_vcr)(void);
    void (*ani_clear)(void);    /* white frame, black pen */
    void (*ani_show)(void);
    void (*ani_color)(int icol); /* 0 black, else a colour index */
    void (*ani_thick)(int t);
    void (*ani_font)(int size, int font, int color); /* font 0 roman, 1 symbol */
    void (*ani_line)(int x1, int y1, int x2, int y2);
    void (*ani_rect)(int x, int y, int w, int h, int fill);
    void (*ani_arc)(int x, int y, int w, int h, int fill); /* ellipse in box */
    void (*ani_text)(int x, int y, char *s);
    void (*ani_slider)(void);    /* vcr.pos changed */

    /* misc front-end hooks called while loading an ODE file */
    void (*init_txtview)(void);

    /* equilibrium eigenvalue summary window */
    void (*show_eq_box)(int cp, int cm, int rp, int rm, int im, double *y,
                        double *ev, int n);

    /* Whole dialogs a front end provides; the core has no logic in them
       beyond what they call back (do_calc, the ODE source in save_eqn).
       Headless: they do nothing. */
    void (*make_txtview)(void); /* File/Prt src: source and active comments */
    void (*q_calc)(void);       /* File/Calculator: evaluate formulas */

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
void respond_box(char *button, char *message);
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][25], int maxchar);
int file_selector(char *title, char *file, char *wild);
int get_dialog(char *wname, char *name, char *value, char *ok, char *cancel, int max);
int do_edit_box(int n, char *title, char **names, char **values);
int GetMouseXY(int *x, int *y);
void flash(int num);
int menu_choose(const struct XppMenu *m, int def);
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
void destroy_a_pop(void);
void kill_all_pops(void);
void GrCol(void);
void BaseCol(void);
void cput_text(void);
void SmallBase(void);
void SmallGr(void);
void reset_film(void);
void set_color(int col);
void draw_one_array_plot(char *bob);
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
void init_txtview(void);
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n);
void bye_bye(void);
void draw_help(void);
int rubber_band(int *i1, int *j1, int *i2, int *j2, int flag);
void scroll_window(void);
void NewColormap(int type);
void make_my_aplot(char *name);
void new_vcr(void);
void redraw_the_graph(void);
void make_txtview(void);
void q_calc(void);

#endif
