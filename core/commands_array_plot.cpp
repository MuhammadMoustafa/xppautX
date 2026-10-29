/* The array plot's registry rows (W527): its window, its settings (the columns, the rows and the colour scale's
   ends), its time scroll, the range of runs it saves as GIFs and its pictures. Each handler calls the core
   function the array plot window's keys call too (arrayplot.h): one behaviour, whichever front end asks. */
#include "arrayplot.h"
#include "command_def.h"
#include "command_range.h"
#include "image_format.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include <array>
#include <limits>

namespace xpp::command {
namespace {

/* every cell of the plot is sent as a number with each aplot event, and the rows it reads are int row numbers:
   at most this many cells (about the 1000 by 1000 of a large field), and none past the largest row number */
constexpr long long MAX_ARRAY_CELLS = 1000000;

Result<> shown_cells_fit(const Command &command, long long columns, long long rows, long long last_row, const Place &place)
{
    if (columns * rows > MAX_ARRAY_CELLS)
        return fail(command.name, xpp::format("{} columns by {} rows is {} cells: at most {} are shown", columns, rows, columns * rows, MAX_ARRAY_CELLS), place);
    if (last_row > std::numeric_limits<int>::max())
        return fail(command.name, xpp::format("the last row shown, {}, is past the largest row number {}", last_row, std::numeric_limits<int>::max()), place);
    return {};
}

/* the window the commands work on, or why there is none */
Result<> window_open(const Session &s, const Command &command, const Place &place)
{
    if (!s.array_plot.plot.alive) return fail(command.name, "No array plot is open: open one first (array_plot.open)", place);
    return {};
}

Result<std::string> open(Session &s, const Command &, const Arguments &, const Place &)
{
    make_my_aplot(s, "Array!");
    return ResultObject().boolean("open", s.array_plot.plot.alive != 0).done();
}

Result<std::string> redraw(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<> ready = window_open(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    ui.aplot_redraw(s);
    return ResultObject().boolean("redrawn", true).done();
}

Result<std::string> configure(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = window_open(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    APLOT &ap = s.array_plot.plot;
    int column = ap.index0;
    const std::string name = args.optional<std::string>("first_column").value_or(ap.name);
    if (args.has("first_column")) {
        column = -1;
        find_variable(s, name, &column);
        if (column < 0) return fail(command.name, xpp::format("\"{}\" is not a column of the stored data", name), place);
    }
    const long long columns = args.optional<long long>("columns").value_or(ap.nacross), rows = args.optional<long long>("rows").value_or(ap.ndown);
    const long long first_row = args.optional<long long>("first_row").value_or(ap.nstart), row_skip = args.optional<long long>("row_skip").value_or(ap.nskip);
    const long long column_skip = args.optional<long long>("column_skip").value_or(ap.ncskip);
    if (const Result<> fits = shown_cells_fit(command, columns / column_skip, rows, first_row + row_skip * (rows - 1), place); !fits)
        return std::unexpected<Error>(fits.error());
    const double z_min = args.optional<double>("z_min").value_or(ap.zmin), z_max = args.optional<double>("z_max").value_or(ap.zmax);
    if (!(z_min < z_max)) return fail(command.name, xpp::format("z_min {} must be below z_max {}", xpp::number(z_min), xpp::number(z_max)), place);
    ap.index0 = column;
    ap.name = name;
    ap.zmin = z_min;
    ap.zmax = z_max;
    ap.nacross = static_cast<int>(columns);
    ap.nstart = static_cast<int>(first_row);
    ap.ndown = static_cast<int>(rows);
    ap.nskip = static_cast<int>(row_skip);
    ap.ncskip = static_cast<int>(column_skip);
    s.array_plot.auto_redraw = args.optional<bool>("auto_redraw").value_or(s.array_plot.auto_redraw != 0) ? 1 : 0;
    ap.plotdef = 1;
    ui.aplot_reset_axes(s);
    return ResultObject().boolean("configured", true).done();
}

Result<std::string> fit(Session &s, const Command &command, const Arguments &, const Place &)
{
    if (const Result<> ready = aplot_ready(s, command.name); !ready) return std::unexpected<Error>(ready.error());
    fit_aplot(s);
    return ResultObject().real("fitted_min", s.array_plot.plot.zmin).real("fitted_max", s.array_plot.plot.zmax).done();
}

Result<std::string> range(Session &s, const Command &command, const Arguments &args, const Place &)
{
    if (const Result<> ready = aplot_ready(s, command.name, false); !ready) return std::unexpected<Error>(ready.error());
    begin_array_range(s, args.optional<bool>("still").value_or(s.array_plot.still != 0), args.optional<bool>("tag").value_or(s.array_plot.tag != 0),
                      args.get<std::string>("file"));
    RangeProgress done;
    set_range_settings(s.integrator.range, args);
    Result<int> swept = resolve_range_items(s, false) ? run_range(s, 0, &done) : std::unexpected<Error>(resolve_range_items(s, false).error());
    s.array_plot.range = 0;
    if (!swept) return std::unexpected<Error>(swept.error());
    return ResultObject().integer("steps_completed", done.runs).done();
}

Result<std::string> export_pdf(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = aplot_ready(s, command.name); !ready) return std::unexpected<Error>(ready.error());
    APLOT &ap = s.array_plot.plot;
    ap.xtitle = args.optional<std::string>("top_label").value_or(ap.xtitle);
    ap.ytitle = args.optional<std::string>("side_label").value_or(ap.ytitle);
    ap.bottom = args.optional<std::string>("bottom_label").value_or(ap.bottom);
    const std::string &file = args.get<std::string>("file");
    const Result<bool> written = write_aplot_pdf(s, file);
    if (!written) return std::unexpected<Error>(written.error());
    if (!*written) return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), place);
    return ResultObject().text("file", file).done();
}

Result<std::string> export_gif(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = aplot_ready(s, command.name); !ready) return std::unexpected<Error>(ready.error());
    const std::string &file = args.get<std::string>("file");
    const Result<bool> written = ui.aplot_gif(s, file);
    if (!written) return std::unexpected<Error>(written.error());
    if (!*written) return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), place);
    const Result<unsigned long long> size = file_size(file, command, place);
    if (!size) return std::unexpected<Error>(size.error());
    return ResultObject().text("file", file).integer("bytes_written", static_cast<long long>(*size)).done();
}

Result<std::string> scroll(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = window_open(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    const long long first_row = args.get<long long>("first_row"), shown = s.array_plot.plot.ndown;
    if (const Result<> fits = shown_cells_fit(command, 1, shown, first_row + s.array_plot.plot.nskip * (shown - 1), place); !fits)
        return std::unexpected<Error>(fits.error());
    s.array_plot.plot.nstart = static_cast<int>(first_row);
    ui.aplot_redraw(s);
    return ResultObject().integer("first_row", s.array_plot.plot.nstart).done();
}

constexpr long long MAX_INT = std::numeric_limits<int>::max();
constexpr std::array CONFIGURE_ARGUMENTS = {
    Argument{"first_column", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column},
    Argument{"columns", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_ARRAY_CELLS)},
    Argument{"first_row", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INT)},
    Argument{"rows", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_ARRAY_CELLS)},
    Argument{"row_skip", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INT)},
    Argument{"z_min", Type::real, false}, Argument{"z_max", Type::real, false},
    Argument{"auto_redraw", Type::boolean, false},
    Argument{"column_skip", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INT)}};
constexpr std::array FILE_ARGUMENT = {Argument{"file", Type::file, true}};
constexpr std::array RANGE_OWN_ARGUMENTS = {Argument{"still", Type::boolean, false}, Argument{"tag", Type::boolean, false}, Argument{"file", Type::file, true}};
constexpr auto RANGE_ALL_ARGUMENTS = join(RANGE_OWN_ARGUMENTS, RUN_RANGE_ARGUMENTS);
constexpr std::array PDF_ARGUMENTS = {Argument{"file", Type::file, true}, Argument{"top_label", Type::text, false},
                                      Argument{"side_label", Type::text, false}, Argument{"bottom_label", Type::text, false}};
constexpr std::array SCROLL_ARGUMENTS = {Argument{"first_row", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INT)}};
constexpr std::array OPEN_RESULTS = {ResultField{"open", "boolean"}};
constexpr std::array REDRAW_RESULTS = {ResultField{"redrawn", "boolean"}};
constexpr std::array CONFIGURE_RESULTS = {ResultField{"configured", "boolean"}};
constexpr std::array FIT_RESULTS = {ResultField{"fitted_min", "real"}, ResultField{"fitted_max", "real"}};
constexpr std::array RANGE_RESULTS = {ResultField{"steps_completed", "integer"}};
constexpr std::array PDF_RESULTS = {ResultField{"file", "file"}};
constexpr std::array GIF_RESULTS = {ResultField{"file", "file"}, ResultField{"bytes_written", "integer"}};
constexpr std::array SCROLL_RESULTS = {ResultField{"first_row", "integer"}};
constexpr std::array WINDOW_EVENTS = {"window", "aplot", "state", "done"};
constexpr std::array APLOT_EVENTS = {"aplot", "state", "done"};
constexpr std::array RANGE_EVENTS = {"aplot", "series", "state", "done"};
constexpr std::array DONE_EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"array_plot.open", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Viewaxes/Array", "VA",
            "Open the array plot window.", OPEN_RESULTS, WINDOW_EVENTS, true, open},
    Command{"array_plot.redraw", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Array plot/Redraw", "D",
            "Draw the array plot again from the stored data.", REDRAW_RESULTS, APLOT_EVENTS, true, redraw},
    Command{"array_plot.configure", CONFIGURE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Array plot/Edit", "E",
            "Set the columns and rows the array plot shows and the ends of its colour scale (z_min below z_max); what is left out stays.",
            CONFIGURE_RESULTS, APLOT_EVENTS, true, configure},
    Command{"array_plot.fit", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Array plot/Fit", "F",
            "Set the colour scale's ends to the smallest and largest value shown.", FIT_RESULTS, APLOT_EVENTS, true, fit},
    Command{"array_plot.range", RANGE_ALL_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Plot, "Array plot/Range", "R",
            "Run once for each value of a parameter or variable as run.range does, saving the array plot as GIF files of the model's folder: one movie, or with still one picture per run; tag writes the swept value on each.",
            RANGE_RESULTS, RANGE_EVENTS, true, range},
    Command{"array_plot.export_pdf", PDF_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "Array plot/Print PDF", "P",
            "Write the array plot as a PDF picture into a file of the model's folder; the labels left out are the last ones.",
            PDF_RESULTS, DONE_EVENTS, true, export_pdf},
    Command{"array_plot.export_gif", FILE_ARGUMENT, XPP_KIND_DATA, CommandCategory::Plot, "Array plot/GIF", "G",
            "Write the array plot as a GIF picture into a file of the model's folder.", GIF_RESULTS, DONE_EVENTS, true, export_gif},
    Command{"array_plot.scroll", SCROLL_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "Show the array plot from the stored row first_row on (row 0 is the first).", SCROLL_RESULTS, APLOT_EVENTS, true, scroll}};

} // namespace

std::span<const Command> array_plot_commands() { return ROWS; }

} // namespace xpp::command
