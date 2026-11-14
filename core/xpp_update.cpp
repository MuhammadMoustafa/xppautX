/* xpp_update.h */
#include "xpp_update.h"

#include <algorithm>
#include <expected>
#include <utility>

#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_sha256.h"

namespace xpp::update {

namespace {

constexpr std::string_view WHERE = "update";
constexpr std::size_t DIGEST_HEX_DIGITS = 64; /* SHA-256 */

bool plain_url_text(std::string_view url)
{
    return std::ranges::none_of(url, [](char c) { return static_cast<unsigned char>(c) <= ' ' || c == 0x7f || c == '\\' || c == '#'; });
}

/* the host of an https address with no user name or port, or nothing */
std::optional<std::string_view> https_host(std::string_view url)
{
    constexpr std::string_view SCHEME = "https://";
    if (!url.starts_with(SCHEME)) return std::nullopt;
    const std::string_view rest = url.substr(SCHEME.size());
    const std::string_view host = rest.substr(0, rest.find_first_of("/?"));
    if (host.empty() || host.find_first_of("@:[]") != std::string_view::npos || host.size() == rest.size()) return std::nullopt;
    return host;
}

/* a path segment of a release: letters, digits and . - _ */
bool segment_ok(std::string_view segment)
{
    return !segment.empty() && segment != "." && segment.find("..") == std::string_view::npos
           && std::ranges::all_of(segment, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'; });
}

std::optional<std::string_view> digest_hex(std::string_view digest)
{
    if (!digest.starts_with(DIGEST_PREFIX)) return std::nullopt;
    const std::string_view hex = digest.substr(DIGEST_PREFIX.size());
    if (hex.size() != DIGEST_HEX_DIGITS || !std::ranges::all_of(hex, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) return std::nullopt;
    return hex;
}

} // namespace

std::optional<std::string> url_problem(std::string_view url, bool first)
{
    if (url.size() > URL_LIMIT || !plain_url_text(url)) return "the address is too long or has a character no release address has";
    if (first) {
        if (!url.starts_with(RELEASE_DOWNLOAD_PREFIX)) return xpp::format("\"{}\" is not this project's release download address", url);
        const std::string_view tail = url.substr(RELEASE_DOWNLOAD_PREFIX.size()); /* <tag>/<file>, nothing else */
        const std::size_t slash = tail.find('/');
        if (slash == std::string_view::npos || !segment_ok(tail.substr(0, slash)) || !segment_ok(tail.substr(slash + 1)))
            return xpp::format("\"{}\" is not a release file address", url);
        return std::nullopt;
    }
    const std::optional<std::string_view> host = https_host(url);
    if (!host) return xpp::format("\"{}\" is not an https address with a plain host", url);
    if (std::ranges::find(FILE_HOSTS, *host) == FILE_HOSTS.end())
        return xpp::format("{} is not one of GitHub's file hosts ({} and {})", *host, FILE_HOSTS[0], FILE_HOSTS[1]);
    return std::nullopt;
}

std::optional<std::string> content_problem(std::string_view name, unsigned long long size, std::string_view digest)
{
    if (!files::name_ok(name)) return xpp::format("\"{}\" is not a plain file name", name);
    if (size == 0 || size > MAX_DOWNLOAD_BYTES)
        return xpp::format("the release list gives a size of {} bytes: a release file is from 1 to {}", size, MAX_DOWNLOAD_BYTES);
    if (!digest_hex(digest)) return xpp::format("\"{}\" is not a sha256 digest (sha256: and 64 lowercase hex digits)", digest);
    return std::nullopt;
}

std::optional<std::string> file_problem(const ReleaseFile &file)
{
    if (const auto why = content_problem(file.name, file.size, file.digest)) return why;
    return url_problem(file.url, true);
}

std::string_view digest_value(std::string_view digest) { return digest.substr(DIGEST_PREFIX.size()); }

Result<Downloaded> download(const ReleaseFile &file, std::string_view folder, const Transport &transport, const Progress &progress)
{
    if (const auto why = file_problem(file)) return fail(std::string(WHERE), "the release file is refused: " + *why, Place{file.url});
    const std::string path = std::string(folder) + "/" + file.name;
    Writer out = Writer::binary(path);
    if (!out) return fail(std::string(WHERE), "the file cannot be written", Place{path});
    Sha256 hash;
    unsigned long long got = 0;
    std::optional<std::string> refused; /* why the body was refused, from inside the chunk callback */
    bool stopped = false;
    std::string url = file.url;
    for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
        if (const auto why = url_problem(url, hop == 0)) return fail(std::string(WHERE), "the address is refused: " + *why, Place{file.url});
        const bool from_a_file_host = hop > 0; /* the first address is github.com's, which only redirects */
        const auto chunk = [&](std::string_view data) {
            if (!from_a_file_host) refused = "github.com sent a file instead of redirecting to a GitHub file host";
            else if (data.size() > file.size - got) refused = xpp::format("the file is longer than the release list's {} bytes", file.size);
            else if (!data.empty() && !out.write(data)) refused = "the file could not be written";
            if (refused) return false;
            hash.update(data.data(), data.size());
            got += data.size();
            stopped = !progress(got, file.size);
            return !stopped;
        };
        const Result<https::Reply> reply = transport(url, chunk);
        if (refused) return fail(std::string(WHERE), *refused, Place{file.url});
        if (stopped) return Downloaded{{}, true, got}; /* the Writer's end removes the partial file */
        if (!reply) return std::unexpected(reply.error());
        if (reply->status >= 300 && reply->status < 400 && !reply->location.empty()) {
            url = reply->location;
            continue;
        }
        if (reply->status != 200) return fail(std::string(WHERE), xpp::format("GitHub answered HTTP {}", reply->status), Place{file.url});
        if (got != file.size) return fail(std::string(WHERE), xpp::format("the file is {} bytes, the release list says {}", got, file.size), Place{file.url});
        const std::string have = hash.hex();
        const std::string_view want = *digest_hex(file.digest);
        if (have != want) return fail(std::string(WHERE), xpp::format("the file's sha256 is {}, the release list says {}: it is not the release's file", have, want), Place{file.url});
        if (const Result<> kept = out.commit(); !kept) return std::unexpected(kept.error());
        return Downloaded{path, false, got};
    }
    return fail(std::string(WHERE), xpp::format("more than {} redirects", MAX_REDIRECTS), Place{file.url});
}

} // namespace xpp::update
