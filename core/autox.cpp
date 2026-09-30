/* The .autox file's pure part: see autox.h. */
#include "autox.h"
#include "xpp_io.h"

#include <array>
#include <vector>

namespace xpp::autox {

namespace {

/* a name there is none of */
constexpr std::string_view no_name = "-";

std::string name_text(const std::string &name) { return name.empty() ? std::string(no_name) : name; }
std::string name_of(std::string_view text) { return text == no_name ? std::string() : std::string(text); }

/* the lines of text, a CR before a newline dropped, blank lines left out */
std::vector<std::string_view> lines_of(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

/* s split at each sep (empty fields kept) */
std::vector<std::string_view> split(std::string_view s, char sep)
{
    std::vector<std::string_view> f;
    while (true) {
        const std::size_t i = s.find(sep);
        f.push_back(s.substr(0, i));
        if (i == std::string_view::npos) return f;
        s = s.substr(i + 1);
    }
}

constexpr std::array<const char *, 4> range_keys = {"xmin", "xmax", "ymin", "ymax"};

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

std::optional<AutoSettingsSet> parse_settings(std::string_view text)
{
    AutoSettingsSet s;
    s.npars = 0;
    s.nmarks = 0;
    for (std::string_view line : lines_of(text)) {
        std::vector<std::string_view> w = split(line, ' ');
        const std::string_view key = w[0];
        if (key == "pars") {
            if (w.size() - 1 > AUTO_SETTINGS_PARS) return std::nullopt;
            s.npars = static_cast<int>(w.size() - 1);
            for (int k = 0; k < s.npars; k++) s.pars[k] = name_of(w[k + 1]);
            continue;
        }
        if (key == "mark") {
            if (w.size() != 3 || s.nmarks >= AUTO_SETTINGS_MARKS) return std::nullopt;
            s.mark_name[s.nmarks] = name_of(w[1]);
            if (!xpp::parse_number(w[2], s.mark_value[s.nmarks])) return std::nullopt;
            s.nmarks++;
            continue;
        }
        if (w.size() != 2) return std::nullopt;
        const std::string_view value = w[1];
        if (key == "plot") {
            if (!xpp::parse_int(value, s.plot)) return std::nullopt;
            s.has_plot = 1;
        } else if (key == "var") s.var = name_of(value);
        else if (key == "par1") s.par1 = name_of(value);
        else if (key == "par2") s.par2 = name_of(value);
        else {
            for (std::size_t i = 0; i < range_keys.size(); i++)
                if (key == range_keys[i]) {
                    if (!xpp::parse_number(value, s.range[i])) return std::nullopt;
                    s.has_range[i] = 1;
                }
            for (int i = 0; i < AUTO_NUM_N; i++)
                if (key == auto_settings_num_key(i)) {
                    if (!xpp::parse_number(value, s.num[i])) return std::nullopt;
                    s.has_num[i] = 1;
                }
        }
    }
    return s;
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

std::optional<std::deque<DiagramPoint>> parse_diagram_csv(std::string_view text, int n)
{
    if (n < 0) return std::nullopt;
    const std::vector<std::string_view> lines = lines_of(text);
    const std::size_t columns = lead_columns.size() + n_pars + 6 * static_cast<std::size_t>(n);
    if (lines.empty()) return std::nullopt;
    const std::vector<std::string_view> head = split(lines[0], ',');
    if (head.size() != columns) return std::nullopt;
    for (std::size_t i = 0; i < lead_columns.size(); i++)
        if (head[i] != lead_columns[i]) return std::nullopt;
    std::deque<DiagramPoint> points;
    for (std::size_t r = 1; r < lines.size(); r++) {
        const std::vector<std::string_view> f = split(lines[r], ',');
        if (f.size() != columns) return std::nullopt;
        DiagramPoint &p = points.emplace_back();
        DIAGRAM &d = p.d;
        int *ints[n_ints] = {&d.calc, &d.ibr, &d.ntot, &d.itp, &d.lab, &d.nfpar, &d.icp1, &d.icp2, &d.icp3, &d.icp4, &d.flag2, &d.from};
        std::size_t c = 0;
        for (int *v : ints)
            if (!xpp::parse_int(f[c++], *v)) return std::nullopt;
        for (double *v : {&d.norm, &d.per, &d.torper})
            if (!xpp::parse_number(f[c++], *v)) return std::nullopt;
        for (int i = 0; i < n_pars; i++)
            if (!xpp::parse_number(f[c++], d.par[i])) return std::nullopt;
        for (std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar, &p.evr, &p.evi}) v->assign(n, 0.0);
        for (int i = 0; i < n; i++)
            for (std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar})
                if (!xpp::parse_number(f[c++], (*v)[i])) return std::nullopt;
        for (int i = 0; i < n; i++)
            for (std::vector<double> *v : {&p.evr, &p.evi})
                if (!xpp::parse_number(f[c++], (*v)[i])) return std::nullopt;
    }
    return points;
}

} // namespace xpp::autox
