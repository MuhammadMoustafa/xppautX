/* Downloading a release file (W622, core/xpp_update.h): which addresses are asked for, which release files are taken, and that
   what comes back is the file the release list named: a body from the wrong host, one longer or shorter than its size, one
   whose sha256 differs, a redirect to a host that is not GitHub's, an endless redirect, a stop; nothing is left behind by a
   refusal. The transport is a stub: no network. */
#include "xpptest.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_sha256.h"
#include "xpp_update.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {

using xpp::update::ReleaseFile;

const std::string FIRST = "https://github.com/MuhammadMoustafa/xppautX/releases/download/v0.1.0/xppautX-v0.1.0-linux-x64.tar.gz";
const std::string FILE_HOST = "https://release-assets.githubusercontent.com/github-production-release-asset/1/2?sp=r&sig=abc";
const std::string BODY = "the release file's bytes, as the release list names them";

std::string sha(const std::string &bytes)
{
    xpp::Sha256 h;
    h.update(bytes.data(), bytes.size());
    return "sha256:" + h.hex();
}

ReleaseFile good() { return ReleaseFile{FIRST, "xppautX-v0.1.0-linux-x64.tar.gz", BODY.size(), sha(BODY)}; }

/* a stub server: each address answers with a status, a Location and a body sent in pieces of `piece` bytes */
struct Answer {
    int status = 200;
    std::string location, body;
};
struct Server {
    std::map<std::string, Answer> answers;
    std::vector<std::string> asked;
    std::size_t piece = 7;
    xpp::update::Transport transport()
    {
        return [this](std::string_view url, const xpp::https::Chunk &chunk) -> xpp::Result<xpp::https::Reply> {
            asked.emplace_back(url);
            const auto it = answers.find(std::string(url));
            if (it == answers.end()) return xpp::fail("test", "no answer for " + std::string(url));
            if (it->second.status == 200)
                for (std::size_t at = 0; at < it->second.body.size(); at += piece)
                    if (!chunk(std::string_view(it->second.body).substr(at, piece))) break;
            return xpp::https::Reply{it->second.status, it->second.location};
        };
    }
};

Server normal(const std::string &body = BODY)
{
    Server s;
    s.answers[FIRST] = {302, FILE_HOST, ""};
    s.answers[FILE_HOST] = {200, "", body};
    return s;
}

bool progress_always(unsigned long long, unsigned long long) { return true; }

void check_addresses()
{
    using xpp::update::url_problem;
    CHECK(!url_problem(FIRST, true));
    for (const char *bad : {"http://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a.zip", "https://github.com/other/xppautX/releases/download/v1/a.zip",
                            "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/../a.zip", "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a.zip?x=1",
                            "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a/b.zip", "https://github.com/MuhammadMoustafa/xppautX/releases/download//a.zip",
                            "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a.zip#x", "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a b.zip",
                            "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a%2f.zip", "https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a\\b.zip",
                            "https://github.com@evil.example/MuhammadMoustafa/xppautX/releases/download/v1/a.zip", "file:///etc/passwd", ""})
        CHECK(url_problem(bad, true));
    CHECK(url_problem(FIRST + std::string(5000, 'a'), true));
    CHECK(!url_problem(FILE_HOST, false));
    CHECK(!url_problem("https://objects.githubusercontent.com/a/b", false));
    for (const char *bad : {"https://github.com/MuhammadMoustafa/xppautX/releases/download/v1/a.zip", "https://evil.example/a", "http://release-assets.githubusercontent.com/a",
                            "https://release-assets.githubusercontent.com.evil.example/a", "https://release-assets.githubusercontent.com@evil.example/a",
                            "https://evil.example@release-assets.githubusercontent.com/a", "https://release-assets.githubusercontent.com:8443/a", "https://release-assets.githubusercontent.com",
                            "https://RELEASE-ASSETS.githubusercontent.com/a", "https://127.0.0.1/a", "https://[::1]/a", "https://release-assets.githubusercontent.com/a b",
                            "https://release-assets.githubusercontent.com/a#f", "https://release-assets.githubusercontent.com\\@evil.example/a", "ftp://objects.githubusercontent.com/a"})
        CHECK(url_problem(bad, false));
}

void check_files()
{
    using xpp::update::file_problem;
    CHECK(!file_problem(good()));
    ReleaseFile f = good();
    for (const char *name : {"", "../x.zip", "a/b.zip", ".hidden", "a\\b.zip", "con", "a:b"}) {
        f = good();
        f.name = name;
        CHECK(file_problem(f));
    }
    for (unsigned long long size : {0ull, xpp::update::MAX_DOWNLOAD_BYTES + 1}) {
        f = good();
        f.size = size;
        CHECK(file_problem(f));
    }
    f = good();
    f.size = xpp::update::MAX_DOWNLOAD_BYTES;
    CHECK(!file_problem(f));
    for (const std::string &digest : {std::string(), std::string("sha256:"), sha(BODY).substr(7), "sha1:" + sha(BODY).substr(7), "SHA256:" + sha(BODY).substr(7),
                                      sha(BODY).substr(0, 20), "sha256:" + std::string(64, 'G'), sha(BODY) + "0", "sha256:" + std::string(64, 'A')}) {
        f = good();
        f.digest = digest;
        CHECK(file_problem(f));
    }
}

bool nothing_left(const xpp::TempDir &dir) { return std::filesystem::is_empty(dir.path()); }

void check_download()
{
    const xpp::TempDir dir;
    CHECK(!dir.path().empty());
    /* the file the release list names: through GitHub's redirect to its file host, in pieces */
    Server s = normal();
    unsigned long long last = 0, of = 0;
    auto done = xpp::update::download(good(), dir.path(), s.transport(), [&](unsigned long long got, unsigned long long all) { last = got; of = all; return true; });
    CHECK(done && !done->stopped && done->bytes == BODY.size() && last == BODY.size() && of == BODY.size());
    std::string read;
    CHECK(done && xpp::read_bytes(done->path, read) && read == BODY && done->path == dir.path() + "/" + good().name);
    CHECK(s.asked.size() == 2 && s.asked[0] == FIRST && s.asked[1] == FILE_HOST);
    std::filesystem::remove(done->path);

    const auto refused = [&](const char *what, const ReleaseFile &file, Server &server, const char *why, xpp::update::Progress progress = progress_always) {
        const auto r = xpp::update::download(file, dir.path(), server.transport(), progress);
        const bool ok = !r && r.error().what.find(why) != std::string::npos && nothing_left(dir);
        if (!ok) printf("not refused as wanted: %s: %s\n", what, r ? "downloaded" : r.error().what.c_str());
        CHECK(ok);
    };
    /* a body longer than the release list's size is cut off the moment it passes it; one shorter, or of other bytes, is not the file */
    Server longer = normal(BODY + "extra");
    refused("a longer body", good(), longer, "longer than the release list's");
    Server shorter = normal(BODY.substr(0, BODY.size() - 1));
    refused("a shorter body", good(), shorter, "the release list says");
    std::string other = BODY;
    other[3] = 'X';
    Server changed = normal(other);
    refused("a changed byte", good(), changed, "sha256");
    ReleaseFile lie = good();
    lie.size = BODY.size() + 100;
    Server fine = normal();
    refused("a size lie", lie, fine, "the release list says");
    /* the wrong place */
    Server direct;
    direct.answers[FIRST] = {200, "", BODY};
    refused("github.com serving the body itself", good(), direct, "instead of redirecting");
    Server evil;
    evil.answers[FIRST] = {302, "https://evil.example/file", ""};
    refused("a redirect to another host", good(), evil, "not one of GitHub's file hosts");
    CHECK(evil.asked.size() == 1); /* never asked */
    Server relative;
    relative.answers[FIRST] = {302, "/elsewhere", ""};
    refused("a relative redirect", good(), relative, "https address");
    Server loop = normal();
    loop.answers[FILE_HOST] = {302, FILE_HOST, ""};
    refused("a redirect loop", good(), loop, "redirects");
    Server missing;
    missing.answers[FIRST] = {404, "", "not found"};
    refused("a missing file", good(), missing, "HTTP 404");
    Server broken;
    broken.answers[FIRST] = {302, FILE_HOST, ""};
    refused("a transport that fails", good(), broken, "no answer");
    ReleaseFile bad = good();
    bad.name = "../escape";
    Server unused = normal();
    refused("a name that leaves the folder", bad, unused, "plain file name");
    CHECK(unused.asked.empty());
    /* a stop leaves nothing and is not an error */
    Server stopping = normal();
    const auto stopped = xpp::update::download(good(), dir.path(), stopping.transport(), [](unsigned long long got, unsigned long long) { return got < 10; });
    CHECK(stopped && stopped->stopped && stopped->path.empty() && stopped->bytes >= 10 && nothing_left(dir));
}

} // namespace

int main()
{
    check_addresses();
    check_files();
    check_download();
    TEST_REPORT("update");
}
