/* Recording (W59a, docs/protocol.md "Recordings"): File/recorD, or the
   record command, records every step of the session into name.recx
   (recx.h). A step is everything from leaving idle to the next idle, the
   protocol's own contract (every command ends with state, then idle): the
   command handle_line runs, the answers to what it asks (a menu's key,
   a dialog's values), and where it stopped when an Abort or Escape
   cancelled it. The model's files and every text file the session reads
   while recording go in with them, and a note set with record note goes
   above the next step. Idle time is not recorded: nothing here keeps a
   clock. */
#include "ui_json_internal.h"
#include "browse.h"
#include "menus.h"
#include "model.h"
#include "model_files.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_about.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_session.h"

#include <cctype>
#include <chrono>
#include <cstring>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::json {

namespace {

/* the step being taken: its parts as JSON text, joined into its line
   when it ends */
struct StepTaken {
    bool open = false;
    std::string label;                /* for a person: "Initialconds → Go" */
    std::string note;                 /* the note set before it began */
    std::string win;                  /* a window's own key layer ("auto") */
    std::string button;               /* the page's control that sent the key */
    std::string cmd;                  /* a command other than a key, whole */
    std::vector<std::string> keys;    /* the key, then the keys that answered a menu or a choice */
    std::vector<std::string> answers; /* every other answer: a value, a form's values, an object, null for a cancel */
    std::vector<size_t> files;        /* the file sections it read, in order */
    std::vector<std::string> during;  /* the keys the job read itself, each {"key":K,"at":AT} */
    bool view = false;                /* only changes what is shown */
    bool file_menu = false;           /* the key that opens the File menu */
};

struct Recorder {
    std::optional<recx::Recording> rec;
    std::string note; /* for the next step */
    StepTaken step;
    bool file_menu_last = false; /* the last step only opened the File menu */
    bool reading = false;        /* file_read() is reading a file itself */
};

Recorder recorder;

/* a JSON string's text */
std::string json_str(std::string_view s)
{
    Buf b;
    buf_str(&b, s);
    return std::move(b.s);
}

/* the commands that are no step of the user's: the client's own requests
   (state, the data events, the browser's paging, the model's folder),
   the answers and aborts that belong to the step they come in, and the
   recording's own */
bool is_step(const char *line)
{
    static constexpr std::string_view none[] = {"state", "data", "equations", "redraw", "file", "answer", "abort", "quit",
                                                "record", "play"};
    std::string c;
    if (!get_string(line, "cmd", c, 16)) return false;
    for (std::string_view n : none)
        if (c == n) return false;
    return !(c == "browser" && !js_find(line, "op"));
}

/* a command's label, for a person reading the file */
std::string command_label(const char *line)
{
    std::string c, o, name, v;
    get_string(line, "cmd", c, 32);
    get_string(line, "op", o, 32);
    if (c == "set" || c == "slide") {
        if (js_find(line, "values")) return "Set values";
        if (!get_string(line, "name", name)) name = xpp::format("{} {}", get_string(line, "kind", v, 16) ? v : "", get_int(line, "index", 0));
        if (!get_string(line, "text", v)) v = std::string(js_raw(js_find(line, "value")));
        return xpp::format("{} {} = {}", c == "set" ? "Set" : "Slide", name, v);
    }
    if (c == "display") return xpp::format("Zoom window {}", get_int(line, "win", 0));
    if (c == "click") return xpp::format("Window {}", get_int(line, "win", 0));
    if (c == "default") return get_string(line, "kind", v, 16) && v == "ic" ? "Default initial conditions" : "Default parameters";
    std::string label = c;
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    return o.empty() ? label : label + " " + o;
}

/* the label of a key of the main window's menu `menu`, or of window win's layer */
std::string key_label(const std::string &k, int menu, const std::string &win)
{
    const int ch = key_code(k.c_str());
    if (!win.empty()) {
        const XppWindowLayer *l = xpp_window_layer(win.c_str());
        const int i = l ? xpp_menu_index(l->menu, ch) : -1;
        return i < 0 ? xpp::format("{} key {}", win, k) : xpp::format("{} → {}", l->menu->title, xpp_menu_label(l->menu->items[i]));
    }
    const char *item = xpp_main_menu_item(menu, ch);
    if (!item) return "Key " + k;
    const char *in = menu == FILE_MENU ? xpp_main_menu_item(MAIN_MENU, 'f') : menu == NUM_MENU ? xpp_main_menu_item(MAIN_MENU, 'u') : nullptr;
    return in ? xpp_menu_label(in) + " → " + xpp_menu_label(item) : xpp_menu_label(item);
}

/* the step's line, one JSON object */
std::string step_line(const StepTaken &t, const std::string &abort)
{
    std::string j = "{\"step\":" + json_str(t.label);
    if (!t.win.empty()) j += ",\"win\":" + json_str(t.win);
    if (!t.button.empty()) j += ",\"button\":" + json_str(t.button);
    if (!t.cmd.empty()) j += ",\"cmd\":" + t.cmd;
    auto array = [&j](const char *name, const std::vector<std::string> &v) {
        if (v.empty()) return;
        j += xpp::format(",\"{}\":[", name);
        for (size_t i = 0; i < v.size(); i++) j += (i ? "," : "") + v[i];
        j += "]";
    };
    std::vector<std::string> keys;
    for (const std::string &k : t.keys) keys.push_back(json_str(k));
    array("keys", keys);
    array("answers", t.answers);
    std::vector<std::string> files;
    for (size_t f : t.files) files.push_back(xpp::format("{}", f));
    array("files", files);
    array("during", t.during);
    if (t.view) j += ",\"view\":true";
    if (!abort.empty()) j += ",\"abort\":" + abort;
    return j + "}";
}

/* the model's files, as it read them (Model::files), into the recording:
   those of a model loaded while recording too */
void add_model_files(const xpp::Model &m)
{
    for (const xpp::ModelFile &f : m.files) recx::add_file(*recorder.rec, f);
}

/* xpp_files_observe_reads' observer: a file the core opened for reading,
   embedded when it is the user's (not in a scratch folder: text as it
   is, any other file, an .autox or a .snapx, as base64), and named by
   the step that read it */
void file_read(const char *path)
{
    if (!recorder.rec || recorder.reading || xpp_files_is_scratch(path)) return;
    try {
        std::string bytes;
        recorder.reading = true;
        const bool read = xpp::read_bytes(path, bytes);
        recorder.reading = false;
        if (!read) return;
        const size_t section = recx::add_file(*recorder.rec, {path, std::move(bytes)});
        if (recorder.step.open) recorder.step.files.push_back(section);
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("recording a file");
    }
}

/* when a recording begins: 2026-09-30T10:14:02Z */
std::string now_utc()
{
    return xpp::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

void start(xpp::Session &s)
{
    if (recorder.rec) {
        j_err_msg("Already recording: File/recorD again stops and saves the recording");
        return;
    }
    recx::Recording r;
    r.program = xpp::format("xppautX {}", xpp_version_string());
    r.model = s.model().this_file;
    r.recorded = now_utc();
    recorder = Recorder{};
    recorder.rec = std::move(r);
    add_model_files(s.model());
    xpp_files_observe_reads(file_read);
}

/* name.recx (asked for when name is empty, as the other File saves are),
   and the recording ends; a cancelled question keeps it going.
   from_menu: File/recorD, whose opening of the File menu is no step. */
void stop(const xpp::Session &s, const std::string &name, bool from_menu)
{
    if (!recorder.rec) {
        j_err_msg("Not recording");
        return;
    }
    recorder.step.open = false; /* this command is no step */
    std::vector<recx::Step> &steps = recorder.rec->steps;
    if (from_menu && recorder.file_menu_last && !steps.empty()) {
        steps.pop_back();
        recorder.file_menu_last = false;
    }
    std::string file = name;
    if (file.empty()) {
        file = xpp_session_file_name(s.model(),recx::extension);
        ping();
        if (!file_selector("Save recording", file, "*.recx") || file.empty()) return;
    }
    file = xpp::snapx::with_extension(file, recx::extension);
    xpp::Writer w = open_writer_asking(file.c_str());
    if (!w) return;
    if (!w.write(recx::text(*recorder.rec)) || !w.commit()) {
        j_err_msg(xpp::format("Cannot write {}", file).c_str());
        return;
    }
    const std::string done = xpp::format("Recorded {} steps in {}", steps.size(), file);
    xpp::log(XPP_LOG_INFO, "{}\n", done);
    bottom_msg(0, done.c_str());
    xpp_files_observe_reads(nullptr);
    recorder = Recorder{};
}

} // namespace

void record_command(xpp::Session &s, const char *line)
{
    std::string o, text;
    get_string(line, "op", o, 16);
    if (o == "start") {
        start(s);
    } else if (o == "stop") {
        get_string(line, "name", text);
        stop(s, text, false);
    } else if (o == "note") {
        if (!recorder.rec) {
            j_err_msg("Not recording: a note goes with a recording's next step");
            return;
        }
        get_string(line, "text", text);
        recorder.note = text;
    } else {
        j_err_msg(xpp::format("Unknown record op {}", o).c_str());
    }
}

void j_record_toggle(xpp::Session &s)
{
    if (recorder.rec) stop(s, "", true);
    else start(s);
}

void record_begin(const char *line)
{
    if (!recorder.rec || !is_step(line)) return;
    StepTaken t;
    t.open = true;
    t.note = std::move(recorder.note);
    recorder.note.clear();
    const char kind = line_kind(line);
    t.view = kind == XPP_KIND_VIEW;
    if (is_cmd(line, "key")) {
        std::string k;
        get_string(line, "key", k);
        get_string(line, "win", t.win, 16);
        get_string(line, "button", t.button);
        const int menu = session.menu.load(std::memory_order_relaxed);
        t.label = key_label(k, menu, t.win);
        t.file_menu = t.win.empty() && menu == MAIN_MENU && key_code(k.c_str()) == 'f';
        t.keys.push_back(std::move(k));
    } else {
        t.label = command_label(line);
        t.cmd = std::string(js_raw(line));
    }
    recorder.step = std::move(t);
}

void record_answer(const char *kind, const char *answer, bool ok)
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open || strcmp(kind, "alert") == 0) return;
    if (strcmp(kind, "menu") == 0 || strcmp(kind, "choice") == 0) {
        std::string k;
        get_string(answer, "key", k);
        t.keys.push_back(ok && !k.empty() ? k : "Escape");
        return;
    }
    if (!ok) t.answers.emplace_back("null");
    else if (strcmp(kind, "string") == 0) t.answers.emplace_back(js_find(answer, "value") ? js_raw(js_find(answer, "value")) : "\"\"");
    else if (strcmp(kind, "form") == 0) t.answers.emplace_back(js_find(answer, "values") ? js_raw(js_find(answer, "values")) : "[]");
    else t.answers.push_back(js_object_without(answer, {"cmd", "id", "ok"}));
}

void record_menu_pick(const XppMenu *m, int ch)
{
    StepTaken &t = recorder.step;
    const int i = xpp_menu_index(m, ch);
    if (!recorder.rec || !t.open || i < 0) return;
    t.label += " → " + xpp_menu_label(m->items[i]);
    if (m->kinds) t.view = m->kinds[i] == XPP_KIND_VIEW;
}

void record_key_read(const std::string &key)
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open) return;
    Buf b;
    BUF_LIT(&b, "{\"key\":");
    buf_str(&b, key);
    BUF_LIT(&b, ",\"at\":");
    buf_stopped_at(&b);
    BUF_LIT(&b, "}");
    t.during.push_back(std::move(b.s));
}

void record_setting(const char *line)
{
    if (!recorder.rec) return;
    StepTaken t;
    t.label = command_label(line);
    t.cmd = std::string(js_raw(line));
    /* before the step it came in: the value was set before that step
       computed anything */
    std::vector<recx::Step> &steps = recorder.rec->steps;
    steps.push_back({"", step_line(t, "")});
}

void record_end(xpp::Session &s, bool cancelled)
{
    if (!recorder.rec) return;
    add_model_files(s.model());
    StepTaken &t = recorder.step;
    if (!t.open) return;
    t.open = false;
    std::string abort;
    if (cancelled) {
        Buf b;
        buf_stopped_at(&b);
        abort = std::move(b.s);
    }
    recorder.rec->steps.push_back({std::move(t.note), step_line(t, abort)});
    recorder.file_menu_last = t.file_menu && t.keys.size() == 1 && t.answers.empty();
}

void buf_recording(Buf *b)
{
    if (!recorder.rec) return;
    buf_format(b, ",\"recording\":{{\"steps\":{:d},\"note\":", recorder.rec->steps.size());
    buf_str(b, recorder.note);
    BUF_LIT(b, "}");
}

} // namespace xpp::json
