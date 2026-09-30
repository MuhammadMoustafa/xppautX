/* The About text (xpp_about.h). The Makefile passes the version and commit
   to this object; a build from a tree with no tags and no git says "dev". */
#include "xpp_about.h"
#include "ui_json.h"

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

const std::string &xpp_about_text()
{
    static const std::string text =
        "xppautX " XPPAUTX_VERSION "\n"
        "Commit " XPPAUTX_COMMIT "\n"
        "Compiler: " XPPAUTX_COMPILER "\n"
        "Protocol " JSON_UI_STR(JSON_UI_PROTOCOL) "\n\n"
        "XPPAUT is by Bard Ermentrout; xppautX is its modernised fork.\n"
        "GPL v2, as XPPAUT; no warranty (see LICENSE).\n\n"
        "Author: Muhammad Ahmad\n"
        "Email: muhammadmoustafa22@gmail.com\n"
        "GitHub: https://github.com/MuhammadMoustafa\n"
        "LinkedIn: https://www.linkedin.com/in/muhammad-ahmad-62743a125/\n\n"
        "Source: https://github.com/MuhammadMoustafa/xppautX\n"
        "Report a problem: https://github.com/MuhammadMoustafa/xppautX/issues";
    return text;
}

const char *xpp_version_string() { return XPPAUTX_VERSION; }
