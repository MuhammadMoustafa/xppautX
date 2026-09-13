#!/usr/bin/env python3
"""Phase 2, step 4: AUTO window seam, array-plot seam, menu submenus via the
seam, colormap split, and the remaining pure helpers out of X11 files.

Run once from the repo root under python3; then tools/verify.sh.
"""
import os
import re
import subprocess
import sys

PY = sys.executable

def rd(p):
    s = open(p, newline="").read()
    return s, ("\r\n" if "\r\n" in s else "\n")

def wr(p, s):
    open(p, "w", newline="").write(s)

def sub(p, pat, rep, count=1):
    s, nl = rd(p)
    s2, n = re.subn(pat, rep, s, count=count, flags=re.M)
    if n == 0:
        sys.exit("pattern not found in %s: %r" % (p, pat))
    wr(p, s2)

def rename(p, names):
    for n in names:
        sub(p, r"^(\s*)(void|int|double|float|char)(\s+\*?\s*)%s(\s*)\(" % re.escape(n),
            r"\1\2\3x11_%s\4(" % n)

def drop_lines(p, exact):
    s, nl = rd(p)
    want = set(exact); seen = set(); out = []
    for l in s.split(nl):
        t = l.strip()
        if t in want:
            seen.add(t); continue
        out.append(l)
    missing = want - seen
    if missing:
        sys.exit("lines not found in %s: %r" % (p, sorted(missing)))
    wr(p, nl.join(out))

def add_include(p, inc, after=None):
    s, nl = rd(p)
    if inc in s:
        return
    if after and after in s:
        i = s.index(after) + len(after)
        i = s.index(nl, i) + len(nl)
    else:
        i = s.index("#include")
    wr(p, s[:i] + inc + nl + s[i:])

def append(p, text):
    s, nl = rd(p)
    wr(p, s.rstrip("\r\n") + nl + nl + text.replace("\n", nl) + nl)

def move(src, dst, *names):
    subprocess.check_call([PY, "tools/move_funcs.py", src, dst] + list(names))

SCRATCH = "build/scratch/step4_removed.c"
os.makedirs("build/scratch", exist_ok=True)
open(SCRATCH, "a").close()

# ---- globals -----------------------------------------------------------------
drop_lines("core/main.c", ["int DCURYb,DCURXb,CURY_OFFb;", "int DCURYs,DCURXs,CURY_OFFs;",
                           "int DCURY,DCURX,CURY_OFF;"])
drop_lines("core/ggets.c", ["int MSStyle=0;"])
add_include("core/ggets.c", '#include "xpp_globals.h"', after='#include "ggets.h"')
drop_lines("core/auto_x11.c", ["int AutoRedrawFlag=1;", "int mark_flag=0;", "int mark_ibrs,mark_ibre;",
                               "int mark_ipts,mark_ipte;", "int mark_ixs,mark_ixe,mark_iys,mark_iye;"])
add_include("core/auto_x11.c", '#include "xpp_globals.h"', after='#include "auto_x11.h"')
drop_lines("core/arrayplot.c", ["int aplot_range;"])
add_include("core/arrayplot.c", '#include "xpp_globals.h"', after='#include "arrayplot.h"')

# ---- AUTO: pure logic to auto_nox.c, drawing via seam ---------------------------
move("core/auto_x11.c", "core/auto_nox.c", "auto_get_info", "auto_set_mark", "find_point", "do_auto_range")
move("core/auto_x11.c", SCRATCH, "DLINE")
append("core/auto_nox.c", "void DLINE(double a,double b,double c,double d)\n{\n  ALINE(IXVal(a),IYVal(b),IXVal(c),IYVal(d));\n}")
add_include("core/auto_nox.c", '#include "xpp_ui.h"')
add_include("core/auto_nox.c", '#include "xpp_globals.h"')
add_include("core/auto_nox.c", '#include "numerics.h"')
add_include("core/auto_nox.c", '#include "integrate.h"')
rename("core/auto_x11.c", ["ALINE", "ATEXT", "clr_stab", "auto_stab_line", "clear_auto_plot",
    "redraw_auto_menus", "clear_auto_info", "draw_auto_info", "refreshdisplay", "byeauto_",
    "Circle", "autocol", "autobw", "auto_rubber", "auto_pop_up_list", "XORCross", "FillCircle",
    "LineWidth", "auto_scroll_window", "traverse_diagram", "make_auto"])
sub("core/auto_nox.c", r'respond_box\("Okay","Can\'t continue infinite period Hopf!"\);',
    'err_msg("Can\'t continue infinite period Hopf!");')

# ---- array plot ---------------------------------------------------------------
rename("core/arrayplot.c", ["init_my_aplot", "close_aplot_files", "draw_one_array_plot", "dump_aplot"])

# ---- browser data ---------------------------------------------------------------
move("core/browse.c", "core/browse_data.c", "wipe_rep", "data_get", "data_get_mybrowser")
drop_lines("core/browse.c", ["float *old_rep;", "int REPLACE=0,R_COL=0;"])
sub("core/browse.c", r"^extern BROWSER my_browser;", "extern BROWSER my_browser;\nextern float *old_rep;\nextern int REPLACE,R_COL;")
sub("core/browse_data.c", r"^BROWSER my_browser;",
    "BROWSER my_browser;\nfloat *old_rep;\nint REPLACE=0,R_COL=0;\n"
    "extern int NODE,NMarkov,FIX_VAR;\nextern char uvar_names[MAXODE][12];\nextern double last_ic[MAXODE];")
add_include("core/browse_data.c", '#include "parserslow.h"')
add_include("core/browse_data.c", '#include <stdlib.h>')

# ---- misc pure helpers ------------------------------------------------------------
move("core/many_pops.c", "core/xpp_util.c", "set_active_windows")
rename("core/many_pops.c", ["create_a_pop"])
move("core/many_pops.c", SCRATCH, "select_table")
append("core/tabular.c", '''#include "xpp_ui.h"
extern char *no_hint[];
int select_table(void)
{
 int i,j;
 char *n[MAX_TAB],key[MAX_TAB],ch;
 for(i=0;i<NTable;i++){
   n[i]=(char *)malloc(25);
   key[i]='a'+i;
   sprintf(n[i],"%c: %s",key[i],my_table[i].name);
 }
 key[NTable]=0;
 ch=(char)xpp_ui.choose_key("Table",n,key,NTable,0,no_hint);
 for(i=0;i<NTable;i++)free(n[i]);
 j=(int)(ch-'a');
 if(j<0||j>=NTable){
   err_msg("Not a valid table");
   return -1;
 }
 return j;
}''')
add_include("core/tabular.c", '#include <stdio.h>')
move("core/graf_par.c", "core/xpp_util.c", "check_windows", "check_val", "dump_ps")
rename("core/graf_par.c", ["auto_freeze_it"])
move("core/init_conds.c", "core/xpp_util.c", "redo_stuff")
rename("core/init_conds.c", ["man_ic"])
move("core/edit_rhs.c", "core/xpp_util.c", "user_fun_info")
rename("core/ggets.c", ["reset_graphics"])
rename("core/menudrive.c", ["set_col_par", "new_lookup", "make_adj", "get_pmap_pars", "froz_cline_stuff", "do_stochast"])
rename("core/menu.c", ["flash", "help"])
rename("core/txtread.c", ["init_txtview"])
rename("core/userbut.c", ["add_user_button"])
sub("core/graphics.c", r"^(\s*)redraw_the_graph\(\);", r"\1xpp_ui.redraw_graph();")

# externs the moved bodies need in xpp_util.c
sub("core/xpp_util.c", r'^#include "browse.h"',
    '#include "browse.h"\n#include "my_ps.h"\n#include "my_svg.h"\n#include "tabular.h"\n#include "volterra2.h"\n#include "derived.h"\n#include "xpplim.h"')
sub("core/xpp_util.c", r"^extern BROWSER my_browser;",
    "extern BROWSER my_browser;\nextern char this_file[XPP_MAX_NAME];\nextern char this_internset[XPP_MAX_NAME];\n"
    "extern char *ufun_def[MAXUFUN];\nextern char ufun_names[MAXUFUN][12];\nextern int narg_fun[MAXUFUN];\n"
    "extern UFUN_ARG ufun_arg[MAXUFUN];\nextern int NFUN;")

# ---- colormap split -----------------------------------------------------------------
move("core/color.c", SCRATCH, "make_cmaps", "rfun", "gfun", "bfun", "read_cmap_from_file",
     "get_ps_color", "get_svg_color", "MakeColormap")
drop_lines("core/color.c", ["int custom_color=0;", "int periodic=0,spectral;"])
add_include("core/color.c", '#include "colormap.h"', after='#include "color.h"')
append("core/color.c", '''void MakeColormap()
{
  Colormap cmap;
  int i;
  int clo=20;
  cmap=(Colormap)NULL;
  xpp_build_colormap();
  if (Xup){cmap = DefaultColormap(display,screen);}
  for (i = 0; i < clo; i++) {
    color[i].pixel = i;
  }
  for(i=20;i<=color_max;i++){
    if(i>=30 && i<color_min) continue;
    color[i].red=xpp_cmap_rgb[i][0];
    color[i].green=xpp_cmap_rgb[i][1];
    color[i].blue=xpp_cmap_rgb[i][2];
    color[i].flags = DoRed | DoGreen | DoBlue;
    if (Xup){XAllocColor(display,cmap,&color[i]);}
  }
}''')

# ---- metric: the X11 program's main() is not core --------------------------------
sub("tools/coredeps.sh", r"grep -v sbml2xpp \| sort", "grep -vE 'sbml2xpp|xppaut_main' | sort")

print("step 4 applied")
