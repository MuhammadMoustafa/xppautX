/* The pop-up menus a command asks one of its choices with (menus.h). */
#include "menus.h"
#include "auto_codes.h"

#include <string>
#include <string_view>

namespace xpp {

namespace {

/* the item count of a menu that has one key per item; any other menu does not compile (a throw is no constant
   expression) */
consteval int menu_count(size_t items, size_t keys)
{
    if (items + 1 != keys) throw "a menu needs one key per item";
    return static_cast<int>(items);
}

} // namespace

/* a menu's initializer, one key per item checked when it compiles */
#define XPP_MENU(name, title, items, keys) {name, title, menu_count(sizeof(items) / sizeof(items[0]), sizeof(keys)), items, keys}

/* pop-up menus, formerly static arrays inside the menudrive.c handlers */

/* (X)tra is defined but not shown: the menu has 6 entries */

static const char *const meth_items[]={"(D)iscrete","(E)uler","(M)od. Euler",
  "(R)unge-Kutta","(A)dams","(G)ear","(V)olterra","(B)ackEul",
  "(Q)ualst.RK4","(S)tiff","(C)Vode","DoPri(5)","DoPri(8)3",
  "Rosen(2)3","sYmplectic"};

/* (F)reeze and (C)olormap open the next two menus instead of a command */
/* (E)dit opens menu_text_edit */
/* Save data (browse_data.cpp data_write): what it writes; the format
   comes next, from the data formats' registry (data_formats.h) */
static const char *const save_what_items[]={"The data table","What the plot shows"};
const XppMenu menu_save_what = XPP_MENU("save_what", "Save data", save_what_items, "tp");
/* sets METHOD directly */
const XppMenu menu_method = XPP_MENU("method", "Method", meth_items, "demragvbqsc582y");

int menu_index(const XppMenu *m, int ch)
{
  if (ch >= MENU_UNKEYED) return ch - MENU_UNKEYED < m->n ? ch - MENU_UNKEYED : -1;
  for (int i = 0; i < m->n && m->keys[i]; i++)
    if (ch == m->keys[i])
      return i;
  return -1;
}



/* AUTO's pop-up menus (W96). The protocol names each one "auto": that is the
   name the page has always been sent. Nothing about them is dynamic but the
   default item, which the caller passes to menu_choose, and the Special
   menu's title (menu_auto_special copied with its own). */

/* AUTO's File menu: session save/open and foreign diagram import. */

// the grab-by-type menu: its items are the point types' names (query_special shows their labels)
// the grab-by-type menu: its items are the point types' labels, in the order auto_grab_kinds (auto_nox.cpp) gives
static const char *const auto_special_items[]={
  auto_point_kind_label(AutoPointKind::BranchPoint).data(), auto_point_kind_label(AutoPointKind::End).data(),
  auto_point_kind_label(AutoPointKind::Hopf).data(), auto_point_kind_label(AutoPointKind::Fold).data(),
  auto_point_kind_label(AutoPointKind::MaxSteps).data(), auto_point_kind_label(AutoPointKind::PeriodDoubling).data(),
  auto_point_kind_label(AutoPointKind::NeimarkSacker).data(), auto_point_kind_label(AutoPointKind::UserZero).data()};
const XppMenu menu_auto_special = XPP_MENU("auto", "", auto_special_items, "behlmptu");

} // namespace xpp
