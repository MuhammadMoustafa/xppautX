/* What a plot window shows on top of its curves as data: the "marks" event
   (marks_data.h, docs/protocol.md "The plot as data").

   Each plot window keeps a record of the marks the core drew in it since
   it was last blanked (the Session's, marks_data.h MarksShown): the equilibria Sing pts marked (their points, in
   the order drawn, each once), and the slots of the labels, graphic
   objects and frozen curves drawn (a label with the text it showed, a
   frozen curve with the generation of its slot). At the end of a command
   the record becomes the window's content, reading each slot still in use
   for this window; a window whose content differs from what the client
   got last gets its event. */
#include <new>
#include "xpp_mem.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "marks_data.h"
#include "session.h"
#include "json_number.h"
#include "series_enc.h"
#include "xpp_io.h"
#include "many_pops.h"
#include "graf_par.h"

namespace xpp {

namespace {

/* equilibria kept per window at most (Sing pts/Monte Carlo can mark many) */
const std::size_t EQ_MAX = 10000;

/* the markers' shapes (grobs.h's object types: MARKER plus the shape) */
const char *const MARKER_SHAPES[] = {"box", "diamond", "triangle", "plus", "cross", "circle"};
static_assert(std::size(MARKER_SHAPES) == MARKER_SHAPE_COUNT);

struct Label {
    float x, y;
    std::string text;
    int size, font;
    bool operator==(const Label &o) const
    {
        return x == o.x && y == o.y && text == o.text && size == o.size && font == o.font;
    }
};

struct Grob {
    int type, color;
    float xs, ys, xe, ye;
    double size;
    bool operator==(const Grob &o) const
    {
        return type == o.type && color == o.color && xs == o.xs && ys == o.ys && xe == o.xe && ye == o.ye
               && size == o.size;
    }
};

/* a frozen curve's values stay those of its generation: they are read
   from frozen_curves.curve[slot] when sent */
struct Frozen {
    int slot;
    unsigned long gen;
    int color, len;
    std::string key, name;
    bool operator==(const Frozen &o) const
    {
        return slot == o.slot && gen == o.gen && color == o.color && len == o.len && key == o.key
               && name == o.name;
    }
};

/* what a window's event says */
struct Content {
    std::vector<MarksShown::Equilibrium> eqs;
    std::vector<Label> labels;
    std::vector<Grob> grobs;
    std::vector<Frozen> frozen;
    bool operator==(const Content &o) const
    {
        return eqs == o.eqs && labels == o.labels && grobs == o.grobs && frozen == o.frozen;
    }
};

/* The protocol client's: whether it subscribed, and what each window's
   last event it got said. It outlives a load, as the client does. */
struct ClientWindow {
    Content sent;
    bool valid = false; /* the client got `sent` */
};
struct MarksClient {
    MarksDataEmit emit_line = nullptr;
    bool marks_on = false, values_f32 = false;
    std::array<ClientWindow, MAXPOP> windows;
};
MarksClient client;

/* the plot window drawn into as w, or -1 */
int pop_of(const XppPlotWindows &pw, XppWinId w)
{
    for (int i = 0; i < MAXPOP; i++)
        if (pw.graph[i].Use && pw.graph[i].w == w) return i;
    return -1;
}

MarksShown::Record *record_of(Session &s, XppWinId w)
{
    if (!client.emit_line) return nullptr;
    const int pop = pop_of(s.plot_windows, w);
    return pop < 0 ? nullptr : &s.marks_shown.windows[pop];
}

/* ---- JSON text ---- */

void add_int(std::string &o, long v) { o += std::to_string(v); }

/* a stored float: 9 digits read back as exactly it */
void add_float(std::string &o, float v) { xpp::json::json_append_number(o, v, 9); }

void add_values(std::string &o, const float *v, int n)
{
    xpp_series_append(o, v, n > 0 ? n : 0, client.values_f32);
}

const char *eq_type(int symbol) { return symbol == 3 ? "stable" : symbol == 1 ? "saddle" : "unstable"; }
const char *eq_symbol(int symbol) { return symbol == 3 ? "circle" : symbol == 1 ? "triangle" : "box"; }

void send_marks(const xpp::Session &s, int pop, const Content &c)
{
    std::string o = "{\"ev\":\"marks\",\"win\":";
    add_int(o, static_cast<long>(s.plot_windows.graph[pop].w));
    if (client.values_f32) o += ",\"enc\":\"f32\"";
    o += ",\"equilibria\":[";
    for (std::size_t k = 0; k < c.eqs.size(); k++) {
        o += k ? ",{\"x\":" : "{\"x\":";
        xpp::json::json_append_number_shortest(o, c.eqs[k].x);
        o += ",\"y\":";
        xpp::json::json_append_number_shortest(o, c.eqs[k].y);
        o += ",\"type\":\"";
        o += eq_type(c.eqs[k].symbol);
        o += "\",\"symbol\":\"";
        o += eq_symbol(c.eqs[k].symbol);
        o += "\"}";
    }
    o += "],\"text\":[";
    for (std::size_t k = 0; k < c.labels.size(); k++) {
        const Label &l = c.labels[k];
        o += k ? ",{\"x\":" : "{\"x\":";
        add_float(o, l.x);
        o += ",\"y\":";
        add_float(o, l.y);
        o += ",\"text\":";
        xpp::json_append_string(o, l.text.c_str());
        o += ",\"size\":";
        add_int(o, l.size);
        o += ",\"font\":";
        add_int(o, l.font);
        o += '}';
    }
    o += "],\"arrows\":[";
    bool first = true;
    for (const Grob &g : c.grobs) {
        if (g.type != ARROW && g.type != POINTER) continue;
        o += first ? "{\"kind\":\"" : ",{\"kind\":\"";
        first = false;
        o += g.type == ARROW ? "arrow" : "pointer";
        o += "\",\"x1\":";
        add_float(o, g.xs);
        o += ",\"y1\":";
        add_float(o, g.ys);
        o += ",\"x2\":";
        add_float(o, g.xe);
        o += ",\"y2\":";
        add_float(o, g.ye);
        o += ",\"size\":";
        xpp::json::json_append_number_shortest(o, g.size);
        o += ",\"color\":";
        add_int(o, g.color);
        o += '}';
    }
    o += "],\"markers\":[";
    first = true;
    for (const Grob &g : c.grobs) {
        if (g.type < MARKER) continue;
        const int shape = g.type - MARKER;
        o += first ? "{\"x\":" : ",{\"x\":";
        first = false;
        add_float(o, g.xs);
        o += ",\"y\":";
        add_float(o, g.ys);
        o += ",\"shape\":\"";
        o += MARKER_SHAPES[shape < MARKER_SHAPE_COUNT ? shape : 0];
        o += "\",\"size\":";
        xpp::json::json_append_number_shortest(o, g.size);
        o += ",\"color\":";
        add_int(o, g.color);
        o += '}';
    }
    o += "],\"frozen\":[";
    for (std::size_t k = 0; k < c.frozen.size(); k++) {
        const Frozen &f = c.frozen[k];
        const CURVE &z = s.frozen_curves.curve[f.slot];
        o += k ? ",{\"key\":" : "{\"key\":";
        xpp::json_append_string(o, f.key.c_str());
        o += ",\"name\":";
        xpp::json_append_string(o, f.name.c_str());
        o += ",\"color\":";
        add_int(o, f.color < 0 ? -f.color : f.color);
        o += ",\"line\":";
        o += f.color < 0 ? '0' : '1';
        o += ",\"x\":";
        add_values(o, z.xv, f.len);
        o += ",\"y\":";
        add_values(o, z.yv, f.len);
        o += '}';
    }
    o += "]}";
    client.emit_line(o);
}

/* the window's record as it stands, each slot read from where it is kept */
Content content_of(const xpp::Session &s, int pop, const MarksShown::Record &r)
{
    const XppWinId w = s.plot_windows.graph[pop].w;
    Content c;
    c.eqs = r.eqs;
    for (const auto &e : r.labels) {
        const LABEL &l = s.labels[e.first];
        if (l.use == 1 && l.w == w) c.labels.push_back({l.x, l.y, e.second, l.size, l.font});
    }
    for (std::size_t i = 0; i < r.grobs.size(); i++) {
        const GROB &g = s.grobs[i];
        if (r.grobs[i] && g.use == 1 && g.w == w) c.grobs.push_back({g.type, g.color, g.xs, g.ys, g.xe, g.ye, g.size});
    }
    for (const auto &e : r.frozen) {
        const CURVE &z = s.frozen_curves.curve[e.first];
        if (z.use == 1 && z.w == w && z.type == 0 && e.second == s.marks_shown.generation[e.first] && z.xv && z.yv)
            c.frozen.push_back({e.first, e.second, z.color, z.len, z.key, z.name});
    }
    return c;
}

void update(xpp::Session &s)
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? s.plot_windows.active : (k == s.plot_windows.active ? 0 : k); /* the active window first */
        ClientWindow &w = client.windows[pop];
        if (!s.plot_windows.graph[pop].Use) {
            s.marks_shown.windows[pop] = MarksShown::Record(); /* a window made again later starts afresh */
            w = ClientWindow();
            continue;
        }
        Content c = content_of(s, pop, s.marks_shown.windows[pop]);
        if (w.valid && c == w.sent) continue;
        send_marks(s, pop, c);
        w.sent = std::move(c);
        w.valid = true;
    }
}

} // namespace

/* ---- the API: no exception leaves it (out of memory drops the record) ---- */

void marks_data_init(MarksDataEmit emit) { client.emit_line = emit; }

void marks_data_subscribe(int on, int f32)
{
    client.marks_on = on != 0;
    client.values_f32 = f32 != 0;
    for (ClientWindow &w : client.windows) w.valid = false; /* the next update sends */
}

void marks_data_update(xpp::Session &s)
{
    if (!client.emit_line || !client.marks_on || s.plot_windows.active < 0 || s.plot_windows.active >= MAXPOP) return;
    try {
        update(s);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the marks");
    }
}

void marks_data_cleared(xpp::Session &s, int pop)
{
    if (!client.emit_line || pop < 0 || pop >= MAXPOP) return;
    s.marks_shown.windows[pop] = MarksShown::Record();
}

void marks_data_equilibrium(xpp::Session &s, double x, double y, int symbol)
{
    const int pop = s.plot_windows.active;
    if (!client.emit_line || pop < 0 || pop >= MAXPOP) return;
    MarksShown::Record &r = s.marks_shown.windows[pop];
    const MarksShown::Equilibrium e{x, y, symbol};
    try {
        for (const MarksShown::Equilibrium &o : r.eqs)
            if (o == e) return; /* marked again: the same picture */
        if (r.eqs.size() < EQ_MAX) r.eqs.push_back(e);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording an equilibrium");
    }
}

void marks_data_label(xpp::Session &s, XppWinId w, int slot, std::string_view text)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r || slot < 0 || slot >= MAXLAB) return;
    try {
        r->labels[slot] = text;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a label");
    }
}

void marks_data_grob(xpp::Session &s, XppWinId w, int slot)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r || slot < 0 || slot >= MAXGROB) return;
    try {
        if (r->grobs.size() != MAXGROB) r->grobs.assign(MAXGROB, false);
        r->grobs[slot] = true;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording an arrow or marker");
    }
}

void marks_data_frozen(xpp::Session &s, XppWinId w, int slot)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r || slot < 0 || slot >= MAXFRZ) return;
    try {
        r->frozen[slot] = s.marks_shown.generation[slot];
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a frozen curve");
    }
}

void marks_data_frozen_new(xpp::Session &s, int slot)
{
    if (!client.emit_line || slot < 0 || slot >= MAXFRZ) return;
    s.marks_shown.generation[slot] = ++s.marks_shown.generations;
    marks_data_frozen(s, s.frozen_curves.curve[slot].w, slot);
}

} // namespace xpp
