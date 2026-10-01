#ifndef ANI_DATA_H
#define ANI_DATA_H

#include <string>
#include <string_view>

namespace xpp {
struct Session; /* session.h */

/* The animation as data: the "ani" "frame" event (docs/protocol.md "The
   animation as data", docs/ui-v2.md event 10), for a front end that draws
   the frames itself.

   aniparse.cpp reports every primitive of the frame it draws, in unit
   coordinates of the animation's `dimension` box (u 0 at xlo, 1 at xhi; v
   0 at ylo, 1 at yhi: y up, as the .ani language has it), not clamped: a
   primitive the .ani puts outside the box lies outside [0,1]. Colours are
   palette indices as the pixel callbacks get them (0 black, 20..29 the
   named colours, 30.. the colour map); the event says them as XPP colour
   indices or #rrggbb. Line widths and a comet's dots are in pixels.

   The finished frame goes to a client that subscribed at once, or when
   frames come faster than 25 a second (Go, Fly) the latest one waits for
   the next that may go, and for the end of the command at the latest.

   ani_data.cpp; nothing escapes it. */

/* The frames drawn, as their events' text: a Session's (Session::ani_shown,
   its animation's), which ani_data.cpp fills; what the client got of them
   is ani_data.cpp's own, the client's. */
struct AniShown {
    std::string prims; /* the frame being drawn: its primitives, comma separated */
    std::string frame; /* the last frame finished, as its event; empty before any */
};

typedef void (*AniDataEmit)(std::string_view line);

/* the front end that sends the events; nothing is recorded before this */
void ani_data_init(AniDataEmit emit);

/* {"cmd":"data"} with or without "ani": the last frame s drew goes at the
   end of that command */
void ani_data_subscribe(const Session &s, int on);

/* the end of a command on s: the frame that has not gone yet */
void ani_data_update(const Session &s);

/* another animation was loaded in s (or another model): the frame drawn so
   far is not one of it */
void ani_data_forget(Session &s);

/* a frame of s's animation, primitive by primitive */
void ani_data_begin(Session &s);
void ani_data_line(Session &s, double u1, double v1, double u2, double v2, int color, int thick);
/* corners in either order */
void ani_data_rect(Session &s, double u1, double v1, double u2, double v2, int color, int thick, int fill);
/* radius r of the .ani along u (r / (xhi-xlo)) and along v (r / (yhi-ylo)) */
void ani_data_circle(Session &s, double u, double v, double ru, double rv, int color, int thick, int fill);
void ani_data_ellipse(Session &s, double u, double v, double ru, double rv, int color, int thick, int fill);
/* a filled circle of r pixels */
void ani_data_dot(Session &s, double u, double v, int r, int color);
/* text from its baseline's left end; size 0..4, font 0 roman, 1 symbol */
void ani_data_text(Session &s, double u, double v, std::string_view text, int color, int size, int font);

typedef struct {
    int pos, rows;      /* the stored row drawn (vcr.pos), of how many */
    double t;           /* the time of the frame */
    int speed, skip;    /* ms between frames of Go, rows per step */
    double xlo, ylo, xhi, yhi; /* the dimension box */
    int w, h;           /* the core's pixel size of the window (vcr.wid, vcr.hgt) */
} AniDataFrame;

void ani_data_end(Session &s, const AniDataFrame *f);

} // namespace xpp
#endif
