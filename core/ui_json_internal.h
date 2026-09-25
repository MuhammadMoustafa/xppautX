/* What the files of the JSON protocol front end share (C++ only; the C API
   is ui_json.h). The front end is split by responsibility:

   ui_json.cpp       the XppUi table, the command dispatch (handle_line),
                     the input classifier, script replay, install and hello
   json_io.cpp       the output lines, the input lines, the JSON reader
   json_prompts.cpp  asks (prompts, dialogs, menus, mouse), long loops
   json_state.cpp    the state event, the data browser, parameter and IC
                     edits, the data subscription, equations, source
   json_windows.cpp  plot windows, pixels, the kinescope, the array plot
   json_auto.cpp     the AUTO window, its diagram data and settings
   json_ani.cpp      the animation window

   docs/protocol.md is the contract. */
#ifndef XPP_UI_JSON_INTERNAL_H
#define XPP_UI_JSON_INTERNAL_H

#ifndef __cplusplus
#error "ui_json_internal.h is C++ only"
#endif

#include "xpp_ui.h"
#include <stddef.h>

/* a name a client sends: one byte more than any name, so a longer one is
   cut to something no name equals */
#define NAME_IN (XPP_NAME_MAX + 2)

/* window ids the client draws into; plot windows are graph index + 1 */
#define WIN_AUTO 101
#define WIN_ANI 104
#define WIN_APLOT 105

/* control_line()'s answer for the animation's Pause */
#define ANI_PAUSE (-1)

/* a string literal into a Buf */
#define BUF_LIT(b, lit) xpp::json::buf_add(b, lit, sizeof(lit) - 1)

namespace xpp::json {

/* ---- ui_json.cpp ---- */

/* the session's mutable state that more than one file needs */
struct Session {
    /* --script FILE (docs/protocol.md "Scripts"): script_mode is set by
       json_ui_set_script(), script_error by an error message, which makes
       the process exit 1 at the end of the file */
    int script_mode, script_error;
};
extern Session session;

[[noreturn]] void quit_session(void); /* exit 1 after a script's error, else 0 */
int handle_async(const char *line);   /* commands that make sense at any moment */
int control_line(const char *line);   /* a control line taken by a checkpoint */
/* a script line that does not fit the dialogue: stop at once */
[[noreturn]] void script_fail(const char *what, const char *line, const char *ask);
void script_next(void); /* the script's next line, and an interruption after it */

/* ---- json_io.cpp: output ---- */

struct Buf {
    char *s;
    size_t len, cap;
};

void buf_add(Buf *b, const char *s, size_t n);
void buf_printf(Buf *b, const char *fmt, ...);
void buf_str(Buf *b, const char *s); /* a JSON string */
void buf_str_array(Buf *b, const char *const *v, int n);

void open_protocol_stdout(void); /* the protocol on the stdout of now */
void out_line(const char *s, size_t n);
void out_flush(void);
void flush_pending(void);
void send_buf(Buf *b);
void send_simple(const char *ev, const char *key, const char *text);
void json_flush(void);
void data_emit(const char *line, size_t n);

/* ---- json_io.cpp: input and the JSON reader ---- */

char *read_line(int which, int wait_ms);
unsigned long read_line_seq(void); /* the sequence number of read_line()'s line */

const char *skip_ws(const char *p);
const char *skip_value(const char *p);
const char *js_find(const char *obj, const char *key);
int js_string(const char *v, char *out, int max);
double js_num(const char *v, double def);
int js_number(const char *v, double *out);
const char *js_elem(const char *arr, int i);
int get_str(const char *obj, const char *key, char *out, int max);
double get_num(const char *obj, const char *key, double def);
int is_cmd(const char *line, const char *name);
int key_code(const char *k);

/* ---- json_prompts.cpp ---- */

int ask_begin(Buf *b, const char *kind);
int ask_wait(Buf *b, int id);
const char *ask_answer(void); /* the answer ask_wait() took */
void answer_point(unsigned long win, int k, int *x, int *y);
int mouse_ask(unsigned long win, const char *kind, int flag, int *v, int nv);
int ask_drag(unsigned long win, int *x, int *y);

void j_err_msg(const char *msg);
void j_ping(void);
void j_bottom_msg(int line, char *msg);
void j_message_box(const char *msg);
void j_kill_message_box(void);
void j_title_text(char *s);
void j_canvas_xy(char *s);
int j_dialog(const char *title, const char *name, char *value, const char *ok, const char *cancel, int max, int kind);
int j_new_string(char *name, char *value, int kind);
int j_yes_no_box(void);
int j_two_choice(char *c1, char *c2, char *q, char *key, char *title);
void j_respond_box(const char *button, const char *message);
int j_checklist(char *title, char **names, int *flags, int n);
int j_string_box(int n, int row, int col, char *title, char **names, char values[][MAX_LEN_SBOX], int maxchar,
                 const int *kinds);
int j_edit_box(int n, char *title, char **names, char **values);
int j_file_selector(char *title, char *file, char *wild);
int j_get_mouse_xy(int *x, int *y);
int j_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag);
int j_menu_choose(const struct XppMenu *m, int def);
void j_show_menu(int which);
void j_open_help(const char *chapter, const char *anchor);
int j_check_abort(void);
int j_progress_begin(void);
void j_progress(int nit, int icount, int cwidth);
void j_q_calc(void);

/* ---- json_state.cpp ---- */

void send_state(void);
void send_state_if_dirty(void);
void j_state_dirty(void);
void j_state_dirty_i(int i);
void j_state_dirty_is(int i, char *s);
void browser_rows(const char *line);
void browser_command(const char *line);
void browser_update(void); /* at a command's end */
void j_browser_changed(int i);
void j_rows_stored(int nrows);
void plotvars_command(const char *line);
void data_command(const char *line);
void send_equations(void);
void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
void eqimport_command(void);
void j_make_txtview(void);
void action_command(const char *line);
void apply_set(const char *line);
void default_command(const char *line);
void slide_command(const char *line);

/* ---- json_windows.cpp ---- */

void windows_init(void);
void send_main_window(const char *title);
void send_window(const char *what, unsigned long id, int w, int h, const char *title);
void select_graph(int i);
void click_command(const char *line);
void view_command(const char *line);
void rotate_command(const char *line);
void view3d_command(const char *line);
void j_get_draw_size(unsigned int *w, unsigned int *h);
void j_blank_draw_window(void);
void j_redraw_all(void);
void j_redraw_graph(void);
void j_redraw_screens(void);
void j_clear_screens(void);
void j_reset_graphics(void);
void j_activate_graph(int i, int flag);
void j_create_plot_window(void);
void j_destroy_plot_window(void);
void j_kill_plot_windows(void);
void j_cput_text(void);
void j_draw_freeze(void);
void j_scroll_window(void);
void j_new_colormap(int type);

unsigned char *ask_pixels(int win, int film, int *w, int *h);
int write_ppm(const char *file, unsigned char *rgb, int w, int h);
void web_safe_colors(unsigned char *rgb, int w, int h);

int j_film_clip(void);
void j_reset_film(void);
void j_movie_play_back(void);
void j_movie_auto_play(void);
void j_movie_save(char *basename, int fmat);
void j_movie_make_anigif(void);

void aplot_changed(void); /* the data behind an array plot changed */
void aplot_update(void);  /* at a command's end */
void aplot_command(const char *line);
void j_aplot_make(char *name);
void j_aplot_redraw(void);
void j_aplot_draw_one(char *tag);

/* ---- json_auto.cpp ---- */

void diag_flush(int final);
int diag_point_of_node(int node);
void auto_command(const char *line);
void auto_redraw_for_client(void);
int is_auto_set(const char *line);
void defer_auto_set(const char *line);
void apply_deferred_sets(void);
void j_auto_make_window(char *wname, char *iname);
int j_auto_check_abort(int *iflag);
int j_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag);
int j_auto_choose_key(char *title, char **list, char *key, int n, int max, int def, int x, int y, char **hints,
                      char *httxt);
int j_auto_grab_event(int *x, int *y);
void j_auto_show_hint(void);
void j_auto_scroll_window(void);
void j_auto_diagram(const XppDiagPoint *p);
void j_auto_refresh(void);

/* ---- json_ani.cpp ---- */

int ani_speed_op(const char *o, const char *line);
void ani_command(const char *line);
void j_ani_slider(void);
void j_new_vcr(void);
void j_ani_show(void);

} // namespace xpp::json

#endif
