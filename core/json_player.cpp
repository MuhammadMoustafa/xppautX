/* The player (W59b, docs/protocol.md "Playing a recording"): File/plaY
   recording, Open model of a .recx, or {"cmd":"play","op":"open"} loads a
   recording's model from the files it holds and replays its steps, as
   they were taken: each step's command, then, as the core asks, the key
   or the answer the recording gives each question, and the recorded
   interruption armed as --script arms an abort line (arm_recorded_stop).
   Nothing is fixed, skipped or changed: a step that asks what it does not
   answer, or ends with answers left, stops the player with an error.

   The files a step read are served from the recording (xpp::files::serve_reads):
   the sections the step names, never the disk; a file it does not hold is
   not there, and says so. The replay runs in a scratch folder of its own,
   so what it writes never touches the user's files and a second play
   behaves as the first.

   Only computation takes time: before each input the core sends a press
   event (the key, the answer, the command: what the page lights) and
   waits its display pace, and after the step's idle the step's pace, a
   view step's shorter, each divided by the speed. Play from a step runs
   the steps before it with no pace (their presses 0 ms). The timer is the
   command loop's and a question's wait (player_wait_ms, player_fire):
   the core stays single-threaded, and reads the client's lines while it
   waits. */
#include "ui_json.h"
#include "ui_json_internal.h"
#include "model.h"
#include "browse.h"
#include "model_files.h"
#include "model_switch.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_files.h"
#include "xpp_inbox.h"
#include "xpp_io.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_ui.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::json {

namespace {

using Clock = std::chrono::steady_clock;

/* a recorded step, read from its line */
struct PlayStep {
    std::string line; /* the step's JSON object, as the file has it */
    int at = 0;       /* its line in the file */
    std::string note;
    std::string win, button, cmd; /* cmd: a command other than key, whole */
    std::vector<std::string> keys, answers;
    std::vector<size_t> files; /* the sections it read, in order */
    std::string abort;         /* the recorded interruption's `at` */
    std::string key_read, key_read_at; /* the first key the job read itself */
    bool view = false;
};

/* what the player does when its time comes */
enum class Next { none, begin, input };

struct Player {
    bool open = false;
    std::string path; /* the .recx, absolute */
    recx::Recording rec;
    std::optional<SavedFile> snapshot; /* rec's: the session the replay starts from (W59d) */
    bool intact = true;
    std::vector<PlayStep> steps;
    std::unique_ptr<xpp::TempDir> work;   /* the replay's working folder */
    std::unique_ptr<xpp::TempDir> copies; /* the served files' copies */
    std::vector<std::string> copy_of;     /* per section: its copy, "" not written */
    bool loading = false;                 /* its model's load is requested */
    bool play_after_load = false;
    int fast_to = 0;  /* the steps below it run with no pace (Play from here) */
    bool pause_at = false; /* and it pauses there */
    int next = 0;     /* the next step to run */
    int running = -1; /* the step running */
    bool in_command = false; /* handle_line runs the running step's command */
    bool off_script = false; /* it went where the recording does not: the user answers */
    size_t keys_used = 0, answers_used = 0;
    std::vector<bool> files_used; /* of the running step's */
    std::string pushed; /* the command line pushed, until handle_line takes it */
    bool playing = false, step_once = false;
    double speed = 1;
    Next what = Next::none;
    Clock::time_point due;
    Clock::duration left{}; /* a wait's rest, while paused */
    std::string input;      /* Next::input's line */
    bool input_last = false; /* it is the step's last input */
};

Player player;

/* ---- reading a recording ---- */

/* the session a recording at path begins from, its snapshot (W59d):
   nothing, with an error message, when it is not one */
std::optional<SavedFile> snapshot_of(const std::string &path, const recx::Recording &rec)
{
    return xpp_saved_parse(path, xpp::format("The @snapshot of {}", xpp::files::split_path(path).second), rec.snapshot,
                           SavedKind::snapshot);
}

/* the strings of the array at arr */
std::vector<std::string> strings_of(const char *arr)
{
    std::vector<std::string> out;
    const char *e;
    for (int i = 0; arr && (e = js_elem(arr, i)) != nullptr; i++) {
        std::string s;
        js_string(e, s);
        out.push_back(std::move(s));
    }
    return out;
}

/* the step of line, or an error saying why it cannot be played */
bool read_step(const recx::Step &st, size_t sections, PlayStep &out, std::string &error)
{
    const char *line = st.line.c_str();
    if (!js_valid(line) || *skip_ws(line) != '{') {
        error = "it is not a JSON object";
        return false;
    }
    out.line = st.line;
    out.at = st.at;
    out.note = st.note;
    get_string(line, "win", out.win);
    get_string(line, "button", out.button);
    if (const char *c = js_find(line, "cmd")) out.cmd = std::string(js_raw(c));
    out.keys = strings_of(js_find(line, "keys"));
    const char *a = js_find(line, "answers"), *e;
    for (int i = 0; a && (e = js_elem(a, i)) != nullptr; i++) out.answers.emplace_back(js_raw(e));
    const char *f = js_find(line, "files");
    for (int i = 0; f && (e = js_elem(f, i)) != nullptr; i++) {
        const double k = js_num(e, -1);
        if (k < 0 || k >= static_cast<double>(sections)) {
            error = xpp::format("it names file section {}, which the recording does not hold", js_raw(e));
            return false;
        }
        out.files.push_back(static_cast<size_t>(k));
    }
    if (const char *ab = js_find(line, "abort")) out.abort = std::string(js_raw(ab));
    if (const char *d = js_find(line, "during"); d && (e = js_elem(d, 0)) != nullptr) {
        get_string(e, "key", out.key_read);
        if (const char *at = js_find(e, "at")) out.key_read_at = std::string(js_raw(at));
    }
    const char *v = js_find(line, "view");
    out.view = v && std::strncmp(v, "true", 4) == 0;
    if (out.cmd.empty() && out.keys.empty()) {
        error = "it has neither keys nor a cmd";
        return false;
    }
    if (!out.cmd.empty() && out.cmd[0] != '{') {
        error = "its cmd is not an object";
        return false;
    }
    return true;
}

/* the recording at path read whole, every step checked (W125: before
   anything is played or loaded) into steps, or the error at its line */
xpp::Result<recx::Read> read_recording(const std::string &path, std::vector<PlayStep> &steps)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) return xpp::fail_reading("recording", "cannot be opened", path);
    xpp::Result<recx::Read> got = recx::read(bytes, path);
    if (!got) return got;
    steps.assign(got->rec.steps.size(), PlayStep());
    std::string error;
    for (size_t i = 0; i < steps.size(); i++) {
        const recx::Step &st = got->rec.steps[i];
        if (!read_step(st, got->rec.files.size(), steps[i], error))
            return xpp::fail("recording", xpp::format("step {} cannot be played: {}", i + 1, error), xpp::Place{path, st.at, 0, st.line});
    }
    return got;
}

/* ---- the pace ---- */

/* every pace below is scaled by this: the maintainer found 1x a little too
   fast (2026-10-01), so the old 1x is now about 0.67x; the speeds stay
   0.5, 1, 2, 4 */
constexpr double PACE_SLOWDOWN = 1.5;

/* ms at the old 1x, times PACE_SLOWDOWN, divided by the speed; none while
   running to Play from here */
Clock::duration pace(double ms)
{
    if (player.next < player.fast_to) return Clock::duration::zero();
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double, std::milli>(ms * PACE_SLOWDOWN / player.speed));
}

bool fast() { return player.next < player.fast_to; }

} // namespace

xpp::Place player_place(void)
{
    if (player.running < 0 || player.running >= static_cast<int>(player.steps.size())) return {};
    const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    return xpp::Place{player.path, st.at, 0, st.line};
}

namespace {

/* the timer runs */
bool timer_on() { return player.open && (player.playing || player.step_once); }

void schedule(Next what, Clock::duration after)
{
    player.what = what;
    player.due = Clock::now() + after;
    player.left = after;
}

/* {"ev":"press",...}: what the step presses next, before it is sent (a
   running step's asks are the player's: the page answers none of them) */
void send_press(const char *what, size_t index, Clock::duration ms)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"press\",\"step\":{:d},\"what\":\"{}\",\"index\":{:d},\"ms\":{:d}}}", player.running, what,
               index, std::chrono::duration_cast<std::chrono::milliseconds>(ms).count());
    send_buf(&b);
}

/* ---- the steps ---- */

/* the step stops playing: an error, and the player pauses */
void diverged(const std::string &why)
{
    j_command_error("play", xpp::format("Step {} of the recording {}; the player stopped", player.running + 1, why));
    player.playing = false;
    player.step_once = false;
    player.fast_to = 0;
    player.what = Next::none;
    player.off_script = true;
}

/* the running step's input `line` goes next, after its press */
bool last_input(const PlayStep &st)
{
    if (player.keys_used != st.keys.size()) return false;
    for (size_t i=player.answers_used; i<st.answers.size(); ++i)
        if (!js_find(st.answers[i].c_str(), "save_replace")) return false;
    return true; /* save decisions are consumed by the owner, not another input */
}

void input_after(std::string line, const char *what, size_t index, double ms)
{
    PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    player.input = std::move(line);
    player.input_last = last_input(st);
    const Clock::duration d = pace(st.view ? ms / 3 : ms);
    send_press(what, index, d);
    schedule(Next::input, d);
}

/* the note's reading time before a step's first input */
double reading_ms(const PlayStep &st) { return std::min(3000.0, 35.0 * static_cast<double>(st.note.size())); }

void begin_step()
{
    PlayStep &st = player.steps[static_cast<size_t>(player.next)];
    player.running = player.next;
    player.keys_used = player.answers_used = 0;
    player.files_used.assign(st.files.size(), false);
    std::string line;
    const char *what = "cmd";
    if (!st.cmd.empty()) {
        line = st.cmd;
    } else {
        Buf b;
        BUF_LIT(&b, "{\"cmd\":\"key\",\"key\":");
        buf_str(&b, st.keys[0]);
        if (!st.win.empty()) {
            BUF_LIT(&b, ",\"win\":");
            buf_str(&b, st.win);
        }
        if (!st.button.empty()) {
            BUF_LIT(&b, ",\"button\":");
            buf_str(&b, st.button);
        }
        BUF_LIT(&b, "}");
        line = std::move(b.s);
        player.keys_used = 1;
        what = "key";
    }
    input_after(std::move(line), what, 0, 700 + reading_ms(st));
}

/* the recorded interruption, or key read, armed for the step's job */
void arm_step(const PlayStep &st)
{
    if (!st.key_read.empty()) arm_recorded_stop(st.key_read_at.c_str(), key_code(st.key_read.c_str()));
    else if (!st.abort.empty()) arm_recorded_stop(st.abort.c_str(), 0);
}

void push_input()
{
    player.what = Next::none;
    const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    if (player.input_last) arm_step(st);
    if (!player.in_command) player.pushed = player.input; /* the step's command, for player_begin */
    xpp::inbox::push(player.input);
}

/* the recording's answer to an ask of kind: a menu's or a choice's key,
   or the next answer, as the answer line a client sends */
std::string answer_line(const std::string &kind, const std::string &value, bool key)
{
    Buf b;
    if (key) {
        if (value == "Escape") return "{\"cmd\":\"answer\",\"ok\":0}";
        BUF_LIT(&b, "{\"cmd\":\"answer\",\"key\":");
        buf_str(&b, value);
        BUF_LIT(&b, "}");
        return std::move(b.s);
    }
    if (value == "null") return "{\"cmd\":\"answer\",\"ok\":0}";
    if (kind == "string") return "{\"cmd\":\"answer\",\"value\":" + value + "}";
    if (kind == "form") return "{\"cmd\":\"answer\",\"values\":" + value + "}";
    const std::string inner = value.size() > 2 && value[0] == '{' ? value.substr(1) : "}";
    return "{\"cmd\":\"answer\"" + std::string(inner == "}" ? "" : ",") + inner;
}

/* a served file's copy, written once */
bool copy_of(size_t section, std::string &copy)
{
    std::string &c = player.copy_of[section];
    if (c.empty()) {
        if (!player.copies) player.copies = std::make_unique<xpp::TempDir>();
        if (player.copies->path().empty()) return false;
        const std::string path = player.copies->file(std::to_string(section));
        xpp::Writer w = xpp::Writer::binary(path.c_str());
        if (!w || !w.write(player.rec.files[section].bytes) || !w.commit()) return false;
        c = path;
    }
    copy = c;
    return true;
}

/* the section's file is the one path names: the same name, or, from
   another folder, the same file name */
bool names(size_t section, std::string_view path)
{
    const std::string &name = player.rec.files[section].name;
    return name == path || xpp::files::split_path(name).second == xpp::files::split_path(path).second;
}

/* xpp::files::serve_reads' server: while the player's model loads, the
   recording's first file of that name; while a step runs, the next of
   the sections it read that has that name (again the last, read again) */
bool serve(const std::string &path, std::string *copy)
{
    std::optional<size_t> section;
    if (player.loading) {
        for (size_t i = 0; i < player.rec.files.size() && !section; i++)
            if (names(i, path)) section = i;
    } else if (player.running >= 0) {
        const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
        for (size_t i = 0; i < st.files.size() && !section; i++)
            if (!player.files_used[i] && names(st.files[i], path)) {
                section = st.files[i];
                if (copy) player.files_used[i] = true;
            }
        for (size_t i = st.files.size(); i-- > 0 && !section;)
            if (names(st.files[i], path)) section = st.files[i];
    }
    if (!section) {
        if (copy && !player.loading)
            j_command_error("play", xpp::format("The recording holds no copy of {} for step {}: it is not there", path,
                                  player.running + 1).c_str());
        return false;
    }
    if (!copy) return true;
    if (!copy_of(*section, *copy)) {
        j_command_error("play", xpp::format("Cannot copy {} out of the recording", path));
        return false;
    }
    return true;
}

/* the player ends: its model stays, in its folder */
void close_player()
{
    if (!player.open && !player.loading) return;
    xpp::files::serve_reads(nullptr);
    std::unique_ptr<xpp::TempDir> work = std::move(player.work); /* the folder the model runs in */
    player = Player{};
    player.work = std::move(work);
}

/* the recording's model loads (handle_line, once this command returned),
   in a fresh scratch folder, as its snapshot has it, the session restored
   as it was when the recording began (W59d): from step 0, running with no
   pace to step `from`, then playing when `play` */
void load(xpp::Session &s, int from, bool play)
{
    std::unique_ptr<xpp::TempDir> before = std::move(player.work);
    player.work = std::make_unique<xpp::TempDir>();
    if (player.work->path().empty()) {
        j_command_error("play", "Cannot make a folder to play the recording in");
        player.work = std::move(before);
        return;
    }
    s.model_request = xpp::saved_request(s, player.work->path(), *player.snapshot);
    s.model_request->home = xpp::files::split_path(player.path).first; /* dialogs stay in the recording's folder */
    player.loading = true;
    player.fast_to = from;
    player.play_after_load = play || from > 0;
    player.pause_at = !play;
    player.what = Next::none;
    player.running = -1;
    xpp::files::serve_reads(serve); /* its model's files: the recording's */
}

/* {"ev":"player",...}: the recording, its steps and notes */
void send_player()
{
    Buf b;
    BUF_LIT(&b, "{\"ev\":\"player\",\"file\":");
    buf_str(&b, player.path);
    BUF_LIT(&b, ",\"model\":");
    buf_str(&b, player.rec.model);
    buf_format(&b, ",\"intact\":{},\"steps\":[", player.intact ? "true" : "false");
    for (size_t i = 0; i < player.steps.size(); i++) {
        const std::string &line = player.steps[i].line;
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "{\"note\":");
        buf_str(&b, player.steps[i].note);
        const std::string_view rest = std::string_view(line).substr(line.find('{') + 1);
        if (skip_ws(rest.data())[0] != '}') BUF_LIT(&b, ",");
        buf_add(&b, rest.data(), rest.size());
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
}

void open_recording(xpp::Session &s, std::string_view path, bool ask = true)
{
    if (player.running >= 0) {
        j_command_error("play", "Not while a recording plays a step");
        return;
    }
    std::string file(path);
    if (file.empty()) {
        if (!file_selector("Play recording", file, "*.recx") || file.empty()) return;
    }
    std::vector<PlayStep> steps;
    xpp::Result<recx::Read> got = read_recording(file, steps);
    if (!got) {
        show_error(got.error());
        return;
    }
    const std::string where = xpp::files::absolute(file);
    std::optional<SavedFile> snapshot = snapshot_of(where, got->rec);
    if (!snapshot) return;
    if (ask && !xpp_model_may_leave(s, file)) return;
    close_player();
    player.path = where;
    player.snapshot = std::move(snapshot);
    player.rec = std::move(got->rec);
    player.intact = got->intact;
    player.steps = std::move(steps);
    player.copy_of.assign(player.rec.files.size(), std::string());
    load(s, 0, false);
}

/* play, pause, step and speed: at any moment, even during a step */
void control(const std::string &op, const char *line)
{
    if (!player.open) {
        j_command_error("play", "No recording is open in the player");
        return;
    }
    if (op == "pause") {
        if (player.what != Next::none) player.left = std::max(Clock::duration::zero(), player.due - Clock::now());
        player.playing = false;
        player.step_once = false;
    } else if (op == "start" || op == "step") {
        const bool paused = !timer_on();
        if (op == "start") player.playing = true;
        else player.step_once = true;
        if (player.what != Next::none) {
            if (paused) player.due = Clock::now() + player.left;
        } else if (player.running < 0 && player.next < static_cast<int>(player.steps.size())) {
            schedule(Next::begin, Clock::duration::zero());
        }
    } else if (op == "speed") {
        const double x = get_num(line, "speed", 1);
        player.speed = x < 0.25 ? 0.25 : x > 8 ? 8 : x;
    }
}

} // namespace

void play_command(xpp::Session &s, const char *line)
{
    std::string op;
    get_string(line, "op", op, 16);
    if (op == "open") {
        std::string file;
        get_string(line, "file", file);
        open_recording(s, file);
    } else if (op == "from") {
        if (!player.open || player.running >= 0) {
            j_command_error("play", player.open ? "Not while a step plays" : "No recording is open in the player");
            return;
        }
        const int n = static_cast<int>(player.steps.size());
        int from = get_int(line, "step", 0);
        from = from < 0 ? 0 : from > n ? n : from;
        load(s, from, js_find(line, "play") ? get_int(line, "play", 0) != 0 : from > 0);
    } else if (op == "note") {
        const int i = get_int(line, "step", -1);
        if (!player.open || i < 0 || i >= static_cast<int>(player.steps.size())) {
            j_command_error("play", "play note: no such step");
            return;
        }
        std::string text;
        get_string(line, "text", text);
        std::string &note = player.rec.steps[static_cast<size_t>(i)].note;
        const std::string was = note;
        note = text;
        /* the file as it was, but for the note: its fingerprint is kept,
           a mismatch included (recx.h) */
        xpp::Writer w=xpp::open_writer_asking(player.path);
        if (!w) {
            note = was;
            return;
        }
        w.write(recx::text(player.rec));
        if(!xpp::ok_or_show(xpp::commit_save(w))){note=was;return;}
        player.steps[static_cast<size_t>(i)].note = text;
        bottom_msg(0, xpp::format("Saved the note of step {} in {}", i + 1, xpp::files::split_path(player.path).second));
        send_player();
    } else if (op == "close") {
        if (player.running >= 0) {
            j_command_error("play", "Not while a step plays");
            return;
        }
        close_player();
    } else if (op == "start" || op == "pause" || op == "step" || op == "speed") {
        control(op, line);
    } else {
        j_command_error("play", xpp::format("Unknown play op {}", op));
    }
}

void player_hold(void)
{
    if (player.open) control("pause", nullptr);
}

bool play_async(const char *line)
{
    if (!is_cmd(line, "play")) return false;
    std::string op;
    get_string(line, "op", op, 16);
    if (op != "start" && op != "pause" && op != "step" && op != "speed") return false;
    control(op, line);
    return true;
}

void j_play_recording(xpp::Session &s, std::string_view path)
{
    open_recording(s, path);
}

} // namespace xpp::json

/* ui_json.h's */
namespace xpp {

std::optional<RecordingLaunch> json_ui_recording_launch(const std::string &path)
{
    std::vector<json::PlayStep> steps;
    xpp::Result<xpp::recx::Read> got = json::read_recording(path, steps);
    if (!got) {
        xpp::log(XPP_LOG_ERROR, "{}\n", got.error().text());
        return std::nullopt;
    }
    /* the model of its snapshot, which the player loads (W59d) */
    std::optional<SavedFile> snapshot = xpp::json::snapshot_of(xpp::files::absolute(path), got->rec);
    if (!snapshot) return std::nullopt; /* the error said why */
    return RecordingLaunch{std::move(snapshot->model), snapshot->manifest.model_name};
}

void json_ui_play_launched(xpp::Session &s, const std::string &path)
{
    json::open_recording(s, path, false);
}

} // namespace xpp

namespace xpp::json {

int player_wait_ms(void)
{
    if (!timer_on() || player.what == Next::none) return -1;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(player.due - Clock::now()).count();
    return ms < 0 ? 0 : static_cast<int>(std::min<long long>(ms, 1 << 30));
}

void player_fire(void)
{
    if (!timer_on() || player.what == Next::none || Clock::now() < player.due) return;
    try {
        if (player.what == Next::begin) begin_step();
        else push_input();
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("playing a recording");
    }
}

void player_begin(const char *line)
{
    if (player.running < 0 || player.in_command || player.pushed != line) return;
    player.in_command = true;
    player.off_script = false;
    player.pushed.clear();
    xpp::files::serve_reads(serve); /* the files it reads: the recording's */
}

int player_save_replace(int decision)
{
    if (decision != SAVE_ASK || !player.in_command || player.off_script) return decision;
    const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    if (player.answers_used == st.answers.size()) return decision;
    const char *value=js_find(st.answers[player.answers_used].c_str(), "save_replace");
    if (!value) return decision;
    double saved;
    if (!js_number(value, &saved) || (saved != static_cast<double>(SAVE_REPLACE) &&
                                    saved != static_cast<double>(SAVE_DECLINE))) {
        diverged("has an invalid recorded save decision");
        return SAVE_DECLINE;
    }
    ++player.answers_used;
    player.input_last=last_input(st);
    return static_cast<int>(saved);
}

void player_asked(const char *kind)
{
    if (!player.in_command || player.off_script) return;
    try {
        const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
        const std::string k = kind;
        if (k == "alert") {
            /* the recording keeps no answer to a message: it was read */
            player.input = "{\"cmd\":\"answer\"}";
            player.input_last = false;
            const Clock::duration d = pace(1200);
            send_press("alert", 0, d);
            schedule(Next::input, d);
        } else if (k == "menu" || k == "choice") {
            if (player.keys_used == st.keys.size()) return diverged(xpp::format("asks a {} it holds no key for", k));
            const size_t i = player.keys_used++;
            input_after(answer_line(k, st.keys[i], true), "key", i, 600);
        } else {
            if (player.answers_used == st.answers.size()) return diverged(xpp::format("asks a {} it holds no answer for", k));
            if (js_find(st.answers[player.answers_used].c_str(), "save_replace"))
                return diverged(xpp::format("asks a {} before its recorded save decision", k));
            const size_t i = player.answers_used++;
            input_after(answer_line(k, st.answers[i], false), "answer", i,
                        std::min(2500.0, 900 + 40.0 * static_cast<double>(st.answers[i].size())));
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("playing a recording");
    }
}

void player_stop_missed(void)
{
    if (!player.in_command || player.off_script) return;
    const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    diverged(xpp::format("never got to its recorded interruption at {}",
                         st.key_read.empty() ? st.abort : st.key_read_at));
}

void player_step_end(void)
{
    if (!player.in_command) return;
    player.in_command = false;
    xpp::files::serve_reads(nullptr);
    const PlayStep &st = player.steps[static_cast<size_t>(player.running)];
    if (player.off_script) {
    } else if (player.what == Next::input) {
        diverged("ended before its next answer was given"); /* the user answered for it */
    } else if (player.keys_used < st.keys.size() || player.answers_used < st.answers.size()) {
        diverged("ended with recorded answers it never asked for");
    }
    player.off_script = false;
    const double ms = st.view ? 250 : 800;
    player.running = -1;
    player.next++;
    player.step_once = false;
    if (player.next == player.fast_to && player.pause_at) player.playing = false;
    if (player.next >= static_cast<int>(player.steps.size())) {
        player.playing = false;
        player.what = Next::none;
        bottom_msg(0, "End of the recording");
    } else if (player.what == Next::none && timer_on()) {
        schedule(Next::begin, pace(ms));
    }
}

void player_model_switched(bool loaded)
{
    if (player.loading) {
        player.loading = false;
        xpp::files::serve_reads(nullptr);
        if (!loaded) {
            close_player();
            return;
        }
        player.open = true;
        player.next = 0;
        player.playing = player.play_after_load;
        send_player();
        if (timer_on() && !player.steps.empty()) schedule(Next::begin, Clock::duration::zero());
        return;
    }
    /* a step that opens or reloads a model is the recording's; any other
       model opened ends the player */
    if (loaded && player.open && !player.in_command) close_player();
}

void buf_player(Buf *b)
{
    if (!player.open) return;
    buf_format(b, ",\"player\":{{\"step\":{:d},\"running\":{:d},\"playing\":{},\"speed\":", player.next, player.running,
               player.playing ? "true" : "false");
    buf_num(b, player.speed, 6);
    buf_format(b, ",\"fast\":{},\"intact\":{}}}", fast() ? "true" : "false", player.intact ? "true" : "false");
}

} // namespace xpp::json
