#include "load_args.h"
#include "auto_unknowns.h"
/* The session's AUTO members (xpp_session_auto.h, W92, W103): its settings, diagram and
   views (W50) read back bit for bit, a file of another shape is refused,
   and in a session the whole file (the model, settings, diagram,
   solutions, two views)
   written and restored gives the same AUTO state, in the same model or
   with its saved model loaded from it (the .ode on the disk edited); a
   file without its model is refused; an XPPAUT .auto is imported
   (tools/models/lecar_diagram.auto, lecar's diagram as XPPAUT wrote it)
   and its AUTO members restore exactly what was imported. */
#include "xpptest.h"
#include "xpp_session_auto.h"
#include "snapx.h"
#include "session.h"
#include "model.h"
#include "diagram.h"
#include "jacobian.h"
#include "auto_nox.h"
#include "auto_c.h"
#include "auto_settings.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "xpp_zip.h"
#include "xpp_session.h"
#include "model_files.h"
#include "browse.h"
#include "many_pops.h"
#include "auto_parallel.h"
#include "xpp_job.h"
#include "solver.h"
#include "xpp_globals.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

/* doubles "%g" would round and a few edge cases */
const std::vector<double> awkward = {0.1 + 0.2,
                                     1.0 / 3.0,
                                     -2.0 / 7.0,
                                     1e-300,
                                     -4.9406564584124654e-324,
                                     std::numeric_limits<double>::max(),
                                     -0.0,
                                     123456789.12345678,
                                     std::nextafter(1.0, 2.0),
                                     -std::numeric_limits<double>::min()};

double awk(std::size_t i) { return awkward[i % awkward.size()]; }

bool same_bits(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

template <class Values> bool same_vector(const Values &a, const Values &b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++)
        if (!same_bits(a[i], b[i])) return false;
    return true;
}

/* two points the same, every field bit for bit */
bool same_point(const xpp::DIAGRAM &a, const xpp::DIAGRAM &b)
{
    if (a.calc != b.calc || a.ibr != b.ibr || a.ntot != b.ntot || a.itp != b.itp || a.lab != b.lab || a.nfpar != b.nfpar ||
        a.icp1 != b.icp1 || a.icp2 != b.icp2 || a.icp3 != b.icp3 || a.icp4 != b.icp4 || a.flag2 != b.flag2 || a.from != b.from || a.jacobian != b.jacobian)
        return false;
    if (!same_bits(a.norm, b.norm) || !same_bits(a.per, b.per) || !same_bits(a.torper, b.torper)) return false;
    if(a.parameter_indices!=b.parameter_indices || !same_vector(a.par,b.par) || !same_vector(a.branch->parameters,b.branch->parameters))return false;
    return same_vector(a.u0, b.u0) && same_vector(a.uhi, b.uhi) && same_vector(a.ulo, b.ulo) && same_vector(a.ubar, b.ubar) &&
           same_vector(a.evr, b.evr) && same_vector(a.evi, b.evi);
}

bool same_diagram(const std::deque<xpp::DIAGRAM> &a, const std::deque<xpp::DIAGRAM> &b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++)
        if (!same_point(a[i], b[i])) return false;
    return true;
}

/* k points of n variables, every value awkward */
std::deque<xpp::DIAGRAM> points_of(int k, int n)
{
    std::deque<xpp::DIAGRAM> pts;
    std::size_t v = 0;
    for (int p = 0; p < k; p++) {
        xpp::DIAGRAM &d = pts.emplace_back();
        d.ibr = 1 + p / 4;
        d.ntot = (p % 5) - 2;
        d.itp = *xpp::auto_point_decode(p % 9);
        d.lab = p % 2 ? p : 0;
        d.calc = *xpp::auto_calculation_from_code(p % 3);
        d.norm = awk(v++);
        d.per = awk(v++);
        d.torper = awk(v++);
        d.nfpar = 2;
        d.icp1 = xpp::AutoAxis::parameter(0);
        d.icp2 = xpp::AutoAxis::parameter(1);
        d.icp3 = xpp::AutoAxis::parameter(2);
        d.icp4 = xpp::AutoAxis::unknown(0); /* the test model has 3 parameters: 3 is AUTO slot 0 */
        d.flag2 = *xpp::auto_calculation_from_code(p % 2);
        d.from = p == 1 ? 7 : 0;
        d.jacobian = p / 4 == 1 ? xpp::JACOBIAN_EXACT : xpp::JACOBIAN_DIFFERENCE; /* one branch of each kind */
        auto branch=std::make_shared<xpp::DiagramBranch>();
        branch->parameters.resize(3);
        for(double &value:branch->parameters)value=awk(v++);
        d.branch=std::move(branch);
        d.parameter_indices={0,1};
        d.par.resize(2);
        for (double &x : d.par) x = awk(v++);
        for (std::vector<double> *a : {&d.u0, &d.uhi, &d.ulo, &d.ubar, &d.evr, &d.evi}) {
            a->resize(n);
            for (double &x : *a) x = awk(v++);
        }
    }
    return pts;
}

void check_settings_text()
{
    xpp::AutoSettingsSet s;
    s.special_points={true,false,true,false,false,true,false,true,true,false,true,false};
    for (int i = 0; i < xpp::AUTO_NUM_N; i++) s.num[i] = awk(i) / (i + 1);
    s.num[xpp::AUTO_NUM_SUPPBP]=1;
    s.plot = xpp::AutoProjection::average;
    s.var = "v";
    s.par1 = "iapp";
    s.par2 = "";
    s.range = {awk(0), awk(1), awk(2), awk(3)};
    s.nmarks = 2;
    s.mark_name = {"T", "phi"};
    s.mark_value = {awk(4), awk(5)};
    const xpp::Result<xpp::snapx::auto_members::SettingsRead> read = xpp::snapx::auto_members::parse_settings(xpp::snapx::auto_members::settings_text(s), "settings.txt");
    CHECK(read.has_value());
    if (!read) return;
    const xpp::AutoSettingsSet *r = &read->set;
    bool num = true;
    for (int i = 0; i < xpp::AUTO_NUM_N; i++) num = num && r->has_num[i] && same_bits(r->num[i], s.num[i]);
    CHECK(num);
    CHECK(r->special_points==s.special_points);
    CHECK(r->plot == xpp::AutoProjection::average && r->var == "v" && r->par1 == "iapp" && r->par2.empty());
    bool range = true;
    for (int i = 0; i < 4; i++) range = range && r->has_range[i] && same_bits(r->range[i], s.range[i]);
    CHECK(range);
    CHECK(r->nmarks == 2 && r->mark_name[0] == "T" && r->mark_name[1] == "phi" && same_bits(r->mark_value[0], s.mark_value[0]) &&
          same_bits(r->mark_value[1], s.mark_value[1]));
    std::string excessive;
    for(int i=0;i<=xpp::auto_file_marks_limit;i++)excessive+="mark T 1\n";
    const auto bounded=xpp::snapx::auto_members::parse_settings(excessive,"sent.snapx/auto/settings.txt");
    CHECK(!bounded && bounded.error().place.line==xpp::auto_file_marks_limit+1);
    CHECK(!bounded && bounded.error().place.source=="mark T 1");
    /* each refusal at its line, the line as written (W125) */
    const auto error = [](const std::string &text) {
        const xpp::Result<xpp::snapx::auto_members::SettingsRead> e = xpp::snapx::auto_members::parse_settings(text, "a.snapx/auto/settings.txt");
        return e ? std::string() : e.error().text() + " [" + e.error().place.source + "]";
    };
    CHECK(error("ds not-a-number\n") == "a.snapx/auto/settings.txt:1: \"not-a-number\" is not a number [ds not-a-number]");
    CHECK(error("plot 1\n") == "a.snapx/auto/settings.txt:1: \"1\" is not a projection (maximum, norm, maximum_and_minimum, period, two_parameter, frequency, average) [plot 1]");
    /* every key there once, none it does not have */
    const std::string text = xpp::snapx::auto_members::settings_text(s);
    const int lines = static_cast<int>(xpp::split_lines(text).size());
    CHECK(error("BT 1\n")=="a.snapx/auto/settings.txt:1: BT must be true or false, got 1 [BT 1]");
    CHECK(!xpp::snapx::auto_members::parse_settings(text+"BT true\n","duplicate.txt"));
    auto missing=text; missing.erase(missing.find("BT true\n"),8);
    CHECK(!xpp::snapx::auto_members::parse_settings(missing,"missing.txt"));
    CHECK(error(text + "later 1\n") == xpp::format("a.snapx/auto/settings.txt:{}: later is not one of AUTO's settings [later 1]", lines + 1));
    CHECK(error(text + "ntst 3\n") == xpp::format("a.snapx/auto/settings.txt:{}: ntst given twice [ntst 3]", lines + 1));
    CHECK(error(text.substr(text.find('\n') + 1)) == xpp::format("a.snapx/auto/settings.txt:{}: the file ends here, without its ntst line []", lines));
    CHECK(read->lines.at("ntst") == 1 && read->lines.at("mark1") == lines);
}

/* views.txt (W50): the views read back bit for bit, a degenerate range
   (a Fit of a flat quantity) and no zoom included; a file of another
   shape is refused */
void check_views_text()
{
    xpp::snapx::auto_members::SavedViews v;
    v.views.push_back({xpp::AutoProjection::maximum_and_minimum, "v", "iapp", "phi", {awk(0), awk(1), awk(2), awk(3)}, {}});
    xpp::Zoom z;
    z.x = {true, awk(4), awk(5) + 1};
    v.views.push_back({xpp::AutoProjection::period, "", "iapp", "", {-0.2, 0.08, 0, 0}, z});
    v.views[0].name="diagram_1";
    v.views[1].name="diagram_2";
    v.active = 1;
    const std::string text = xpp::snapx::auto_members::views_text(v);
    const xpp::Result<xpp::snapx::auto_members::ViewsRead> r = xpp::snapx::auto_members::parse_views(text, "views.txt");
    CHECK(r && r->views == v && r->lines == std::vector<int>({1, 2}));
    CHECK(text.ends_with("active \"diagram_2\"\n"));
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" maximum_and_minimum v iapp phi 0 1 0 1 - -\n", "views.txt"));                /* no active line */
    CHECK(!xpp::snapx::auto_members::parse_views("active \"diagram_1\"\n", "views.txt"));                                     /* no view */
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" maximum_and_minimum v iapp phi 0 1 0 1 - -\nactive \"missing\"\n", "views.txt"));     /* no view 1 */
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" maximum_and_minimum v iapp phi 0 1 0 1 1:0 -\nactive \"diagram_1\"\n", "views.txt"));   /* a zoom low above high */
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" maximum_and_minimum v iapp phi 0 1 0 -\nactive \"diagram_1\"\n", "views.txt"));        /* a field short */
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" 2 v iapp phi 0 1 0 1 - -\nactive \"diagram_1\"\n", "views.txt"));   /* a projection is its name */
}

void check_solutions()
{
    const int count=xpp::auto_unknown::block_size+2;
    std::string values;
    for(int k=0;k<count;k++)values+=(k?" 0":"0");
    const std::string algebraic=xpp::format("1 1 4 1 2 0 1 3 2 0 0 {}\n0 1 2\n{}\n",count,values);
    CHECK(xpp::check_auto_solutions(algebraic, "sent.snapx/auto/solutions.s", 24, count,false));
    const std::string old="1 1 4 1 2 0 1 3 2 0 0 2\n0 1 2\n0 0\n";
    const auto layout=xpp::check_auto_solutions(old,"sent.snapx/auto/solutions.s",24,count,false);
    CHECK(!layout && layout.error().place.line==1 && layout.error().place.source.starts_with("1 1 4")
          && layout.error().place.file=="sent.snapx/auto/solutions.s");
    CHECK(xpp::check_auto_solutions(old,"foreign.auto",24,count,true));
    CHECK(!xpp::check_auto_solutions(xpp::format("1 1 4 1 2 0 1 9999 2 0 0 {}\n0 1 2\n{}\n",count,values), "solutions.s", 24, count,false));
    CHECK(!xpp::check_auto_solutions(xpp::format("1 1 4 1 2 0 1 3 2 0 0 {} junk\n0 1 2\n{}\n",count,values), "solutions.s", 24, count,false));
    CHECK(!xpp::check_auto_solutions(xpp::format("1 1 4 1 2 0 1 3 2 0 0 {}\n0 1 2\n{} nan\n",count,values), "solutions.s", 24, count,false));
    const auto cut=xpp::check_auto_solutions(xpp::format("1 1 4 1 2 0 1 3 2 0 0 {}\n0 1 2\n0\n",count), "sent.snapx/auto/solutions.s", 24, count,false);
    CHECK(!cut && cut.error().place.file=="sent.snapx/auto/solutions.s" && cut.error().place.line==3);
    const std::string periodic=xpp::format("1 1 2 1 1 1 3 2 5 1 2 {}\n0 1 0.5 2 1 3\n{}\n1\n1 1 1\n{}\n",count,xpp::auto_unknown::block_size,values);
    CHECK(xpp::check_auto_solutions(periodic, "solutions.s", 24, count,false));
    CHECK(!xpp::check_auto_solutions(xpp::format("1 1 2 1 1 1 3 2 5 1 2 {}\n0 1 0.5 2 1 3\n9999\n1\n1 1 1\n{}\n",count,values), "solutions.s", 24, count,false));
    xpp::TokenReader header=xpp::TokenReader::of_text("    2-1234  ");
    long a,b;
    CHECK(!header.at_end() && header.read(a) && a==2 && header.read(b) && b==-1234 && header.at_end());
}

void check_untrusted_zoom()
{
    CHECK(!xpp::snapx::auto_members::parse_views("view \"diagram_1\" maximum_and_minimum v iapp phi 0 1 0 1 -inf:inf -\nactive \"diagram_1\"\n", "sent.snapx/auto/views.txt"));
}

void check_diagram_csv()
{
    const std::vector<std::string> vars = {"v", "w", "long_name_of_a_variable"};
    const std::deque<xpp::DIAGRAM> pts = points_of(7, 3);
    const std::string csv = xpp::snapx::auto_members::diagram_csv(pts, 3, vars);
    CHECK(csv.starts_with("branch,0,") && csv.find("calc,ibr,ntot,itp,lab,nfpar,icp1,icp2,icp3,icp4,flag2,from,jacobian,norm,per,torper,branch,continued,values,")!=std::string::npos);
    CHECK(csv.find(",u0.long_name_of_a_variable,") != std::string::npos && csv.find(",evi3\n") != std::string::npos);
    const xpp::Result<std::deque<xpp::DIAGRAM>> back = xpp::snapx::auto_members::parse_diagram_csv(csv, 3, 3, "diagram.csv");
    CHECK(back && same_diagram(*back, pts)); /* bit for bit */
    CHECK(!xpp::snapx::auto_members::parse_diagram_csv(csv, 2, 3, "diagram.csv")); /* of another model */
    CHECK(!xpp::snapx::auto_members::parse_diagram_csv("", 3, 3, "diagram.csv"));
    std::string bad = csv;
    bad.back() = ',';
    const xpp::Result<std::deque<xpp::DIAGRAM>> long_row = xpp::snapx::auto_members::parse_diagram_csv(bad + "1\n", 3, 3, "diagram.csv");
    CHECK(!long_row && long_row.error().place.line == static_cast<int>(pts.size()) * 2 + 1); /* a row too long, at its line */
    /* Native calculation columns accept names only, at the point's line. */
    for (const std::string token : {"9", "unknown_calculation"}) {
        std::string bad_calc=csv;
        const auto first=csv.find('\n',csv.find("calc,ibr"))+1;
        bad_calc.replace(first,csv.find(',',first)-first,token);
        const auto refused=xpp::snapx::auto_members::parse_diagram_csv(bad_calc,3,3,"sent.snapx/auto/diagram.csv");
        CHECK(!refused && refused.error().place.line==static_cast<int>(pts.size())+2);
    }
    /* a jacobian that is neither setting, at its line; a diagram without the column (saved before W278) */
    std::string wrong = csv;
    const std::size_t header=csv.find("calc,ibr");
    std::size_t at = csv.find('\n',header) + 1; /* the first row: its 13th field is the jacobian */
    for (int commas = 0; commas < 12; at++) commas += csv[at] == ',';
    wrong.replace(at, csv.find_first_of(",\n", at) - at, "2");
    const xpp::Result<std::deque<xpp::DIAGRAM>> bad_jacobian = xpp::snapx::auto_members::parse_diagram_csv(wrong, 3, 3, "diagram.csv");
    CHECK(!bad_jacobian && bad_jacobian.error().place.line == static_cast<int>(pts.size())+2 && bad_jacobian.error().text().find("jacobian 2") != std::string::npos);
    /* a point type that is none of AUTO's (W329), at the point's line, with its text */
    std::string bad_type = csv;
    std::size_t type_at = csv.find('\n', header) + 1; /* the first row: its 4th field is the type */
    for (int commas = 0; commas < 3; type_at++) commas += csv[type_at] == ',';
    bad_type.replace(type_at, csv.find(',', type_at) - type_at, "-5");
    const xpp::Result<std::deque<xpp::DIAGRAM>> unknown_type = xpp::snapx::auto_members::parse_diagram_csv(bad_type, 3, 3, "diagram.csv");
    CHECK(!unknown_type && unknown_type.error().place.line == static_cast<int>(pts.size()) + 2
          && unknown_type.error().text().find("-5 is not an AUTO point type") != std::string::npos);
    /* an axis that is neither one of the model's parameters nor one of AUTO's slots (W330), at the point's line */
    std::string bad_axis = csv;
    std::size_t axis_at = csv.find('\n', header) + 1; /* the first row: its 8th field is icp2 */
    for (int commas = 0; commas < 7; axis_at++) commas += csv[axis_at] == ',';
    bad_axis.replace(axis_at, csv.find(',', axis_at) - axis_at, "9999");
    const xpp::Result<std::deque<xpp::DIAGRAM>> unknown_axis = xpp::snapx::auto_members::parse_diagram_csv(bad_axis, 3, 3, "diagram.csv");
    CHECK(!unknown_axis && unknown_axis.error().place.line == static_cast<int>(pts.size()) + 2
          && unknown_axis.error().text().find("axis 9999") != std::string::npos);
    const std::size_t column = csv.find("jacobian,");
    const xpp::Result<std::deque<xpp::DIAGRAM>> old = xpp::snapx::auto_members::parse_diagram_csv(csv.substr(0, column) + csv.substr(column + 9), 3, 3, "diagram.csv");
    CHECK(!old && old.error().place.line == static_cast<int>(pts.size())+1);
    std::string bad_branch=csv;
    const auto value=bad_branch.find(',',bad_branch.find(',')+1)+1;
    bad_branch.replace(value,bad_branch.find(',',value)-value,"nan");
    const auto nonfinite=xpp::snapx::auto_members::parse_diagram_csv(bad_branch,3,3,"sent.snapx/auto/diagram.csv");
    CHECK(!nonfinite && nonfinite.error().place.line==1 && nonfinite.error().place.source.starts_with("branch,0,nan,"));
    bad_branch=csv;
    bad_branch.replace(bad_branch.find("branch,1,"),9,"branch,0,");
    const auto duplicate=xpp::snapx::auto_members::parse_diagram_csv(bad_branch,3,3,"sent.snapx/auto/diagram.csv");
    CHECK(!duplicate && duplicate.error().place.line==2);
    std::string bad_indices=csv;
    bad_indices.replace(bad_indices.find(",0;1,"),5,",0;0,");
    const auto repeated=xpp::snapx::auto_members::parse_diagram_csv(bad_indices,3,3,"sent.snapx/auto/diagram.csv");
    CHECK(!repeated && repeated.error().place.line==static_cast<int>(pts.size())+2
          && repeated.error().what=="continued parameter given twice");
}

bool write_file(const std::string &path, std::string_view bytes)
{
    xpp::Writer w = xpp::Writer::binary(path.c_str());
    return w && w.write(bytes) && w.commit();
}

std::string file_text(const std::string &path)
{
    std::string s;
    xpp::read_bytes(path.c_str(), s);
    return s;
}

bool load(const std::string &ode, const xpp::SavedModel *saved = nullptr)
{
    std::string arg0 = "test_session_auto", arg1 = ode;
    char *argv[] = {arg0.data(), arg1.data(), nullptr};
    if (!load_args(2, argv, 1, saved)) return false;
    init_browser(xpp::client_session());
    init_all_graph(xpp::client_session());
    xpp::client_session().plot_windows.graph[0].Use = 1;
    return true;
}

// W340/#401: a model without parameters cannot select index zero in an empty model span.
void check_parameterless_model()
{
    xpp::TempDir tmp;
    const std::string model=tmp.file("no-parameters.ode");
    CHECK(write_file(model,"x'=-x\ninit x=0\ndone\n"));
    CHECK(load(model));
    auto &s=xpp::client_session();
    s.auto_state.dir=tmp.path();
    CHECK(s.model().nupar==0);
    const int points=diagram_count(s.diagram);
    xpp::auto_start_diff_ss(s);
    CHECK(!s.auto_state.diag_flag && diagram_count(s.diagram)==points);
    CHECK(!xpp::files::exists(xpp::auto_solutions_file(s))
          && !xpp::files::exists(s.auto_state.fort3) && !xpp::files::exists(s.auto_state.fort8));
}

/* W339/#400: a constant changed between branches must come from the older
   branch on grab and restart, including after reopening the entire session. */
void check_branch_parameters()
{
    xpp::TempDir tmp;
    const std::string model=tmp.file("branch-parameters.ode");
    CHECK(write_file(model,"par a=1,b=2\nx'=b-a*x\ninit x=2\n@ nmax=4,npr=1,ds=0.02,dsmin=0.001,dsmax=0.1,parmin=0.5,parmax=3,epsl=1e-10,epsu=1e-10,epss=1e-10\ndone\n"));
    CHECK(load(model));
    auto &s=xpp::client_session();
    s.auto_state.dir=tmp.path();
    xpp::init_auto_win(s);
    // Mark functions must read a held model constant even though AUTO never received it.
    AutoLib mark_lib;
    mark_lib.session=&s;
    mark_lib.parameter_count=s.auto_state.parameter_layout.size();
    iap_type mark_iap{};
    mark_iap.lib=&mark_lib;
    mark_iap.nfpr=1;
    rap_type mark_rap{};
    std::vector<double> parameters(mark_lib.parameter_count);
    std::array<integer,1> free={s.auto_state.parameter_layout.solver_index(0)};
    std::array<AutoMark,1> mark={AutoMark{static_cast<int>(s.auto_state.parameter_layout.solver_index(1)),0.5}};
    bool changed=false;
    const auto mark_value=[&]{
        return fnuzae(&mark_iap,&mark_rap,parameters.data(),free.data(),&changed,nullptr,nullptr,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,mark);
    };
    CHECK(mark_value()==1.5 && changed==true);
    mark[0].parameter=static_cast<int>(free[0]);
    parameters[free[0]]=0.75;
    CHECK(mark_value()==0.25);
    mark[0].parameter=xpp::auto_unknown::period;
    parameters[xpp::auto_unknown::period]=7.5;
    CHECK(mark_value()==7);
    xpp::auto_start_diff_ss(s);
    CHECK(s.diagram.points.size()==4);
    const int label=s.diagram.points.front().lab;
    CHECK(label>0);
    const auto older=s.diagram.points.front().branch;
    CHECK(older->parameters.size()==2 && older->parameters[1]==2);
    CHECK(s.diagram.points.back().branch==older && s.diagram.points.back().par.size()==1);
    s.parser.constants[s.model().upar_con[1]]=7;
    s.last_ic[0]=7/s.parser.constants[s.model().upar_con[0]];
    const xpp::XppUi saved_ui=xpp::ui;
    xpp::XppUi retain_ui=saved_ui;
    retain_ui.question_policy=[](bool){return xpp::QuestionPolicy::User;};
    retain_ui.choose=[](std::string_view,std::string_view question,std::span<const std::string>,std::string_view keys,char)->int{
        CHECK(question=="Destroy AUTO diagram & files" && keys=="yn");
        return 'n';
    };
    xpp::set_ui(&retain_ui);
    xpp::auto_new_ss(s);
    xpp::set_ui(&saved_ui);
    CHECK(s.diagram.points.size()==8 && s.diagram.points.back().branch!=older);
    CHECK(s.diagram.points.back().branch->parameters[1]==7 && older->parameters[1]==2);
    CHECK(xpp::auto_grab_label(s,label)==1 && s.parser.constants[s.model().upar_con[1]]==2);
    s.parser.constants[s.model().upar_con[1]]=9;
    xpp::auto_restart(s,label);
    CHECK(s.parser.constants[s.model().upar_con[1]]==2 && s.diagram.points.back().branch->parameters[1]==2);
    CHECK(std::abs(diagram_parameter(s.diagram.points.back(),0)*s.diagram.points.back().u0[0]-2)<1e-8);
    const auto bytes=xpp_session_snapshot(s);
    CHECK(bytes.has_value());
    const auto saved=bytes?xpp_saved_parse(tmp.file("branches.snapx"),"branches.snapx",*bytes,SavedKind::snapshot):std::nullopt;
    s.parser.constants[s.model().upar_con[1]]=11;
    start_diagram(s,1);
    CHECK(saved && xpp_saved_restore(s,*saved));
    CHECK(xpp::auto_grab_label(s,label)==1 && s.parser.constants[s.model().upar_con[1]]==2);
    s.parser.constants[s.model().upar_con[1]]=13;
    xpp::auto_restart(s,label);
    CHECK(s.parser.constants[s.model().upar_con[1]]==2 && s.diagram.points.back().branch->parameters[1]==2);
    CHECK(std::abs(diagram_parameter(s.diagram.points.back(),0)*s.diagram.points.back().u0[0]-2)<1e-8);
    s.auto_state.axes().icp1=1;
    s.parser.constants[s.model().upar_con[0]]=19;
    xpp::auto_restart(s,label);
    CHECK(s.parser.constants[s.model().upar_con[0]]==1 && s.diagram.points.back().icp1.is_parameter(1));
    CHECK(std::abs(s.diagram.points.back().u0[0]-diagram_parameter(s.diagram.points.back(),1))<1e-8);
}

/* the whole file for this session: written with its model, the state
   changed, restored into the same model */
void check_orbit_growth(const xpp::TempDir &tmp)
{
    xpp::Session &s = xpp::client_session();
    std::string parameters;
    for(int k=0;k<s.auto_state.parameter_layout.size();k++)parameters+=(k?" 0":"0");
    const std::string orbit=xpp::format("1 1 2 1 1 1 3 3 9 1 2 {}\n0 1 2\n0.5 3 4\n1 5 6\n{}\n1\n0 0\n0 0\n0 0\n{}\n",s.auto_state.parameter_layout.size(),s.auto_state.parameter_layout.solver_index(0),parameters);
    CHECK(xpp::check_auto_solutions(orbit, "orbit.s", 24, s.auto_state.parameter_layout.size(),false));
    s.auto_state.file = tmp.file("orbit");
    CHECK(write_file(xpp::auto_solutions_file(s), orbit));
    s.data_store.allocate(2, s.model().neq + 1);
    CHECK(xpp::load_auto_orbitx(s, -1, 1, 1, 2));
    CHECK(s.data_store.rows == 3 && s.data_store.max_rows >= 3);
    CHECK(s.data_store.col[0][0] == 0 && s.data_store.col[0][1] == 1 && s.data_store.col[0][2] == 2);
    CHECK(s.data_store.col[1][0] == 1 && s.data_store.col[1][2] == 5 && s.data_store.col[2][0] == 2 && s.data_store.col[2][2] == 6);
}

void check_session_round_trip(const xpp::TempDir &tmp)
{
    xpp::Session &s = xpp::client_session();
    const int n = xpp::client_session().model().node;
    CHECK(n == 2);
    std::deque<xpp::DIAGRAM> pts = points_of(9, n);
    for (xpp::DIAGRAM &p : pts) { /* parameters this model's AUTO has */
        p.icp1 = xpp::AutoAxis::parameter(0);
        p.icp2 = xpp::AutoAxis::parameter(1);
        p.icp3 = xpp::AutoAxis::parameter(0);
        p.icp4 = xpp::AutoAxis::parameter(1);
        auto branch=std::make_shared<xpp::DiagramBranch>(*p.branch);
        branch->parameters.resize(s.model().nupar,0.0);
        p.branch=std::move(branch);
    }
    pts.back().icp2 = xpp::AutoAxis::period();
    pts.back().parameter_indices.pop_back();
    pts.back().par.pop_back(); /* the period is kept separately from model parameters */
    diagram_restore(s, pts);
    CHECK(diagram_count(xpp::client_session().diagram) == 9 && xpp::client_session().diagram.points[3].uhi[1] == pts[3].uhi[1]);
    std::string solutions;
    const int parameter_rows=(s.auto_state.parameter_layout.size()+6)/7;
    // One orbit row, then seven-column parameter rows, matching AUTO's writer.
    for (int value : {1,1,4,1,2,0,1,3,1+parameter_rows,0,0,s.auto_state.parameter_layout.size()})solutions+=xpp::format("{:5}",value);
    solutions+="\n0.0 1 2\n";
    for(int k=0;k<s.auto_state.parameter_layout.size();k++)solutions+="0"+std::string((k+1)%7==0 || k+1==s.auto_state.parameter_layout.size()?"\n":" ");
    CHECK(write_file(xpp::auto_solutions_file(xpp::client_session()), solutions));
    s.auto_state.bifur.ds = 0.1 + 0.2;
    s.auto_state.bifur.dsmax = 0.5;
    s.auto_state.bifur.rl1 = 1.0 / 3.0;
    xpp::AutoSettingsSet mark_set;
    mark_set.nmarks=128;
    mark_set.mark_name.resize(128,"T");
    for(int i=0;i<128;i++)mark_set.mark_value.push_back(awk(i));
    std::string why;
    CHECK(xpp::auto_settings_apply(s,mark_set,why)==0);
    const xpp::AutoSettingsSet before = xpp::auto_settings_now(xpp::client_session());
    /* a second view (W50), the norm, a zoom of its own; the first active */
    xpp::auto_new_view(xpp::client_session());
    s.auto_state.views[1].axes.plot = xpp::AutoProjection::norm;
    s.auto_state.views[1].axes.ymax = 5;
    s.auto_state.views[1].zoom.y = {true, 0.5, 1.5};
    s.auto_state.active_view = 0;

    const std::optional<std::string> bytes = xpp_session_snapshot(xpp::client_session());
    CHECK(bytes.has_value());
    if (!bytes) return;
    const std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(*bytes);
    const std::string model_member = "model/" + xpp::client_session().model().this_file;
    CHECK(entries && (*entries)[0].name == "session.setx" && (*entries)[1].name == model_member);
    const xpp::Result<xpp::snapx::Manifest> man =
        xpp::snapx::parse_manifest("a/session.setx", entries ? (*entries)[0].bytes : std::string());
    CHECK(entries && man && man->model_name == xpp::client_session().model().this_file);
    CHECK(entries && (*entries)[1].bytes == file_text(xpp::client_session().model().this_file));

    /* everything changed, then the file restored */
    s.auto_state.bifur.marks.clear();
    start_diagram(xpp::client_session(), n);
    CHECK(write_file(xpp::auto_solutions_file(xpp::client_session()), "other"));
    s.auto_state.bifur.ds = 0.05;
    s.auto_state.bifur.rl1 = 7;
    s.auto_state.views.resize(1);
    const std::string path = tmp.file("t.snapx");
    CHECK(write_file(path, *bytes));
    std::optional<SavedFile> f = xpp_saved_read(path);
    CHECK(f && f->model.files == xpp::client_session().model().files);
    CHECK(f && xpp_saved_restore(xpp::client_session(), *f));
    CHECK(same_diagram(s.diagram.points, pts));
    bool indexed = true;
    for (int i = 0; i < diagram_count(xpp::client_session().diagram); i++)
        indexed = indexed && xpp::client_session().diagram.points[i].index == i;
    CHECK(indexed);
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())) == solutions);
    CHECK(same_bits(s.auto_state.bifur.ds, 0.1 + 0.2) && same_bits(s.auto_state.bifur.rl1, 1.0 / 3.0));
    const xpp::AutoSettingsSet after = xpp::auto_settings_now(xpp::client_session());
    CHECK(xpp::snapx::auto_members::settings_text(after) == xpp::snapx::auto_members::settings_text(before));
    CHECK(s.auto_state.views.size() == 2 && s.auto_state.active_view == 0 && s.auto_state.views[1].axes.plot == xpp::AutoProjection::norm &&
          s.auto_state.views[1].axes.ymax == 5 && (s.auto_state.views[1].zoom.y == xpp::AxisRange{true, 0.5, 1.5}) &&
          !s.auto_state.views[1].zoom.x.set && s.auto_state.views[0].axes.plot == s.auto_state.axes().plot);

    /* another model loaded, the .ode on the disk edited: the file's own
       model loads from it, with the diagram */
    CHECK(write_file(xpp::client_session().model().this_file, "par a=1\nq'=-a*q\ndone\n"));
    CHECK(load(tmp.file("u.ode")));
    CHECK(f && load(f->manifest.model_name, &f->model) && xpp_saved_restore(xpp::client_session(), *f));
    CHECK(xpp::client_session().model().node == 2 && xpp::client_session().model().saved_in == path && same_diagram(xpp::client_session().diagram.points, pts));

    /* a file without its model, or that is not a zip, is refused */
    std::vector<xpp::zip::Entry> no_model = *entries;
    no_model.erase(no_model.begin() + 1);
    CHECK(write_file(path, xpp::zip::make_zip(no_model)));
    CHECK(!xpp_saved_read(path));
    CHECK(write_file(path, "PK\x03\x04 not really"));
    CHECK(!xpp_saved_read(path));
}

/* an XPPAUT .auto imported, then its AUTO members restored: the same */
void check_import(const std::string &auto_text, const xpp::TempDir &tmp)
{
    xpp::Session &s = xpp::client_session();
    const std::string path = tmp.file("lecar.auto");
    CHECK(write_file(path, auto_text));
    xpp::UniqueFile fp = xpp::open_read(path);
    CHECK(fp && xpp::import_auto_file(s, fp.get(), path).has_value());
    fp.reset();
    CHECK(diagram_count(xpp::client_session().diagram) == 2042);
    /* its first point, as the file prints it */
    const xpp::DIAGRAM *d = &xpp::client_session().diagram.points.front();
    CHECK(d && d->ibr == 1 && d->ntot == 1 && d->itp.is(xpp::AutoPointKind::End) && d->lab == 1 && diagram_parameter(*d,0) == 0.05 && d->u0[0] == -0.144 &&
          d->u0[1] == 0.03);
    CHECK(s.auto_state.bifur.ntst == 15 && s.auto_state.bifur.nmx == 2000 && s.auto_state.bifur.dsmin == 1e-05);
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())).size() > 100); /* the .s part */
    CHECK(s.auto_state.bifur.marks.empty());
    /* Sent foreign files must preserve every AUTO member after any late failure. */
    const auto before_settings = xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s));
    const auto before_diagram = xpp::snapx::auto_members::diagram_csv(s.diagram.points, s.model().nupar,
        std::span<const std::string>(s.model().uvar_names.data(), s.model().node));
    const auto before_solutions = file_text(xpp::auto_solutions_file(s));
    const auto before_parameters = s.auto_state.imported_parameters;
    const auto before_marks = s.auto_state.bifur.marks;
    s.auto_state.from_auto_flag = true;
    s.auto_state.grabpt.flag = s.auto_state.diagram_mark.state = 1;
    const auto rows = xpp::split_lines(auto_text);
    const auto refuse_import = [&](std::string bad, int line) {
        CHECK(write_file(path, bad));
        auto input = xpp::open_read(path);
        const auto result = xpp::import_auto_file(s, input.get(), path);
        CHECK(!result);
        if (!result) {
            CHECK(result.error().place.file == path && result.error().place.line == line);
            CHECK(result.error().place.source == std::string(xpp::line_place(path, bad, line).source));
        }
        CHECK(xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s)) == before_settings);
        CHECK(xpp::snapx::auto_members::diagram_csv(s.diagram.points, s.model().nupar,
            std::span<const std::string>(s.model().uvar_names.data(), s.model().node)) == before_diagram);
        CHECK(file_text(xpp::auto_solutions_file(s)) == before_solutions);
        CHECK(s.auto_state.imported_parameters == before_parameters);
        CHECK(s.auto_state.bifur.marks == before_marks);
        CHECK(s.auto_state.from_auto_flag && s.auto_state.grabpt.flag == 1 && s.auto_state.diagram_mark.state == 1);
    };
    const auto replace_row = [&](int row, std::string value) {
        std::string bad;
        for (std::size_t k = 0; k < rows.size(); k++) bad += (k == static_cast<std::size_t>(row) ? value : std::string(rows[k])) + '\n';
        return bad;
    };
    refuse_import(replace_row(0, "8 0 1 2 3 4 5 6 7 21"), 1);
    refuse_import(replace_row(0, "8 999 1 2 3 4 5 6 7 0"), 1);
    refuse_import(replace_row(15, "2147483647"), 16);
    refuse_import(replace_row(10, "10001 2000 500"), 11);
    refuse_import(replace_row(17, "nan 0.333 -0.01 0.15 0.1 0.145 1 -0.7 0.147092 0"), 18);
    refuse_import(replace_row(14, "-0.2 -0.5 0.5 0.4 999 2"), 15);
    refuse_import(replace_row(20, "9 1 2 0 0 1 1 999 1 2 3 0"), 21);
    refuse_import(replace_row(20, "9 1 2 0 0 1 1 8 1 2 3 0"), 21);
    std::string truncated;
    for (int k = 0; k < 21; k++) truncated += std::string(rows[k]) + '\n';
    refuse_import(truncated, 22);
    refuse_import(auto_text + "bad solution header\n", static_cast<int>(rows.size()) + 1);
    CHECK(write_file(path, auto_text));
    auto valid_again = xpp::open_read(path);
    CHECK(xpp::import_auto_file(s, valid_again.get(), path));
    CHECK(!s.auto_state.from_auto_flag && s.auto_state.grabpt.flag == 0 && s.auto_state.diagram_mark.state == 0);
    const std::deque<xpp::DIAGRAM> imported = s.diagram.points;
    const std::string solutions = file_text(xpp::auto_solutions_file(xpp::client_session()));

    std::vector<xpp::zip::Entry> entries;
    CHECK(xpp::snapx::auto_members::add_members(xpp::client_session(), entries, "auto/"));
    std::map<std::string, std::string> members;
    for (xpp::zip::Entry &e : entries) members[e.name] = std::move(e.bytes);
    CHECK(members.size() == 4 && members.contains("auto/diagram.csv") && members.contains("auto/views.txt"));
    start_diagram(xpp::client_session(), xpp::client_session().model().node);
    xpp::Result<xpp::snapx::auto_members::Members> read = xpp::snapx::auto_members::members_read(xpp::client_session(), members, "auto/", "lecar.snapx");
    CHECK(read && xpp::snapx::auto_members::restore_members(xpp::client_session(), std::move(*read)));
    /* one missing: named */
    std::map<std::string, std::string> cut = members;
    cut.erase("auto/solutions.s");
    const xpp::Result<xpp::snapx::auto_members::Members> none = xpp::snapx::auto_members::members_read(xpp::client_session(), cut, "auto/", "lecar.snapx");
    CHECK(!none && none.error().text() == "lecar.snapx: its auto/solutions.s is missing");
    /* all or nothing (W125): a value on settings.txt's last line that does
       not read, or a value AUTO refuses, is the error at its line, before
       anything is restored */
    const std::string settings = members["auto/settings.txt"];
    /* text with the line of key given value instead */
    const auto keyed = [](std::string text, const std::string &key, const std::string &value) {
        const std::size_t at = text.find(key + " ");
        return text.replace(at, text.find('\n', at) - at, key + " " + value);
    };
    const std::string last_bad = settings.substr(0, settings.rfind("ymax ")) + "ymax nope\n";
    cut = members;
    cut["auto/settings.txt"] = last_bad;
    const xpp::Result<xpp::snapx::auto_members::Members> bad = xpp::snapx::auto_members::members_read(xpp::client_session(), cut, "auto/", "lecar.snapx");
    CHECK(!bad && bad.error().place.file == "lecar.snapx/auto/settings.txt" &&
          bad.error().place.line == static_cast<int>(xpp::split_lines(last_bad).size()) && bad.error().place.source == "ymax nope");
    cut["auto/settings.txt"] = keyed(settings, "ncol", "9");
    const xpp::Result<xpp::snapx::auto_members::Members> refused = xpp::snapx::auto_members::members_read(xpp::client_session(), cut, "auto/", "lecar.snapx");
    CHECK(!refused && refused.error().place.line == xpp::AUTO_NUM_NCOL + 1 && refused.error().place.source == "ncol 9");
    cut["auto/settings.txt"] = keyed(settings, "ntst", std::to_string(xpp::auto_max_mesh_intervals + 1));
    const auto mesh_refused = xpp::snapx::auto_members::members_read(xpp::client_session(), cut, "auto/", "lecar.snapx");
    CHECK(!mesh_refused);
    if (!mesh_refused) CHECK(mesh_refused.error().place.file == "lecar.snapx/auto/settings.txt" && mesh_refused.error().place.line == 1);
    CHECK(same_diagram(s.diagram.points, imported));
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())) == solutions);


}

// W247: a normal form with derived parameters, fixed variables and a user
// function; compare all stored values, including Floquet eigenvalues.
// Meshes below and above W325's worker threshold for this two-state, Ncol=4 fixture.
constexpr long serial_mesh = 17;
constexpr long worker_mesh = 64;
std::deque<xpp::DIAGRAM> collocation_run(unsigned threads, bool compiled, bool impure=false,
                                        const std::string &benchmark={}, bool exact=false, long mesh=serial_mesh)
{
    xpp::TempDir tmp;
    const std::string file=benchmark.empty()?tmp.file("hopf.ode"):benchmark;
    if (benchmark.empty()) CHECK(write_file(file,
        "par mu=-0.1,w=1\n!frequency=w\nsquare(q)=q*q\nr=square(x)+square(y)\n"
        "x'=mu*x-frequency*y-x*r"+std::string(impure?"+0*shift(x,0)":"")+"\n"
        "y'=frequency*x+mu*y-y*r\ninit x=0,y=0\n"
        +xpp::format("@ ntst={},ncol=4,nmax=60,npr=10,ds=0.02,dsmax=0.05,dsmin=0.0001\n",mesh)+
        "@ parmin=-1,parmax=1,epsl=1e-8,epsu=1e-8,epss=1e-8\n"+std::string(exact?"@ jacobian=exact\n":"")+"done\n"));
    program.compile=compiled;
    CHECK(load(file));
    xpp::Session &s=xpp::client_session();
    CHECK(s.model().auto_rhs_pure==(compiled && !impure));
    s.auto_state.dir=tmp.path();
    xpp::init_auto_win(s);
    s.auto_state.threads=threads; /* the preference's count, selected once for the run */
    xpp::auto_start_diff_ss(s);
    CHECK(xpp::auto_grab_type_index(s,"HB",1)==1);
    const auto start=std::chrono::steady_clock::now();
    xpp::auto_new_per(s);
    // W340/#401: periodic continuation frees AUTO's period independently of the model.
    CHECK(std::any_of(s.diagram.points.begin(),s.diagram.points.end(),[](const xpp::DIAGRAM &p){
        return p.nfpar==2 && p.icp2.is_period()
            && p.parameter_indices==std::vector<int>{0};
    }));
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("perf: AUTO %u threads %.6f s, %zu diagram points\n",threads,seconds,s.diagram.points.size());
    CHECK(std::any_of(s.diagram.points.begin(),s.diagram.points.end(),[](const auto &p){return p.ibr<0;}));
    if (compiled && !impure) CHECK(s.auto_lib.collocation && (!benchmark.empty()
        || s.auto_lib.collocation->threads()==(mesh==serial_mesh?1:threads)));
    else CHECK(!s.auto_lib.collocation && !xpp::auto_parallel_ready(s));
    return s.diagram.points;
}

void check_collocation_purity()
{
    xpp::TempDir tmp;
    program.compile=true;
    struct Case { const char *source; bool pure; };
    const Case cases[]={
        {"par a=1\nx'=a*x\ninit x=1\ndone\n",true},
        {"par a=1\nf(q)=g(q)\ng(q)=shift(x,0)\nx'=f(x)\ninit x=1\ndone\n",false},
        {"par a=1\nf(q)=f(q)\nx'=f(x)\ninit x=1\ndone\n",false},
        {"par a=1\nx'=ran(1)\ninit x=1\ndone\n",false},
        {"par a=1\np=q+1\nq=x\nx'=p\ninit x=1\ndone\n",false},
        {"par a=1\nx'=sum(1,2)of(i'*x)\ninit x=1\ndone\n",true},
        {"par a=1\ntable tb % 3 0 1 t*a\nx'=tb(x)\ninit x=1\ndone\n",false},
    };
    unsigned index=0;
    for (const auto &test:cases) {
        const std::string file=tmp.file(xpp::format("purity{}.ode",index++));
        CHECK(write_file(file,test.source));
        CHECK(load(file));
        CHECK(xpp::client_session().model().auto_rhs_pure==test.pure);
    }
    CHECK(load("tools/models/compile_derived.odex"));
    auto &s=xpp::client_session();
    CHECK(s.model().auto_rhs_pure && xpp::auto_parallel_ready(s));
    s.numerics.method=xpp::method::DISCRETE;
    s.numerics.store_every=2;
    CHECK(!xpp::auto_parallel_ready(s));
    s.numerics.store_every=1;
    CHECK(xpp::auto_parallel_ready(s));
}

void check_collocation()
{
    const auto interpreted=collocation_run(1,false);
    const auto serial=collocation_run(1,true);
    CHECK(same_diagram(interpreted,serial));
    // Below W325's threshold every count stays serial (collocation_run checks the pool).
    CHECK(same_diagram(serial,collocation_run(4,true)));
    CHECK(same_diagram(serial,collocation_run(4,true,true)));
    // Above it, the setubv/conpar workers keep the whole diagram and leave the
    // parser's state alone.
    const auto large=collocation_run(1,true,false,{},false,worker_mesh);
    const auto constants=xpp::client_session().parser.constants;
    const auto variables=xpp::client_session().parser.variables;
    for (unsigned threads:{2u,3u,4u}) {
        CHECK(same_diagram(large,collocation_run(threads,true,false,{},false,worker_mesh)));
        CHECK(same_vector(constants,xpp::client_session().parser.constants));
        CHECK(same_vector(variables,xpp::client_session().parser.variables));
    }
    /* W260: the exact Jacobian (AUTO's JAC=1), computed on the workers
       from their own constants: the same bits on every count */
    const auto large_exact=collocation_run(1,true,false,{},true,worker_mesh);
    CHECK(!same_diagram(large,large_exact));
    for (unsigned threads:{2u,4u})
        CHECK(same_diagram(large_exact,collocation_run(threads,true,false,{},true,worker_mesh)));
    program.compile=true;

    // Failures are values observed after every worker has finished. A failed
    // dispatch must leave the persistent pool usable for the next call.
    auto &s=xpp::client_session();
    AutoLib lib;
    lib.session=&s;
    xpp::AutoParallel pool(4);
    pool.prepare(lib,2,2);
    std::atomic<int> visited=0;
    bool caught=false;
    try {
        pool.intervals(17,[&](long begin,long end,AutoLib &){
            visited+=static_cast<int>(end-begin);
            if (begin>0) throw std::runtime_error("injected worker failure");
        });
    } catch (const xpp::AutoFailed &failure) { caught=failure.error.what.find("injected worker failure")!=std::string::npos; }
    CHECK(caught && visited==17);
    visited=0;
    pool.intervals(17,[&](long begin,long end,AutoLib &){visited+=static_cast<int>(end-begin);});
    CHECK(visited==17);
    // No mesh allocation: even the largest accepted integer partitions exactly.
    std::array<std::pair<long,long>,xpp::AutoParallel::max_threads> ranges{};
    std::atomic<unsigned> next=0;
    pool.intervals(std::numeric_limits<long>::max(),[&](long begin,long end,AutoLib &){ranges[next++]={begin,end};});
    std::sort(ranges.begin(),ranges.end());
    CHECK(next==ranges.size() && ranges.front().first==0 && ranges.back().second==std::numeric_limits<long>::max());
    for (size_t i=1;i<ranges.size();++i) CHECK(ranges[i-1].second==ranges[i].first);
}

/* W336: a pool is reused across calls and replaced by one of another size
   (AutoParallel::select_run does so when the thread count changes): every
   size covers every interval once; a worker's failed Jacobian is the
   Session's and ends the job as a Stop does; a worker throwing leaves the
   pool usable. */
void check_pool_reuse()
{
    auto &s=xpp::client_session();
    AutoLib lib;
    lib.session=&s;
    for (unsigned count:{4u,1u,3u,2u,4u}) {
        xpp::AutoParallel pool(count);
        pool.prepare(lib,2,2);
        for (long n:{17L,1L,5L,64L}) {
            std::atomic<long> covered=0;
            std::atomic<int> pieces=0;
            pool.intervals(n,[&](long begin,long end,AutoLib &){covered+=end-begin;++pieces;});
            CHECK(covered==n);
            CHECK(pieces>=1 && static_cast<unsigned>(pieces)<=count);
        }
        pool.prepare(lib,3,1); /* the dimension changes: the workers' scratch follows */
        std::atomic<long> covered=0;
        std::atomic<bool> resized=true; /* CHECK's counters are the caller's, not the workers' */
        pool.intervals(9,[&](long begin,long end,AutoLib &worker){
            if(worker.scratch.uu1.size()!=3)resized=false;
            covered+=end-begin;
        });
        CHECK(resized);
        CHECK(covered==9);
        bool failed=false;
        try { pool.intervals(9,[&](long begin,long,AutoLib &){if(begin>0)throw 1;}); }
        catch (const xpp::AutoFailed &) { failed=count>1; }
        CHECK(failed==(count>1));
        covered=0;
        pool.intervals(9,[&](long begin,long end,AutoLib &){covered+=end-begin;});
        CHECK(covered==9);
    }
    xpp::AutoParallel pool(3);
    pool.prepare(lib,2,2);
    xpp::job::begin(1);
    CHECK(!xpp::job::cancelled());
    pool.intervals(9,[&](long begin,long,AutoLib &worker){
        if(begin>0)worker.jacobian_failure=xpp::Error{"exact jacobian","injected"};
    });
    CHECK(s.auto_lib.jacobian_failure.has_value() && xpp::job::cancelled());
    xpp::job::end();
    s.auto_lib.jacobian_failure.reset();
}

/* W474: brbd reduces the condensed blocks in a tree whose shape NTST alone
   fixes, so a solve gives the same bits on every thread count; the
   solution, a refactored one and one through the kept factors (ifst=0)
   alike, satisfies the system it was given. NTST 13 is no power of two and
   more than the eight chunks, so chunks of one, two and more blocks meet. */
void check_brbd_tree()
{
    constexpr integer nov=3, ncol=2, ntst=13, nbc=3, nint=1, ncb=nint+1;
    constexpr integer nra=nov*ncol, nca=nra+nov, nrc=nbc+nint+1;
    std::uint64_t state=474;
    const auto next=[&]{
        state=state*6364136223846793005ULL+1442695040888963407ULL;
        return static_cast<double>(state>>11)/static_cast<double>(1ULL<<53)*2-1;
    };
    std::vector<double> a(nca*nra*ntst),b(ncb*nra*ntst),c(nca*nrc*ntst,0.),d(ncb*nrc),fa(nra*ntst),fc(nrc),fa2(nra*ntst),fc2(nrc);
    for (auto &v:a) v=next();
    for (auto &v:b) v=next();
    for (integer i=0;i<ntst;++i)
        for (integer ir=0;ir<nrc;++ir)
            for (integer j=0;j<nca;++j) {
                // boundary conditions hold x(0) and x(ntst) only
                const bool bc_column=(i==0 && j<nov) || (i==ntst-1 && j>=nra);
                if (ir>=nbc || bc_column) c[j+(ir+i*nrc)*nca]=next();
            }
    for (auto &v:d) v=next();
    for (auto &v:fa) v=next();
    for (auto &v:fc) v=next();
    for (auto &v:fa2) v=next();
    for (auto &v:fc2) v=next();

    /* the largest residual of the solution in fa/fc (x(i) and the local
       values per block, then x(ntst) and the parameters) against the rhs */
    const auto residual=[&](const std::vector<double> &u,const std::vector<double> &w,
                            const std::vector<double> &rhs,const std::vector<double> &rhsc){
        const auto value=[&](integer i,integer j){
            if (j<nra) return u[j+i*nra];
            return i+1<ntst ? u[j-nra+(i+1)*nra] : w[j-nra];
        };
        double worst=0;
        for (integer i=0;i<ntst;++i)
            for (integer r=0;r<nra;++r) {
                double sm=-rhs[r+i*nra];
                for (integer j=0;j<nca;++j) sm+=a[j+(r+i*nra)*nca]*value(i,j);
                for (integer k=0;k<ncb;++k) sm+=b[k+(r+i*nra)*ncb]*w[nov+k];
                worst=std::max(worst,std::fabs(sm));
            }
        for (integer ir=0;ir<nrc;++ir) {
            double sm=-rhsc[ir];
            for (integer i=0;i<ntst;++i)
                for (integer j=0;j<nca;++j) sm+=c[j+(ir+i*nrc)*nca]*value(i,j);
            for (integer k=0;k<ncb;++k) sm+=d[k+ir*ncb]*w[nov+k];
            worst=std::max(worst,std::fabs(sm));
        }
        return worst;
    };

    std::vector<double> first_fa,first_fc,second_fa,second_fc;
    double first_det=0;
    for (unsigned threads=1;threads<=xpp::AutoParallel::max_threads;++threads) {
        AutoLib lib;
        lib.session=&xpp::client_session();
        lib.parameter_count=ncb;
        lib.collocation=std::make_unique<xpp::AutoParallel>(threads);
        lib.collocation->prepare(lib,nov,nbc);
        SolvbvWork &work=lib.solvbv;
        work.resize(nov,ncol,ntst,nbc,nint,ncb);
        std::copy(a.begin(),a.end(),work.a.begin());
        std::copy(b.begin(),b.end(),work.b.begin());
        std::copy(c.begin(),c.end(),work.c.begin());
        std::copy(d.begin(),d.end(),work.d.begin());
        std::vector<double> u(fa),w(fc),p0(nov*nov),p1(nov*nov);
        u.resize(work.ft.size());
        integer ifst=1,idb=0,nllv=0,nov_=nov,na=ntst,nbc_=nbc,nra_=nra,nca_=nca,ncb_=ncb,nrc_=nrc;
        double det=0;
        brbd(lib,nullptr,u.data(),w.data(),p0.data(),p1.data(),&ifst,&idb,&nllv,&det,&nov_,&na,&nbc_,&nra_,&nca_,&ncb_,&nrc_);
        CHECK(residual(u,w,fa,fc)<1e-10);
        std::vector<double> u2(fa2),w2(fc2);
        u2.resize(work.ft.size());
        ifst=0;
        brbd(lib,nullptr,u2.data(),w2.data(),p0.data(),p1.data(),&ifst,&idb,&nllv,&det,&nov_,&na,&nbc_,&nra_,&nca_,&ncb_,&nrc_);
        CHECK(residual(u2,w2,fa2,fc2)<1e-10);
        if (threads==1) { first_fa=u; first_fc=w; second_fa=u2; second_fc=w2; first_det=det; }
        else CHECK(same_vector(first_fa,u) && same_vector(first_fc,w) && same_vector(second_fa,u2)
                   && same_vector(second_fc,w2) && same_bits(first_det,det));
    }
}

} // namespace

int main(int argc,char **argv)
{
    if (argc==3) {
        xpp::TokenReader count=xpp::TokenReader::of_text(argv[2]);
        int requested=0;
        const bool valid=count.read(requested) && count.at_end() && requested>=1
            && requested<=static_cast<int>(xpp::AutoParallel::max_threads);
        CHECK(valid);
        if (!valid) TEST_REPORT("AUTO benchmark");
        const unsigned threads=static_cast<unsigned>(requested);
        const auto points=collocation_run(threads,true,false,argv[1]);
        const auto &m=xpp::client_session().model();
        CHECK(write_file("build/w247-diagram-"+std::to_string(threads)+".csv",
              xpp::snapx::auto_members::diagram_csv(points,m.nupar,{m.uvar_names.data(),static_cast<size_t>(m.node)})));
        TEST_REPORT("AUTO benchmark");
    }
    check_collocation_purity();
    check_collocation();
    check_brbd_tree();
    check_branch_parameters();
    check_parameterless_model();
    check_pool_reuse();
    check_settings_text();
    check_views_text();
    check_diagram_csv();
    check_solutions();
    check_untrusted_zoom();
    CHECK(xpp::zip::is_zip(std::string_view("PK\x03\x04", 4)) && !xpp::zip::is_zip("8 0 1 2"));

    std::string lecar_auto;
    CHECK(xpp::read_bytes("tools/models/lecar_diagram.auto", lecar_auto));
    xpp::TempDir tmp;
    CHECK(!tmp.path().empty());
    xpp::client_session().auto_state.dir = tmp.path(); /* AUTO's files there, not in HOME */

    const std::string ode = tmp.file("t.ode");
    CHECK(write_file(ode, "par a=1,b=2\nx'=-a*x+y\ny'=b*x-y\ninit x=1\ndone\n"));
    CHECK(write_file(tmp.file("u.ode"), "par a=1,b=2\nx'=-a*x+z\nz'=b*x-z\ninit x=1\ndone\n"));
    CHECK(load(ode));
    {
        xpp::Session &s = xpp::client_session();
        s.auto_state.bifur.nmx = 37;
        const auto before = xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s));
        const auto bytes = xpp_session_snapshot(s);
        CHECK(bytes.has_value());
        const auto f = bytes ? xpp_saved_parse(tmp.file("settings.snapx"), "settings.snapx", *bytes, SavedKind::snapshot) : std::nullopt;
        s.auto_state.bifur.nmx = 38;
        CHECK(f && xpp_saved_restore(s, *f));
        CHECK(xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s)) == before);
        CHECK(diagram_count(s.diagram) <= 1 && !s.auto_state.bifur.exist);
        if (f) {
            SavedFile missing = *f;
            missing.members.erase("auto/settings.txt");
            missing.members.erase("auto/views.txt");
            CHECK(xpp_saved_check(s, missing).has_value());
            CHECK(xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s)) == before);
        }
    }
    check_orbit_growth(tmp);
    check_session_round_trip(tmp);

    CHECK(load("examples/ode/lecar.odex"));
    check_import(lecar_auto, tmp);
    TEST_REPORT("session_auto");
}
