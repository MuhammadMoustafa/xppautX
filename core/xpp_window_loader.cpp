/* xpp_window.h on Linux (W13e): the window is a library of its own,
   libxppwindow.so (xpp_window.cpp and third_party/webview, linked against
   GTK 3 and WebKitGTK 4.1), embedded in xppautX as bytes
   (build/.../window_lib.c, tools/embed_bytes.c) and loaded from memory
   only when the window opens: memfd_create, then dlopen of
   /proc/self/fd/N. xppautX itself links neither library, so one binary
   starts on every Linux: -silent, --server and --browser never load it,
   and without WebKitGTK the window mode says what to install
   (xpp_window_hint.h) and uses the browser.

   What the library calls in the core comes in an XppWindowHost table
   (xpp_window_plugin.h), not through xppautX's own symbols: it links with
   -z defs, so it has no reference the loader does not hand it.

   Test hook: XPP_WINDOW_FAIL_LOAD=1 makes the load fail as if WebKitGTK
   were missing (tools/modecheck.sh), after writing the library out as
   usual; beside XPP_MEM_FAIL_AT (xpp_mem.h). */
#include "xpp_window.h"
#include "xpp_window_plugin.h"
#include "xpp_window_hint.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_io.h"
#include "xpp_log.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MFD_EXEC
#define MFD_EXEC 0x0010U /* linux/memfd.h, Linux 6.3; glibc 2.35 lacks it */
#endif

/* build/.../window_lib.c: libxppwindow.so's bytes */
extern "C" const unsigned char xpp_window_lib[];
extern "C" const unsigned long xpp_window_lib_len;

namespace {

const XppWindowHost host = {XPP_WINDOW_HOST_VERSION, xpp_http_url, xpp_http_release, xpp_http_said_bye,
                            xpp_inbox_push, xpp_log};
XppWindowApi api; /* the library's, once it is loaded */
bool loaded;

/* what dlerror says for a library that is not installed */
constexpr std::string_view SIMULATED_MISSING = "libwebkit2gtk-4.1.so.0: cannot open shared object file: No such file or directory";

bool write_all(int fd, const unsigned char *p, size_t n)
{
    while (n > 0) {
        ssize_t k = write(fd, p, n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return false;
        p += k;
        n -= static_cast<size_t>(k);
    }
    return true;
}

/* The library, dlopen'ed from a memfd (nothing on disk) when from_memory
   and the kernel gives one, else from a temp file removed at once; on
   failure NULL with what went wrong in err. */
void *open_library(std::string &err, bool from_memory)
{
    std::string path, temp;
    /* executable asked for by name: with vm.memfd_noexec=1 (Linux 6.3+) a
       memfd is otherwise sealed non-executable and dlopen cannot map it;
       an older kernel refuses the unknown flag (then without it), and
       vm.memfd_noexec=2 refuses executable ones (then a temp file) */
    int fd = from_memory ? memfd_create("xppautx-window", MFD_CLOEXEC | MFD_EXEC) : -1;
    if (from_memory && fd < 0 && errno == EINVAL) fd = memfd_create("xppautx-window", MFD_CLOEXEC);
    if (fd >= 0) {
        path = "/proc/self/fd/" + std::to_string(fd);
    } else {
        const char *dir = std::getenv("TMPDIR");
        temp = std::string(dir && *dir ? dir : "/tmp") + "/xppautx-window-XXXXXX";
        fd = mkstemp(temp.data());
        if (fd < 0) {
            err = "cannot write the window's library: " + std::string(std::strerror(errno));
            return nullptr;
        }
        path = temp;
    }
    void *lib = nullptr;
    const char *fail = std::getenv("XPP_WINDOW_FAIL_LOAD");
    if (!write_all(fd, xpp_window_lib, xpp_window_lib_len))
        err = "cannot write the window's library: " + std::string(std::strerror(errno));
    else if (fail && *fail && std::strcmp(fail, "0") != 0)
        err = path + ": " + std::string(SIMULATED_MISSING);
    else if (!(lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))) {
        const char *e = dlerror();
        err = e ? e : "dlopen failed";
    }
    /* the mapping keeps the library: neither the descriptor nor the file */
    close(fd);
    if (!temp.empty()) unlink(temp.c_str());
    return lib;
}

/* /etc/os-release's text (/usr/lib/os-release when that is missing), for
   the install command; empty when neither is there */
std::string os_release()
{
    for (const char *path : {"/etc/os-release", "/usr/lib/os-release"}) {
        xpp::LineReader r(path);
        if (!r) continue;
        std::string text;
        while (auto line = r.next()) {
            text += *line;
            text += '\n';
        }
        return text;
    }
    return {};
}

bool load(std::string &err)
{
    void *lib = open_library(err, true);
    /* a memfd the system will not map executable: the same bytes through a
       temp file; a missing WebKitGTK fails either way, so it is said once */
    if (!lib && err.find(": cannot open shared object file") == std::string::npos) {
        std::string again;
        lib = open_library(again, false);
        if (!lib) err = again;
    }
    if (!lib) return false;
    auto init = reinterpret_cast<XppWindowPluginInit>(dlsym(lib, XPP_WINDOW_PLUGIN_INIT));
    if (!init || !init(&host, &api)) {
        err = "its library is not this xppautX's";
        return false; /* left loaded: its code may have run */
    }
    return true;
}

} /* namespace */

int xpp_window_supported(void) { return 1; }

int xpp_window_run(void (*session)(void), const char *about)
{
    try {
        std::string err;
        loaded = load(err);
        if (!loaded) {
            std::array<char, 1024> msg;
            xpp_window_load_message(msg.data(), msg.size(), os_release().c_str(), err.c_str());
            xpp_log(XPP_LOG_WARN, "%s", msg.data());
            return 0;
        }
    } catch (const std::exception &e) {
        xpp_log(XPP_LOG_WARN, "xppautX: the window cannot open (%s); using the browser instead\n", e.what());
        return 0;
    }
    return api.run(session, about);
}

void xpp_window_set_model(const char *path)
{
    if (loaded) api.set_model(path);
}
