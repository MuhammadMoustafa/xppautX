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
#include "auto_x11.h"
#include "arrayplot.h"
#include "menudrive.h"
#include "menus.h"
#include "graf_par.h"

extern Window draw_win, main_win, info_pop;
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
int x11_TwoChoice(char *c1, char *c2, char *q, char *key, char *title);
int x11_menu_choose(const XppMenu *m, int def);
int x11_do_edit_box(int n, char *title, char **names, char **values);
extern BoxList ParamBox, ICBox;
static void x11_param_box_set(int i, char *s) { set_edit_params(&ParamBox, i, s); }
static void x11_ic_box_set(int i, char *s) { set_edit_params(&ICBox, i, s); }
static void x11_ic_box_redraw(int i) { draw_one_box(ICBox, i); }
static void x11_param_box_redraw(int i)
{
    draw_one_box(ParamBox, i);
    reset_sliders();
}
void x11_respond_box(char *button, char *message);
int x11_checklist(char *title, char **names, int *flags, int n);
void x11_show_menu(int j);
int x11_do_string_box(int n, int row, int col, char *title, char **names,
                      char values[][MAX_LEN_SBOX], int maxchar);
int x11_file_selector(char *title, char *file, char *wild);
int x11_get_dialog(char *wname, char *name, char *value, char *ok, char *cancel, int max);
int x11_GetMouseXY(int *x, int *y);
void x11_flash(int num);
int x11_my_abort(void);
int x11_get_command_width(void);
void x11_plot_command(int nit, int icount, int cwidth);
void x11_FlushDisplay(void);
void x11_redraw_params(void);
void x11_redraw_ics(void);
void x11_redraw_all(void);
void x11_redraw_bcs(void);
void x11_redraw_delays(void);
void x11_redraw_the_graph(void);
void x11_draw_help(void);
void x11_scroll_window(void);
void x11_NewColormap(int type);
void x11_make_my_aplot(char *name);
static void x11_aplot_redraw(void) { redraw_aplot(aplot); }
static void x11_aplot_reset_axes(void) { reset_aplot_axes(aplot); }
void x11_new_vcr(void);
int rubber(int *x1, int *y1, int *x2, int *y2, Window w, int f);

static int x11_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    return rubber(i1, j1, i2, j2, draw_win, flag);
}
void x11_drw_all_scrns(void);
void x11_clr_all_scrns(void);
void x11_clear_draw_window(void);
void x11_reset_graphics(void);
void x11_create_a_pop(void);
void x11_destroy_a_pop(void);
void x11_kill_all_pops(void);
void x11_GrCol(void);
void x11_BaseCol(void);
void x11_cput_text(void);
extern Display *display;
static void x11_lower_plot_window(void) { XLowerWindow(display, draw_win); }
void x11_get_draw_size(unsigned int *w, unsigned int *h);
void x11_SmallBase(void);
void x11_SmallGr(void);
void x11_reset_film(void);
void x11_ani_clear(void);
void x11_ani_show(void);
void x11_ani_color(int icol);
void x11_ani_thick(int t);
void x11_ani_font(int size, int font, int color);
void x11_ani_line(int x1, int y1, int x2, int y2);
void x11_ani_rect(int x, int y, int w, int h, int fill);
void x11_ani_arc(int x, int y, int w, int h, int fill);
void x11_ani_text(int x, int y, char *s);
void redraw_ani_slider(void);
void x11_set_color(int col);
void x11_draw_one_array_plot(char *bob);
void x11_make_auto(char *wname, char *iname);
void x11_ALINE(int a, int b, int c, int d);
void x11_ATEXT(int a, int b, char *c);
void x11_Circle(int x, int y, int r);
void x11_FillCircle(int x, int y, int r);
void x11_XORCross(int x, int y);
void x11_LineWidth(int wid);
void x11_autocol(int col);
void x11_autobw(void);
void x11_clr_stab(void);
void x11_auto_stab_line(int x, int y, int xp, int yp);
void x11_clear_auto_plot(void);
void x11_redraw_auto_menus(void);
void x11_clear_auto_info(void);
void x11_draw_auto_info(char *bob, int x, int y);
void x11_refreshdisplay(void);
int x11_byeauto_(int *iflag);
int x11_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag);
int x11_auto_pop_up_list(char *title, char **list, char *key, int n, int max,
                         int def, int x, int y, char **hints, char *httxt);
void x11_auto_scroll_window(void);
int x11_auto_grab_event(int *x, int *y);
void x11_auto_show_hint(void);
void x11_auto_grab_end(int done);
/* X11 draws the diagram from its primitives only */
static void x11_auto_diagram(const XppDiagPoint *p) { (void)p; }
void x11_init_txtview(void);
void x11_create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                       double *ev, int n);
void x11_bye_bye(void);
void x11_make_txtview(void);
void x11_q_calc(void);

static void x11_data_changed(int length)
{
    (void)length;
    if (Xup && my_browser.xflag == 1) draw_data(my_browser);
}

static void x11_browser_redraw(int full)
{
    if (full) redraw_browser(my_browser);
    else draw_data(my_browser);
}

static void x11_activate_graph(int i, int flag)
{
    (void)i;
    draw_win = MyGraph->w;
    get_draw_area_flag(flag);
}

static void x11_draw_freeze(void) { draw_freeze(draw_win); }
static void x11_blank_draw_window(void) { blank_screen(draw_win); }

static const XppUi x11_ui = {
    .err_msg = x11_err_msg,
    .ping = x11_ping,
    .bottom_msg = x11_bottom_msg,
    .message_box = x11_MessageBox,
    .kill_message_box = x11_KillMessageBox,
    .title_text = x11_title_text,
    .canvas_xy = x11_canvas_xy,
    .new_string = x11_new_string,
    .yes_no_box = x11_yes_no_box,
    .two_choice = x11_TwoChoice,
    .edit_box = x11_do_edit_box,
    .param_box_set = x11_param_box_set,
    .param_box_redraw = x11_param_box_redraw,
    .ic_box_set = x11_ic_box_set,
    .ic_box_redraw = x11_ic_box_redraw,
    .respond_box = x11_respond_box,
    .checklist = x11_checklist,
    .string_box = x11_do_string_box,
    .file_selector = x11_file_selector,
    .dialog = x11_get_dialog,
    .get_mouse_xy = x11_GetMouseXY,
    .menu_flash = x11_flash,
    .show_menu = x11_show_menu,
    .menu_choose = x11_menu_choose,
    .check_abort = x11_my_abort,
    .progress_begin = x11_get_command_width,
    .progress = x11_plot_command,
    .flush = x11_FlushDisplay,
    .redraw_params = x11_redraw_params,
    .redraw_ics = x11_redraw_ics,
    .redraw_all = x11_redraw_all,
    .redraw_bcs = x11_redraw_bcs,
    .redraw_delays = x11_redraw_delays,
    .redraw_graph = x11_redraw_the_graph,
    .redraw_screens = x11_drw_all_scrns,
    .clear_screens = x11_clr_all_scrns,
    .clear_draw_window = x11_clear_draw_window,
    .reset_graphics = x11_reset_graphics,
    .data_changed = x11_data_changed,
    .browser_redraw = x11_browser_redraw,
    .activate_graph = x11_activate_graph,
    .create_plot_window = x11_create_a_pop,
    .destroy_plot_window = x11_destroy_a_pop,
    .kill_plot_windows = x11_kill_all_pops,
    .lower_plot_window = x11_lower_plot_window,
    .gr_col = x11_GrCol,
    .base_col = x11_BaseCol,
    .cput_text = x11_cput_text,
    .get_draw_size = x11_get_draw_size,
    .draw_freeze = x11_draw_freeze,
    .blank_draw_window = x11_blank_draw_window,
    .put_text = put_text_x11,
    .small_base = x11_SmallBase,
    .small_gr = x11_SmallGr,
    .film_clip = film_clip,
    .reset_film = x11_reset_film,
    .movie_play_back = play_back,
    .movie_auto_play = auto_play,
    .movie_save = save_movie,
    .movie_make_anigif = make_anigif,
    .draw_point = point_x11,
    .draw_line = line_x11,
    .draw_bead = bead_x11,
    .draw_frect = rect_x11,
    .draw_text = put_text_x11,
    .draw_special_text = special_put_text_x11,
    .draw_linestyle = set_line_style_x11,
    .set_color = x11_set_color,
    .aplot_draw_one = x11_draw_one_array_plot,
    .auto_make_window = x11_make_auto,
    .auto_line = x11_ALINE,
    .auto_text = x11_ATEXT,
    .auto_circle = x11_Circle,
    .auto_fill_circle = x11_FillCircle,
    .auto_xor_cross = x11_XORCross,
    .auto_line_width = x11_LineWidth,
    .auto_col = x11_autocol,
    .auto_bw = x11_autobw,
    .auto_clr_stab = x11_clr_stab,
    .auto_stab_line = x11_auto_stab_line,
    .auto_clear_plot = x11_clear_auto_plot,
    .auto_redraw_menus = x11_redraw_auto_menus,
    .auto_clear_info = x11_clear_auto_info,
    .auto_draw_info = x11_draw_auto_info,
    .auto_refresh = x11_refreshdisplay,
    .auto_check_abort = x11_byeauto_,
    .auto_rubber = x11_auto_rubber,
    .auto_choose_key = x11_auto_pop_up_list,
    .auto_scroll_window = x11_auto_scroll_window,
    .auto_grab_event = x11_auto_grab_event,
    .auto_show_hint = x11_auto_show_hint,
    .auto_grab_end = x11_auto_grab_end,
    .auto_diagram = x11_auto_diagram,
    .init_txtview = x11_init_txtview,
    .show_eq_box = x11_create_eq_box,
    .redraw_menu = x11_draw_help,
    .rubber_band = x11_rubber_band,
    .scroll_window = x11_scroll_window,
    .new_colormap = x11_NewColormap,
    .aplot_make = x11_make_my_aplot,
    .aplot_redraw = x11_aplot_redraw,
    .aplot_reset_axes = x11_aplot_reset_axes,
    .new_vcr = x11_new_vcr,
    .ani_clear = x11_ani_clear,
    .ani_show = x11_ani_show,
    .ani_color = x11_ani_color,
    .ani_thick = x11_ani_thick,
    .ani_font = x11_ani_font,
    .ani_line = x11_ani_line,
    .ani_rect = x11_ani_rect,
    .ani_arc = x11_ani_arc,
    .ani_text = x11_ani_text,
    .ani_slider = redraw_ani_slider,
    .make_txtview = x11_make_txtview,
    .q_calc = x11_q_calc,
    .exit_program = x11_bye_bye,
};

void xpp_install_x11_ui(void) { xpp_set_ui(&x11_ui); }
