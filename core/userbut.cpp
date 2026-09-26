#include "userbut.h"
#include "xpp_log.h"

#include <string>
#include "kbs.h"
#include "xpp_ui.h"
#include "xpp_io.h"


int nuserbut=0;

USERBUT userbut[USERBUTMAX];

namespace {

/* Split "name:keys" into its two parts. */
void get_button_info(const std::string &s, std::string &bname, std::string &sc)
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

void add_user_button(const char *s)
{
  if (nuserbut >= USERBUTMAX) return;
  if (s == nullptr || s[0] == '\0') return;
  std::string bname, sc;
  get_button_info(s, bname, sc);
  if (bname.empty() || sc.empty()) return;
  int z = find_kbs(sc);
  if (z == -1) {
    xpp::log(XPP_LOG_WARN, "{} - not implemented\n", sc);
    return;
  }
  /* Don't add buttons with the same functionality twice. */
  for (int i = 0; i < nuserbut; i++) {
    if (userbut[i].com == z) return;
  }
  userbut[nuserbut].com = z;
  XPP_STRCPY(userbut[nuserbut].bname, bname.c_str());
  xpp::log(XPP_LOG_INFO, " added button({})  -- {} {}\n",
           nuserbut, userbut[nuserbut].bname, userbut[nuserbut].com);
  nuserbut++;
}
