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
#include "auto_nox.h"
#include "auto_c.h"
#include "auto_settings.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_zip.h"
#include "xpp_session.h"
#include "model_files.h"
#include "browse.h"
#include "many_pops.h"
#include "auto_parallel.h"
#include "solver.h"
#include "xpp_globals.h"
#include <algorithm>
#include <atomic>
#include <chrono>
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
bool same_point(const DiagramPoint &a, const DiagramPoint &b)
{
    const xpp::DIAGRAM &x = a.d, &y = b.d;
    if (x.calc != y.calc || x.ibr != y.ibr || x.ntot != y.ntot || x.itp != y.itp || x.lab != y.lab || x.nfpar != y.nfpar ||
        x.icp1 != y.icp1 || x.icp2 != y.icp2 || x.icp3 != y.icp3 || x.icp4 != y.icp4 || x.flag2 != y.flag2 || x.from != y.from)
        return false;
    if (!same_bits(x.norm, y.norm) || !same_bits(x.per, y.per) || !same_bits(x.torper, y.torper)) return false;
    for (int i = 0; i < 20; i++)
        if (!same_bits(x.par[i], y.par[i])) return false;
    return same_vector(a.u0, b.u0) && same_vector(a.uhi, b.uhi) && same_vector(a.ulo, b.ulo) && same_vector(a.ubar, b.ubar) &&
           same_vector(a.evr, b.evr) && same_vector(a.evi, b.evi);
}

bool same_diagram(const std::deque<DiagramPoint> &a, const std::deque<DiagramPoint> &b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++)
        if (!same_point(a[i], b[i])) return false;
    return true;
}

/* k points of n variables, every value awkward */
std::deque<DiagramPoint> points_of(int k, int n)
{
    std::deque<DiagramPoint> pts;
    std::size_t v = 0;
    for (int p = 0; p < k; p++) {
        DiagramPoint &q = pts.emplace_back();
        xpp::DIAGRAM &d = q.d;
        d.ibr = 1 + p / 4;
        d.ntot = (p % 5) - 2;
        d.itp = p % 9;
        d.lab = p % 2 ? p : 0;
        d.calc = p % 3;
        d.norm = awk(v++);
        d.per = awk(v++);
        d.torper = awk(v++);
        d.nfpar = 2;
        d.icp1 = 0;
        d.icp2 = 1;
        d.icp3 = 2;
        d.icp4 = 3;
        d.flag2 = p % 2;
        d.from = p == 1 ? 7 : 0;
        for (double &x : q.d.par) x = awk(v++);
        for (std::vector<double> *a : {&q.u0, &q.uhi, &q.ulo, &q.ubar, &q.evr, &q.evi}) {
            a->resize(n);
            for (double &x : *a) x = awk(v++);
        }
    }
    return pts;
}

void check_settings_text()
{
    xpp::AutoSettingsSet s;
    for (int i = 0; i < xpp::AUTO_NUM_N; i++) s.num[i] = awk(i) / (i + 1);
    s.npars = 3;
    s.pars = {"iapp", "", "phi"};
    s.plot = 11;
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
    CHECK(r->npars == 3 && r->pars[0] == "iapp" && r->pars[1].empty() && r->pars[2] == "phi");
    CHECK(r->has_plot && r->plot == 11 && r->var == "v" && r->par1 == "iapp" && r->par2.empty());
    bool range = true;
    for (int i = 0; i < 4; i++) range = range && r->has_range[i] && same_bits(r->range[i], s.range[i]);
    CHECK(range);
    CHECK(r->nmarks == 2 && r->mark_name[0] == "T" && r->mark_name[1] == "phi" && same_bits(r->mark_value[0], s.mark_value[0]) &&
          same_bits(r->mark_value[1], s.mark_value[1]));
    /* each refusal at its line, the line as written (W125) */
    const auto error = [](const std::string &text) {
        const xpp::Result<xpp::snapx::auto_members::SettingsRead> e = xpp::snapx::auto_members::parse_settings(text, "a.snapx/auto/settings.txt");
        return e ? std::string() : e.error().text() + " [" + e.error().place.source + "]";
    };
    CHECK(error("ds not-a-number\n") == "a.snapx/auto/settings.txt:1: \"not-a-number\" is not a number [ds not-a-number]");
    CHECK(error("plot 1.5\n") == "a.snapx/auto/settings.txt:1: \"1.5\" is not a whole number [plot 1.5]");
    /* every key there once, none it does not have */
    const std::string text = xpp::snapx::auto_members::settings_text(s);
    const int lines = static_cast<int>(xpp::split_lines(text).size());
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
    v.views.push_back({2, "v", "iapp", "phi", {awk(0), awk(1), awk(2), awk(3)}, {}});
    xpp::Zoom z;
    z.x = {true, awk(4), awk(5) + 1};
    v.views.push_back({3, "", "iapp", "", {-0.2, 0.08, 0, 0}, z});
    v.active = 1;
    const std::string text = xpp::snapx::auto_members::views_text(v);
    const xpp::Result<xpp::snapx::auto_members::ViewsRead> r = xpp::snapx::auto_members::parse_views(text, "views.txt");
    CHECK(r && r->views == v && r->lines == std::vector<int>({1, 2}));
    CHECK(text.ends_with("active 1\n"));
    CHECK(!xpp::snapx::auto_members::parse_views("view 2 v iapp phi 0 1 0 1 - -\n", "views.txt"));                /* no active line */
    CHECK(!xpp::snapx::auto_members::parse_views("active 0\n", "views.txt"));                                     /* no view */
    CHECK(!xpp::snapx::auto_members::parse_views("view 2 v iapp phi 0 1 0 1 - -\nactive 1\n", "views.txt"));     /* no view 1 */
    CHECK(!xpp::snapx::auto_members::parse_views("view 2 v iapp phi 0 1 0 1 1:0 -\nactive 0\n", "views.txt"));   /* a zoom low above high */
    CHECK(!xpp::snapx::auto_members::parse_views("view 2 v iapp phi 0 1 0 -\nactive 0\n", "views.txt"));        /* a field short */
}

void check_solutions()
{
    const std::string algebraic = "1 1 4 1 2 0 1 3 2 0 0 2\n0 1 2\n0 0\n";
    CHECK(xpp::check_auto_solutions(algebraic, "sent.snapx/auto/solutions.s"));
    CHECK(!xpp::check_auto_solutions("1 1 4 1 2 0 1 9999 2 0 0 2\n0 1 2\n0 0\n", "solutions.s"));
    CHECK(!xpp::check_auto_solutions("1 1 4 1 2 0 1 3 2 0 0 2 junk\n0 1 2\n0 0\n", "solutions.s"));
    CHECK(!xpp::check_auto_solutions("1 1 4 1 2 0 1 3 2 0 0 2\n0 1 2\n0 nan\n", "solutions.s"));
    const auto cut = xpp::check_auto_solutions("1 1 4 1 2 0 1 3 2 0 0 2\n0 1 2\n0\n", "sent.snapx/auto/solutions.s");
    CHECK(!cut && cut.error().place.file == "sent.snapx/auto/solutions.s" && cut.error().place.line == 3);
    const std::string periodic = "1 1 2 1 1 1 3 2 5 1 2 1\n0 1 0.5 2 1 3\n0\n1\n1 1 1\n0\n";
    CHECK(xpp::check_auto_solutions(periodic, "solutions.s"));
    CHECK(!xpp::check_auto_solutions("1 1 2 1 1 1 3 2 5 1 2 1\n0 1 0.5 2 1 3\n9999\n1\n1 1 1\n0\n", "solutions.s"));
    xpp::TokenReader header = xpp::TokenReader::of_text("    2-1234  ");
    long a, b;
    CHECK(!header.at_end() && header.read(a) && a == 2 && header.read(b) && b == -1234 && header.at_end());
}

void check_untrusted_zoom()
{
    CHECK(!xpp::snapx::auto_members::parse_views("view 2 v iapp phi 0 1 0 1 -inf:inf -\nactive 0\n", "sent.snapx/auto/views.txt"));
}

void check_diagram_csv()
{
    const std::vector<std::string> vars = {"v", "w", "long_name_of_a_variable"};
    const std::deque<DiagramPoint> pts = points_of(7, 3);
    const std::string csv = xpp::snapx::auto_members::diagram_csv(pts, vars);
    CHECK(csv.starts_with("calc,ibr,ntot,itp,lab,nfpar,icp1,icp2,icp3,icp4,flag2,from,norm,per,torper,par1,"));
    CHECK(csv.find(",u0.long_name_of_a_variable,") != std::string::npos && csv.find(",evi3\n") != std::string::npos);
    const xpp::Result<std::deque<DiagramPoint>> back = xpp::snapx::auto_members::parse_diagram_csv(csv, 3, "diagram.csv");
    CHECK(back && same_diagram(*back, pts)); /* bit for bit */
    CHECK(!xpp::snapx::auto_members::parse_diagram_csv(csv, 2, "diagram.csv")); /* of another model */
    CHECK(!xpp::snapx::auto_members::parse_diagram_csv("", 3, "diagram.csv"));
    std::string bad = csv;
    bad.back() = ',';
    const xpp::Result<std::deque<DiagramPoint>> long_row = xpp::snapx::auto_members::parse_diagram_csv(bad + "1\n", 3, "diagram.csv");
    CHECK(!long_row && long_row.error().place.line == static_cast<int>(pts.size()) + 1); /* a row too long, at its line */
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
    if (!xpp::load_model(2, argv, 1, saved)) return false;
    init_browser(xpp::client_session());
    init_all_graph(xpp::client_session());
    xpp::client_session().plot_windows.graph[0].Use = 1;
    return true;
}

/* the whole file for this session: written with its model, the state
   changed, restored into the same model */
void check_orbit_growth(const xpp::TempDir &tmp)
{
    xpp::Session &s = xpp::client_session();
    const std::string orbit = "1 1 2 1 1 1 3 3 9 1 2 2\n0 1 2\n0.5 3 4\n1 5 6\n0\n1\n0 0\n0 0\n0 0\n0 0\n";
    CHECK(xpp::check_auto_solutions(orbit, "orbit.s"));
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
    std::deque<DiagramPoint> pts = points_of(9, n);
    for (DiagramPoint &p : pts) { /* parameters this model's AUTO has */
        p.d.icp1 = 0;
        p.d.icp2 = 1;
    }
    pts.back().d.icp2 = 10; /* a periodic run's second parameter is PAR(11), the period: past the model's parameters */
    diagram_restore(s, pts);
    CHECK(diagram_count(xpp::client_session().diagram) == 9 && diagram_point(xpp::client_session().diagram, 3)->uhi[1] == pts[3].uhi[1]);
    std::string solutions;
    for (int value : {1, 1, 4, 1, 2, 0, 1, 3, 7, 0, 0, NPARX}) solutions += xpp::format("{:5}", value);
    solutions += "\n0.0 1 2\n";
    for (int row = 0; row < 6; row++) solutions += "0 0 0 0 0 0\n";
    CHECK(write_file(xpp::auto_solutions_file(xpp::client_session()), solutions));
    s.auto_state.bifur.ds = 0.1 + 0.2;
    s.auto_state.bifur.dsmax = 0.5;
    s.auto_state.bifur.rl1 = 1.0 / 3.0;
    const xpp::AutoSettingsSet before = xpp::auto_settings_now(xpp::client_session());
    /* a second view (W50), the norm, a zoom of its own; the first active */
    xpp::auto_new_view(xpp::client_session());
    s.auto_state.views[1].axes.plot = 1;
    s.auto_state.views[1].axes.ymax = 5;
    s.auto_state.views[1].zoom.y = {true, 0.5, 1.5};
    s.auto_state.active_view = 0;

    const std::optional<std::string> bytes = xpp_session_snapshot(xpp::client_session());
    CHECK(bytes.has_value());
    if (!bytes) return;
    const std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(*bytes);
    const std::string model_member = "model/" + xpp::client_session().model().this_file;
    CHECK(entries && (*entries)[0].name == "session.txt" && (*entries)[1].name == model_member);
    const xpp::Result<xpp::snapx::Manifest> man =
        xpp::snapx::parse_manifest("a/session.txt", entries ? (*entries)[0].bytes : std::string());
    CHECK(entries && man && man->model_name == xpp::client_session().model().this_file);
    CHECK(entries && (*entries)[1].bytes == file_text(xpp::client_session().model().this_file));

    /* everything changed, then the file restored */
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
    bool pointers = true;
    for (int i = 0; i < diagram_count(xpp::client_session().diagram); i++)
        pointers = pointers && diagram_point(xpp::client_session().diagram, i)->index == i && diagram_point(xpp::client_session().diagram, i)->evi == s.diagram.points[i].evi.data();
    CHECK(pointers);
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())) == solutions);
    CHECK(same_bits(s.auto_state.bifur.ds, 0.1 + 0.2) && same_bits(s.auto_state.bifur.rl1, 1.0 / 3.0));
    const xpp::AutoSettingsSet after = xpp::auto_settings_now(xpp::client_session());
    CHECK(xpp::snapx::auto_members::settings_text(after) == xpp::snapx::auto_members::settings_text(before));
    CHECK(s.auto_state.views.size() == 2 && s.auto_state.active_view == 0 && s.auto_state.views[1].axes.plot == 1 &&
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
    CHECK(fp && xpp::import_auto_file(s, fp.get()) == 1);
    fp.reset();
    CHECK(diagram_count(xpp::client_session().diagram) > 50);
    /* its first point, as the file prints it */
    const xpp::DIAGRAM *d = diagram_first(xpp::client_session().diagram);
    CHECK(d && d->ibr == 1 && d->ntot == 1 && d->itp == 9 && d->lab == 1 && d->par[0] == 0.05 && d->u0[0] == -0.144 &&
          d->u0[1] == 0.03);
    CHECK(s.auto_state.bifur.ntst == 15 && s.auto_state.bifur.nmx == 2000 && s.auto_state.bifur.dsmin == 1e-05);
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())).size() > 100); /* the .s part */
    const std::deque<DiagramPoint> imported = s.diagram.points;
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
    CHECK(same_diagram(s.diagram.points, imported));
    CHECK(file_text(xpp::auto_solutions_file(xpp::client_session())) == solutions);


}

// W247: a normal form with derived parameters, fixed variables and a user
// function; compare all stored values, including Floquet eigenvalues.
std::deque<DiagramPoint> collocation_run(unsigned threads, bool compiled, bool impure=false,
                                        const std::string &benchmark={})
{
    xpp::TempDir tmp;
    const std::string file=benchmark.empty()?tmp.file("hopf.ode"):benchmark;
    if (benchmark.empty()) CHECK(write_file(file,
        "par mu=-0.1,w=1\n!frequency=w\nsquare(q)=q*q\nr=square(x)+square(y)\n"
        "x'=mu*x-frequency*y-x*r"+std::string(impure?"+0*shift(x,0)":"")+"\n"
        "y'=frequency*x+mu*y-y*r\ninit x=0,y=0\n"
        "@ ntst=17,ncol=4,nmax=60,npr=10,ds=0.02,dsmax=0.05,dsmin=0.0001\n"
        "@ parmin=-1,parmax=1,epsl=1e-8,epsu=1e-8,epss=1e-8\ndone\n"));
    program.compile=compiled;
    CHECK(load(file));
    xpp::Session &s=xpp::client_session();
    CHECK(s.model().auto_rhs_pure==(compiled && !impure));
    s.auto_state.dir=tmp.path();
    xpp::init_auto_win(s);
    if (compiled && !impure) s.auto_lib.collocation=std::make_unique<xpp::AutoParallel>(threads);
    xpp::auto_start_diff_ss(s);
    CHECK(xpp::auto_grab_type_index(s,"HB",1)==1);
    const auto start=std::chrono::steady_clock::now();
    xpp::auto_new_per(s);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("perf: AUTO %u threads %.6f s, %zu diagram points\n",threads,seconds,s.diagram.points.size());
    CHECK(std::any_of(s.diagram.points.begin(),s.diagram.points.end(),[](const auto &p){return p.d.ibr<0;}));
    if (compiled && !impure) CHECK(s.auto_lib.collocation && s.auto_lib.collocation->threads()==threads);
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
    const auto constants=xpp::client_session().parser.constants;
    const auto variables=xpp::client_session().parser.variables;
    for (unsigned threads:{2u,4u}) {
        CHECK(same_diagram(serial,collocation_run(threads,true)));
        CHECK(same_vector(constants,xpp::client_session().parser.constants));
        CHECK(same_vector(variables,xpp::client_session().parser.variables));
    }
    CHECK(same_diagram(serial,collocation_run(4,true,true)));
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
    } catch (const xpp::AutoFailed &failure) { caught=failure.what.find("injected worker failure")!=std::string::npos; }
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
              xpp::snapx::auto_members::diagram_csv(points,{m.uvar_names.data(),static_cast<size_t>(m.node)})));
        TEST_REPORT("AUTO benchmark");
    }
    check_collocation_purity();
    check_collocation();
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
