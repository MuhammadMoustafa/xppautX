/* The plot windows (create, select, destroy, redraw, the view), the
   pictures only the client has (pixels, for frame and GIF writers), the
   kinescope, whose frames the client keeps, and the array plot window. */
#include "xpp_files.h"
#include "model.h"
#include "session.h"
#include "ui_json_internal.h"
#include "xpp_util.h"
#include "many_pops.h"
#include "graphics.h"
#include "graf_par.h"
#include "integrate.h"
#include "nullcline.h"
#include "browse.h"
#include "colormap.h"
#include "axes2.h"
#include "arrayplot.h"
#include "command_registry.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "grobs.h"
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


/* ---- plot windows ------------------------------------------------------------ */

/* the main plot window, for hello */
void send_main_window(const xpp::Session &s,const char *title) { send_window(xpp::events::WINDOW_CREATED, s.plot_windows.graph[0].name, main_plot_width, main_plot_height, title); }

void j_get_draw_size(xpp::Session &s, unsigned int *w, unsigned int *h)
{
    int i = graph_of(s, s.plot_windows.draw_win);
    *w = s.plot_windows.graph[i].x11Wid;
    *h = s.plot_windows.graph[i].x11Hgt;
}

namespace {

/* a blanked plot window no longer shows its nullclines, direction field
   and flows (phase_data.h), nor its marks (marks_data.h), until they are
   drawn again */
void blank_draw_window(xpp::Session &s)
{
    const int i=graph_of(s,s.plot_windows.draw_win);
    if(s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w==s.plot_windows.draw_win){
        phase_data_cleared(s,i);
        marks_data_cleared(s,i);
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

void send_window(std::string_view event, std::string_view name, int w, int h, std::string_view title, std::string_view kind)
{
    std::string b;
    b=xpp::events::open(event);
    BUF_LIT(b,",\"win\":");
    buf_str(b,name);
    BUF_LIT(b,",\"kind\":");
    buf_str(b,kind);
    buf_format(b,",\"w\":{},\"h\":{},\"title\":",w,h);
    buf_str(b, title);
    BUF_LIT(b, "}");
    send_buf(b);
}

std::string windows_event_line(const xpp::Session &s) { return xpp::events::open(xpp::events::WINDOW_LIST) + ",\"windows\":" + xpp::windows_json(s) + '}'; }

void j_window_selected(xpp::Session &, std::string_view name, std::string_view kind) { send_window(xpp::events::WINDOW_SELECTED, name, 0, 0, "", kind); }

void j_window_renamed(xpp::Session &, std::string_view old_name, std::string_view new_name)
{
    std::string event=xpp::events::open(xpp::events::WINDOW_RENAMED)+",\"window\":";
    json_append_string(event,old_name);
    event+=",\"new_name\":";
    json_append_string(event,new_name);
    event+='}';
    send_buf(event);
}

void j_auto_closed(xpp::Session &s, std::string_view name)
{
    send_window(xpp::events::WINDOW_DESTROYED,name,0,0,"","diagram");
    diag_forget();
    auto_data_forget(s);
}

void j_tool_window_closed(xpp::Session &, std::string_view name, std::string_view kind) { send_window(xpp::events::WINDOW_DESTROYED, name, 0, 0, "", kind); }

void j_create_plot_window(xpp::Session &s)
{
    const int i = s.plot_windows.active;
    send_window(xpp::events::WINDOW_CREATED, xpp::window_name(s,s.plot_windows.graph[i].w), s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt, "");
    send_window(xpp::events::WINDOW_SELECTED, xpp::window_name(s,s.plot_windows.draw_win), s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt);
}

/* The core removes the window; this callback only tells the page. */
void j_destroy_plot_window(xpp::Session &s)
{
    send_window(xpp::events::WINDOW_DESTROYED,xpp::window_name(s,s.plot_windows.draw_win),0,0);
    const GRAPH &main=s.plot_windows.graph[0];
    send_window(xpp::events::WINDOW_SELECTED,xpp::window_name(s,main.w),main.x11Wid,main.x11Hgt);
}

void j_cput_text(xpp::Session &s)
{
    int x, y;
    j_message_box("Place text with mouse");
    const bool placed = j_get_mouse_xy(s, &x, &y);
    j_kill_message_box();
    xpp::LabelLook look;
    if (!placed || !j_text_dialog(look)) return;
    if (const std::string problem = xpp::label_problem(look); !problem.empty()) {
        j_command_error("text", problem);
        return;
    }
    float xd, yd;
    scale_to_real(s, x, y, &xd, &yd);
    if (add_label(s, look, s.plot_windows.draw_win, xd, yd) < 0) j_command_error("text", xpp::format("a session has at most {} texts", xpp::ANNOTATION_LIST_MAX));
}

/* Window/zoom Scroll: drag the plot (rubber.c x11_scroll_window) */
void j_scroll_window(xpp::Session &s)
{
    int i, j, t, state = 0;
    float x, y, x0 = 0, y0 = 0, dx = 0, dy = 0;
    float xlo = s.plot_windows.current->xlo, ylo = s.plot_windows.current->ylo, xhi = s.plot_windows.current->xhi, yhi = s.plot_windows.current->yhi;
    send_simple(xpp::events::SESSION_MESSAGE, "box", "Drag the plot to scroll it; any key ends");
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

/* ---- pixels -------------------------------------------------------------------
   Only the client has the rendered picture: it draws the window from the
   data events. Frame and GIF writers ask for it: {"kind":"pixels","win":W}
   or {"film":i} (a kinescope frame), answered with w, h and base64 RGB. */

/* w*h*3 RGB bytes, or none when cancelled */
std::vector<unsigned char> ask_pixels(int win, int film, int *w, int *h)
{
    std::string b;
    std::vector<unsigned char> rgb;
    int id = ask_begin(b, "pixels");
    if (film >= 0) buf_format(b, ",\"film\":{:d}", film);
    else {BUF_LIT(b,",\"win\":");buf_str(b,xpp::fixed_window_name(win));}
    if (!ask_wait(b, id)) return rgb;
    *w = get_int(ask_answer(), "w", 0);
    *h = get_int(ask_answer(), "h", 0);
    const char *v = js_find(ask_answer(), "rgb");
    // Bound a client-supplied picture's allocation to the existing canvas limit.
    constexpr int max_picture_dimension = 8192;
    if (!v || *v != '"' || *w <= 0 || *h <= 0 || *w > max_picture_dimension || *h > max_picture_dimension) return rgb;
    size_t n = static_cast<size_t>(*w) * static_cast<size_t>(*h) * 3;
    try {
        std::string bytes;
        bytes.reserve(n + 2);
        xpp::Base64Decoder d(bytes);
        for (v++; *v && *v != '"' && bytes.size() < n; v++)
            if (!d.feed(*v)) break;
        d.finish();
        if (bytes.size() != n || *v != '"') return rgb;
        rgb.assign(bytes.begin(), bytes.end());
    } catch (...) {
        xpp::out_of_memory("taking a picture");
    }
    return rgb;
}

/* ---- kinescope: the client keeps the frames ------------------------------------ */

namespace {

void send_film(const xpp::Session &s, const char *what)
{
    std::string b;
    buf_format(b,"{},\"op\":\"{}\",\"count\":{},\"win\":",xpp::events::open(xpp::events::KINESCOPE_FILM),what,s.kinescope.frames);
    buf_str(b,xpp::window_name(s,s.plot_windows.draw_win));
    buf_format(b,",\"cycles\":{},\"delay\":{}}}",s.kinescope.cycles,s.kinescope.frame_ms);
    send_buf(b);
}

} // namespace

xpp::Result<> j_film_clip(xpp::Session &s)
{
    if (s.kinescope.frames >= kinescope_max_frames)
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

xpp::Result<int> j_movie_save(xpp::Session &s, std::string_view basename, int fmat)
{
    const xpp::ImageFormat &format = xpp::image_formats[fmat];
    std::vector<xpp::Writer> outputs;
    for (int i = 0; i < s.kinescope.frames; i++) {
        const std::string file = i == 0 ? std::string(basename)
            : xpp::files::frame_name(basename, xpp::format(".{}", format.extension), i);
        if (i > 0)
            if (const auto decided = reset_save_decision(); !decided) return std::unexpected(decided.error());
        xpp::Result<> opened;
        auto output = xpp::open_writer_asking(file, true, &opened);
        if (!opened) return std::unexpected(opened.error());
        if (!output) return xpp::fail("kinescope", xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), xpp::command_place());
        outputs.push_back(std::move(output));
    }
    for (int i = 0; i < s.kinescope.frames; i++) {
        int w = 0, h = 0;
        const std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty())
            return xpp::fail("kinescope", xpp::format("The client gave no picture of frame {} of {}: nothing was written", i + 1, s.kinescope.frames), xpp::command_place());
        format.pixels(outputs[static_cast<std::size_t>(i)], rgb, w, h, xpp::IMAGE_STILL_FRAME);
    }
    for (auto &output : outputs)
        if (const auto saved = xpp::commit_save(output); !saved) return std::unexpected(saved.error());
    return s.kinescope.frames;
}

xpp::Result<> j_movie_make_anigif(xpp::Session &s, std::string_view file)
{
    const XppKinescope &k = s.kinescope;
    int w, h, w0 = 0, h0 = 0;
    xpp::Result<> opened;
    xpp::Writer out = xpp::open_writer_asking(file, true, &opened);
    if (!opened) return std::unexpected(opened.error());
    if (!out) return xpp::fail("kinescope", xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), xpp::command_place());
    for (int i = 0; i < k.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) {
            xpp::abort_save(out);
            return xpp::fail("kinescope", xpp::format("The client gave no picture of frame {} of {}: nothing was written", i + 1, k.frames), xpp::command_place());
        }
        if (i == 0) {
            w0 = w;
            h0 = h;
        } else if (w != w0 || h != h0) {
            xpp::abort_save(out);
            return xpp::fail("kinescope", xpp::format("All clips must be the same size: frame {} is {} by {}, the first {} by {}", i + 1, w, h, w0, h0), xpp::command_place());
        }
        xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels(out,rgb,w,h,i);
    }
    xpp::image_formats[xpp::IMAGE_FORMAT_GIF].finish_movie(out);
    return xpp::commit_save(out);
}

/* ---- array plot ------------------------------------------------------------------
   The array plot as data (docs/ui-v2.md T12): `values` is the cells' numbers (encoded like a series
   column, series_enc.h: base64 float32 when the client last asked for that, data_command's "enc":"f32",
   reused here via plot_data_want_f32 since aplot is not itself in the "data" subscription list: it is sent
   whenever the window is alive and dirtied), with zmin, zmax, the range of time shown and the colour scale
   the session has, as its name and its 256 colours: the page colours a cell by ramp_color's rule, a fraction
   of zmin to zmax into the ramp, and the print file does the same. */

namespace {

int aplot_dirty; /* the data behind an array plot changed */

void send_aplot(xpp::Session &s, const char *tag)
{
    std::string b;
    int num, i, j, nx, ny, nrows = s.browser.view.maxrow;
    APLOT *ap = &s.array_plot.plot;
    std::vector<float> vals;
    int f32 = plot_data_want_f32();
    aplot_dirty = 0;
    if (!ap->alive) return;
    const std::string sroot = get_root(ap->name, &num);
    b += xpp::events::open(xpp::events::ARRAY_PLOT_SHOWN);
    BUF_LIT(b, ",\"title\":\"");
    buf_format(b, "{:.60}{:d}..{:d}\"", sroot, num, num + ap->nacross - 1);
    nx = ap->ncskip > 0 ? ap->nacross / ap->ncskip : 0;
    ny = ap->ndown;
    if (nrows <= 2 || ap->plotdef == 0 || ap->nacross < 2 || ap->ndown < 2) nx = ny = 0;
    const AplotTimes times = nx ? aplot_times(s, *ap) : AplotTimes{0.0, 0.0};
    BUF_LIT(b, ",\"tlo\":");
    buf_num(b, times.first, 6);
    BUF_LIT(b, ",\"thi\":");
    buf_num(b, times.last, 6);
    BUF_LIT(b, ",\"zmin\":");
    buf_num(b, ap->zmin, 6);
    BUF_LIT(b, ",\"zmax\":");
    buf_num(b, ap->zmax, 6);
    buf_format(b, ",\"colormap\":\"{}\",\"ramp\":[", xpp::colormap_names[static_cast<std::size_t>(s.colormap)]);
    for (std::size_t k = 0; k < s.ramp().size(); k++) buf_format(b, "{}{}", k ? "," : "", xpp::quoted_color(s.ramp()[k]));
    buf_format(b, "],\"nx\":{:d},\"ny\":{:d},\"first_row\":{:d}", nx, ny, ap->nstart);
    if (tag) {
        BUF_LIT(b, ",\"tag\":");
        buf_str(b, tag);
    }
    try {
        if (nx * ny > 0) vals.resize(static_cast<size_t>(nx * ny));
    } catch (...) {
        xpp::out_of_memory("sending an array plot");
    }
    /* NaN: past the stored rows or columns (left blank) */
    for (j = 0; j < ny; j++) {
        int jb = ap->nstart + ap->nskip * j;
        for (i = 0; i < nx; i++) {
            int ib = ap->index0 + i * ap->ncskip;
            float v = NAN;
            if (ib < s.browser.view.maxcol && jb < nrows && jb >= 0) v = static_cast<float>(s.browser.view.data[ib][jb]);
            if (!vals.empty()) vals[j * nx + i] = v;
        }
    }
    if (f32) BUF_LIT(b, ",\"enc\":\"f32\"");
    BUF_LIT(b, ",\"values\":");
    xpp_series_append(b, vals.empty() ? nullptr : vals.data(), nx * ny, f32);
    BUF_LIT(b, "}");
    send_buf(b);
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
    s.tools_hidden.array_plot = false;
    ap.plotw = ap.width - 30 - 10 * text_metrics.small_width;
    ap.ploth = ap.height - 55;
    send_window(xpp::events::WINDOW_CREATED, xpp::window_name(s,WIN_APLOT), ap.plotw, ap.ploth, name,"array_plot");
}

void j_aplot_redraw(xpp::Session &s) { send_aplot(s, NULL); }

namespace {

/* write the picture the client shows as a GIF: one file, or a frame of
   the range movie (aplotwin.c gif_aplot_all); false when replacing the file was declined */
xpp::Result<bool> aplot_gif(ArrayPlotState &a, const char *file, int still)
{
    int w, h;
    xpp::Writer one; /* a still: one GIF, in place once whole */
    xpp::Result<> opened;
    if (still == 1) {
        one = xpp::open_writer_asking(file,true,&opened);
        if (!opened) return std::unexpected(opened.error());
        if (!one) return false;
    } else if (a.range_count == 0) {
        /* a range movie's frames all go into the first frame's file, a
           stream kept open until arrayplot.cpp's close_aplot_files */
        a.movie=xpp::open_writer_asking(file,true,&opened);
        if (!opened) return std::unexpected(opened.error());
    }
    if(still!=1&&!a.movie)return false;
    std::vector<unsigned char> rgb = ask_pixels(WIN_APLOT, -1, &w, &h);
    if(rgb.empty()){
        if(one)xpp::abort_save(one);
        else if(a.movie)xpp::abort_save(a.movie);
        return xpp::fail("array plot", "The client gave no picture of the array plot", xpp::command_place());
    }
    xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels(still == 1 ? one : a.movie,rgb,w,h,still == 1 ? xpp::IMAGE_STILL_FRAME : a.range_count);
    if (one) {
        if (const xpp::Result<> committed = xpp::commit_save(one); !committed) return std::unexpected(committed.error());
    }
    return true;
}

} // namespace

void j_aplot_draw_one(xpp::Session &s, std::string_view tag)
{
    const std::string shown(tag); /* send_aplot's tag: NUL-terminated, or none */
    send_aplot(s, s.array_plot.tag ? shown.c_str() : nullptr);
    if(!s.array_plot.save_cancelled){
        std::string file=xpp::files::frame_name(s.array_plot.range_stem,".gif",s.array_plot.still?s.array_plot.range_count:0);
        if(s.array_plot.still && s.array_plot.range_count>0) /* asked once, for the first */
            s.array_plot.save_cancelled=!xpp::ok_or_show(reset_save_decision());
        if(!s.array_plot.save_cancelled){
            const xpp::Result<bool> saved=aplot_gif(s.array_plot,file.c_str(),s.array_plot.still);
            s.array_plot.save_cancelled=!xpp::ok_or_show(saved)||!*saved;
        }
    }
    s.array_plot.range_count++;
}

xpp::Result<bool> j_aplot_gif(xpp::Session &s, std::string_view file) { return aplot_gif(s.array_plot, std::string(file).c_str(), 1); }

} // namespace xpp::json
