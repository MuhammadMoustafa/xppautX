#pragma once

#include "command_category.h"

#include <optional>
#include <string>
#include <string_view>

namespace xpp {

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
  /* one XPP_KIND_* letter per item, parallel to keys (every menu of
     menus.cpp); NULL for a list a command builds as it asks (a parameter
     set, a table, a marker): its items take the kind of that command */
  const char *kinds = nullptr;
} XppMenu;

/* What menu_choose returns, plus the item's index, for an item of a long list that has no key (the
   named sets past the 26th): the page answers such a menu with the item's `index` */
inline constexpr int MENU_UNKEYED = 0x100;

/* the index of the item of m that key ch picks (a MENU_UNKEYED + index picks that item), -1 for none;
   m->keys may hold fewer keys than m->n items: the rest have none */
int menu_index(const XppMenu *m, int ch);
/* the kind of the item of m that key ch picks, 0 for none */

extern const XppMenu menu_method, menu_save_what;

/* AUTO's pop-up menus; menu_auto_special has no title (its caller's) */
extern const XppMenu menu_auto_special;

} // namespace xpp
