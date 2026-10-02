/* The session's AUTO-member serializers: see xpp_session_auto.h. */
#include "xpp_session_auto.h"
#include "xpp_io.h"

#include <algorithm>
#include <array>
#include <limits>
#include <cmath>
#include <vector>

namespace xpp::snapx::auto_members {

namespace {

/* a name there is none of */
constexpr std::string_view no_name = "-";

std::string name_text(const std::string &name) { return name.empty() ? std::string(no_name) : name; }
std::string name_of(std::string_view text) { return text == no_name ? std::string() : std::string(text); }

constexpr std::array<const char *, 4> range_keys = {"xmin", "xmax", "ymin", "ymax"};

/* a view's zoom of one axis: LO:HI, or "-" when it shows the whole axis */
std::string range_text(const xpp::AxisRange &r)
{
    return r.set ? xpp::number(r.lo) + ':' + xpp::number(r.hi) : std::string(no_name);
}
bool parse_range(std::string_view text, xpp::AxisRange &r)
{
    r = xpp::AxisRange();
    if (text == no_name) return true;
    const std::size_t c = text.find(':');
    if (c == std::string_view::npos) return false;
    r.set = true;
    return xpp::parse_number(text.substr(0, c), r.lo) && xpp::parse_number(text.substr(c + 1), r.hi) && std::isfinite(r.lo) && std::isfinite(r.hi) && r.lo < r.hi;
}

/* diagram.csv's columns before the parameters: n_ints whole numbers, then
   three numbers */
constexpr std::array<const char *, 15> lead_columns = {"calc", "ibr",  "ntot",  "itp",  "lab",  "nfpar", "icp1",  "icp2",
                                                       "icp3", "icp4", "flag2", "from", "norm", "per",   "torper"};
constexpr int n_ints = 12;
constexpr int n_pars = 20; /* DIAGRAM's par */

} // namespace

std::string settings_text(const AutoSettingsSet &s)
{
    std::string o;
    for (int i = 0; i < AUTO_NUM_N; i++) o += xpp::format("{} {}\n", auto_settings_num_key(i), xpp::number(s.num[i]));
    o += "pars";
    for (int k = 0; k < s.npars; k++) o += ' ' + name_text(s.pars[k]);
    o += '\n';
    o += xpp::format("plot {}\nvar {}\npar1 {}\npar2 {}\n", s.plot, name_text(s.var), name_text(s.par1), name_text(s.par2));
    for (std::size_t i = 0; i < range_keys.size(); i++) o += xpp::format("{} {}\n", range_keys[i], xpp::number(s.range[i]));
    for (int i = 0; i < s.nmarks; i++) o += xpp::format("mark {} {}\n", name_text(s.mark_name[i]), xpp::number(s.mark_value[i]));
    return o;
}

Result<SettingsRead> parse_settings(std::string_view text, std::string file)
{
    return read_lines("AUTO's settings", std::move(file), text, [](Lines &l) {
        SettingsRead r;
        AutoSettingsSet &s = r.set;
        s.npars = 0;
        s.nmarks = 0;
        /* key's line, once */
        const auto at = [&](std::string key) {
            if (!r.lines.emplace(key, l.line()).second) l.fail(xpp::format("{} given twice", key));
        };
        const auto number = [&](std::string_view t, double &v) {
            if (!xpp::parse_number(t, v) || !std::isfinite(v)) l.fail(xpp::format("\"{}\" is not a number", t));
        };
        while (!l.at_end()) {
            const std::string_view line = l.next();
            if (line.empty()) continue;
            const std::vector<std::string_view> w = split_fields(line, ' ');
            const std::string_view key = w[0];
            if (key == "pars") {
                at("pars");
                if (w.size() - 1 > AUTO_SETTINGS_PARS) l.fail(xpp::format("{} parameters: AUTO has at most {}", w.size() - 1, AUTO_SETTINGS_PARS));
                s.npars = static_cast<int>(w.size() - 1);
                for (int k = 0; k < s.npars; k++) s.pars[k] = name_of(w[k + 1]);
                continue;
            }
            if (key == "mark") {
                if (s.nmarks >= AUTO_SETTINGS_MARKS) l.fail(xpp::format("more than {} Mark values", AUTO_SETTINGS_MARKS));
                if (w.size() != 3) l.fail("not \"mark NAME VALUE\"");
                at(xpp::format("mark{}", s.nmarks));
                s.mark_name[s.nmarks] = name_of(w[1]);
                number(w[2], s.mark_value[s.nmarks]);
                s.nmarks++;
                continue;
            }
            if (w.size() != 2) l.fail(xpp::format("not \"{} VALUE\"", key));
            const std::string_view value = w[1];
            if (key == "plot") {
                at("plot");
                if (!xpp::parse_int(value, s.plot)) l.fail(xpp::format("\"{}\" is not a whole number", value));
                s.has_plot = 1;
            } else if (key == "var" || key == "par1" || key == "par2") {
                at(std::string(key));
                (key == "var" ? s.var : key == "par1" ? s.par1 : s.par2) = name_of(value);
            } else {
                bool known = false;
                for (std::size_t i = 0; i < range_keys.size(); i++)
                    if (key == range_keys[i]) {
                        at(range_keys[i]);
                        number(value, s.range[i]);
                        s.has_range[i] = 1;
                        known = true;
                    }
                for (int i = 0; i < AUTO_NUM_N; i++)
                    if (key == auto_settings_num_key(i)) {
                        at(auto_settings_num_key(i));
                        number(value, s.num[i]);
                        s.has_num[i] = 1;
                        known = true;
                    }
                if (!known) l.fail(xpp::format("{} is not one of AUTO's settings", key));
            }
        }
        /* every key but the marks, which may be none, in settings_text's order */
        std::vector<std::string> keys;
        for (int i = 0; i < AUTO_NUM_N; i++) keys.emplace_back(auto_settings_num_key(i));
        keys.insert(keys.end(), {"pars", "plot", "var", "par1", "par2"});
        for (const char *k : range_keys) keys.emplace_back(k);
        for (const std::string &k : keys)
            if (!r.lines.contains(k)) l.fail(l.line() + 1, xpp::format("the file ends here, without its {} line", k));
        return r;
    });
}

std::string views_text(const SavedViews &v)
{
    std::string o;
    for (const SavedView &w : v.views) {
        o += xpp::format("view {} {} {} {}", w.plot, name_text(w.var), name_text(w.par1), name_text(w.par2));
        for (double r : w.range) o += ' ' + xpp::number(r);
        o += ' ' + range_text(w.zoom.x) + ' ' + range_text(w.zoom.y) + '\n';
    }
    o += xpp::format("active {}\n", v.active);
    return o;
}

Result<ViewsRead> parse_views(std::string_view text, std::string file)
{
    return read_lines("AUTO's views", std::move(file), text, [](Lines &l) {
        ViewsRead r;
        SavedViews &v = r.views;
        int active_line = 0;
        while (!l.at_end()) {
            const std::string_view line = l.next();
            if (line.empty()) continue;
            const std::vector<std::string_view> w = split_fields(line, ' ');
            if (w[0] == "active" && w.size() == 2) {
                if (active_line) l.fail("a second active line");
                if (!xpp::parse_int(w[1], v.active)) l.fail(xpp::format("\"{}\" is not a whole number", w[1]));
                active_line = l.line();
                continue;
            }
            if (w[0] != "view" || w.size() != 11)
                l.fail("not \"view PLOT VAR PAR1 PAR2 XMIN XMAX YMIN YMAX ZOOMX ZOOMY\" nor \"active K\"");
            if (v.views.size() >= saved_views_limit) l.fail("too many AUTO views");
            SavedView &s = v.views.emplace_back();
            r.lines.push_back(l.line());
            if (!xpp::parse_int(w[1], s.plot)) l.fail(xpp::format("\"{}\" is not a whole number", w[1]));
            s.var = name_of(w[2]);
            s.par1 = name_of(w[3]);
            s.par2 = name_of(w[4]);
            for (std::size_t i = 0; i < s.range.size(); i++)
                if (!xpp::parse_number(w[5 + i], s.range[i]) || !std::isfinite(s.range[i])) l.fail(xpp::format("\"{}\" is not a number", w[5 + i]));
            for (std::size_t i = 0; i < 2; i++)
                if (!parse_range(w[9 + i], i == 0 ? s.zoom.x : s.zoom.y))
                    l.fail(xpp::format("\"{}\" is not a zoom (LO:HI, LO below HI, or -)", w[9 + i]));
        }
        if (v.views.empty()) l.fail(l.line() + 1, "the file ends here, without a view");
        if (!active_line) l.fail(l.line() + 1, "the file ends here, without its active line");
        if (v.active < 0 || v.active >= static_cast<int>(v.views.size()))
            l.fail(active_line, xpp::format("view {} is not one of the {} views", v.active, v.views.size()));
        return r;
    });
}

std::string diagram_csv(const std::deque<DiagramPoint> &points, std::span<const std::string> vars)
{
    std::string o;
    for (std::size_t i = 0; i < lead_columns.size(); i++) o += xpp::format("{}{}", i ? "," : "", lead_columns[i]);
    for (int i = 1; i <= n_pars; i++) o += xpp::format(",par{}", i);
    for (const std::string &x : vars) o += xpp::format(",u0.{0},uhi.{0},ulo.{0},ubar.{0}", x);
    for (std::size_t i = 1; i <= vars.size(); i++) o += xpp::format(",evr{0},evi{0}", i);
    o += '\n';
    const std::size_t n = vars.size();
    for (const DiagramPoint &p : points) {
        const DIAGRAM &d = p.d;
        o += xpp::format("{},{},{},{},{},{},{},{},{},{},{},{}", d.calc, d.ibr, d.ntot, d.itp, d.lab, d.nfpar, d.icp1, d.icp2,
                         d.icp3, d.icp4, d.flag2, d.from);
        for (double v : {d.norm, d.per, d.torper}) o += ',' + xpp::number(v);
        for (int i = 0; i < n_pars; i++) o += ',' + xpp::number(d.par[i]);
        for (std::size_t i = 0; i < n; i++)
            for (const std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar})
                o += ',' + xpp::number(i < v->size() ? (*v)[i] : 0.0);
        for (std::size_t i = 0; i < n; i++)
            for (const std::vector<double> *v : {&p.evr, &p.evi}) o += ',' + xpp::number(i < v->size() ? (*v)[i] : 0.0);
        o += '\n';
    }
    return o;
}

Result<std::deque<DiagramPoint>> parse_diagram_csv(std::string_view text, int n, std::string file)
{
    return read_lines("AUTO's diagram", std::move(file), text, [n](Lines &l) {
        if (n < 0) l.fail("negative variable count");
        const std::size_t columns = lead_columns.size() + n_pars + 6 * static_cast<std::size_t>(n < 0 ? 0 : n);
        const std::vector<std::string_view> head = split_fields(l.next("the header row"), ',');
        if (head.size() != columns) l.fail(xpp::format("{} columns: a diagram of this model's {} variables has {}", head.size(), n, columns));
        for (std::size_t i = 0; i < lead_columns.size(); i++)
            if (head[i] != lead_columns[i]) l.fail(xpp::format("column {} is \"{}\", not \"{}\"", i + 1, head[i], lead_columns[i]));
        std::deque<DiagramPoint> points;
        while (!l.at_end()) {
            const std::string_view line = l.next();
            if (line.empty()) continue;
            const std::vector<std::string_view> f = split_fields(line, ',');
            if (f.size() != columns) l.fail(xpp::format("{} fields, not {}", f.size(), columns));
            /* Bound the expanded vectors too: short textual numbers can occupy
               several times their archive bytes once stored as doubles. */
            const std::size_t point_bytes = sizeof(DiagramPoint) + (columns - n_ints) * sizeof(double);
            if (points.size() >= saved_points_limit || points.size() >= zip::archive_bytes_limit / point_bytes)
                l.fail("too many AUTO diagram points for the session memory limit");
            DiagramPoint &p = points.emplace_back();
            DIAGRAM &d = p.d;
            int *ints[n_ints] = {&d.calc, &d.ibr, &d.ntot, &d.itp, &d.lab, &d.nfpar, &d.icp1, &d.icp2, &d.icp3, &d.icp4, &d.flag2, &d.from};
            std::size_t c = 0;
            const auto bad = [&](std::size_t k, const char *kind) {
                l.fail(xpp::format("field {} (\"{}\") is not {}", k + 1, f[k], kind));
            };
            for (int *v : ints) {
                if (!xpp::parse_int(f[c], *v) || *v == std::numeric_limits<int>::min()) bad(c, "a whole number");
                c++;
            }
            /* Drawing and grabbing index AUTO's parameter arrays directly. */
            for (const int index : {d.icp1, d.icp2, d.icp3, d.icp4})
                if (index < 0 || index >= AUTO_SETTINGS_PARS)
                    l.fail(xpp::format("parameter index {} is outside AUTO's parameters", index));
            if (d.nfpar < 0 || d.nfpar > n_pars)
                l.fail(xpp::format("nfpar {} is outside AUTO's parameter storage", d.nfpar));
            const auto real = [&](double &v) {
                if (!xpp::parse_number(f[c], v) || !std::isfinite(v)) bad(c, "a number");
                c++;
            };
            for (double *v : {&d.norm, &d.per, &d.torper}) real(*v);
            for (int i = 0; i < n_pars; i++) real(d.par[i]);
            for (std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar, &p.evr, &p.evi}) v->assign(static_cast<std::size_t>(n), 0.0);
            for (int i = 0; i < n; i++)
                for (std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar}) real((*v)[static_cast<std::size_t>(i)]);
            for (int i = 0; i < n; i++)
                for (std::vector<double> *v : {&p.evr, &p.evi}) real((*v)[static_cast<std::size_t>(i)]);
        }
        return points;
    });
}

} // namespace xpp::snapx::auto_members
