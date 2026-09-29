/* The session file's manifest and fingerprint (snapx.h, W57): a manifest
   reads back as it was written, a file of another kind or version is
   refused, and the fingerprint is the SHA-256 of the model's bytes. */
#include "xpptest.h"
#include "snapx.h"

#include <string>

int main(void)
{
    using namespace xpp::snapx;

    Manifest m;
    m.model = "/home/me/my models/lecar.ode";
    m.model_name = "lecar.ode";
    m.sha256 = fingerprint("dv/dt=-v\n");
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

    /* the fingerprint: SHA-256 of the bytes */
    CHECK_STR(fingerprint("abc").c_str(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(fingerprint("dv/dt=-v\n") != fingerprint("dv/dt=-2*v\n"));

    CHECK(is_session_file("a/b.snapx") && is_session_file("B.SNAPX") && !is_session_file("b.snapx.zip"));
    CHECK_STR(session_file_name("run1").c_str(), "run1.snapx");
    CHECK_STR(session_file_name("run1.SnapX").c_str(), "run1.SnapX");

    TEST_REPORT("test_snapx");
}
