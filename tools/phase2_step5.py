#!/usr/bin/env python3
"""Phase 2, step 5: headless batch entry point, libxppcore.a and xppcore-cli.

Run once from the repo root under python3; then tools/verify.sh.
"""
import re
import subprocess
import sys

PY = sys.executable

def rd(p):
    s = open(p, newline="").read()
    return s, ("\r\n" if "\r\n" in s else "\n")

def wr(p, s):
    open(p, "w", newline="").write(s)

def append(p, text):
    s, nl = rd(p)
    wr(p, s.rstrip("\r\n") + nl + nl + text.replace("\n", nl) + nl)

# 1. check_for_quiet / do_vis_env out of main.c
subprocess.check_call([PY, "tools/move_funcs.py", "core/main.c", "core/xpp_batch.c",
                       "check_for_quiet", "do_vis_env"])

# 2. the notAlreadySet block out of do_main() into xpp_reset_options()
s, nl = rd("core/main.c")
m = re.search(r"(  /\*Track which options have not been set already\*/\r?\n)?(  notAlreadySet\.BIG_FONT_NAME=1;.*?notAlreadySet\.COLORLO=1;\r?\n)", s, re.S)
if not m:
    sys.exit("notAlreadySet block not found in main.c")
block = m.group(2)
s = s[:m.start()] + "  xpp_reset_options();" + nl + s[m.end():]
s = s.replace('#include "xpp_globals.h"', '#include "xpp_globals.h"' + nl + '#include "xpp_batch.h"', 1)
wr("core/main.c", s)
append("core/xpp_batch.c", "void xpp_reset_options(void)\n{\n" + block.replace("\r\n", "\n").rstrip("\n") + "\n}")

# 3. the batch driver
append("core/xpp_batch.c", r'''int xpp_batch_main(int argc, char **argv)
{
    char myfile[XPP_MAX_NAME];
    OptionsSet *tempNS;

    xpp_reset_options();
    get_directory(myfile);
    Xup = 0;
    sprintf(batchout, "output.dat");
    sprintf(PlotFormat, "ps");
    logfile = stdout;
    check_for_quiet(argc, argv);
    do_comline(argc, argv);
    XPPBatch = 1; /* headless: always batch, even without -silent */

    load_eqn();

    tempNS = (OptionsSet *)malloc(sizeof(OptionsSet));
    *tempNS = notAlreadySet;
    set_internopts(tempNS);
    free(tempNS);

    init_alloc_info();
    do_vis_env();
    set_all_vals();
    init_alloc_info();
    set_init_guess();
    update_all_ffts();
#ifdef AUTO
    init_auto_win();
#endif
    if (disc(this_file)) METHOD = 0;
    xppvermaj = (float)cstringmaj;
    xppvermin = (float)cstringmin;
    do_meth();
    set_delay();
    rhs = my_rhs;
    init_fit_info();
    strip_saveqn();
    create_plot_list();
    auto_load_dll();

    xpp_build_colormap();
    init_browser();
    init_all_graph();
    if_needed_load_set();
    if_needed_load_par();
    if_needed_load_ic();
    if_needed_select_sets();
    if_needed_load_ext_options();
    set_extra_graphs();
    set_colorization_stuff();
    batch_integrate();
    if (NCBatch > 0) silent_nullclines();
    if (DFBatch > 0) silent_dfields();
    silent_equilibria();
    return 0;
}''')

# 4. Makefile: UI file list, library and CLI targets
s, nl = rd("Makefile")
s = s.replace(
    "# sbml2xpp.c needs libsbml and is not part of the upstream build.\nSOURCES := $(filter-out $(SRCDIR)/sbml2xpp.c,$(wildcard $(SRCDIR)/*.c))\nOBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))\n",
    """# Sources that need X11 (the front end). Everything else is libxppcore.
UI_SOURCES := $(addprefix $(SRCDIR)/, abort.c aniparse.c arrayplot.c auto_x11.c \\
  browse.c calc.c choice_box.c color.c dialog_box.c edit_rhs.c eig_list.c \\
  ggets.c graf_par.c graphics_x11.c init_conds.c kinescope.c main.c many_pops.c \\
  menu.c menudrive.c pop_list.c rubber.c scrngif.c torus.c txtread.c ui_x11.c \\
  userbut.c xppaut_main.c)
# sbml2xpp.c needs libsbml and is not part of the upstream build.
CORE_SOURCES := $(filter-out $(UI_SOURCES) $(SRCDIR)/sbml2xpp.c $(SRCDIR)/xppcore_cli.c,$(wildcard $(SRCDIR)/*.c))
SOURCES := $(CORE_SOURCES) $(UI_SOURCES)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CORE_SOURCES))
""")
s = s.replace(".PHONY: all clean x11free\nall: xppaut\n",
              ".PHONY: all clean x11free lib cli\nall: xppaut\nlib: libxppcore.a\ncli: xppcore-cli\n")
s = s.replace("$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)",
              """libxppcore.a: $(CORE_OBJECTS)
\tar rcs $@ $(CORE_OBJECTS)

xppcore-cli: $(BUILDDIR)/xppcore_cli.o libxppcore.a
\t$(CC) -o $@ $(BUILDDIR)/xppcore_cli.o libxppcore.a -lm -ldl

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)""")
s = s.replace("\trm -rf $(BUILDDIR) xppaut", "\trm -rf $(BUILDDIR) xppaut libxppcore.a xppcore-cli")
s = s.replace("-include $(OBJECTS:.o=.d)", "-include $(OBJECTS:.o=.d) $(BUILDDIR)/xppcore_cli.d")
wr("Makefile", s)

# 5. verify.sh: also build and smoke-test the headless CLI
s, nl = rd("tools/verify.sh")
s = s.replace("make -j8 > build/last-build.log 2>&1", "make -j8 xppaut xppcore-cli > build/last-build.log 2>&1")
s = s.replace("tools/x11free.sh\n", """tmp=$(mktemp -d)
( cd "$tmp" && "$OLDPWD/xppcore-cli" "$OLDPWD/examples/ode/lecar.ode" >/dev/null 2>&1 )
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
rm -rf "$tmp"
if [ "$sum" = "$BASELINE" ]; then
  echo "headless cli ok: checksum matches baseline"
else
  echo "HEADLESS CLI MISMATCH: sum=$sum"
  exit 1
fi
tools/x11free.sh
""")
wr("tools/verify.sh", s)

# 6. .gitignore
s, nl = rd(".gitignore")
if "/xppcore-cli" not in s:
    wr(".gitignore", s.rstrip("\r\n") + nl + "/xppcore-cli" + nl + "/libxppcore.a" + nl)

print("step 5 applied")
