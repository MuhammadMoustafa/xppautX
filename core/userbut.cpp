#include "userbut.h"
#include "session.h"
#include "xpp_log.h"

#include <string>
#include "kbs.h"
#include <iterator>

namespace xpp {




namespace {

/* Split "name:keys" into its two parts. */
void get_button_info(std::string_view s, std::string &bname, std::string &sc)
{
  bname.clear();
  sc.clear();
  bool after_colon = false;
  for (char c : s) {
    if (c == ':') {
      after_colon = true;
    } else if (after_colon) {
      sc.push_back(c);
    } else {
      bname.push_back(c);
    }
  }
}

int find_kbs(const std::string &sc)
{
  int i = 0;
  while (true) {
    if (sc == kbs[i].seq) return kbs[i].com;
    i++;
    if (kbs[i].com == 0) return -1;
  }
}

} // namespace

bool is_button_sequence(std::string_view spec)
{
  std::string bname, sc;
  get_button_info(spec, bname, sc);
  return !sc.empty() && find_kbs(sc) != -1;
}

const char *add_user_button(xpp::Session &s, std::string_view spec, bool apply)
{
  std::string bname, sc;
  get_button_info(spec, bname, sc);
  if (bname.empty() || sc.empty()) return "must be a name and a command sequence, name:keys";
  int z = find_kbs(sc);
  if (z == -1) return "names a command sequence that no button runs";
  if (s.nuserbut >= USERBUTMAX) return "is one button too many";
  if (!apply) return nullptr;
  /* Don't add buttons with the same functionality twice. */
  for (int i = 0; i < s.nuserbut; i++) {
    if (s.userbut[i].com == z) return nullptr;
  }
  s.userbut[s.nuserbut].com = z;
  s.userbut[s.nuserbut].bname = bname;
  xpp::log(XPP_LOG_INFO, " added button({})  -- {} {}\n", s.nuserbut, s.userbut[s.nuserbut].bname,
           s.userbut[s.nuserbut].com);
  s.nuserbut++;
  return nullptr;
}

} // namespace xpp
