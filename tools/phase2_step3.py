#!/usr/bin/env python3
"""Phase 2, step 3: move core state and pure helpers out of X11 files, route
the remaining UI calls from numerics through the XppUi seam, split the X11
primitives out of graphics.c, and turn menus.h into a compilation unit.

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

def drop_lines(p, exact):
    """Remove lines whose stripped text equals one of `exact`; all must exist."""
    s, nl = rd(p)
    lines = s.split(nl)
    want = set(exact)
    seen = set()
    out = []
    for l in lines:
        t = l.strip()
        if t in want:
            seen.add(t)
            continue
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

def move(src, dst, *names):
    subprocess.check_call([PY, "tools/move_funcs.py", src, dst] + list(names))

SCRATCH = "build/scratch/step3_removed.c"
os.makedirs("build/scratch", exist_ok=True)
open(SCRATCH, "a").close()

# ---- 1. main.c ---------------------------------------------------------------
if "--menus-only" not in sys.argv:
  pass
  drop_lines("core/main.c", [
      "int allwinvis=0;", "int use_intern_sets=1;", "int use_ani_file=0;",
      "char anifile[XPP_MAX_NAME];", "float xppvermaj,xppvermin;",
      "int Xup,TipsFlag=1;", "int XPPBatch=0,batch_range=0,BatchEquil=-1;",
      "char batchout[256];", "char UserOUTFILE[256];",
      "char big_font_name[100],small_font_name[100];", "char PlotFormat[100];",
      "int PaperWhite=-1;", "char UserBlack[8];", "char UserWhite[8];",
      "char UserMainWinColor[8];", "char UserDrawWinColor[8];",
      "char UserBGBitmap[XPP_MAX_NAME];", "int UserGradients=-1;",
      "int UserMinWidth=0,UserMinHeight=0;", "FILE *logfile;", "int XPPVERBOSE=1;",
      "int OVERRIDE_QUIET=0;", "int OVERRIDE_LOGFILE=0;", "int tfBell;",
      "int SLIDER1=-1;", "int SLIDER2=-1;", "int SLIDER3=-1;",
      "char SLIDER1VAR[20];", "char SLIDER2VAR[20];", "char SLIDER3VAR[20];",
      "double SLIDER1LO=0.0;", "double SLIDER2LO=0.0;", "double SLIDER3LO=0.0;",
      "double SLIDER1HI=1.0;", "double SLIDER2HI=1.0;", "double SLIDER3HI=1.0;",
      "double SLIDER1INIT=0.5;", "double SLIDER2INIT=0.5;", "double SLIDER3INIT=0.5;",
      "int DoTutorial=0;", "OptionsSet notAlreadySet;",
  ])
  add_include("core/main.c", '#include "xpp_globals.h"', after='#include "main.h"')
  sub("core/main.c", r"^void bye_bye\(\)", "void x11_bye_bye()")
  move("core/main.c", SCRATCH, "clr_scrn")

  # ---- 2. graf_par.c -------------------------------------------------------------
  drop_lines("core/graf_par.c", ["int PS_Color=1;"])
  add_include("core/graf_par.c", '#include "xpp_globals.h"', after='#include "graf_par.h"')
  move("core/graf_par.c", "core/xpp_util.c", "ind_to_sym", "get_max")

  # ---- 3. many_pops.c ------------------------------------------------------------
  drop_lines("core/many_pops.c", [
      "GRAPH graph[MAXPOP];", "GRAPH *MyGraph;", "int SimulPlotFlag=0;",
      "int current_pop;", "int num_pops;", "int ActiveWinList[MAXPOP];",
  ])
  add_include("core/many_pops.c", '#include "xpp_globals.h"', after='#include "many_pops.h"')
  move("core/many_pops.c", SCRATCH, "make_active", "restore_off", "restore_on")
  sub("core/many_pops.c", r"^void title_text\(string\)", "void x11_title_text(string)")
  sub("core/many_pops.c", r"^void canvas_xy\(buf\)", "void x11_canvas_xy(buf)")
  sub("core/many_pops.c", r"^void SmallBase\(\)", "void x11_SmallBase()")
  sub("core/many_pops.c", r"^void SmallGr\(\)", "void x11_SmallGr()")

  # ---- 4. aniparse.c -------------------------------------------------------------
  move("core/aniparse.c", "core/xpp_util.c", "de_space")
  drop_lines("core/aniparse.c", ["int animation_on_the_fly=0;"])
  add_include("core/aniparse.c", '#include "xpp_globals.h"', after='#include "aniparse.h"')
  sub("core/aniparse.c", r"^void on_the_fly\(int task\)", "void x11_on_the_fly(int task)")

  # ---- 5..7. stranded pure functions -------------------------------------------
  move("core/init_conds.c", "core/xpp_util.c", "find_user_name")
  move("core/calc.c", "core/xpp_util.c", "do_calc", "has_eq", "calculate")
  move("core/browse.c", "core/browse_data.c", "open_write_file")
  sub("core/browse_data.c", r'respond_box\("Ok",\s*"Cannot open file"\);', 'err_msg("Cannot open file");')

  # ---- 8..12. X11 originals get the x11_ prefix ---------------------------------
  sub("core/abort.c", r"^int get_command_width\(\)", "int x11_get_command_width()")
  sub("core/abort.c", r"^void plot_command\(nit,icount,cwidth\)", "void x11_plot_command(nit,icount,cwidth)")
  sub("core/abort.c", r"^int my_abort\(\)", "int x11_my_abort()")
  for name in ["TwoChoice", "FlushDisplay", "GetMouseXY", "MessageBox", "KillMessageBox",
               "drw_all_scrns", "clr_all_scrns", "clear_draw_window"]:
      sub("core/menudrive.c", r"^(int|void) %s\(" % name, r"\1 x11_%s(" % name)
  sub("core/color.c", r"^void set_color\(col\)", "void x11_set_color(col)")
  drop_lines("core/color.c", ["int color_mode=1,color_min,color_total,COLOR,color_max;"])
  add_include("core/color.c", '#include "xpp_globals.h"', after='#include "color.h"')
  sub("core/ggets.c", r"^void bottom_msg\(line,msg\)", "void x11_bottom_msg(line,msg)")
  sub("core/kinescope.c", r"^void reset_film\(\)", "void x11_reset_film()")

  # ---- 13. main() out of my_rhs.c ---------------------------------------------
  move("core/my_rhs.c", SCRATCH, "main")

  # ---- 14. graphics.c split --------------------------------------------------------
  move("core/graphics.c", "core/graphics_x11.c",
       "point_x11", "set_line_style_x11", "bead_x11", "rect_x11", "draw_many_lines",
       "line_x11", "put_text_x11", "special_put_text_x11", "fancy_put_text_x11")
  s, nl = rd("core/graphics.c")
  s = re.sub(r"char dashes\[10\]\[5\] = \{.*?\};\r?\n", "", s, count=1, flags=re.S)
  wr("core/graphics.c", s)
  drop_lines("core/graphics.c", [
      "XFontStruct *symfonts[5],*romfonts[5];", "int avsymfonts[5],avromfonts[5];",
      "extern GC gc_graph;", "extern Display *display;", "extern Window win;",
      "extern Window draw_win;", "extern GC small_gc;", "extern XFontStruct *small_font;",
      "extern GC font_gc;", "#include <X11/Xlib.h>", "#include <X11/Xutil.h>",
      "extern GRAPH graph[MAXPOP];", "extern GRAPH *MyGraph;",
  ])
  add_include("core/graphics.c", '#include "xpp_globals.h"', after='#include "graphics.h"')
  add_include("core/graphics.c", '#include "xpp_ui.h"', after='#include "graphics.h"')
  sub("core/graphics.c", r"else point_x11\(x,y\);", "else xpp_ui.draw_point(x,y);")
  sub("core/graphics.c", r"else line_x11\(x1,y1,x2,y2\);", "else xpp_ui.draw_line(x1,y1,x2,y2);")
  sub("core/graphics.c", r"else bead_x11\(x1,y1\);", "else xpp_ui.draw_bead(x1,y1);")
  sub("core/graphics.c", r"else rect_x11\(x1,y1,w,h\);", "else xpp_ui.draw_frect(x1,y1,w,h);")
  sub("core/graphics.c", r"else put_text_x11\(x,y,str\);", "else xpp_ui.draw_text(x,y,str);")
  sub("core/graphics.c", r"else set_line_style_x11\(ls\);", "else xpp_ui.draw_linestyle(ls);")
  sub("core/graphics.c", r"else special_put_text_x11\(xp,yp,text,size\);", "else xpp_ui.draw_special_text(xp,yp,text,size);")
  sub("core/graphics.c", r"XGetGeometry\(display,draw_win,&root,&x,&y,&w,&h,&bw,&de\);", "xpp_ui.get_draw_size(&w,&h);")
  sub("core/graphics.c", r"^  Window root;\r?\n", "")

# ---- 15. menus.h -> menus.c + extern header ----------------------------------
s, nl = rd("core/menus.h")
lines = s.split(nl)
# remove the two bogus guards the guard script put around string literals
# (they contain the word "Window") but keep the one around MENUDEF
out = []
i = 0
while i < len(lines):
    l = lines[i]
    if l.startswith("#if defined(_XLIB_H_)") and i + 1 < len(lines) and lines[i + 1].lstrip().startswith('"'):
        i += 1
        while not lines[i].startswith("#endif"):
            out.append(lines[i]); i += 1
        i += 1
        continue
    out.append(l); i += 1
body = nl.join(out)
# MENUDEF typedef block -> header only
m = re.search(r"#if defined\(_XLIB_H_\).*?typedef struct \{.*?\} MENUDEF;\s*#endif /\* Xlib\.h \*/\r?\n", body, re.S)
if not m:
    sys.exit("MENUDEF block not found")
menudef = m.group(0)
body = body.replace(menudef, "")
defines = [l for l in out if l.startswith("#define ")]
names = re.findall(r"^char \*(\w+)\[\]", body, re.M)
csrc = body.replace("#ifndef _menus_h_" + nl + "#define _menus_h_", "/* Menu labels and hint strings. Generated from upstream menus.h; the\n   MENUDEF widget struct lives in menus.h. */" + nl + '#include "menus.h"')
csrc = re.sub(r"\r?\n#endif\s*$", nl, csrc)
wr("core/menus.c", csrc)
hdr = ["#ifndef _menus_h_", "#define _menus_h_", ""] + defines + [""] + \
      ["extern char *%s[];" % n for n in names] + ["", menudef.rstrip(), "", "#endif", ""]
wr("core/menus.h", nl.join(hdr))

print("step 3 applied")
