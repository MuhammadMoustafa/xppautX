/* The declared events (event_registry.h): name, fields and meaning, the one source `help events` and docs/protocol.md's
   generated event table read. An event is declared once, whatever forms it has (an `op` field names them). */
#include "event_registry.h"

namespace xpp::events {
namespace {

constexpr std::array EVENTS = {
    Event{HELLO, "protocol, features, title, file, output_names, commands, lists, userbuttons, keymap, about, quit, window_names",
          "The first event, and again after Open model or Reload loaded a model in its place."},
    Event{DONE, "job, id, status, result, error, file, line, col, source, field",
          "A command's end: every command sends one, with its result when status is ok."},
    Event{ASK, "job, id, kind", "A question the running command asks; answer it with its `answer`."},
    Event{EXIT, "code", "The process ended (said by the host in browser mode)."},
    Event{SESSION_STATE, "sliders, pars, ics, now, rates, bcs, delays, view, auto, rows, win, seed, session, changed, can_undo, can_redo, recording, player",
          "The session's current values, sent after every command."},
    Event{SESSION_SAVED, "saved, file", "The save owner's result for this exact destination; only true authorizes delivery."},
    Event{SESSION_SOURCE, "lines, comments", "The model's source lines."},
    Event{SESSION_EQUATIONS, "lines", "One dX/dT=... line per equation."},
    Event{SESSION_MESSAGE, "bottom, help, box, auto, calc, error, file, line, col, source",
          "Status text: one of bottom, box, auto or calc, or an error with its place."},
    Event{SESSION_ERROR, "error, file, line, col, source", "The model did not load: sent instead of hello, then the program exits."},
    Event{SESSION_LOG, "text", "What the program printed on stderr, in chunks cut anywhere (said by the host in browser mode)."},
    Event{SESSION_PING, "", "Beep."},
    Event{SESSION_BYE, "", "The program is exiting normally, sent before an error-free exit."},
    Event{JOB_COMPUTING, "job", "The running command began computing: until its done the server refuses data and computation commands."},
    Event{JOB_PROGRESS, "n, of", "A computation's progress, at most 10 a second."},
    Event{JOB_STOPPED, "at", "The command's computation was cancelled; at says where it stopped."},
    Event{JOB_LIVE_STATE, "time, now, rates", "The solver's doubles for every state at the latest output step of a running integration."},
    Event{PLOT_SERIES, "op, win, rows, three, enc, xlabel, ylabel, zlabel, curves, shift, columns, from, version",
          "A plot window's curves as numbers; with op append only the rows stored since, with op end the run's end (from and version).", PLOT_SERIES},
    Event{PLOT_RUNS, "win, enc, erased, clear, drop, keep, add", "A plot window's earlier runs changed.", PLOT_SERIES},
    Event{PLOT_SHOWN, "active, simultaneous_plot, windows", "Every plot window and the active one.", PLOT_SHOWN},
    Event{PLOT_CLEARED, "win", "The Erase command blanked plot window win.", PLOT_SERIES},
    Event{PLOT_REDRAWN, "win", "The Redraw command drew plot window win again.", PLOT_SERIES},
    Event{PLOT_MARKS, "win, enc, equilibria, text, arrows, markers, frozen",
          "A plot window's equilibria, text, arrows, markers and frozen curves.", PLOT_MARKS},
    Event{NULLCLINE_CURVES, "win, enc, xname, yname, xcolor, ycolor, x, y, frozen", "A plot window's nullclines as segments.", NULLCLINE_CURVES},
    Event{DIRFIELD_FIELD, "win, enc, scaled, color, n, du, dv, grid, speed, flows", "A plot window's direction field and its flow's trajectories.", DIRFIELD_FIELD},
    Event{EQUILIBRIUM_FOUND, "type, cplus, cminus, rplus, rminus, im, values, eigenvalues", "The result of an equilibrium search."},
    Event{WINDOW_CREATED, "kind, win, w, h, title", "A window was made."},
    Event{WINDOW_SELECTED, "kind, win, w, h, title", "A window became the active one."},
    Event{WINDOW_DESTROYED, "kind, win, w, h, title", "A window was destroyed."},
    Event{WINDOW_RENAMED, "window, new_name", "A window was renamed."},
    Event{WINDOW_LIST, "windows", "Every open window and whether it is shown, sent again when it changed.", WINDOW_LIST},
    Event{AUTO_DIAGRAM, "op, view", "The AUTO diagram as data, each view of it; the rest of its fields depend on op (axes, reset, add, views)."},
    Event{AUTO_INFO, "info, stab, stop, notes, start_refined", "AUTO's info strip and stability circle, and why the last branch ended.", AUTO_INFO},
    Event{AUTO_SETTINGS, "numerics, pars, axes, marks", "AUTO's settings as data.", AUTO_SETTINGS},
    Event{AUTO_VIEW, "earlier, show, active, views", "AUTO's hidden branches, the active view and each view's zoom."},
    Event{ARRAY_PLOT_SHOWN, "title, nx, ny, values, enc, colormap, ramp, zmin, zmax, tlo, thi, first_row, tag", "The array plot's cells and colours."},
    Event{ANIMATION_STATE, "pos, rows, fly, grab, skip, speed, loaded, open", "The animation's state for its slider and toggles."},
    Event{ANIMATION_FRAME, "pos, rows, t, speed, skip, dim, w, h, prims", "A frame of the animation as data.", ANIMATION_FRAME},
    Event{KINESCOPE_FILM, "op, count, win, cycles, delay", "The kinescope: capture, reset, play or autoplay."},
    Event{BROWSER_UPDATED, "rows, cols, row0, start, end, from, col, data", "Rows of the data browser."},
    Event{RUN_NUMERICS, "fields", "The main numerics as data.", RUN_NUMERICS},
    Event{RECORDING_PLAYER, "file, model, intact, steps", "A recording opened in the player, and its steps.", RECORDING_PLAYER},
    Event{RECORDING_PRESS, "step, what, index, ms", "What the player sends next, before it sends it."},
    Event{KEYMAP_CHANGED, "op, keymap", "The keymap, after keymap.get, keymap.set or keymap.reset."},
    Event{FILE_RESULT, "op, name, ok, size, sha256, data, files, error, file, line, col, source, field", "The answer to a file command."}};

} // namespace

std::span<const Event> all() { return EVENTS; }

} // namespace xpp::events
