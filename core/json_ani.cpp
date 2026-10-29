/* The animation window: its slider and buttons, and
   Go, which plays the frames (ani_data.cpp sends them) and can write them
   to files with the pixels the client renders. */
#include "ui_json_internal.h"
#include "session.h"
#include "many_pops.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "browse.h"
#include "aniparse.h"
#include "command_registry.h"
#include "mykeydef.h"
#include "image_format.h"
#include "my_rhs.h"
#include "form_ode.h"
#include "menus.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <sys/time.h>
#include <vector>
#include "model.h"

/* the core's own globals and functions that have no header of their own */
namespace xpp::json {

/* ---- animation window ------------------------------------------------------------ */

namespace {

/* the animation window's state for its slider and toggles */
void send_ani_slider(const xpp::Session &s)
{
    std::string b;
    buf_format(b, "{},\"pos\":{:d},\"rows\":{:d},\"fly\":{:d},\"grab\":{:d},\"skip\":{:d},\"speed\":{:d},"
               "\"loaded\":{:d},\"open\":{:d}}}",
               xpp::events::open(xpp::events::ANIMATION_STATE), s.animation.vcr.pos, s.browser.view.maxrow, s.animation.options.on_the_fly, s.animation.grab_flag, s.animation.vcr.inc, s.animation.speed, s.animation.ncom > 0,
               s.animation.vcr.iexist);
    send_buf(b);
}

} // namespace

void j_ani_slider(xpp::Session &s) { send_ani_slider(s); }

namespace {

/* a Go that writes its frames waits this many times as long between them: the client draws and answers each */
constexpr int ani_export_pace = 6;

/* between frames of Go: 1 when Pause, ABORT or Esc stops the playback */
int ani_wait(xpp::Session &s, int ms)
{
    struct timeval start, now;
    gettimeofday(&start, NULL);
    flush_pending();
    out_flush();
    for (;;) {
        char *line;
        long left;
        int r;
        if (player_controls(s) == ANI_PAUSE) return 1;
        if (xpp::job::cancelled() || xpp::job::take_key() == ESC) return 1; /* a replayed Escape (xpp_job.h) */
        gettimeofday(&now, NULL);
        left = ms - ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
        line = read_line(xpp::inbox::From::control, left > 0 ? static_cast<int>(left) : 0);
        if (!line) return 0;
        r = control_line(s, line);
        if (r == ESC || r == ANI_PAUSE) return 1;
    }
}

/* the Go button (aniwin.c ani_flip without the X pixmap): the frames from the one shown to the last, also
   written when `out` says so; how many were shown, or why they were not written */
xpp::Result<int> ani_go(xpp::Session &s, const xpp::AniExport &out)
{
    std::vector<double> y(static_cast<std::size_t>(s.model().node + s.model().nmarkov));
    float **ss = s.browser.view.data;
    xpp::Writer gif; /* chosen GIF, when the animation is written as one */
    xpp::Result<> outcome; /* ok until a frame cannot be written */
    int i, stop = 0, frame = 0, w, h;
    const bool writing = out.kind != xpp::AniExportKind::none;
    set_ani_perm(s);
    if (out.kind == xpp::AniExportKind::gif) {
        xpp::Result<> opened;
        gif = xpp::open_writer_asking(out.file, true, &opened);
        if (!opened) return std::unexpected(opened.error());
        if (!gif) return xpp::fail("animation", xpp::format("\"{}\" exists and replacing it was declined: nothing was written", out.file), xpp::command_place());
    }
    while (!stop) {
        int row = s.animation.vcr.pos;
        for (i = 0; i < s.model().node + s.model().nmarkov; i++) y[i] = ss[i + 1][row];
        set_fix_rhs(s,static_cast<double>(ss[0][row]), y.data());
        render_ani(s);
        ui.ani_show();
        if (writing) {
            std::vector<unsigned char> rgb = ask_pixels(WIN_ANI, -1, &w, &h);
            if (rgb.empty()) {
                outcome = xpp::fail("animation", xpp::format("The client gave no picture of frame {}", frame + 1), xpp::command_place());
                break;
            }
            if (out.kind == xpp::AniExportKind::ppm) {
                const xpp::ImageFormat &format=xpp::image_formats[xpp::IMAGE_FORMAT_PPM];
                const std::string file=xpp::files::frame_name(out.file,".ppm",frame);
                if (frame > 0) outcome = reset_save_decision();
                if (outcome) {
                    const xpp::Result<bool> saved=xpp::save_pixels(format,file.c_str(),rgb,w,h);
                    if (!saved) outcome = std::unexpected(saved.error());
                    else if (!*saved) outcome = xpp::fail("animation", xpp::format("\"{}\" exists and replacing it was declined: {} frames written", file, frame), xpp::command_place());
                }
                if (!outcome) break;
                frame++; /* counted once written */
            } else xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels(gif,rgb,w,h,frame++);
        } else frame++;
        xpp::job::report_frame(frame);
        stop = ani_wait(s, s.animation.speed * (writing ? ani_export_pace : 1));
        s.animation.vcr.pos += s.animation.vcr.inc;
        if (s.animation.vcr.pos >= s.browser.view.maxrow) {
            stop = 1;
            s.animation.vcr.pos = 0;
            reset_comets(s);
        }
    }
    if (gif) {
        if (!outcome) xpp::abort_save(gif);
        else {
            xpp::image_formats[xpp::IMAGE_FORMAT_GIF].finish_movie(gif);
            outcome = xpp::commit_save(gif);
        }
    }
    send_ani_slider(s);
    if (!outcome) return std::unexpected(outcome.error());
    return frame;
}

} // namespace

xpp::Result<int> j_ani_go(xpp::Session &s, const xpp::AniExport &out) { return ani_go(s, out); }

void j_new_vcr(xpp::Session &s)
{
    /* already open: say so (a client that reconnected has not seen it made) */
    if (s.animation.vcr.iexist == 1) {
        send_ani_slider(s);
        return;
    }
    s.animation.vcr.wid = 280;
    s.animation.vcr.hgt = 350;
    s.animation.vcr.iexist = 1;
    s.tools_hidden.animation = false;
    send_window(xpp::events::WINDOW_CREATED,xpp::window_name(s,WIN_ANI), s.animation.vcr.wid, s.animation.vcr.hgt, "Animation","animation");
    ani_view_created(s);
}
void j_ani_show(void)
{
    flush_pending();
    out_flush();
}

} // namespace xpp::json
