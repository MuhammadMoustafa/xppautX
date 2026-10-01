# The options file

XPPAUT's options file (an `option <filename>` line in an ODE file, else `default.opt`) never took effect in XPPAUT itself, and xppautX does not support it: an ODE file with an `option` line is refused at load, with its file and line. Write the settings as `@ name=value` lines instead, in the model or in a file it includes (an .odex model's `include "file"`; the included file's `@` lines apply as if written in the model). The full list of option names is in [Quick reference](16-quick-reference.md#the-options-list).
