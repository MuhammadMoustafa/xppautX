/* Installing a downloaded release (W622, core/xpp_install.h, xpp_zip.h's tar reader): how the program was installed is told
   from where it runs and its version; a hostile archive (traversal, absolute names, links, devices, extended headers, a size
   larger than the archive, a wrong checksum, a bomb) is refused whole; the program swapped in is the checked archive's, and
   a file changed after its check is not; the kept installer file is offered only while it is what its record says. */
#include "xpptest.h"
#include "xpp_files.h"
#include "xpp_install.h"
#include "xpp_io.h"
#include "xpp_sha256.h"
#include "xpp_zip.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {


/* a ustar member: name, type, bytes, its header's checksum right unless `wrong_sum` */
std::string member(const std::string &name, char type, const std::string &bytes, const std::string &size_field = {}, bool wrong_sum = false)
{
    std::string h(512, '\0');
    h.replace(0, name.size(), name);
    h.replace(100, 7, "0000755");
    h.replace(108, 7, "0000000");
    h.replace(116, 7, "0000000");
    const std::string size = size_field.empty() ? [&] {
        std::string o(11, '0');
        std::size_t n = bytes.size();
        for (int i = 10; i >= 0; i--, n /= 8) o[static_cast<std::size_t>(i)] = static_cast<char>('0' + n % 8);
        return o;
    }() : size_field;
    h.replace(124, size.size(), size);
    h.replace(136, 11, "00000000000");
    h[156] = type;
    h.replace(257, 5, "ustar");
    h.replace(148, 8, "        ");
    unsigned sum = 0;
    for (const unsigned char c : h) sum += c;
    std::string field(6, '0');
    for (int i = 5; i >= 0; i--, sum /= 8) field[static_cast<std::size_t>(i)] = static_cast<char>('0' + sum % 8);
    h.replace(148, 6, field);
    h[154] = '\0';
    h[155] = ' ';
    if (wrong_sum) h[148] = h[148] == '7' ? '6' : '7';
    std::string out = h + bytes;
    out.append((512 - bytes.size() % 512) % 512, '\0');
    return out;
}

std::string tar_of(const std::vector<std::string> &members) { return std::string([&] { std::string s; for (const auto &m : members) s += m; return s; }()) + std::string(1024, '\0'); }

const std::string PROGRAM_BYTES = std::string("\x7f" "ELF") + "a program of this system";

std::string sha(const std::string &bytes)
{
    xpp::Sha256 h;
    h.update(bytes.data(), bytes.size());
    return "sha256:" + h.hex();
}

void check_names()
{
    for (const char *ok : {"a", "a/b", "xppautX-v1/xppautX", "a/b.c/d-e_f"}) CHECK(xpp::zip::entry_name_ok(ok));
    for (const char *bad : {"", "/etc/passwd", "../x", "a/../x", "a/..", "./a", "a/./b", "a//b", "a/", "C:/x", "a\\b", "a\x01" "b", "a:b"}) CHECK(!xpp::zip::entry_name_ok(bad));
    CHECK(!xpp::zip::entry_name_ok(std::string(256, 'a')));
    CHECK(xpp::zip::entry_name_ok(std::string(255, 'a')));
}

void check_tar()
{
    const auto read = [](const std::string &tar) { return xpp::zip::read_tar(tar, "x.tar"); };
    const auto refused = [&](const std::string &tar, const char *word) {
        const auto r = read(tar);
        CHECK(!r);
        if (!r) { CHECK(r.error().what.find(word) != std::string::npos); }
        if (!r) CHECK(r.error().place.file == "x.tar");
    };
    const auto good = read(tar_of({member("d/", '5', ""), member("d/xppautX", '0', PROGRAM_BYTES), member("d/note", '\0', "hi")}));
    CHECK(good && good->size() == 2 && (*good)[0].name == "d/xppautX" && (*good)[0].bytes == PROGRAM_BYTES && (*good)[1].bytes == "hi");
    CHECK(read(tar_of({}))->empty());
    /* a hostile archive is refused whole: nothing is returned */
    refused(tar_of({member("../evil", '0', "x")}), "plain relative path");
    refused(tar_of({member("/etc/evil", '0', "x")}), "plain relative path");
    refused(tar_of({member("a/../../evil", '0', "x")}), "plain relative path");
    refused(tar_of({member("d/xppautX", '0', "x"), member("d/xppautX", '0', "y")}), "twice");
    refused(tar_of({member("d/link", '2', "")}), "links, devices");
    refused(tar_of({member("d/hard", '1', "")}), "links, devices");
    refused(tar_of({member("d/dev", '3', "")}), "links, devices");
    refused(tar_of({member("d/fifo", '6', "")}), "links, devices");
    refused(tar_of({member("pax", 'x', "15 path=/etc/x\n")}), "links, devices");
    refused(tar_of({member("long", 'L', "d/xppautX")}), "links, devices");
    refused(tar_of({member("d/a", '0', "x", {}, true)}), "checksum");
    refused(tar_of({member("d/a", '0', "x", "77777777777")}), "larger");
    refused(tar_of({member("d/a", '0', "x", "7777777777777")}), "octal");
    refused(tar_of({member("d/a", '0', "x", "-000000001")}), "octal");
    refused(tar_of({member("d/a", '0', "x", "1000000000000")}), "octal");
    refused(tar_of({member("d/", '5', "x")}), "has data");
    refused(member("d/a", '0', "x"), "end marker");          /* no end marker */
    refused(member("d/a", '0', "x").substr(0, 300), "header"); /* cut inside a header */
    std::string trailing = tar_of({member("d/a", '0', "x")});
    trailing += "junk";
    refused(trailing, "end marker");
    std::vector<std::string> many;
    for (std::size_t i = 0; i <= xpp::zip::archive_entries_limit; i++) many.push_back(member("d/f" + std::to_string(i), '0', ""));
    refused(tar_of(many), "too many");
    /* a gzip that expands is read through gunzip's limit, and what is not gzip is refused */
    const auto gz = xpp::zip::read_tar_gz(xpp::zip::gzip(tar_of({member("d/xppautX", '0', PROGRAM_BYTES)})), "x.tar.gz");
    CHECK(gz && gz->size() == 1);
    CHECK(!xpp::zip::read_tar_gz("not gzip at all, but long enough to be looked at", "x.tar.gz"));
}

xpp::update::Where where(const std::string &version, xpp::update::Os os, const std::string &folder, const std::string &system, bool writable)
{
    return xpp::update::Where{version, os, folder, system, writable};
}

void check_plan()
{
    const auto how = [](const xpp::update::Where &w) { return xpp::update::plan_for(w); };
    const auto linux_at = [](const std::string &v, const std::string &folder, bool writable) { return where(v, xpp::update::Os::linux_x64, folder, "/usr/bin", writable); };
    CHECK(how(linux_at("v0.2.0", "/home/u/xppautX", true)).install == xpp::update::Install::portable);
    CHECK(how(linux_at("v0.2.0", "/home/u/xppautX", true)).asset_suffix == "-linux-x64.tar.gz");
    CHECK(how(linux_at("v0.2.0-beta.1", "/home/u/xppautX", true)).install == xpp::update::Install::portable);
    CHECK(how(linux_at("v0.2.0", "/usr/bin", false)).install == xpp::update::Install::deb);
    CHECK(how(linux_at("v0.2.0", "/usr/bin", false)).asset_suffix == "-linux-x64.deb");
    CHECK(how(linux_at("v0.2.0", "/opt/xppautX", false)).install == xpp::update::Install::release_page);
    CHECK(how(linux_at("v0.2.0", "/opt/xppautX", false)).why.find("cannot be written") != std::string::npos);
    CHECK(how(linux_at("v0.2.0", "", true)).install == xpp::update::Install::release_page);
    /* a build from a source checkout never installs over itself, wherever it is */
    for (const char *source : {"v0.2.0-12-gabcdef0", "v0.2.0-beta.1-3-g1234567", "abcdef0", "dev", ""}) {
        const xpp::update::Plan p = how(linux_at(source, "/home/u/xppautX", true));
        CHECK(p.install == xpp::update::Install::release_page && p.asset_suffix.empty() && p.why.find("source checkout") != std::string::npos);
    }
    const xpp::update::Plan win = how(where("v0.2.0", xpp::update::Os::windows_x64, "C:\\Users\\u\\xppautX", "C:\\Program Files\\xppautX", true));
    CHECK(win.install == xpp::update::Install::portable && win.asset_suffix == "-windows-x64.zip");
    const xpp::update::Plan msi = how(where("v0.2.0", xpp::update::Os::windows_x64, "c:\\program files\\XPPAUTX", "C:\\Program Files\\xppautX", false));
    CHECK(msi.install == xpp::update::Install::msi && msi.asset_suffix == "-windows-x64.msi");
    /* a case-different Linux folder is another folder */
    CHECK(how(where("v0.2.0", xpp::update::Os::linux_x64, "/USR/bin", "/usr/bin", false)).install == xpp::update::Install::release_page);
    CHECK(how(where("v0.2.0", xpp::update::Os::macos, "/Applications", "", true)).install == xpp::update::Install::release_page);
    CHECK(how(where("v0.2.0", xpp::update::Os::other, "/x", "", true)).install == xpp::update::Install::release_page);
    CHECK(xpp::update::is_release_version("v1.2.3") && xpp::update::is_release_version("v1.2.3-rc.1") && !xpp::update::is_release_version("v1.2.3-4-gabc1234") && !xpp::update::is_release_version("1.2.3"));
    const xpp::update::Plan p = xpp::update::plan_for(linux_at("v0.2.0", "/home/u/x", true));
    CHECK(xpp::update::fits(p, "xppautX-v0.2.0-linux-x64.tar.gz") && !xpp::update::fits(p, "xppautX-v0.2.0-linux-x64.deb") && !xpp::update::fits(p, "xppautX-v0.2.0-windows-x64.zip") &&
          !xpp::update::fits(p, "other-v0.2.0-linux-x64.tar.gz") && !xpp::update::fits(p, "xppautX-linux-x64.tar.gz") && !xpp::update::fits(p, "-linux-x64.tar.gz") && !xpp::update::fits(xpp::update::Plan{}, "xppautX-v1-"));
    CHECK(xpp::update::version_ok("0.2.0-beta.1") && !xpp::update::version_ok("") && !xpp::update::version_ok("0.2 0") && !xpp::update::version_ok("../x") && !xpp::update::version_ok(std::string(65, '1')));
}

/* the swap: a fake "running program", an archive built here, the release list's size and digest */
struct Swap {
    xpp::TempDir scratch;
    std::filesystem::path dir = scratch.path();
    std::string running = (dir / "running").string(), archive = (dir / "a.tar.gz").string();
    Swap() { xpp::Writer w = xpp::Writer::binary(running); w.write("\x7f" "ELF old"); (void)w.commit(); }
    xpp::update::ReleaseFile put(const std::string &tar_gz)
    {
        xpp::Writer w = xpp::Writer::binary(archive);
        w.write(tar_gz);
        (void)w.commit();
        return xpp::update::ReleaseFile{"", "xppautX-v0.2.0-linux-x64.tar.gz", tar_gz.size(), sha(tar_gz)};
    }
    std::string running_bytes() const { std::string b; xpp::read_bytes(running, b); return b; }
};

void check_swap()
{
    const xpp::update::Plan plan = xpp::update::plan_for(where("v0.2.0", xpp::update::Os::linux_x64, "/home/u/x", "/usr/bin", true));
    const auto swap = [&](Swap &s, const std::string &tar, const xpp::update::ReleaseFile *override = nullptr) {
        const xpp::update::ReleaseFile f = s.put(xpp::zip::gzip(tar));
        return xpp::update::swap_program(plan, override ? *override : f, s.archive, s.running);
    };
    {
        Swap s;
        const auto done = swap(s, tar_of({member("d/", '5', ""), member("d/xppautX", '0', PROGRAM_BYTES), member("d/LICENSE", '0', "text")}));
        CHECK(done && s.running_bytes() == PROGRAM_BYTES && !std::filesystem::exists(s.running + ".new"));
#ifndef _WIN32
        CHECK((std::filesystem::status(s.running).permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
#endif
    }
    /* every refusal leaves the running program as it was and no .new beside it */
    const auto refused = [&](const std::string &tar, const char *word) {
        Swap s;
        const auto done = swap(s, tar);
        CHECK(!done);
        if (!done) CHECK(done.error().what.find(word) != std::string::npos);
        CHECK(s.running_bytes() == "\x7f" "ELF old" && !std::filesystem::exists(s.running + ".new"));
    };
    refused(tar_of({member("d/LICENSE", '0', "text")}), "no single");
    refused(tar_of({member("a/xppautX", '0', PROGRAM_BYTES), member("b/xppautX", '0', PROGRAM_BYTES)}), "no single");
    refused(tar_of({member("xppautX", '0', PROGRAM_BYTES)}), "no single");
    refused(tar_of({member("d/e/xppautX", '0', PROGRAM_BYTES)}), "no single");
    refused(tar_of({member("d/xppautX", '0', "MZ not for this system")}), "not a program");
    refused(tar_of({member("d/xppautX", '0', PROGRAM_BYTES), member("../evil", '0', "x")}), "plain relative path");
    refused(tar_of({member("d/xppautX", '2', "")}), "links, devices");
    {
        /* the archive on disk is not the one that was checked: the digest, or the size, differs */
        Swap s;
        xpp::update::ReleaseFile f = s.put(xpp::zip::gzip(tar_of({member("d/xppautX", '0', PROGRAM_BYTES)})));
        f.digest = sha("another file");
        const auto done = xpp::update::swap_program(plan, f, s.archive, s.running);
        CHECK(!done && done.error().what.find("changed after it was checked") != std::string::npos && s.running_bytes() == "\x7f" "ELF old");
        f = s.put(xpp::zip::gzip(tar_of({member("d/xppautX", '0', PROGRAM_BYTES)})));
        f.size += 1;
        CHECK(!xpp::update::swap_program(plan, f, s.archive, s.running));
    }
    {
        /* not gzip, though its digest is the file's */
        Swap s;
        const xpp::update::ReleaseFile f = s.put("this is not a gzip file at all, it is just text of some length");
        CHECK(!xpp::update::swap_program(plan, f, s.archive, s.running) && s.running_bytes() == "\x7f" "ELF old");
    }
    {
        /* a zip (the Windows portable's form): names are checked as the tar's are */
        const xpp::update::Plan zipped = xpp::update::plan_for(where("v0.2.0", xpp::update::Os::windows_x64, "C:\\x", "C:\\Program Files\\xppautX", true));
        Swap s;
        const std::string zip = xpp::zip::make_zip({{"d/xppautX.exe", "MZ program"}, {"../evil", "x"}});
        xpp::Writer w = xpp::Writer::binary(s.archive);
        w.write(zip);
        (void)w.commit();
        const xpp::update::ReleaseFile f{"", "xppautX-v0.2.0-windows-x64.zip", zip.size(), sha(zip)};
        const auto done = xpp::update::swap_program(zipped, f, s.archive, s.running);
        CHECK(!done && done.error().what.find("plain relative path") != std::string::npos);
    }
}

void check_kept()
{
    const xpp::TempDir scratch;
    const std::filesystem::path config = scratch.path();
    test_setenv(xpp::files::CONFIG_DIR_ENV, config.string().c_str());
    const xpp::update::Plan deb = xpp::update::plan_for(where("v0.2.0", xpp::update::Os::linux_x64, "/usr/bin", "/usr/bin", false));
    const std::string body = "!<arch>\ndebian-binary a package of some length";
    const std::string staged = (config / "download.deb").string();
    const xpp::update::ReleaseFile file{"https://github.com/MuhammadMoustafa/xppautX/releases/download/v0.2.0/xppautX-v0.2.0-linux-x64.deb", "xppautX-v0.2.0-linux-x64.deb", body.size(), sha(body)};
    const auto stage = [&] { xpp::Writer w = xpp::Writer::binary(staged); w.write(body); (void)w.commit(); };
    CHECK(deb.install == xpp::update::Install::deb);
    auto none = xpp::update::pending(deb);
    CHECK(none && !*none);
    stage();
    CHECK(xpp::update::prepare(deb, file, "0.2.0", staged) == xpp::update::Ready::install);
    CHECK(!std::filesystem::exists(staged));
    auto kept = xpp::update::pending(deb);
    CHECK(kept && *kept && (*kept)->version == "0.2.0" && (*kept)->name == file.name && std::filesystem::exists((*kept)->path));
    /* a file that is not the one recorded (changed, cut, or longer) is refused and everything kept is removed with a reason */
    for (const std::string &changed : {body + "x", body.substr(0, body.size() - 1), std::string(body.size(), 'x')}) {
        stage();
        CHECK(xpp::update::prepare(deb, file, "0.2.0", staged));
        { xpp::Writer w = xpp::Writer::binary((*xpp::update::pending(deb))->path); w.write(changed); (void)w.commit(); }
        const auto bad = xpp::update::pending(deb);
        CHECK(!bad && bad.error().what.find("not the one recorded") != std::string::npos);
        const auto after = xpp::update::pending(deb);
        CHECK(after && !*after);
    }
    /* a record that is not exactly its four lines is refused */
    for (const char *record : {"", "version 0.2.0\n", "version 0.2.0\nname x.deb\nsize 5\nsha256 00\n", "name a\nversion 0.2.0\nsize 5\nsha256 0\n", "version 0.2.0\nname ../x.deb\nsize 5\nsha256 0\nextra\n"}) {
        stage();
        CHECK(xpp::update::prepare(deb, file, "0.2.0", staged));
        const std::string path = xpp::update::kept_folder() + "/pending";
        { xpp::Writer w = xpp::Writer::binary(path); w.write(record); (void)w.commit(); }
        const auto bad = xpp::update::pending(deb);
        CHECK(!bad && !bad.error().place.file.empty());
        CHECK(std::filesystem::is_empty(xpp::update::kept_folder()));
    }
    /* a file for another installation, a wrong version text, the release page */
    stage();
    CHECK(!xpp::update::prepare(deb, xpp::update::ReleaseFile{file.url, "xppautX-v0.2.0-linux-x64.tar.gz", file.size, file.digest}, "0.2.0", staged));
    CHECK(!xpp::update::prepare(deb, file, "../0.2.0", staged));
    CHECK(!xpp::update::prepare(xpp::update::Plan{}, file, "0.2.0", staged));
    stage();
    CHECK(xpp::update::prepare(deb, file, "0.2.0", staged));
    const xpp::update::Plan portable = xpp::update::plan_for(where("v0.2.0", xpp::update::Os::linux_x64, "/home/u/x", "/usr/bin", true));
    CHECK(!xpp::update::start_installer(portable, "", false));
    xpp::update::discard_pending();
    CHECK(std::filesystem::is_empty(xpp::update::kept_folder()));
    test_setenv(xpp::files::CONFIG_DIR_ENV, nullptr);
}

} // namespace

TEST_CASE("test_install")
{
    check_names();
    check_tar();
    check_plan();
    check_swap();
    check_kept();
    TEST_REPORT("install");
}
