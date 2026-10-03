/* AUTO's info strip and stability circle as data: the "autoinfo" event
   (auto_data.h, docs/protocol.md "The AUTO diagram as data").

   auto_nox.c reports what the strip and the circle show as it draws them;
   the event is built from that record when the front end asks for an
   update and sent only when its text differs from the last one sent. */
#include <new>
#include "xpp_mem.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "auto_data.h"
#include "diagram.h"
#include "xpp_io.h"
#include "json_number.h"
#include "auto_stop.h"
#include "xpp_job.h"
#include "data_event.h"
#include "session.h"
#include "xpp_math.h"

namespace xpp {

namespace {

/* the front end's map of AUTO's diagram entries to its data's indices,
   given once with its sink (auto_data_init) */
AutoDataPointOf point_of_node;

/* ---- JSON text (as plot_data.cpp writes it) ---- */

void add_pair(std::string &o, double re, double im)
{
    o += '[';
    xpp::json::json_append_number_shortest(o, re);
    o += ',';
    xpp::json::json_append_number_shortest(o, im);
    o += ']';
}

void add_info(std::string &o, const AutoDataShown &info)
{
    const AutoDataInfo &v = info.v;
    o += "{\"point\":";
    o += std::to_string(point_of_node ? point_of_node(v.node) : -1);
    o += ",\"br\":" + std::to_string(std::abs(v.ibr)) + ",\"pt\":" + std::to_string(std::abs(v.pt));
    o += ",\"type\":" + std::to_string(v.type);
    o += xpp::point_is_stable(v.type) ? ",\"stable\":true" : ",\"stable\":false";
    o += xpp::point_is_periodic(v.type) ? ",\"periodic\":true,\"sym\":" : ",\"periodic\":false,\"sym\":";
    xpp::json_append_string(o, info.sym.c_str());
    o += ",\"lab\":" + std::to_string(v.lab);
    if (v.flag2) o += ",\"f2\":" + std::to_string(v.flag2);
    o += ",\"par\":[{\"name\":";
    xpp::json_append_string(o, info.p1name.c_str());
    o += ",\"value\":";
    xpp::json::json_append_number_shortest(o, v.p1);
    o += '}';
    if (info.two) {
        o += ",{\"name\":";
        xpp::json_append_string(o, info.p2name.c_str());
        o += ",\"value\":";
        xpp::json::json_append_number_shortest(o, v.p2);
        o += '}';
    }
    o += ']';
    xpp::json::json_append_field(o, "norm", v.norm);
    o += ",\"var\":";
    xpp::json_append_string(o, info.vname.c_str());
    xpp::json::json_append_field(o, "u", v.u);
    xpp::json::json_append_field(o, "per", v.per);
    xpp::json::json_append_field(o, "x", v.x);
    xpp::json::json_append_field(o, "y", v.y);
    xpp::json::json_append_field(o, "y2", v.y2);
    o += '}';
}

/* the circle's values; for a steady state the eigenvalues too, lambda =
   log z: exact in its real part while e^lambda is not 0, its imaginary part
   the principal value (XPP keeps e^lambda only, so a frequency is known
   modulo 2 pi) */
void add_stab(std::string &o, const AutoDataShown &sh)
{
    const std::vector<double> &stab_re = sh.stab_re, &stab_im = sh.stab_im;
    const bool stab_periodic = sh.stab_periodic;
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
            const double r = xpp::math::hypot(stab_re[i], stab_im[i]);
            if (i) o += ',';
            if (r > 0) add_pair(o, xpp::math::log(r), xpp::math::atan2(stab_im[i], stab_re[i]));
            else o += "[null,null]";
        }
        o += ']';
    }
    o += '}';
}

/* why the run's last branch ended (auto_stop.h), or null */
void add_stop(std::string &o, const AutoStop &stop)
{
    AutoStopInfo st;
    auto_stop_last(stop, &st);
    if (st.why == AUTO_STOP_NONE) {
        o += "null";
        return;
    }
    o += "{\"why\":";
    xpp::json_append_string(o, st.key);
    o += ",\"text\":";
    xpp::json_append_string(o, st.text);
    o += ",\"br\":" + std::to_string(st.br) + ",\"pt\":" + std::to_string(st.pt);
    xpp::json::json_append_field(o, "value", st.value);
    xpp::json::json_append_field(o, "limit", st.limit);
    o += '}';
}

std::string event_line(const Session &s)
{
    const AutoDataShown &sh = s.auto_state.shown;
    std::string o = "{\"ev\":\"autoinfo\",\"info\":";
    if (sh.has_info) add_info(o, sh);
    else o += "null";
    o += ",\"stab\":";
    if (sh.has_stab) add_stab(o, sh);
    else o += "null";
    o += ",\"stop\":";
    add_stop(o, s.auto_state.stop);
    o += '}';
    return o;
}

/* the client's subscription and the line it was sent last: the front
   end's, which outlives a load */
xpp::ChangedEvent<Session> event{event_line, "sending AUTO's info strip"};
/* the clock that paces the event during a run (xpp::every) */
double last_update;

} // namespace

void auto_data_init(AutoDataEmit emit, AutoDataPointOf point_of)
{
    event.init(emit);
    point_of_node = point_of;
}

void auto_data_subscribe(int on)
{
    event.want(on != 0);
}

void auto_data_forget(Session &s)
{
    s.auto_state.shown.has_info = s.auto_state.shown.has_stab = false;
    auto_stop_clear(s.auto_state.stop);
}

void auto_data_info(Session &s, const AutoDataInfo *v)
{
    AutoDataShown &info = s.auto_state.shown;
    if (!event.ready() || !v || info.held > 0) return;
    try {
        info.v = *v;
        info.sym = std::string(xpp::trim_blanks(v->sym));
        info.p1name = v->p1name ? v->p1name : "";
        info.two = v->p2name != nullptr;
        info.p2name = info.two ? v->p2name : "";
        info.vname = v->vname ? v->vname : "";
        info.v.sym = info.v.p1name = info.v.p2name = info.v.vname = nullptr; /* the strings above hold them */
        info.has_info = true;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording AUTO's info strip");
    }
}

void auto_data_stab(Session &s, const double *evr, const double *evi, int n, int periodic)
{
    AutoDataShown &sh = s.auto_state.shown;
    if (!event.ready() || sh.held > 0) return;
    try {
        sh.stab_re.assign(evr, evr + (n > 0 ? n : 0));
        sh.stab_im.assign(evi, evi + (n > 0 ? n : 0));
        sh.stab_periodic = periodic != 0;
        sh.has_stab = true;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a point's stability");
    }
}

void auto_data_hold(Session &s, int on)
{
    int &held = s.auto_state.shown.held;
    held += on ? 1 : (held > 0 ? -1 : 0);
}

void auto_data_update(const Session &s, int final)
{
    if (!event.ready() || !event.subscribed()) return;
    if (!final && !xpp::every(last_update, xpp::PROGRESS_SECONDS)) return;
    event.update(s);
}

} // namespace xpp
