/* What a plot window shows on top of its curves as data: the "marks" event
   (marks_data.h, docs/protocol.md "The plot as data").

   Each plot window keeps a record of the marks the core drew in it since
   it was last blanked: the equilibria Sing pts marked (their points, in
   the order drawn, each once), and the slots of the labels, graphic
   objects and frozen curves drawn (a label with the text it showed, a
   frozen curve with the generation of its slot). At the end of a command
   the record becomes the window's content, reading each slot still in use
   for this window; a window whose content differs from what the client
   got last gets its event. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "marks_data.h"
#include "grobs.h"
#include "series_enc.h"
#include "xpp_globals.h"
#include "xpp_mem.h"
#include "many_pops.h"
#include "graf_par.h"

namespace {

MarksDataEmit emit_line;
bool marks_on, values_f32;

/* equilibria kept per window at most (Sing pts/Monte Carlo can mark many) */
const std::size_t EQ_MAX = 10000;

/* grobs.cpp's object types: 0 pointer, 1 arrow, 2.. markers */
const int POINTER = 0, ARROW = 1, MARKER = 2;
const char *const MARKER_SHAPES[] = {"box", "diamond", "triangle", "plus", "cross", "circle"};
const int N_SHAPES = static_cast<int>(sizeof MARKER_SHAPES / sizeof MARKER_SHAPES[0]);

struct Equilibrium {
    double x, y;
    int symbol; /* eq_symb's: 0 box, 1 triangle, 3 circle */
    bool operator==(const Equilibrium &o) const
    {
        return std::memcmp(&x, &o.x, sizeof x) == 0 && std::memcmp(&y, &o.y, sizeof y) == 0 && symbol == o.symbol;
    }
};

/* what was drawn since the window was blanked */
struct Record {
    std::vector<Equilibrium> eqs;
    std::map<int, std::string> labels;    /* lb[] slot -> the text drawn */
    std::vector<bool> grobs;              /* grob[] slots drawn */
    std::map<int, unsigned long> frozen;  /* frozen_curves.curve[] slot -> its generation */
};

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
    std::vector<Equilibrium> eqs;
    std::vector<Label> labels;
    std::vector<Grob> grobs;
    std::vector<Frozen> frozen;
    bool operator==(const Content &o) const
    {
        return eqs == o.eqs && labels == o.labels && grobs == o.grobs && frozen == o.frozen;
    }
};

struct Window {
    Record rec;
    Content sent;
    bool valid = false; /* the client got `sent` */
};

Window windows[MAXPOP];
unsigned long generation[MAXFRZ]; /* bumped when frozen_curves.curve[slot] is made */
unsigned long generations;

/* the plot window drawn into as w, or -1 */
int pop_of(XppWinId w)
{
    for (int i = 0; i < MAXPOP; i++)
        if (plot_windows.graph[i].Use && plot_windows.graph[i].w == w) return i;
    return -1;
}

Record *record_of(XppWinId w)
{
    if (!emit_line) return nullptr;
    const int pop = pop_of(w);
    return pop < 0 ? nullptr : &windows[pop].rec;
}

/* ---- JSON text ---- */

void add_int(std::string &o, long v) { o += std::to_string(v); }

/* the shortest of 15 or 17 digits that reads back as v; null when not finite */
void add_num(std::string &o, double v)
{
    char t[32];
    if (!std::isfinite(v)) {
        o += "null";
        return;
    }
    std::snprintf(t, sizeof t, "%.15g", v);
    if (std::strtod(t, nullptr) != v) std::snprintf(t, sizeof t, "%.17g", v);
    o += t;
}

/* a stored float: 9 digits read back as exactly it */
void add_float(std::string &o, float v)
{
    char t[32];
    if (!std::isfinite(v)) {
        o += "null";
        return;
    }
    std::snprintf(t, sizeof t, "%.9g", static_cast<double>(v));
    o += t;
}

/* the length of the UTF-8 sequence at s (valid, shortest form), else 0 */
int utf8_length(const unsigned char *s)
{
    int n;
    unsigned int c = s[0];
    if (c >= 0xc2 && c <= 0xdf) n = 2;
    else if (c >= 0xe0 && c <= 0xef) n = 3;
    else if (c >= 0xf0 && c <= 0xf4) n = 4;
    else return 0;
    for (int k = 1; k < n; k++)
        if ((s[k] & 0xc0) != 0x80) return 0;
    if (c == 0xe0 && s[1] < 0xa0) return 0; /* overlong */
    if (c == 0xed && s[1] >= 0xa0) return 0; /* a surrogate */
    if (c == 0xf0 && s[1] < 0x90) return 0; /* overlong */
    if (c == 0xf4 && s[1] >= 0x90) return 0; /* past U+10FFFF */
    return n;
}

/* a label's text: UTF-8 the user typed stays as it is, other bytes are
   Latin-1 (the core's strings); backslashes (XPP's font escapes) escaped */
void add_text(std::string &o, const char *s)
{
    char esc[8];
    o += '"';
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p;) {
        const unsigned char c = *p;
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c < 0x20) {
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            o += esc;
        } else if (c >= 0x80) {
            const int n = utf8_length(p);
            if (n) {
                o.append(reinterpret_cast<const char *>(p), n);
                p += n;
                continue;
            }
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            o += esc;
        } else o += static_cast<char>(c);
        p++;
    }
    o += '"';
}

void add_values(std::string &o, const float *v, int n)
{
    std::size_t len;
    char *t = xpp_series_values(v, n > 0 ? n : 0, values_f32, &len);
    if (t) {
        o.append(t, len);
        xpp_free(t);
    } else o += "[]";
}

const char *eq_type(int symbol) { return symbol == 3 ? "stable" : symbol == 1 ? "saddle" : "unstable"; }
const char *eq_symbol(int symbol) { return symbol == 3 ? "circle" : symbol == 1 ? "triangle" : "box"; }

void send_marks(int pop, const Content &c)
{
    std::string o = "{\"ev\":\"marks\",\"win\":";
    add_int(o, static_cast<long>(plot_windows.graph[pop].w));
    if (values_f32) o += ",\"enc\":\"f32\"";
    o += ",\"equilibria\":[";
    for (std::size_t k = 0; k < c.eqs.size(); k++) {
        o += k ? ",{\"x\":" : "{\"x\":";
        add_num(o, c.eqs[k].x);
        o += ",\"y\":";
        add_num(o, c.eqs[k].y);
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
        add_text(o, l.text.c_str());
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
        add_num(o, g.size);
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
        o += MARKER_SHAPES[shape < N_SHAPES ? shape : 0];
        o += "\",\"size\":";
        add_num(o, g.size);
        o += ",\"color\":";
        add_int(o, g.color);
        o += '}';
    }
    o += "],\"frozen\":[";
    for (std::size_t k = 0; k < c.frozen.size(); k++) {
        const Frozen &f = c.frozen[k];
        const CURVE &z = frozen_curves.curve[f.slot];
        o += k ? ",{\"key\":" : "{\"key\":";
        add_text(o, f.key.c_str());
        o += ",\"name\":";
        add_text(o, f.name.c_str());
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
    emit_line(o.data(), o.size());
}

/* the window's record as it stands, each slot read from where it is kept */
Content content_of(int pop, const Record &r)
{
    const XppWinId w = plot_windows.graph[pop].w;
    Content c;
    c.eqs = r.eqs;
    for (const auto &e : r.labels) {
        const LABEL &l = lb[e.first];
        if (l.use == 1 && l.w == w) c.labels.push_back({l.x, l.y, e.second, l.size, l.font});
    }
    for (std::size_t i = 0; i < r.grobs.size(); i++) {
        const GROB &g = grob[i];
        if (r.grobs[i] && g.use == 1 && g.w == w) c.grobs.push_back({g.type, g.color, g.xs, g.ys, g.xe, g.ye, g.size});
    }
    for (const auto &e : r.frozen) {
        const CURVE &z = frozen_curves.curve[e.first];
        if (z.use == 1 && z.w == w && z.type == 0 && e.second == generation[e.first] && z.xv && z.yv)
            c.frozen.push_back({e.first, e.second, z.color, z.len, z.key, z.name});
    }
    return c;
}

void update()
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? plot_windows.active : (k == plot_windows.active ? 0 : k); /* the active window first */
        Window &w = windows[pop];
        if (!plot_windows.graph[pop].Use) {
            w = Window(); /* a window made again later starts afresh */
            continue;
        }
        Content c = content_of(pop, w.rec);
        if (w.valid && c == w.sent) continue;
        send_marks(pop, c);
        w.sent = std::move(c);
        w.valid = true;
    }
}

} // namespace

/* ---- the C API: no exception leaves it (out of memory drops the record) ---- */

extern "C" void marks_data_init(MarksDataEmit emit) { emit_line = emit; }

extern "C" void marks_data_subscribe(int on, int f32)
{
    marks_on = on != 0;
    values_f32 = f32 != 0;
    for (Window &w : windows) w.valid = false; /* the next update sends */
}

extern "C" void marks_data_update(void)
{
    if (!emit_line || !marks_on || plot_windows.active < 0 || plot_windows.active >= MAXPOP) return;
    try {
        update();
    } catch (...) {
    }
}

extern "C" void marks_data_cleared(int pop)
{
    if (!emit_line || pop < 0 || pop >= MAXPOP) return;
    windows[pop].rec = Record();
}

extern "C" void marks_data_equilibrium(double x, double y, int symbol)
{
    if (!emit_line || plot_windows.active < 0 || plot_windows.active >= MAXPOP) return;
    Record &r = windows[plot_windows.active].rec;
    const Equilibrium e{x, y, symbol};
    try {
        for (const Equilibrium &o : r.eqs)
            if (o == e) return; /* marked again: the same picture */
        if (r.eqs.size() < EQ_MAX) r.eqs.push_back(e);
    } catch (...) {
        r.eqs.clear();
    }
}

extern "C" void marks_data_label(XppWinId w, int slot, const char *text)
{
    Record *r = record_of(w);
    if (!r || slot < 0 || slot >= MAXLAB) return;
    try {
        r->labels[slot] = text ? text : "";
    } catch (...) {
        r->labels.erase(slot);
    }
}

extern "C" void marks_data_grob(XppWinId w, int slot)
{
    Record *r = record_of(w);
    if (!r || slot < 0 || slot >= MAXGROB) return;
    try {
        if (r->grobs.size() != MAXGROB) r->grobs.assign(MAXGROB, false);
        r->grobs[slot] = true;
    } catch (...) {
    }
}

extern "C" void marks_data_frozen(XppWinId w, int slot)
{
    Record *r = record_of(w);
    if (!r || slot < 0 || slot >= MAXFRZ) return;
    try {
        r->frozen[slot] = generation[slot];
    } catch (...) {
    }
}

extern "C" void marks_data_frozen_new(int slot)
{
    if (!emit_line || slot < 0 || slot >= MAXFRZ) return;
    generation[slot] = ++generations;
    marks_data_frozen(frozen_curves.curve[slot].w, slot);
}
