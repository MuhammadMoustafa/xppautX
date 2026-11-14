#pragma once
/* Installing a downloaded release the way xppautX was installed (W622, #693). The download is xpp_update.h's (checked
   against the release list); this is what happens to the checked file. This crosses a trust boundary (a downloaded
   program is run), so what is run is only ever the content the release list's digest named:

   - how the program was installed is told from where it runs and its version (plan_for): a portable copy (Linux
     .tar.gz, Windows .zip) is swapped in, an .msi or a .deb is handed to the system's installer, and a build from a
     source checkout, macOS and a folder xppautX cannot write only open the release page, saying why;
   - the release file that fits is the one whose name ends with the plan's asset_suffix (the page picks it from the
     release list; update.download refuses any other);
   - a portable archive is read whole and checked (xpp_zip.h's readers: plain names, no link, bounded sizes); only its
     one program, `<folder>/xppautX[.exe]`, is taken, written beside the running one and renamed into place; the archive's
     digest is checked again just before, so a file changed in the temp folder is never installed;
   - an installer's file is kept (kept_folder, beside the preferences) with a record of its size and digest and checked
     again before it is offered at the next start and before the installer is started with it;
   - the installer and the new program are started by xpp_process.h: no shell, fixed arguments, the file as one argument.
   C++ only. */
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"
#include "xpp_update.h"

namespace xpp::update {

/* how a release is installed over this program: a choice, named */
enum class Install { portable, msi, deb, release_page };
std::string_view install_name(Install install);

enum class Os { linux_x64, windows_x64, macos, other };

/* what the rule needs to know about the running program */
struct Where {
    std::string version;          /* hello's version: a release tag, or a git describe / "dev" for a source checkout */
    Os os = Os::other;
    std::string program_folder;   /* the folder of the running program */
    std::string installer_folder; /* the folder the system's installer puts it in ("" when it has none) */
    bool writable = false;        /* a file can be made in program_folder */
};

struct Plan {
    Install install = Install::release_page;
    /* release_page: why the program is not installed over; "" otherwise */
    std::string why;
    /* the end of the name of the release file that fits ("-linux-x64.tar.gz"); "" for the release page */
    std::string asset_suffix;
    /* the system the release file is for, whose program the archive must hold; other for the release page */
    Os os = Os::other;
};

/* a tag as a release has it (v0.1.0, v0.1.0-beta.1), not a build from a source checkout (v0.1.0-12-gabcdef, abcdef, dev) */
bool is_release_version(std::string_view version);
/* the one rule, pure */
Plan plan_for(const Where &where);
/* the rule applied to the running program (its path, its version, the system's folders, a write probe) */
Plan running_plan();
/* the name of a release file fits the plan: it begins "xppautX-" and ends with the plan's asset_suffix */
bool fits(const Plan &plan, std::string_view asset_name);

/* the end of a downloaded and checked file's preparation: the new program is in place and needs a restart, or the
   installer's file is kept and installing needs one */
enum class Ready { restart, install };
std::string_view ready_name(Ready ready);

/* the program's own name in a release archive, and how a program file starts (a sanity check of what is renamed in) */
inline constexpr std::string_view PROGRAM_NAME = "xppautX";
/* the old program of a swap, beside the new one, removed at the next start (Windows cannot remove a running program) */
inline constexpr std::string_view REPLACED_SUFFIX = ".old";
inline constexpr std::string_view NEW_SUFFIX = ".new";

/* the version is a plain tag text (letters, digits . - _), at most this long */
inline constexpr std::size_t VERSION_BYTES = 64;
bool version_ok(std::string_view version);

/* portable: swaps the program of the archive `staged` into the place of the running one; an installer's file: keeps it.
   `file` is what the release list named and `staged` the checked download (xpp_update.h). */
Result<Ready> prepare(const Plan &plan, const ReleaseFile &file, std::string_view version, std::string_view staged);

/* the swap alone: the program of the checked archive `staged` (a .tar.gz or .zip as the plan's suffix says) in place of the file
   `running`; the archive is checked against `file`'s size and sha256 again first */
Result<> swap_program(const Plan &plan, const ReleaseFile &file, std::string_view staged, const std::string &running);

/* the folder an installer's file is kept in: beside the preferences ("" when there is no config folder) */
std::string kept_folder();
/* an installer's file kept at the last start or the last download, which still is what its record says */
struct Pending {
    std::string version, name, path;
};
/* the kept file, checked again against its record (size and sha256); nothing when none is kept; a record or a file that
   fails is removed and the error says why (the caller logs it) */
Result<std::optional<Pending>> pending(const Plan &plan);
/* removes what is kept */
void discard_pending();
/* the .msi's two public properties that only xppautX's own msiexec call sets, so an install by hand never starts the program: the
   flag, and the command line the program is started with (packaging/windows/xppautX.wxs's custom action reads both: the same names) */
inline constexpr std::string_view MSI_RELAUNCH = "XPPAUTX_RELAUNCH", MSI_RELAUNCH_ARGS = "XPPAUTX_RELAUNCH_ARGS";
/* the .msi's install as one argv: msiexec of `windows_folder`, the file, and the two properties that make the installer start the
   program again at its end, with --browser when `browser` and the file `reopen` ("" for none); an error when `reopen` could not
   be one quoted argument. Started with process::Quoting::installer, which writes the properties as the installer reads them. */
Result<std::vector<std::string>> msiexec_argv(std::string_view windows_folder, std::string_view msi, std::string_view reopen, bool browser);
/* hands the kept file to the system's installer (the .msi to Windows' msiexec, which starts the program again at its end with
   what start_new_program would pass; the .deb to xdg-open, which only installs: the system's software installer asks for the
   password itself), once it is checked again */
Result<> start_installer(const Plan &plan, std::string_view reopen, bool browser);
/* starts the program that was swapped in (one argument each: the model or session to reopen, "--browser" when the page
   is in a browser) and returns, the caller then quits */
Result<> start_new_program(std::string_view reopen, bool browser);
/* removes the old program a swap left (Windows), at the start */
void remove_replaced_program();

} // namespace xpp::update
