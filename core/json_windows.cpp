/* The plot windows (create, select, destroy, redraw, the view), the
   pictures only the client has (pixels, for frame and GIF writers), the
   kinescope, whose frames the client keeps, and the array plot window. */
#include "xpp_files.h"
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


/* ---- plot windows ------------------------------------------------------------ */

/* the main plot window, for hello */
void send_main_window(const char *title) { send_window("create", 1, main_plot_width, main_plot_height, title); }

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

void send_window(const char *what, unsigned long id, int w, int h, std::string_view title)
{
    std::string b;
    buf_format(b, "{{\"ev\":\"window\",\"op\":\"{}\",\"win\":{:d},\"w\":{:d},\"h\":{:d},\"title\":",
               what, id, w, h);
    buf_str(b, title);
    BUF_LIT(b, "}");
    send_buf(b);
}

void select_graph(xpp::Session &s, int i)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[i];
    s.plot_windows.draw_win = s.plot_windows.graph[i].w;
    get_draw_area(s);
    send_window("select", s.plot_windows.draw_win, s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt);
}

void j_create_plot_window(xpp::Session &s)
{
    const int i = s.plot_windows.active;
    send_window("create", s.plot_windows.graph[i].w, s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt, "");
    send_window("select", s.plot_windows.draw_win, s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt);
}

/* The core removes the window; this callback only tells the page. */
void j_destroy_plot_window(xpp::Session &s)
{
    send_window("destroy",s.plot_windows.draw_win,0,0);
    const GRAPH &main=s.plot_windows.graph[0];
    send_window("select",main.w,main.x11Wid,main.x11Hgt);
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
    if (add_label(s, look, s.plot_windows.draw_win, xd, yd) < 0) j_command_error("text", xpp::format("a session has at most {} texts", MAXLAB));
}

/* Parse a plot ID once for commands that name a window. Invalid IDs are
   errors at the command (or recording step), never the main window fallback. */
bool read_plot_window(const xpp::Session &s,const char *line,const char *command,int &index)
{
    int id=1;
    if(!get_whole(line,"win",id)||id<1){
        j_command_error(command,xpp::format("{}: win is a whole number, a positive plot window ID",command));
        return false;
    }
    index=graph_of(s,static_cast<XppWinId>(id));
    if(!s.plot_windows.graph[index].Use || s.plot_windows.graph[index].w!=static_cast<XppWinId>(id)){
        j_command_error(command,xpp::format("{}: window {} is not an open plot window",command,id));
        return false;
    }
    return true;
}

/* {"cmd":"click","win":w}: the user clicked in plot window w */
void click_command(xpp::Session &s, const char *line)
{
    int win;
    if(!read_plot_window(s,line,"click",win))return;
    if(s.plot_windows.active!=win)select_graph(s,win);
}

/* {"cmd":"text","op":"add"|"edit"|"delete","win":w,...}: the text labels of
   plot window w, which the page's text dialog drives (docs/protocol.md) */
void text_command(xpp::Session &s, const char *line)
{
    std::string op;
    get_string(line, "op", op);
    int win;
    if(!read_plot_window(s,line,"text",win))return;
    if (op != "add" && op != "edit" && op != "delete") {
        j_command_error("text", "text takes op add, edit or delete");
        return;
    }
    const XppWinId w = s.plot_windows.graph[win].w;
    const bool add = op == "add";
    int id = -1;
    if (!add) {
        if (!get_whole(line, "id", id) || id < 0 || id >= MAXLAB || s.labels[id].use != 1 || s.labels[id].w != w) {
            j_command_error("text", xpp::format("text {}: window {} has no text with that id", op, w));
            return;
        }
        if (op == "delete") {
            xpp::delete_label(s, id);
            return;
        }
    }
    /* edit changes what is given; add takes the defaults of a new text */
    xpp::LabelLook look;
    if (!add) look = {s.labels[id].s, s.labels[id].size, s.labels[id].style, s.labels[id].color};
    double x = add ? 0 : s.labels[id].x, y = add ? 0 : s.labels[id].y;
    const char *jx = js_find(line, "x"), *jy = js_find(line, "y");
    if ((add && (!jx || !jy)) || (jx && !js_number(jx, &x)) || (jy && !js_number(jy, &y)) || !std::isfinite(x) || !std::isfinite(y)) {
        j_command_error("text", "x and y are the position in the plot's coordinates");
        return;
    }
    if (const char *jt = js_find(line, "text"); (jt && !js_string(jt, look.text)) || (add && !jt)) {
        j_command_error("text", "the text is a string");
        return;
    }
    if (!get_whole(line, "size", look.size) || !get_whole(line, "style", look.style)) {
        j_command_error("text", "size and style are whole numbers");
        return;
    }
    if (const char *jc = js_find(line, "color")) {
        std::string word;
        const std::optional<xpp::Color> parsed = js_string(jc, word) ? xpp::parse_color(word) : std::nullopt;
        if (!parsed) {
            j_command_error("text", "the color is a string: #rrggbb, #rrggbbaa, a CSS colour name or foreground");
            return;
        }
        look.color = *parsed;
    }
    if (const std::string problem = xpp::label_problem(look); !problem.empty()) {
        j_command_error("text", problem);
        return;
    }
    const float xd = static_cast<float>(x), yd = static_cast<float>(y);
    if (!add) {
        xpp::change_label(s, id, std::move(look), jx || jy, xd, yd);
        return;
    }
    if (xpp::add_label(s, std::move(look), w, xd, yd) < 0) j_command_error("text", xpp::format("a session has at most {} texts", MAXLAB));
}

void display_command(xpp::Session &s, const char *line)
{
    int win;
    if(!read_plot_window(s,line,"display",win))return;
    xpp::PlotDisplay &d = s.plot_display[win];
    xpp::Zoom z = d.zoom;
    const int rx = get_range(line, "x", z.x), ry = get_range(line, "y", z.y);
    if (rx < 0 || ry < 0) {
        j_command_error("display", "display: x and y are [low, high] with low below high, or null");
        return;
    }
    d.zoom = z;
    const char *jr = js_find(line, "runs");
    if (jr) d.show_runs = get_num(line, "runs", 1) != 0;
    const char *freeze = js_find(line, "freeze");
    if (freeze) d.freeze_runs = get_num(line, "freeze", 1) != 0;
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
    else buf_format(b, ",\"win\":{:d}", win);
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
    buf_format(b, "{{\"ev\":\"film\",\"op\":\"{}\",\"count\":{:d},\"win\":{:d},\"cycles\":{:d},\"delay\":{:d}}}",
               what, s.kinescope.frames, s.plot_windows.draw_win, s.kinescope.cycles, s.kinescope.frame_ms);
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

void j_movie_save(xpp::Session &s, std::string_view basename, int fmat)
{
    const xpp::ImageFormat &format = xpp::image_formats[fmat];
    int w, h;
    for (int i = 0; i < s.kinescope.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) return;
        std::string file(basename);
        if(i>0){ /* the series was asked for once; later names are derived */
            file=xpp::files::frame_name(basename,xpp::format(".{}",format.extension),i);
            if(!xpp::ok_or_show(reset_save_decision()))return;
        }
        const xpp::Result<bool> saved=xpp::save_pixels(format,file.c_str(),rgb,w,h);
        if (!xpp::ok_or_show(saved) || !*saved) return;
    }
}

void j_movie_make_anigif(xpp::Session &s)
{
    const XppKinescope &k = s.kinescope;
    int w, h, w0 = 0, h0 = 0;
    if (!xpp::save_ready(k.frames > 0)) return;
    xpp::Writer out = xpp::ask_output_writer(s,"Save kinescope GIF",".gif",{},true);
    if (!out) return;
    for (int i = 0; i < k.frames; i++) {
        std::vector<unsigned char> rgb = ask_pixels(0, i, &w, &h);
        if (rgb.empty()) { xpp::abort_save(out); return; }
        if (i == 0) {
            w0 = w;
            h0 = h;
        } else if (w != w0 || h != h0) {
            j_command_error("kinescope", "All clips must be same size");
            xpp::abort_save(out);
            return;
        }
        xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels(out,rgb,w,h,i);
    }
    xpp::image_formats[xpp::IMAGE_FORMAT_GIF].finish_movie(out);
    xpp::ok_or_show(xpp::commit_save(out));
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
    BUF_LIT(b, "{\"ev\":\"aplot\",\"title\":\"");
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
    buf_format(b, "],\"nx\":{:d},\"ny\":{:d}", nx, ny);
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
    ap.plotw = ap.width - 30 - 10 * text_metrics.small_width;
    ap.ploth = ap.height - 55;
    send_window("create", WIN_APLOT, ap.plotw, ap.ploth, name);
}

void j_aplot_redraw(xpp::Session &s) { send_aplot(s, NULL); }

namespace {

/* write the picture the client shows as a GIF: one file, or a frame of
   the range movie (aplotwin.c gif_aplot_all) */
bool aplot_gif(ArrayPlotState &a, const char *file, int still)
{
    int w, h;
    xpp::Writer one; /* a still: one GIF, in place once whole */
    if (still == 1) {
        one = xpp::open_writer_asking(file,true);
        if (!one) return false;
    } else if (a.range_count == 0) {
        /* a range movie's frames all go into the first frame's file, a
           stream kept open until arrayplot.cpp's close_aplot_files */
        a.movie=xpp::open_writer_asking(file,true);
        if(!a.movie)return false;
    }
    if(still!=1&&!a.movie)return false;
    std::vector<unsigned char> rgb = ask_pixels(WIN_APLOT, -1, &w, &h);
    if(rgb.empty()){
        if(one)xpp::abort_save(one);
        else if(a.movie)xpp::abort_save(a.movie);
        return false;
    }
    xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels(still == 1 ? one : a.movie,rgb,w,h,still == 1 ? xpp::IMAGE_STILL_FRAME : a.range_count);
    return !one || xpp::ok_or_show(xpp::commit_save(one));
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
        if(!s.array_plot.save_cancelled)s.array_plot.save_cancelled=!aplot_gif(s.array_plot,file.c_str(),s.array_plot.still);
    }
    s.array_plot.range_count++;
}

/* {"cmd":"aplot","op":"scroll"|"close"}: what the array plot window's keys
   (menu_aplot_window) do not say */
void aplot_command(xpp::Session &s, const char *line)
{
    std::string o;
    get_string(line, "op", o);
    if (o != "scroll" && o != "close") {
        j_command_error("aplot", xpp::format("Unknown aplot op {}", o));
        return;
    }
    if (!s.array_plot.plot.alive) return;
    if (o == "scroll") {
        /* dragging the plot by dy pixels moves the first row */
        s.array_plot.plot.nstart -= get_int(line, "dy", 0);
        if (s.array_plot.plot.nstart < 0) s.array_plot.plot.nstart = 0;
        send_aplot(s, NULL);
    } else if (o == "close") {
        s.array_plot.plot.alive = 0;
        send_window("destroy", WIN_APLOT, 0, 0);
    }
}

/* {"cmd":"colormap","name":"plasma"}: the session's colour scale (the Colormap menu's change, by name); the
   array plot shows it at once */
void colormap_command(xpp::Session &s, const char *line)
{
    std::string name;
    if (!get_string(line, "name", name)) {
        j_command_error("colormap", "colormap takes a name");
        return;
    }
    const int i = xpp::choice_index(xpp::colormap_names, name);
    if (i < 0) {
        j_command_error("colormap", xpp::format("`{}` is no colour scale", name));
        return;
    }
    s.colormap = static_cast<xpp::Colormap>(i);
    send_aplot(s, NULL);
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
        if(!xpp::save_ready(s.array_plot.plot.plotdef!=0&&s.browser.view.maxrow>2))return;
        const char *ext = xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension;
        std::string file = xpp::files::output_name(s.model().this_file,xpp::format(".{}",ext),"array");
        if (file_selector("GIF plot", file, xpp::format("*.{}", ext))) aplot_gif(s.array_plot, file.c_str(), 1);
        break;
    }
    }
}

} // namespace xpp::json
