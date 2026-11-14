/* The update resource's registry rows (W622): whether the opening's check for a newer release is due, the page's report of
   a check that failed, and the download of a release file (checked against the release list: xpp_update.h). */
#include "command_def.h"
#include "session.h"
#include "xpp_session.h"
#include "model_switch.h"
#include "xpp_files.h"
#include "xpp_globals.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_prefs.h"
#include "xpp_ui.h"
#include "xpp_install.h"
#include "xpp_update.h"
#include "model.h"
#include "event_registry.h"
#include "command_category.h"
#include "xpp_error.h"
#include "xpp_https.h"
#include "xpp_io.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace xpp::command {
namespace {

/* the page's opening asks whether to check for a newer release now: a recording's replay leaves the user's preferences alone,
   so it never is */
Result<std::string> due(Session &, const Command &, const Arguments &, const Place &)
{
    ResultObject result;
    if (files::serving_reads()) return result.boolean("due", false).done();
    const Result<bool> now = prefs::update_check_due();
    /* a bad preferences file is shown by the commands that use it; here the check is simply not made */
    if (!now) xpp::log(XPP_LOG_INFO, "{}\n", now.error().text());
    return result.boolean("due", now.value_or(false)).done();
}

/* the page's check for a newer release failed: shown nowhere, the log tells it at INFO */
Result<std::string> report_failure(Session &, const Command &, const Arguments &args, const Place &)
{
    xpp::log(XPP_LOG_INFO, "The check for a newer release failed: {}\n", args.get<std::string>("reason"));
    return ResultObject().boolean("logged", true).done();
}

/* downloads the release file the release list named for this installation and prepares it (xpp_install.h): a portable copy's
   program is swapped in, an installer's file is kept; Stop ends the download, naming where it got to */
Result<std::string> download(Session &, const Command &command, const Arguments &args, const Place &place)
{
    if (files::serving_reads()) return fail(command.name, "a recording's replay downloads nothing", place);
    const update::ReleaseFile file{args.get<std::string>("url"), args.get<std::string>("name"),
                                   static_cast<unsigned long long>(args.get<long long>("size")), args.get<std::string>("digest")};
    if (const auto why = update::file_problem(file)) return fail(command.name, "the release file is refused: " + *why, place);
    const update::Plan plan = update::running_plan();
    if (plan.install == update::Install::release_page) return fail(command.name, "this installation is updated from the release page: " + plan.why, place);
    if (!update::fits(plan, file.name))
        return fail(command.name, xpp::format("\"{}\" is not the release file for this installation (its name ends with {})", file.name, plan.asset_suffix), place);
    const Computation computing;
    const TempDir folder;
    if (folder.path().empty()) return fail(command.name, "a folder for the download could not be made", place);
    const Result<update::Downloaded> done = update::download(
        file, folder.path(), https::get, [](unsigned long long got, unsigned long long of) {
            job::report_bytes(got, of);
            xpp::ui.progress(static_cast<std::int64_t>(of), static_cast<std::int64_t>(got), 0);
            return !job::cancelled();
        });
    if (!done) return fail(command.name, done.error().what, done.error().place.file.empty() ? place : done.error().place);
    ResultObject result;
    result.boolean("downloaded", !done->stopped).integer("bytes", static_cast<long long>(done->bytes));
    if (done->stopped) {
        xpp::log(XPP_LOG_INFO, "The download of \"{}\" stopped at {} of {} bytes\n", file.name, done->bytes, file.size);
        return result.done();
    }
    const Result<update::Ready> ready = update::prepare(plan, file, args.get<std::string>("version"), done->path);
    if (!ready) return fail(command.name, ready.error().what, ready.error().place.file.empty() ? place : ready.error().place);
    return result.text("ready", update::ready_name(*ready)).done();
}

/* the kept installer file of an earlier download, checked again against its record; a file that fails is removed and the log says why
   (the page asks once when it opens, after update.due) */
Result<std::string> pending(Session &, const Command &, const Arguments &, const Place &)
{
    ResultObject result;
    if (files::serving_reads()) return result.boolean("pending", false).done();
    const Result<std::optional<update::Pending>> kept = update::pending(update::running_plan());
    if (!kept) xpp::log(XPP_LOG_WARN, "{}\n", Error{kept.error().where, "The kept update is removed: " + kept.error().what, kept.error().place}.text());
    if (!kept || !*kept) return result.boolean("pending", false).done();
    return result.boolean("pending", true).text("version", (*kept)->version).done();
}

/* the page found the kept update is not newer than this program (it was installed another way) */
Result<std::string> discard(Session &, const Command &, const Arguments &, const Place &)
{
    if (!files::serving_reads()) update::discard_pending();
    return ResultObject().boolean("discarded", true).done();
}

/* the file the next start reopens: the session saved or opened, else the model, "" for the start screen */
std::string reopen_target(const Session &s)
{
    const Model &m = s.model();
    if (!s.saved_session->file.empty()) return s.saved_session->file;
    return m.loaded() && m.this_file != "console" ? files::absolute(m.this_file, m.load_dir) : std::string();
}

/* update.restart and update.install: the leave question Quit asks (save, don't save, cancel), then the new program or the installer is
   started and this one quits; a cancelled question changes nothing */
Result<std::string> switch_program(Session &s, const Command &command, const Arguments &args, const Place &place, bool installer)
{
    if (files::serving_reads()) return fail(command.name, "a recording's replay restarts nothing", place);
    if (program.page == XppProgram::Page::none) return fail(command.name, "only the window and the browser page restart xppautX", place);
    /* checked before the question: a restart is a portable copy's, an install an installer's (start_installer checks which) */
    const update::Plan plan = update::running_plan();
    if (!installer && plan.install != update::Install::portable)
        return fail(command.name, xpp::format("only a portable copy restarts into an update; this one is updated {}", plan.install == update::Install::release_page ? "from the release page: " + plan.why : "by its installer (update.install)"), place);
    const bool recording = recording_in_progress();
    ResultObject result;
    /* save given: the page already asked the leave question (during a run, W689), so its answer goes as session.quit's does */
    const std::optional<bool> save = args.optional<bool>("save");
    const bool may_leave = save ? xpp_session_leave_saved(s, *save, recording)
                                : xpp_session_may_leave(s, leave_question(installer ? Leave::install : Leave::restart, recording), recording);
    if (!may_leave) return result.boolean("restarting", false).done();
    const std::string reopen = reopen_target(s);
    const bool browser = program.page == XppProgram::Page::browser;
    const Result<> started = installer ? update::start_installer(plan, reopen, browser) : update::start_new_program(reopen, browser);
    if (!started) return fail(command.name, started.error().what, started.error().place.file.empty() ? place : started.error().place);
    bye_bye();
    return result.boolean("restarting", true).done();
}

Result<std::string> restart(Session &s, const Command &command, const Arguments &args, const Place &place) { return switch_program(s, command, args, place, false); }
Result<std::string> install(Session &s, const Command &command, const Arguments &args, const Place &place) { return switch_program(s, command, args, place, true); }

constexpr std::array FAILURE_ARGUMENTS = {Argument{.name = "reason", .type = Type::text, .required = true, .description = "Why the check failed, as the page tells it."}};
constexpr std::array DOWNLOAD_ARGUMENTS = {
    Argument{.name = "url", .type = Type::text, .required = true, .description = "The file's browser_download_url in the release list."},
    Argument{.name = "name", .type = Type::text, .required = true, .description = "The file's name in the release list."},
    Argument{.name = "size", .type = Type::integer, .required = true, .min = constant(1), .max = constant(static_cast<double>(update::MAX_DOWNLOAD_BYTES)),
             .description = "The file's size in bytes in the release list: no more is taken, and no less is accepted."},
    Argument{.name = "digest", .type = Type::text, .required = true, .description = "The file's digest in the release list: sha256: and 64 lowercase hex digits."},
    Argument{.name = "version", .type = Type::text, .required = true, .description = "The release's version as its tag gives it without the v (0.2.0, 0.2.0-beta.1): what an installer's kept file is recorded as."}};
constexpr std::array DONE_EVENTS = {xpp::events::DONE};
constexpr std::array ANSWER_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array RESULT_DUE = {ResultField{"due", "boolean"}};
constexpr std::array RESULT_PENDING = {ResultField{"pending", "boolean"}, ResultField{"version", "text"}};
constexpr std::array RESULT_DISCARDED = {ResultField{"discarded", "boolean"}};
constexpr std::array RESULT_SWITCH = {ResultField{"restarting", "boolean"}};
/* save as session.quit's: the page's answer to the leave question, given while a computation runs (the run stops, then this runs);
   left out, the core asks the question itself, while idle */
constexpr std::array SWITCH_ARGUMENTS = {Argument{.name = "save", .type = Type::boolean, .required = false,
    .description = "The leave question's answer, given by the page while a computation runs: true saves the session (and a recording in progress) first, false leaves at once; left out, the question is asked."}};
constexpr std::array RESULT_LOGGED = {ResultField{"logged", "boolean"}};
constexpr std::array RESULT_DOWNLOADED = {ResultField{"downloaded", "boolean"}, ResultField{"bytes", "integer"}, ResultField{"ready", "text"}};
constexpr std::array DOWNLOAD_EVENTS = {xpp::events::JOB_COMPUTING, xpp::events::JOB_PROGRESS, xpp::events::JOB_STOPPED, xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"update.due", {}, XPP_KIND_CONTROL, CommandCategory::Tools, "Update check due", "",
            "Whether the check for a newer release is due now: the preference is on and none began today (UTC). When it is, the check is recorded as begun. The page asks once when it opens.",
            RESULT_DUE, ANSWER_EVENTS, false, due, {}, {}},
    Command{"update.report_failure", FAILURE_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Tools, "Update check failed", "",
            "The page's check for a newer release failed: the log says why at INFO; nothing is shown.", RESULT_LOGGED, ANSWER_EVENTS, false, report_failure, {}, {}},
    Command{"update.download", DOWNLOAD_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Tools, "Download update", "",
            "Download the release file the release list named for this installation, from GitHub's file hosts only, over the system's HTTPS: no more than its size, refused if its size or sha256 digest differs, into a folder the program makes and removes. Then a portable copy's program is swapped in (ready: restart) or an installer's file is kept (ready: install). Progress comes as job.progress; Stop ends it.",
            RESULT_DOWNLOADED, DOWNLOAD_EVENTS, false, download, {}, {}},
    Command{"update.pending", {}, XPP_KIND_CONTROL, CommandCategory::Tools, "Kept update", "",
            "Whether an update's installer file was kept by an earlier download (Later): it is checked again against its recorded size and sha256 and offered only when it still is that file; one that is not is removed and the log says why. The page asks once when it opens.",
            RESULT_PENDING, ANSWER_EVENTS, false, pending, {}, {}},
    Command{"update.discard", {}, XPP_KIND_CONTROL, CommandCategory::Tools, "Discard kept update", "",
            "Remove the kept installer file: the page found it is not newer than this program.", RESULT_DISCARDED, ANSWER_EVENTS, false, discard, {}, {}},
    Command{"update.restart", SWITCH_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Restart to update", "",
            "After a portable copy was updated: ask the leave question Quit asks, then start the new program, reopening the model or session saved, and quit. Only the window and the browser page restart.",
            RESULT_SWITCH, DONE_EVENTS, false, restart, {}, {}},
    Command{"update.install", SWITCH_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Install update and quit", "",
            "After a download that kept an installer's file: ask the leave question Quit asks, hand the checked file to the system's installer (msiexec for an .msi, which starts the program again at its end, reopening the model or session saved and with --browser in the browser page; the software installer for a .deb, which only installs: no shell, no sudo) and quit.",
            RESULT_SWITCH, DONE_EVENTS, false, install, {}, {}}};

} // namespace

std::span<const Command> update_commands() { return ROWS; }

} // namespace xpp::command
