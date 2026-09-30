/* AUTO's file, .autox (autox.h, W92, W103): its settings, diagram and
   views (W50) read back bit for bit, a file of another shape is refused,
   and in a session the whole file (the model, settings, diagram,
   solutions, two views)
   written and restored gives the same AUTO state, in the same model or
   with its saved model loaded from it (the .ode on the disk edited); a
   file without its model is refused; an XPPAUT .auto is imported
   (tools/models/lecar_diagram.auto, lecar's diagram as XPPAUT wrote it)
   and its AUTO members restore exactly what was imported. */
#include "xpptest.h"
#include "autox.h"
#include "snapx.h"
#include "session.h"
#include "model.h"
#include "diagram.h"
#include "auto_nox.h"
#include "auto_settings.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_zip.h"
#include "xpp_session.h"
#include "model_files.h"

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

bool same_vector(const std::vector<double> &a, const std::vector<double> &b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++)
        if (!same_bits(a[i], b[i])) return false;
    return true;
}

/* two points the same, every field bit for bit */
bool same_point(const DiagramPoint &a, const DiagramPoint &b)
{
    const DIAGRAM &x = a.d, &y = b.d;
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
        DIAGRAM &d = q.d;
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
    AutoSettingsSet s;
    for (int i = 0; i < AUTO_NUM_N; i++) s.num[i] = awk(i) * (i + 1);
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
    const std::optional<AutoSettingsSet> r = xpp::autox::parse_settings(xpp::autox::settings_text(s));
    CHECK(r.has_value());
    if (!r) return;
    bool num = true;
    for (int i = 0; i < AUTO_NUM_N; i++) num = num && r->has_num[i] && same_bits(r->num[i], s.num[i]);
    CHECK(num);
    CHECK(r->npars == 3 && r->pars[0] == "iapp" && r->pars[1].empty() && r->pars[2] == "phi");
    CHECK(r->has_plot && r->plot == 11 && r->var == "v" && r->par1 == "iapp" && r->par2.empty());
    bool range = true;
    for (int i = 0; i < 4; i++) range = range && r->has_range[i] && same_bits(r->range[i], s.range[i]);
    CHECK(range);
    CHECK(r->nmarks == 2 && r->mark_name[0] == "T" && r->mark_name[1] == "phi" && same_bits(r->mark_value[0], s.mark_value[0]) &&
          same_bits(r->mark_value[1], s.mark_value[1]));
    CHECK(!xpp::autox::parse_settings("ds not-a-number\n"));
    CHECK(!xpp::autox::parse_settings("plot 1.5\n"));
}

/* views.txt (W50): the views read back bit for bit, a degenerate range
   (a Fit of a flat quantity) and no zoom included; a file of another
   shape is refused */
void check_views_text()
{
    xpp::autox::SavedViews v;
    v.views.push_back({2, "v", "iapp", "phi", {awk(0), awk(1), awk(2), awk(3)}, {}});
    xpp::Zoom z;
    z.x = {true, awk(4), awk(5) + 1};
    v.views.push_back({3, "", "iapp", "", {-0.2, 0.08, 0, 0}, z});
    v.active = 1;
    const std::string text = xpp::autox::views_text(v);
    const std::optional<xpp::autox::SavedViews> r = xpp::autox::parse_views(text);
    CHECK(r && *r == v);
    CHECK(text.ends_with("active 1\n"));
    CHECK(!xpp::autox::parse_views("view 2 v iapp phi 0 1 0 1 - -\n"));                /* no active line */
    CHECK(!xpp::autox::parse_views("active 0\n"));                                     /* no view */
    CHECK(!xpp::autox::parse_views("view 2 v iapp phi 0 1 0 1 - -\nactive 1\n"));     /* no view 1 */
    CHECK(!xpp::autox::parse_views("view 2 v iapp phi 0 1 0 1 1:0 -\nactive 0\n"));   /* a zoom low above high */
    CHECK(!xpp::autox::parse_views("view 2 v iapp phi 0 1 0 -\nactive 0\n"));        /* a field short */
}

void check_diagram_csv()
{
    const std::vector<std::string> vars = {"v", "w", "long_name_of_a_variable"};
    const std::deque<DiagramPoint> pts = points_of(7, 3);
    const std::string csv = xpp::autox::diagram_csv(pts, vars);
    CHECK(csv.starts_with("calc,ibr,ntot,itp,lab,nfpar,icp1,icp2,icp3,icp4,flag2,from,norm,per,torper,par1,"));
    CHECK(csv.find(",u0.long_name_of_a_variable,") != std::string::npos && csv.find(",evi3\n") != std::string::npos);
    const std::optional<std::deque<DiagramPoint>> back = xpp::autox::parse_diagram_csv(csv, 3);
    CHECK(back && same_diagram(*back, pts)); /* bit for bit */
    CHECK(!xpp::autox::parse_diagram_csv(csv, 2)); /* of another model */
    CHECK(!xpp::autox::parse_diagram_csv("", 3));
    std::string bad = csv;
    bad.back() = ',';
    CHECK(!xpp::autox::parse_diagram_csv(bad + "1\n", 3)); /* a row too long */
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
    std::string arg0 = "test_autox", arg1 = ode;
    char *argv[] = {arg0.data(), arg1.data(), nullptr};
    return !xpp::load_model(2, argv, 1, saved);
}

/* the whole file for this session: written with its model, the state
   changed, restored into the same model */
void check_session_round_trip(const xpp::TempDir &tmp)
{
    xpp::Session &s = xpp::session();
    const int n = xpp::model().node;
    CHECK(n == 2);
    std::deque<DiagramPoint> pts = points_of(9, n);
    for (DiagramPoint &p : pts) { /* parameters this model's AUTO has */
        p.d.icp1 = 0;
        p.d.icp2 = 1;
    }
    diagram_restore(pts);
    CHECK(diagram_count() == 9 && diagram_point(3)->uhi[1] == pts[3].uhi[1]);
    const std::string solutions = "   1   1   4   1   2   0   1   3 ...\r\n0.0 1 2\n";
    CHECK(write_file(auto_solutions_file(), solutions));
    s.auto_state.bifur.ds = 0.1 + 0.2;
    s.auto_state.bifur.dsmax = 0.5;
    s.auto_state.bifur.rl1 = 1.0 / 3.0;
    const AutoSettingsSet before = auto_settings_now();
    /* a second view (W50), the norm, a zoom of its own; the first active */
    auto_new_view();
    s.auto_state.views[1].axes.plot = 1;
    s.auto_state.views[1].axes.ymax = 5;
    s.auto_state.views[1].zoom.y = {true, 0.5, 1.5};
    s.auto_state.active_view = 0;

    const std::optional<std::string> bytes = xpp::autox::file_bytes();
    CHECK(bytes.has_value());
    if (!bytes) return;
    const std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(*bytes);
    const std::string model_member = "model/" + xpp::model().this_file;
    CHECK(entries && entries->size() == 6 && (*entries)[0].name == "autox.txt" && (*entries)[1].name == model_member &&
          (*entries)[2].name == "settings.txt" && (*entries)[3].name == "diagram.csv" && (*entries)[4].name == "solutions.s" &&
          (*entries)[5].name == "views.txt");
    const std::optional<xpp::snapx::Manifest> man =
        entries ? xpp::snapx::parse_manifest((*entries)[0].bytes, xpp::autox::kind) : std::nullopt;
    CHECK(man && man->model_name == xpp::model().this_file);
    CHECK(entries && (*entries)[1].bytes == file_text(xpp::model().this_file)); /* the model itself */
    CHECK(entries && !xpp::snapx::parse_manifest((*entries)[0].bytes)); /* not a session file's */

    /* everything changed, then the file restored */
    start_diagram(n);
    CHECK(write_file(auto_solutions_file(), "other"));
    s.auto_state.bifur.ds = 0.05;
    s.auto_state.bifur.rl1 = 7;
    s.auto_state.views.resize(1);
    const std::string path = tmp.file("t.autox");
    CHECK(write_file(path, *bytes));
    std::optional<SavedFile> f = xpp_saved_read(path);
    CHECK(f && !f->session && f->model.files == xpp::model().files);
    CHECK(f && xpp_saved_restore(*f));
    CHECK(same_diagram(s.diagram.points, pts));
    bool pointers = true;
    for (int i = 0; i < diagram_count(); i++)
        pointers = pointers && diagram_point(i)->index == i && diagram_point(i)->evi == s.diagram.points[i].evi.data();
    CHECK(pointers);
    CHECK(file_text(auto_solutions_file()) == solutions);
    CHECK(same_bits(s.auto_state.bifur.ds, 0.1 + 0.2) && same_bits(s.auto_state.bifur.rl1, 1.0 / 3.0));
    const AutoSettingsSet after = auto_settings_now();
    CHECK(xpp::autox::settings_text(after) == xpp::autox::settings_text(before));
    CHECK(s.auto_state.views.size() == 2 && s.auto_state.active_view == 0 && s.auto_state.views[1].axes.plot == 1 &&
          s.auto_state.views[1].axes.ymax == 5 && (s.auto_state.views[1].zoom.y == xpp::AxisRange{true, 0.5, 1.5}) &&
          !s.auto_state.views[1].zoom.x.set && s.auto_state.views[0].axes.plot == s.auto_state.axes().plot);

    /* another model loaded, the .ode on the disk edited: the file's own
       model loads from it, with the diagram */
    CHECK(write_file(xpp::model().this_file, "par a=1\nq'=-a*q\ndone\n"));
    CHECK(load(tmp.file("u.ode")));
    CHECK(f && load(f->manifest.model_name, &f->model) && xpp_saved_restore(*f));
    CHECK(xpp::model().node == 2 && xpp::model().saved_in == path && same_diagram(xpp::session().diagram.points, pts));

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
    xpp::Session &s = xpp::session();
    const std::string path = tmp.file("lecar.auto");
    CHECK(write_file(path, auto_text));
    CHECK(xpp::autox::import_file(path));
    CHECK(diagram_count() > 50);
    /* its first point, as the file prints it */
    const DIAGRAM *d = diagram_first();
    CHECK(d && d->ibr == 1 && d->ntot == 1 && d->itp == 9 && d->lab == 1 && d->par[0] == 0.05 && d->u0[0] == -0.144 &&
          d->u0[1] == 0.03);
    CHECK(s.auto_state.bifur.ntst == 15 && s.auto_state.bifur.nmx == 2000 && s.auto_state.bifur.dsmin == 1e-05);
    CHECK(file_text(auto_solutions_file()).size() > 100); /* the .s part */
    const std::deque<DiagramPoint> imported = s.diagram.points;
    const std::string solutions = file_text(auto_solutions_file());

    std::vector<xpp::zip::Entry> entries;
    xpp::autox::add_members(entries, "auto/");
    std::map<std::string, std::string> members;
    for (xpp::zip::Entry &e : entries) members[e.name] = std::move(e.bytes);
    CHECK(members.size() == 4 && members.contains("auto/diagram.csv") && members.contains("auto/views.txt"));
    start_diagram(xpp::model().node);
    CHECK(xpp::autox::restore_members(members, "auto/", "lecar.snapx"));
    CHECK(same_diagram(s.diagram.points, imported));
    CHECK(file_text(auto_solutions_file()) == solutions);

    /* an .autox is not imported as an XPPAUT .auto */
    CHECK(write_file(path, xpp::zip::make_zip(entries)));
    CHECK(!xpp::autox::import_file(path));
}

} // namespace

int main(void)
{
    check_settings_text();
    check_views_text();
    check_diagram_csv();
    CHECK(xpp::zip::is_zip(std::string_view("PK\x03\x04", 4)) && !xpp::zip::is_zip("8 0 1 2"));

    std::string lecar_auto;
    CHECK(xpp::read_bytes("tools/models/lecar_diagram.auto", lecar_auto));
    xpp::TempDir tmp;
    CHECK(!tmp.path().empty());
    xpp::session().auto_state.dir = tmp.path(); /* AUTO's files there, not in HOME */

    const std::string ode = tmp.file("t.ode");
    CHECK(write_file(ode, "par a=1,b=2\nx'=-a*x+y\ny'=b*x-y\ninit x=1\ndone\n"));
    CHECK(write_file(tmp.file("u.ode"), "par a=1,b=2\nx'=-a*x+z\nz'=b*x-z\ninit x=1\ndone\n"));
    CHECK(load(ode));
    check_session_round_trip(tmp);

    CHECK(load("examples/ode/lecar.ode"));
    check_import(lecar_auto, tmp);
    TEST_REPORT("autox");
}
