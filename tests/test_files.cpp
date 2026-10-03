/* xpp_files: what the page may read and write in the model's folder
   (docs/ui-v2.md section 4). A name that slips through reaches a file
   outside the folder; an upload cut short or over the cap must leave
   nothing behind. tools/webcheck.py checks the same through HTTP, and
   tools/servercheck.py through the protocol's `file` command. */
#include "xpptest.h"
#include "xpp_files.h"
#include "json_files.h"
#include "snapx.h"
#include "odex.h"
#ifdef _WIN32
#include "xpp_win32.h"
#endif
#include "xpp_mem.h"
#include "xpp_sha256.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <io.h>
#include <cerrno>
#else
#include <unistd.h>
#endif

namespace {

std::string sha_of(const void *p, size_t n)
{
    xpp::Sha256 c;
    c.update(p, n);
    return c.hex();
}

/* the names in the current folder, hidden ones included */
std::string folder()
{
    std::string all;
    DIR *d = opendir(".");
    while (struct dirent *e = d ? readdir(d) : nullptr) {
        if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
        if (!all.empty()) all += ' ';
        all += e->d_name;
    }
    if (d) closedir(d);
    return all;
}

std::string slurp(const char *name)
{
    std::string s;
    std::FILE *f = std::fopen(name, "rb");
    int c;
    while (f && (c = std::fgetc(f)) != EOF) s += static_cast<char>(c);
    if (f) std::fclose(f);
    return s;
}

std::string last_event;
void keep(std::string_view line) { last_event = line; }

} // namespace

int main()
{
    /* SHA-256 against FIPS 180-4's examples */
    CHECK_STR(sha_of("", 0).c_str(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_STR(sha_of("abc", 3).c_str(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    {
        const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        CHECK_STR(sha_of(m, std::strlen(m)).c_str(), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        std::string million(1000000, 'a');
        xpp::Sha256 c; /* in uneven pieces: the block boundary handling */
        for (size_t i = 0; i < million.size(); i += 777)
            c.update(million.data() + i, i + 777 <= million.size() ? 777 : million.size() - i);
        CHECK_STR(c.hex().c_str(), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }

    /* W121b: one basename suffix rule, including the bare-extension edge. */
    CHECK(xpp::files::has_extension("dir/model.ODEX", ".odex"));
    CHECK(!xpp::files::has_extension(".odex", ".odex"));
    CHECK(!xpp::files::has_extension("dir/.odex", ".odex"));
    CHECK(!xpp::files::has_extension("model.odex.bak", ".odex"));
    CHECK(!xpp::files::has_extension("model", ""));
    CHECK(xpp::odex::is_odex("model.ODEX"));
    CHECK(!xpp::odex::is_odex("dir/.odex"));
    CHECK(xpp::snapx::is_session_file("dir/model.SNAPX"));
    CHECK(!xpp::snapx::is_session_file(".snapx"));

    /* W130: every extension uses the same model base; no filename truncation. */
    for(const char *ext : {".snapx", ".set", ".pars", ".par", ".ic", ".ps", ".svg", ".gif", ".dat", ".csv", ".csv.gz", ".npz", ".tab", ".recx"}) {
        CHECK(xpp::files::output_name("folder/lecar.odex",ext)==std::string("lecar")+ext);
    }
    CHECK(xpp::files::output_name("lecar.ode",".csv","curves")=="lecar-curves.csv");
    CHECK(xpp::files::output_name("lecar.ode",".dat","eigen1")=="lecar-eigen1.dat");
    CHECK(xpp::files::output_name(std::string(100,'a')+".ode",".ps")==std::string(100,'a')+".ps");
    CHECK(xpp::files::output_name("folder/model",".dat")=="model.dat");

    CHECK(xpp::files::frame_name("lecar.gif",".gif",0)=="lecar.gif");
    CHECK(xpp::files::frame_name("lecar.gif",".gif",1)=="lecar-frame1.gif");
    CHECK(xpp::files::frame_name("long.model.gif",".gif",2)=="long.model-frame2.gif");

    /* names: base names only */
    CHECK(xpp::files::name_ok("lecar.set"));
    CHECK(xpp::files::name_ok("a b-2.ode"));
    CHECK(xpp::files::name_ok("donn\xc3\xa9" "es.dat")); /* UTF-8 */
    CHECK(xpp::files::name_ok("x"));
    const char *bad[] = {"", ".", "..", "../x", "a/b", "a\\b", "/etc/passwd", "C:x", "c:\\x", ".hidden", "..x",
                         "a..b", "x.set.", "x.set ", " x", "a\tb", "a\nb", "a\x7f", "a|b", "a*b", "a?b", "a<b",
                         "a\"b", "CON", "con.txt", "Nul", "com1.dat", "LPT9", "conin$"};
    for (const char *b : bad)
        if (xpp::files::name_ok(b)) CHECK_STR(b, "(refused)");
    CHECK(!xpp::files::name_ok(std::string_view()));
    CHECK(xpp::files::name_ok(std::string(255, 'n').c_str()));
    CHECK(!xpp::files::name_ok(std::string(256, 'n').c_str()));
    CHECK(xpp::files::name_ok("console.txt")); /* only the device names themselves */

    /* the ask's mode, for every file selector title in the core */
    const char *reads[] = {"Import XPPAUT set", "Load Auto", "Load data", "Load animation", "Load table",
                           "Read initial data", "Import Diagram", "Library:", "Select an ODE file", "Load session", "Play recording"};
    const char *writes[] = {"Save Auto", "Write data", "Write all info", "Write init data file",
                            "Write points", "Postscript", "SVG", "Save As", "Print postscript", "Print svg",
                            "Export graph data", "Save info", "Save nullclines", "Clone ODE file", "GIF plot",
                            "Save session", "Export CSV"};
    for (const char *t : reads) CHECK_STR(xpp::files::ask_mode(t), "read");
    for (const char *t : writes) CHECK_STR(xpp::files::ask_mode(t), "write");

    /* the folder: a fresh one of our own */
    char dir[512];
#ifdef _WIN32
    const char *tmp = std::getenv("TEMP");
    std::snprintf(dir, sizeof dir, "%s\\xpp-test-files-%d", tmp ? tmp : ".", _getpid());
    CHECK(_mkdir(dir) == 0);
#else
    std::snprintf(dir, sizeof dir, "/tmp/xpp-test-files-XXXXXX");
    CHECK(mkdtemp(dir) != nullptr);
#endif
    char here[1024];
    CHECK(getcwd(here, sizeof here) != nullptr);
    CHECK(chdir(dir) == 0);

    /* a put lands only at commit, under its name */
    xpp::files::Put *put;
    unsigned long long size = 0;
    std::string sha;
    const unsigned char bin[] = {0, 1, 2, 0xff, '\r', '\n', 0x80, 'x'};
    const std::string_view bin_bytes(reinterpret_cast<const char *>(bin), sizeof bin);
    CHECK(xpp::files::put_begin("a.bin", 100, put) == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, bin_bytes.substr(0, 5)) == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, bin_bytes.substr(5)) == XPP_FILES_OK);
    CHECK(!xpp::files::exists("a.bin")); /* only the hidden temp file so far */
    CHECK(xpp::files::put_commit(put, size, sha) == XPP_FILES_OK);
    CHECK(size == 8);
    CHECK(sha == sha_of(bin, 8));
    CHECK(slurp("a.bin") == std::string(reinterpret_cast<const char *>(bin), 8));
    CHECK_STR(folder().c_str(), "a.bin");

    /* Match classes, escapes and star retries without an input-sized stack. */
    for (const auto &[pattern, matches] : std::vector<std::pair<std::string, bool>>{
             {"a.bin", true}, {"a?bin", true}, {"[a-c].bin", true},
             {"[!b-c].bin", true}, {"[!a-c].bin", false}, {"a*in", true},
             {"*a*b*i*n", true}, {"*a*b*z", false}, {"a\\.bin", true},
             {"a[", false}, {"a\\", false}, {std::string(100000, '*'), true}}) {
        std::vector<std::string> dirs, files;
        CHECK(xpp::files::list_matching(pattern, ".", dirs, files));
        CHECK((files == std::vector<std::string>{"a.bin"}) == matches);
    }

    /* over the cap: refused, and abort leaves nothing */
    CHECK(xpp::files::put_begin("big.dat", 10, put) == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, "0123456789") == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, "x") == XPP_FILES_TOO_LARGE);
    xpp::files::put_abort(put);
    CHECK_STR(folder().c_str(), "a.bin");
    /* an aborted replace keeps the old file */
    CHECK(xpp::files::put_begin("a.bin", 100, put) == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, "new") == XPP_FILES_OK);
    xpp::files::put_abort(put);
    CHECK(slurp("a.bin").size() == 8);
    CHECK_STR(folder().c_str(), "a.bin");
    /* a committed one replaces it */
    CHECK(xpp::files::put_begin("a.bin", 100, put) == XPP_FILES_OK);
    CHECK(xpp::files::put_write(*put, "new") == XPP_FILES_OK);
    CHECK(xpp::files::put_commit(put, size, sha) == XPP_FILES_OK);
    CHECK(slurp("a.bin") == "new");
    CHECK(xpp::files::put_begin("../a.bin", 100, put) == XPP_FILES_BAD_NAME);
    CHECK(put == nullptr);

    /* reading */
    std::FILE *fp = nullptr;
    CHECK(xpp::files::open("a.bin", fp, size) == XPP_FILES_OK && size == 3);
    if (fp) std::fclose(fp);
    CHECK(xpp::files::open("none.dat", fp, size) == XPP_FILES_NOT_FOUND);
    CHECK(xpp::files::open("../a.bin", fp, size) == XPP_FILES_BAD_NAME);
#ifdef _WIN32
    CHECK(_mkdir("sub") == 0);
#else
    CHECK(mkdir("sub", 0755) == 0);
#endif
    CHECK(xpp::files::open("sub", fp, size) == XPP_FILES_REFUSED);
    CHECK(xpp::files::put_begin("sub", 100, put) == XPP_FILES_REFUSED);
#ifdef _WIN32
    /* Exercise the handle check directly, without a pathname precheck. */
    int fd = xpp::win32::open_plain("a.bin", false, true, size);
    CHECK(fd >= 0 && size == 3);
    if (fd >= 0) _close(fd);
    for (const char *name : {"sub", "NUL"}) {
        fd = xpp::win32::open_plain(name, false, true, size);
        CHECK(fd < 0 && errno == ELOOP);
        if (fd >= 0) _close(fd);
    }
    /* Junctions need no symlink privilege; only fixed test names reach cmd. */
    const intptr_t junction = _spawnlp(_P_WAIT, "cmd.exe", "cmd.exe", "/c",
                                     "mklink /J link.txt sub >NUL", nullptr);
    CHECK(junction == 0);
    if (junction == 0) {
        fd = xpp::win32::open_plain("link.txt", false, true, size);
        CHECK(fd < 0 && errno == ELOOP);
        if (fd >= 0) _close(fd);
        CHECK(xpp::files::open("link.txt", fp, size) == XPP_FILES_REFUSED && !fp);
        CHECK(xpp::files::create_new("link.txt", true) == nullptr);
        CHECK(_rmdir("link.txt") == 0);
    }
    const intptr_t symlink = _spawnlp(_P_WAIT, "cmd.exe", "cmd.exe", "/c",
                                     "mklink link.txt a.bin >symlink-result.txt 2>&1", nullptr);
    CHECK(symlink >= 0);
    if (symlink != 0) {
        std::printf("SKIP Windows file symlink: %s (junction tested)\n", slurp("symlink-result.txt").c_str());
    } else {
        fd = xpp::win32::open_plain("link.txt", false, true, size);
        CHECK(fd < 0 && errno == ELOOP);
        if (fd >= 0) _close(fd);
        CHECK(xpp::files::open("link.txt", fp, size) == XPP_FILES_REFUSED && !fp);
        CHECK(xpp::files::put_begin("link.txt", 100, put) == XPP_FILES_REFUSED);
        CHECK(xpp::files::create_new("link.txt", true) == nullptr);
        CHECK(std::filesystem::remove("link.txt"));
    }
    CHECK(std::remove("symlink-result.txt") == 0);
    fp = xpp::files::create_new("exclusive.txt", false);
    CHECK(fp != nullptr);
    if (fp) { CHECK(std::fputs("line\n", fp) >= 0); std::fclose(fp); }
    CHECK(xpp::files::create_new("exclusive.txt", true) == nullptr);
    CHECK(slurp("exclusive.txt") == "line\r\n");
    CHECK(std::remove("exclusive.txt") == 0);
#endif
#ifndef _WIN32
    /* a link is neither followed nor replaced */
    CHECK(symlink("/etc/hostname", "link.txt") == 0);
    CHECK(xpp::files::open("link.txt", fp, size) == XPP_FILES_REFUSED);
    CHECK(xpp::files::put_begin("link.txt", 100, put) == XPP_FILES_REFUSED);
#endif

    const std::string long_name(xpp::files::NAME_MAX_BYTES + 1, 'x');
    CHECK(!xpp::files::name_ok(long_name));
    const auto bad_name = xpp::files::command("get", ("\"" + long_name + "\"").c_str(), nullptr);
    CHECK(bad_name.error && bad_name.error->what == xpp::files::status_text(XPP_FILES_BAD_NAME));

    /* the listing: plain files only */
    std::string list = xpp::json::files_json(xpp::files::command("list", nullptr, nullptr));
    std::string want = "{\"files\":[{\"name\":\"a.bin\",\"size\":3,\"mtime\":";
    CHECK(list.starts_with(want));
    CHECK(list.find(sha_of("new", 3)) != std::string::npos);
    CHECK(list.find("sub") == std::string::npos && list.find("link") == std::string::npos);

    /* the protocol's file command */
    keep(xpp::json::file_event("put", xpp::files::command("put", "\"c.dat\"", "\"AAEC/w0KgHg=\""))); /* bin, base64 */
    CHECK(last_event.find("\"ok\":1") != std::string::npos);
    CHECK(slurp("c.dat") == std::string(reinterpret_cast<const char *>(bin), 8));
    keep(xpp::json::file_event("get", xpp::files::command("get", "\"c.dat\"", nullptr)));
    CHECK(last_event.find("\"data\":\"AAEC/w0KgHg=\"") != std::string::npos);
    CHECK(last_event.find(sha_of(bin, 8)) != std::string::npos);
    keep(xpp::json::file_event("put", xpp::files::command("put", "\"..\\/xpp-escape-probe\"", "\"AA==\""))); /* ../xpp-escape-probe */
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    keep(xpp::json::file_event("put", xpp::files::command("put", "\"a\\u0000b\"", "\"AA==\"")));
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    keep(xpp::json::file_event("put", xpp::files::command("put", "\"d.dat\"", "\"not base64!\"")));
    CHECK(last_event.find("not a base64") != std::string::npos);
    keep(xpp::json::file_event("get", xpp::files::command("get", "\"sub\"", nullptr)));
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    keep(xpp::json::file_event("list", xpp::files::command("list", nullptr, nullptr)));
    CHECK(last_event.find("\"name\":\"c.dat\"") != std::string::npos);
    std::string names = " " + folder() + " ";
    CHECK(names.find(" x ") == std::string::npos && names.find(" d.dat ") == std::string::npos);
    CHECK(names.find(".tmp-") == std::string::npos); /* no upload left its temp file */
    /* a name no other program leaves in the temp folder, unlike "x" */
    std::FILE *up = std::fopen("../xpp-escape-probe", "rb");
    CHECK(up == nullptr);
    if (up) std::fclose(up);

    /* the core's own files (W32b): copy, prepend (AUTO's "append"), move,
       remove, whole or not at all */
    CHECK(xpp::files::exists("a.bin"));
    CHECK(!xpp::files::exists("none.dat"));
    CHECK(xpp::files::exists("sub")); /* a folder too */
    xpp::files::copy("a.bin", "b.bin");
    CHECK(slurp("b.bin") == "new");
    xpp::files::copy("none.dat", "b.bin"); /* an unreadable source leaves the target */
    CHECK(slurp("b.bin") == "new");
    xpp::files::prepend("c.dat", "b.bin");
    CHECK(slurp("b.bin") == slurp("c.dat") + "new");
    xpp::files::prepend("a.bin", "e.bin"); /* no target yet: a copy */
    CHECK(slurp("e.bin") == "new");
    xpp::files::move("e.bin", "b.bin"); /* replaces */
    CHECK(slurp("b.bin") == "new" && !xpp::files::exists("e.bin"));
    CHECK(xpp::files::remove("b.bin") == 0 && !xpp::files::exists("b.bin"));
    CHECK(xpp::files::remove("b.bin") != 0);
    CHECK(xpp::files::dir_writable("."));
    CHECK(!xpp::files::dir_writable("no-such-folder"));
    CHECK(!xpp::files::dir_writable(""));
    std::FILE *nf = xpp::files::create_new("n.txt", false);
    CHECK(nf != nullptr);
    if (nf) std::fclose(nf);
    CHECK(xpp::files::create_new("n.txt", true) == nullptr); /* exists: refused */
    CHECK(xpp::files::remove("n.txt") == 0);
    CHECK(folder().find(".tmp-") == std::string::npos); /* no temp file left behind */

    /* the scratch folder: made, emptied and removed */
    std::string scratch = xpp::files::make_temp_dir();
    CHECK(!scratch.empty());
    if (!scratch.empty()) {
        CHECK(scratch.find("xppautoX-") != std::string::npos);
        CHECK(xpp::files::exists(scratch.c_str()));
        /* A sent recording can write its own plain outputs, but cannot
           write outside scratch or through a nested directory/link. */
        xpp::files::serve_reads([](const std::string &, std::string *) { return false; });
        std::string inside = scratch + "/fort.7";
        std::FILE *f7 = xpp::files::open_stream(inside.c_str(), "w");
        CHECK(f7 != nullptr);
        if (f7) std::fclose(f7);
        const std::string current = xpp::files::working_dir();
        CHECK(chdir(scratch.c_str()) == 0);
        CHECK(!xpp::files::is_scratch("fort.7"));
        std::FILE *relative = xpp::files::open_stream("fort.7", "r");
        CHECK(relative == nullptr); /* a relative read must consult the recording */
        if (relative) std::fclose(relative);
        CHECK(chdir(current.c_str()) == 0);
        CHECK(xpp::files::open_stream("n.txt", "w") == nullptr);
        CHECK(!xpp::files::exists("n.txt"));
        const std::string nested = scratch + "/nested";
#ifdef _WIN32
        CHECK(_mkdir(nested.c_str()) == 0);
#else
        CHECK(mkdir(nested.c_str(), 0700) == 0);
#endif
        const std::string outside = nested + "/outside.txt";
        CHECK(xpp::files::open_stream(outside, "w") == nullptr);
        CHECK(!xpp::files::exists(outside));
        xpp::files::serve_reads(nullptr);
        CHECK(rmdir(nested.c_str()) == 0);
        xpp::files::remove_temp_dir(scratch.c_str());
        CHECK(!xpp::files::exists(scratch.c_str()));
    }
    xpp::files::remove_temp_dir(""); /* does nothing */
#ifdef _WIN32
    std::printf("SKIP scratch through symlink: Windows symlink creation requires privilege; Linux/macOS exercise it\n");
#else
    /* macOS /var -> /private/var: TMPDIR's spelling and getcwd differ.
       Resolve only ancestors of scratch, never links within it. */
    {
        const char *previous = std::getenv("TMPDIR");
        const std::string saved_tmp = previous ? previous : "";
        const std::string real_base = std::filesystem::canonical(dir).string();
        const std::string alias = real_base + "-link";
        CHECK(symlink(real_base.c_str(), alias.c_str()) == 0);
        CHECK(setenv("TMPDIR", alias.c_str(), 1) == 0);
        const std::string real_scratch = xpp::files::make_temp_dir();
        CHECK(!real_scratch.empty());
        const std::string linked_scratch = alias + "/" + xpp::files::split_path(real_scratch).second;
        CHECK(real_scratch == std::filesystem::canonical(linked_scratch).string());
        CHECK(xpp::files::is_scratch(real_scratch, true));
        CHECK(xpp::files::is_scratch(linked_scratch, true));
        xpp::files::serve_reads([](const std::string &, std::string *) { return false; });
        for (const std::string &root : {real_scratch, linked_scratch}) {
            CHECK(xpp::files::write_path_ok(root + "/run1.dat"));
            std::FILE *out = xpp::files::open_stream(root + "/run1.dat", "w");
            CHECK(out != nullptr);
            if (out) std::fclose(out);
        }
        const std::string escape = real_scratch + "/escape";
        CHECK(symlink(real_base.c_str(), escape.c_str()) == 0);
        CHECK(!xpp::files::write_path_ok(escape + "/outside.dat"));
        CHECK(!xpp::files::is_scratch(escape + "/outside.dat"));
        CHECK(!xpp::files::write_path_ok(real_scratch + "/../outside.dat"));
        CHECK(unlink(escape.c_str()) == 0);
        xpp::files::serve_reads(nullptr);
        xpp::files::remove_temp_dir(real_scratch);
        CHECK(unlink(alias.c_str()) == 0);
        if (previous) CHECK(setenv("TMPDIR", saved_tmp.c_str(), 1) == 0);
        else CHECK(unsetenv("TMPDIR") == 0);
    }
#endif
#ifndef _WIN32
    /* a killed run's folder is swept, a live one's is kept (issue #32) */
    const char *old_tmpdir = std::getenv("TMPDIR");
    std::string saved = old_tmpdir ? old_tmpdir : "";
    CHECK(setenv("TMPDIR", dir, 1) == 0);
    std::string dead = std::string(dir) + "/xppautoX-999999999-0";
    std::string live = std::string(dir) + "/xppautoX-" + std::to_string(getpid()) + "-5";
    CHECK(mkdir(dead.c_str(), 0700) == 0 && mkdir(live.c_str(), 0700) == 0);
    std::FILE *df = std::fopen((dead + "/fort.9").c_str(), "w");
    if (df) std::fclose(df);
    xpp::files::cleanup_stale_temp_dirs();
    CHECK(!xpp::files::exists(dead.c_str()));
    CHECK(xpp::files::exists(live.c_str()));
    CHECK(rmdir(live.c_str()) == 0);
    if (old_tmpdir) setenv("TMPDIR", saved.c_str(), 1);
    else unsetenv("TMPDIR");
#endif

#ifdef _WIN32
    constexpr unsigned long SYSTEM_PID = 4; /* Windows' System process stays alive, and its query may be denied. */
    CHECK(!xpp::win32::process_gone(SYSTEM_PID));
    CHECK(!xpp::win32::process_gone(0)); /* invalid reserved PID cannot prove an exit */
#endif

    /* clean up */
    std::remove("a.bin");
    std::remove("c.dat");
    std::remove("link.txt");
    CHECK(rmdir("sub") == 0);
    CHECK(chdir(here) == 0);
    CHECK(rmdir(dir) == 0);
    TEST_REPORT("test_files");
}
