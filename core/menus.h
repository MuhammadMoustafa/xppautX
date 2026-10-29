#pragma once

#include "command_category.h"

#include <optional>
#include <string>
#include <string_view>

namespace xpp {

/* A list of choices as data. Core code asks the front end to show one with
   menu_choose() (a `choice` ask) and gets back the chosen key. */
typedef struct XppMenu {
  const char *name;  /* stable identifier for front ends */
  const char *title;
  int n;
  const char *const *items;
  const char *keys;
} XppMenu;

/* What menu_choose returns, plus the item's index, for an item of a long list that has no key (the
   named sets past the 26th): the page answers such a menu with the item's `index` */
inline constexpr int MENU_UNKEYED = 0x100;

/* the index of the item of m that key ch picks (a MENU_UNKEYED + index picks that item), -1 for none;
   m->keys may hold fewer keys than m->n items: the rest have none */
int menu_index(const XppMenu *m, int ch);

extern const XppMenu menu_method, menu_save_what;

/* AUTO's pop-up menus; menu_auto_special has no title (its caller's) */
extern const XppMenu menu_auto_special;

} // namespace xpp
