/* The update resource's registry rows (W622): whether the opening's check for a newer release is due, the page's report of
   a check that failed, and the download of a release file (checked against the release list: xpp_update.h). */
#include "command_def.h"
#include "command_registry.h"
#include "integrate.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_prefs.h"
#include "xpp_ui.h"
#include "xpp_update.h"
#include <array>

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

/* downloads a release file the release list named and checks it (xpp_update.h); Stop ends it, naming where it got to */
Result<std::string> download(Session &, const Command &command, const Arguments &args, const Place &place)
{
    if (files::serving_reads()) return fail(command.name, "a recording's replay downloads nothing", place);
    const update::ReleaseFile file{args.get<std::string>("url"), args.get<std::string>("name"),
                                   static_cast<unsigned long long>(args.get<long long>("size")), args.get<std::string>("digest")};
    if (const auto why = update::file_problem(file)) return fail(command.name, "the release file is refused: " + *why, place);
    const Computation computing;
    const Result<update::Downloaded> done = update::download_staged(
        file, https::get, [](unsigned long long got, unsigned long long of) {
            xpp::ui.progress(static_cast<std::int64_t>(of), static_cast<std::int64_t>(got), 0);
            return !job::cancelled();
        });
    if (!done) return fail(command.name, done.error().what, done.error().place.file.empty() ? place : done.error().place);
    return ResultObject().boolean("downloaded", !done->stopped).integer("bytes", static_cast<long long>(done->bytes)).text("file", update::staged_file()).done();
}

constexpr std::array FAILURE_ARGUMENTS = {Argument{.name = "reason", .type = Type::text, .required = true, .description = "Why the check failed, as the page tells it."}};
constexpr std::array DOWNLOAD_ARGUMENTS = {
    Argument{.name = "url", .type = Type::text, .required = true, .description = "The file's browser_download_url in the release list."},
    Argument{.name = "name", .type = Type::text, .required = true, .description = "The file's name in the release list."},
    Argument{.name = "size", .type = Type::integer, .required = true, .min = constant(1), .max = constant(static_cast<double>(update::MAX_DOWNLOAD_BYTES)),
             .description = "The file's size in bytes in the release list: no more is taken, and no less is accepted."},
    Argument{.name = "digest", .type = Type::text, .required = true, .description = "The file's digest in the release list: sha256: and 64 lowercase hex digits."}};
constexpr std::array ANSWER_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array RESULT_DUE = {ResultField{"due", "boolean"}};
constexpr std::array RESULT_LOGGED = {ResultField{"logged", "boolean"}};
constexpr std::array RESULT_DOWNLOADED = {ResultField{"downloaded", "boolean"}, ResultField{"bytes", "integer"}, ResultField{"file", "text"}};
constexpr std::array DOWNLOAD_EVENTS = {xpp::events::JOB_COMPUTING, xpp::events::JOB_PROGRESS, xpp::events::JOB_STOPPED, xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"update.due", {}, XPP_KIND_CONTROL, CommandCategory::Tools, "Update check due", "",
            "Whether the check for a newer release is due now: the preference is on and none began today (UTC). When it is, the check is recorded as begun. The page asks once when it opens.",
            RESULT_DUE, ANSWER_EVENTS, false, due, {}, {}},
    Command{"update.report_failure", FAILURE_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Tools, "Update check failed", "",
            "The page's check for a newer release failed: the log says why at INFO; nothing is shown.", RESULT_LOGGED, ANSWER_EVENTS, false, report_failure, {}, {}},
    Command{"update.download", DOWNLOAD_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Tools, "Download update", "",
            "Download a release file the release list named, from GitHub's file hosts only, over the system's HTTPS: no more than its size, refused if its size or sha256 digest differs, into a folder the program makes and removes. Progress comes as job.progress; Stop ends it.",
            RESULT_DOWNLOADED, DOWNLOAD_EVENTS, false, download, {}, {}}};

} // namespace

std::span<const Command> update_commands() { return ROWS; }

} // namespace xpp::command
