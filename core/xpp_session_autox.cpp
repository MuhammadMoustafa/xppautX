/* The session's autoX state: its settings and views written and read, its diagram's members: see xpp_session_autox.h. */
#include "xpp_session_autox.h"
#include "xpp_io.h"
#include "snapx.h"
#include "jacobian.h"
#include "autox_state.h"
#include "session_settings.h"
#include "many_pops.h"

#include <algorithm>
#include <array>
#include <limits>
#include <cmath>
#include <vector>
#include <map>

namespace xpp::snapx::autox_members {

namespace {

constexpr std::array<const char *, 4> range_keys = {"xmin", "xmax", "ymin", "ymax"};

/* diagram.csv's columns before the parameters: n_ints whole numbers, then
   three numbers */
constexpr std::array<const char *, 16> lead_columns = {"calc", "ibr",  "ntot",  "itp",  "lab",  "nfpar", "icp1",  "icp2",
                                                       "icp3", "icp4", "flag2", "from", "jacobian", "norm", "per",   "torper"};
constexpr int n_ints = 13;


} // namespace

void write_autox_settings(odex::Out &out, const AutoxSettingsSet &s)
{
    out.begin("settings");
    for (int i = 0; i < AUTOX_NUM_N; i++) out.word(autox_settings_num_key(i), autox_settings_num_text(i, s.num[i]));
    for (std::size_t i = 0; i < autox_codim_kinds.size(); ++i) out.flag(autox_codim_keys[i], *s.special_points[i]);
    out.word("plot", autox_projection_name(*s.plot));
    out.name("var", s.var);
    out.name("par1", s.par1);
    out.name("par2", s.par2);
    for (std::size_t i = 0; i < range_keys.size(); i++) out.real(range_keys[i], s.range[i]);
    out.begin_list("marks");
    for (int i = 0; i < s.nmarks.value_or(0); i++) {
        out.begin_item();
        out.name("name", s.mark_name[i]);
        out.real("value", s.mark_value[i]);
        out.end_item();
    }
    out.end_list();
    out.end();
}

SettingsRead read_autox_settings(odex::Block &g)
{
    SettingsRead r;
    AutoxSettingsSet &s = r.set;
    const auto at = [&](std::string_view key) {
        const odex::Member &m = *g.member(key);
        r.places[std::string(key)] = {m.value.line, m.value.col};
        return &m;
    };
    for (int i = 0; i < AUTOX_NUM_N; i++) {
        const std::string key = autox_settings_num_key(i);
        const odex::Member &m = *at(key);
        const Result<double> parsed = autox_settings_num_parse(i, g.word_of(m));
        if (!parsed) g.fail(m.value, parsed.error().what);
        s.num[i] = *parsed;
        s.has_num[i] = 1;
    }
    for (std::size_t i = 0; i < autox_codim_kinds.size(); ++i) {
        bool enabled = false;
        at(autox_codim_keys[i]);
        g.flag(autox_codim_keys[i], enabled);
        s.special_points[i] = enabled;
    }
    {
        const odex::Member &m = *at("plot");
        s.plot = autox_projection_from_name(g.word_of(m));
        if (!s.plot) g.fail(m.value, autox_projection_unknown(g.word_of(m)));
    }
    for (const std::string_view key : {"var", "par1", "par2"}) {
        std::string &name = key == "var" ? s.var : key == "par1" ? s.par1 : s.par2;
        name = g.name_of(*at(key));
    }
    for (std::size_t i = 0; i < range_keys.size(); i++) {
        at(range_keys[i]);
        g.real(range_keys[i], s.range[i]);
        s.has_range[i] = 1;
    }
    std::vector<odex::Block> marks;
    g.groups("marks", marks, autox_file_marks_limit);
    const odex::Member &list = *g.member("marks");
    s.nmarks = 0;
    for (std::size_t i = 0; i < marks.size(); i++) {
        odex::Block &m = marks[i];
        r.places[xpp::format("mark{}", i)] = {list.value.items[i].line, list.value.items[i].col};
        s.mark_name.push_back(m.name_of(*m.member("name")));
        s.mark_value.emplace_back();
        m.real("value", s.mark_value.back());
        m.done();
        (*s.nmarks)++;
    }
    g.done();
    return r;
}

void write_views(odex::Out &out, const SavedViews &v)
{
    out.begin_list("views");
    for (const SavedView &w : v.views) {
        out.begin_item();
        out.text("name", w.name);
        out.flag("hidden", w.hidden);
        out.word("group", group_name(w.second));
        out.word("plot", autox_projection_name(w.plot));
        out.name("var", w.var);
        out.name("par1", w.par1);
        out.name("par2", w.par2);
        for (std::size_t i = 0; i < range_keys.size(); i++) out.real(range_keys[i], w.range[i]);
        write_zoom(out, w.zoom);
        out.end_item();
    }
    out.end_list();
    out.text("active", v.views[static_cast<std::size_t>(v.active)].name);
}

ViewsRead read_views(odex::Block &autox)
{
    ViewsRead r;
    SavedViews &v = r.views;
    std::vector<odex::Block> list;
    autox.groups("views", list, saved_views_limit);
    const odex::Member &member = *autox.member("views");
    if (list.empty()) autox.fail(member, "an autoX session has at least one view");
    for (std::size_t k = 0; k < list.size(); k++) {
        odex::Block &g = list[k];
        SavedView &s = v.views.emplace_back();
        r.places.emplace_back(member.value.items[k].line, member.value.items[k].col);
        g.text("name", s.name);
        if (const std::string problem = window_name_problem(s.name); !problem.empty()) g.fail(*g.member("name"), problem);
        for (std::size_t j = 0; j < k; j++)
            if (v.views[j].name == s.name) g.fail(*g.member("name"), "a view name is listed twice");
        g.flag("hidden", s.hidden);
        std::string group;
        g.word("group", group);
        const std::optional<bool> second = group_second_from_name(group);
        if (!second) g.fail(*g.member("group"), xpp::format("Unknown group \"{}\": {} or {}", group, GROUP_FIRST, GROUP_SECOND));
        s.second = *second;
        const odex::Member &plot = *g.member("plot");
        const std::optional<AutoxProjection> projection = autox_projection_from_name(g.word_of(plot));
        if (!projection) g.fail(plot.value, autox_projection_unknown(g.word_of(plot)));
        s.plot = *projection;
        s.var = g.name_of(*g.member("var"));
        s.par1 = g.name_of(*g.member("par1"));
        s.par2 = g.name_of(*g.member("par2"));
        for (std::size_t i = 0; i < range_keys.size(); i++) g.real(range_keys[i], s.range[i]);
        s.zoom = read_zoom(g);
        g.done();
    }
    std::string active;
    autox.text("active", active);
    const auto selected = std::ranges::find(v.views, active, &SavedView::name);
    if (selected == v.views.end()) autox.fail(*autox.member("active"), xpp::format("Unknown window \"{}\"", active));
    v.active = static_cast<int>(selected - v.views.begin());
    return r;
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
    /* sized once from the points, not grown by doubling (a long run's text is tens of MiB, and each doubling holds the old
       text beside the new): a row's numbers, each at most longest_number characters (-1.2345678901234567e-308), its
       separators and its fixed words; the pages the text does not reach are never touched */
    constexpr std::size_t longest_number = 24, row_words = 128;
    std::size_t bound = o.size();
    for (const DIAGRAM &p : points) bound += row_words + (6 + 6 * n + p.par.size() + p.parameter_indices.size()) * (longest_number + 1);
    o.reserve(bound);
    for (const DIAGRAM &p : points) {
        const DIAGRAM &d = p;
        /* an axis is written as the file's integer (autox_axis.h) */
        const auto axis_code = [model_parameters](AutoxAxis a) { return autox_axis_encode(a, model_parameters); };
        o += xpp::format("{},{},{},{},{},{},{},{},{},{},{},{},{}", autox_calculation_name(d.calc), d.ibr, d.ntot, autox_point_text(d.itp), d.lab, d.nfpar, axis_code(d.icp1), axis_code(d.icp2),
                         axis_code(d.icp3), axis_code(d.icp4), autox_calculation_name(d.flag2), d.from, d.jacobian);
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
       parameter is one (its name is indexed by it); the others may be autoX's own
       unknowns, such as the period of a periodic run (the axes were decoded, so each is
       a model parameter or one of autoX's slots). */
    if (!d.icp1.is_parameter())
        l.fail(line, xpp::format("axis icp1 is autoX's slot {}, not one of the model's {} parameters", d.icp1.index, n_pars));
    if (d.jacobian < 0 || d.jacobian >= static_cast<int>(jacobian_names.size()))
        l.fail(line, xpp::format("jacobian {} is neither {} ({}) nor {} ({})", d.jacobian,
                              jacobian_names[JACOBIAN_DIFFERENCE], JACOBIAN_DIFFERENCE, jacobian_names[JACOBIAN_EXACT], JACOBIAN_EXACT));
    if (d.nfpar < 0 || static_cast<std::size_t>(d.nfpar) > saved_parameters_limit)
        l.fail(line, xpp::format("nfpar {} is outside autoX's parameter storage", d.nfpar));
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
    return read_lines("autoX's diagram", std::move(file), text, [n, n_pars](Lines &l) {
        if (n < 0 || n_pars < 0 || static_cast<std::size_t>(n_pars) > saved_parameters_limit) l.fail("invalid model dimensions for autoX diagram storage");
        std::vector<std::shared_ptr<const DiagramBranch>> branches;
        std::size_t expanded_bytes=0;
        std::string_view header=l.next("the branch records and point header");
        while(header.starts_with("branch,")){
            const auto fields=split_fields(header,',');
            if(fields.size()!=static_cast<std::size_t>(n_pars)+2)l.fail("branch parameter count does not match the model");
            int id;
            if(!parse_int(fields[1],id) || id!=static_cast<int>(branches.size()))l.fail("branch ids must be contiguous from zero");
            const std::size_t bytes=sizeof(DiagramBranch)+static_cast<std::size_t>(n_pars)*sizeof(double);
            if(branches.size()>=saved_points_limit || !account_memory(expanded_bytes, 1, bytes))l.fail("too many autoX branches for the session memory limit");
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
                l.fail("too many autoX diagram points for the session memory limit");
            DIAGRAM &p = points.emplace_back();
            DIAGRAM &d = p;
            std::string_view point_type;
            int axis_codes[4] = {};
            constexpr std::size_t type_column = 3;
            int *ints[n_ints] = {nullptr, &d.ibr, &d.ntot, nullptr, &d.lab, &d.nfpar, &axis_codes[0], &axis_codes[1], &axis_codes[2], &axis_codes[3], nullptr, &d.from, &d.jacobian};
            std::size_t c = 0;
            const auto bad = [&](std::size_t k, const char *kind) {
                l.fail(xpp::format("field {} (\"{}\") is not {}", k + 1, f[k], kind));
            };
            for (int *v : ints) {
                if (c == type_column) {
                    point_type = f[c];
                } else if(v) {
                    if (!xpp::parse_int(f[c], *v) || *v == std::numeric_limits<int>::min()) bad(c, "a whole number");
                } else {
                    const auto kind=autox_calculation_from_name(f[c]);
                    if(!kind) bad(c,"a calculation name");
                    (c==0 ? d.calc : d.flag2)=*kind;
                }
                c++;
            }
            const auto itp = autox_point_from_text(point_type);
            if (!itp) l.fail(xpp::format("field 4 (\"{}\"): {}", point_type, autox_point_text_unknown(point_type)));
            d.itp = *itp;
            const std::array<AutoxAxis *, 4> axes = {&d.icp1, &d.icp2, &d.icp3, &d.icp4};
            for (std::size_t k = 0; k < axes.size(); k++) {
                const auto decoded = autox_axis_decode(axis_codes[k], n_pars);
                if (!decoded) l.fail(xpp::format("axis {} is neither one of the model's {} parameters nor one of autoX's slots", axis_codes[k], n_pars));
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
            if(!account_memory(expanded_bytes, 1, point_bytes))l.fail("too many autoX diagram points for the session memory limit");
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
    std::span<const int> imported, std::span<const double> parameters, AutoxCalculation calc, AutoxCalculation flag2, double torper)
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
        int calc_code, curve_code;
        lines.numbers(calc_code, d.ibr, d.ntot, itp_code, d.lab, foreign_index, d.nfpar,
                      slots[0], slots[1], slots[2], slots[3], curve_code);
        const auto imported_calc=autox_calculation_from_code(calc_code), imported_curve=autox_calculation_from_code(curve_code);
        if(!imported_calc || !imported_curve) lines.fail("unknown XPPAUT calculation code");
        d.calc=*imported_calc; d.flag2=*imported_curve;
        const auto itp = autox_point_decode(itp_code);
        if (!itp) lines.fail(autox_point_unknown(itp_code));
        d.itp = *itp;
        const int point_line=lines.line();
        if (foreign_index != index || d.nfpar < 1 || d.nfpar > foreign_continuation_columns)
            lines.fail("invalid foreign point index or free parameter count");
        for (int slot : slots)
            if (slot < 0 || slot >= autox_unknown::block_size)
                lines.fail("foreign parameter index is outside autoX's slots");
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
        AutoxAxis *const axes[] = {&d.icp1, &d.icp2, &d.icp3, &d.icp4};
        for (int k = 0; k < 4; k++) *axes[k] = *autox_axis_decode(mapped(slots[k]), static_cast<int>(parameters.size())); /* the slots were checked above */
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

} // namespace xpp::snapx::autox_members
