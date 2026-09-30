#ifndef _menus_h_
#define _menus_h_
#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU 0
#define FILE_MENU 1
#define NUM_MENU 2
#define MAIN_ENTRIES 20
#define FILE_ENTRIES 20
#define NUM_ENTRIES 18

extern const char *const main_menu[];
extern const char *const num_menu[];
extern const char *const file_menu[];
extern const char *const main_hint[];
extern const char *const file_hint[];
extern const char *const num_hint[];
extern const char *const auto_hint[];
extern const char *const no_hint[];
extern const char *const aaxes_hint[];
extern const char *const aspecial_hint[];
extern const char *const arun_hint[];

/* key strings for the three main-window menus, one key per entry */
extern const char *const main_menu_keys;
extern const char *const num_menu_keys;
extern const char *const file_menu_keys;

/* The kind of an action (W95), one letter: what the page may still do while
   a computation runs (docs/protocol.md "Action kinds"). A control action
   (Abort, Quit, an answer) always goes; a view action only changes what is
   shown and goes during a computation too (the core runs it after the
   computation); a setting (W106: a parameter, an initial or boundary
   condition, a delay, the numerics, AUTO's forms) goes during a computation
   too and is applied when it ends, never to the run in progress; a data
   action (saving, loading) and a computation (anything that starts one)
   are refused during one. */
#define XPP_KIND_CONTROL 'c'
#define XPP_KIND_VIEW 'v'
#define XPP_KIND_SETTING 's'
#define XPP_KIND_DATA 'd'
#define XPP_KIND_COMPUTE 'x'

/* the kinds of the three main-window menus' items, parallel to their keys;
   an item that opens a pop-up menu has the least restrictive kind among
   that menu's items: it opens when any of them could run (during a
   computation it opens once the computation ends) */
extern const char *const main_menu_kinds;
extern const char *const num_menu_kinds;
extern const char *const file_menu_kinds;

/* the kind of main-window key ch in main-window menu `which` (MAIN_MENU,
   FILE_MENU, NUM_MENU), 0 when that menu has no such key */
char xpp_main_menu_kind(int which, int ch);
/* the item of main-window menu `which` that key ch picks, NULL for none */
const char *xpp_main_menu_item(int which, int ch);

/* A pop-up menu as data. Core code asks the front end to show one with
   menu_choose() and gets back the chosen key. Item i usually runs
   run_the_commands(first_cmd + i); first_cmd is -1 when the caller handles
   the choice itself. width and row are the X11 pop_up_list layout (maximum
   label width, and the text row the list opens at; -1 is the top edge). */
typedef struct XppMenu {
  const char *name;  /* stable identifier for front ends */
  const char *title;
  int n;
  const char *const *items;
  const char *keys;
  const char *const *hints;
  int first_cmd;
  /* one XPP_KIND_* letter per item, parallel to keys (every menu of
     menus.cpp); NULL for a list a command builds as it asks (a parameter
     set, a table, a marker): its items take the kind of that command */
  const char *kinds;
} XppMenu;

/* The windows' own key layers (protocol: {"cmd":"key","win":...}), one XppMenu
   each, the keys defined here and nowhere else; the enums number the items in
   the menus' order (xpp_menu_index gives the item a key picks). */
enum AutoWindowKey { AK_PARAM, AK_AXES, AK_NUMERICS, AK_RUN, AK_GRAB, AK_USR, AK_CLEAR, AK_REDRAW, AK_FILE };
enum BrowserWindowKey { BK_FIND, BK_GET, BK_REPLACE, BK_UNREPLACE, BK_TABLE, BK_FIRST, BK_LAST, BK_RESTORE,
  BK_ADDCOL, BK_DELCOL, BK_LOAD, BK_WRITE };
enum AniWindowKey { NK_FILE, NK_GO, NK_RESET, NK_SKIP, NK_MPEG, NK_FLY, NK_GRAB };
enum AplotWindowKey { PK_REDRAW, PK_EDIT, PK_FIT, PK_RANGE, PK_PRINT, PK_GIF };
enum EquilibriumWindowKey { EK_IMPORT };

/* the index of the item of m that key ch picks, -1 for none */
int xpp_menu_index(const XppMenu *m, int ch);
/* the kind of the item of m that key ch picks, 0 for none */
char xpp_menu_kind(const XppMenu *m, int ch);

/* A window's key layer: the protocol's `win` name, its menu, and the page's
   name for each item (hello.windows, docs/protocol.md "Window keys"). */
typedef struct XppWindowLayer {
  const char *win;
  const XppMenu *menu;
  const char *const *ids;
} XppWindowLayer;
#define XPP_WINDOW_LAYERS 5
extern const XppWindowLayer xpp_window_layers[XPP_WINDOW_LAYERS];
/* the layer of window `win`, NULL for none */
const XppWindowLayer *xpp_window_layer(const char *win);

extern const XppMenu menu_auto_window, menu_browser_window, menu_ani_window, menu_aplot_window,
  menu_equilibrium_window;

extern const XppMenu menu_integrate, menu_nullclines, menu_freeze_cline,
  menu_dirfield, menu_window, menu_torus, menu_kinescope, menu_curves,
  menu_freeze, menu_freeze_off, menu_freeze_key, menu_colormap,
  menu_windows, menu_windows_simoff, menu_text, menu_text_edit,
  menu_equilibria, menu_view, menu_bvp, menu_stochastic, menu_poincare,
  menu_color_code, menu_adjoint, menu_lookup, menu_method, menu_save_what;

/* AUTO's pop-up menus; menu_auto_special has no title (its caller's) */
extern const XppMenu menu_auto_plot_type, menu_auto_mark, menu_auto_start, menu_auto_torus,
  menu_auto_per_doub, menu_auto_periodic, menu_auto_hopf, menu_auto_branch, menu_auto_file,
  menu_auto_special;



#ifdef __cplusplus
}

#include <string>
#include <string_view>

/* an item as a person reads it, its key's parentheses gone: "(G)o" is
   "Go" (a recording's step labels, W59a) */
std::string xpp_menu_label(std::string_view item);
#endif
#endif
