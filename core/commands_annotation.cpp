/* The annotation resource's registry rows (W523): list, add, edit, move and delete the texts, arrows,
   pointers and markers of the graphs, by id, in plot coordinates (docs/protocol.md "Annotations"). The
   page's text dialog and the Text,etc menu's Edit go through the same core functions (grobs.h). */
#include "command_def.h"
#include "grobs.h"
#include "json_error.h"
#include "many_pops.h"
#include "marks_data.h"
#include "session.h"
#include "xpp_io.h"
#include <array>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window", *ID_ARGUMENT = "annotation_id", *COLOR_KEY = "color", *SIZE_KEY = "size";

Result<XppWinId> graph_handle(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = args.optional<std::string>(WINDOW_KEY).value_or(s.selected_window);
    const auto target = find_window(s, name);
    if (!target || target->kind != WindowKind::graph) return fail(command.name, xpp::format("Unknown graph \"{}\"", name), place);
    return s.plot_windows.graph[static_cast<std::size_t>(target->index)].w;
}

Result<std::string> full(const Command &command, const char *what, const Place &place)
{
    return fail(command.name, xpp::format("a session has at most {} {}", ANNOTATION_LIST_MAX, what), place);
}

std::string id_result(int id, const char *field, bool value)
{
    return xpp::format("{{\"id\":{},\"{}\":{}}}", id, field, value);
}

/* a text's look: the given members over look, checked as the ask and the session file check it (a text's
   size is a whole number of the real `size`, which an object's size shares) */
Result<LabelLook> look_of(const Command &command, const Arguments &args, LabelLook look, const Place &place)
{
    if (const auto text = args.optional<std::string>("text")) look.text = *text;
    if (const auto size = args.optional<double>(SIZE_KEY)) {
        look.size = static_cast<int>(*size);
        if (look.size != *size) return fail(command.name, xpp::format("the size of a text is a whole number, not {}", xpp::number(*size)), place);
    }
    if (const auto style = args.optional<long long>("style")) look.style = static_cast<int>(*style);
    if (const auto color = args.optional<Color>(COLOR_KEY)) look.color = *color;
    if (const std::string problem = label_problem(look); !problem.empty()) return fail(command.name, problem, place);
    return look;
}

Result<std::string> list(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<XppWinId> w = graph_handle(s, command, args, place);
    if (!w) return std::unexpected<Error>(w.error());
    return annotations_json(s, *w);
}

Result<std::string> add_text(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<XppWinId> w = graph_handle(s, command, args, place);
    if (!w) return std::unexpected<Error>(w.error());
    Result<LabelLook> look = look_of(command, args, {}, place);
    if (!look) return std::unexpected<Error>(look.error());
    const int id = add_label(s, std::move(*look), *w, static_cast<float>(args.get<double>("x")), static_cast<float>(args.get<double>("y")));
    return id < 0 ? full(command, "texts", place) : Result<std::string>(xpp::format("{{\"id\":{}}}", id));
}

Result<std::string> add_head(Session &s, const Command &command, const Arguments &args, const Place &place, int type)
{
    const Result<XppWinId> w = graph_handle(s, command, args, place);
    if (!w) return std::unexpected<Error>(w.error());
    const double tip_x = args.get<double>("tip_x"), tip_y = args.get<double>("tip_y");
    const double tail_x = args.get<double>("tail_x"), tail_y = args.get<double>("tail_y");
    if (tip_x == tail_x && tip_y == tail_y) return fail(command.name, "the tip and the tail are the same point", place);
    const int id = add_grob(s, *w, tip_x, tip_y, tail_x, tail_y, args.optional<double>(SIZE_KEY).value_or(ARROW_SIZE_DEFAULT), type,
                            args.optional<Color>(COLOR_KEY).value_or(foreground_color));
    return id < 0 ? full(command, "arrows, pointers and markers", place) : Result<std::string>(xpp::format("{{\"id\":{}}}", id));
}

Result<std::string> add_arrow(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return add_head(s, command, args, place, ARROW);
}

Result<std::string> add_pointer(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return add_head(s, command, args, place, POINTER);
}

int marker_type(const std::string &shape)
{
    return MARKER + static_cast<int>(std::ranges::find(marker_shape_names, shape) - marker_shape_names.begin());
}

Result<std::string> add_marker_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<XppWinId> w = graph_handle(s, command, args, place);
    if (!w) return std::unexpected<Error>(w.error());
    const int id = add_grob(s, *w, args.get<double>("x"), args.get<double>("y"), 0, 0, args.optional<double>(SIZE_KEY).value_or(MARKER_SIZE_DEFAULT),
                            marker_type(args.get<std::string>("shape")), args.optional<Color>(COLOR_KEY).value_or(foreground_color));
    return id < 0 ? full(command, "arrows, pointers and markers", place) : Result<std::string>(xpp::format("{{\"id\":{}}}", id));
}

Result<std::string> no_annotation(const Command &command, int id, const Place &place)
{
    return fail(command.name, xpp::format("there is no annotation with that id ({})", id), place);
}

Result<std::string> edit(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const int id = static_cast<int>(args.get<long long>(ID_ARGUMENT));
    if (const LABEL *label = find_annotation(s.labels, id)) {
        if (args.has("shape")) return fail(command.name, "a shape is for a marker", place);
        Result<LabelLook> look = look_of(command, args, {label->s, label->size, label->style, label->color}, place);
        if (!look) return std::unexpected<Error>(look.error());
        edit_label(s, id, std::move(*look));
        return id_result(id, "updated", true);
    }
    const GROB *grob = find_annotation(s.grobs, id);
    if (!grob) return no_annotation(command, id, place);
    if (args.has("text") || args.has("style")) return fail(command.name, "a text and a style are for a text", place);
    GrobLook look{grob->type, grob->size, grob->color};
    if (const auto shape = args.optional<std::string>("shape")) {
        if (grob->type < MARKER) return fail(command.name, "a shape is for a marker", place);
        look.type = marker_type(*shape);
    }
    if (const auto size = args.optional<double>(SIZE_KEY)) look.size = *size;
    if (const auto color = args.optional<Color>(COLOR_KEY)) look.color = *color;
    edit_grob(s, id, look);
    return id_result(id, "updated", true);
}

Result<std::string> move(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const int id = static_cast<int>(args.get<long long>(ID_ARGUMENT));
    if (!move_annotation(s, id, args.get<double>("new_x"), args.get<double>("new_y"))) return no_annotation(command, id, place);
    return id_result(id, "moved", true);
}

Result<std::string> delete_one(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const int id = static_cast<int>(args.get<long long>(ID_ARGUMENT));
    if (!delete_annotation(s, id)) return no_annotation(command, id, place);
    return id_result(id, "deleted", true);
}

Result<std::string> delete_all_annotations(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<XppWinId> w = graph_handle(s, command, args, place);
    if (!w) return std::unexpected<Error>(w.error());
    destroy_labels_and_grobs(s, *w);
    return "{\"cleared\":true}";
}

constexpr Argument WINDOW = {WINDOW_KEY, Type::window, false};
constexpr Argument ID = {ID_ARGUMENT, Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(ANNOTATION_ID_MAX)};
constexpr Argument COLOR = {COLOR_KEY, Type::color, false};
constexpr Argument OBJECT_SIZE = {SIZE_KEY, Type::real, false, MAX_TEXT_BYTES, {}, constant(0)};
constexpr Argument TEXT_SIZE = {SIZE_KEY, Type::real, false, MAX_TEXT_BYTES, {}, constant(0), constant(LABEL_SIZE_MAX)};
constexpr Argument STYLE = {"style", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(LABEL_STYLE_COUNT - 1)};
constexpr Argument TEXT = {"text", Type::text, true, LABEL_TEXT_MAX};
constexpr std::array SHAPES = {"box", "diamond", "triangle", "plus", "cross", "circle"};
static_assert(SHAPES.size() == MARKER_SHAPE_COUNT);

constexpr std::array NO_WINDOW = {WINDOW};
constexpr std::array ADD_TEXT_ARGUMENTS = {WINDOW, TEXT, Argument{"x", Type::real, true}, Argument{"y", Type::real, true}, TEXT_SIZE, STYLE, COLOR};
constexpr std::array HEAD_ARGUMENTS = {WINDOW, Argument{"tip_x", Type::real, true}, Argument{"tip_y", Type::real, true},
                                       Argument{"tail_x", Type::real, true}, Argument{"tail_y", Type::real, true}, OBJECT_SIZE, COLOR};
constexpr std::array ADD_MARKER_ARGUMENTS = {WINDOW, Argument{"shape", Type::choice, true, MAX_TEXT_BYTES, SHAPES},
                                             Argument{"x", Type::real, true}, Argument{"y", Type::real, true}, OBJECT_SIZE, COLOR};
constexpr std::array EDIT_ARGUMENTS = {ID, Argument{"text", Type::text, false, LABEL_TEXT_MAX}, Argument{"shape", Type::choice, false, MAX_TEXT_BYTES, SHAPES},
                                       Argument{SIZE_KEY, Type::real, false, MAX_TEXT_BYTES, {}, constant(0)}, STYLE, COLOR};
constexpr std::array MOVE_ARGUMENTS = {ID, Argument{"new_x", Type::real, true}, Argument{"new_y", Type::real, true}};
constexpr std::array ID_ARGUMENTS = {ID};
constexpr std::array LIST_RESULTS = {ResultField{"window", "window"}, ResultField{"text", "list of texts"}, ResultField{"arrows", "list of arrows and pointers"},
                                     ResultField{"markers", "list of markers"}};
constexpr std::array ID_RESULT = {ResultField{"id", "integer"}};
constexpr std::array UPDATED_RESULTS = {ResultField{"id", "integer"}, ResultField{"updated", "boolean"}};
constexpr std::array MOVED_RESULTS = {ResultField{"id", "integer"}, ResultField{"moved", "boolean"}};
constexpr std::array DELETED_RESULTS = {ResultField{"id", "integer"}, ResultField{"deleted", "boolean"}};
constexpr std::array CLEARED_RESULTS = {ResultField{"cleared", "boolean"}};
constexpr std::array MARKS_EVENTS = {"marks", "state", "done"};

constexpr std::array ROWS = {
    Command{"annotation.list", NO_WINDOW, XPP_KIND_VIEW, CommandCategory::Plot, "", "", "List the texts, arrows, pointers and markers of a graph.",
            LIST_RESULTS, MARKS_EVENTS, false, list},
    Command{"annotation.add_text", ADD_TEXT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Text", "TT", "Add a text at a point of a graph.",
            ID_RESULT, MARKS_EVENTS, true, add_text},
    Command{"annotation.add_arrow", HEAD_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Arrow", "TA", "Add an arrow head at the tip of a line.",
            ID_RESULT, MARKS_EVENTS, true, add_arrow},
    Command{"annotation.add_pointer", HEAD_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Pointer", "TP", "Add a pointer: a head and a shaft.",
            ID_RESULT, MARKS_EVENTS, true, add_pointer},
    Command{"annotation.add_marker", ADD_MARKER_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Marker", "TM", "Add a marker at a point of a graph.",
            ID_RESULT, MARKS_EVENTS, true, add_marker_command},
    Command{"annotation.edit", EDIT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Edit/Change", "TEC",
            "Change a text (its text, size, style, colour) or an object (its size, colour, a marker's shape).", UPDATED_RESULTS, MARKS_EVENTS, true, edit},
    Command{"annotation.move", MOVE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Edit/Move", "TEM",
            "Move an annotation so that its position (an object's start, its end following) is the point.", MOVED_RESULTS, MARKS_EVENTS, true, move},
    Command{"annotation.delete", ID_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Edit/Delete", "TED", "Delete an annotation.",
            DELETED_RESULTS, MARKS_EVENTS, true, delete_one},
    Command{"annotation.delete_all", NO_WINDOW, XPP_KIND_VIEW, CommandCategory::Plot, "Text,etc/Delete all", "TD",
            "Delete every annotation of a graph.", CLEARED_RESULTS, MARKS_EVENTS, true, delete_all_annotations}};

} // namespace

std::span<const Command> annotation_commands() { return ROWS; }

} // namespace xpp::command
