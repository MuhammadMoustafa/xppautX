/* Load values from a file by scope (set_load.h). */
#include "set_load.h"

#include "lunch-new.h"
#include "model.h"
#include "model_files.h"
#include "model_options.h"
#include "model_save.h"
#include "odex.h"
#include "odex_write.h"
#include "session.h"
#include "session_settings.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"

#include <map>
#include <memory>

namespace xpp {

namespace {

constexpr std::string_view LOADING = "load values";
/* the formats of the files of values, as their extensions name them */
constexpr std::string_view FORMAT_ODEX = "odex", FORMAT_PAR = "par", FORMAT_IC = "ic", FORMAT_SET = "set";

Error problem_at(std::string what, Place at) { return Error{std::string(LOADING), std::move(what), std::move(at)}; }

/* the parsed .odex and its text, which say where a position of it is */
class Parsed {
public:
    Parsed(const odex::Parsed &parsed, std::string_view text) : p_(parsed), text_(text) {}

    /* where pos is, without its source line and, in the file itself, without its name (an item of a part keeps this: a
       thing is copied into an error only; an item with no file is in the one the part is from) */
    Place at(const odex::Pos &pos) const
    {
        return Place{pos.file == 0 ? std::string() : p_.files.at(static_cast<std::size_t>(pos.file)), pos.line, pos.col};
    }
    /* ... and with both, when the position is in the file itself (its lines are split once, when the first is wanted) */
    Place shown(const odex::Pos &pos) const
    {
        Place place{p_.files.at(static_cast<std::size_t>(pos.file)), pos.line, pos.col};
        if (pos.file != 0) return place;
        if (!lines_) lines_.emplace("", "", text_);
        place.source = std::string(lines_->text(pos.line));
        return place;
    }

private:
    const odex::Parsed &p_;
    std::string_view text_;
    mutable std::optional<Lines> lines_;
};

/* the first thing wrong with a part is the one its load says */
void keep_problem(ValuesPart &part, const Parsed &file, const odex::Pos &pos, const std::string &what)
{
    if (!part.problem) part.problem = problem_at(what, file.shown(pos));
}

/* what a name of the file must be in the model */
enum class Wants { parameter, state, either };

/* the binding b of statement s as an item of part, or the part's problem: a name the model has not (never
   skipped), an array, a value that is not a number */
void add_binding(const Model &m, ValuesPart &part, const Parsed &file, const odex::Statement &s, const odex::Binding &b, Wants wants)
{
    const odex::ValueKind kind = value_kind(m, b.name);
    const bool known = wants == Wants::parameter ? kind == odex::ValueKind::parameter
                       : wants == Wants::state   ? kind == odex::ValueKind::state
                                                 : kind != odex::ValueKind::other;
    if (!known) {
        const char *what = wants == Wants::parameter ? "parameter" : wants == Wants::state ? "state" : "parameter or state";
        return keep_problem(part, file, b.pos, xpp::format("the model has no {} \"{}\"", what, b.name));
    }
    std::string number;
    switch (odex::whole_number(s, b, number)) {
    case odex::WholeNumber::element:
        return keep_problem(part, file, b.pos, xpp::format("\"{}\" is an array's element: only a whole name's value is loaded", b.name));
    case odex::WholeNumber::formula:
        return keep_problem(part, file, b.value.pos, xpp::format("\"{}\" is not given a number: only numbers are loaded from a file", b.name));
    case odex::WholeNumber::number: break;
    }
    part.items.push_back({b.name, number, false, file.at(b.pos)});
}

/* the settings entries as items of part. A set holds settings of some groups only (set_holds): in a set another is the
   part's problem, among the defaults (the model's own interface and output) it is not a value to load */
void add_options(ValuesPart &part, std::span<const odex::Option> entries, const Parsed &file, bool in_set)
{
    for (const odex::Option &o : entries) {
        int index = 0;
        const OptionRow *row = find_odex_option(o.group, o.name, index);
        if (!row) {
            keep_problem(part, file, o.pos, xpp::format("\"{}\" is not a setting", o.name));
            continue;
        }
        if (!set_holds(*row)) {
            if (in_set) keep_problem(part, file, o.pos, xpp::format("\"{}\" is a setting of {}, which no set holds", o.name, row->group));
            continue;
        }
        part.items.push_back({option_setting_name(*row, index), o.value, true, file.at(o.pos)});
    }
}

/* an .odex's defaults (its par and init lines, the options a set may hold) and each of its sets */
Result<ValuesSource> read_odex_source(Session &s, std::string_view path)
{
    Result<std::string> text = read_file_bytes("values file", path, settings_bytes_limit);
    if (!text) return std::unexpected(text.error());
    const Model &m = s.model();
    const std::unique_ptr<Model> scratch = scratch_model(m);
    /* the whole read, the imports in it too, is within the bound; a path outside the folder asks every time and is never kept */
    scratch->files_bytes_limit = text->size() < settings_bytes_limit ? settings_bytes_limit - text->size() : 0;
    scratch->outside_once_only = true;
    const Result<odex::Parsed> parsed = odex::read_statements(*scratch, *text, std::string(path));
    if (!parsed) return std::unexpected(parsed.error());
    const Parsed file{*parsed, *text};
    ValuesPart defaults;
    defaults.origin = ValuesOrigin::defaults;
    defaults.where = Place{std::string(path)};
    std::vector<ValuesPart> sets;
    std::map<std::string, odex::Pos> named;
    for (const odex::Statement &st : parsed->statements) {
        switch (st.kind) {
        case odex::Statement::Kind::Par:
        case odex::Statement::Kind::Init:
            for (const odex::Binding &b : st.bindings)
                add_binding(m, defaults, file, st, b, st.kind == odex::Statement::Kind::Par ? Wants::parameter : Wants::state);
            break;
        case odex::Statement::Kind::Options: add_options(defaults, st.more().options, file, false); break;
        case odex::Statement::Kind::Set: {
            ValuesPart part;
            part.origin = ValuesOrigin::set;
            part.name = st.name;
            part.where = Place{std::string(path), st.pos.line, st.pos.col};
            if (const auto [first, fresh] = named.emplace(st.name, st.pos); !fresh)
                keep_problem(part, file, st.pos, xpp::format("the set \"{}\" is defined twice: at line {} and at line {}", st.name, first->second.line, st.pos.line));
            for (const odex::Binding &b : st.bindings) add_binding(m, part, file, st, b, Wants::either);
            add_options(part, st.more().options, file, true);
            sets.push_back(std::move(part));
            break;
        }
        default: break;
        }
    }
    ValuesSource source{FORMAT_ODEX, std::move(*text), {}};
    if (!defaults.items.empty() || defaults.problem) source.parts.push_back(std::move(defaults));
    for (ValuesPart &part : sets) source.parts.push_back(std::move(part));
    return source;
}

/* an XPPAUT file's one part, empty: the model's names with the values the file gave them are put in it */
ValuesPart whole_file(std::string_view path)
{
    ValuesPart part;
    part.origin = ValuesOrigin::file;
    part.where = Place{std::string(path)};
    return part;
}

/* an XPPAUT .par or .ic (kind says which, whatever the file is called): its values, one for each name of the model's, as
   the part's items */
Result<ValuesSource> numbers_source(const Model &m, ValueFile kind, std::string_view path)
{
    const bool parameters = kind == ValueFile::parameters;
    const Result<std::vector<double>> z = parameters ? read_parameter_file(m, path) : read_ic_file(m, path);
    if (!z) return std::unexpected(z.error());
    const std::vector<std::string> &names = parameters ? m.upar_names : m.uvar_names;
    ValuesPart part = whole_file(path);
    for (std::size_t i = 0; i < z->size(); i++) part.items.push_back({names[i], odex::print_number((*z)[i]), false, {}});
    return ValuesSource{parameters ? FORMAT_PAR : FORMAT_IC, {}, {std::move(part)}};
}

/* e with the line of its place as written, which the items did not copy (a place in the file itself, by its name) */
Error with_source(Error e, const ValuesSource &source, std::string_view path)
{
    if (e.place.line > 0 && e.place.source.empty() && e.place.file == path && !source.text.empty())
        e.place.source = std::string(Lines("", "", source.text).text(e.place.line));
    return e;
}

/* an XPPAUT set's parameters, initial conditions and the numerics settings a set may hold; what no set holds (its graphs,
   delays, boundary conditions, run position) is Import XPPAUT session state's */
Result<ValuesSource> read_set_source(Session &s, std::string_view path)
{
    const Result<Settings> f = read_xppaut_set(s, path);
    if (!f) return std::unexpected(f.error());
    const Model &m = s.model();
    ValuesPart part = whole_file(path);
    for (int i = 0; i < m.nupar; i++)
        part.items.push_back({m.upar_names[static_cast<std::size_t>(i)], odex::print_number(f->params[static_cast<std::size_t>(i)]), false, {}});
    for (int i = 0; i < m.node + m.nmarkov; i++)
        part.items.push_back({m.uvar_names[static_cast<std::size_t>(i)], odex::print_number(f->last_ic[static_cast<std::size_t>(i)]), false, {}});
    for (const auto &[row, text] : f->options)
        if (set_holds(*row))
            for (std::string &item : option_items(*row, text)) part.items.push_back({option_setting_name(*row, 0), std::move(item), true, {}});
    return ValuesSource{FORMAT_SET, {}, {std::move(part)}};
}

}

Result<ValuesSource> read_values_source(Session &s, std::string_view path)
{
    const Model &m = s.model();
    if (files::has_extension(path, ".odex")) return read_odex_source(s, path);
    if (files::has_extension(path, ".par")) return numbers_source(m, ValueFile::parameters, path);
    if (files::has_extension(path, ".ic")) return numbers_source(m, ValueFile::initial_conditions, path);
    if (files::has_extension(path, ".set")) return read_set_source(s, path);
    return std::unexpected(problem_at(xpp::format("\"{}\" is none of the files of values: .odex, .par, .ic or .set", path), Place{std::string(path)}));
}

bool offers(const Session &s, const ValuesPart &part, std::string_view scope)
{
    return part.problem || !scoped_items(s, part.items, scope).empty();
}

Result<> load_values_from_file(Session &s, std::string_view path, std::string_view scope, ValuesOrigin origin, std::string_view name,
                               std::optional<int> graph)
{
    if (const std::string why = set_scope_problem(scope); !why.empty()) return std::unexpected(problem_at(why, command_place()));
    const Result<ValuesSource> source = read_values_source(s, path);
    if (!source) return std::unexpected(source.error());
    const std::string_view origin_name = values_origin_names[static_cast<std::size_t>(origin)];
    for (const ValuesPart &part : source->parts) {
        if (part.origin != origin || part.name != name) continue;
        if (part.problem) return std::unexpected(*part.problem);
        if (scoped_items(s, part.items, scope).empty()) {
            const std::string what = origin == ValuesOrigin::set ? xpp::format("the set \"{}\" of {}", name, path) : xpp::format("{}'s {}", path, origin_name);
            return std::unexpected(with_source(problem_at(xpp::format("{} holds no {}", what, scope == set_scope::all ? "values" : scope), part.where), *source, path));
        }
        Result<> applied = apply_values(s, part.items, part.where, scope, graph, LOADING);
        if (!applied) return std::unexpected(with_source(std::move(applied.error()), *source, path));
        return applied;
    }
    return std::unexpected(problem_at(origin == ValuesOrigin::set ? xpp::format("{} has no set \"{}\"", path, name) : xpp::format("{} has no {}", path, origin_name),
                                      Place{std::string(path)}));
}

Result<> load_xppaut_values(Session &s, ValueFile kind, std::string_view fn)
{
    const Result<ValuesSource> source = numbers_source(s.model(), kind, fn);
    if (!source) return std::unexpected(source.error());
    const ValuesPart &part = source->parts.front();
    return apply_values(s, part.items, part.where, kind == ValueFile::parameters ? set_scope::parameters : set_scope::initial, std::nullopt, LOADING);
}

int convert_state_file(const std::string &model, const std::string &state)
{
  /* the model first, as --convert alone writes it; an .odex is used as it is */
  if (!odex::is_odex(model))
    if (const int status = odex::convert_file(model); status != 0) return status;
  const std::string odex_file = odex::is_odex(model) ? model : odex::odex_name(model);
  const Result<CommandLine> line = parse_command_line({"xppautX", odex_file, "--convert"});
  if (!line) { show_error(line.error()); return 1; }
  const Loaded loaded = load_model(*line, 1);
  if (!loaded) return 1;
  Session &s = **loaded;
  /* the file's values and numerics are the model's own starting ones, written as its defaults */
  Result<> done = load_values_from_file(s, state, set_scope::all, ValuesOrigin::file, "", std::nullopt);
  if (done) done = save_defaults(s, set_scope::all, std::nullopt, false);
  if (!done) { show_error(done.error()); return 1; }
  xpp::log(XPP_LOG_INFO, "wrote {}\n", odex_file);
  return 0;
}

}
