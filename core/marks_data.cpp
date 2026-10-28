/* What a plot window shows on top of its curves as data: the "marks" event
   (marks_data.h, docs/protocol.md "The plot as data").

   Each plot window keeps a record of the marks the core drew in it since
   it was last blanked (the Session's, marks_data.h MarksShown): the equilibria Sing pts marked (their points, in
   the order drawn, each once), and the ids of the labels, graphic
   objects and frozen curves drawn (a label with the text it showed, a
   frozen curve with the generation of its slot). At the end of a command
   the record becomes the window's content, reading each one still in use
   for this window; a window whose content differs from what the client
   got last gets its event. */
#include <new>
#include "xpp_mem.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "marks_data.h"
#include "grobs.h"
#include "session.h"
#include "xpp_util.h"
#include "json_number.h"
#include "series_enc.h"
#include "xpp_io.h"
#include "many_pops.h"
#include "graf_par.h"

namespace xpp {

namespace {

/* equilibria kept per window at most (Sing pts/Monte Carlo can mark many) */
const std::size_t EQ_MAX = 10000;


struct Label {
    int id; /* the annotation's id: the page names the label by it */
    float x, y;
    std::string text, source; /* as drawn, and as written (its \{expr}s unfilled) */
    int size, font;
    TextStyle style;
    Color color;
    bool operator==(const Label &o) const
    {
        return id == o.id && x == o.x && y == o.y && text == o.text && source == o.source && size == o.size && font == o.font
               && style == o.style && color == o.color;
    }
};

struct Grob {
    int id;
    int type;
    Color color;
    float xs, ys, xe, ye;
    double size;
    bool operator==(const Grob &o) const
    {
        return id == o.id && type == o.type && color == o.color && xs == o.xs && ys == o.ys && xe == o.xe && ye == o.ye
               && size == o.size;
    }
};

/* a frozen curve's values stay those of its generation: they are read
   from frozen_curves.curve[slot] when sent */
struct Frozen {
    int slot;
    unsigned long gen;
    Color color;
    LineStyle style;
    int len;
    std::string key, name;
    bool operator==(const Frozen &o) const
    {
        return slot == o.slot && gen == o.gen && color == o.color && style == o.style && len == o.len && key == o.key
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
    std::vector<ClientWindow> windows;
};
MarksClient client;

MarksShown::Record *record_of(Session &s, XppWinId w)
{
    if (!client.emit_line) return nullptr;
    const int pop = graph_of(s,w);
    return s.plot_windows.graph[pop].Use && s.plot_windows.graph[pop].w==w ? &s.marks_shown.windows[pop] : nullptr;
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

/* the "text", "arrows" and "markers" members: the texts, then the arrows and pointers, then the markers */
void append_annotations(std::string &o, const std::vector<Label> &labels, const std::vector<Grob> &grobs)
{
    o += "\"text\":[";
    for (std::size_t k = 0; k < labels.size(); k++) {
        const Label &l = labels[k];
        o += k ? ",{\"id\":" : "{\"id\":";
        add_int(o, l.id);
        o += ",\"x\":";
        add_float(o, l.x);
        o += ",\"y\":";
        add_float(o, l.y);
        o += ",\"text\":";
        xpp::json_append_string(o, l.text.c_str());
        o += ",\"source\":";
        xpp::json_append_string(o, l.source.c_str());
        o += ",\"size\":";
        add_int(o, l.size);
        o += ",\"font\":";
        add_int(o, l.font);
        o += ",\"style\":";
        xpp::json_append_string(o, xpp::text_style_word(l.style));
        o += ",\"color\":";
        o += xpp::quoted_color(l.color);
        o += '}';
    }
    o += "],\"arrows\":[";
    bool first = true;
    for (const Grob &g : grobs) {
        if (g.type != ARROW && g.type != POINTER) continue;
        o += first ? "{\"id\":" : ",{\"id\":";
        first = false;
        add_int(o, g.id);
        o += ",\"kind\":\"";
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
        o += xpp::quoted_color(g.color);
        o += '}';
    }
    o += "],\"markers\":[";
    first = true;
    for (const Grob &g : grobs) {
        if (g.type < MARKER) continue;
        const int shape = g.type - MARKER;
        o += first ? "{\"id\":" : ",{\"id\":";
        first = false;
        add_int(o, g.id);
        o += ",\"x\":";
        add_float(o, g.xs);
        o += ",\"y\":";
        add_float(o, g.ys);
        o += ",\"shape\":\"";
        o += marker_shape_names[shape < MARKER_SHAPE_COUNT ? shape : 0];
        o += "\",\"size\":";
        xpp::json::json_append_number_shortest(o, g.size);
        o += ",\"color\":";
        o += xpp::quoted_color(g.color);
        o += '}';
    }
    o += ']';
}

void send_marks(const xpp::Session &s, int pop, const Content &c)
{
    std::string o = "{\"ev\":\"marks\",\"win\":";
    xpp::json_append_string(o,xpp::window_name(s,s.plot_windows.graph[pop].w));
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
    o += "],";
    append_annotations(o, c.labels, c.grobs);
    o += ",\"frozen\":[";
    for (std::size_t k = 0; k < c.frozen.size(); k++) {
        const Frozen &f = c.frozen[k];
        const CURVE &z = s.frozen_curves.curve[f.slot];
        o += k ? ",{\"key\":" : "{\"key\":";
        xpp::json_append_string(o, f.key.c_str());
        o += ",\"name\":";
        xpp::json_append_string(o, f.name.c_str());
        o += ",\"color\":";
        o += xpp::quoted_color(f.color);
        o += ',';
        o += line_style_json(f.style);
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
        const LABEL *l = find_annotation(s.labels, e.first);
        if (l && l->w == w) c.labels.push_back({e.first, l->x, l->y, e.second, l->s, l->size, l->font, l->style, l->color});
    }
    for (const int id : r.grobs) {
        const GROB *g = find_annotation(s.grobs, id);
        if (g && g->w == w) c.grobs.push_back({id, g->type, g->color, g->xs, g->ys, g->xe, g->ye, g->size});
    }
    for (const auto &e : r.frozen) {
        const CURVE &z = s.frozen_curves.curve[e.first];
        if (z.use == 1 && z.w == w && z.type == 0 && e.second == s.marks_shown.generation[e.first] && z.xv && z.yv)
            c.frozen.push_back({e.first, e.second, z.color, z.style, z.len, z.key, z.name});
    }
    return c;
}

void update(xpp::Session &s)
{
    client.windows.resize(s.plot_windows.graph.size());
    for (int k = 0; k < static_cast<int>(s.plot_windows.graph.size()); k++) {
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
    if (!client.emit_line || !client.marks_on || s.plot_windows.active < 0 || s.plot_windows.active >= static_cast<int>(s.plot_windows.graph.size())) return;
    try {
        update(s);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the marks");
    }
}

void marks_data_cleared(xpp::Session &s, int pop)
{
    if (pop < 0 || pop >= static_cast<int>(s.plot_windows.graph.size())) return;
    s.marks_shown.windows[pop] = MarksShown::Record();
}

void marks_data_equilibrium(xpp::Session &s, double x, double y, int symbol)
{
    const int pop = s.plot_windows.active;
    if (pop < 0 || pop >= static_cast<int>(s.plot_windows.graph.size())) return;
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

void marks_data_label(xpp::Session &s, XppWinId w, int id, std::string_view text)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r) return;
    try {
        r->labels[id] = text;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a label");
    }
}

void marks_data_grob(xpp::Session &s, XppWinId w, int id)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r) return;
    try {
        r->grobs.insert(id);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording an arrow or marker");
    }
}

void marks_data_frozen(xpp::Session &s, XppWinId w, int slot)
{
    MarksShown::Record *r = record_of(s, w);
    if (!r || slot < 0 || slot >= static_cast<int>(s.frozen_curves.curve.size())) return;
    try {
        r->frozen[slot] = s.marks_shown.generation[slot];
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a frozen curve");
    }
}

std::string annotations_json(Session &s, XppWinId w)
{
    std::vector<Label> labels;
    for (const LABEL &l : s.labels)
        if (l.w == w) labels.push_back({l.id, l.x, l.y, shown_label_text(s, l), l.s, l.size, l.font, l.style, l.color});
    std::vector<Grob> grobs;
    for (const GROB &g : s.grobs)
        if (g.w == w) grobs.push_back({g.id, g.type, g.color, g.xs, g.ys, g.xe, g.ye, g.size});
    std::string o = "{\"window\":";
    xpp::json_append_string(o,xpp::window_name(s,w));
    o += ',';
    append_annotations(o, labels, grobs);
    return o + '}';
}

void marks_data_frozen_new(xpp::Session &s, int slot)
{
    if (!client.emit_line || slot < 0 || slot >= static_cast<int>(s.frozen_curves.curve.size())) return;
    s.marks_shown.generation[slot] = ++s.marks_shown.generations;
    marks_data_frozen(s, s.frozen_curves.curve[slot].w, slot);
}

} // namespace xpp
