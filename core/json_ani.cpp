/* The animation window (aniwin.c without X): its slider and buttons, and
   Go, which plays the frames (ani_data.cpp sends them) and can write them
   to files with the pixels the client renders. */
#include "ui_json_internal.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "browse.h"
#include "aniparse.h"
#include "mykeydef.h"
#include "scrngif.h"
#include "my_rhs.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <sys/time.h>
#include <vector>

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern BROWSER my_browser;
extern int NODE, NMarkov;
extern MPEG_SAVE mpeg;
extern int n_anicom, ani_speed, ani_speed_inc, ani_grab_flag;
}

namespace xpp::json {

/* ---- animation window ------------------------------------------------------------ */

/* the animation window's state for its slider and toggles */
void j_ani_slider(void)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"ani\",\"pos\":{:d},\"rows\":{:d},\"fly\":{:d},\"grab\":{:d},\"skip\":{:d},\"speed\":{:d},"
               "\"loaded\":{:d},\"open\":{:d}}}",
               vcr.pos, my_browser.maxrow, ani_options.on_the_fly, ani_grab_flag, vcr.inc, ani_speed, n_anicom > 0,
               vcr.iexist);
    send_buf(&b);
}

/* ani fast, slow and speed: the delay between two frames of Go, ms; 0 when
   op is none of them */
int ani_speed_op(const char *o, const char *line)
{
    if (strcmp(o, "fast") == 0) {
        if ((ani_speed -= ani_speed_inc) < 0) ani_speed = 0;
    } else if (strcmp(o, "slow") == 0) {
        if ((ani_speed += ani_speed_inc) > 100) ani_speed = 100;
    } else if (strcmp(o, "speed") == 0) {
        double ms = get_num(line, "ms", ani_speed);
        ani_speed = ms < 0 ? 0 : ms > 1000 ? 1000 : static_cast<int>(ms); /* the .ani `speed` command's range */
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
    double y[MAXODE];
    float **ss = my_browser.data;
    xpp::Writer gif; /* anim.gif, when the animation is written as one */
    int i, stop = 0, frame = 0, written = 0, w, h;
    if (n_anicom == 0 || my_browser.maxrow < 2) return;
    set_ani_perm();
    if (mpeg.aviflag == 1) {
        gif = xpp::Writer::binary("anim.gif");
        set_global_map(1);
    }
    while (!stop) {
        int row = vcr.pos, ppm = mpeg.flag > 0 && frame % (mpeg.skip > 0 ? mpeg.skip : 1) == 0;
        for (i = 0; i < NODE + NMarkov; i++) y[i] = ss[i + 1][row];
        set_fix_rhs(static_cast<double>(ss[0][row]), y);
        xpp_ui.ani_clear();
        render_ani();
        xpp_ui.ani_show();
        if (ppm || gif) {
            std::vector<unsigned char> rgb = ask_pixels(WIN_ANI, -1, &w, &h);
            if (rgb.empty()) break;
            if (ppm) write_ppm(xpp::format("{}_{}.ppm", mpeg.root, written++).c_str(), rgb, w, h);
            if (gif) {
                web_safe_colors(rgb);
                gif_stuff_ppm(rgb.data(), w, h, gif.file(), frame == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
            }
        }
        frame++;
        stop = ani_wait(ani_speed * (mpeg.aviflag == 1 || mpeg.flag > 0 ? 6 : 1));
        vcr.pos += vcr.inc;
        if (vcr.pos >= my_browser.maxrow) {
            stop = 1;
            vcr.pos = 0;
            reset_comets();
        }
    }
    mpeg.flag = 0;
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
    std::string o, what;
    int x = get_int(line, "x", 0), yy = get_int(line, "y", 0);
    /* a point in the animation's unit coordinates (u, v: y up, as the ani
       frame event's) instead of pixels: the nearest pixel of the window */
    if (js_find(line, "u") && js_find(line, "v")) {
        x = static_cast<int>(floor(get_num(line, "u", 0) * vcr.wid + 0.5));
        yy = static_cast<int>(floor((1 - get_num(line, "v", 0)) * vcr.hgt + 0.5));
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
        if (get_ani_file(NULL) && my_browser.maxrow >= 2) ani_reset();
    } else if (o == "go") ani_go();
    else if (o == "skip") ani_newskip();
    else if (o == "mpeg") ani_create_mpeg();
    else if (o == "fly") ani_options.on_the_fly = 1 - ani_options.on_the_fly;
    else if (o == "grab") ani_grab_start();
    else if (o == "seek" && my_browser.maxrow >= 2) {
        vcr.pos = 0;
        ani_flip1(0);
        ani_flip1(get_int(line, "pos", 0));
    } else if (o == "mouse" && ani_grab_flag) {
        /* dragging a grab point: down, move..., up (which may integrate) */
        get_string(line, "what", what, 8);
        if (what == "down") ani_grab_mouse(1, x, yy);
        else if (what == "move") update_ani_motion_stuff(x, yy);
        else if (what == "up") ani_grab_mouse(0, x, yy);
    } else if (o == "close" && vcr.iexist) {
        vcr.iexist = 0;
        ani_grab_flag = 0;
        send_window("destroy", WIN_ANI, 0, 0, NULL);
    }
    j_ani_slider();
}

void j_new_vcr(void)
{
    /* already open: say so (a client that reconnected has not seen it made) */
    if (vcr.iexist == 1) {
        j_ani_slider();
        return;
    }
    vcr.wid = 280;
    vcr.hgt = 350;
    vcr.iexist = 1;
    send_window("create", WIN_ANI, vcr.wid, vcr.hgt, "Animation");
    ani_view_created();
}
void j_ani_show(void)
{
    flush_pending();
    out_flush();
}

} // namespace xpp::json
