#ifndef ANI_DATA_H
#define ANI_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

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

   ani_data.cpp; C++ with a C API, nothing escapes it. */

typedef void (*AniDataEmit)(const char *line, size_t len);

/* the front end that sends the events; nothing is recorded before this */
void ani_data_init(AniDataEmit emit);

/* {"cmd":"data"} with or without "ani": the last frame drawn goes at the
   end of that command */
void ani_data_subscribe(int on);

/* the end of a command: the frame that has not gone yet */
void ani_data_update(void);

/* another animation was loaded: the frame drawn so far is not one of it */
void ani_data_forget(void);

/* a frame, primitive by primitive */
void ani_data_begin(void);
void ani_data_line(double u1, double v1, double u2, double v2, int color, int thick);
/* corners in either order */
void ani_data_rect(double u1, double v1, double u2, double v2, int color, int thick, int fill);
/* radius r of the .ani along u (r / (xhi-xlo)) and along v (r / (yhi-ylo)) */
void ani_data_circle(double u, double v, double ru, double rv, int color, int thick, int fill);
void ani_data_ellipse(double u, double v, double ru, double rv, int color, int thick, int fill);
/* a filled circle of r pixels */
void ani_data_dot(double u, double v, int r, int color);
/* text from its baseline's left end; size 0..4, font 0 roman, 1 symbol */
void ani_data_text(double u, double v, const char *s, int color, int size, int font);

typedef struct {
    int pos, rows;      /* the stored row drawn (vcr.pos), of how many */
    double t;           /* the time of the frame */
    int speed, skip;    /* ms between frames of Go, rows per step */
    double xlo, ylo, xhi, yhi; /* the dimension box */
    int w, h;           /* the core's pixel size of the window (vcr.wid, vcr.hgt) */
} AniDataFrame;

void ani_data_end(const AniDataFrame *f);

#ifdef __cplusplus
}
#endif
#endif
