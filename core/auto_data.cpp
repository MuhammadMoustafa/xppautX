/* AUTO's info strip and stability circle as data: the "autoinfo" event
   (auto_data.h, docs/protocol.md "The AUTO diagram as data").

   auto_nox.c reports what the strip and the circle show as it draws them;
   the event is built from that record when the front end asks for an
   update and sent only when its text differs from the last one sent. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "auto_data.h"
#include "xpp_job.h"

namespace {

AutoDataEmit emit_line;
AutoDataPointOf point_of_node;
bool enabled, subscribed;

struct Info {
    AutoDataInfo v{};
    std::string sym, p1name, p2name, vname;
    bool two = false;
};

bool has_info, has_stab, stab_periodic;
Info info;
std::vector<double> stab_re, stab_im;

std::string sent;
bool sent_valid;

/* ---- JSON text (as plot_data.cpp writes it) ---- */

void add_str(std::string &o, const char *s)
{
    char esc[8];
    o += '"';
    for (; s && *s; s++) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20 || c >= 0x80) {
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            o += esc;
        } else o += static_cast<char>(c);
    }
    o += '"';
}

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

void add_field(std::string &o, const char *name, double v)
{
    o += ",\"";
    o += name;
    o += "\":";
    add_num(o, v);
}

void add_pair(std::string &o, double re, double im)
{
    o += '[';
    add_num(o, re);
    o += ',';
    add_num(o, im);
    o += ']';
}

/* a name as the strip shows it, without the blanks get_bif_sym pads with */
std::string trimmed(const char *s)
{
    std::string t = s ? s : "";
    const std::size_t a = t.find_first_not_of(' '), b = t.find_last_not_of(' ');
    return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
}

void add_info(std::string &o)
{
    const AutoDataInfo &v = info.v;
    o += "{\"point\":";
    o += std::to_string(point_of_node ? point_of_node(v.node) : -1);
    o += ",\"br\":" + std::to_string(std::abs(v.ibr)) + ",\"pt\":" + std::to_string(std::abs(v.pt));
    o += ",\"type\":" + std::to_string(v.type) + ",\"sym\":";
    add_str(o, info.sym.c_str());
    o += ",\"lab\":" + std::to_string(v.lab);
    if (v.flag2) o += ",\"f2\":" + std::to_string(v.flag2);
    o += ",\"par\":[{\"name\":";
    add_str(o, info.p1name.c_str());
    o += ",\"value\":";
    add_num(o, v.p1);
    o += '}';
    if (info.two) {
        o += ",{\"name\":";
        add_str(o, info.p2name.c_str());
        o += ",\"value\":";
        add_num(o, v.p2);
        o += '}';
    }
    o += ']';
    add_field(o, "norm", v.norm);
    o += ",\"var\":";
    add_str(o, info.vname.c_str());
    add_field(o, "u", v.u);
    add_field(o, "per", v.per);
    add_field(o, "x", v.x);
    add_field(o, "y", v.y);
    add_field(o, "y2", v.y2);
    o += '}';
}

/* the circle's values; for a steady state the eigenvalues too, lambda =
   log z: exact in its real part while e^lambda is not 0, its imaginary part
   the principal value (XPP keeps e^lambda only, so a frequency is known
   modulo 2 pi) */
void add_stab(std::string &o)
{
    const std::size_t n = stab_re.size();
    o += "{\"periodic\":";
    o += stab_periodic ? '1' : '0';
    o += ",\"circle\":[";
    for (std::size_t i = 0; i < n; i++) {
        if (i) o += ',';
        add_pair(o, stab_re[i], stab_im[i]);
    }
    o += ']';
    if (!stab_periodic) {
        o += ",\"eig\":[";
        for (std::size_t i = 0; i < n; i++) {
            const double r = std::hypot(stab_re[i], stab_im[i]);
            if (i) o += ',';
            if (r > 0) add_pair(o, std::log(r), std::atan2(stab_im[i], stab_re[i]));
            else o += "[null,null]";
        }
        o += ']';
    }
    o += '}';
}

std::string event()
{
    std::string o = "{\"ev\":\"autoinfo\",\"info\":";
    if (has_info) add_info(o);
    else o += "null";
    o += ",\"stab\":";
    if (has_stab) add_stab(o);
    else o += "null";
    o += '}';
    return o;
}

} // namespace

extern "C" void auto_data_init(AutoDataEmit emit, AutoDataPointOf point_of)
{
    emit_line = emit;
    point_of_node = point_of;
    enabled = true;
}

extern "C" void auto_data_subscribe(int on)
{
    subscribed = on != 0;
    sent_valid = false;
}

extern "C" void auto_data_forget(void)
{
    has_info = has_stab = false;
}

extern "C" void auto_data_info(const AutoDataInfo *v)
{
    if (!enabled || !v) return;
    try {
        info.v = *v;
        info.sym = trimmed(v->sym);
        info.p1name = v->p1name ? v->p1name : "";
        info.two = v->p2name != nullptr;
        info.p2name = info.two ? v->p2name : "";
        info.vname = v->vname ? v->vname : "";
        info.v.sym = info.v.p1name = info.v.p2name = info.v.vname = nullptr; /* the strings above hold them */
        has_info = true;
    } catch (...) {
        has_info = false;
    }
}

extern "C" void auto_data_stab(const double *evr, const double *evi, int n, int periodic)
{
    if (!enabled) return;
    try {
        stab_re.assign(evr, evr + (n > 0 ? n : 0));
        stab_im.assign(evi, evi + (n > 0 ? n : 0));
        stab_periodic = periodic != 0;
        has_stab = true;
    } catch (...) {
        has_stab = false;
    }
}

extern "C" void auto_data_update(int final)
{
    static double last;
    if (!enabled || !subscribed || !emit_line) return;
    if (!final && !xpp_every(&last, 0.1)) return;
    try {
        std::string e = event();
        if (sent_valid && e == sent) return;
        emit_line(e.data(), e.size());
        sent.swap(e);
        sent_valid = true;
    } catch (...) {
    }
}
