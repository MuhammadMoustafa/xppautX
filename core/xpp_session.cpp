/* A session (issue #11, the protocol's "session" command): one name for
   the files a long AUTO run is picked back up from, <base>.set (as File/
   Write set) and, when there is a diagram, <base>.auto (as AUTO's File/
   Save diagram, orbits included). */
#include "xpp_session.h"
#include "xpp_ui.h"
#include "xpp_io.h"
#include "lunch-new.h"
#include "diagram.h"    /* redraw_diagram; pulls in auto_nox.h */
#include "load_eqn.h"   /* XPP_MAX_NAME */
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>

extern int NBifs;       /* diagram.c: >1 once a diagram has a point in it */
extern BIFUR Auto;
extern char this_file[XPP_MAX_NAME];

namespace {

std::string session_set;
std::string session_auto;

struct FileCloser {
    void operator()(FILE *fp) const noexcept { std::fclose(fp); }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

/* base may be empty: ask for one the way do_lunch's Write/Read set does
   (file_selector writes up to 256 bytes into its buffer), returning it
   without its .set. false on cancel. */
bool ask_base(const char *title, std::string &base)
{
    std::array<char, XPP_MAX_NAME + 10> buf{};
    std::string def = std::string(this_file) + ".set";
    def.copy(buf.data(), std::min(def.size(), buf.size() - 1));
    ping();
    if (!file_selector(title, buf.data(), "*.set")) return false;
    base = buf.data();
    if (base.size() >= 4 && base.ends_with(".set")) base.resize(base.size() - 4);
    return true;
}

/* base, or when it is NULL/empty the one the user picks */
bool base_name(const char *title, const char *base, std::string &out)
{
    if (base == nullptr || base[0] == 0) return ask_base(title, out);
    out = base;
    return true;
}

} // namespace

const char *xpp_session_set_file(void) { return session_set.c_str(); }
const char *xpp_session_auto_file(void) { return session_auto.c_str(); }

int xpp_session_save(const char *base_arg)
{
    std::string base;
    if (!base_name("Save session", base_arg, base)) return 0;

    std::string set_name = base + ".set";
    {
        xpp::Writer w(set_name.c_str());
        if (!w) {
            err_msg("Cannot open file");
            return 0;
        }
        redraw_params(); /* as do_lunch's Write set does, before write_lunch */
        write_lunch(w.file());
        if (!w.commit()) return 0;
    }
    session_set = set_name;
    session_auto.clear();

    if (NBifs > 1) { /* a diagram exists (save_diagram's own empty check) */
        std::string auto_name = base + ".auto";
        xpp::Writer w(auto_name.c_str());
        if (!w) {
            err_msg("Cannot open AUTO file");
            return 0;
        }
        if (save_auto_file(w.file()) != 1) {
            err_msg("Empty diagram -- nothing to save");
            return 0;
        }
        if (!w.commit()) return 0;
        session_auto = auto_name;
    }
    return 1;
}

int xpp_session_load(const char *base_arg)
{
    std::string base;
    if (!base_name("Load session", base_arg, base)) return 0;

    std::string set_name = base + ".set";
    {
        FilePtr fp(std::fopen(set_name.c_str(), "r"));
        if (!fp) {
            err_msg("Cannot open file");
            return 0;
        }
        if (!read_lunch(fp.get())) return 0;
    }
    session_set = set_name;
    session_auto.clear();

    std::string auto_name = base + ".auto";
    FilePtr fp(std::fopen(auto_name.c_str(), "r"));
    if (fp) {
        if (NBifs > 1) yes_reset_auto(); /* as load_auto does, without its confirmation ask */
        if (!Auto.exist) do_auto_win(); /* the diagram needs a window to draw into */
        if (load_auto_file(fp.get()) != 1) {
            err_msg("Bad AUTO file");
            return 0;
        }
        fp.reset();
        if (Auto.exist) redraw_diagram(); /* load_auto leaves this to the caller */
        session_auto = auto_name;
    }
    return 1;
}
