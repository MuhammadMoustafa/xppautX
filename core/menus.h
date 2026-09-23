#ifndef _menus_h_
#define _menus_h_
#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU 0
#define FILE_MENU 1
#define NUM_MENU 2
#define MAIN_ENTRIES 20
#define FILE_ENTRIES 16
#define NUM_ENTRIES 18

extern char *main_menu[];
extern char *num_menu[];
extern char *fileon_menu[];
extern char *fileoff_menu[];
extern char *main_hint[];
extern char *file_hint[];
extern char *num_hint[];
extern char *null_hint[];
extern char *null_freeze[];
extern char *ic_hint[];
extern char *wind_hint[];
extern char *flow_hint[];
extern char *phas_hint[];
extern char *kin_hint[];
extern char *graf_hint[];
extern char *cmap_hint[];
extern char *frz_hint[];
extern char *stoch_hint[];
extern char *bvp_hint[];
extern char *adj_hint[];
extern char *map_hint[];
extern char *view_hint[];
extern char *half_hint[];
extern char *text_hint[];
extern char *edit_hint[];
extern char *sing_hint[];
extern char *meth_hint[];
extern char *color_hint[];
extern char *tab_hint[];
extern char *edrh_hint[];
extern char *auto_hint[];
extern char *no_hint[];
extern char *aaxes_hint[];
extern char *afile_hint[];
extern char *aspecial_hint[];
extern char *arun_hint[];
extern char *browse_hint[];

/* key strings for the three main-window menus, one key per entry */
extern char main_menu_keys[];
extern char num_menu_keys[];
extern char file_menu_keys[];

/* A pop-up menu as data. Core code asks the front end to show one with
   menu_choose() and gets back the chosen key. Item i usually runs
   run_the_commands(first_cmd + i); first_cmd is -1 when the caller handles
   the choice itself. width and row are the X11 pop_up_list layout (maximum
   label width, and the text row the list opens at; -1 is the top edge). */
typedef struct XppMenu {
  const char *name;  /* stable identifier for front ends */
  char *title;
  int n;
  char **items;
  char *keys;
  char **hints;
  int first_cmd;
  int width, row;
} XppMenu;

extern const XppMenu menu_integrate, menu_nullclines, menu_freeze_cline,
  menu_dirfield, menu_window, menu_torus, menu_kinescope, menu_curves,
  menu_freeze, menu_freeze_off, menu_freeze_key, menu_colormap,
  menu_windows, menu_windows_simoff, menu_text, menu_text_edit,
  menu_equilibria, menu_view, menu_bvp, menu_stochastic, menu_poincare,
  menu_color_code, menu_adjoint, menu_lookup, menu_method, menu_edit_rhs;

/* every menu above, for front ends that build their menus up front */
extern const XppMenu *const xpp_menus[];
extern const int xpp_menu_count;

#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
typedef struct {
  Window base,title;
  Window w[25];
  char key[25];
  char **names;
  char **hints;
  int n,visible;
} MENUDEF;





#endif /* Xlib.h */

#ifdef __cplusplus
}
#endif
#endif
