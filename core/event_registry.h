#pragma once

/* The events the core sends (W529, Discussion #415 decision 10): each one has one name, `resource.event`, its owner
   resource's, and is declared here with its fields. The emitters, the command rows' `events` lists, hello's
   `features`, events.subscribe and `help events` all read these names. The protocol's own events stay bare: `hello`
   (the handshake), `done` (a command's end), `ask` (a question) and `exit` (the process ended, said by the host). C++
   only; it includes nothing of the core, so the HTTP host can name its events too. */
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace xpp::events {

/* the protocol's own, bare */
inline constexpr char HELLO[] = "hello";
inline constexpr char DONE[] = "done";
inline constexpr char ASK[] = "ask";
inline constexpr char EXIT[] = "exit";

inline constexpr char SESSION_STATE[] = "session.state";
inline constexpr char SESSION_SAVED[] = "session.saved";
inline constexpr char SESSION_SOURCE[] = "session.source";
inline constexpr char SESSION_EQUATIONS[] = "session.equations";
inline constexpr char SESSION_MESSAGE[] = "session.message";
inline constexpr char SESSION_ERROR[] = "session.error";
inline constexpr char SESSION_LOG[] = "session.log";
inline constexpr char SESSION_PING[] = "session.ping";
inline constexpr char SESSION_BYE[] = "session.bye";
inline constexpr char JOB_COMPUTING[] = "job.computing";
inline constexpr char JOB_PROGRESS[] = "job.progress";
inline constexpr char JOB_STOPPED[] = "job.stopped";
inline constexpr char JOB_LIVE_STATE[] = "job.live_state";
inline constexpr char PLOT_SERIES[] = "plot.series";
inline constexpr char PLOT_RUNS[] = "plot.runs";
inline constexpr char PLOT_SHOWN[] = "plot.shown";
inline constexpr char PLOT_CLEARED[] = "plot.cleared";
inline constexpr char PLOT_REDRAWN[] = "plot.redrawn";
inline constexpr char PLOT_MARKS[] = "plot.marks";
inline constexpr char NULLCLINE_CURVES[] = "nullcline.curves";
inline constexpr char DIRFIELD_FIELD[] = "dirfield.field";
inline constexpr char EQUILIBRIUM_FOUND[] = "equilibrium.found";
inline constexpr char WINDOW_CREATED[] = "window.created";
inline constexpr char WINDOW_SELECTED[] = "window.selected";
inline constexpr char WINDOW_DESTROYED[] = "window.destroyed";
inline constexpr char WINDOW_RENAMED[] = "window.renamed";
inline constexpr char WINDOW_LIST[] = "window.list";
inline constexpr char AUTO_DIAGRAM[] = "auto.diagram";
inline constexpr char AUTO_INFO[] = "auto.info";
inline constexpr char AUTO_SETTINGS[] = "auto.settings";
inline constexpr char AUTO_VIEW[] = "auto.view";
inline constexpr char ARRAY_PLOT_SHOWN[] = "array_plot.shown";
inline constexpr char ANIMATION_STATE[] = "animation.state";
inline constexpr char ANIMATION_FRAME[] = "animation.frame";
inline constexpr char KINESCOPE_FILM[] = "kinescope.film";
inline constexpr char BROWSER_UPDATED[] = "browser.updated";
inline constexpr char RUN_NUMERICS[] = "run.numerics";
inline constexpr char RECORDING_PLAYER[] = "recording.player";
inline constexpr char RECORDING_PRESS[] = "recording.press";
inline constexpr char KEYMAP_CHANGED[] = "keymap.changed";
inline constexpr char FILE_RESULT[] = "file.result";

/* A declared event: its name, the fields it carries (comma-separated, an `op` field naming the form where one event has
   several), and what it means. `stream`: the event a client subscribes to (events.subscribe) to get it, this one or the
   one it comes with; empty for one every client gets. */
struct Event {
    std::string_view name;
    std::string_view fields;
    std::string_view description;
    std::string_view stream = {};
};

/* every event, in the order `help events` lists them */
std::span<const Event> all();

/* `{"ev":"NAME"`: the start of an event line, its closing brace the caller's */
inline std::string open(std::string_view name)
{
    std::string line = "{\"ev\":\"";
    line += name;
    line += '"';
    return line;
}

/* the heavy data events a client may subscribe to (events.subscribe), in hello's `features` order */
inline constexpr std::array<std::string_view, 11> STREAMS = {PLOT_SERIES, PLOT_SHOWN, NULLCLINE_CURVES, DIRFIELD_FIELD, PLOT_MARKS, ANIMATION_FRAME,
                                                             AUTO_INFO, AUTO_SETTINGS, RUN_NUMERICS, WINDOW_LIST, RECORDING_PLAYER};

} // namespace xpp::events
