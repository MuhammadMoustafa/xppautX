#ifndef _command_table_h_
#define _command_table_h_

/* The one table of commands (W207, docs/command-design.md): every item of the
   three main-window menus once, with the stable id the protocol and
   recordings name it by, its category, plain label, one-line description,
   kind, default keys and whether it may be pinned. The sidebar, command
   search, the native window menus and `hello` (docs/protocol.md "Hello",
   `command_table`) are all made from it; nothing else lists a command's
   label, key or kind. Plain C++ with no core dependency, so the Linux window
   library (xpp_window.cpp) includes it too. */
#include <array>
#include <cstddef>
#include <string_view>

#include "menus.h"

namespace xpp {

/* what a command is for, the sidebar's groups in the order they are listed;
   Panels and Layer are not groups. Panels (W229, #283) are the commands
   the page opens from a panel or the title bar (the Model panel's Source,
   the Data panel's Transpose and Lookup tables, the Values panel's Named
   parameter sets and Copy parameter set line, Help's Keyboard shortcuts,
   the title bar's Record and Play): they keep a row for their keys, the
   protocol and recordings, are not listed in the sidebar and cannot be
   pinned. Layer marks the three items that only switch the
   legacy one-letter shortcut layer (File, Numerics and Return to main
   shortcuts), which the page has no use for: its XPPAUT sequences preset
   (off by default) types the layer's key as the first key of a chord */
enum class CommandCategory { Run, Files, Analysis, Plot, Tools, Panels, Layer };

struct CategoryInfo {
  CommandCategory category;
  std::string_view id, label;
  bool listed;   /* a sidebar group; false for Panels and Layer */
  bool expanded; /* the group is open before a search */
};

inline constexpr std::array<CategoryInfo, 7> COMMAND_CATEGORIES = {{
  {CommandCategory::Run, "run", "Run", true, true},
  {CommandCategory::Files, "files", "Files", true, true},
  {CommandCategory::Analysis, "analysis", "Analysis", true, false},
  {CommandCategory::Plot, "plot", "Plot", true, false},
  {CommandCategory::Tools, "tools", "Tools", true, false},
  {CommandCategory::Panels, "panels", "Opened from a panel", false, false},
  {CommandCategory::Layer, "layer", "Shortcut layer", false, false}}};

/* the most keys one command has (a binding and one alternative, as redo has
   Ctrl+Shift+Z and Ctrl+Y in the design); an empty entry is no key */
inline constexpr std::size_t MAX_DEFAULT_KEYS = 2;

/* The menu's name in the protocol (`menu` of a key command), indexed by
   MAIN_MENU, FILE_MENU, NUM_MENU: state's menu number names one */
inline constexpr std::array<std::string_view, 3> MENU_NAMES = {"main", "file", "num"};
static_assert(MAIN_MENU == 0 && FILE_MENU == 1 && NUM_MENU == 2, "MENU_NAMES is indexed by the menu numbers");

/* the legacy key of a command the page runs itself: it has no key in any menu, so no XPPAUT sequence either, and
   the core's `key` command never takes it (main_menu_action finds none); the page's own action runs it */
inline constexpr char PAGE_KEY = '\0';

struct CommandRow {
  int menu;                /* MAIN_MENU, FILE_MENU or NUM_MENU: where the legacy shortcut lives */
  char key;                /* the legacy one-letter key in that menu ('\033' is Esc) */
  std::string_view id;     /* stable: the protocol's `item`, recordings, the page's calls */
  std::string_view label;  /* plain text for buttons, menus and search */
  std::string_view description; /* one line */
  char kind;               /* XPP_KIND_*: what the command needs (docs/protocol.md "Action kinds") */
  CommandCategory category;
  bool pinnable;           /* may sit in the quick-access toolbar */
  bool primary;            /* listed in its group before a search; the others only when searching */
  /* keys that run it from the page ("Ctrl+O": Ctrl, or Cmd on macOS); the
     XPPAUT sequence is not here, it is `legacy_keys` of hello */
  std::array<std::string_view, MAX_DEFAULT_KEYS> default_keys;
};

/* Kinds: an item that opens a pop-up menu takes the least restrictive kind of that menu's items (the
   maintainer's "menus open, only their disabled items greyed": Nullcline, Dir.field, Kinescope and Graphic
   stuff are views, stocHast and Averaging data; Initialconds, Sing pts and Bndryval, whose items all
   compute, computations); nUmerics and File only switch the main menu, Esc switches it back. Parameters,
   File/Get par set and the Numerics items that ask for a value (Total ... dElay, Poincare map, rUelle plot,
   bndVal) are settings (W106): pressed during a computation, their dialog opens when it ends.

   Order is the sidebar's: by category, then as listed. Every item of the
   legacy menus has exactly one row (tools/keycheck.py reads the keys here;
   the page's navigationProblems checks the rest against hello). */
/* the one wording of the store-every setting: the command's label here and the
   option row's in model_options.cpp (a constant, not a lookup: a sanitizer
   build of GCC refuses to evaluate the table's search as a constant) */
inline constexpr std::string_view STORE_EVERY_LABEL = "Store every N steps";

inline constexpr std::array<CommandRow, 61> COMMANDS = {{
    {MAIN_MENU, 'i', "initialconds", "Initial conditions", "Integrate the equations", XPP_KIND_COMPUTE, CommandCategory::Run, true, true, {}},
    {MAIN_MENU, PAGE_KEY, "run_initial", "Run from initial", "Start a new trajectory from the Initial values", XPP_KIND_COMPUTE, CommandCategory::Run, true, false, {"Ctrl+Enter"}},
    {MAIN_MENU, PAGE_KEY, "run_last", "Run from last state", "Use the last state as Initial and start a new trajectory", XPP_KIND_COMPUTE, CommandCategory::Run, true, false, {"Ctrl+Shift+Enter"}},
    {MAIN_MENU, PAGE_KEY, "steady", "Run to steady state", "Run from Initial until every state stops changing at the chosen precision", XPP_KIND_COMPUTE, CommandCategory::Run, true, false, {"Alt+S"}},
    {MAIN_MENU, 'c', "continue", "Continue integration", "Extend the trajectory by a duration or to an end time (rounded up to the output grid)", XPP_KIND_COMPUTE, CommandCategory::Run, true, false, {"Alt+Enter"}},
    {MAIN_MENU, 'p', "parameters", "Parameters", "Change problem parameters", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 't', "total_time", "Total time", "Total time to integrate eqns", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 's', "start_time", "Start time", "Starting time -- T0", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 'r', "transient_time", "Transient time", "Time to integrate before storing", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 'd', "dt", "Time step", "Time step to use", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 'm', "method", "Solver method", "Integration method", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 'o', "store_every", STORE_EVERY_LABEL, "Store one row per N output steps", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {NUM_MENU, 'b', "bounds", "Bounds", "Maximum allowed size of any variable", XPP_KIND_SETTING, CommandCategory::Run, true, false, {}},
    {MAIN_MENU, 'z', "undo", "Undo value edit", "Take back the last edit of the parameters, initial conditions or numerics (not runs, plots or files)", XPP_KIND_SETTING, CommandCategory::Run, true, false, {"Ctrl+Z"}},
    {MAIN_MENU, 'y', "redo", "Redo value edit", "Put back the value edit that was undone", XPP_KIND_SETTING, CommandCategory::Run, true, false, {"Ctrl+Shift+Z", "Ctrl+Y"}},
    {FILE_MENU, 'm', "openmodel", "Open model…", "Load another model in place of this one", XPP_KIND_DATA, CommandCategory::Files, true, true, {"Ctrl+O"}},
    {FILE_MENU, 'n', "opensession", "Open session…", "Open a session file: its model, values, windows, data and diagram", XPP_KIND_DATA, CommandCategory::Files, true, true, {}},
    {FILE_MENU, 'v', "savesession", "Save session", "Save everything to this session's file (.snapx) to continue later; the first save asks for one", XPP_KIND_DATA, CommandCategory::Files, true, true, {"Ctrl+S"}},
    {FILE_MENU, 'w', "savesessionas", "Save session as…", "Save everything to a session file you choose (.snapx)", XPP_KIND_DATA, CommandCategory::Files, true, true, {"Ctrl+Shift+S"}},
    {FILE_MENU, 'b', "savesessioncopy", "Save a copy of the session…", "Write the session to another .snapx file and go on with this one: its file and unsaved state stay as they were", XPP_KIND_DATA, CommandCategory::Files, true, true, {}},
    {FILE_MENU, 'e', "reload", "Reload model", "Read the model's file again, keeping the values", XPP_KIND_DATA, CommandCategory::Files, true, true, {"Ctrl+R"}},
    {FILE_MENU, 'r', "importset", "Import XPPAUT settings", "Import a set file XPPAUT wrote (values, numerics, the active window)", XPP_KIND_DATA, CommandCategory::Files, true, true, {}},
    {FILE_MENU, 's', "saveinfo", "Export simulation information", "Save info about simulation in human readable format", XPP_KIND_DATA, CommandCategory::Files, true, true, {}},
    {FILE_MENU, 'q', "quit", "Quit", "Quit the application, with an option to save this session", XPP_KIND_CONTROL, CommandCategory::Files, false, true, {}},
    {MAIN_MENU, 's', "singpts", "Equilibria and stability", "Find fixed points and stability", XPP_KIND_COMPUTE, CommandCategory::Analysis, true, true, {}},
    {MAIN_MENU, 'n', "nullcline", "Nullclines", "Draw nullclines", XPP_KIND_VIEW, CommandCategory::Analysis, true, true, {}},
    {MAIN_MENU, 'd', "dirfield", "Direction fields and flow", "Direction fields and flows of the phaseplane", XPP_KIND_VIEW, CommandCategory::Analysis, true, true, {}},
    {MAIN_MENU, 'b', "bndryval", "Boundary-value solver", "Run boundary value solver", XPP_KIND_COMPUTE, CommandCategory::Analysis, true, true, {}},
    {FILE_MENU, 'a', "auto", "AUTO continuation", "Run AUTO, the bifurcation package", XPP_KIND_VIEW, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'n', "nullcline_mesh", "Nullcline settings", "Mesh for nullclines", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'i', "singpt", "Equilibrium settings", "Numerical parameters for fixed points", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'e', "delay", "Delay settings", "Maximum delay and delay related stuff", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'h', "stochastic", "Stochastic analysis", "Curve fitting, FFT, mean, variance, seed, etc", XPP_KIND_DATA, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'p', "poincare", "Poincaré map", "Define Poincare map parameters", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'u', "ruelle", "Ruelle plot", "Define shifted plots", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'v', "bndval", "Boundary-value settings", "Numerical setup for boundary value solver", XPP_KIND_SETTING, CommandCategory::Analysis, true, true, {}},
    {NUM_MENU, 'a', "averaging", "Adjoint and averaging", "Compute adjoint and averaged functions", XPP_KIND_DATA, CommandCategory::Analysis, true, true, {}},
    {MAIN_MENU, 'a', "phasespace", "Phase space", "Set up periodic/torus phase space", XPP_KIND_DATA, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'g', "graphic", "Curves and export", "Adding graphs,hard copy, etc", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'x', "xivst", "Variable vs time", "Plot variable vs time", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 't', "text", "Labels and annotations", "Add fancy text and lines,arrows", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'm', "makewindow", "Plot windows", "Create other windows", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'r', "restore", "Redraw plot", "Redraw the graph", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'e', "erase", "Clear plot", "Clear screen", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'k', "kinescope", "Captured frames", "Take snapshots of the screen", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {NUM_MENU, 'c', "colorcode", "Color by value", "Color trajectories according to velocity,etc", XPP_KIND_VIEW, CommandCategory::Plot, true, true, {}},
    {MAIN_MENU, 'w', "window", "Zoom and view", "Change the size of two-dimensional view", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {MAIN_MENU, 'v', "viewaxes", "Plot axes", "Change 2 or 3d views", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {MAIN_MENU, '3', "3dparams", "3D view", "Set parameters for 3D view", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'p', "source", "Model source", "Display source and active comments", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'c', "calculator", "Calculator", "A little calculator -- press ESC to exit", XPP_KIND_VIEW, CommandCategory::Tools, true, true, {}},
    {FILE_MENU, 't', "transpose", "Transpose data", "Transpose storage", XPP_KIND_DATA, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'g', "getparset", "Named parameter sets", "Set predefined parameters", XPP_KIND_SETTING, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'o', "copyset", "Copy parameter set line", "Copy the current values as a named set line for the .ode", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'd', "record", "Record steps", "Record the steps you take to a .recx file; again to stop and save it", XPP_KIND_DATA, CommandCategory::Panels, false, true, {}},
    {FILE_MENU, 'y', "play", "Play recording…", "Play a recording (.recx): its model, then its steps as they were taken", XPP_KIND_DATA, CommandCategory::Panels, false, true, {}},
    {MAIN_MENU, PAGE_KEY, "keymapeditor", "Keyboard shortcuts…", "Change the keys of the commands and pin commands to the toolbar", XPP_KIND_VIEW, CommandCategory::Panels, false, true, {}},
    {NUM_MENU, 'k', "lookup", "Lookup tables", "Modify lookup tables", XPP_KIND_DATA, CommandCategory::Panels, false, true, {}},
    {MAIN_MENU, 'f', "file", "File", "Quit, save stuff, etc", XPP_KIND_VIEW, CommandCategory::Layer, false, true, {}},
    {MAIN_MENU, 'u', "numerics", "Numerics", "Numerics options", XPP_KIND_VIEW, CommandCategory::Layer, false, true, {}},
    {NUM_MENU, '\033', "exit", "Return to main shortcuts", "Return to main menu", XPP_KIND_VIEW, CommandCategory::Layer, false, true, {}},
}};

/* the row of item `id` of menu `which`, nullptr for none */
constexpr const CommandRow *find_command(int which, std::string_view id)
{
  for (const CommandRow &row : COMMANDS)
    if (row.menu == which && row.id == id) return &row;
  return nullptr;
}

/* the row of the command `id`, whichever menu it is in, nullptr for none (an
   id is in one menu only: tests/test_keymap.cpp checks it): what a keymap
   names a command by */
constexpr const CommandRow *find_command_by_id(std::string_view id)
{
  for (const CommandRow &row : COMMANDS)
    if (row.id == id) return &row;
  return nullptr;
}

/* the row of the key `ch` of menu `which`, nullptr for none */
constexpr const CommandRow *find_command_by_key(int which, int ch)
{
  for (const CommandRow &row : COMMANDS)
    if (row.key != PAGE_KEY && row.menu == which && static_cast<unsigned char>(row.key) == ch) return &row;
  return nullptr;
}

/* the row of the main-menu item that enters menu `which`'s shortcut layer (File, Numerics), nullptr for the
   main menu itself */
constexpr const CommandRow *layer_entry(int which)
{
  return which == FILE_MENU ? find_command(MAIN_MENU, "file") : which == NUM_MENU ? find_command(MAIN_MENU, "numerics") : nullptr;
}

}  // namespace xpp

#endif
