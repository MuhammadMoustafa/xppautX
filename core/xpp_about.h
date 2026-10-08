/* The About text (W107, W224): the one place it is written. The desktop
   window's Help > About box shows it and hello's "about" carries the same
   lines to the page's Help > About, so they cannot differ. */
#ifndef XPP_ABOUT_H
#define XPP_ABOUT_H

#include <string>
#include <string_view>
#include <vector>

/* a run of the About text: plain, or a link when `url` is not empty */
struct AboutPart {
    std::string text;
    std::string url;
};

struct AboutLine {
    std::vector<AboutPart> parts; /* none: a blank line */
};

/* the version line, the author, the contact and project links, the credit
   and licence, and the build (commit, compiler, protocol); the links'
   labels and targets are written once, here */
const std::vector<AboutLine> &xpp_about_lines();

/* the same lines as the native boxes' markup: a link is
   <a href="url">label</a>, the rest XML-escaped; lines separated by "\n" */
std::string xpp_about_markup();

/* whether `url` is exactly the target of one of the About links: the only
   addresses the window opens in the system browser */
bool xpp_about_has_link(std::string_view url);

/* what --version prints after "xppautX ": the release tag, or "dev" */
const char *xpp_version_string();

#endif
