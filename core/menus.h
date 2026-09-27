#ifndef _menus_h_
#define _menus_h_
#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU 0
#define FILE_MENU 1
#define NUM_MENU 2
#define MAIN_ENTRIES 20
#define FILE_ENTRIES 13
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
extern const char *const afile_hint[];
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

extern const XppMenu menu_integrate, menu_nullclines, menu_freeze_cline,
  menu_dirfield, menu_window, menu_torus, menu_kinescope, menu_curves,
  menu_freeze, menu_freeze_off, menu_freeze_key, menu_colormap,
  menu_windows, menu_windows_simoff, menu_text, menu_text_edit,
  menu_equilibria, menu_view, menu_bvp, menu_stochastic, menu_poincare,
  menu_color_code, menu_adjoint, menu_lookup, menu_method;



#ifdef __cplusplus
}
#endif
#endif
