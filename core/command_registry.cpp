#include "command_registry.h"
#include "command_table.h"
#include "json_reader.h"
#include "json_error.h"
#include "menus.h"
#include "session.h"
#include "many_pops.h"
#include "auto_nox.h"
#include "xpp_ui.h"
#include "xpp_io.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>

namespace xpp::command {
namespace {

/* Registry requests describe small operations; bulk data uses /files. */
constexpr std::size_t MAX_COMMAND_BYTES = 16384;
/* Room for descriptive command names and opaque client correlation ids. */
constexpr std::size_t MAX_TEXT_BYTES = 1024;
constexpr const char *COMMAND_KEY = "cmd";
constexpr const char *ID_KEY = "id";

bool valid_id(const char *value)
{
    std::string text;
    return json_decode_string(value, text, std::string::npos, true) && text.size() <= MAX_TEXT_BYTES;
}

enum class Type { text, window, new_name, boolean, choice };
struct Argument {
    const char *name;
    Type type;
    bool required;
    std::size_t max_length = MAX_TEXT_BYTES;
    std::span<const char *const> choices = {};
};
struct ResultField {
    const char *name;
    const char *type;
};
enum class WindowAction { create, rename, select, close, close_all, bottom, redraw_mode, simultaneous };
struct Command {
    const char *name;
    std::span<const Argument> arguments;
    char kind;
    CommandCategory category;
    const char *menu_path;
    const char *legacy_keys;
    const char *description;
    std::span<const ResultField> results;
    std::span<const char *const> events;
    std::optional<WindowAction> action;
    bool step;
    Result<std::string> (*execute)(Session &, const Command &, const char *, const Place &);
};

Result<std::string> describe(Session &, const Command &command, const char *line, const Place &place);
Result<std::string> window_command(Session &, const Command &, const char *, const Place &);

constexpr std::array HELP_ARGUMENTS = {Argument{"name", Type::text, false}};
constexpr std::array HELP_RESULTS = {ResultField{"commands", "list[command]"}};
constexpr std::array HELP_EVENTS = {"state","done"};
constexpr std::array CREATE_ARGUMENTS = {Argument{"name",Type::new_name,false}};
constexpr std::array RENAME_ARGUMENTS = {Argument{"window",Type::window,true},Argument{"name",Type::new_name,true}};
constexpr std::array SELECT_ARGUMENTS = {Argument{"window",Type::window,true}};
constexpr std::array WINDOW_ARGUMENTS = {Argument{"window",Type::window,false}};
constexpr std::array REDRAW_CHOICES = {"auto","manual"};
constexpr std::array MODE_ARGUMENTS = {Argument{"window",Type::window,false},Argument{"mode",Type::choice,true,MAX_TEXT_BYTES,REDRAW_CHOICES}};
constexpr std::array SIMULTANEOUS_ARGUMENTS = {Argument{"enabled",Type::boolean,true}};
constexpr std::array WINDOW_RESULTS = {ResultField{"window","window"}};
constexpr std::array RENAME_RESULTS = {ResultField{"window","window"},ResultField{"new_name","text"}};
constexpr std::array CLOSE_RESULTS = {ResultField{"window","window"},ResultField{"closed","boolean"}};
constexpr std::array CLOSE_ALL_RESULTS = {ResultField{"closed_count","integer"}};
constexpr std::array MODE_RESULTS = {ResultField{"redraw_mode","choice"}};
constexpr std::array SIMULTANEOUS_RESULTS = {ResultField{"simultaneous_plot","boolean"}};
constexpr std::array WINDOW_EVENTS = {"window","state","done"};
constexpr std::array RENAME_EVENTS = {"window.renamed","window","state","done"};
constexpr std::array STATE_EVENTS = {"state","done"};
constexpr std::array REGISTRY = {
    Command{"help",HELP_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Tools,"","",
            "Describe registered commands, or the command named by name.",HELP_RESULTS,HELP_EVENTS,std::nullopt,false,describe},
    Command{"window.create",CREATE_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"Make window/Create","MC",
            "Create a graph copied from the active graph.",WINDOW_RESULTS,WINDOW_EVENTS,WindowAction::create,true,window_command},
    Command{"window.rename",RENAME_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"","",
            "Rename an open graph or diagram view.",RENAME_RESULTS,RENAME_EVENTS,WindowAction::rename,true,window_command},
    Command{"window.select",SELECT_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"","",
            "Select an open graph or diagram view.",WINDOW_RESULTS,STATE_EVENTS,WindowAction::select,true,window_command},
    Command{"window.close",WINDOW_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"Make window/Destroy","MD",
            "Close a graph or diagram view; the main graph stays open.",CLOSE_RESULTS,WINDOW_EVENTS,WindowAction::close,true,window_command},
    Command{"window.destroy_all",{},XPP_KIND_VIEW,CommandCategory::Plot,"Make window/Kill all","MK",
            "Close every extra graph.",CLOSE_ALL_RESULTS,WINDOW_EVENTS,WindowAction::close_all,true,window_command},
    Command{"window.send_to_bottom",WINDOW_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"Make window/Bottom","MB",
            "Send a graph to the bottom.",WINDOW_RESULTS,STATE_EVENTS,WindowAction::bottom,true,window_command},
    Command{"window.set_redraw_mode",MODE_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"Make window/Auto or Manual","MA/MM",
            "Choose automatic or manual graph redrawing.",MODE_RESULTS,STATE_EVENTS,WindowAction::redraw_mode,true,window_command},
    Command{"window.set_simultaneous_plot",SIMULTANEOUS_ARGUMENTS,XPP_KIND_VIEW,CommandCategory::Plot,"Make window/SimPlot","MS",
            "Draw on every open graph when enabled.",SIMULTANEOUS_RESULTS,STATE_EVENTS,WindowAction::simultaneous,true,window_command}
};

constexpr const char *type_name(Type type)
{
    switch(type) {
    case Type::text:return "text";
    case Type::window:return "window";
    case Type::new_name:return "new_name";
    case Type::boolean:return "boolean";
    case Type::choice:return "choice";
    }
    return "";
}

const Command *find(std::string_view name)
{
    const auto found = std::ranges::find(REGISTRY, name, &Command::name);
    return found == REGISTRY.end() ? nullptr : &*found;
}

void append_command(std::string &out, const Command &command)
{
    out += "{\"cmd\":";
    json_append_string(out, command.name);
    out += xpp::format(",\"kind\":\"{}\",\"step\":{},\"completion\":\"done\",\"max_input_bytes\":{},\"category\":", command.kind, command.step, MAX_COMMAND_BYTES);
    json_append_string(out, category_id(command.category));
    out += ",\"menu_path\":";
    json_append_string(out, command.menu_path);
    out += ",\"legacy_keys\":";
    json_append_string(out, command.legacy_keys);
    out += ",\"description\":";
    json_append_string(out, command.description);
    out += ",\"arguments\":[";
    for (std::size_t i = 0; i < command.arguments.size(); ++i) {
        if (i) out += ',';
        const Argument &arg = command.arguments[i];
        out += "{\"name\":";
        json_append_string(out, arg.name);
        out += ",\"type\":";
        json_append_string(out,type_name(arg.type));
        out += xpp::format(",\"required\":{},\"max_length\":{},\"choices\":[",arg.required,arg.max_length);
        for (std::size_t j=0;j<arg.choices.size();++j) { if(j)out+=',';json_append_string(out,arg.choices[j]); }
        out += "]}";
    }
    out += "],\"result\":[";
    for (std::size_t i = 0; i < command.results.size(); ++i) {
        if (i) out += ',';
        out += "{\"name\":";
        json_append_string(out, command.results[i].name);
        out += ",\"type\":";
        json_append_string(out, command.results[i].type);
        out += '}';
    }
    out += "],\"events\":[";
    for (std::size_t i = 0; i < command.events.size(); ++i) {
        if (i) out += ',';
        json_append_string(out, command.events[i]);
    }
    out += "]}";
}

Result<std::string> describe(Session &, const Command &command, const char *line, const Place &place)
{
    std::string selected;
    const bool one = json::js_string(json::js_find(line, command.arguments.front().name), selected);
    const Command *target = one ? find(selected) : nullptr;
    if (one && !target) return fail(command.name, xpp::format("Unknown registry command \"{}\"", selected), place);
    std::string result = "{";
    json_append_string(result, command.results.front().name);
    result += ":[";
    if (target) append_command(result, *target);
    else {
        for (std::size_t i = 0; i < REGISTRY.size(); ++i) {
            if (i) result += ',';
            append_command(result, REGISTRY[i]);
        }
    }
    result += "]}";
    return result;
}

Result<std::string> window_command(Session &s,const Command &command,const char *line,const Place &place)
{
    std::string name=s.selected_window,new_name;
    if(const char *value=json::js_find(line,"window"))json::js_string(value,name);
    const auto target=find_window(s,name);
    if(command.action==WindowAction::create) {
        json::js_string(json::js_find(line,"name"),new_name);
        create_a_pop(s,new_name);
        name=s.plot_windows.current->name;
    } else if(command.action==WindowAction::close_all) {
        const int count=s.plot_windows.count-1;
        kill_all_pops(s);
        return xpp::format("{{\"closed_count\":{}}}",count);
    } else if(command.action==WindowAction::simultaneous) {
        bool enabled=false;
        parse_bool(json::js_raw(json::js_find(line,"enabled")),enabled);
        s.plot_windows.simul=enabled;
        return xpp::format("{{\"simultaneous_plot\":{}}}",enabled);
    } else {
        if(!target)return fail(command.name,xpp::format("Unknown window \"{}\"",name),place);
        if(command.action==WindowAction::rename) {
            json::js_string(json::js_find(line,"name"),new_name);
            if(target->kind==WindowKind::graph)s.plot_windows.graph[target->index].name=new_name;
            else s.auto_state.views[target->index].name=new_name;
            ui.window_renamed(s,name,new_name);
            if(s.selected_window==name)s.selected_window=new_name;
        } else if(command.action==WindowAction::select) {
            if(target->kind==WindowKind::graph)make_active(s,target->index,1);
            else auto_activate_view(s,target->index);
        } else if(command.action==WindowAction::close) {
            if(target->kind==WindowKind::graph) {
                if(target->index==0)return fail(command.name,"The main graph stays open",place);
                make_active(s,target->index,1);destroy_a_pop(s);
            } else if(!auto_close_view(s,target->index)) {
                s.auto_state.bifur.exist=0;
                s.auto_view = AutoView();
                for (AutoDiagramView &view : s.auto_state.views) view.zoom = Zoom();
                ui.auto_closed(s,name);
                s.selected_window=s.plot_windows.graph[s.plot_windows.active].name;
            } else s.selected_window=s.auto_state.views[s.auto_state.active_view].name;
        } else {
            if(target->kind!=WindowKind::graph)return fail(command.name,"This setting applies to a graph",place);
            if(command.action==WindowAction::bottom)ui.lower_plot_window();
            else {
                std::string mode;json::js_string(json::js_find(line,"mode"),mode);
                s.plot_windows.graph[target->index].Nullrestore=mode=="auto";
                return std::string("{\"redraw_mode\":\"")+mode+"\"}";
            }
        }
    }
    std::string result="{\"window\":";json_append_string(result,name);
    if(command.action==WindowAction::rename){result+=",\"new_name\":";json_append_string(result,new_name);}
    if(command.action==WindowAction::close)result+=",\"closed\":true";
    return result+'}';
}

Result<> validate(const Session *session, const Command &command, const char *line, const Place &place)
{
    const char *wrong_at = nullptr;
    if (const char *why = json::js_problem(line, wrong_at))
        return fail(command.name, xpp::format("Command {} at character {}", why, wrong_at - line + 1), place);
    const char *cursor = line, *value = nullptr;
    std::string key;
    std::vector<std::string> seen;
    while (json::js_member(cursor, key, value)) {
        if (std::ranges::find(seen, key) != seen.end())
            return fail(command.name, xpp::format("Repeated argument \"{}\"", key), place);
        seen.push_back(key);
        if (key == COMMAND_KEY) continue;
        if (key == ID_KEY) {
            if (valid_id(value)) continue;
            return fail(command.name, "id must be text of at most 1024 bytes", place);
        }
        const auto arg = std::ranges::find(command.arguments, key, &Argument::name);
        if (arg == command.arguments.end())
            return fail(command.name, xpp::format("Unknown argument \"{}\"", key), place);
        std::string text;
        if (arg->type==Type::boolean) {
            bool enabled;
            if (!parse_bool(json::js_raw(value),enabled)) return fail(command.name,xpp::format("Argument \"{}\" must be boolean",key),place);
            continue;
        }
        if (!json_decode_string(value,text,std::string::npos,true) || text.size()>arg->max_length)
            return fail(command.name,xpp::format("Argument \"{}\" must be text of at most {} bytes",key,arg->max_length),place);
        if (arg->type==Type::window && session && !find_window(*session,text))
            return fail(command.name,xpp::format("Unknown window \"{}\"",text),place);
        if (arg->type==Type::choice && std::ranges::find(arg->choices,text)==arg->choices.end())
            return fail(command.name,xpp::format("Invalid choice \"{}\" for {}",text,key),place);
        if (arg->type==Type::new_name) {
            std::string replacing;
            json::js_string(json::js_find(line,"window"),replacing);
            if (const std::string problem=session?window_name_problem(*session,text,replacing):window_name_problem(text);!problem.empty()) return fail(command.name,problem,place);
        }
    }
    for (const Argument &arg : command.arguments)
        if (arg.required && !json::js_find(line, arg.name))
            return fail(command.name, xpp::format("Missing argument \"{}\"", arg.name), place);
    return {};
}

} // namespace

std::string_view name(const char *line)
{
    line = json::skip_ws(line);
    if (*line == '{') {
        std::string word;
        if (json::js_string(json::js_find(line, COMMAND_KEY), word))
            if (const Command *command = find(word)) return command->name;
        return {};
    }
    const char *end = line;
    while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') ++end;
    if (const Command *command = find({line, static_cast<std::size_t>(end - line)})) return command->name;
    return {};
}

bool is_step(const char *line)
{
    const Command *command=find(name(line));
    return command && command->step;
}

char kind(const char *line)
{
    const Command *command = find(name(line));
    return command ? command->kind : 0;
}

void append_commands(std::string &out)
{
    for (const Command &command : REGISTRY) {
        out += ',';
        append_command(out, command);
    }
}

Result<std::string> tokenize(const char *line, const Place &place)
{
    if (std::strlen(line) > MAX_COMMAND_BYTES)
        return fail("command", xpp::format("Command exceeds {} bytes", MAX_COMMAND_BYTES), place);
    const char *cursor = json::skip_ws(line), *end = cursor;
    while (*end && *end != ' ' && *end != '\t') ++end;
    const Command *command = find({cursor, static_cast<std::size_t>(end - cursor)});
    if (!command) return fail("command", "Unknown registry command", place);
    std::string out = "{";
    json_append_string(out, COMMAND_KEY);
    out += ':';
    json_append_string(out, command->name);
    cursor = json::skip_ws(end);
    while (*cursor) {
        std::string key;
        const bool named = cursor[0] == '-' && cursor[1] == '-';
        if (named) {
            cursor += 2;
            end = cursor;
            while (*end && *end != ' ' && *end != '\t') ++end;
            key.assign(cursor, end);
            cursor = json::skip_ws(end);
        } else {
            /* help NAME is the explicitly specified meta-command syntax;
               ordinary resource commands will accept named arguments only. */
            if (command->execute != describe) return fail(command->name, "Arguments must be named", place);
            key = command->arguments.front().name;
        }
        if (!*cursor) return fail(command->name, xpp::format("Missing value for \"{}\"", key), place);
        out += ',';
        json_append_string(out, key);
        out += ':';
        if (*cursor == '"') {
            const auto arg=std::ranges::find(command->arguments,key,&Argument::name);
            if(arg!=command->arguments.end() && (arg->type==Type::choice || arg->type==Type::boolean))
                return fail(command->name,"Choices and booleans must be bare",place);
            end = json::skip_value(cursor);
            std::string value;
            const std::string token(cursor, end);
            if (!json::js_valid(token.c_str()) || !json_decode_string(token.c_str(), value, std::string::npos, true))
                return fail(command->name, "Invalid quoted text", place);
            out += token;
        } else {
            end = cursor;
            while (*end && *end != ' ' && *end != '\t') ++end;
            /* A command name is a keyword, so help NAME may be bare.
               Named text arguments and client ids are always quoted. */
            if (!named && find({cursor, static_cast<std::size_t>(end - cursor)}))
                json_append_string(out, std::string_view(cursor, static_cast<std::size_t>(end - cursor)));
            else {
                const auto arg=std::ranges::find(command->arguments,key,&Argument::name);
                if(arg==command->arguments.end() || (arg->type!=Type::choice && arg->type!=Type::boolean))
                    return fail(command->name,"Text must be double-quoted",place);
                const std::string word(cursor,end);
                if(arg->type==Type::choice)json_append_string(out,word);else out+=word;
            }

        }
        if (*end && *end != ' ' && *end != '\t') return fail(command->name, "Expected whitespace after value", place);
        cursor = json::skip_ws(end);
    }
    out += '}';
    return out;
}

Result<> window_button(Session &s,int button,const Place &place)
{
    // XPPAUT's imported button sequences MC, MK, MD, MB, MA, MM, MS.
    WindowAction action=WindowAction::create;
    switch(button){
    case 0:action=WindowAction::create;break;
    case 1:action=WindowAction::close_all;break;
    case 2:action=WindowAction::close;break;
    case 3:action=WindowAction::bottom;break;
    case 4:case 5:action=WindowAction::redraw_mode;break;
    case 6:action=WindowAction::simultaneous;break;
    default:return fail("window button","Unknown imported window button",place);
    }
    const auto command=std::ranges::find(REGISTRY,std::optional<WindowAction>(action),&Command::action);
    std::string line="{\"cmd\":";json_append_string(line,command->name);
    if(action==WindowAction::redraw_mode)line+=button==4?",\"mode\":\"auto\"":",\"mode\":\"manual\"";
    if(action==WindowAction::simultaneous)line+=s.plot_windows.simul?",\"enabled\":false":",\"enabled\":true";
    line+='}';
    const Result<std::string> result=run(s,line.c_str(),place);
    if(!result)return std::unexpected<Error>(result.error());
    return {};
}

Result<> check_shape(const char *line,const Place &place)
{
    if(std::strlen(line)>MAX_COMMAND_BYTES)return fail("command","Command exceeds the input limit",place);
    const Command *command=find(name(line));
    if(!command)return fail("command","Unknown registry command",place);
    return validate(nullptr,*command,line,place);
}

Result<std::string> run(Session &s, const char *line, const Place &place)
{
    if (std::strlen(line) > MAX_COMMAND_BYTES)
        return fail("command", xpp::format("Command exceeds {} bytes", MAX_COMMAND_BYTES), place);
    const Command *command = find(name(line));
    if (!command) return fail("command", "Unknown registry command", place);
    if (const Result<> checked = validate(&s, *command, line, place); !checked)
        return std::unexpected<Error>(checked.error());
    return command->execute(s, *command, line, place);
}

std::string done(const char *line, const Result<std::string> &result, bool stopped)
{
    std::string out = "{\"ev\":\"done\"";
    const char *wrong_at = nullptr;
    if (!json::js_problem(line, wrong_at)) {
        if (const char *id = json::js_find(line, ID_KEY); id && valid_id(id)) {
            out += ',';
            json_append_string(out, ID_KEY);
            out += ':';
            std::string text;
            json::js_string(id, text);
            json_append_string(out, text);
        }
    }
    if (!result) {
        out += ",\"status\":\"error\"";
        json::append_error(out, result.error());
    } else if (stopped) out += ",\"status\":\"stopped\"";
    else out += ",\"status\":\"ok\",\"result\":" + *result;
    out += '}';
    return out;
}

} // namespace xpp::command
