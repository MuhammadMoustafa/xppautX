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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <sys/time.h>
#include <vector>
#include "model.h"

/* the core's own globals and functions that have no header of their own */
extern "C" {
}

namespace xpp::json {

/* ---- animation window ------------------------------------------------------------ */

/* the animation window's state for its slider and toggles */
void j_ani_slider(void)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"ani\",\"pos\":{:d},\"rows\":{:d},\"fly\":{:d},\"grab\":{:d},\"skip\":{:d},\"speed\":{:d},"
               "\"loaded\":{:d},\"open\":{:d}}}",
               xpp::session().animation.vcr.pos, xpp::session().browser.view.maxrow, xpp::session().animation.options.on_the_fly, xpp::session().animation.grab_flag, xpp::session().animation.vcr.inc, xpp::session().animation.speed, xpp::session().animation.ncom > 0,
               xpp::session().animation.vcr.iexist);
    send_buf(&b);
}

namespace {
/* the step of ani fast and slow */
constexpr int ani_speed_inc = 2;
}

/* ani fast, slow and speed: the delay between two frames of Go, ms; 0 when
   op is none of them */
int ani_speed_op(const char *o, const char *line)
{
    if (strcmp(o, "fast") == 0) {
        if ((xpp::session().animation.speed -= ani_speed_inc) < 0) xpp::session().animation.speed = 0;
    } else if (strcmp(o, "slow") == 0) {
        if ((xpp::session().animation.speed += ani_speed_inc) > 100) xpp::session().animation.speed = 100;
    } else if (strcmp(o, "speed") == 0) {
        double ms = get_num(line, "ms", xpp::session().animation.speed);
        xpp::session().animation.speed = ms < 0 ? 0 : ms > 1000 ? 1000 : static_cast<int>(ms); /* the .ani `speed` command's range */
    } else
        return 0;
    return 1;
}

namespace {

/* between frames of Go: 1 when Pause, ABORT or Esc stops the playback */
int ani_wait(int ms)
{
    struct timeval start, now;
    gettimeofday(&start, NULL);
    flush_pending();
    out_flush();
    for (;;) {
        char *line;
        long left;
        int r;
        if (xpp_job_cancelled()) return 1;
        gettimeofday(&now, NULL);
        left = ms - ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
        line = read_line(XPP_INBOX_CONTROL, left > 0 ? static_cast<int>(left) : 0);
        if (!line) return 0;
        r = control_line(line);
        if (r == ESC || r == ANI_PAUSE) return 1;
    }
}

/* the Go button (aniwin.c ani_flip without the X pixmap) */
void ani_go(void)
{
    xpp::Session &s=xpp::session();
    double y[MAXODE];
    float **ss = s.browser.view.data;
    xpp::Writer gif; /* anim.gif, when the animation is written as one */
    int i, stop = 0, frame = 0, written = 0, w, h;
    if (s.animation.ncom == 0 || s.browser.view.maxrow < 2) return;
    set_ani_perm();
    if (s.animation.mpeg.aviflag == 1) {
        gif = xpp::Writer::binary("anim.gif");
        set_global_map(1);
    }
    while (!stop) {
        int row = s.animation.vcr.pos, ppm = s.animation.mpeg.flag > 0 && frame % (s.animation.mpeg.skip > 0 ? s.animation.mpeg.skip : 1) == 0;
        for (i = 0; i < xpp::model().node + xpp::model().nmarkov; i++) y[i] = ss[i + 1][row];
        set_fix_rhs(static_cast<double>(ss[0][row]), y);
        xpp_ui.ani_clear();
        render_ani();
        xpp_ui.ani_show();
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
        stop = ani_wait(s.animation.speed * (s.animation.mpeg.aviflag == 1 || s.animation.mpeg.flag > 0 ? 6 : 1));
        s.animation.vcr.pos += s.animation.vcr.inc;
        if (s.animation.vcr.pos >= s.browser.view.maxrow) {
            stop = 1;
            s.animation.vcr.pos = 0;
            reset_comets();
        }
    }
    s.animation.mpeg.flag = 0;
    if (gif) {
        end_ani_gif(gif.file());
        gif.commit();
        set_global_map(0);
    }
    j_ani_slider();
}

} // namespace

void ani_command(const char *line)
{
    xpp::Session &s=xpp::session();
    std::string o, what;
    int x = get_int(line, "x", 0), yy = get_int(line, "y", 0);
    /* a point in the animation's unit coordinates (u, v: y up, as the ani
       frame event's) instead of pixels: the nearest pixel of the window */
    if (js_find(line, "u") && js_find(line, "v")) {
        x = static_cast<int>(floor(get_num(line, "u", 0) * s.animation.vcr.wid + 0.5));
        yy = static_cast<int>(floor((1 - get_num(line, "v", 0)) * s.animation.vcr.hgt + 0.5));
    }
    get_string(line, "op", o, 16);
    if (ani_speed_op(o.c_str(), line)) {
        j_ani_slider();
        return;
    }
    if (o == "step") ani_flip1(get_int(line, "n", 1));
    else if (o == "reset") ani_reset();
    else if (o == "file") {
        /* a new animation shows its first frame at once when there is data */
        if (get_ani_file(NULL) && s.browser.view.maxrow >= 2) ani_reset();
    } else if (o == "go") ani_go();
    else if (o == "skip") ani_newskip();
    else if (o == "mpeg") ani_create_mpeg();
    else if (o == "fly") s.animation.options.on_the_fly = 1 - s.animation.options.on_the_fly;
    else if (o == "grab") ani_grab_start();
    else if (o == "seek" && s.browser.view.maxrow >= 2) {
        s.animation.vcr.pos = 0;
        ani_flip1(0);
        ani_flip1(get_int(line, "pos", 0));
    } else if (o == "mouse" && s.animation.grab_flag) {
        /* dragging a grab point: down, move..., up (which may integrate) */
        get_string(line, "what", what, 8);
        if (what == "down") ani_grab_mouse(1, x, yy);
        else if (what == "move") update_ani_motion_stuff(x, yy);
        else if (what == "up") ani_grab_mouse(0, x, yy);
    } else if (o == "close" && s.animation.vcr.iexist) {
        s.animation.vcr.iexist = 0;
        s.animation.grab_flag = 0;
        send_window("destroy", WIN_ANI, 0, 0, NULL);
    }
    j_ani_slider();
}

void j_new_vcr(void)
{
    /* already open: say so (a client that reconnected has not seen it made) */
    if (xpp::session().animation.vcr.iexist == 1) {
        j_ani_slider();
        return;
    }
    xpp::session().animation.vcr.wid = 280;
    xpp::session().animation.vcr.hgt = 350;
    xpp::session().animation.vcr.iexist = 1;
    send_window("create", WIN_ANI, xpp::session().animation.vcr.wid, xpp::session().animation.vcr.hgt, "Animation");
    ani_view_created();
}
void j_ani_show(void)
{
    flush_pending();
    out_flush();
}

} // namespace xpp::json
