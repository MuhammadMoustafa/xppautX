#ifndef XPP_UI_H
#define XPP_UI_H

/*
 * The seam between the numerics and whatever front end is driving them.
 *
 * Core code keeps calling the historical names (err_msg, new_int, ping,
 * refresh_browser, ...). Those are now thin dispatchers in xpp_ui.c that go
 * through the XppUi table below. The default table is headless: messages go
 * to the log, prompts are declined, redraws do nothing. The X11 front end
 * installs its own table (ui_x11.c) before it shows a window.
 *
 * Conventions kept from the original code:
 *   new_string / file_selector / string_box return 0 when the user cancels.
 *   yes_no_box returns 1 for yes, 0 for no.
 */

#include <stdio.h>

typedef struct XppUi {
    /* messages */
    void (*err_msg)(char *msg);
    void (*ping)(void);

    /* prompts */
    int (*new_string)(char *name, char *value);
    int (*yes_no_box)(void);
    int (*string_box)(int n, int row, int col, char *title, char **names,
                      char values[][25], int maxchar);
    int (*file_selector)(char *title, char *file, char *wild);
    /* single-key chooser used for the integration method menu; returns the
       chosen key character, or 0 */
    int (*choose_key)(char *title, char **items, char *keys, int n, int def,
                      char **hints);

    /* things changed, please redraw */
    void (*redraw_params)(void);
    void (*redraw_ics)(void);
    void (*redraw_all)(void);
    void (*redraw_bcs)(void);
    void (*redraw_delays)(void);
    void (*redraw_graph)(void);
    void (*data_changed)(int length); /* browser storage grew/shrank */

    /* drawing on the main plot; only used by range/movie loops */
    void (*draw_label)(void);
    void (*blank_draw_window)(void);
    void (*put_text)(int x, int y, char *s);
    int (*film_clip)(void); /* returns 0 when the movie buffer is full */

    /* equilibrium eigenvalue summary window */
    void (*show_eq_box)(int cp, int cm, int rp, int rm, int im, double *y,
                        double *ev, int n);
} XppUi;

/* The active table. Never NULL; defaults to the headless implementation. */
extern XppUi xpp_ui;

void xpp_set_ui(const XppUi *ui); /* copies; missing entries keep defaults */

/* Historical names, now dispatchers. Declared here so every core file sees
   one consistent prototype. */
void err_msg(char *string);
void ping(void);
int plintf(char *fmt, ...);
int new_string(char *name, char *value);
int new_int(char *name, int *value);
int new_float(char *name, double *value);
int yes_no_box(void);
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][25], int maxchar);
int file_selector(char *title, char *file, char *wild);
void redraw_params(void);
void redraw_ics(void);
void redraw_all(void);
void redraw_bcs(void);
void redraw_delays(void);
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n);

#endif
