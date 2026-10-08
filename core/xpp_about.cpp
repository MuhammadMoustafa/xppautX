/* The About text (xpp_about.h). The Makefile passes the version and commit
   to this object; a build from a tree with no tags and no git says "dev". */
#include "xpp_about.h"
#include "ui_json.h"
#include <initializer_list>
#include <utility>

#ifndef XPPAUTX_VERSION
#define XPPAUTX_VERSION "dev"
#endif
#ifndef XPPAUTX_COMMIT
#define XPPAUTX_COMMIT "unknown"
#endif
#if defined(__clang__)
#define XPPAUTX_COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#define XPPAUTX_COMPILER "gcc " __VERSION__
#else
#define XPPAUTX_COMPILER "unknown"
#endif

namespace {

/* the About links: label and target, written once; the box, the page and
   the check of a link the page asks to open all read them from here */
constexpr std::string_view EMAIL = "mailto:muhammadmoustafa22@gmail.com";
constexpr std::string_view GITHUB = "https://github.com/MuhammadMoustafa";
constexpr std::string_view LINKEDIN = "https://www.linkedin.com/in/muhammad-ahmad-62743a125/";
constexpr std::string_view SOURCE = "https://github.com/MuhammadMoustafa/xppautX";
constexpr std::string_view ISSUES = "https://github.com/MuhammadMoustafa/xppautX/issues";
constexpr std::string_view SEPARATOR = " \u00b7 ";

void add_links(AboutLine &line, std::initializer_list<std::pair<std::string_view, std::string_view>> links)
{
    for (const auto &[label, url] : links) {
        if (!line.parts.empty()) line.parts.push_back({std::string(SEPARATOR), ""});
        line.parts.push_back({std::string(label), std::string(url)});
    }
}

std::vector<AboutLine> make_lines()
{
    std::vector<AboutLine> lines;
    lines.push_back({{{"xppautX " XPPAUTX_VERSION, ""}}});
    lines.push_back({{{"by Muhammad Ahmad", ""}}});
    AboutLine contact, project;
    add_links(contact, {{"Email", EMAIL}, {"GitHub", GITHUB}, {"LinkedIn", LINKEDIN}});
    add_links(project, {{"Source", SOURCE}, {"Report a problem", ISSUES}});
    lines.push_back(contact);
    lines.push_back(project);
    lines.push_back({});
    lines.push_back({{{"Based on XPPAUT 8.0 by G. Bard Ermentrout (University of Pittsburgh).", ""}}});
    lines.push_back({{{"GPL v2, as XPPAUT; no warranty (see LICENSE).", ""}}});
    lines.push_back({});
    lines.push_back({{{"Commit " XPPAUTX_COMMIT " \u00b7 Compiler " XPPAUTX_COMPILER " \u00b7 Protocol " JSON_UI_STR(JSON_UI_PROTOCOL), ""}}});
    return lines;
}

/* XML-escaped, for the native boxes' markup */
std::string escaped(std::string_view s)
{
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

} // namespace

const std::vector<AboutLine> &xpp_about_lines()
{
    static const std::vector<AboutLine> lines = make_lines();
    return lines;
}

std::string xpp_about_markup()
{
    std::string out;
    for (const AboutLine &line : xpp_about_lines()) {
        if (!out.empty()) out += '\n';
        for (const AboutPart &p : line.parts) {
            if (p.url.empty()) out += escaped(p.text);
            else out += "<a href=\"" + escaped(p.url) + "\">" + escaped(p.text) + "</a>";
        }
    }
    return out;
}

bool xpp_about_has_link(std::string_view url)
{
    for (const AboutLine &line : xpp_about_lines())
        for (const AboutPart &p : line.parts)
            if (!p.url.empty() && p.url == url) return true;
    return false;
}

const char *xpp_version_string() { return XPPAUTX_VERSION; }
