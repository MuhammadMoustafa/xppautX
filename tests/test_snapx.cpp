/* The files that carry a model (snapx.h, xpp_session.h; W57, W103): a
   manifest reads back as it was written, a file of another kind or
   version is refused; a loaded model records every file it read (the
   .ode, an included file, a file table) and a file written with it
   carries them all; that saved model loads from the file alone, the
   files on the disk gone, and a file without its model is refused. */
#include "xpptest.h"
#include "snapx.h"
#include "model.h"
#include "model_files.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_session.h"
#include "xpp_zip.h"

#include <string>
#include <vector>

namespace {

bool write_file(const std::string &path, std::string_view text)
{
    xpp::Writer w = xpp::Writer::binary(path.c_str());
    return w && w.write(text) && w.commit();
}

bool load(const std::string &file, const xpp::SavedModel *saved = nullptr)
{
    std::string arg0 = "test_snapx", arg1 = file;
    char *argv[] = {arg0.data(), arg1.data(), nullptr};
    return xpp::load_model(2, argv, 1, saved).has_value();
}

void check_manifest()
{
    using namespace xpp::snapx;
    Manifest m;
    m.model_name = "my models/lecar.ode";
    m.anifile = "lecar.ani";
    m.data = true;
    const std::string text = manifest_text(m);
    CHECK(text.starts_with("xppautX session 1\n"));
    CHECK(text.find("\nname my models/lecar.ode\n") != std::string::npos);
    const auto back = parse_manifest("s/session.txt", text);
    CHECK(back.has_value() && *back == m);

    /* no animation, no data; CRLF line ends read the same */
    Manifest e;
    e.model_name = "x.ode";
    std::string crlf;
    for (char c : manifest_text(e)) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    const auto eb = parse_manifest("s/session.txt", crlf);
    CHECK(eb.has_value() && *eb == e && eb->anifile.empty() && !eb->data);

    /* a key it does not have, another file or a later version is refused,
       saying what is wrong */
    const auto later = parse_manifest("s/session.txt", text + "future thing\n");
    CHECK(!later && later.error().place.file == "s/session.txt" && later.error().place.line == 5 && later.error().place.source == "future thing");
    CHECK(!parse_manifest("s/session.txt", "xppautX session 1\nname x.ode\ndata 2\n").has_value()); /* data is 0 or 1 */
    CHECK(!parse_manifest("s/session.txt", "xppautX session 1\ndata 0\n").has_value());           /* no model */
    CHECK(!parse_manifest("s/session.txt", "xppautX session 1\nname a.ode\nname b.ode\n").has_value()); /* a key twice */
    CHECK(!parse_manifest("s/session.txt", "# Set file\n").has_value());
    CHECK(!parse_manifest("s/session.txt", "").has_value());
    CHECK(!parse_manifest("s/session.txt", "xppautX session 2\n").has_value());
    CHECK(!parse_manifest("s/session.txt", manifest_text(m), "autox").has_value());

    /* the model's members: model/<name>, in order; the model's own needed */
    std::vector<xpp::zip::Entry> entries{{"session.txt", text}};
    const std::vector<xpp::ModelFile> files{{"a.ode", "x'=-x\n"}, {"../t/w.tab", "2 0 1 0 1\n"}};
    add_model_members(entries, files);
    entries.push_back({"model.set", "#\n"});
    CHECK(entries.size() == 4 && entries[1].name == "model/a.ode" && entries[2].name == "model/../t/w.tab");
    const std::optional<std::vector<xpp::ModelFile>> got = model_members(entries, "a.ode");
    CHECK(got && *got == files);
    CHECK(!model_members(entries, "b.ode"));
    CHECK(!model_members({{"session.txt", text}}, "a.ode"));

    CHECK(is_session_file("a/b.snapx") && is_session_file("B.SNAPX") && !is_session_file("b.snapx.zip"));
    CHECK_STR(session_file_name("run1").c_str(), "run1.snapx");
    CHECK_STR(session_file_name("run1.SnapX").c_str(), "run1.SnapX");
    CHECK(xpp_saved_file_name("d.autox") && xpp_saved_file_name("s.SNAPX") && !xpp_saved_file_name("m.ode"));
}

/* a model with an included file and a file table: recorded, saved, read
   back and loaded from the saved copies alone */
void check_saved_model(const xpp::TempDir &tmp)
{
    const std::string dir = tmp.path();
    CHECK(xpp::files::change_dir(dir.c_str()) == 0);
    CHECK(write_file("inc.ode", "par a=2\ndone\n"));
    CHECK(write_file("w.tab", "3\n0\n2\n0\n10\n40\n"));
    CHECK(write_file("main.ode", "#include inc.ode\ntable w w.tab\nx'=-a*x+w(1)\ninit x=1\ndone\n"));
    CHECK(load("main.ode"));
    const std::vector<xpp::ModelFile> files = xpp::client_session().model().files;
    CHECK(files.size() >= 3 && files[0].name == "main.ode" && files[1].name == "inc.ode" && files[2].name == "w.tab");
    CHECK(xpp::client_session().model().saved_in.empty() && xpp::client_session().model().nupar == 1);

    /* written as a session file's first members, read back whole */
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(xpp::client_session(), xpp::snapx::Manifest{}, xpp::snapx::session_kind);
    CHECK(entries.has_value());
    if (!entries) return;
    const std::string path = tmp.file("s.snapx");
    CHECK(write_file(path, xpp::zip::make_zip(*entries)));
    std::optional<SavedFile> f = xpp_saved_read(path);
    CHECK(f && f->session && f->manifest.model_name == "main.ode" && f->model.files == files && f->model.in == path);
    if (!f) return;
    CHECK(xpp_saved_args(*f) == std::vector<std::string>{"main.ode"});

    /* the files on the disk gone: the saved model loads from the file */
    for (const char *name : {"main.ode", "inc.ode", "w.tab"}) CHECK(xpp::files::remove(name) == 0);
    CHECK(load("main.ode", &f->model));
    CHECK(xpp::client_session().model().saved_in == path && xpp::client_session().model().files == files && xpp::client_session().model().nupar == 1);
    CHECK(xpp::client_session().model().upar_names[0] == "a" || xpp::client_session().model().upar_names[0] == "A");
    CHECK(xpp::model_title(xpp::client_session().model()) == "main.ode (saved in s.snapx)");
    std::string bytes;
    CHECK(xpp::read_model_file(xpp::client_session().model(), "w.tab", bytes) && bytes == files[2].bytes);
    CHECK(!xpp::read_model_file(xpp::client_session().model(), "other.tab", bytes)); /* not saved: not there */

    /* the same bytes on the disk, edited: the saved model still loads as saved */
    CHECK(write_file("main.ode", "par a=5,b=1\nx'=-a*x\ninit x=1\ndone\n"));
    CHECK(load("main.ode", &f->model));
    CHECK(xpp::client_session().model().nupar == 1 && xpp::client_session().model().files == files);

    /* a file without its model is refused, as is one that is no zip */
    std::vector<xpp::zip::Entry> no_model{(*entries)[0]};
    CHECK(write_file(path, xpp::zip::make_zip(no_model)));
    CHECK(!xpp_saved_read(path));
    CHECK(write_file(path, "not a zip"));
    CHECK(!xpp_saved_read(path));

    /* a zip or another binary file opened as a model is refused */
    CHECK(write_file("zip.ode", xpp::zip::make_zip(*entries)));
    CHECK(!load("zip.ode"));
    CHECK(write_file("bin.ode", std::string_view("x'=-x\n\0\1\2", 9)));
    CHECK(!load("bin.ode"));
    CHECK(xpp::client_session().model().saved_in == path); /* the model before is still loaded */
    CHECK(xpp::is_model_text("x'=-x\n") && !xpp::is_model_text(std::string_view("a\0b", 3)));
}

} // namespace

int main(void)
{
    check_manifest();
    const std::string here = xpp::files::working_dir();
    {
        xpp::TempDir tmp;
        CHECK(!tmp.path().empty());
        check_saved_model(tmp);
        xpp::files::change_dir(here.c_str());
    }
    TEST_REPORT("test_snapx");
}
