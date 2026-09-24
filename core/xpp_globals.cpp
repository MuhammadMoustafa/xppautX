/* Definitions for xpp_globals.h. */
#include "xpp_globals.h"

XppProgram program = {
    0,       /* interactive: set by the front end once it is up */
    nullptr, /* auto_dir: HOME until xppautX makes a private one */
    0.0f,    /* version_major, version_minor: set when the model loads */
    0.0f,
    0,       /* tutorial: @ tutorial=1 shows it at start-up */
};
