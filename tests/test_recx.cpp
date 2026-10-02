/* A recording's file (recx.h, W59a): the header, each file as a section
   (a line that would end it written with one more "@"), the steps with
   their notes as # lines, and a fingerprint of the files and the steps
   that a note does not change and a step does. */
#include "xpptest.h"
#include "recx.h"
#include "xpp_sha256.h"

#include <optional>
#include <string>

namespace {

xpp::recx::Recording sample()
{
    xpp::recx::Recording r;
    r.program = "xppautX dev";
    r.model = "lecar.odex";
    r.recorded = "2026-09-30T10:14:02Z";
    r.snapshot = std::string("PK\x05\x06", 4); /* the session, a zip (W59d) */
    xpp::recx::add_file(r, {"lecar.odex", "x'=-x\r\n@end\n@@ twice\n@ total=10\ndone"});
    xpp::recx::add_file(r, {"lecar.odex", "x'=-x\r\n@end\n@@ twice\n@ total=10\ndone"}); /* the same: once */
    r.steps.push_back({"First run.\n\nIt settles.", R"({"step":"Initialconds → Go","keys":["i","g"]})"});
    r.steps.push_back({"", R"({"step":"Erase","keys":["e"]})"});
    return r;
}

std::string sha(const std::string &hashed)
{
    xpp::Sha256 s;
    s.update(hashed.data(), hashed.size());
    return s.hex();
}

} // namespace

int main()
{
    xpp::recx::Recording r = sample();
    CHECK(r.files.size() == 1);
    const std::string hashed = "@snapshot\nUEsFBg==\n@end\n@file lecar.odex\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n"
                               "{\"step\":\"Initialconds → Go\",\"keys\":[\"i\",\"g\"]}\n{\"step\":\"Erase\",\"keys\":[\"e\"]}\n";
    const std::string want = "xppautx-recording 1\nprogram: xppautX dev\nmodel: lecar.odex\nrecorded: 2026-09-30T10:14:02Z\n\n"
                             "@snapshot\nUEsFBg==\n@end\n\n@file lecar.odex\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n\n@steps\n"
                             "# First run.\n#\n# It settles.\n{\"step\":\"Initialconds → Go\",\"keys\":[\"i\",\"g\"]}\n"
                             "{\"step\":\"Erase\",\"keys\":[\"e\"]}\n\nfingerprint: "
                             + sha(hashed) + "\n";
    CHECK_STR(xpp::recx::text(r).c_str(), want.c_str());

    /* a note is not in the fingerprint; a step and a file are */
    const std::string fp = xpp::recx::text(r).substr(want.size() - 65);
    r.steps[1].note = "Clear the screen.";
    CHECK(xpp::recx::text(r).ends_with(fp));
    r.steps[1].line = R"({"step":"Erase","keys":["x"]})";
    CHECK(!xpp::recx::text(r).ends_with(fp));
    r = sample();
    xpp::recx::add_file(r, {"lecar.odex", "x'=-2*x\n"}); /* read again, changed: a second section */
    CHECK(r.files.size() == 2);
    CHECK(!xpp::recx::text(r).ends_with(fp));

    /* read back: the same recording, intact; a note edited keeps it so,
       and the text written again with it keeps what the file said */
    r = sample();
    xpp::recx::add_file(r, {"lecar.snapx", std::string("PK\x03\x04\0binary\xff", 12)}); /* a zip: @binary */
    const std::string file = xpp::recx::text(r);
    CHECK(file.find("\n@binary lecar.snapx\nUEsDBABiaW5hcnn/\n@end\n") != std::string::npos);
    xpp::Result<xpp::recx::Read> back = xpp::recx::read(file, "");
    CHECK(back && back->intact);
    CHECK(back->rec.files.size() == 2 && back->rec.files[1].bytes == r.files[1].bytes);
    CHECK_STR(back->rec.files[0].bytes.c_str(), "x'=-x\n@end\n@@ twice\n@ total=10\ndone\n");
    CHECK(back->rec.steps.size() == 2);
    CHECK(back->rec.snapshot == r.snapshot);
    CHECK_STR(back->rec.steps[0].note.c_str(), "First run.\n\nIt settles.");
    CHECK_STR(xpp::recx::text(back->rec).c_str(), file.c_str());
    std::string edited = file;
    edited.replace(edited.find("# It settles."), 13, "# It fires and settles.");
    back = xpp::recx::read(edited, "");
    CHECK(back && back->intact);
    edited.replace(edited.find("[\"e\"]"), 5, "[\"x\"]");
    back = xpp::recx::read(edited, "");
    CHECK(back && !back->intact); /* a step changed: it still reads */
    back->rec.steps[0].note = "Saved again.";
    CHECK(!xpp::recx::read(xpp::recx::text(back->rec), "")->intact); /* saving a note does not mend it */
    /* each refusal at its line (W125): "line N: what" with no file named */
    const auto error = [](std::string_view text) {
        const xpp::Result<xpp::recx::Read> e = xpp::recx::read(text, "");
        return e ? std::string() : e.error().text();
    };
    const std::string head = "xppautx-recording 1\nprogram: xppautX\nmodel: a.odex\nrecorded: now\n\n";
    CHECK(error("xppautx-recording 1\nprogram: x\nmodel: a.ode\n") == "line 3: a recording stores an .odex model only; .ode is refused");
    CHECK(error(head + "@snapshot\nUEsFBg==\n@end\n@file a.odex\nx'=1\n") == "line 9: the section of a.odex has no @end");
    /* no fallback: a file without the session it began from is not a recording */
    CHECK(error(head + "@file a.odex\nx'=1\n@end\n@steps\n") == "line 6: no @snapshot section before the files: a recording begins with the session's state");
    CHECK(error(head + "@steps\n") == "line 6: no @snapshot section: a recording begins with the session's state");
    CHECK(error("not one\n").starts_with("line 1: not a recording"));
    /* the header: each line once, none other, a model named */
    CHECK(error("xppautx-recording 1\nprogram: x\nmodel: a.odex\n\n") == "line 4: the header has no \"recorded:\" line");
    CHECK(error("xppautx-recording 1\nprogram: x\nmodel: a.odex\nmodel: b.odex\n") == "line 4: a second \"model:\" line");
    CHECK(error("xppautx-recording 1\nauthor: me\n") == "line 2: \"author: me\" is not a header line (program:, model:, recorded:)");
    /* the fingerprint ends it: missing, or a line after it */
    const std::string steps = head + "@snapshot\nUEsFBg==\n@end\n@steps\n{\"keys\":[\"g\"]}\n";
    CHECK(error(steps) == "line 11: the file ends here, before its fingerprint");
    CHECK(error(steps + "fingerprint: 00\nmore\n") == "line 12: \"more\" after the end of what the file holds");
    const xpp::Result<xpp::recx::Read> one = xpp::recx::read(steps + "fingerprint: 00\n", "");
    CHECK(one && !one->intact && one->rec.steps.size() == 1 && one->rec.steps[0].at == 10);
    TEST_REPORT("test_recx");
}
