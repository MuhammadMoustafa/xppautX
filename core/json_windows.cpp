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
void send_main_window(const xpp::Session &s,const char *title) { send_window("create", s.plot_windows.graph[0].name, main_plot_width, main_plot_height, title); }

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

void send_window(const char *what, std::string_view name, int w, int h, std::string_view title, std::string_view kind)
{
    std::string b;
    buf_format(b,"{{\"ev\":\"window\",\"op\":\"{}\",\"win\":",what);
    buf_str(b,name);
    BUF_LIT(b,",\"kind\":");
    buf_str(b,kind);
    buf_format(b,",\"w\":{},\"h\":{},\"title\":",w,h);
    buf_str(b, title);
    BUF_LIT(b, "}");
    send_buf(b);
}

void j_window_renamed(xpp::Session &, std::string_view old_name, std::string_view new_name)
{
    std::string event="{\"ev\":\"window.renamed\",\"window\":";
    json_append_string(event,old_name);
    event+=",\"new_name\":";
    json_append_string(event,new_name);
    event+='}';
    send_buf(event);
}

void j_auto_closed(xpp::Session &s, std::string_view name)
{
    send_window("destroy",name,0,0,"","diagram");
    diag_forget();
    auto_data_forget(s);
}

void j_create_plot_window(xpp::Session &s)
{
    const int i = s.plot_windows.active;
    send_window("create", xpp::window_name(s,s.plot_windows.graph[i].w), s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt, "");
    send_window("select", xpp::window_name(s,s.plot_windows.draw_win), s.plot_windows.graph[i].x11Wid, s.plot_windows.graph[i].x11Hgt);
}

/* The core removes the window; this callback only tells the page. */
void j_destroy_plot_window(xpp::Session &s)
{
    send_window("destroy",xpp::window_name(s,s.plot_windows.draw_win),0,0);
    const GRAPH &main=s.plot_windows.graph[0];
    send_window("select",xpp::window_name(s,main.w),main.x11Wid,main.x11Hgt);
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

/* Parse a plot ID once for commands that name a window. Invalid IDs are
   errors at the command (or recording step), never the main window fallback. */
bool read_plot_window(const xpp::Session &s,const char *line,const char *command,int &index)
{
    if(js_find(line,"win")){j_command_error(command,"Use window with a name; win is not accepted");return false;}
    std::string name=s.plot_windows.graph[s.plot_windows.active].name;
    if(js_find(line,"window") && !get_string(line,"window",name)){
        j_command_error(command,"window must be text");return false;
    }
    const auto target=xpp::find_window(s,name);
    if(!target || target->kind!=xpp::WindowKind::graph){j_command_error(command,xpp::format("Unknown graph \"{}\"",name));return false;}
    index=target->index;
    return true;
}

/* {"cmd":"annotation","op":OP,...}: the texts, arrows, pointers and markers of the plot windows by their ids, in
   plot coordinates (docs/protocol.md "Annotations"); the page's text dialog and the Text,etc menu's Edit go
   through the same core functions (grobs.h) */
void annotation_command(xpp::Session &s, const char *line)
{
    std::string op;
    get_string(line, "op", op);
    const auto fail = [&](std::string what) { j_command_error("annotation", xpp::format("annotation {}: {}", op, what)); };
    /* member key as a finite number in v (left as it was when absent, which is an error when required) */
    const auto real = [&](const char *key, double &v, bool required) {
        const char *j = js_find(line, key);
        if (!j && !required) return true;
        if (j && js_number(j, &v) && std::isfinite(v)) return true;
        fail(xpp::format("{} is {}a finite number", key, j ? "" : "required: "));
        return false;
    };
    const auto size_of = [&](double &size) {
        if (!real("size", size, false)) return false;
        if (size < 0) return fail(xpp::format("the size {} is not 0 or more", size)), false;
        return true;
    };
    const auto color_of = [&](xpp::Color &color) {
        const char *j = js_find(line, "color");
        std::string word;
        if (!j) return true;
        const std::optional<xpp::Color> parsed = js_string(j, word) ? xpp::parse_color(word) : std::nullopt;
        if (!parsed) return fail("the color is a string: #rrggbb, #rrggbbaa, a CSS colour name or foreground"), false;
        color = *parsed;
        return true;
    };
    /* the shape word of a marker in `shape` as its object type, when given */
    const auto shape_of = [&](int &type) {
        std::string word;
        if (!js_find(line, "shape")) return true;
        const auto at = get_string(line, "shape", word) ? std::ranges::find(xpp::marker_shape_names, word) : xpp::marker_shape_names.end();
        if (at == xpp::marker_shape_names.end()) return fail("the shape is box, diamond, triangle, plus, cross or circle"), false;
        type = xpp::MARKER + static_cast<int>(at - xpp::marker_shape_names.begin());
        return true;
    };
    /* a text's look: the given members over look */
    const auto look_of = [&](xpp::LabelLook &look) {
        if (const char *jt = js_find(line, "text"); jt && !js_string(jt, look.text)) return fail("the text is a string"), false;
        if (!get_whole(line, "size", look.size) || !get_whole(line, "style", look.style)) return fail("size and style are whole numbers"), false;
        if (!color_of(look.color)) return false;
        if (const std::string problem = xpp::label_problem(look); !problem.empty()) return fail(problem), false;
        return true;
    };
    const auto full = [&](const char *what) { fail(xpp::format("a session has at most {} {}", xpp::ANNOTATION_LIST_MAX, what)); };
    const auto added = [&](int id) {
        std::string b = xpp::format("{{\"ev\":\"annotation\",\"op\":\"{}\",\"id\":{}}}", op, id);
        send_buf(b);
    };

    static constexpr std::array<std::string_view, 9> ops{"list", "add_text", "add_arrow", "add_pointer", "add_marker", "edit", "move", "delete", "delete_all"};
    if (std::ranges::find(ops, op) == ops.end()) {
        j_command_error("annotation", "annotation takes op list, add_text, add_arrow, add_pointer, add_marker, edit, move, delete or delete_all");
        return;
    }
    if (op == "edit" || op == "move" || op == "delete") {
        int id = -1;
        if (!get_whole(line, "id", id) || (!xpp::find_annotation(s.labels, id) && !xpp::find_annotation(s.grobs, id))) {
            fail(xpp::format("there is no annotation with that id ({})", id));
            return;
        }
        if (op == "delete") {
            xpp::delete_annotation(s, id);
        } else if (op == "move") {
            double x = 0, y = 0;
            if (real("new_x", x, true) && real("new_y", y, true)) xpp::move_annotation(s, id, x, y);
        } else if (const LABEL *l = xpp::find_annotation(s.labels, id)) {
            xpp::LabelLook look{l->s, l->size, l->style, l->color};
            if (look_of(look)) xpp::edit_label(s, id, std::move(look));
        } else {
            const xpp::GROB *g = xpp::find_annotation(s.grobs, id);
            xpp::GrobLook look{g->type, g->size, g->color};
            if (g->type >= xpp::MARKER && !shape_of(look.type)) return;
            if (size_of(look.size) && color_of(look.color)) xpp::edit_grob(s, id, look);
        }
        return;
    }
    int win;
    if (!read_plot_window(s, line, "annotation", win)) return;
    const XppWinId w = s.plot_windows.graph[win].w;
    if (op == "list") {
        std::string b = xpp::annotations_json(s, w);
        send_buf(b);
    } else if (op == "delete_all") {
        xpp::destroy_labels_and_grobs(s, w);
    } else if (op == "add_text") {
        xpp::LabelLook look;
        double x = 0, y = 0;
        if (!js_find(line, "text")) return fail("the text is required");
        if (!real("x", x, true) || !real("y", y, true) || !look_of(look)) return;
        const int id = xpp::add_label(s, std::move(look), w, static_cast<float>(x), static_cast<float>(y));
        if (id < 0) full("texts");
        else added(id);
    } else if (op == "add_marker") {
        double x = 0, y = 0, size = xpp::MARKER_SIZE_DEFAULT;
        xpp::Color color = xpp::foreground_color;
        int type = xpp::MARKER;
        if (!real("x", x, true) || !real("y", y, true) || !size_of(size) || !color_of(color) || !shape_of(type)) return;
        const int id = xpp::add_grob(s, w, x, y, 0, 0, size, type, color);
        if (id < 0) full("arrows, pointers and markers");
        else added(id);
    } else {
        double tip_x = 0, tip_y = 0, tail_x = 0, tail_y = 0, size = xpp::ARROW_SIZE_DEFAULT;
        xpp::Color color = xpp::foreground_color;
        if (!real("tip_x", tip_x, true) || !real("tip_y", tip_y, true) || !real("tail_x", tail_x, true) || !real("tail_y", tail_y, true) ||
            !size_of(size) || !color_of(color))
            return;
        if (tip_x == tail_x && tip_y == tail_y) return fail("the tip and the tail are the same point");
        const int id = xpp::add_grob(s, w, tip_x, tip_y, tail_x, tail_y, size, op == "add_arrow" ? xpp::ARROW : xpp::POINTER, color);
        if (id < 0) full("arrows, pointers and markers");
        else added(id);
    }
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
    buf_format(b,"{{\"ev\":\"film\",\"op\":\"{}\",\"count\":{},\"win\":",what,s.kinescope.frames);
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
    send_window("create", xpp::window_name(s,WIN_APLOT), ap.plotw, ap.ploth, name,"array_plot");
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
        send_window("destroy", xpp::window_name(s,WIN_APLOT), 0, 0,"","array_plot");
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
