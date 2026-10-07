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
   Layer is not a group: it marks the three items that only switch the
   legacy one-letter shortcut layer (File, Numerics and Return to main
   shortcuts), which the page shows as a state, never as a command */
enum class CommandCategory { Run, Files, Analysis, Plot, Tools, Layer };

struct CategoryInfo {
  CommandCategory category;
  std::string_view id, label;
  bool listed; /* a sidebar group; false for Layer */
};

inline constexpr std::array<CategoryInfo, 6> COMMAND_CATEGORIES = {{
  {CommandCategory::Run, "run", "Run", true},
  {CommandCategory::Files, "files", "Files", true},
  {CommandCategory::Analysis, "analysis", "Analysis", true},
  {CommandCategory::Plot, "plot", "Plot", true},
  {CommandCategory::Tools, "tools", "Tools", true},
  {CommandCategory::Layer, "layer", "Shortcut layer", false}}};

/* the most keys one command has (a binding and one alternative, as redo has
   Ctrl+Shift+Z and Ctrl+Y in the design); an empty entry is no key */
inline constexpr std::size_t MAX_DEFAULT_KEYS = 2;

/* The menu's name in the protocol (`menu` of a key command), indexed by
   MAIN_MENU, FILE_MENU, NUM_MENU: state's menu number names one */
inline constexpr std::array<std::string_view, 3> MENU_NAMES = {"main", "file", "num"};
static_assert(MAIN_MENU == 0 && FILE_MENU == 1 && NUM_MENU == 2, "MENU_NAMES is indexed by the menu numbers");

struct CommandRow {
  int menu;                /* MAIN_MENU, FILE_MENU or NUM_MENU: where the legacy shortcut lives */
  char key;                /* the legacy one-letter key in that menu ('\033' is Esc) */
  std::string_view id;     /* stable: the protocol's `item`, recordings, the page's calls */
  std::string_view label;  /* plain text for buttons, menus and search */
  std::string_view description; /* one line */
  char kind;               /* XPP_KIND_*: what the command needs (docs/protocol.md "Action kinds") */
  CommandCategory category;
  bool pinnable;           /* may sit in the quick-access toolbar */
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
inline constexpr std::array<CommandRow, 57> COMMANDS = {{
    {MAIN_MENU, 'i', "initialconds", "Initial conditions", "Integrate the equations", XPP_KIND_COMPUTE, CommandCategory::Run, true, {}},
    {MAIN_MENU, 'c', "continue", "Continue integration", "Continue integration for specified time", XPP_KIND_COMPUTE, CommandCategory::Run, true, {}},
    {MAIN_MENU, 'p', "parameters", "Parameters", "Change problem parameters", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 't', "total", "Integration duration", "Total time to integrate eqns", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 's', "start", "Start time", "Starting time -- T0", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 'r', "transient", "Transient", "Time to integrate before storing", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 'd', "dt", "Time step", "Time step to use", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 'm', "method", "Solver method", "Integration method", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 'o', "store_every", "Output stride", "Number of steps per plotted point", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {NUM_MENU, 'b', "bounds", "Bounds", "Maximum allowed size of any variable", XPP_KIND_SETTING, CommandCategory::Run, true, {}},
    {FILE_MENU, 'm', "openmodel", "Open model…", "Load another model in place of this one", XPP_KIND_DATA, CommandCategory::Files, true, {"Ctrl+O"}},
    {FILE_MENU, 'n', "opensession", "Open session…", "Open a session file: its model, values, windows, data and diagram", XPP_KIND_DATA, CommandCategory::Files, true, {}},
    {FILE_MENU, 'v', "savesession", "Save session as…", "Save everything to one session file (.snapx) to continue later", XPP_KIND_DATA, CommandCategory::Files, true, {"Ctrl+S"}},
    {FILE_MENU, 'e', "reload", "Reload model", "Read the model's file again, keeping the values", XPP_KIND_DATA, CommandCategory::Files, true, {}},
    {FILE_MENU, 'r', "importset", "Import XPPAUT settings", "Import a set file XPPAUT wrote (values, numerics, the active window)", XPP_KIND_DATA, CommandCategory::Files, true, {}},
    {FILE_MENU, 's', "saveinfo", "Export simulation information", "Save info about simulation in human readable format", XPP_KIND_DATA, CommandCategory::Files, true, {}},
    {FILE_MENU, 'q', "quit", "Quit", "Quit the application, with an option to save this session", XPP_KIND_CONTROL, CommandCategory::Files, false, {}},
    {MAIN_MENU, 's', "singpts", "Equilibria and stability", "Find fixed points and stability", XPP_KIND_COMPUTE, CommandCategory::Analysis, true, {}},
    {MAIN_MENU, 'n', "nullcline", "Nullclines", "Draw nullclines", XPP_KIND_VIEW, CommandCategory::Analysis, true, {}},
    {MAIN_MENU, 'd', "dirfield", "Direction fields and flow", "Direction fields and flows of the phaseplane", XPP_KIND_VIEW, CommandCategory::Analysis, true, {}},
    {MAIN_MENU, 'b', "bndryval", "Boundary-value solver", "Run boundary value solver", XPP_KIND_COMPUTE, CommandCategory::Analysis, true, {}},
    {FILE_MENU, 'a', "auto", "AUTO continuation", "Run AUTO, the bifurcation package", XPP_KIND_VIEW, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'n', "ncline", "Nullcline settings", "Mesh for nullclines", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'i', "singpt", "Equilibrium settings", "Numerical parameters for fixed points", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'e', "delay", "Delay settings", "Maximum delay and delay related stuff", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'h', "stochastic", "Stochastic analysis", "Curve fitting, FFT, mean, variance, seed, etc", XPP_KIND_DATA, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'p', "poincare", "Poincaré map", "Define Poincare map parameters", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'u', "ruelle", "Ruelle plot", "Define shifted plots", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'v', "bndval", "Boundary-value settings", "Numerical setup for boundary value solver", XPP_KIND_SETTING, CommandCategory::Analysis, true, {}},
    {NUM_MENU, 'a', "averaging", "Adjoint and averaging", "Compute adjoint and averaged functions", XPP_KIND_DATA, CommandCategory::Analysis, true, {}},
    {MAIN_MENU, 'w', "window", "Zoom and view", "Change the size of two-dimensional view", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'a', "phasespace", "Phase space", "Set up periodic/torus phase space", XPP_KIND_DATA, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'g', "graphic", "Curves and export", "Adding graphs,hard copy, etc", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'v', "viewaxes", "Plot axes", "Change 2 or 3d views", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'x', "xivst", "Variable vs time", "Plot variable vs time", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 't', "text", "Labels and annotations", "Add fancy text and lines,arrows", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'm', "makewindow", "Plot windows", "Create other windows", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'r', "restore", "Redraw plot", "Redraw the graph", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, '3', "3dparams", "3D view", "Set parameters for 3D view", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'e', "erase", "Clear plot", "Clear screen", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {MAIN_MENU, 'k', "kinescope", "Captured frames", "Take snapshots of the screen", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {NUM_MENU, 'c', "colorcode", "Color by value", "Color trajectories according to velocity,etc", XPP_KIND_VIEW, CommandCategory::Plot, true, {}},
    {FILE_MENU, 'p', "source", "Model source", "Display source and active comments", XPP_KIND_VIEW, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'c', "calculator", "Calculator", "A little calculator -- press ESC to exit", XPP_KIND_VIEW, CommandCategory::Tools, true, {}},
    {FILE_MENU, 't', "transpose", "Transpose data", "Transpose storage", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'g', "getparset", "Named parameter sets", "Set predefined parameters", XPP_KIND_SETTING, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'l', "clone", "Clone model", "Clone the ode file", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'x', "xpprc", "Preferences", "Edit your .xpprc preferences file", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'u', "tutorial", "Tutorial", "Run a quick tutorial on XPPAUT", XPP_KIND_VIEW, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'o', "copyset", "Copy parameter set line", "Copy the current values as a named set line for the .ode", XPP_KIND_VIEW, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'd', "record", "Record steps", "Record the steps you take to a .recx file; again to stop and save it", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'y', "play", "Play recording…", "Play a recording (.recx): its model, then its steps as they were taken", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {FILE_MENU, 'h', "help", "Help", "Browser help", XPP_KIND_VIEW, CommandCategory::Tools, true, {}},
    {NUM_MENU, 'k', "lookup", "Lookup tables", "Modify lookup tables", XPP_KIND_DATA, CommandCategory::Tools, true, {}},
    {MAIN_MENU, 'f', "file", "File", "Quit, save stuff, etc", XPP_KIND_VIEW, CommandCategory::Layer, false, {}},
    {MAIN_MENU, 'u', "numerics", "Numerics", "Numerics options", XPP_KIND_VIEW, CommandCategory::Layer, false, {}},
    {NUM_MENU, '\033', "exit", "Return to main shortcuts", "Return to main menu", XPP_KIND_VIEW, CommandCategory::Layer, false, {}},
}};

/* the row of item `id` of menu `which`, nullptr for none */
constexpr const CommandRow *find_command(int which, std::string_view id)
{
  for (const CommandRow &row : COMMANDS)
    if (row.menu == which && row.id == id) return &row;
  return nullptr;
}

/* the row of the key `ch` of menu `which`, nullptr for none */
constexpr const CommandRow *find_command_by_key(int which, int ch)
{
  for (const CommandRow &row : COMMANDS)
    if (row.menu == which && static_cast<unsigned char>(row.key) == ch) return &row;
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
