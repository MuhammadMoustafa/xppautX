/* The animation window (aniwin.c without X): its slider and buttons, and
   Go, which plays the frames (ani_data.cpp sends them) and can write them
   to files with the pixels the client renders. */
#include "ui_json_internal.h"
#include "session.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "browse.h"
#include "aniparse.h"
#include "mykeydef.h"
#include "scrngif.h"
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
    Buf b;
    buf_format(&b, "{{\"ev\":\"ani\",\"pos\":{:d},\"rows\":{:d},\"fly\":{:d},\"grab\":{:d},\"skip\":{:d},\"speed\":{:d},"
               "\"loaded\":{:d},\"open\":{:d}}}",
               s.animation.vcr.pos, s.browser.view.maxrow, s.animation.options.on_the_fly, s.animation.grab_flag, s.animation.vcr.inc, s.animation.speed, s.animation.ncom > 0,
               s.animation.vcr.iexist);
    send_buf(&b);
}

} // namespace

void j_ani_slider(xpp::Session &s) { send_ani_slider(s); }

namespace {
/* the step of ani fast and slow */
constexpr int ani_speed_inc = 2;
}

/* ani fast, slow and speed: the delay between two frames of Go, ms; 0 when
   op is none of them */
int ani_speed_op(xpp::Session &s, const char *o, const char *line)
{
    if (strcmp(o, "fast") == 0) {
        if ((s.animation.speed -= ani_speed_inc) < 0) s.animation.speed = 0;
    } else if (strcmp(o, "slow") == 0) {
        if ((s.animation.speed += ani_speed_inc) > 100) s.animation.speed = 100;
    } else if (strcmp(o, "speed") == 0) {
        double ms = get_num(line, "ms", s.animation.speed);
        s.animation.speed = ms < 0 ? 0 : ms > 1000 ? 1000 : static_cast<int>(ms); /* the .ani `speed` command's range */
    } else
        return 0;
    return 1;
}

namespace {

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
        if (xpp::job::cancelled() || xpp::job::take_key() == ESC) return 1; /* a replayed Escape (xpp_job.h) */
        gettimeofday(&now, NULL);
        left = ms - ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
        line = read_line(xpp::inbox::From::control, left > 0 ? static_cast<int>(left) : 0);
        if (!line) return 0;
        r = control_line(s, line);
        if (r == ESC || r == ANI_PAUSE) return 1;
    }
}

/* the Go button (aniwin.c ani_flip without the X pixmap) */
void ani_go(xpp::Session &s)
{
    double y[MAXODE];
    float **ss = s.browser.view.data;
    xpp::Writer gif; /* anim.gif, when the animation is written as one */
    int i, stop = 0, frame = 0, written = 0, w, h;
    if (s.animation.ncom == 0 || s.browser.view.maxrow < 2) return;
    set_ani_perm(s);
    if (s.animation.mpeg.aviflag == 1) {
        gif = xpp::Writer::binary("anim.gif");
        set_global_map(1);
    }
    while (!stop) {
        int row = s.animation.vcr.pos, ppm = s.animation.mpeg.flag > 0 && frame % (s.animation.mpeg.skip > 0 ? s.animation.mpeg.skip : 1) == 0;
        for (i = 0; i < s.model().node + s.model().nmarkov; i++) y[i] = ss[i + 1][row];
        set_fix_rhs(s,static_cast<double>(ss[0][row]), y);
        ui.ani_clear();
        render_ani(s);
        ui.ani_show();
        if (ppm || gif) {
            std::vector<unsigned char> rgb = ask_pixels(WIN_ANI, -1, &w, &h);
            if (rgb.empty()) break;
            if (ppm) write_ppm(xpp::format("{}_{}.ppm", s.animation.mpeg.root, written++).c_str(), rgb, w, h);
            if (gif) {
                web_safe_colors(rgb);
                gif_stuff_ppm(rgb.data(), w, h, gif.file(), frame == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
            }
        }
        frame++;
        xpp::job::report_frame(frame);
        stop = ani_wait(s, s.animation.speed * (s.animation.mpeg.aviflag == 1 || s.animation.mpeg.flag > 0 ? 6 : 1));
        s.animation.vcr.pos += s.animation.vcr.inc;
        if (s.animation.vcr.pos >= s.browser.view.maxrow) {
            stop = 1;
            s.animation.vcr.pos = 0;
            reset_comets(s);
        }
    }
    s.animation.mpeg.flag = 0;
    if (gif) {
        end_ani_gif(gif.file());
        gif.commit();
        set_global_map(0);
    }
    send_ani_slider(s);
}

} // namespace

/* {"cmd":"ani","op":...}: what the animation window's keys do not say, the
   ones that carry a number or steer a running Go */
void ani_command(xpp::Session &s, const char *line)
{
    std::string o, what;
    int x = get_int(line, "x", 0), yy = get_int(line, "y", 0);
    /* a point in the animation's unit coordinates (u, v: y up, as the ani
       frame event's) instead of pixels: the nearest pixel of the window */
    if (js_find(line, "u") && js_find(line, "v")) {
        x = static_cast<int>(floor(get_num(line, "u", 0) * s.animation.vcr.wid + 0.5));
        yy = static_cast<int>(floor((1 - get_num(line, "v", 0)) * s.animation.vcr.hgt + 0.5));
    }
    get_string(line, "op", o, 16);
    if (ani_speed_op(s, o.c_str(), line)) {
        send_ani_slider(s);
        return;
    }
    if (o == "step") ani_flip1(s,get_int(line, "n", 1));
    else if (o == "seek") {
        if (s.browser.view.maxrow >= 2) {
            s.animation.vcr.pos = 0;
            ani_flip1(s,0);
            ani_flip1(s,get_int(line, "pos", 0));
        }
    } else if (o == "mouse") {
        /* dragging a grab point: down, move..., up (which may integrate) */
        get_string(line, "what", what, 8);
        if (s.animation.grab_flag) {
            if (what == "down") ani_grab_mouse(s,1, x, yy);
            else if (what == "move") update_ani_motion_stuff(s,x, yy);
            else if (what == "up") ani_grab_mouse(s,0, x, yy);
        }
    } else if (o == "close") {
        if (s.animation.vcr.iexist) {
            s.animation.vcr.iexist = 0;
            s.animation.grab_flag = 0;
            send_window("destroy", WIN_ANI, 0, 0);
        }
    } else {
        j_err_msg(xpp::format("Unknown ani op {}", o));
        return;
    }
    send_ani_slider(s);
}

/* a key of the animation window (menu_ani_window) */
void ani_key(xpp::Session &s, int ch)
{
    switch (menu_index(&menu_ani_window, ch)) {
    case NK_FILE:
        /* a new animation shows its first frame at once when there is data */
        if (get_ani_file(s,NULL) && s.browser.view.maxrow >= 2) ani_reset(s);
        break;
    case NK_GO: ani_go(s); break;
    case NK_RESET: ani_reset(s); break;
    case NK_SKIP: ani_newskip(s); break;
    case NK_MPEG: ani_create_mpeg(s); break;
    case NK_FLY: s.animation.options.on_the_fly = 1 - s.animation.options.on_the_fly; break;
    case NK_GRAB: ani_grab_start(s); break;
    }
    send_ani_slider(s);
}

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
    send_window("create", WIN_ANI, s.animation.vcr.wid, s.animation.vcr.hgt, "Animation");
    ani_view_created(s);
}
void j_ani_show(void)
{
    flush_pending();
    out_flush();
}

} // namespace xpp::json
