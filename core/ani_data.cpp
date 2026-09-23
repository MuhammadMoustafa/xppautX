/* The animation's frames as data: the "ani" "frame" event (ani_data.h,
   docs/protocol.md "The animation as data").

   aniparse.cpp reports the primitives of each frame it draws; the frame
   finished last is kept as JSON text and goes to a subscribed client at
   most 25 times a second, the last one always (at the end of the command
   when the rate held it back). */
#include <chrono>
#include <cstdio>
#include <string>

#include "ani_data.h"
#include "colormap.h"
#include "xpp_globals.h"

namespace {

AniDataEmit emit_line;
bool subscribed;

std::string prims;   /* the frame being drawn: its primitives, comma separated */
std::string frame;   /* the last frame finished, as its event; empty before any */
bool unsent;         /* frame has not gone to the client */
std::chrono::steady_clock::time_point last_sent;
bool sent_once;

/* frames of Go and Fly go at most this often: 25 a second */
const std::chrono::milliseconds MIN_GAP(40);

/* unit coordinates: 6 digits are a millionth of the picture, far under a pixel */
void add_num(std::string &o, double v)
{
    char t[32];
    if (v != v || v > 1e300 || v < -1e300) {
        o += "null";
        return;
    }
    std::snprintf(t, sizeof t, "%.6g", v);
    o += t;
}

void add_int(std::string &o, long v) { o += std::to_string(v); }

void add_str(std::string &o, const char *s)
{
    char esc[8];
    o += '"';
    for (; s && *s; s++) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x80) {
            /* the core's strings are ASCII or Latin-1; keep the byte value */
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            o += esc;
        } else
            o += static_cast<char>(c);
    }
    o += '"';
}

/* a palette index as the event says it: an XPP colour index (0 the
   foreground, 1..10 red .. purple) for the named colours, the colour
   map's colour as #rrggbb for the others */
void add_color(std::string &o, int icol)
{
    char t[16];
    if (icol >= 20 && icol <= 29) {
        add_int(o, icol - 19);
        return;
    }
    if (icol < 30 || icol >= XPP_MAX_COLORS) {
        o += '0';
        return;
    }
    std::snprintf(t, sizeof t, "\"#%02x%02x%02x\"", xpp_cmap_rgb[icol][0] >> 8, xpp_cmap_rgb[icol][1] >> 8,
                  xpp_cmap_rgb[icol][2] >> 8);
    o += t;
}

/* ["kind", ... */
void open_prim(const char *kind)
{
    if (!emit_line) return;
    if (!prims.empty()) prims += ',';
    prims += "[\"";
    prims += kind;
    prims += '"';
}

void num(double v)
{
    prims += ',';
    add_num(prims, v);
}

void integer(long v)
{
    prims += ',';
    add_int(prims, v);
}

void color(int icol)
{
    prims += ',';
    add_color(prims, icol);
}

void send_frame()
{
    if (!emit_line || frame.empty()) return;
    emit_line(frame.data(), frame.size());
    unsent = false;
    last_sent = std::chrono::steady_clock::now();
    sent_once = true;
}

} // namespace

extern "C" void ani_data_init(AniDataEmit emit) { emit_line = emit; }

extern "C" void ani_data_subscribe(int on)
{
    subscribed = on != 0;
    unsent = subscribed && !frame.empty();
}

extern "C" void ani_data_update(void)
{
    if (subscribed && unsent) send_frame();
}

extern "C" void ani_data_forget(void)
{
    frame.clear();
    prims.clear();
    unsent = false;
}

extern "C" void ani_data_begin(void) { prims.clear(); }

extern "C" void ani_data_line(double u1, double v1, double u2, double v2, int c, int thick)
{
    if (!emit_line) return;
    open_prim("line");
    num(u1);
    num(v1);
    num(u2);
    num(v2);
    color(c);
    integer(thick);
    prims += ']';
}

static void box_prim(const char *kind, double a, double b, double c, double d, int col, int thick, int fill)
{
    if (!emit_line) return;
    open_prim(kind);
    num(a);
    num(b);
    num(c);
    num(d);
    color(col);
    integer(thick);
    integer(fill ? 1 : 0);
    prims += ']';
}

extern "C" void ani_data_rect(double u1, double v1, double u2, double v2, int c, int thick, int fill)
{
    box_prim("rect", u1, v1, u2, v2, c, thick, fill);
}

extern "C" void ani_data_circle(double u, double v, double ru, double rv, int c, int thick, int fill)
{
    box_prim("circle", u, v, ru, rv, c, thick, fill);
}

extern "C" void ani_data_ellipse(double u, double v, double ru, double rv, int c, int thick, int fill)
{
    box_prim("ellipse", u, v, ru, rv, c, thick, fill);
}

extern "C" void ani_data_dot(double u, double v, int r, int c)
{
    if (!emit_line) return;
    open_prim("dot");
    num(u);
    num(v);
    integer(r);
    color(c);
    prims += ']';
}

extern "C" void ani_data_text(double u, double v, const char *s, int c, int size, int font)
{
    if (!emit_line) return;
    open_prim("text");
    num(u);
    num(v);
    prims += ',';
    add_str(prims, s);
    color(c);
    integer(size);
    integer(font);
    prims += ']';
}

extern "C" void ani_data_end(const AniDataFrame *f)
{
    if (!emit_line) return;
    std::string &o = frame;
    o.clear();
    o += "{\"ev\":\"ani\",\"op\":\"frame\",\"pos\":";
    add_int(o, f->pos);
    o += ",\"rows\":";
    add_int(o, f->rows);
    o += ",\"t\":";
    char t[32];
    if (f->t == f->t) {
        std::snprintf(t, sizeof t, "%.9g", f->t);
        o += t;
    } else
        o += "null";
    o += ",\"speed\":";
    add_int(o, f->speed);
    o += ",\"skip\":";
    add_int(o, f->skip);
    o += ",\"dim\":[";
    add_num(o, f->xlo);
    o += ',';
    add_num(o, f->ylo);
    o += ',';
    add_num(o, f->xhi);
    o += ',';
    add_num(o, f->yhi);
    o += "],\"w\":";
    add_int(o, f->w);
    o += ",\"h\":";
    add_int(o, f->h);
    o += ",\"prims\":[";
    o += prims;
    o += "]}";
    prims.clear();
    unsent = true;
    if (subscribed && (!sent_once || std::chrono::steady_clock::now() - last_sent >= MIN_GAP)) send_frame();
}
