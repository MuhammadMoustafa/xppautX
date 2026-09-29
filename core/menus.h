#ifndef _menus_h_
#define _menus_h_
#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU 0
#define FILE_MENU 1
#define NUM_MENU 2
#define MAIN_ENTRIES 20
#define FILE_ENTRIES 18
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
#endif
#endif
