/* The animation's frames as data: the "ani" "frame" event (ani_data.h,
   docs/protocol.md "The animation as data").

   aniparse.cpp reports the primitives of each frame it draws; the frame
   finished last is kept as JSON text (the Session's, ani_data.h AniShown)
   and goes to a subscribed client at most 25 times a second, the last one
   always (at the end of the command when the rate held it back). */
#include <chrono>
#include <string>

#include "ani_data.h"
#include "session.h"
#include "json_number.h"
#include "xpp_io.h"
#include "colormap.h"

namespace xpp {

namespace {

/* The protocol client's: whether it subscribed, and whether and when it
   got the last frame. It outlives a load, as the client does. */
struct AniClient {
    AniDataEmit emit_line = nullptr;
    bool subscribed = false;
    bool unsent = false; /* the Session's frame has not gone to the client */
    std::chrono::steady_clock::time_point last_sent;
    bool sent_once = false;
};
AniClient client;

/* frames of Go and Fly go at most this often: 25 a second */
const std::chrono::milliseconds MIN_GAP(40);

/* unit coordinates: 6 digits are a millionth of the picture, far under a pixel */
void add_num(std::string &o, double v)
{
    if (v > 1e300 || v < -1e300) {
        o += "null";
        return;
    }
    xpp::json::json_append_number(o, v, 6);
}

void add_int(std::string &o, long v) { o += std::to_string(v); }

/* a palette index as the event says it: an XPP colour index (0 the
   foreground, 1..10 red .. purple) for the named colours, the colour
   map's colour as #rrggbb for the others */
void add_color(std::string &o, int icol)
{
    if (icol >= 20 && icol <= 29) {
        add_int(o, icol - 19);
        return;
    }
    if (icol < 30 || icol >= XPP_MAX_COLORS) {
        o += '0';
        return;
    }
    o += xpp::format("\"#{:02x}{:02x}{:02x}\"", cmap_rgb[icol][0] >> 8, cmap_rgb[icol][1] >> 8,
                     cmap_rgb[icol][2] >> 8);
}

/* ["kind", ... */
void open_prim(std::string &prims, const char *kind)
{
    if (!prims.empty()) prims += ',';
    prims += "[\"";
    prims += kind;
    prims += '"';
}

void num(std::string &prims, double v)
{
    prims += ',';
    add_num(prims, v);
}

void integer(std::string &prims, long v)
{
    prims += ',';
    add_int(prims, v);
}

void color(std::string &prims, int icol)
{
    prims += ',';
    add_color(prims, icol);
}

void send_frame(const std::string &frame)
{
    if (!client.emit_line || frame.empty()) return;
    client.emit_line(frame);
    client.unsent = false;
    client.last_sent = std::chrono::steady_clock::now();
    client.sent_once = true;
}

} // namespace

void ani_data_init(AniDataEmit emit) { client.emit_line = emit; }

void ani_data_subscribe(const xpp::Session &s, int on)
{
    client.subscribed = on != 0;
    client.unsent = client.subscribed && !s.ani_shown.frame.empty();
}

void ani_data_update(const xpp::Session &s)
{
    if (client.subscribed && client.unsent) send_frame(s.ani_shown.frame);
}

void ani_data_forget(xpp::Session &s)
{
    s.ani_shown.frame.clear();
    s.ani_shown.prims.clear();
    client.unsent = false;
}

void ani_data_begin(xpp::Session &s) { s.ani_shown.prims.clear(); }

void ani_data_line(xpp::Session &s, double u1, double v1, double u2, double v2, int c, int thick)
{
    if (!client.emit_line) return;
    std::string &prims = s.ani_shown.prims;
    open_prim(prims, "line");
    num(prims, u1);
    num(prims, v1);
    num(prims, u2);
    num(prims, v2);
    color(prims, c);
    integer(prims, thick);
    prims += ']';
}

static void box_prim(xpp::Session &s, const char *kind, double a, double b, double c, double d, int col, int thick,
                     int fill)
{
    if (!client.emit_line) return;
    std::string &prims = s.ani_shown.prims;
    open_prim(prims, kind);
    num(prims, a);
    num(prims, b);
    num(prims, c);
    num(prims, d);
    color(prims, col);
    integer(prims, thick);
    integer(prims, fill ? 1 : 0);
    prims += ']';
}

void ani_data_rect(xpp::Session &s, double u1, double v1, double u2, double v2, int c, int thick, int fill)
{
    box_prim(s, "rect", u1, v1, u2, v2, c, thick, fill);
}

void ani_data_circle(xpp::Session &s, double u, double v, double ru, double rv, int c, int thick, int fill)
{
    box_prim(s, "circle", u, v, ru, rv, c, thick, fill);
}

void ani_data_ellipse(xpp::Session &s, double u, double v, double ru, double rv, int c, int thick, int fill)
{
    box_prim(s, "ellipse", u, v, ru, rv, c, thick, fill);
}

void ani_data_dot(xpp::Session &s, double u, double v, int r, int c)
{
    if (!client.emit_line) return;
    std::string &prims = s.ani_shown.prims;
    open_prim(prims, "dot");
    num(prims, u);
    num(prims, v);
    integer(prims, r);
    color(prims, c);
    prims += ']';
}

void ani_data_text(xpp::Session &s, double u, double v, std::string_view text, int c, int size, int font)
{
    if (!client.emit_line) return;
    std::string &prims = s.ani_shown.prims;
    open_prim(prims, "text");
    num(prims, u);
    num(prims, v);
    prims += ',';
    xpp::json_append_string(prims, text);
    color(prims, c);
    integer(prims, size);
    integer(prims, font);
    prims += ']';
}

void ani_data_end(xpp::Session &s, const AniDataFrame *f)
{
    if (!client.emit_line) return;
    std::string &prims = s.ani_shown.prims;
    std::string &o = s.ani_shown.frame;
    o.clear();
    o += "{\"ev\":\"ani\",\"op\":\"frame\",\"pos\":";
    add_int(o, f->pos);
    o += ",\"rows\":";
    add_int(o, f->rows);
    o += ",\"t\":";
    xpp::json::json_append_number(o, f->t, 9);
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
    client.unsent = true;
    if (client.subscribed && (!client.sent_once || std::chrono::steady_clock::now() - client.last_sent >= MIN_GAP))
        send_frame(o);
}

} // namespace xpp
