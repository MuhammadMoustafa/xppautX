#ifndef XPP_UI_H
#define XPP_UI_H

/*
 * The seam between the numerics and whatever front end is driving them.
 *
 * Core code keeps calling the historical names (err_msg, new_int, ping,
 * refresh_browser, TwoChoice, set_color, ...). Those are thin dispatchers
 * in xpp_ui.c that go through the XppUi table below. The default table is
 * headless: messages go to the log, prompts are declined, redraws and
 * drawing do nothing. The X11 front end installs its own table (ui_x11.c)
 * before it shows a window.
 *
 * Conventions kept from the original code:
 *   new_string / file_selector / string_box return 0 when the user cancels.
 *   yes_no_box returns 1 for yes, 0 for no.
 *   two_choice returns the chosen key character, 0 if none.
 *   check_abort returns a key code, 27 for escape, 64 when nothing happened.
 */

#include <stdio.h>

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
    void (*data_changed)(int length); /* browser storage grew/shrank */

    /* plot windows */
    void (*activate_graph)(int i, int flag); /* graph i became MyGraph */
    void (*get_draw_size)(unsigned int *w, unsigned int *h);
    void (*draw_label)(void);
    void (*blank_draw_window)(void);
    void (*put_text)(int x, int y, char *s);
    void (*small_base)(void);  /* pen selection for small text */
    void (*small_gr)(void);
    int (*film_clip)(void); /* returns 0 when the movie buffer is full */
    void (*reset_film)(void);
    void (*on_the_fly)(int task); /* animation hook during integration */

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
void SmallBase(void);
void SmallGr(void);
void reset_film(void);
void on_the_fly(int task);
void set_color(int col);
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n);
void bye_bye(void);

#endif
