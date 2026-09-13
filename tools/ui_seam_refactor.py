#!/usr/bin/env python3
"""One-shot refactoring script: introduce the XppUi seam (phase 2, step 2).

Run from the repo root. Idempotent-ish: refuses to run twice on the same file
where it can tell. Review the diff afterwards; tools/verify.sh must pass.
"""
import re
import subprocess
import sys

def rd(p):
    s = open(p, newline="").read()
    return s, ("\r\n" if "\r\n" in s else "\n")

def wr(p, s):
    open(p, "w", newline="").write(s)

def sub1(p, pat, rep, count=1, must=True):
    s, nl = rd(p)
    s2, n = re.subn(pat, rep, s, count=count, flags=re.M)
    if must and n == 0:
        sys.exit("pattern not found in %s: %r" % (p, pat))
    wr(p, s2)

def drop_lines(p, pats):
    s, nl = rd(p)
    out = [l for l in s.split(nl) if not any(re.match(pt, l) for pt in pats)]
    wr(p, nl.join(out))

def add_include(p, inc):
    s, nl = rd(p)
    if inc in s:
        return
    i = s.index("#include")
    wr(p, s[:i] + inc + nl + s[i:])

def unguard(p):
    drop_lines(p, [r"#if defined\(_XLIB_H_\)", r"#endif /\* Xlib\.h \*/"])

# ---- A. struct.h: opaque handles -------------------------------------------
if "--from-d" not in sys.argv:
    unguard("core/struct.h")
    sub1("core/struct.h", r"^(\s*)Window (\w)", r"\1XppWinId \2", count=0)
    sub1("core/struct.h", r'^#include "xpplim.h"', '#include "xpplim.h"\n#include "xpp_types.h"')

    # ---- B. browse.h: BROWSER fields --------------------------------------------
    unguard("core/browse.h")
    s, nl = rd("core/browse.h")
    out, inb = [], False
    for l in s.split(nl):
        if re.match(r"\s*typedef\s+struct\s*\{", l):
            inb = True
        if inb:
            l = re.sub(r"\bWindow\b", "XppWinId", l)
        if inb and re.search(r"\}\s*BROWSER\s*;", l):
            inb = False
        out.append(l)
    s = nl.join(out).replace("#define BMAXCOL 20", "#define BMAXCOL 20" + nl + '#include "xpp_types.h"', 1)
    wr("core/browse.h", s)

    # ---- C. browse.c -> browse_data.c ------------------------------------------
    subprocess.check_call([sys.executable, "tools/move_funcs.py", "core/browse.c", "core/browse_data.c",
        "get_browser_data", "set_browser_data", "get_data_col", "gettimenow", "waitasec",
        "get_maxrow_browser", "write_mybrowser_data", "write_browser_data", "find_variable",
        "refresh_browser", "reset_browser", "init_browser"])
    sub1("core/browse.c", r"^BROWSER my_browser;", "extern BROWSER my_browser;")
    sub1("core/browse_data.c", r"^ if\(Xup&&my_browser\.xflag==1\) draw_data\(my_browser\);", " xpp_ui.data_changed(length);")

# ---- D. ggets.c ---------------------------------------------------------------
subprocess.check_call([sys.executable, "tools/move_funcs.py", "core/ggets.c", "build/scratch/removed.c",
    "plintf", "new_int", "new_float"])
sub1("core/ggets.c", r"^void ping\(\) *$", "void x11_ping()")
sub1("core/ggets.c", r"^void err_msg\(string\)", "void x11_err_msg(string)")
sub1("core/ggets.c", r"^int new_string\(name,value\)", "int x11_new_string(name,value)")
drop_lines("core/ggets.c", [r"new_float\(\);\s*$", r"new_int\(\);\s*$"])

# ---- E/F/G/H. other X11 originals get the x11_ prefix -----------------------
sub1("core/pop_list.c", r"^int do_string_box\(n,row,col,title,names,values,maxchar\)", "int x11_do_string_box(n,row,col,title,names,values,maxchar)")
sub1("core/pop_list.c", r"^int yes_no_box\(\)", "int x11_yes_no_box()")
sub1("core/init_conds.c", r"^int file_selector\(title,file,wild\)", "int x11_file_selector(title,file,wild)")
sub1("core/init_conds.c", r"^void redraw_params\(\)", "void x11_redraw_params()")
sub1("core/init_conds.c", r"^void redraw_ics\(\)", "void x11_redraw_ics()")
sub1("core/main.c", r"^void redraw_all\(\)", "void x11_redraw_all()")
sub1("core/eig_list.c", r"^(void )?create_eq_box\(", r"\1x11_create_eq_box(")

# install hook at the top of init_X()
s, nl = rd("core/main.c")
i = s.index("void init_X ()")
j = s.index("{", i) + 1
s = s[:j] + nl + "  xpp_install_x11_ui();" + s[j:]
s = s.replace("void init_X ()", "void xpp_install_x11_ui(void);" + nl + "void init_X ()", 1)
wr("core/main.c", s)

# ---- I. numerics call sites --------------------------------------------------
sub1("core/integrate.c", r"draw_label\(draw_win\);", "xpp_ui.draw_label();", count=0)
sub1("core/integrate.c", r"put_text_x11\(5,10,bob\);", "xpp_ui.put_text(5,10,bob);", count=0)
sub1("core/integrate.c", r"\bfilm_clip\(\)", "xpp_ui.film_clip()", count=0)
sub1("core/pp_shoot.c", r"\bfilm_clip\(\)", "xpp_ui.film_clip()")
sub1("core/axes2.c", r"blank_screen\(draw_win\);", "xpp_ui.blank_draw_window();")
sub1("core/numerics.c", r"^ Window temp=main_win;\r?\n", "")
sub1("core/numerics.c",
     r' ch = \(char\)pop_up_list\(&temp,"Method",n,key,nmeth,15,METHOD,10,DCURY\+8,\s*meth_hint,info_pop,info_message\);',
     ' ch = (char)xpp_ui.choose_key("Method",n,key,nmeth,METHOD,meth_hint);')

# ---- J. drop X11 from the numerics files ------------------------------------
for f in ["integrate", "axes2", "lunch-new", "pp_shoot", "numerics"]:
    drop_lines("core/%s.c" % f, [
        r"#include *<X11/Xlib\.h>",
        r"\s*extern Window draw_win;", r"extern Window main_win,info_pop;",
        r"extern Display \*display;", r"extern Window command_pop;",
        r"\s*extern Window main_win;", r"extern GC small_gc;",
    ])
for f in ["integrate", "axes2", "pp_shoot", "numerics", "ggets", "browse", "browse_data", "del_stab", "gear", "calc"]:
    add_include("core/%s.c" % f, '#include "xpp_ui.h"')

# ---- re-guard the two headers we unguarded ---------------------------------
subprocess.check_call([sys.executable, "tools/guard_x11_headers.py", "core/struct.h", "core/browse.h"])
print("refactor applied")
