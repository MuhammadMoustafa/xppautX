/* Headless defaults for the UI seam, plus the dispatchers that keep the
   historical function names working. See xpp_ui.h. */
#include "xpp_ui.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *logfile;
extern int XPPVERBOSE;

int do_calc(char *temp, double *z); /* calc.c */

/* ---- headless defaults ------------------------------------------------ */

static void hl_err_msg(char *msg) { plintf("%s\n", msg); }
static void hl_ping(void) {}
static int hl_new_string(char *name, char *value) { (void)name; (void)value; return 0; }
static int hl_yes_no_box(void) { return 0; }
static int hl_string_box(int n, int row, int col, char *title, char **names,
                         char values[][25], int maxchar)
{
    (void)n; (void)row; (void)col; (void)title; (void)names; (void)values; (void)maxchar;
    return 0;
}
static int hl_file_selector(char *title, char *file, char *wild)
{
    (void)title; (void)file; (void)wild;
    return 0;
}
static int hl_choose_key(char *title, char **items, char *keys, int n, int def,
                         char **hints)
{
    (void)title; (void)items; (void)hints;
    if (def >= 0 && def < n) return keys[def];
    return 0;
}
static void hl_void(void) {}
static void hl_data_changed(int length) { (void)length; }
static void hl_put_text(int x, int y, char *s) { (void)x; (void)y; (void)s; }
static int hl_film_clip(void) { return 1; }
static void hl_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                           double *ev, int n)
{
    int i;
    plintf("Equilibrium: c+=%d c-=%d r+=%d r-=%d im=%d\n", cp, cm, rp, rm, im);
    for (i = 0; i < n; i++)
        plintf("  y[%d]=%.8g  eig=%.8g%+.8gi\n", i, y[i], ev[2 * i], ev[2 * i + 1]);
}

XppUi xpp_ui = {
    hl_err_msg, hl_ping,
    hl_new_string, hl_yes_no_box, hl_string_box, hl_file_selector, hl_choose_key,
    hl_void, hl_void, hl_void, hl_void, hl_void, hl_void, hl_data_changed,
    hl_void, hl_void, hl_put_text, hl_film_clip,
    hl_show_eq_box,
};

void xpp_set_ui(const XppUi *ui)
{
    XppUi d = xpp_ui;
#define TAKE(f) if (ui->f) d.f = ui->f
    TAKE(err_msg); TAKE(ping);
    TAKE(new_string); TAKE(yes_no_box); TAKE(string_box); TAKE(file_selector);
    TAKE(choose_key);
    TAKE(redraw_params); TAKE(redraw_ics); TAKE(redraw_all); TAKE(redraw_bcs); TAKE(redraw_delays); TAKE(redraw_graph); TAKE(data_changed);
    TAKE(draw_label); TAKE(blank_draw_window); TAKE(put_text); TAKE(film_clip);
    TAKE(show_eq_box);
#undef TAKE
    xpp_ui = d;
}

/* ---- dispatchers with the historical names ---------------------------- */

void err_msg(char *string) { xpp_ui.err_msg(string); }
void ping(void) { xpp_ui.ping(); }
int new_string(char *name, char *value) { return xpp_ui.new_string(name, value); }
int yes_no_box(void) { return xpp_ui.yes_no_box(); }
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][25], int maxchar)
{
    return xpp_ui.string_box(n, row, col, title, names, values, maxchar);
}
int file_selector(char *title, char *file, char *wild)
{
    return xpp_ui.file_selector(title, file, wild);
}
void redraw_params(void) { xpp_ui.redraw_params(); }
void redraw_ics(void) { xpp_ui.redraw_ics(); }
void redraw_all(void) { xpp_ui.redraw_all(); }
void redraw_bcs(void) { xpp_ui.redraw_bcs(); }
void redraw_delays(void) { xpp_ui.redraw_delays(); }
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n)
{
    xpp_ui.show_eq_box(cp, cm, rp, rm, im, y, ev, n);
}

/* plintf, new_int and new_float were in ggets.c; they never touched X. */

int plintf(char *fmt, ...)
{
    int nchar = 0;
    va_list arglist;

    if (!XPPVERBOSE) return nchar; /* Don't print at all! */

    if (logfile == NULL) {
        printf("The log file is NULL!\n");
        logfile = stdout;
    }

    va_start(arglist, fmt);
    nchar = vfprintf(logfile, fmt, arglist);
    va_end(arglist);
    /* Flush so log info survives a crash. */
    fflush(logfile);

    return nchar;
}

int new_int(char *name, int *value)
{
    char svalue[200];
    sprintf(svalue, "%d", *value);
    if (new_string(name, svalue) == 0 || strlen(svalue) == 0) return -1;
    *value = atoi(svalue);
    return 0;
}

int new_float(char *name, double *value)
{
    int done;
    int flag;
    double newz;
    char tvalue[200];
    sprintf(tvalue, "%.16g", *value);
    done = new_string(name, tvalue);
    if (done == 0 || strlen(tvalue) == 0) return -1;

    if (tvalue[0] == '%') {
        flag = do_calc(&tvalue[1], &newz);
        if (flag != -1) *value = newz;
        return 0;
    }
    *value = atof(tvalue);

    return 0;
}
