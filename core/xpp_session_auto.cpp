/* The session's AUTO-member serializers: see xpp_session_auto.h. */
#include "xpp_session_auto.h"
#include "xpp_io.h"
#include "snapx.h"
#include "jacobian.h"
#include "auto_state.h"

#include <algorithm>
#include <array>
#include <limits>
#include <cmath>
#include <vector>
#include <map>

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
constexpr std::array<const char *, 16> lead_columns = {"calc", "ibr",  "ntot",  "itp",  "lab",  "nfpar", "icp1",  "icp2",
                                                       "icp3", "icp4", "flag2", "from", "jacobian", "norm", "per",   "torper"};
constexpr int n_ints = 13;


} // namespace

std::string settings_text(const AutoSettingsSet &s)
{
    std::string o;
    for (int i = 0; i < AUTO_NUM_N; i++) o += xpp::format("{} {}\n", auto_settings_num_key(i), xpp::number(s.num[i]));
    for (std::size_t i=0;i<auto_codim_kinds.size();++i)
        o += xpp::format("{} {}\n",auto_codim_keys[i],*s.special_points[i] ? "true" : "false");
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
            if (key == "mark") {
                if (s.nmarks >= auto_file_marks_limit) l.fail(xpp::format("more than {} Mark values", auto_file_marks_limit));
                if (w.size() != 3) l.fail("not \"mark NAME VALUE\"");
                at(xpp::format("mark{}", s.nmarks));
                s.mark_name.push_back(name_of(w[1]));
                s.mark_value.emplace_back();
                number(w[2], s.mark_value.back());
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
                for (std::size_t i=0;i<auto_codim_kinds.size();++i)
                    if (key==auto_codim_keys[i]) {
                        at(auto_codim_keys[i]);
                        bool enabled;
                        if (!parse_bool(value,enabled)) l.fail(xpp::format("{} must be true or false, got {}",key,value));
                        s.special_points[i]=enabled; known=true;
                    }
                if (!known) l.fail(xpp::format("{} is not one of AUTO's settings", key));
            }
        }
        /* every key but the marks, which may be none, in settings_text's order */
        std::vector<std::string> keys;
        for (int i = 0; i < AUTO_NUM_N; i++) keys.emplace_back(auto_settings_num_key(i));
        for (const auto *key:auto_codim_keys) keys.emplace_back(key);
        keys.insert(keys.end(), {"plot", "var", "par1", "par2"});
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

std::string diagram_csv(const std::deque<DIAGRAM> &points, int model_parameters, std::span<const std::string> vars)
{
    std::string o;
    std::map<const DiagramBranch *,std::size_t> branches;
    for(const DIAGRAM &p:points){
        const auto [entry,added]=branches.try_emplace(p.branch.get(),branches.size());
        if(!added)continue;
        o+=xpp::format("branch,{}",entry->second);
        for(double value:p.branch->parameters)o+=','+xpp::number(value);
        o+='\n';
    }
    for (std::size_t i = 0; i < lead_columns.size(); i++) o += xpp::format("{}{}", i ? "," : "", lead_columns[i]);
    o+=",branch,continued,values";
    for (const std::string &x : vars) o += xpp::format(",u0.{0},uhi.{0},ulo.{0},ubar.{0}", x);
    for (std::size_t i = 1; i <= vars.size(); i++) o += xpp::format(",evr{0},evi{0}", i);
    o += '\n';
    const std::size_t n = vars.size();
    for (const DIAGRAM &p : points) {
        const DIAGRAM &d = p;
        /* an axis is written as the file's integer (auto_axis.h) */
        const auto axis_code = [model_parameters](AutoAxis a) { return auto_axis_encode(a, model_parameters); };
        o += xpp::format("{},{},{},{},{},{},{},{},{},{},{},{},{}", d.calc, d.ibr, d.ntot, auto_point_encode(d.itp), d.lab, d.nfpar, axis_code(d.icp1), axis_code(d.icp2),
                         axis_code(d.icp3), axis_code(d.icp4), d.flag2, d.from, d.jacobian);
        for (double v : {d.norm, d.per, d.torper}) o += ',' + xpp::number(v);
        o+=','+xpp::number(branches.at(d.branch.get()))+',';
        for(std::size_t i=0;i<d.parameter_indices.size();i++)o+=(i?";":"")+xpp::number(d.parameter_indices[i]);
        o+=',';
        for(std::size_t i=0;i<d.par.size();i++)o+=(i?";":"")+xpp::number(d.par[i]);
        for (std::size_t i = 0; i < n; i++)
            for (const std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar})
                o += ',' + xpp::number((*v)[i]);
        for (std::size_t i = 0; i < n; i++)
            for (const std::vector<double> *v : {&p.evr, &p.evi}) o += ',' + xpp::number((*v)[i]);
        o += '\n';
    }
    return o;
}

namespace {
void check_point_indices(const DIAGRAM &d, int n_pars, Lines &l, int line)
{
    /* Drawing and grabbing index the model's parameters directly: the first continued
       parameter is one (its name is indexed by it); the others may be AUTO's own
       unknowns, such as the period of a periodic run (the axes were decoded, so each is
       a model parameter or one of AUTO's slots). */
    if (!d.icp1.is_parameter())
        l.fail(line, xpp::format("axis icp1 is AUTO's slot {}, not one of the model's {} parameters", d.icp1.index, n_pars));
    if (d.jacobian < 0 || d.jacobian >= static_cast<int>(jacobian_names.size()))
        l.fail(line, xpp::format("jacobian {} is neither {} ({}) nor {} ({})", d.jacobian,
                              jacobian_names[JACOBIAN_DIFFERENCE], JACOBIAN_DIFFERENCE, jacobian_names[JACOBIAN_EXACT], JACOBIAN_EXACT));
    if (d.nfpar < 0 || static_cast<std::size_t>(d.nfpar) > saved_parameters_limit)
        l.fail(line, xpp::format("nfpar {} is outside AUTO's parameter storage", d.nfpar));
}
void check_point_parameters(const DIAGRAM &d, Lines &l, int line)
{
    // Sorting a bounded copy avoids quadratic work on a hostile free-parameter list.
    auto ordered=d.parameter_indices;
    std::sort(ordered.begin(),ordered.end());
    if(std::adjacent_find(ordered.begin(),ordered.end())!=ordered.end())l.fail(line, "continued parameter given twice");
    if(d.parameter_indices.empty() || d.parameter_indices.front()!=d.icp1.index)l.fail(line, "first continued parameter differs from icp1");
}
} // namespace

Result<std::deque<DIAGRAM>> parse_diagram_csv(std::string_view text, int n, int n_pars, std::string file)
{
    return read_lines("AUTO's diagram", std::move(file), text, [n, n_pars](Lines &l) {
        if (n < 0 || n_pars < 0 || static_cast<std::size_t>(n_pars) > saved_parameters_limit) l.fail("invalid model dimensions for AUTO diagram storage");
        std::vector<std::shared_ptr<const DiagramBranch>> branches;
        std::size_t expanded_bytes=0;
        std::string_view header=l.next("the branch records and point header");
        while(header.starts_with("branch,")){
            const auto fields=split_fields(header,',');
            if(fields.size()!=static_cast<std::size_t>(n_pars)+2)l.fail("branch parameter count does not match the model");
            int id;
            if(!parse_int(fields[1],id) || id!=static_cast<int>(branches.size()))l.fail("branch ids must be contiguous from zero");
            const std::size_t bytes=sizeof(DiagramBranch)+static_cast<std::size_t>(n_pars)*sizeof(double);
            if(branches.size()>=saved_points_limit || !account_memory(expanded_bytes, 1, bytes))l.fail("too many AUTO branches for the session memory limit");
            auto branch=std::make_shared<DiagramBranch>();
            for(std::size_t i=2;i<fields.size();i++){
                double value;
                if(!parse_number(fields[i],value) || !std::isfinite(value))l.fail(xpp::format("branch value \"{}\" is not a number",fields[i]));
                branch->parameters.push_back(value);
            }
            branches.push_back(std::move(branch));
            header=l.next("the point header");
        }
        const std::size_t columns = lead_columns.size() + 3 + 6 * static_cast<std::size_t>(n);
        const std::vector<std::string_view> head = split_fields(header, ',');
        if (head.size() != columns) l.fail(xpp::format("{} columns: a diagram of this model's {} variables has {}", head.size(), n, columns));
        for (std::size_t i = 0; i < lead_columns.size(); i++)
            if (head[i] != lead_columns[i]) l.fail(xpp::format("column {} is \"{}\", not \"{}\"", i + 1, head[i], lead_columns[i]));
        if(head[lead_columns.size()]!="branch" || head[lead_columns.size()+1]!="continued" || head[lead_columns.size()+2]!="values")l.fail("missing branch and continued-parameter columns");
        std::deque<DIAGRAM> points;
        while (!l.at_end()) {
            const std::string_view line = l.next();
            if (line.empty()) continue;
            const std::vector<std::string_view> f = split_fields(line, ',');
            if (f.size() != columns) l.fail(xpp::format("{} fields, not {}", f.size(), columns));
            /* Bound the expanded vectors too: short textual numbers can occupy
               several times their archive bytes once stored as doubles. */
            if (points.size() >= saved_points_limit)
                l.fail("too many AUTO diagram points for the session memory limit");
            DIAGRAM &p = points.emplace_back();
            DIAGRAM &d = p;
            int itp_code = 0;
            int axis_codes[4] = {};
            int *ints[n_ints] = {&d.calc, &d.ibr, &d.ntot, &itp_code, &d.lab, &d.nfpar, &axis_codes[0], &axis_codes[1], &axis_codes[2], &axis_codes[3], &d.flag2, &d.from, &d.jacobian};
            std::size_t c = 0;
            const auto bad = [&](std::size_t k, const char *kind) {
                l.fail(xpp::format("field {} (\"{}\") is not {}", k + 1, f[k], kind));
            };
            for (int *v : ints) {
                if (!xpp::parse_int(f[c], *v) || *v == std::numeric_limits<int>::min()) bad(c, "a whole number");
                c++;
            }
            const auto itp = auto_point_decode(itp_code);
            if (!itp) l.fail(xpp::format("field 4 (\"{}\"): {}", f[3], auto_point_unknown(itp_code)));
            d.itp = *itp;
            const std::array<AutoAxis *, 4> axes = {&d.icp1, &d.icp2, &d.icp3, &d.icp4};
            for (std::size_t k = 0; k < axes.size(); k++) {
                const auto decoded = auto_axis_decode(axis_codes[k], n_pars);
                if (!decoded) l.fail(xpp::format("axis {} is neither one of the model's {} parameters nor one of AUTO's slots", axis_codes[k], n_pars));
                *axes[k] = *decoded;
            }
            check_point_indices(d, n_pars, l, l.line());
            const auto real = [&](double &v) {
                if (!xpp::parse_number(f[c], v) || !std::isfinite(v)) bad(c, "a number");
                c++;
            };
            for (double *v : {&d.norm, &d.per, &d.torper}) real(*v);
            int branch;
            if(!parse_int(f[c++],branch) || branch<0 || static_cast<std::size_t>(branch)>=branches.size())l.fail("unknown branch id");
            d.branch=branches[branch];
            const auto indices=split_fields(f[c++],';');
            const auto values=split_fields(f[c++],';');
            if(indices.empty() || indices.size()>static_cast<std::size_t>(d.nfpar) || values.size()!=indices.size())l.fail("continued model parameter count differs from nfpar");
            const std::size_t point_bytes=sizeof(DIAGRAM)+6*static_cast<std::size_t>(n)*sizeof(double)
                +indices.size()*(sizeof(int)+sizeof(double));
            if(!account_memory(expanded_bytes, 1, point_bytes))l.fail("too many AUTO diagram points for the session memory limit");
            for(std::size_t i=0;i<indices.size();i++){
                int index;
                double value;
                if(!parse_int(indices[i],index) || index<0 || index>=n_pars)l.fail("invalid continued model parameter index");
                if(!parse_number(values[i],value) || !std::isfinite(value))l.fail(xpp::format("continued value \"{}\" is not a number",values[i]));
                d.parameter_indices.push_back(index);
                d.par.push_back(value);
            }
            check_point_parameters(d, l, l.line());
            p.size_variables(static_cast<std::size_t>(n));
            for (int i = 0; i < n; i++)
                for (std::vector<double> *v : {&p.u0, &p.uhi, &p.ulo, &p.ubar}) real((*v)[static_cast<std::size_t>(i)]);
            for (int i = 0; i < n; i++)
                for (std::vector<double> *v : {&p.evr, &p.evi}) real((*v)[static_cast<std::size_t>(i)]);
        }
        return points;
    });
}

std::deque<DIAGRAM> read_foreign_diagram(Lines &lines, int n,
    std::span<const int> imported, std::span<const double> parameters, int calc, int flag2, double torper)
{
    // XPPAUT diagram records have exactly four continuation index columns.
    constexpr int foreign_continuation_columns = 4;
    int last;
    lines.numbers(last);
    const std::size_t bytes = sizeof(DIAGRAM) + sizeof(DiagramBranch)
        + (6 * static_cast<std::size_t>(n) + parameters.size()) * sizeof(double)
        + imported.size() * (sizeof(int) + sizeof(double));
    if (n <= 0 || last < 1 || static_cast<std::size_t>(last) >= saved_points_limit
        || bytes > zip::archive_bytes_limit / (static_cast<std::size_t>(last) + 1))
        lines.fail("foreign diagram counts exceed the session memory limit");
    std::deque<DIAGRAM> points;
    std::shared_ptr<const DiagramBranch> branch;
    for (int index = 0; index <= last; index++) {
        DIAGRAM p;
        DIAGRAM &d = p;
        int foreign_index;
        int itp_code;
        int slots[4];
        lines.numbers(d.calc, d.ibr, d.ntot, itp_code, d.lab, foreign_index, d.nfpar,
                      slots[0], slots[1], slots[2], slots[3], d.flag2);
        const auto itp = auto_point_decode(itp_code);
        if (!itp) lines.fail(auto_point_unknown(itp_code));
        d.itp = *itp;
        const int point_line=lines.line();
        if (foreign_index != index || d.nfpar < 1 || d.nfpar > foreign_continuation_columns)
            lines.fail("invalid foreign point index or free parameter count");
        for (int slot : slots)
            if (slot < 0 || slot >= auto_unknown::block_size)
                lines.fail("foreign parameter index is outside AUTO's slots");
        // XPPAUT stores eight selected parameter values on each point's numeric row.
        std::array<double, xppaut_parameter_prefix> values;
        lines.numbers(values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], d.norm, d.per);
        if (!branch || std::abs(static_cast<long long>(d.ntot)) == 1 || points.back().ibr != d.ibr) {
            auto held = std::make_shared<DiagramBranch>();
            held->parameters.assign(parameters.begin(), parameters.end());
            for (std::size_t k = 0; k < imported.size(); k++) held->parameters[imported[k]] = values[k];
            branch = std::move(held);
        }
        d.branch = branch;
        const auto mapped = [&](int slot) {
            return static_cast<std::size_t>(slot) < imported.size() ? imported[slot] : static_cast<int>(parameters.size()) + slot;
        };
        AutoAxis *const axes[] = {&d.icp1, &d.icp2, &d.icp3, &d.icp4};
        for (int k = 0; k < 4; k++) *axes[k] = *auto_axis_decode(mapped(slots[k]), static_cast<int>(parameters.size())); /* the slots were checked above */
        for (int k = 0; k < d.nfpar; k++)
            if (static_cast<std::size_t>(slots[k]) < imported.size()) {
                d.parameter_indices.push_back(imported[slots[k]]);
                d.par.push_back(values[slots[k]]);
            }
        check_point_indices(d, static_cast<int>(parameters.size()), lines, point_line);
        check_point_parameters(d, lines, point_line);
        p.size_variables(static_cast<std::size_t>(n));
        for (int k = 0; k < n; k++) lines.numbers(p.u0[k], p.uhi[k], p.ulo[k], p.ubar[k], p.evr[k], p.evi[k]);
        // Preserve the old import's run presentation fields.
        d.calc = calc; d.torper = torper;
        if (index == 0) d.flag2 = flag2;
        points.push_back(std::move(p));
    }
    return points;
}

} // namespace xpp::snapx::auto_members
