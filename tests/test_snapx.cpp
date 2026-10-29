/* The files that carry a model (snapx.h, xpp_session.h; W57, W103): a
   manifest reads back as it was written, a file of another kind or
   version is refused; a loaded model records every file it read (the
   .odex, an included file, a file table) and a file written with it
   carries them all; that saved model loads from the file alone, the
   files on the disk gone, and a file without its model is refused. */
#include "load_args.h"
#include "model_text.h"
#include "xpptest.h"
#include "snapx.h"
#include "model.h"
#include "model_files.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "nullcline.h"
#include "setx.h"
#include "expr.h"
#include "xpp_session.h"
#include "xpp_zip.h"
#include "xpp_util.h"
#include "many_pops.h"
#include "xpp_ui.h"
#include "graf_par.h"
#include "browse.h"
#include "data_formats.h"
#include "setx_file.h"
#include <array>
#include <limits>

#include <string>
#include <vector>

namespace {

/* a model or animation file's text takes the format version first; a zip or other bytes written under a model's name are as given */
bool write_file(const std::string &path, std::string_view text)
{
    const bool plain = !text.starts_with("PK") && text.find('\0') == std::string_view::npos;
    const std::string body = !plain ? std::string(text) : xpp::files::has_extension(path, ".odex") ? versioned(text)
                           : xpp::files::has_extension(path, ".anix") ? versioned_anix(text) : std::string(text);
    xpp::Writer w = xpp::Writer::binary(path.c_str());
    return w && w.write(body) && w.commit();
}

bool load(const std::string &file, const xpp::SavedModel *saved = nullptr)
{
    std::string arg0 = "test_snapx", arg1 = file;
    char *argv[] = {arg0.data(), arg1.data(), nullptr};
    return load_args(2, argv, 1, saved).has_value();
}

void check_manifest()
{
    using namespace xpp::snapx;
    const std::string file = "s/session.setx";
    const auto foreign = parse_manifest(file, "session: { version: 4, model: \"x.ode\", data: false }\n");
    CHECK(!foreign && foreign.error().place.line == 1 && foreign.error().what.find(".ode is refused") != std::string::npos);
    Manifest m;
    m.model_name = "my models/lecar.odex";
    m.anifile = "lecar.anix";
    m.data = true;
    const std::string text = *manifest_text(m);
    CHECK(text.starts_with("session: {\n"));
    CHECK(text.find("  model: \"my models/lecar.odex\",\n") != std::string::npos);
    const auto back = parse_manifest(file, text);
    CHECK(back.has_value() && *back == m);

    /* no animation, no data; CRLF line ends read the same */
    Manifest e;
    e.model_name = "x.odex";
    std::string crlf;
    for (char c : *manifest_text(e)) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    const auto eb = parse_manifest(file, crlf);
    CHECK(eb.has_value() && *eb == e && eb->anifile.empty() && !eb->data);

    /* a key it does not have, another version or an older layout is refused,
       saying what is wrong and where */
    const auto later = parse_manifest(file, "session: { version: 4, model: \"a.odex\", data: false, future: 1 }\n");
    CHECK(!later && later.error().place.file == file && later.error().place.line == 1 && later.error().what.find("`future` is not a setting") != std::string::npos);
    CHECK(!parse_manifest(file, "session: { version: 4, model: \"x.odex\", data: 2 }\n").has_value()); /* data is 0 or 1 */
    CHECK(!parse_manifest(file, "session: { version: 4, data: false }\n").has_value());                    /* no model */
    CHECK(!parse_manifest(file, "session: { version: 4, model: \"a.odex\", model: \"b.odex\", data: false }\n").has_value()); /* a key twice */
    CHECK(!parse_manifest(file, "xppautX session 1\nname x.odex\ndata 0\n").has_value());              /* the older layout */
    CHECK(!parse_manifest(file, "").has_value());
    CHECK(!parse_manifest(file, "session: { version: 3, model: \"m.odex\", data: false }\n").has_value()); /* another version */

    /* the model's members: model/<name>, in order; the model's own needed */
    std::vector<xpp::zip::Entry> entries{{"session.setx", text}};
    const std::vector<xpp::ModelFile> files{{"a.odex", "x'=-x\n"}, {"../t/w.tab", "2 0 1 0 1\n"}};
    add_model_members(entries, files);
    entries.push_back({"model.setx", "numerics: {},\n"});
    CHECK(entries.size() == 4 && entries[1].name == "model/a.odex" && entries[2].name == "model/../t/w.tab");
    const std::optional<std::vector<xpp::ModelFile>> got = model_members(entries, "a.odex");
    CHECK(got && *got == files);
    CHECK(!model_members(entries, "b.odex"));
    CHECK(!model_members({{"session.setx", text}}, "a.odex"));

    CHECK(is_session_file("a/b.snapx") && is_session_file("B.SNAPX") && !is_session_file("b.snapx.zip"));
    CHECK_STR(session_file_name("run1").c_str(), "run1.snapx");
    CHECK_STR(session_file_name("run1.SnapX").c_str(), "run1.SnapX");
    CHECK(xpp_saved_file_name("s.SNAPX") && !xpp_saved_file_name("m.odex"));
}

/* a model with an included file and a file table: recorded, saved, read
   back and loaded from the saved copies alone */
void check_nullclines()
{
    const auto model = std::make_unique<xpp::Model>();
    model->node = 2;
    const std::string text =
        "current: { x_axis: 1, y_axis: 2, x_segments: [0, 1, 2, 3], y_segments: [] },\n"
        "frozen: [ { x_axis: 1, y_axis: 2, x_segments: [], y_segments: [-1, -2, -3, -4] } ],\n";
    const auto read = [&](std::string_view bytes) {
        return xpp::setx::read_block("session", "s.snapx/nullclines.setx", bytes, true,
            [&](xpp::setx::Block &file) { return xpp::read_nullclines(*model, file); });
    };
    const auto got = read(text);
    CHECK(got && got->num_x_n == 1 && got->num_y_n == 0 && got->null_ix == 1 && got->null_iy == 2);
    CHECK(got && got->x_null == std::vector<float>({0, 1, 2, 3}));
    CHECK(got && got->frozen_started && got->frozen.size() == 1 && got->frozen[0].yn == std::vector<float>({-1, -2, -3, -4}));
    const auto cut = read(text.substr(0, text.find("frozen")));
    CHECK(!cut && cut.error().place.file == "s.snapx/nullclines.setx" && cut.error().what.find("`frozen`") != std::string::npos);
    const auto extra = read(text + "extra: 1,\n");
    CHECK(!extra && extra.error().place.line == 3);
    const auto short_segment = read("current: { x_axis: 1, y_axis: 2, x_segments: [1, 2, 3], y_segments: [] }, frozen: [],\n");
    CHECK(!short_segment && short_segment.error().place.line == 1 && short_segment.error().what.find("four for each segment") != std::string::npos);
    const auto bad = read("current: { x_axis: 1, y_axis: 2, x_segments: [1e100, 0, 0, 0], y_segments: [] }, frozen: [],\n");
    CHECK(!bad && bad.error().place.line == 1 && bad.error().place.source.find("1e100") != std::string::npos);
    const auto nan = read("current: { x_axis: 1, y_axis: 2, x_segments: [nan, 0, 0, 0], y_segments: [] }, frozen: [],\n");
    CHECK(!nan && nan.error().what.find("nan") != std::string::npos);
    const auto axes = read("current: { x_axis: 1, y_axis: 1, x_segments: [0, 0, 0, 0], y_segments: [] }, frozen: [],\n");
    CHECK(!axes && axes.error().what.find("distinct") != std::string::npos);
}

void check_saved_animation(const xpp::TempDir &tmp)
{
    CHECK(xpp::files::change_dir(tmp.path().c_str()) == 0);
    CHECK(write_file("animated.odex", "x'=-x\ninit x=1\noptions { animation: \"motion.anix\", equilibria: { neartol: .1 } }\n"));
    CHECK(write_file("motion.anix", "model: \"animated.odex\"\nline { x1: near(0,.05), y1: 0, x2: x, y2: 1 }\n"));
    CHECK(load("animated.odex"));
    CHECK(xpp::client_session().animation.ncom == 1);
    CHECK(xpp::evaluate(xpp::client_session(), xpp::client_session().animation.commands.front().x1.data()) == 1);
    auto entries = xpp_saved_entries(xpp::client_session(), xpp::snapx::Manifest{});
    CHECK(entries.has_value());
    if (!entries) return;
    const std::string path = tmp.file("animation.snapx");
    CHECK(write_file(path, xpp::zip::make_zip(*entries)));
    auto saved = xpp_saved_read(path);
    CHECK(saved && saved->manifest.anifile == "motion.anix");
    if (!saved) return;
    CHECK(xpp::files::remove("animated.odex") == 0 && xpp::files::remove("motion.anix") == 0);
    CHECK(load("animated.odex", &saved->model));
    CHECK(xpp::client_session().animation.ncom == 1);
    for (auto &file : saved->model.files)
        if (file.name == "motion.anix") file.bytes = versioned_anix("line { x1: missing, y1: 0, x2: 1, y2: 1 }\n");
    /* an animation that no longer fits the model is refused whole: the model loads without it */
    CHECK(load("animated.odex", &saved->model));
    CHECK(xpp::client_session().animation.ncom == 0);
}

void check_saved_model(const xpp::TempDir &tmp)
{
    const std::string dir = tmp.path();
    CHECK(xpp::files::change_dir(dir.c_str()) == 0);
    CHECK(write_file("inc.odex", "par a=2\n"));
    CHECK(write_file("w.tab", "3\n0\n2\n0\n10\n40\n"));
    CHECK(write_file("main.odex", "import \"inc.odex\"\ntable w \"w.tab\"\nx'=-a*x+w(1)\ninit x=1\n"));
    CHECK(load("main.odex"));
    /* A disk change after recording must not change the stream a parser sees. */
    CHECK(write_file("inc.odex", "par a=99\n"));
    xpp::UniqueFile stream = xpp::open_model_file(xpp::client_session().model(), "inc.odex");
    std::string streamed;
    CHECK(xpp::read_bytes(stream.get(), streamed) && streamed == versioned("par a=2\n"));
    CHECK(write_file("inc.odex", "par a=2\n"));
    const std::vector<xpp::ModelFile> files = xpp::client_session().model().files;
    CHECK(files.size() >= 3 && files[0].name == "main.odex" && files[1].name == "inc.odex" && files[2].name == "w.tab");
    CHECK(xpp::client_session().model().saved_in.empty() && xpp::client_session().model().nupar == 1);

    /* written as a session file's first members, read back whole */
    xpp::Result<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(xpp::client_session(), xpp::snapx::Manifest{});
    CHECK(entries.has_value());
    if (!entries) return;
    const std::string path = tmp.file("s.snapx");
    CHECK(write_file(path, xpp::zip::make_zip(*entries)));
    std::optional<SavedFile> f = xpp_saved_read(path);
    CHECK(f && f->manifest.model_name == "main.odex" && f->model.files == files && f->model.in == path);
    if (!f) return;
    CHECK(xpp_saved_args(*f) == std::vector<std::string>{"main.odex"});

    /* the files on the disk gone: the saved model loads from the file */
    for (const char *name : {"main.odex", "inc.odex", "w.tab"}) CHECK(xpp::files::remove(name) == 0);
    CHECK(load("main.odex", &f->model));
    CHECK(xpp::client_session().model().saved_in == path && xpp::client_session().model().files == files && xpp::client_session().model().nupar == 1);
    CHECK(xpp::client_session().model().upar_names[0] == "a" || xpp::client_session().model().upar_names[0] == "A");
    CHECK(xpp::model_title(xpp::client_session().model()) == "main.odex (saved in s.snapx)");
    std::string bytes;
    CHECK(xpp::read_model_file(xpp::client_session().model(), "w.tab", bytes) && bytes == files[2].bytes);
    CHECK(!xpp::read_model_file(xpp::client_session().model(), "other.tab", bytes)); /* not saved: not there */

    /* the same bytes on the disk, edited: the saved model still loads as saved */
    CHECK(write_file("main.odex", "par a=5,b=1\nx'=-a*x\ninit x=1\n"));
    CHECK(load("main.odex", &f->model));
    CHECK(xpp::client_session().model().nupar == 1 && xpp::client_session().model().files == files);

    /* a file without its model is refused, as is one that is no zip */
    std::vector<xpp::zip::Entry> no_model{(*entries)[0]};
    CHECK(write_file(path, xpp::zip::make_zip(no_model)));
    CHECK(!xpp_saved_read(path));
    CHECK(write_file(path, "not a zip"));
    CHECK(!xpp_saved_read(path));

    /* a zip or another binary file opened as a model is refused */
    CHECK(write_file("zip.odex", xpp::zip::make_zip(*entries)));
    CHECK(!load("zip.odex"));
    CHECK(write_file("bin.odex", std::string_view("x'=-x\n\0\1\2", 9)));
    CHECK(!load("bin.odex"));
    CHECK(xpp::client_session().model().saved_in == path); /* the model before is still loaded */
    CHECK(xpp::is_model_text("x'=-x\n") && !xpp::is_model_text(std::string_view("a\0b", 3)));
}


/* Each list refuses expansion at its own line before staging or applying
   anything. Seed the budget with earlier allocations to exercise its boundary
   using tiny files instead of allocating half a gigabyte in a unit test. */
void check_list_memory()
{
    constexpr std::size_t record_bytes = sizeof(GRAPH);
    for (const char *key : {"windows", "frozen_curves", "curves"}) {
        const std::string text = "\n" + std::string(key) + ": [{}, {}]\n";
        std::size_t used = xpp::zip::archive_bytes_limit - record_bytes;
        const std::size_t before = used;
        std::vector<xpp::setx::Block> staged;
        bool applied = false;
        const auto read = xpp::setx::read_block("session", "memory.setx", text, true, [&](xpp::setx::Block &b) {
            xpp::snapx::memory_groups(b, key, staged, record_bytes, used);
            return true;
        });
        if (read) applied = *read;
        CHECK(!read && read.error().what == "too many " + std::string(key) + " for the session memory limit");
        CHECK(!read && read.error().place.file == "memory.setx" && read.error().place.line == 2);
        CHECK(!read && read.error().place.source == std::string(key) + ": [{}, {}]");
        CHECK(!applied && staged.empty() && used == before);
    }
    std::size_t used = 0;
    CHECK(!xpp::snapx::account_memory(used, std::numeric_limits<std::size_t>::max(), sizeof(GRAPH)) && used == 0);
    CHECK(xpp::snapx::account_memory(used, 1, xpp::zip::archive_bytes_limit));
    CHECK(!xpp::snapx::account_memory(used, 1, 1) && used == xpp::zip::archive_bytes_limit);
}

/* W376 (#438): all three input-sized dimensions, holes, and the protocol's
   auxiliary ID range survive a recording snapshot and a session reload. */
void check_plot_limits(const xpp::TempDir &tmp)
{
    std::string model;
    constexpr int curves=13, frozen=32, windows=107;
    for(int i=0;i<curves;i++)model+="x"+std::to_string(i)+"'=-x"+std::to_string(i)+"\n";
    for(int i=0;i<curves;i++)model+="init x"+std::to_string(i)+"=0\n";
    CHECK(write_file("plotlimits.odex",model));
    CHECK(load("plotlimits.odex"));
    xpp::Session &s=xpp::client_session();
    xpp::batch_start(s);
    std::array<int,curves> variables;
    for(int i=0;i<curves;i++)variables[static_cast<std::size_t>(i)]=i+1;
    CHECK(xpp::plot_variables(s,xpp::PlotHow::time_series,variables));
    CHECK(s.plot_windows.current->nvars==curves && s.plot_windows.current->yv.back()==curves);
    CHECK(s.plot_windows.current->plot_points[0].size()==curves);
    xpp::DataTable data;
    data.names={"T"};data.columns={{0,1,2}};
    for(int i=0;i<curves;i++){
        data.names.push_back("x"+std::to_string(i));
        data.columns.push_back({static_cast<double>(i),i+0.5,i+1.0});
    }
    CHECK(xpp::put_stored_data(s,data)==3);
    for(int i=0;i<frozen;i++){const auto slot=xpp::freeze_curve(s,curves-1);CHECK(slot&&*slot==i);}
    CHECK(s.frozen_curves.curve.size()==frozen && s.frozen_curves.curve.back().yv[2]==curves);
    for(int i=1;i<windows;i++){
        xpp::create_a_pop(s);
        CHECK(s.plot_windows.active==i && s.plot_windows.current==&s.plot_windows.graph[i]);
        CHECK(s.plot_windows.current->nvars==curves);
        CHECK(s.plot_windows.current->w<WIN_AUTO || s.plot_windows.current->w>WIN_APLOT);
    }
    CHECK(s.plot_windows.graph.back().w==windows+WIN_APLOT-WIN_AUTO+1);
    xpp::make_active(s,7,1);
    xpp::destroy_a_pop(s);
    xpp::make_active(s,windows-1,1);
    xpp::delete_frz_crv(s,5);
    const auto bytes=xpp_session_snapshot(s);
    CHECK(bytes.has_value());
    if(!bytes)return;
    auto file=xpp_saved_parse(tmp.file("limits.snapx"),"limits.snapx",*bytes,SavedKind::snapshot);
    CHECK(file.has_value());
    if(!file)return;
    CHECK(!xpp_saved_check(s,*file));
    CHECK(file->members.at("windows.setx").find("name: \"graph_8\",") == std::string::npos);
    CHECK(file->members.at("marks.setx").find("slot: 5,") == std::string::npos);
    /* A scalar posing as a list, sparse hostile indices, and dishonest point
       counts fail at their member line, without changing any current state. */
    const auto bad=[&](const char *member,const char *old,const char *replacement){
        SavedFile damaged=*file;
        std::string &text=damaged.members.at(member);
        const auto pos=text.find(old);
        CHECK(pos!=std::string::npos);
        if(pos==std::string::npos)return;
        text.replace(pos,std::string_view(old).size(),replacement);
        const auto error=xpp_saved_check(s,damaged);
        CHECK(error && error->place.file.ends_with(member) && error->place.line>0 && !error->place.source.empty());
        CHECK(s.plot_windows.active==windows-1 && s.plot_windows.count==windows-1);
        CHECK(s.frozen_curves.curve.size()==frozen && !s.frozen_curves.curve[5].use);
    };
    bad("windows.setx","name: \"graph_1\"","name: \"\"");
    bad("windows.setx","active: \"graph_107\"","active: \"graph_8\"");
    const std::string &window_text=file->members.at("windows.setx");
    const auto curve_begin=window_text.find("curves: [");
    const auto curve_end=window_text.find(']',curve_begin);
    const std::string curve_list=window_text.substr(curve_begin,curve_end-curve_begin+1);
    bad("windows.setx",curve_list.c_str(),"curves: 2147483647");
    bad("marks.setx","slot: 0","slot: 2147483647");
    bad("marks.setx","points: 3","points: -1");
    bad("marks.setx","points: 3","points: 2147483647");
    bad("marks.setx","points: 3","points: 2");
    /* The real curve reader shares the budget with preceding window lists.
       A rejected oversized list leaves both the graph and live session intact. */
    GRAPH graph = *s.plot_windows.current;
    const auto old_y = graph.yv;
    std::size_t used = xpp::zip::archive_bytes_limit - 1;
    const auto rejected = xpp::setx::read_block("session", "limits.snapx/windows.setx", "\ncurves: [{}, {}]", true,
        [&](xpp::setx::Block &b) {
            xpp::read_curves(b, s, graph, {}, used);
            return true;
        });
    CHECK(!rejected && rejected.error().what == "too many curves for the session memory limit");
    CHECK(!rejected && rejected.error().place.line == 2 && rejected.error().place.file == "limits.snapx/windows.setx");
    CHECK(graph.yv == old_y && graph.nvars == curves && s.plot_windows.current->nvars == curves);
    /* .setx uses the same curve reader, independently of the archive. */
    const auto settings=xpp::setx_text(s,xpp::capture_settings(s,xpp::SettingsUse::file));
    CHECK(settings.has_value());
    if(settings){
        const auto read=xpp::read_settings(s,"limits.setx",*settings,xpp::SettingsUse::file);
        CHECK(read && read->graph.nvars==curves && read->graph.yv.back()==curves);
    }
    CHECK(write_file(tmp.file("limits.snapx"),*bytes));
    auto disk=xpp_saved_read(tmp.file("limits.snapx"));
    CHECK(disk.has_value());
    if(!disk)return;
    CHECK(load("plotlimits.odex",&disk->model));
    xpp::Session &restored=xpp::client_session();
    xpp::batch_start(restored);
    CHECK(xpp_saved_restore(restored,*disk));
    CHECK(restored.plot_windows.graph.size()==windows-1 && restored.plot_windows.count==windows-1);
    CHECK(restored.plot_windows.active==windows-2 && restored.plot_windows.graph[7].name=="graph_9");
    CHECK(restored.plot_windows.current->nvars==curves && restored.plot_windows.current->yv.back()==curves);
    CHECK(restored.frozen_curves.curve.size()==frozen && !restored.frozen_curves.curve[5].use);
    const auto again=xpp_session_snapshot(restored);
    CHECK(again.has_value());
    if(!again)return;
    const auto back=xpp_saved_parse(tmp.file("again.snapx"),"again.snapx",*again,SavedKind::snapshot);
    CHECK(back.has_value());
    if(back)for(const char *member:{"windows.setx","marks.setx","frozen.npz"})
        CHECK(back->members.at(member)==file->members.at(member));
}

} // namespace

int main(void)
{
    check_manifest();
    check_list_memory();
    check_nullclines();
    const std::string here = xpp::files::working_dir();
    {
        xpp::TempDir tmp;
        CHECK(!tmp.path().empty());
        check_saved_animation(tmp);
        check_saved_model(tmp);
        check_plot_limits(tmp);
        xpp::files::change_dir(here.c_str());
    }
    TEST_REPORT("test_snapx");
}
