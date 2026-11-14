/* The About text (W107, W224): the one place it is written. The desktop
   page's Help > About shows it, from hello's "about". */
#pragma once

#include "ui_json.h" /* AboutLine and AboutPart, the text's own types, are hello's */

#include <string>
#include <string_view>
#include <vector>

/* the version line, the author, the contact and project links, the credit
   and licence, and the build (commit, compiler, protocol); the links'
   labels and targets are written once, here */
const std::vector<AboutLine> &xpp_about_lines();

/* whether `url` is exactly the target of one of the About links: the only
   addresses the window opens in the system browser */
bool xpp_about_has_link(std::string_view url);

/* what --version prints after "xppautX ": the release tag, or "dev" */
const char *xpp_version_string();

