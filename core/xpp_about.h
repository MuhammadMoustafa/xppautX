/* The About text (W107): the one place it is written. The desktop
   window's Help > About box shows it and hello's "about" carries the same
   text to the page's Help > About, so they cannot differ. */
#ifndef XPP_ABOUT_H
#define XPP_ABOUT_H

#include <string>

/* version, commit, compiler, protocol, the credit and licence, the author,
   his contact details and where to report a problem; lines separated by
   "\n", no trailing newline */
const std::string &xpp_about_text();

/* what --version prints after "xppautX ": the release tag, or "dev" */
const char *xpp_version_string();

#endif
