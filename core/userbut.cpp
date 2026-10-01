#include "userbut.h"
#include "session.h"
#include "xpp_log.h"

#include <string>
#include "kbs.h"

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

void add_user_button(xpp::Session &s, std::string_view spec)
{
  if (s.nuserbut >= USERBUTMAX) return;
  if (spec.empty()) return;
  std::string bname, sc;
  get_button_info(spec, bname, sc);
  if (bname.empty() || sc.empty()) return;
  int z = find_kbs(sc);
  if (z == -1) {
    xpp::log(XPP_LOG_WARN, "{} - not implemented\n", sc);
    return;
  }
  /* Don't add buttons with the same functionality twice. */
  for (int i = 0; i < s.nuserbut; i++) {
    if (s.userbut[i].com == z) return;
  }
  s.userbut[s.nuserbut].com = z;
  s.userbut[s.nuserbut].bname = bname;
  xpp::log(XPP_LOG_INFO, " added button({})  -- {} {}\n", s.nuserbut, s.userbut[s.nuserbut].bname,
           s.userbut[s.nuserbut].com);
  s.nuserbut++;
}

} // namespace xpp
