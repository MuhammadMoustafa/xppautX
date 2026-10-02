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
#include "image_format.h"
#include <algorithm>
#include <array>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include "kinescope.h"
#include "menus.h"
#include "load_eqn.h"

/* the core's own globals and functions that have no header of their own */
namespace xpp::json {

namespace {

int win_w[MAXPOP], win_h[MAXPOP];

} // namespace

/* ---- plot windows ------------------------------------------------------------ */

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

void j_get_draw_size(xpp::Session &s, unsigned int *w, unsigned int *h)
{
    int i = graph_of(s, s.plot_windows.draw_win);
    *w = win_w[i];
    *h = win_h[i];
}

namespace {

/* a blanked plot window no longer shows its nullclines, direction field
   and flows (phase_data.h), nor its marks (marks_data.h), until they are
   drawn again */
void blank_draw_window(xpp::Session &s)
{
    int i;
    for (i = 0; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w == s.plot_windows.draw_win) {
            phase_data_cleared(s, i);
            marks_data_cleared(s, i);
        }
}

} // namespace

void j_blank_draw_window(xpp::Session &s) { blank_draw_window(s); }

void j_redraw_all(xpp::Session &s)
{
    redraw_dfield(s);
    restore(s, 0, s.browser.view.maxrow);
    draw_label(s, s.plot_windows.draw_win);
    draw_freeze(s,s.plot_windows.draw_win);
}

void redraw_graph(xpp::Session &s)
{
    blank_draw_window(s);
    set_normal_scale(s);
    do_axes(s);
    restore(s, 0, s.browser.view.maxrow);
    draw_label(s, s.plot_windows.draw_win);
    draw_freeze(s,s.plot_windows.draw_win);
    redraw_dfield(s);
    if (s.plot_windows.current->Nullrestore) restore_nullclines(s);
}

void j_redraw_graph(xpp::Session &s) { redraw_graph(s); }

void j_redraw_screens(xpp::Session &s) { for_each_shown_window(s, 1, [&s] { j_redraw_all(s); }); }

void j_clear_screens(xpp::Session &s)
{
    for_each_shown_window(s, 1, [&s] { clr_scrn(s); });
}

void j_reset_graphics(xpp::Session &s)
{
    blank_draw_window(s);
    do_axes(s);
}

void send_window(const char *what, unsigned long id, int w, int h, std::string_view title)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"window\",\"op\":\"{}\",\"win\":{:d},\"w\":{:d},\"h\":{:d},\"title\":",
               what, id, w, h);
    buf_str(&b, title);
    BUF_LIT(&b, "}");
    send_buf(&b);
}

void select_graph(xpp::Session &s, int i)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[i];
    s.plot_windows.draw_win = s.plot_windows.graph[i].w;
    get_draw_area(s);
    send_window("select", s.plot_windows.draw_win, win_w[i], win_h[i]);
}

void j_activate_graph(xpp::Session &s, int i, int flag)
{
    s.plot_windows.draw_win = s.plot_windows.graph[i].w;
    get_draw_area_flag(s,flag);
}

void j_create_plot_window(xpp::Session &s)
{
    int i;
    for (i = 1; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use == 0) break;
    if (i >= MAXPOP) {
        j_respond_box("Okay", "Too many windows!");
        return;
    }
    copy_graph(s,i, s.plot_windows.active);
    s.plot_windows.graph[i].w = i + 1;
    win_w[i] = 450;
    win_h[i] = 350;
    s.plot_windows.count++;
    send_window("create", s.plot_windows.graph[i].w, win_w[i], win_h[i], "");
    select_graph(s, i);
}

namespace {

void destroy_graph(xpp::Session &s, int i)
{
    s.plot_windows.graph[i].Use = 0;
    destroy_labels_and_grobs(s, s.plot_windows.graph[i].w);
    send_window("destroy", s.plot_windows.graph[i].w, 0, 0);
    s.plot_windows.count--;
}

} // namespace

void j_destroy_plot_window(xpp::Session &s)
{
    int i;
    if (s.plot_windows.draw_win == s.plot_windows.graph[0].w) {
        j_respond_box("Okay", "Can't destroy big window!");
        return;
    }
    i = graph_of(s, s.plot_windows.draw_win);
    if (i == 0) return;
    select_graph(s, 0);
    destroy_graph(s, i);
}

void j_kill_plot_windows(xpp::Session &s)
{
    int i;
    select_graph(s, 0);
    for (i = 1; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use) destroy_graph(s, i);
    s.plot_windows.count = 1;
}

void j_cput_text(xpp::Session &s)
{
    std::string string;
    int x, y, size = 2;
    if (new_string("Text: ", string) == 0) return;
    if (string[0] == '%') string = fill_in_text(s, std::string_view(string).substr(1));
    new_int("Size 0-4 :", &size);
    if (size > 4) size = 4;
    if (size < 0) size = 0;
    j_message_box("Place text with mouse");
    if (j_get_mouse_xy(s, &x, &y)) {
        const std::string text = fill_in_text(s, string);
        marks_data_label(s, s.plot_windows.draw_win, add_label(s, string, x, y, size, 0), text);
    }
    j_kill_message_box();
}

void j_draw_freeze(xpp::Session &s)
{
    draw_freeze(s, s.plot_windows.draw_win);
}

/* {"cmd":"click","win":w}: the user clicked in plot window w */
void click_command(xpp::Session &s, const char *line)
{
    int win = get_int(line, "win", 1) - 1;
    if (win >= 0 && win < MAXPOP && s.plot_windows.graph[win].Use && s.plot_windows.active != win) select_graph(s, win);
}

void display_command(xpp::Session &s, const char *line)
{
    const int win = get_int(line, "win", 1) - 1;
    if (win < 0 || win >= MAXPOP || !s.plot_windows.graph[win].Use) {
        j_command_error("display", "display: no such plot window");
        return;
    }
    xpp::PlotDisplay &d = s.plot_display[win];
    xpp::Zoom z = d.zoom;
    const int rx = get_range(line, "x", z.x), ry = get_range(line, "y", z.y);
    if (rx < 0 || ry < 0) {
        j_command_error("display", "display: x and y are [low, high] with low below high, or null");
        return;
    }
    d.zoom = z;
    const char *jr = js_find(line, "runs");
    if (jr) d.show_runs = js_num(jr, 1) != 0;
}

/* Window/zoom Scroll: drag the plot (rubber.c x11_scroll_window) */
void j_scroll_window(xpp::Session &s)
{
    int i, j, t, state = 0;
    float x, y, x0 = 0, y0 = 0, dx = 0, dy = 0;
    float xlo = s.plot_windows.current->xlo, ylo = s.plot_windows.current->ylo, xhi = s.plot_windows.current->xhi, yhi = s.plot_windows.current->yhi;
    send_simple("message", "box", "Drag the plot to scroll it; any key ends");
    while ((t = ask_drag(s, s.plot_windows.draw_win, &i, &j)) != 0) {
        if (t == 1 && state == 0) {
            scale_to_real(s,i, j, &x0, &y0);
            state = 1;
        } else if (t == 2 && state == 1) {
            scale_to_real(s,i, j, &x, &y);
            dx = -(x - x0) / 2;
            dy = -(y - y0) / 2;
            update_view(s,xlo + dx, xhi + dx, ylo + dy, yhi + dy);
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
    xpp_build_colormap(type);
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
    int id = ask_begin(&b, "pixels");
    if (film >= 0) buf_format(&b, ",\"film\":{:d}", film);
    else buf_format(&b, ",\"win\":{:d}", win);
    if (!ask_wait(&b, id)) return rgb;
    *w = get_int(ask_answer(), "w", 0);
    *h = get_int(ask_answer(), "h", 0);
    const char *v = js_find(ask_answer(), "rgb");
    if (!v || *v != '"' || *w <= 0 || *h <= 0 || *w > 8192 || *h > 8192) return rgb;
    size_t n = static_cast<size_t>(*w) * static_cast<size_t>(*h) * 3;
    try {
        std::string bytes;
        bytes.reserve(n + 2);
        xpp::Base64Decoder d(bytes);
        for (v++; *v && *v != '"' && bytes.size() < n; v++)
            if (!d.feed(*v)) break;
        d.finish();
        rgb.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(std::min(n, bytes.size())));
        rgb.resize(n);
    } catch (...) {
        xpp::out_of_memory("taking a picture");
    }
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

/* a whole picture as one GIF frame (MAKE_ONE_GIF): the kinescope's still
   export below and the array plot's aplot_gif used to write this by
   hand, twice (W53, issue #101) */
void write_gif_frame(FILE *out, std::vector<unsigned char> &rgb, int w, int h)
{
    web_safe_colors(rgb);
    gif_stuff_ppm(rgb.data(), w, h, out, MAKE_ONE_GIF);
}

void write_gif(const char *file, std::vector<unsigned char> &rgb, int w, int h)
{
    xpp::Writer out = xpp::Writer::binary(file);
    if (!out) return;
    write_gif_frame(out.file(), rgb, w, h);
    out.commit();
}

} // namespace

/* ---- kinescope: the client keeps the frames ------------------------------------ */

namespace {

#define MAXFILM 250 /* kinescope.c */

void send_film(const xpp::Session &s, const char *what)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"film\",\"op\":\"{}\",\"count\":{:d},\"win\":{:d},\"cycles\":{:d},\"delay\":{:d}}}",
               what, s.kinescope.frames, s.plot_windows.draw_win, s.kinescope.cycles, s.kinescope.frame_ms);
    send_buf(&b);
}

} // namespace

xpp::Result<> j_film_clip(xpp::Session &s)
{
    if (s.kinescope.frames >= MAXFILM)
        return xpp::fail("kinescope", "Out of film: the kinescope holds no more frames", xpp::command_place());
    s.kinescope.frames++;
    send_film(s, "capture");
    return {};
}

void j_reset_film(xpp::Session &s)
{
    s.kinescope.frames = 0;
    send_film(s, "reset");
}

namespace {

/* Playback (play) and Autoplay (autoplay) of the frames the client holds,
   when there are any */
void play_film(const xpp::Session &s, const char *how)
{
    if (s.kinescope.frames) send_film(s, how);
}

} // namespace

void j_movie_play_back(xpp::Session &s) { play_film(s, "play"); }

void j_movie_auto_play(xpp::Session &s) { play_film(s, "autoplay"); }

void j_movie_save(xpp::Session &s, std::string_view basename, int fmat)
{
    int w, h;
    for (int i = 0; i < s.kinescope.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) return;
        std::string file = xpp::format("{}_{}.{}", basename, i,
                                        fmat == 1 ? "ppm" : xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension);
        if (fmat == 1) write_ppm(file.c_str(), rgb, w, h);
        else write_gif(file.c_str(), rgb, w, h);
    }
}

void j_movie_make_anigif(xpp::Session &s)
{
    const XppKinescope &k = s.kinescope;
    int w, h, w0 = 0, h0 = 0;
    if (k.frames == 0) return;
    xpp::Writer out = xpp::Writer::binary(xpp::format("anim.{}", xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension).c_str());
    if (!out) return;
    for (int i = 0; i < k.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) break;
        if (i == 0) {
            w0 = w;
            h0 = h;
        } else if (w != w0 || h != h0) {
            j_command_error("kinescope", "All clips must be same size");
            break;
        }
        web_safe_colors(rgb);
        gif_stuff_ppm(rgb.data(), w, h, out.file(), i == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
    }
    end_ani_gif(out.file());
    out.commit();
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

void send_aplot(xpp::Session &s, const char *tag)
{
    Buf b;
    int num, i, j, nx, ny, nrows = s.browser.view.maxrow;
    double tlo = 0.0, thi = 20.0;
    APLOT *ap = &s.array_plot.plot;
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
        if (j > 0 && j < nrows) tlo = s.browser.view.data[0][j];
        j = ap->nstart + ap->nskip * (ap->ndown - 1);
        if (j >= nrows) j = nrows - 1;
        if (j >= 0) thi = s.browser.view.data[0][j];
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
        xpp::out_of_memory("sending an array plot");
    }
    /* -1 (cells) / NaN (values): past the stored rows or columns (left blank) */
    BUF_LIT(&b, ",\"cells\":[");
    for (j = 0; j < ny; j++) {
        int jb = ap->nstart + ap->nskip * j;
        for (i = 0; i < nx; i++) {
            int ib = ap->index0 + i * ap->ncskip, c = -1;
            float v = NAN;
            if (ib < s.browser.view.maxcol && jb < nrows && jb >= 0) {
                double z = s.browser.view.data[ib][jb];
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
void aplot_update(xpp::Session &s)
{
    if (aplot_dirty && s.array_plot.plot.alive && s.array_plot.auto_redraw == 1) send_aplot(s, NULL);
    aplot_dirty = 0;
}

void j_aplot_make(xpp::Session &s, std::string_view name)
{
    APLOT &ap = s.array_plot.plot;
    if (ap.alive) return;
    ap.alive = 1;
    ap.plotw = ap.width - 30 - 10 * text_metrics.small_width;
    ap.ploth = ap.height - 55;
    send_window("create", WIN_APLOT, ap.plotw, ap.ploth, name);
}

void j_aplot_redraw(xpp::Session &s) { send_aplot(s, NULL); }

namespace {

/* write the picture the client shows as a GIF: one file, or a frame of
   the range movie (aplotwin.c gif_aplot_all) */
void aplot_gif(ArrayPlotState &a, const char *file, int still)
{
    int w, h;
    xpp::Writer one; /* a still: one GIF, in place once whole */
    if (still == 1) {
        one = xpp::Writer::binary(file);
        if (!one) {
            j_command_error("aplot", xpp::format("Cannot write {}", file));
            return;
        }
    } else if (a.range_count == 0) {
        /* a range movie's frames all go into the first frame's file, a
           stream kept open until arrayplot.cpp's close_aplot_files */
        if ((a.fp = xpp::files::open_stream(file, "wb")) == NULL) {
            j_command_error("aplot", xpp::format("Cannot write {}", file));
            return;
        }
    }
    std::vector<unsigned char> rgb = ask_pixels(WIN_APLOT, -1, &w, &h);
    if (!rgb.empty()) {
        if (still == 1) write_gif_frame(one.file(), rgb, w, h);
        else {
            web_safe_colors(rgb);
            gif_stuff_ppm(rgb.data(), w, h, a.fp, a.range_count == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
        }
    }
    one.commit();
}

} // namespace

void j_aplot_draw_one(xpp::Session &s, std::string_view tag)
{
    const std::string shown(tag); /* send_aplot's tag: NUL-terminated, or none */
    send_aplot(s, s.array_plot.tag ? shown.c_str() : nullptr);
    aplot_gif(s.array_plot, xpp::format("{}.{}.{}", s.array_plot.range_stem, s.array_plot.range_count,
                          xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension).c_str(), s.array_plot.still);
    s.array_plot.range_count++;
}

/* {"cmd":"aplot","op":"scroll"|"close"}: what the array plot window's keys
   (menu_aplot_window) do not say */
void aplot_command(xpp::Session &s, const char *line)
{
    std::string o;
    get_string(line, "op", o, 16);
    if (o != "scroll" && o != "close") {
        j_command_error("aplot", xpp::format("Unknown aplot op {}", o));
        return;
    }
    if (!s.array_plot.plot.alive) return;
    if (o == "scroll") {
        /* dragging the plot by dy pixels moves the first row, as in X11 */
        s.array_plot.plot.nstart -= get_int(line, "dy", 0);
        if (s.array_plot.plot.nstart < 0) s.array_plot.plot.nstart = 0;
        send_aplot(s, NULL);
    } else if (o == "close") {
        s.array_plot.plot.alive = 0;
        send_window("destroy", WIN_APLOT, 0, 0);
    }
}

/* a key of the array plot window (menu_aplot_window) */
void aplot_key(xpp::Session &s, int ch)
{
    if (!s.array_plot.plot.alive) return;
    switch (menu_index(&menu_aplot_window, ch)) {
    case PK_REDRAW: send_aplot(s, NULL); break;
    case PK_EDIT:
        editaplot(s, &s.array_plot.plot);
        send_aplot(s, NULL);
        break;
    case PK_FIT: fit_aplot(s); break;
    case PK_RANGE: set_up_aplot_range(s); break;
    case PK_PRINT: print_aplot(s, &s.array_plot.plot); break;
    case PK_GIF: {
        const char *ext = xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension;
        std::string file = xpp::format("{}.{}", s.model().this_file, ext);
        if (file_selector("GIF plot", file, xpp::format("*.{}", ext))) aplot_gif(s.array_plot, file.c_str(), 1);
        break;
    }
    }
}

} // namespace xpp::json
