/* The plot windows (create, select, destroy, redraw, the view), the
   pictures only the client has (pixels, for frame and GIF writers), the
   kinescope, whose frames the client keeps, and the array plot window. */
#include "model.h"
#include "session.h"
#include "ui_json_internal.h"
#include "xpp_util.h"
#include "graphics.h"
#include "graf_par.h"
#include "integrate.h"
#include "nullcline.h"
#include "browse.h"
#include "colormap.h"
#include "axes2.h"
#include "scrngif.h"
#include "arrayplot.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "series_enc.h"
#include <array>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include "kinescope.h"
#include "load_eqn.h"

/* the core's own globals and functions that have no header of their own */
extern "C" {
}

namespace xpp::json {

namespace {

int win_w[MAXPOP], win_h[MAXPOP];

} // namespace

/* ---- plot windows ------------------------------------------------------------ */

namespace {

int graph_of(unsigned long w)
{
    int i;
    for (i = 0; i < MAXPOP; i++)
        if (xpp::session().plot_windows.graph[i].Use && xpp::session().plot_windows.graph[i].w == w) return i;
    return 0;
}

} // namespace

void windows_init(void)
{
    int i;
    for (i = 0; i < MAXPOP; i++) {
        win_w[i] = 640;
        win_h[i] = 480;
    }
}

/* the main plot window, for hello */
void send_main_window(const char *title) { send_window("create", 1, win_w[0], win_h[0], title); }

void j_get_draw_size(unsigned int *w, unsigned int *h)
{
    int i = graph_of(xpp::session().plot_windows.draw_win);
    *w = win_w[i];
    *h = win_h[i];
}

/* a blanked plot window no longer shows its nullclines, direction field
   and flows (phase_data.h), nor its marks (marks_data.h), until they are
   drawn again */
void j_blank_draw_window(void)
{
    int i;
    for (i = 0; i < MAXPOP; i++)
        if (xpp::session().plot_windows.graph[i].Use && xpp::session().plot_windows.graph[i].w == xpp::session().plot_windows.draw_win) {
            phase_data_cleared(i);
            marks_data_cleared(i);
        }
}

void j_redraw_all(void)
{
    redraw_dfield();
    restore(0, xpp::session().browser.view.maxrow);
    draw_label(xpp::session().plot_windows.draw_win);
    draw_freeze(xpp::session().plot_windows.draw_win);
}

void j_redraw_graph(void)
{
    j_blank_draw_window();
    set_normal_scale();
    do_axes();
    restore(0, xpp::session().browser.view.maxrow);
    draw_label(xpp::session().plot_windows.draw_win);
    draw_freeze(xpp::session().plot_windows.draw_win);
    redraw_dfield();
    if (xpp::session().plot_windows.current->Nullrestore) restore_nullclines();
}

void j_redraw_screens(void) { for_each_shown_window(1, j_redraw_all); }

void j_clear_screens(void) { for_each_shown_window(1, clr_scrn); }

void j_reset_graphics(void)
{
    j_blank_draw_window();
    do_axes();
}

void send_window(const char *what, unsigned long id, int w, int h, const char *title)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"window\",\"op\":\"{}\",\"win\":{:d},\"w\":{:d},\"h\":{:d},\"title\":",
               what, id, w, h);
    buf_str(&b, title ? title : "");
    BUF_LIT(&b, "}");
    send_buf(&b);
}

void select_graph(int i)
{
    xpp::session().plot_windows.active = i;
    xpp::session().plot_windows.current = &xpp::session().plot_windows.graph[i];
    xpp::session().plot_windows.draw_win = xpp::session().plot_windows.graph[i].w;
    get_draw_area();
    send_window("select", xpp::session().plot_windows.draw_win, win_w[i], win_h[i], NULL);
}

void j_activate_graph(int i, int flag)
{
    xpp::session().plot_windows.draw_win = xpp::session().plot_windows.graph[i].w;
    get_draw_area_flag(flag);
}

void j_create_plot_window(void)
{
    int i;
    for (i = 1; i < MAXPOP; i++)
        if (xpp::session().plot_windows.graph[i].Use == 0) break;
    if (i >= MAXPOP) {
        j_respond_box("Okay", "Too many windows!");
        return;
    }
    copy_graph(i, xpp::session().plot_windows.active);
    xpp::session().plot_windows.graph[i].w = i + 1;
    win_w[i] = 450;
    win_h[i] = 350;
    xpp::session().plot_windows.count++;
    send_window("create", xpp::session().plot_windows.graph[i].w, win_w[i], win_h[i], "");
    select_graph(i);
}

namespace {

void destroy_graph(int i)
{
    xpp::session().plot_windows.graph[i].Use = 0;
    destroy_labels_and_grobs(xpp::session().plot_windows.graph[i].w);
    send_window("destroy", xpp::session().plot_windows.graph[i].w, 0, 0, NULL);
    xpp::session().plot_windows.count--;
}

} // namespace

void j_destroy_plot_window(void)
{
    int i;
    if (xpp::session().plot_windows.draw_win == xpp::session().plot_windows.graph[0].w) {
        j_respond_box("Okay", "Can't destroy big window!");
        return;
    }
    i = graph_of(xpp::session().plot_windows.draw_win);
    if (i == 0) return;
    select_graph(0);
    destroy_graph(i);
}

void j_kill_plot_windows(void)
{
    int i;
    select_graph(0);
    for (i = 1; i < MAXPOP; i++)
        if (xpp::session().plot_windows.graph[i].Use) destroy_graph(i);
    xpp::session().plot_windows.count = 1;
}

void j_cput_text(void)
{
    std::string string;
    int x, y, size = 2;
    if (new_string("Text: ", string) == 0) return;
    if (string[0] == '%') string = fill_in_text(std::string_view(string).substr(1));
    new_int("Size 0-4 :", &size);
    if (size > 4) size = 4;
    if (size < 0) size = 0;
    j_message_box("Place text with mouse");
    if (j_get_mouse_xy(&x, &y)) {
        const std::string text = fill_in_text(string);
        marks_data_label(xpp::session().plot_windows.draw_win, add_label(string.c_str(), x, y, size, 0), text.c_str());
    }
    j_kill_message_box();
}

void j_draw_freeze(void) { draw_freeze(xpp::session().plot_windows.draw_win); }

/* {"cmd":"click","win":w}: the user clicked in plot window w */
void click_command(const char *line)
{
    int win = get_int(line, "win", 1) - 1;
    if (win >= 0 && win < MAXPOP && xpp::session().plot_windows.graph[win].Use && xpp::session().plot_windows.active != win) select_graph(win);
}

namespace {

/* the plot window a command names ("win", 1-based): its index, or -1
   after telling the client it does not exist */
int command_window(const char *line)
{
    int i = get_int(line, "win", -1) - 1;
    if (i < 0 || i >= MAXPOP || !xpp::session().plot_windows.graph[i].Use) {
        j_err_msg("No such window");
        return -1;
    }
    return i;
}

} // namespace

/* "Use this view" (docs/ui-v2.md T9): {"cmd":"view","win":w,"xlo":..,
   "xhi":..,"ylo":..,"yhi":..} sets window w's axes exactly as
   Window/Window (graf_par.c user_window, here update_view()) would: the
   client's zoom becomes the core's own, so a PostScript/SVG export,
   Restore and later redraws all agree with it. A range that is not
   finite or not increasing, or a window that does not exist, is refused
   (message error) and changes nothing. */
void view_command(const char *line)
{
    int i = command_window(line);
    double xlo = get_num(line, "xlo", 0), xhi = get_num(line, "xhi", 0);
    double ylo = get_num(line, "ylo", 0), yhi = get_num(line, "yhi", 0);
    if (i < 0) return;
    if (!isfinite(xlo) || !isfinite(xhi) || !isfinite(ylo) || !isfinite(yhi) || xlo >= xhi || ylo >= yhi) {
        j_err_msg("Bad view");
        return;
    }
    if (i != xpp::session().plot_windows.active) select_graph(i);
    update_view(static_cast<float>(xlo), static_cast<float>(xhi), static_cast<float>(ylo), static_cast<float>(yhi));
}

/* dragging a 3D plot turns it (many_pops.c rotate3dcheck):
   {"cmd":"rotate","what":"down|move|up","x","y"} */
void rotate_command(const char *line)
{
    static int x0, y0;
    static double theta, phi;
    std::string what;
    int x = get_int(line, "x", 0), y = get_int(line, "y", 0);
    if (!xpp::session().plot_windows.current->ThreeDFlag) return;
    get_string(line, "what", what, 8);
    if (what == "down") {
        x0 = x;
        y0 = y;
        phi = xpp::session().plot_windows.current->Phi;
        theta = xpp::session().plot_windows.current->Theta;
    } else if (what == "move") {
        xpp::session().plot_windows.current->Phi = phi - static_cast<double>(y - y0);
        xpp::session().plot_windows.current->Theta = theta - static_cast<double>(x - x0);
        redraw_cube_pt(xpp::session().plot_windows.current->Theta, xpp::session().plot_windows.current->Phi);
    } else if (what == "up") {
        do_axes();
        j_redraw_all();
    }
}

/* a web2 client turns a 3D plot itself (projecting the box with its own
   angles, docs/ui-v2.md T14) and reports where it settled, so the core's
   own state agrees for a PostScript/SVG export, Restore, and any other
   client: {"cmd":"view3d","win":w,"theta":..,"phi":..} sets window w's
   angles exactly, redraws it, and sends state and idle as usual. Simpler
   than replaying `rotate`'s pixel deltas, which only make sense relative
   to a drag the core itself is tracking. A window that is not a 3D plot,
   does not exist, or an angle that is not finite, is refused (message
   error) and changes nothing. */
void view3d_command(const char *line)
{
    int i = command_window(line);
    double theta = get_num(line, "theta", 0), phi = get_num(line, "phi", 0);
    if (i < 0) return;
    if (!xpp::session().plot_windows.graph[i].ThreeDFlag) {
        j_err_msg("Not a 3D window");
        return;
    }
    if (!isfinite(theta) || !isfinite(phi)) {
        j_err_msg("Bad view");
        return;
    }
    if (i != xpp::session().plot_windows.active) select_graph(i);
    xpp::session().plot_windows.current->Theta = theta;
    xpp::session().plot_windows.current->Phi = phi;
    do_axes();
    j_redraw_all();
}

/* Window/zoom Scroll: drag the plot (rubber.c x11_scroll_window) */
void j_scroll_window(void)
{
    int i, j, t, state = 0;
    float x, y, x0 = 0, y0 = 0, dx = 0, dy = 0;
    float xlo = xpp::session().plot_windows.current->xlo, ylo = xpp::session().plot_windows.current->ylo, xhi = xpp::session().plot_windows.current->xhi, yhi = xpp::session().plot_windows.current->yhi;
    send_simple("message", "box", "Drag the plot to scroll it; any key ends");
    while ((t = ask_drag(xpp::session().plot_windows.draw_win, &i, &j)) != 0) {
        if (t == 1 && state == 0) {
            scale_to_real(i, j, &x0, &y0);
            state = 1;
        } else if (t == 2 && state == 1) {
            scale_to_real(i, j, &x, &y);
            dx = -(x - x0) / 2;
            dy = -(y - y0) / 2;
            update_view(xlo + dx, xhi + dx, ylo + dy, yhi + dy);
        } else if (t == 3) {
            state = 0;
            xlo += dx;
            xhi += dx;
            ylo += dy;
            yhi += dy;
            dx = dy = 0;
        }
        json_flush();
    }
    j_kill_message_box();
}

void j_new_colormap(int type)
{
    custom_color = type;
    xpp_build_colormap();
}

/* ---- pixels -------------------------------------------------------------------
   Only the client has the rendered picture: it draws the window from the
   data events. Frame and GIF writers ask for it: {"kind":"pixels","win":W}
   or {"film":i} (a kinescope frame), answered with w, h and base64 RGB. */

/* w*h*3 RGB bytes, or none when cancelled */
std::vector<unsigned char> ask_pixels(int win, int film, int *w, int *h)
{
    Buf b;
    std::vector<unsigned char> rgb;
    size_t k = 0;
    int q[4], nq = 0, id = ask_begin(&b, "pixels");
    if (film >= 0) buf_format(&b, ",\"film\":{:d}", film);
    else buf_format(&b, ",\"win\":{:d}", win);
    if (!ask_wait(&b, id)) return rgb;
    *w = get_int(ask_answer(), "w", 0);
    *h = get_int(ask_answer(), "h", 0);
    const char *v = js_find(ask_answer(), "rgb");
    if (!v || *v != '"' || *w <= 0 || *h <= 0 || *w > 8192 || *h > 8192) return rgb;
    size_t n = static_cast<size_t>(*w) * static_cast<size_t>(*h) * 3;
    try {
        rgb.resize(n);
    } catch (...) {
        xpp_out_of_memory("taking a picture");
    }
    for (v++; *v && *v != '"' && k < n; v++) {
        int d = xpp::base64_value(static_cast<unsigned char>(*v));
        if (d < 0) continue;
        q[nq++] = d;
        if (nq == 4) {
            rgb[k++] = static_cast<unsigned char>(q[0] << 2 | q[1] >> 4);
            if (k < n) rgb[k++] = static_cast<unsigned char>(q[1] << 4 | q[2] >> 2);
            if (k < n) rgb[k++] = static_cast<unsigned char>(q[2] << 6 | q[3]);
            nq = 0;
        }
    }
    if (nq >= 2 && k < n) rgb[k++] = static_cast<unsigned char>(q[0] << 2 | q[1] >> 4);
    if (nq >= 3 && k < n) rgb[k++] = static_cast<unsigned char>(q[1] << 4 | q[2] >> 2);
    return rgb;
}

int write_ppm(const char *file, std::span<const unsigned char> rgb, int w, int h)
{
    xpp::Writer out = xpp::Writer::binary(file);
    if (!out) return 0;
    out.print("P6\n{} {}\n255\n", w, h);
    fwrite(rgb.data(), 3, static_cast<size_t>(w) * h, out.file());
    return out.commit();
}

/* the GIF writer takes at most 256 colours; a canvas smooths its lines */
void web_safe_colors(std::span<unsigned char> rgb)
{
    for (unsigned char &c : rgb) c = static_cast<unsigned char>(((c + 25) / 51) * 51);
}

namespace {

void write_gif(const char *file, std::vector<unsigned char> &rgb, int w, int h)
{
    xpp::Writer out = xpp::Writer::binary(file);
    if (!out) return;
    web_safe_colors(rgb);
    gif_stuff_ppm(rgb.data(), w, h, out.file(), MAKE_ONE_GIF);
    out.commit();
}

} // namespace

/* ---- kinescope: the client keeps the frames ------------------------------------ */

namespace {

#define MAXFILM 250 /* kinescope.c */

void send_film(const char *what)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"film\",\"op\":\"{}\",\"count\":{:d},\"win\":{:d},\"cycles\":{:d},\"delay\":{:d}}}",
               what, xpp::session().kinescope.frames, xpp::session().plot_windows.draw_win, xpp::session().kinescope.cycles, xpp::session().kinescope.frame_ms);
    send_buf(&b);
}

} // namespace

int j_film_clip(void)
{
    if (xpp::session().kinescope.frames >= MAXFILM) return 0;
    xpp::session().kinescope.frames++;
    send_film("capture");
    return 1;
}

void j_reset_film(void)
{
    xpp::session().kinescope.frames = 0;
    send_film("reset");
}

void j_movie_play_back(void)
{
    if (xpp::session().kinescope.frames) send_film("play");
}

void j_movie_auto_play(void)
{
    if (xpp::session().kinescope.frames) send_film("autoplay");
}

void j_movie_save(const char *basename, int fmat)
{
    int w, h;
    for (int i = 0; i < xpp::session().kinescope.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) return;
        std::string file = xpp::format("{}_{}.{}", basename, i, fmat == 1 ? "ppm" : "gif");
        if (fmat == 1) write_ppm(file.c_str(), rgb, w, h);
        else write_gif(file.c_str(), rgb, w, h);
    }
}

void j_movie_make_anigif(void)
{
    int w, h, w0 = 0, h0 = 0;
    if (xpp::session().kinescope.frames == 0) return;
    xpp::Writer out = xpp::Writer::binary("anim.gif");
    if (!out) return;
    set_global_map(1);
    for (int i = 0; i < xpp::session().kinescope.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) break;
        if (i == 0) {
            w0 = w;
            h0 = h;
        } else if (w != w0 || h != h0) {
            j_err_msg("All clips must be same size");
            break;
        }
        web_safe_colors(rgb);
        gif_stuff_ppm(rgb.data(), w, h, out.file(), i == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
    }
    end_ani_gif(out.file());
    out.commit();
    set_global_map(0);
}

/* ---- array plot ------------------------------------------------------------------
   The picture is a grid of colour indices (aplotwin.c redraw_aplot).
   `values` (docs/ui-v2.md T12) carries the same cells' numbers before that
   mapping, so a client can pick its own colour scale from them and zmin/zmax;
   encoded like a series column (series_enc.h), base64 float32 when the
   client last asked for that (data_command's "enc":"f32", reused here via
   plot_data_want_f32 since aplot is not itself in the "data" subscription
   list -- it is sent whenever the window is alive and dirtied, as before). */

namespace {

#define FIRSTCOLOR 30 /* aplotwin.c */

int aplot_dirty; /* the data behind an array plot changed */

void send_aplot(const char *tag)
{
    Buf b;
    int num, i, j, nx, ny, nrows = xpp::session().browser.view.maxrow;
    double tlo = 0.0, thi = 20.0;
    APLOT *ap = &xpp::session().array_plot.plot;
    std::vector<float> vals;
    int f32 = plot_data_want_f32();
    aplot_dirty = 0;
    if (!ap->alive) return;
    const std::string sroot = get_root(ap->name, &num);
    BUF_LIT(&b, "{\"ev\":\"aplot\",\"title\":\"");
    buf_format(&b, "{:.60}{:d}..{:d}\"", sroot, num, num + ap->nacross - 1);
    nx = ap->ncskip > 0 ? ap->nacross / ap->ncskip : 0;
    ny = ap->ndown;
    if (nrows <= 2 || ap->plotdef == 0 || ap->nacross < 2 || ap->ndown < 2) nx = ny = 0;
    if (nx) {
        j = ap->nstart;
        if (j > 0 && j < nrows) tlo = xpp::session().browser.view.data[0][j];
        j = ap->nstart + ap->nskip * (ap->ndown - 1);
        if (j >= nrows) j = nrows - 1;
        if (j >= 0) thi = xpp::session().browser.view.data[0][j];
    }
    BUF_LIT(&b, ",\"tlo\":");
    buf_num(&b, tlo, 6);
    BUF_LIT(&b, ",\"thi\":");
    buf_num(&b, thi, 6);
    BUF_LIT(&b, ",\"zmin\":");
    buf_num(&b, ap->zmin, 6);
    BUF_LIT(&b, ",\"zmax\":");
    buf_num(&b, ap->zmax, 6);
    buf_format(&b, ",\"first\":{:d},\"ncolors\":{:d},\"nx\":{:d},\"ny\":{:d}", FIRSTCOLOR, color_table.count, nx, ny);
    if (tag) {
        BUF_LIT(&b, ",\"tag\":");
        buf_str(&b, tag);
    }
    try {
        if (nx * ny > 0) vals.resize(static_cast<size_t>(nx * ny));
    } catch (...) {
        xpp_out_of_memory("sending an array plot");
    }
    /* -1 (cells) / NaN (values): past the stored rows or columns (left blank) */
    BUF_LIT(&b, ",\"cells\":[");
    for (j = 0; j < ny; j++) {
        int jb = ap->nstart + ap->nskip * j;
        for (i = 0; i < nx; i++) {
            int ib = ap->index0 + i * ap->ncskip, c = -1;
            float v = NAN;
            if (ib < xpp::session().browser.view.maxcol && jb < nrows && jb >= 0) {
                double z = xpp::session().browser.view.data[ib][jb];
                v = static_cast<float>(z);
                if (ap->zmax > ap->zmin) {
                    c = static_cast<int>(color_table.count * (z - ap->zmin) / (ap->zmax - ap->zmin));
                    if (c < 0) c = 0;
                    if (c > color_table.count) c = color_table.count;
                }
            }
            if (!vals.empty()) vals[j * nx + i] = v;
            if (i || j) BUF_LIT(&b, ",");
            buf_format(&b, "{:d}", c);
        }
    }
    BUF_LIT(&b, "]");
    if (f32) BUF_LIT(&b, ",\"enc\":\"f32\"");
    BUF_LIT(&b, ",\"values\":");
    xpp_series_append(b.s, vals.empty() ? nullptr : vals.data(), nx * ny, f32);
    BUF_LIT(&b, "}");
    send_buf(&b);
}

} // namespace

void aplot_changed(void) { aplot_dirty = 1; }

/* an auto-redrawn array plot shows the data that changed */
void aplot_update(void)
{
    if (aplot_dirty && xpp::session().array_plot.plot.alive && xpp::session().array_plot.auto_redraw == 1) send_aplot(NULL);
    aplot_dirty = 0;
}

void j_aplot_make(const char *name)
{
    if (xpp::session().array_plot.plot.alive) return;
    xpp::session().array_plot.plot.alive = 1;
    xpp::session().array_plot.plot.plotw = xpp::session().array_plot.plot.width - 30 - 10 * text_metrics.small_width;
    xpp::session().array_plot.plot.ploth = xpp::session().array_plot.plot.height - 55;
    send_window("create", WIN_APLOT, xpp::session().array_plot.plot.plotw, xpp::session().array_plot.plot.ploth, name);
}

void j_aplot_redraw(void) { send_aplot(NULL); }

namespace {

/* write the picture the client shows as a GIF: one file, or a frame of
   the range movie (aplotwin.c gif_aplot_all) */
void aplot_gif(const char *file, int still)
{
    int w, h;
    xpp::Writer one; /* a still: one GIF, in place once whole */
    if (still == 1) {
        one = xpp::Writer::binary(file);
        if (!one) {
            j_err_msg("Cannot open file ");
            return;
        }
    } else if (xpp::session().array_plot.range_count == 0) {
        /* a range movie's frames all go into the first frame's file, a
           stream kept open until arrayplot.cpp's close_aplot_files */
        if ((xpp::session().array_plot.fp = xpp_files_open_stream(file, "wb")) == NULL) {
            j_err_msg("Cannot open file ");
            return;
        }
    }
    std::vector<unsigned char> rgb = ask_pixels(WIN_APLOT, -1, &w, &h);
    if (!rgb.empty()) {
        web_safe_colors(rgb);
        if (still == 1) gif_stuff_ppm(rgb.data(), w, h, one.file(), MAKE_ONE_GIF);
        else gif_stuff_ppm(rgb.data(), w, h, xpp::session().array_plot.fp, xpp::session().array_plot.range_count == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
    }
    one.commit();
}

} // namespace

void j_aplot_draw_one(const char *tag)
{
    send_aplot(xpp::session().array_plot.tag ? tag : NULL);
    aplot_gif(xpp::format("{}.{}.gif", xpp::session().array_plot.range_stem, xpp::session().array_plot.range_count).c_str(), xpp::session().array_plot.still);
    xpp::session().array_plot.range_count++;
}

/* the array plot window's buttons */
void aplot_command(const char *line)
{
    std::string o;
    get_string(line, "op", o, 16);
    if (!xpp::session().array_plot.plot.alive) return;
    if (o == "redraw") send_aplot(NULL);
    else if (o == "edit") {
        editaplot(&xpp::session().array_plot.plot);
        send_aplot(NULL);
    } else if (o == "fit") fit_aplot();
    else if (o == "range") set_up_aplot_range();
    else if (o == "print") print_aplot(&xpp::session().array_plot.plot);
    else if (o == "gif") {
        std::string file = xpp::format("{}.gif", xpp::model().this_file);
        if (file_selector("GIF plot", file, "*.gif")) aplot_gif(file.c_str(), 1);
    } else if (o == "scroll") {
        /* dragging the plot by dy pixels moves the first row, as in X11 */
        xpp::session().array_plot.plot.nstart -= get_int(line, "dy", 0);
        if (xpp::session().array_plot.plot.nstart < 0) xpp::session().array_plot.plot.nstart = 0;
        send_aplot(NULL);
    } else if (o == "close") {
        xpp::session().array_plot.plot.alive = 0;
        send_window("destroy", WIN_APLOT, 0, 0, NULL);
    }
}

} // namespace xpp::json
