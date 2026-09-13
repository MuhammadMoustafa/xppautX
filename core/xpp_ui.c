/* Headless defaults for the UI seam, plus the dispatchers that keep the
   historical function names working. See xpp_ui.h. */
#include "xpp_ui.h"
#include "xpp_globals.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int do_calc(char *temp, double *z); /* xpp_util.c */

/* ---- headless defaults ------------------------------------------------ */

static void hl_err_msg(char *msg) { plintf("%s\n", msg); }
static void hl_void(void) {}
static void hl_str(char *s) { (void)s; }
static void hl_bottom_msg(int line, char *msg) { (void)line; (void)msg; }
static int hl_new_string(char *name, char *value) { (void)name; (void)value; return 0; }
static int hl_no(void) { return 0; }
static int hl_two_choice(char *c1, char *c2, char *q, char *key)
{
    (void)c1; (void)c2; (void)q; (void)key;
    return 0;
}
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
static int hl_get_mouse_xy(int *x, int *y) { (void)x; (void)y; return 0; }
static int hl_check_abort(void) { return 64; }
static void hl_progress(int nit, int icount, int cwidth) { (void)nit; (void)icount; (void)cwidth; }
static void hl_data_changed(int length) { (void)length; }
static void hl_activate_graph(int i, int flag) { (void)i; (void)flag; }
static void hl_get_draw_size(unsigned int *w, unsigned int *h)
{
    /* whatever the graph last had, else a sensible canvas */
    *w = MyGraph && MyGraph->x11Wid > 0 ? (unsigned int)MyGraph->x11Wid : 640;
    *h = MyGraph && MyGraph->x11Hgt > 0 ? (unsigned int)MyGraph->x11Hgt : 480;
}
static void hl_put_text(int x, int y, char *s) { (void)x; (void)y; (void)s; }
static int hl_film_clip(void) { return 1; }
static void hl_task(int task) { (void)task; }
static void hl_draw_point(int x, int y) { (void)x; (void)y; }
static void hl_draw_line(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
static void hl_draw_frect(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static void hl_draw_special_text(int x, int y, char *s, int size) { (void)x; (void)y; (void)s; (void)size; }
static void hl_int(int v) { (void)v; }
static void hl_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                           double *ev, int n)
{
    int i;
    plintf("Equilibrium: c+=%d c-=%d r+=%d r-=%d im=%d\n", cp, cm, rp, rm, im);
    for (i = 0; i < n; i++)
        plintf("  y[%d]=%.8g  eig=%.8g%+.8gi\n", i, y[i], ev[2 * i], ev[2 * i + 1]);
}
static void hl_exit_program(void) { exit(1); }

XppUi xpp_ui = {
    /* messages */
    hl_err_msg, hl_void, hl_bottom_msg, hl_str, hl_void, hl_str, hl_str,
    /* prompts */
    hl_new_string, hl_no, hl_two_choice, hl_string_box, hl_file_selector,
    hl_choose_key, hl_get_mouse_xy,
    /* polling */
    hl_check_abort, hl_no, hl_progress, hl_void,
    /* redraw */
    hl_void, hl_void, hl_void, hl_void, hl_void, hl_void, hl_void, hl_void,
    hl_void, hl_data_changed,
    /* plot windows */
    hl_activate_graph, hl_get_draw_size, hl_void, hl_void, hl_put_text,
    hl_void, hl_void, hl_film_clip, hl_void, hl_task,
    /* drawing */
    hl_draw_point, hl_draw_line, hl_draw_point, hl_draw_frect, hl_put_text,
    hl_draw_special_text, hl_int, hl_int,
    /* misc */
    hl_show_eq_box,
    hl_exit_program,
};

void xpp_set_ui(const XppUi *ui)
{
    XppUi d = xpp_ui;
#define TAKE(f) if (ui->f) d.f = ui->f
    TAKE(err_msg); TAKE(ping); TAKE(bottom_msg); TAKE(message_box);
    TAKE(kill_message_box); TAKE(title_text); TAKE(canvas_xy);
    TAKE(new_string); TAKE(yes_no_box); TAKE(two_choice); TAKE(string_box);
    TAKE(file_selector); TAKE(choose_key); TAKE(get_mouse_xy);
    TAKE(check_abort); TAKE(progress_begin); TAKE(progress); TAKE(flush);
    TAKE(redraw_params); TAKE(redraw_ics); TAKE(redraw_all); TAKE(redraw_bcs);
    TAKE(redraw_delays); TAKE(redraw_graph); TAKE(redraw_screens);
    TAKE(clear_screens); TAKE(clear_draw_window); TAKE(data_changed);
    TAKE(activate_graph); TAKE(get_draw_size); TAKE(draw_label);
    TAKE(blank_draw_window); TAKE(put_text); TAKE(small_base); TAKE(small_gr);
    TAKE(film_clip); TAKE(reset_film); TAKE(on_the_fly);
    TAKE(draw_point); TAKE(draw_line); TAKE(draw_bead); TAKE(draw_frect);
    TAKE(draw_text); TAKE(draw_special_text); TAKE(draw_linestyle);
    TAKE(set_color);
    TAKE(show_eq_box); TAKE(exit_program);
#undef TAKE
    xpp_ui = d;
}

/* ---- dispatchers with the historical names ---------------------------- */

void err_msg(char *string) { xpp_ui.err_msg(string); }
void ping(void) { xpp_ui.ping(); }
void bottom_msg(int line, char *msg) { xpp_ui.bottom_msg(line, msg); }
void MessageBox(char *m) { xpp_ui.message_box(m); }
void KillMessageBox(void) { xpp_ui.kill_message_box(); }
void title_text(char *s) { xpp_ui.title_text(s); }
void canvas_xy(char *s) { xpp_ui.canvas_xy(s); }
int new_string(char *name, char *value) { return xpp_ui.new_string(name, value); }
int yes_no_box(void) { return xpp_ui.yes_no_box(); }
int TwoChoice(char *c1, char *c2, char *q, char *key)
{
    return xpp_ui.two_choice(c1, c2, q, key);
}
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][25], int maxchar)
{
    return xpp_ui.string_box(n, row, col, title, names, values, maxchar);
}
int file_selector(char *title, char *file, char *wild)
{
    return xpp_ui.file_selector(title, file, wild);
}
int GetMouseXY(int *x, int *y) { return xpp_ui.get_mouse_xy(x, y); }
int my_abort(void) { return xpp_ui.check_abort(); }
int get_command_width(void) { return xpp_ui.progress_begin(); }
void plot_command(int nit, int icount, int cwidth) { xpp_ui.progress(nit, icount, cwidth); }
void FlushDisplay(void) { xpp_ui.flush(); }
void redraw_params(void) { xpp_ui.redraw_params(); }
void redraw_ics(void) { xpp_ui.redraw_ics(); }
void redraw_all(void) { xpp_ui.redraw_all(); }
void redraw_bcs(void) { xpp_ui.redraw_bcs(); }
void redraw_delays(void) { xpp_ui.redraw_delays(); }
void drw_all_scrns(void) { xpp_ui.redraw_screens(); }
void clr_all_scrns(void) { xpp_ui.clear_screens(); }
void clear_draw_window(void) { xpp_ui.clear_draw_window(); }
void SmallBase(void) { xpp_ui.small_base(); }
void SmallGr(void) { xpp_ui.small_gr(); }
void reset_film(void) { xpp_ui.reset_film(); }
void on_the_fly(int task) { xpp_ui.on_the_fly(task); }
void set_color(int col) { xpp_ui.set_color(col); }
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n)
{
    xpp_ui.show_eq_box(cp, cm, rp, rm, im, y, ev, n);
}
void bye_bye(void) { xpp_ui.exit_program(); }

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
