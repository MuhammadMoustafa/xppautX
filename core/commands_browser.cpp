/* The browser resource's registry rows (W526): the data browser's table (the block of it a client shows), the
   rows First..Last kept, and what it does to the stored data: find a value, replace a column by a formula (and
   take it back), add and delete columns, tabulate a column, transpose, and load and save data files.
   A handler calls the same core function the browser window's keys do after their forms (browse.h, adj2.h):
   one behaviour, whichever front end asks. Get (a row made the initial conditions) is ic.from_data_row. */
#include "adj2.h"
#include "browse.h"
#include "command_def.h"
#include "model_options.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include <array>
#include <limits>

namespace xpp::command {
namespace {

constexpr const char *COLUMN_KEY = "column", *NAME_KEY = "name", *FORMULA_KEY = "formula", *FILE_KEY = "file", *ROW_KEY = "row",
                     *FORMAT_KEY = "format", *TARGET_KEY = "target";

constexpr auto TARGET_CHOICES = c_names(data_target_names);
constexpr std::array TARGETS = {DataTarget::table, DataTarget::output, DataTarget::plot_curves};

Result<std::string> find(Session &s, const Command &, const Arguments &args, const Place &)
{
    return found(find_in_column(s, args.get<std::string>(COLUMN_KEY), args.get<double>("value")), "row_found");
}

Result<std::string> replace_column(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &column = args.get<std::string>(COLUMN_KEY);
    if (const Result<> replaced = replace_data_column(s, column, args.get<std::string>(FORMULA_KEY)); !replaced)
        return std::unexpected<Error>(replaced.error());
    return ResultObject().text(COLUMN_KEY, column).boolean("replaced", true).done();
}

Result<std::string> undo_replace(Session &s, const Command &, const Arguments &, const Place &)
{
    return performed(unreplace_column(s), "undone");
}

Result<std::string> tabulate(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const Result<std::size_t> rows = tabulate_column(s, args.get<std::string>("variable"), args.get<double>("x_low"), args.get<double>("x_high"), file);
    if (!rows) return std::unexpected<Error>(rows.error());
    if (*rows == 0) return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), place);
    return ResultObject().text(FILE_KEY, file).integer("rows_written", static_cast<long long>(*rows)).done();
}

Result<std::string> set_slice_start(Session &s, const Command &, const Arguments &args, const Place &)
{
    return found(set_first_row(s, static_cast<int>(args.get<long long>(ROW_KEY))), "first_row");
}

Result<std::string> set_slice_end(Session &s, const Command &, const Arguments &args, const Place &)
{
    return found(set_last_row(s, static_cast<int>(args.get<long long>(ROW_KEY))), "last_row");
}

Result<std::string> restore_full_slice(Session &s, const Command &, const Arguments &, const Place &)
{
    const Result<int> last = restore_whole_data(s);
    if (!last) return std::unexpected<Error>(last.error());
    return ResultObject().integer("first_row", 0).integer("last_row", *last).done();
}

Result<std::string> add_column(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &name = args.get<std::string>(NAME_KEY);
    if (const Result<> added = add_data_column(s, name, args.get<std::string>(FORMULA_KEY)); !added) return std::unexpected<Error>(added.error());
    return ResultObject().text(COLUMN_KEY, s.browser.added_columns.back().name).boolean("added", true).done();
}

Result<std::string> delete_column(Session &s, const Command &, const Arguments &args, const Place &)
{
    return performed(delete_added_column(s, args.get<std::string>(NAME_KEY)), "deleted");
}

Result<std::string> load(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<DataLoaded> loaded = load_data_file(s, args.optional<std::string>(FORMAT_KEY).value_or(std::string{}),
                                                     args.optional<std::string>(FILE_KEY).value_or(std::string{}));
    if (!loaded) return std::unexpected<Error>(loaded.error());
    return ResultObject().text(FILE_KEY, loaded->file).integer("rows_loaded", static_cast<long long>(loaded->rows)).done();
}

Result<std::string> export_data(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    std::optional<DataTarget> target;
    if (const auto name = args.optional<std::string>(TARGET_KEY)) target = TARGETS[choice_index(data_target_names, *name)];
    const Result<DataSaved> saved = save_data_file(s, target, args.optional<std::string>(FORMAT_KEY).value_or(std::string{}),
                                                   args.optional<std::string>(FILE_KEY).value_or(std::string{}));
    if (!saved) return std::unexpected<Error>(saved.error());
    if (!saved->file.empty() && saved->rows == 0)
        return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", saved->file), place);
    return ResultObject().text(FILE_KEY, saved->file).integer("rows_written", static_cast<long long>(saved->rows)).done();
}

Result<std::string> page(Session &s, const Command &, const Arguments &args, const Place &)
{
    BrowserBlock &block = s.browser.block;
    block.from = static_cast<int>(args.get<long long>("from_row"));
    block.count = static_cast<int>(args.get<long long>("count"));
    block.col = static_cast<int>(args.optional<long long>("first_column").value_or(block.col));
    block.ncol = static_cast<int>(args.optional<long long>("columns").value_or(block.ncol));
    fit_browser_block(s);
    ui.browser_redraw(0);
    return ResultObject()
        .integer("first_row", block.from)
        .integer("total_rows", s.browser.view.dataflag ? s.browser.view.maxrow : 0)
        .raw("rows", browser_block_rows(s))
        .done();
}

Result<std::string> transpose(Session &s, const Command &, const Arguments &args, const Place &)
{
    const AdjointState &now = s.adjoint;
    const auto number = [&](const char *name, int kept) { return static_cast<int>(args.optional<long long>(name).value_or(kept)); };
    const Result<> done = transpose_data(s, args.optional<std::string>("first_column").value_or(now.transpose.firstcol), number("columns", now.transpose.ncol),
                                         number("column_skip", now.transpose.colskip), number("first_row", now.transpose.row0),
                                         number("rows", now.transpose.nrow), number("row_skip", now.transpose.rowskip));
    if (!done) return std::unexpected<Error>(done.error());
    return ResultObject().integer("transposed_rows", s.adjoint.transpose.ncol).integer("transposed_cols", s.adjoint.transpose.nrow).done();
}

constexpr int MAX_INDEX = std::numeric_limits<int>::max();
constexpr Argument COLUMN = {COLUMN_KEY, Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument ROW = {ROW_KEY, Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INDEX)};
constexpr Argument FORMULA = {FORMULA_KEY, Type::expression, true};
/* a file of the model's folder; left out, asked for (the desktop window's own dialog) */
constexpr Argument FILE_ASKED = {FILE_KEY, Type::file, false};
constexpr Argument FORMAT = {FORMAT_KEY, Type::choice, false, MAX_TEXT_BYTES, data_format_ids};
constexpr Argument TRANSPOSE_INTEGER_FIRST = {"first_row", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INDEX)};

constexpr std::array FIND_ARGUMENTS = {COLUMN, Argument{"value", Type::real, true}};
/* a formula that is a sequence (1:10), an integral (&x) or a derivative (@x) is not an expression: the core reads it */
constexpr std::array REPLACE_ARGUMENTS = {COLUMN, Argument{FORMULA_KEY, Type::text, true}};
constexpr std::array TABULATE_ARGUMENTS = {Argument{"variable", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column},
                                           Argument{"x_low", Type::real, true}, Argument{"x_high", Type::real, true}, Argument{FILE_KEY, Type::file, true}};
constexpr std::array SLICE_ARGUMENTS = {ROW};
constexpr std::array ADD_ARGUMENTS = {Argument{NAME_KEY, Type::text, true}, FORMULA};
constexpr std::array DELETE_ARGUMENTS = {Argument{NAME_KEY, Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column}};
constexpr std::array LOAD_ARGUMENTS = {FILE_ASKED, FORMAT};
constexpr std::array EXPORT_ARGUMENTS = {FILE_ASKED, FORMAT, Argument{TARGET_KEY, Type::choice, false, MAX_TEXT_BYTES, TARGET_CHOICES}};
constexpr std::array PAGE_ARGUMENTS = {Argument{"from_row", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INDEX)},
                                       Argument{"count", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(BROWSER_MAX_ROWS)},
                                       Argument{"first_column", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INDEX)},
                                       Argument{"columns", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(BROWSER_MAX_COLS)}};
constexpr std::array TRANSPOSE_ARGUMENTS = {Argument{"first_column", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column},
                                            Argument{"columns", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INDEX)},
                                            Argument{"column_skip", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INDEX)},
                                            TRANSPOSE_INTEGER_FIRST,
                                            Argument{"rows", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INDEX)},
                                            Argument{"row_skip", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_INDEX)}};

constexpr std::array FIND_RESULTS = {ResultField{"row_found", "integer"}};
constexpr std::array REPLACE_RESULTS = {ResultField{COLUMN_KEY, "name"}, ResultField{"replaced", "boolean"}};
constexpr std::array UNDO_RESULTS = {ResultField{"undone", "boolean"}};
constexpr std::array TABULATE_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{"rows_written", "integer"}};
constexpr std::array START_RESULTS = {ResultField{"first_row", "integer"}};
constexpr std::array END_RESULTS = {ResultField{"last_row", "integer"}};
constexpr std::array RESTORE_RESULTS = {ResultField{"first_row", "integer"}, ResultField{"last_row", "integer"}};
constexpr std::array ADD_RESULTS = {ResultField{COLUMN_KEY, "name"}, ResultField{"added", "boolean"}};
constexpr std::array DELETE_RESULTS = {ResultField{"deleted", "boolean"}};
constexpr std::array LOAD_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{"rows_loaded", "integer"}};
constexpr std::array EXPORT_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{"rows_written", "integer"}};
constexpr std::array PAGE_RESULTS = {ResultField{"first_row", "integer"}, ResultField{"total_rows", "integer"},
                                     ResultField{"rows", "list[list[real]]"}};
constexpr std::array TRANSPOSE_RESULTS = {ResultField{"transposed_rows", "integer"}, ResultField{"transposed_cols", "integer"}};

constexpr std::array STATE_EVENTS = {"state", "done"};
constexpr std::array BROWSER_EVENTS = {"browser", "state", "done"};
constexpr std::array PAGE_EVENTS = {"browser", "done"};

constexpr std::array ROWS = {
    Command{"browser.find", FIND_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Tools, "Data/Find", "F",
            "Select the row, from the selected one on, whose value in a column is nearest a number.", FIND_RESULTS, BROWSER_EVENTS, true, find, {}, {}, "browser"},
    Command{"browser.replace_column", REPLACE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Replace", "R",
            "Replace a column of the data by a formula of the columns, '&column' (its running integral), '@column' (its derivative), 'a:b' (from a to b) or 'a;b' (from a in steps of b).",
            REPLACE_RESULTS, BROWSER_EVENTS, true, replace_column, {}, {}, "browser"},
    Command{"browser.undo_replace", {}, XPP_KIND_DATA, CommandCategory::Tools, "Data/Unreplace", "U",
            "Put back the column the last replace overwrote.", UNDO_RESULTS, BROWSER_EVENTS, true, undo_replace, {}, {}, "browser"},
    Command{"browser.tabulate_column", TABULATE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Table", "T",
            "Write the rows kept between First and Last of a column as a lookup table over x_low to x_high into a file of the model's folder.",
            TABULATE_RESULTS, STATE_EVENTS, true, tabulate, {}, {}, "browser", "write", "*.tab"},
    Command{"browser.set_slice_start", SLICE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/First", "H",
            "Keep the data from a row on (the first is 0) for saving and tabulating, and select that row.", START_RESULTS, BROWSER_EVENTS, true,
            set_slice_start, {}, {}, "browser"},
    Command{"browser.set_slice_end", SLICE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Last", "E",
            "Keep the data up to and including a row for saving and tabulating, and select that row.", END_RESULTS, BROWSER_EVENTS, true,
            set_slice_end, {}, {}, "browser"},
    Command{"browser.restore_full_slice", {}, XPP_KIND_DATA, CommandCategory::Tools, "Data/Restore", "S",
            "Keep all the data again, from the first row to the last, and replot it in the selected graph.", RESTORE_RESULTS, BROWSER_EVENTS,
            true, restore_full_slice, {}, {}, "browser"},
    Command{"browser.add_column", ADD_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Add column", "A",
            "Add a column computed from a formula of the others, now and after every run, as though it were another auxiliary quantity.",
            ADD_RESULTS, BROWSER_EVENTS, true, add_column, {}, {}, "browser"},
    Command{"browser.delete_column", DELETE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Delete column", "D",
            "Delete a column made by add_column (time and the model's columns stay; a plotted one is refused).", DELETE_RESULTS, BROWSER_EVENTS,
            true, delete_column, {}, {}, "browser"},
    Command{"browser.load", LOAD_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Load data", "L",
            "Read a data file of the model's folder into the table (its columns fill T and the columns after it in order); with no file, ask for one (the result's file is empty when that question was cancelled).",
            LOAD_RESULTS, BROWSER_EVENTS, true, load, {}, {}, "browser"},
    Command{"browser.export", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Export data", "W",
            "Save the data table (the rows kept between First and Last), the model's output columns or what the plot shows as a data file in the model's folder; left out, format follows the file's extension and file and target are asked for (the result's file is empty when a question was cancelled).",
            EXPORT_RESULTS, STATE_EVENTS, true, export_data, {}, {}, "browser", "write", "*"},
    Command{"browser.page", PAGE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Tools, "Data/Browser/Page", "",
            "Choose the block of the table the client shows: rows from_row on (at most the limit hello gives), count rows (0 stops the block's events), the columns after T from first_column; answered at once, even during a computation.",
            PAGE_RESULTS, PAGE_EVENTS, false, page, {}, {}, "browser"},
    Command{"browser.transpose", TRANSPOSE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Tools, "Data/Transpose data", "FT",
            "Show the stored data transposed in the table: columns columns from first_column in steps of column_skip, and rows rows from first_row in steps of row_skip, each column a row; left out, the last values.",
            TRANSPOSE_RESULTS, BROWSER_EVENTS, true, transpose}};

} // namespace

std::span<const Command> browser_commands() { return ROWS; }

} // namespace xpp::command
