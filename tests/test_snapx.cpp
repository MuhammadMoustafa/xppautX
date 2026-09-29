/* The session file's manifest and fingerprint (snapx.h, W57): a manifest
   reads back as it was written, a file of another kind or version is
   refused, and the fingerprint covers the model's file and every file it
   includes: loaded, a model with an #include has both in its fingerprint,
   and an edit to the included file alone changes it. */
#include "xpptest.h"
#include "snapx.h"
#include "model.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_session.h"

#include <string>
#include <vector>

namespace {

bool write_file(const std::string &path, const char *text)
{
    xpp::Writer w(path.c_str());
    return w && w.write(text) && w.commit();
}

/* a model that includes another file: its fingerprint follows that file */
void check_included_files()
{
    xpp::TempDir tmp;
    CHECK(!tmp.path().empty());
    const std::string main_ode = tmp.file("main.ode"), inc = tmp.file("inc.ode");
    CHECK(write_file(inc, "par a=2\ndone\n"));
    const std::string text = "#include " + inc + "\nx'=-a*x\ninit x=1\ndone\n";
    CHECK(write_file(main_ode, text.c_str()));
    std::string arg0 = "test_snapx", arg1 = main_ode;
    char *argv[] = {arg0.data(), arg1.data(), nullptr};
    CHECK(xpp_load_model(2, argv, 1) == 1);
    const std::vector<std::string> &files = xpp::model().source_files;
    CHECK(files.size() == 2 && files[0] == main_ode && files[1] == inc);
    CHECK(xpp::model().nupar == 1 && (xpp::model().upar_names[0] == "a" || xpp::model().upar_names[0] == "A")); /* the included file was read */

    const std::string before = xpp_session_fingerprint();
    std::string main_bytes, inc_bytes;
    CHECK(xpp::read_bytes(main_ode.c_str(), main_bytes) && xpp::read_bytes(inc.c_str(), inc_bytes));
    CHECK(before == xpp::snapx::fingerprint(std::vector<std::string>{main_bytes, inc_bytes}));
    CHECK(before != xpp::snapx::fingerprint(std::vector<std::string>{main_bytes}));
    CHECK(write_file(inc, "par a=3\ndone\n")); /* the included file edited, the .ode not */
    CHECK(xpp_session_fingerprint() != before);
    CHECK(write_file(inc, "par a=2\ndone\n"));
    CHECK(xpp_session_fingerprint() == before);
}

} // namespace

int main(void)
{
    using namespace xpp::snapx;

    Manifest m;
    m.model = "/home/me/my models/lecar.ode";
    m.model_name = "lecar.ode";
    m.sha256 = fingerprint(std::vector<std::string>{"dv/dt=-v\n"});
    m.node = 2;
    m.nmarkov = 1;
    m.vars = {"V", "W", "Z", "IT"};
    m.pars = {"IAPP", "PHI"};
    m.data = true;
    const std::string text = manifest_text(m);
    CHECK(text.starts_with("xppautX session 1\n"));
    CHECK(text.find("\nmodel /home/me/my models/lecar.ode\n") != std::string::npos);
    CHECK(text.find("\nvars V W Z IT\n") != std::string::npos);
    const std::optional<Manifest> back = parse_manifest(text);
    CHECK(back.has_value() && *back == m);

    /* no names, no data; CRLF line ends read the same */
    Manifest e;
    e.model = "C:\\models\\x.ode";
    std::string crlf;
    for (char c : manifest_text(e)) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    const std::optional<Manifest> eb = parse_manifest(crlf);
    CHECK(eb.has_value() && *eb == e && eb->vars.empty() && !eb->data);

    /* a later key is skipped; another file, a later version or a bad count is refused */
    CHECK(parse_manifest(text + "future thing\n").has_value());
    CHECK(!parse_manifest("# Set file\n").has_value());
    CHECK(!parse_manifest("").has_value());
    CHECK(!parse_manifest("xppautX session 2\n").has_value());
    CHECK(!parse_manifest("xppautX session 1\nnode two\n").has_value());

    /* the fingerprint: SHA-256 of each file's length and bytes, in order */
    using files = std::vector<std::string>;
    CHECK_STR(fingerprint(files{"abc"}).c_str(), "b00c078ef550998002854805b6d1fac35a8601ebe3d9a9b32bf6f6157038e1b9");
    CHECK(fingerprint(files{"dv/dt=-v\n"}) != fingerprint(files{"dv/dt=-2*v\n"}));
    CHECK(fingerprint(files{"x'=-x\n", "par a=1\n"}) != fingerprint(files{"x'=-x\n", "par a=2\n"}));
    CHECK(fingerprint(files{"ab", "c"}) != fingerprint(files{"a", "bc"}));
    CHECK(fingerprint(files{"x", "y"}) != fingerprint(files{"y", "x"}));
    check_included_files();

    CHECK(is_session_file("a/b.snapx") && is_session_file("B.SNAPX") && !is_session_file("b.snapx.zip"));
    CHECK_STR(session_file_name("run1").c_str(), "run1.snapx");
    CHECK_STR(session_file_name("run1.SnapX").c_str(), "run1.SnapX");

    TEST_REPORT("test_snapx");
}
