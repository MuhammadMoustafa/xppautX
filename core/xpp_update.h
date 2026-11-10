#pragma once
/* Downloading a release file and proving it is the one the release list named (W622, #693). This crosses a trust
   boundary (a downloaded program is run), so every part is checked here and nowhere else:

   - the address: the first request goes only to the project's own release download path on github.com; GitHub
     answers it with a redirect to its file hosts (FILE_HOSTS), the only hosts a body is ever taken from, over
     https, port 443, no user name, at most MAX_REDIRECTS hops, each address checked before it is asked for;
   - the size: the release list's `size`, never more bytes than it names (nor more than MAX_DOWNLOAD_BYTES), and
     exactly that many by the end;
   - the content: the release list's sha256 `digest`, compared once the whole file is in;
   - the place: a folder xppautX made (the caller's TempDir), the file named by a plain base name (files::name_ok)
     and written temp-then-rename, so a file that is there is whole and checked.
   What GitHub's release list proves is that GitHub served it (signatures are W624). The system's HTTPS client
   (xpp_https.h) is the transport, which a test replaces. C++ only. */
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "xpp_error.h"
#include "xpp_https.h"

namespace xpp::update {

inline constexpr std::string_view RELEASE_TAG_PREFIX = "https://github.com/MuhammadMoustafa/xppautX/releases/tag/";
inline constexpr std::string_view RELEASE_DOWNLOAD_PREFIX = "https://github.com/MuhammadMoustafa/xppautX/releases/download/";
/* GitHub's release file hosts: where a release file's bytes come from */
inline constexpr std::array<std::string_view, 2> FILE_HOSTS = {"release-assets.githubusercontent.com", "objects.githubusercontent.com"};
/* a signed redirect address is long (a few hundred characters of token); no address needs more */
inline constexpr std::size_t URL_LIMIT = 4096;
/* github.com redirects to a file host, which may redirect once more */
inline constexpr int MAX_REDIRECTS = 3;
/* the largest release file taken: what the archive reader accepts (xpp_zip.h) */
inline constexpr unsigned long long MAX_DOWNLOAD_BYTES = 512ull * 1024 * 1024;
inline constexpr std::string_view DIGEST_PREFIX = "sha256:";

/* a release file as the release list gives it: its `browser_download_url`, `name`, `size` and `digest` ("sha256:" and
   64 hex digits) */
struct ReleaseFile {
    std::string url, name;
    unsigned long long size = 0;
    std::string digest;
};

/* why url cannot be asked for, or nothing: `first` is the address the release list named, which must be this project's
   release download path; any later one must be a file host */
std::optional<std::string> url_problem(std::string_view url, bool first);
/* why this file cannot be downloaded (a bad name, size, digest or address), or nothing */
std::optional<std::string> file_problem(const ReleaseFile &file);

/* the transport: one request without redirects (xpp::https::get) */
using Transport = std::function<Result<https::Reply>(std::string_view url, const https::Chunk &chunk)>;
/* bytes so far and the whole; false stops the download */
using Progress = std::function<bool(unsigned long long got, unsigned long long of)>;

struct Downloaded {
    /* the checked file, "" when the download was stopped */
    std::string path;
    bool stopped = false;
    unsigned long long bytes = 0;
};

/* downloads `file` into `folder` and checks it; an error says what failed (the address, the host, the size, the digest,
   the connection) and leaves nothing in the folder */
Result<Downloaded> download(const ReleaseFile &file, std::string_view folder, const Transport &transport, const Progress &progress);

/* the checked file waiting to be installed: the one the last successful download_staged left (a newer one replaces it), "" when
   none. It lives in a folder of ours (files::make_temp_dir) that is removed when it is replaced and when the program ends, so
   it outlives a model switch and nothing else. */
std::string staged_file();
/* download() into a fresh folder of ours; success makes the file the staged one, any failure or a stop leaves the staged one as it was
   and removes the new folder */
Result<Downloaded> download_staged(const ReleaseFile &file, const Transport &transport, const Progress &progress);

} // namespace xpp::update
