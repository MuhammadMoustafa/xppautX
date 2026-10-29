/* Recording (W59a, docs/protocol.md "Recordings"): File/recorD, or the
   record command, records every step of the session into name.recx
   (recx.h). A step is everything from the command's start to its done, the
   protocol's completion (session.state then done): the
   command handle_line runs, the answers to what it asks (a choice's
   key, a dialog's values), and where it stopped when an Abort or Escape
   cancelled it. The model's files and every text file the session reads
   while recording go in with them, and a note set with record note goes
   above the next step. Idle time is not recorded: nothing here keeps a
   clock. */
#include "ui_json_internal.h"
#include "browse.h"
#include "command_def.h"
#include "command_registry.h"
#include "menus.h"
#include "model.h"
#include "model_files.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_about.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_util.h"
#include "xpp_job.h"
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
    std::string cmd;                  /* a command, including an explicit save decision */
    size_t save_answers = 0;          /* answers present before this save's conditional choice */
    std::vector<std::string> answers; /* every answer, in the order asked: a value, a form's values, an object ({"key":"y"} for a choice), null for a cancel */
    std::vector<size_t> files;        /* the file sections it read, in order */
    std::vector<std::string> during;  /* the keys the job read itself, each {"key":K,"at":AT} */
    bool view = false;                /* only changes what is shown */
};

struct Recorder {
    std::optional<recx::Recording> rec;
    std::string note; /* for the next step */
    StepTaken step;
    bool reading = false;        /* file_read() is reading a file itself */
};

Recorder recorder;

/* a command's label, for a person reading the file */
std::string command_label(const char *line)
{
    std::string c, o, name, v;
    get_string(line, "cmd", c);
    get_string(line, "op", o);
    if (c == "set") {
        if (js_find(line, "values")) return "Set values";
        if (!get_string(line, "name", name)) name = xpp::format("{} {}", get_string(line, "kind", v) ? v : "", get_int(line, "index", 0));
        if (!get_string(line, "text", v)) v = std::string(js_raw(js_find(line, "value")));
        return xpp::format("Set {} = {}", name, v);
    }
    if (c == "value.slide") {
        get_string(line, "name", name);
        return xpp::format("Slide {} = {}", name, js_raw(js_find(line, "value")));
    }
    if (c == "value.set_parameters") {
        const char *cursor = js_find(line, "values"), *item = nullptr;
        std::string key;
        if (cursor && js_member(cursor, key, item)) {
            const std::string_view first = js_raw(item);
            if (!js_member(cursor, name, item)) return xpp::format("Set {} = {}", key, first);
        }
        return "Set values";
    }
    if (c == "window.set_display") {get_string(line,"window",name);return xpp::format("Zoom window {}",name);}
    if (c == "window.clear") return "Clear plot";
    if (c == "window.redraw") return "Redraw plot";
    if (c == "set.apply") return xpp::format("Apply set {} ({})", get_string(line, "name", name) ? name : "", get_string(line, "scope", v) ? v : "");
    if (c == "set.restore_defaults") return xpp::format("Restore defaults of {}{}", get_string(line, "scope", v) ? v : "", get_string(line, "field", name) ? " " + name : "");
    if (c == "set.save_new" || c == "set.save_into") return xpp::format("Save {} values in set {}", get_string(line, "scope", v) ? v : "", get_string(line, "name", name) ? name : "");
    if (c == "set.save_defaults") return xpp::format("Save {} values as defaults", get_string(line, "scope", v) ? v : "");
    if (c == "set.export") return xpp::format("Export {} values as set {}", get_string(line, "scope", v) ? v : "", get_string(line, "name", name) ? name : "");
    if (c == "set.rename") return xpp::format("Rename set {} to {}", get_string(line, "from", name) ? name : "", get_string(line, "to", v) ? v : "");
    if (c == "set.delete") return xpp::format("Delete set {}", get_string(line, "name", name) ? name : "");
    /* a registry command's label is its menu path ("Initialconds → Go") */
    if (const xpp::command::Command *registered = xpp::command::find(xpp::command::name(line)); registered && *registered->menu_path) {
        std::string label = registered->menu_path;
        for (std::size_t at = label.find('/'); at != std::string::npos; at = label.find('/', at + 3)) label.replace(at, 1, " → ");
        return label;
    }
    std::string label = c;
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    return o.empty() ? label : label + " " + o;
}

/* the step's line, one JSON object */
std::string step_line(const StepTaken &t, const std::string &abort)
{
    std::string j = "{\"step\":";
    xpp::json_append_string(j, t.label);
    if (!t.cmd.empty()) j += ",\"cmd\":" + t.cmd;
    auto array = [&j](const char *name, const std::vector<std::string> &v) {
        if (v.empty()) return;
        j += xpp::format(",\"{}\":[", name);
        for (size_t i = 0; i < v.size(); i++) j += (i ? "," : "") + v[i];
        j += "]";
    };
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

/* xpp::files::observe_reads' observer: a file the core opened for reading,
   embedded when it is the user's (not in a scratch folder: text as it
   is, any other file, a .snapx, as base64), and named by
   the step that read it */
void file_read(const std::string &path)
{
    if (!recorder.rec || recorder.reading || xpp::files::is_scratch(path)) return;
    try {
        std::string bytes;
        recorder.reading = true;
        const bool read = xpp::read_bytes(path, bytes);
        recorder.reading = false;
        if (!read) return;
        const size_t section = recx::add_file(*recorder.rec, {path, std::move(bytes)});
        if (recorder.step.open) recorder.step.files.push_back(section);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording a file");
    }
}

xpp::Result<> start(xpp::Session &s)
{
    if (recorder.rec)
        return xpp::fail("recording", "Already recording: File/recorD again stops and saves the recording", xpp::command_place());
    /* the session as it is now, what the replay starts from (W59d) */
    std::optional<std::string> snapshot = xpp_session_snapshot(s);
    if (!snapshot)
        return xpp::fail("recording", "Cannot record: the session's state could not be saved to begin the recording with", xpp::command_place());
    recx::Recording r;
    r.program = xpp::format("xppautX {}", xpp_version_string());
    r.model = s.model().this_file;
    r.recorded = xpp::utc_now();
    r.snapshot = std::move(*snapshot);
    recorder = Recorder{};
    recorder.rec = std::move(r);
    add_model_files(s.model());
    xpp::files::observe_reads(file_read);
    return {};
}

/* a saved recording */
struct Saved {
    std::string file;
    size_t steps;
};

/* name.recx (asked for when name is empty, as the other File saves are), and the recording ends; nothing when a
   question is cancelled or declined, which keeps it going. from_menu: File/recorD or File/Quit, whose opening
   of the File menu is no step. confirm: replace an existing file without asking. */
xpp::Result<std::optional<Saved>> stop(const xpp::Session &s, const std::string &name, bool confirm = false)
{
    if (!recorder.rec) return xpp::fail("recording", "Not recording", xpp::command_place());
    recorder.step.open = false; /* this command is no step */
    std::vector<recx::Step> &steps = recorder.rec->steps;
    std::string file = name;
    if (file.empty()) {
        file = xpp::files::output_name(s.model().this_file,recx::extension);
        ping();
        if (!file_selector("Save recording", file, "*" + std::string(recx::extension)) || file.empty()) return std::nullopt;
    }
    file = xpp::snapx::with_extension(file, recx::extension);
    const xpp::Result<std::string> text = recx::text(*recorder.rec);
    if (!text) return std::unexpected(text.error());
    xpp::Result<> opened;
    xpp::Writer w = confirm ? open_writer_replacing(file, false, &opened) : open_writer_asking(file, false, &opened);
    if (!opened) return std::unexpected(opened.error());
    if (!w) return std::nullopt;
    w.write(*text);
    if (const xpp::Result<> saved = xpp::commit_save(w); !saved) return std::unexpected(saved.error());
    const std::string done = xpp::format("Recorded {} steps in {}", steps.size(), file);
    xpp::log(XPP_LOG_INFO, "{}\n", done);
    bottom_msg(0, done);
    xpp::files::observe_reads(nullptr);
    const Saved saved{file, steps.size()};
    recorder = Recorder{};
    return saved;
}

/* stop for the menus and a quit: whether the recording was saved and ended, the error shown */
bool stop_shown(const xpp::Session &s)
{
    const xpp::Result<std::optional<Saved>> saved = stop(s, "");
    return xpp::ok_or_show(saved) && saved->has_value();
}

} // namespace

xpp::Result<std::string> recording_start(xpp::Session &s)
{
    if (const xpp::Result<> started = start(s); !started) return std::unexpected(started.error());
    return std::string("{\"recording\":true}");
}

xpp::Result<std::string> recording_stop(const xpp::Session &s, const std::optional<std::string> &file, bool confirm)
{
    const xpp::Result<std::optional<Saved>> saved = stop(s, file.value_or(""), confirm);
    if (!saved) return std::unexpected(saved.error());
    if (!*saved) return xpp::fail("recording.stop", "The recording was not saved: the question was cancelled or declined, and the recording goes on", xpp::command_place());
    std::string out = "{\"file\":";
    buf_str(out, (*saved)->file);
    return out + xpp::format(",\"steps_recorded\":{}}}", (*saved)->steps);
}

xpp::Result<std::string> recording_add_note(const std::string &note)
{
    if (!recorder.rec) return xpp::fail("recording.add_note", "Not recording: a note goes with a recording's next step", xpp::command_place());
    recorder.note = note;
    return std::string("{\"note_added\":true}");
}

bool j_recording(void) { return recorder.rec.has_value(); }

bool j_save_recording(xpp::Session &s)
{
    /* File/Quit's step is open (the q of the File menu): it and the File
       menu opened for it are no steps; a quit from the window has none */
    return stop_shown(s);
}

void record_begin(const char *line)
{
    if (!recorder.rec || !line_is_step(line)) return;
    StepTaken t;
    t.open = true;
    t.note = std::move(recorder.note);
    recorder.note.clear();
    const char kind = line_kind(line);
    t.view = kind == XPP_KIND_VIEW;
    t.label = command_label(line);
    t.cmd = std::string(js_raw(line));
    recorder.step = std::move(t);
}

void record_save_permission()
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open) return;
    t.save_answers = t.answers.size();
}

void record_save_decision(int decision, bool asked)
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open) return;
    if (asked && t.answers.size() > t.save_answers)
        t.answers.pop_back(); /* this save's choice becomes a decision, not a conditional answer */
    t.answers.push_back(xpp::format("{{\"save_replace\":{}}}", decision));
}

void record_answer(const char *kind, const char *answer, bool ok)
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open) return;
    if (!ok) t.answers.emplace_back("null");
    else if (strcmp(kind, "string") == 0) t.answers.emplace_back(js_find(answer, "value") ? js_raw(js_find(answer, "value")) : "\"\"");
    else if (strcmp(kind, "form") == 0) t.answers.emplace_back(js_find(answer, "values") ? js_raw(js_find(answer, "values")) : "[]");
    else t.answers.push_back(js_object_without(answer, {"cmd", "id", "ok"}));
}



namespace {

/* A state-changing control belongs to the job that took it. */
void record_during(std::string_view field, std::string_view value)
{
    StepTaken &t = recorder.step;
    if (!recorder.rec || !t.open) return;
    std::string at;
    buf_stopped_at(at);
    t.during.push_back(xpp::format("{{\"{}\":{},\"at\":{}}}", field, value, at));
}

} // namespace



void record_control(const char *line) { record_during("cmd", js_raw(line)); }

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

void record_end(xpp::Session &s, bool cancelled, bool refused)
{
    if (!recorder.rec) return;
    StepTaken &t = recorder.step;
    if (!t.open) return;
    t.open = false;
    if (refused) {
        if (recorder.note.empty()) recorder.note = std::move(t.note);
        return;
    }
    add_model_files(s.model());
    if (xpp::job::computed()) t.view = false;
    std::string abort;
    if (cancelled) {
        std::string b;
        buf_stopped_at(b);
        abort = std::move(b);
    }
    recorder.rec->steps.push_back({std::move(t.note), step_line(t, abort)});
}

void buf_recording(std::string &b)
{
    if (!recorder.rec) return;
    buf_format(b, ",\"recording\":{{\"steps\":{:d},\"note\":", recorder.rec->steps.size());
    buf_str(b, recorder.note);
    BUF_LIT(b, "}");
}

} // namespace xpp::json
