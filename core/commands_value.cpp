/* The value resource's registry rows (W526): the parameters, the sliders that move them, the undo and redo of
   value edits, and the .par and .ic files of values. They are settings: sent during a computation they apply when
   it ends, never to the run in progress. A handler calls the same core function the main menu's keys do (the
   Parameters form, Undo and Redo): one behaviour, whichever front end asks. */
#include "command_def.h"
#include "expr.h"
#include "lunch-new.h"
#include "session.h"
#include "typed_values.h"
#include "value_undo.h"
#include "xpp_io.h"
#include "xpp_util.h"
#include <array>

namespace xpp::command {
namespace {

constexpr const char *NAME_KEY = "name", *VALUE_KEY = "value", *SLOT_KEY = "slot", *KIND_KEY = "kind", *FILE_KEY = "file";

/* the field name the page keys a parameter's error by (docs/protocol.md "Errors") */
std::string field_of(std::string_view name) { return "par:" + lower_case(std::string(name)); }

/* the parameter's value now, as the page shows it */
double parameter_now(const Session &s, std::string_view name)
{
    double value = 0;
    get_val(s, name, &value);
    return value;
}

/* the parameters set as {"name":value,...} */
std::string updated(const Session &s, std::span<const std::string> names)
{
    std::string text = "{";
    for (const std::string &name : names) {
        if (text.size() > 1) text += ',';
        json_append_string(text, name);
        text += ':';
        json::json_append_number_shortest(text, parameter_now(s, name));
    }
    return text + '}';
}

Result<std::string> set_parameters(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const auto entries = args.map<double>("values");
    if (entries.empty()) return fail(command.name, "Give at least one parameter", place);
    std::vector<std::string> names;
    for (const auto &[name, value] : entries) {
        const int index = find_user_name(s.model(), PARAMBOX, name);
        if (index < 0 || index >= s.model().nupar)
            return std::unexpected<Error>(Error{command.name, xpp::format("The model has no parameter \"{}\"", name), place, field_of(name)});
        names.push_back(s.model().upar_names[static_cast<std::size_t>(index)]);
    }
    push_value_undo(s);
    for (std::size_t i = 0; i < entries.size(); ++i) set_val(s, names[i], entries[i].second);
    box_values_loaded(s, PARAMBOX);
    return ResultObject().raw("updated", updated(s, names)).done();
}

Result<std::string> set_by_formula(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &name = args.get<std::string>(NAME_KEY);
    const int index = find_user_name(s.model(), PARAMBOX, name);
    if (index < 0 || index >= s.model().nupar)
        return std::unexpected<Error>(Error{command.name, xpp::format("The model has no parameter \"{}\"", name), place, field_of(name)});
    const Result<double> value = evaluate_formula(s, args.get<std::string>("formula"));
    if (!value) return std::unexpected<Error>(Error{command.name, value.error().what, place, field_of(name)});
    const std::string &parameter = s.model().upar_names[static_cast<std::size_t>(index)];
    push_value_undo(s);
    set_val(s, parameter, *value);
    box_values_loaded(s, PARAMBOX);
    return ResultObject().text(NAME_KEY, parameter).real(VALUE_KEY, parameter_now(s, parameter)).done();
}

Result<std::string> undo(Session &s, const Command &, const Arguments &, const Place &) { return performed(undo_value_edit(s), "undone"); }

Result<std::string> redo(Session &s, const Command &, const Arguments &, const Place &) { return performed(redo_value_edit(s), "redone"); }

Result<std::string> slide(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &name = args.get<std::string>(NAME_KEY);
    int type = 0, index = 0;
    if (!find_par_or_var(s.model(), name, &type, &index))
        return fail(command.name, xpp::format("The model has no parameter or variable \"{}\"", name), place);
    const double value = args.get<double>(VALUE_KEY);
    push_value_undo(s, name);
    set_par_or_var(s, name, type, index, value);
    return ResultObject().text(NAME_KEY, name).real(VALUE_KEY, value).done();
}

Result<std::string> configure_slider(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::size_t slot = static_cast<std::size_t>(args.get<long long>(SLOT_KEY));
    if (slot > s.sliders.size())
        return fail(command.name, xpp::format("Slot {}: the sliders are in slots 0 to {}, and the next one is {}", slot, s.sliders.size() ? s.sliders.size() - 1 : 0, s.sliders.size()), place);
    XppSlider slider;
    slider.var = args.get<std::string>(NAME_KEY);
    slider.lo = args.get<double>("low");
    slider.hi = args.get<double>("high");
    slider.step = args.optional<double>("step").value_or(slot < s.sliders.size() ? s.sliders[slot].step : 0.0);
    if (const auto why = slider_wrong(s.model(), slider)) return fail(command.name, *why, place);
    if (slot == s.sliders.size()) s.sliders.push_back(std::move(slider));
    else s.sliders[slot] = std::move(slider);
    return ResultObject().integer(SLOT_KEY, static_cast<long long>(slot)).boolean("configured", true).done();
}

Result<std::string> remove_slider(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::size_t slot = static_cast<std::size_t>(args.get<long long>(SLOT_KEY));
    if (slot >= s.sliders.size())
        return fail(command.name, xpp::format("Slot {}: the model has {} sliders", slot, s.sliders.size()), place);
    s.sliders[slot] = XppSlider{};
    return ResultObject().integer(SLOT_KEY, static_cast<long long>(slot)).boolean("removed", true).done();
}

/* the values file `kind` names */
ValueFile value_file_of(const Arguments &args)
{
    return args.get<std::string>(KIND_KEY) == value_file_names[0] ? ValueFile::parameters : ValueFile::initial_conditions;
}

Result<std::string> save_file(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<ValueFileSaved> saved = save_value_file(s, value_file_of(args), args.optional<std::string>(FILE_KEY).value_or(std::string{}));
    if (!saved) return std::unexpected<Error>(saved.error());
    if (!saved->file.empty() && !saved->written)
        return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", saved->file), place);
    return ResultObject().text(FILE_KEY, saved->file).text(KIND_KEY, args.get<std::string>(KIND_KEY)).done();
}

Result<std::string> load_file(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<std::string> read = load_value_file(s, value_file_of(args), args.optional<std::string>(FILE_KEY).value_or(std::string{}));
    if (!read) return std::unexpected<Error>(read.error());
    return ResultObject().text(FILE_KEY, *read).text(KIND_KEY, args.get<std::string>(KIND_KEY)).done();
}

constexpr std::array KIND_CHOICES = {"parameters", "initial_conditions"};
static_assert(KIND_CHOICES[0] == value_file_names[0] && KIND_CHOICES[1] == value_file_names[1], "the kinds are the value files' names");

constexpr Argument SLOT = {SLOT_KEY, Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)};
constexpr Argument SLIDER_NAME = {NAME_KEY, Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either};
constexpr Argument KIND = {KIND_KEY, Type::choice, true, MAX_TEXT_BYTES, KIND_CHOICES};
/* the file is the model folder's, or asked for (the desktop window's own dialog) when it is left out */
constexpr Argument FILE = {FILE_KEY, Type::file, false};

constexpr std::array PARAMETERS_ARGUMENTS = {Argument{"values", Type::map, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter, Type::real}};
constexpr std::array FORMULA_ARGUMENTS = {Argument{NAME_KEY, Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter},
                                          Argument{"formula", Type::expression, true}};
constexpr std::array SLIDE_ARGUMENTS = {SLIDER_NAME, Argument{VALUE_KEY, Type::real, true}};
constexpr std::array SLIDER_ARGUMENTS = {SLOT, SLIDER_NAME, Argument{"low", Type::real, true}, Argument{"high", Type::real, true},
                                         Argument{"step", Type::real, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array REMOVE_SLIDER_ARGUMENTS = {SLOT};
constexpr std::array FILE_ARGUMENTS = {KIND, FILE};

constexpr std::array PARAMETERS_RESULTS = {ResultField{"updated", "map[name, real]"}};
constexpr std::array FORMULA_RESULTS = {ResultField{NAME_KEY, "name"}, ResultField{VALUE_KEY, "real"}};
constexpr std::array UNDO_RESULTS = {ResultField{"undone", "boolean"}};
constexpr std::array REDO_RESULTS = {ResultField{"redone", "boolean"}};
constexpr std::array SLIDE_RESULTS = {ResultField{NAME_KEY, "name"}, ResultField{VALUE_KEY, "real"}};
constexpr std::array SLIDER_RESULTS = {ResultField{SLOT_KEY, "integer"}, ResultField{"configured", "boolean"}};
constexpr std::array REMOVE_SLIDER_RESULTS = {ResultField{SLOT_KEY, "integer"}, ResultField{"removed", "boolean"}};
constexpr std::array FILE_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{KIND_KEY, "choice"}};

constexpr std::array STATE_EVENTS = {"state", "done"};

constexpr std::array UNDO_KEYS = {"Ctrl+Z"};
constexpr std::array REDO_KEYS = {"Ctrl+Shift+Z"};
constexpr std::array ROWS = {
    Command{"value.set_parameters", PARAMETERS_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Data/Parameters",
            "Set the values of some parameters; the others keep theirs. Runs nothing.", PARAMETERS_RESULTS, STATE_EVENTS, true,
            set_parameters},
    Command{"value.set_by_formula", FORMULA_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Data/Parameter formula",
            "Set a parameter to what a formula works out to now (once: the parameter keeps the number, not the formula).", FORMULA_RESULTS,
            STATE_EVENTS, true, set_by_formula},
    Command{"value.undo", {}, XPP_KIND_SETTING, CommandCategory::Run, "Data/Undo value edit",
            "Take back the last edit of the parameters, initial conditions or numerics (not runs, plots or files).", UNDO_RESULTS, STATE_EVENTS,
            true, undo, {}, UNDO_KEYS},
    Command{"value.redo", {}, XPP_KIND_SETTING, CommandCategory::Run, "Data/Redo value edit",
            "Put back the value edit that was undone.", REDO_RESULTS, STATE_EVENTS, true, redo, {}, REDO_KEYS},
    Command{"value.slide", SLIDE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Data/Values/Slide",
            "Set a parameter or a state's initial value to a number, as a slider does; moves of one slider in a row are one undo step.",
            SLIDE_RESULTS, STATE_EVENTS, true, slide},
    Command{"value.configure_slider", SLIDER_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Data/Values/Configure slider",
            "Define a slider: its slot (an existing one, or the next), the parameter or state it moves, its range, and its step (0: the page picks one; left out: the slot's own).",
            SLIDER_RESULTS, STATE_EVENTS, true, configure_slider},
    Command{"value.remove_slider", REMOVE_SLIDER_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Data/Values/Remove slider",
            "Empty a slider's slot.", REMOVE_SLIDER_RESULTS, STATE_EVENTS, true, remove_slider},
    Command{"value.save_file", FILE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Values/Save file",
            "Write the parameters (.par) or the initial conditions (.ic) as XPP's own file into the model's folder; with no file, ask for one (the result's file is empty when that question was cancelled).",
            FILE_RESULTS, STATE_EVENTS, true, save_file, {}, {}, "write", "*"},
    Command{"value.load_file", FILE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "File/Values/Load file",
            "Read the parameters (.par) or the initial conditions (.ic) from XPP's own file of the model's folder, all or nothing; with no file, ask for one (the result's file is empty when that question was cancelled).",
            FILE_RESULTS, STATE_EVENTS, true, load_file}};

} // namespace

std::span<const Command> value_commands() { return ROWS; }

} // namespace xpp::command
