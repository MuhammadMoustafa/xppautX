/* xpp_files: what the page may read and write in the model's folder
   (docs/ui-v2.md section 4). A name that slips through reaches a file
   outside the folder; an upload cut short or over the cap must leave
   nothing behind. tools/webcheck.py checks the same through HTTP, and
   tools/servercheck.py through the protocol's `file` command. */
#include "xpptest.h"
#include "xpp_files.h"
#include "xpp_mem.h"
#include "xpp_sha256.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::string sha_of(const void *p, size_t n)
{
    XppSha256 c;
    char hex[65];
    xpp_sha256_init(&c);
    xpp_sha256_update(&c, p, n);
    xpp_sha256_hex(&c, hex);
    return hex;
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
void keep(const char *line, size_t n) { last_event.assign(line, n); }

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
        XppSha256 c; /* in uneven pieces: the block boundary handling */
        char hex[65];
        xpp_sha256_init(&c);
        for (size_t i = 0; i < million.size(); i += 777)
            xpp_sha256_update(&c, million.data() + i, i + 777 <= million.size() ? 777 : million.size() - i);
        xpp_sha256_hex(&c, hex);
        CHECK_STR(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }

    /* names: base names only */
    CHECK(xpp_files_name_ok("lecar.set"));
    CHECK(xpp_files_name_ok("a b-2.ode"));
    CHECK(xpp_files_name_ok("donn\xc3\xa9" "es.dat")); /* UTF-8 */
    CHECK(xpp_files_name_ok("x"));
    const char *bad[] = {"", ".", "..", "../x", "a/b", "a\\b", "/etc/passwd", "C:x", "c:\\x", ".hidden", "..x",
                         "a..b", "x.set.", "x.set ", " x", "a\tb", "a\nb", "a\x7f", "a|b", "a*b", "a?b", "a<b",
                         "a\"b", "CON", "con.txt", "Nul", "com1.dat", "LPT9", "conin$"};
    for (const char *b : bad)
        if (xpp_files_name_ok(b)) CHECK_STR(b, "(refused)");
    CHECK(!xpp_files_name_ok(nullptr));
    CHECK(xpp_files_name_ok(std::string(255, 'n').c_str()));
    CHECK(!xpp_files_name_ok(std::string(256, 'n').c_str()));
    CHECK(xpp_files_name_ok("console.txt")); /* only the device names themselves */

    /* the ask's mode, for every file selector title in the core */
    const char *reads[] = {"Load SET File", "Load Auto", "Load data", "Load animation", "Load table",
                           "Read initial data", "Import Diagram", "Library:", "Select an ODE file", "Load session"};
    const char *writes[] = {"Save SET File", "Save Auto", "Write data", "Write all info", "Write init data file",
                            "Write points", "Postscript", "SVG", "Save As", "Print postscript", "Print svg",
                            "Export graph data", "Save info", "Save nullclines", "Clone ODE file", "GIF plot",
                            "Save session"};
    for (const char *t : reads) CHECK_STR(xpp_files_ask_mode(t), "read");
    for (const char *t : writes) CHECK_STR(xpp_files_ask_mode(t), "write");

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
    XppFilePut *put;
    unsigned long long size = 0;
    char sha[65];
    const unsigned char bin[] = {0, 1, 2, 0xff, '\r', '\n', 0x80, 'x'};
    CHECK(xpp_files_put_begin("a.bin", 100, &put) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, bin, 5) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, bin + 5, 3) == XPP_FILES_OK);
    CHECK(folder().find("a.bin") == std::string::npos); /* only the hidden part file so far */
    CHECK(xpp_files_put_commit(put, &size, sha) == XPP_FILES_OK);
    CHECK(size == 8);
    CHECK_STR(sha, sha_of(bin, 8).c_str());
    CHECK(slurp("a.bin") == std::string(reinterpret_cast<const char *>(bin), 8));
    CHECK_STR(folder().c_str(), "a.bin");

    /* over the cap: refused, and abort leaves nothing */
    CHECK(xpp_files_put_begin("big.dat", 10, &put) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, "0123456789", 10) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, "x", 1) == XPP_FILES_TOO_LARGE);
    xpp_files_put_abort(put);
    CHECK_STR(folder().c_str(), "a.bin");
    /* an aborted replace keeps the old file */
    CHECK(xpp_files_put_begin("a.bin", 100, &put) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, "new", 3) == XPP_FILES_OK);
    xpp_files_put_abort(put);
    CHECK(slurp("a.bin").size() == 8);
    CHECK_STR(folder().c_str(), "a.bin");
    /* a committed one replaces it */
    CHECK(xpp_files_put_begin("a.bin", 100, &put) == XPP_FILES_OK);
    CHECK(xpp_files_put_write(put, "new", 3) == XPP_FILES_OK);
    CHECK(xpp_files_put_commit(put, &size, sha) == XPP_FILES_OK);
    CHECK(slurp("a.bin") == "new");
    CHECK(xpp_files_put_begin("../a.bin", 100, &put) == XPP_FILES_BAD_NAME);
    CHECK(put == nullptr);

    /* reading */
    std::FILE *fp = nullptr;
    CHECK(xpp_files_open("a.bin", &fp, &size) == XPP_FILES_OK && size == 3);
    if (fp) std::fclose(fp);
    CHECK(xpp_files_open("none.dat", &fp, &size) == XPP_FILES_NOT_FOUND);
    CHECK(xpp_files_open("../a.bin", &fp, &size) == XPP_FILES_BAD_NAME);
#ifdef _WIN32
    CHECK(_mkdir("sub") == 0);
#else
    CHECK(mkdir("sub", 0755) == 0);
#endif
    CHECK(xpp_files_open("sub", &fp, &size) == XPP_FILES_REFUSED);
    CHECK(xpp_files_put_begin("sub", 100, &put) == XPP_FILES_REFUSED);
#ifndef _WIN32
    /* a link is neither followed nor replaced */
    CHECK(symlink("/etc/hostname", "link.txt") == 0);
    CHECK(xpp_files_open("link.txt", &fp, &size) == XPP_FILES_REFUSED);
    CHECK(xpp_files_put_begin("link.txt", 100, &put) == XPP_FILES_REFUSED);
#endif

    /* the listing: plain files only */
    size_t len;
    char *list = xpp_files_list_json(&len);
    std::string want = "{\"files\":[{\"name\":\"a.bin\",\"size\":3,\"mtime\":";
    CHECK(std::strncmp(list, want.c_str(), want.size()) == 0);
    CHECK(std::strstr(list, sha_of("new", 3).c_str()) != nullptr);
    CHECK(std::strstr(list, "sub") == nullptr && std::strstr(list, "link") == nullptr);
    xpp_free(list);

    /* the protocol's file command */
    xpp_files_command("put", "\"c.dat\"", "\"AAEC/w0KgHg=\"", keep); /* bin, base64 */
    CHECK(last_event.find("\"ok\":1") != std::string::npos);
    CHECK(slurp("c.dat") == std::string(reinterpret_cast<const char *>(bin), 8));
    xpp_files_command("get", "\"c.dat\"", nullptr, keep);
    CHECK(last_event.find("\"data\":\"AAEC/w0KgHg=\"") != std::string::npos);
    CHECK(last_event.find(sha_of(bin, 8)) != std::string::npos);
    xpp_files_command("put", "\"..\\/xpp-escape-probe\"", "\"AA==\"", keep); /* ../xpp-escape-probe */
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    xpp_files_command("put", "\"a\\u0000b\"", "\"AA==\"", keep);
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    xpp_files_command("put", "\"d.dat\"", "\"not base64!\"", keep);
    CHECK(last_event.find("not a base64") != std::string::npos);
    xpp_files_command("get", "\"sub\"", nullptr, keep);
    CHECK(last_event.find("\"ok\":0") != std::string::npos);
    xpp_files_command("list", nullptr, nullptr, keep);
    CHECK(last_event.find("\"name\":\"c.dat\"") != std::string::npos);
    std::string names = " " + folder() + " ";
    CHECK(names.find(" x ") == std::string::npos && names.find(" d.dat ") == std::string::npos);
    CHECK(names.find(".part") == std::string::npos);
    /* a name no other program leaves in the temp folder, unlike "x" */
    std::FILE *up = std::fopen("../xpp-escape-probe", "rb");
    CHECK(up == nullptr);
    if (up) std::fclose(up);

    /* clean up */
    std::remove("a.bin");
    std::remove("c.dat");
    std::remove("link.txt");
    CHECK(rmdir("sub") == 0);
    CHECK(chdir(here) == 0);
    CHECK(rmdir(dir) == 0);
    TEST_REPORT("test_files");
}
