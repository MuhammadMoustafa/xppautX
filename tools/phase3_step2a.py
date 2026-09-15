#!/usr/bin/env python3
"""Phase 3, step 2a: graf_par.c becomes a core file.

The graph-parameter commands (view axes, window/zoom, 3D parameters,
curves, freeze, colormap, freeze key, x vs t) move behind no seam at all;
only their genuinely interactive pieces stay in X11 files:
  redraw_the_graph -> graphics_x11.c (x11_redraw_the_graph)
  scroll_window, test_rot -> rubber.c
and new seam entries cover rubber_band, scroll_window, redraw_menu
(draw_help), new_colormap (NewColormap), aplot_make/aplot_edit
(make_my_aplot/edit_aplot) and new_vcr. draw_win moves to xpp_globals.c.

Run once from the repo root; then tools/verify.sh and tools/guicheck.sh.
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


def sub(p, old, new, count=1):
    s, nl = rd(p)
    old_n, new_n = old.replace("\n", nl), new.replace("\n", nl)
    if old_n not in s:
        sys.exit("%s: not found:\n%s" % (p, old))
    wr(p, s.replace(old_n, new_n, count))


def append(p, text):
    s, nl = rd(p)
    wr(p, s.rstrip("\r\n") + nl + nl + text.replace("\n", nl) + nl)


def unprefix(p, names):
    s, nl = rd(p)
    for n in names:
        s, k = re.subn(r"^((?:void|int) *)x11_%s *\(" % n, r"\1%s(" % n, s, count=1, flags=re.M)
        if k != 1:
            sys.exit("%s: x11_%s not found" % (p, n))
    wr(p, s)


def prefix(p, names):
    s, nl = rd(p)
    for n in names:
        s, k = re.subn(r"^((?:void|int) *)%s *\(" % n, r"\1x11_%s(" % n, s, count=1, flags=re.M)
        if k != 1:
            sys.exit("%s: %s not found" % (p, n))
    wr(p, s)


GP = "core/graf_par.c"
COMMANDS = ["xi_vs_t", "get_3d_par_com", "window_zoom_com", "change_view_com",
            "add_a_curve_com", "freeze_com", "change_cmap_com", "key_frz_com"]

# 1. the X11-only pieces of graf_par.c move out
subprocess.check_call([PY, "tools/move_funcs.py", GP, "core/graphics_x11.c", "redraw_the_graph"])
subprocess.check_call([PY, "tools/move_funcs.py", GP, "core/rubber.c", "test_rot", "scroll_window"])
prefix("core/graphics_x11.c", ["redraw_the_graph"])
prefix("core/rubber.c", ["scroll_window"])
sub("core/rubber.c", '#include "rubber.h"', '#include "rubber.h"\n#include "xpp_globals.h"\n#include "axes2.h"\n#include "graf_par.h"\n#include "graphics.h"\n#include "struct.h"\n#include "mykeydef.h"')
sub("core/graphics_x11.c", '#include "xpp_globals.h"', '''#include "xpp_globals.h"
#include "axes2.h"
#include "ggets.h"
#include "graf_par.h"
#include "integrate.h"
#include "many_pops.h"
#include "nullcline.h"
#include "browse.h"

extern BROWSER my_browser;''')
sub("core/xpp_util.h", "void restore_on(void);", "void restore_on(void);\nvoid ps_restore(void);\nvoid svg_restore(void);")
sub("core/commands.c", '#include "extra.h"', '#include "extra.h"\n#include "graf_par.h"')

# 2. graf_par.c itself
unprefix(GP, COMMANDS + ["auto_freeze_it"])
sub(GP, '''#include <X11/Xlib.h>

#include "graf_par.h"''', '''#include "graf_par.h"''')
for inc in ['#include "aniparse.h"\n', '#include "arrayplot.h"\n', '#include "auto_x11.h"\n',
            '#include "color.h"\n', '#include "init_conds.h"\n', '#include "rubber.h"\n',
            '#include "auto_x11.h"\n', '#include "ggets.h"\n', '#include "menu.h"\n',
            '#include "pop_list.h"\n', '#include <X11/Xutil.h>\n', '#include "many_pops.h"\n',
            '#include "kinescope.h"\n']:
    sub(GP, inc, "")
sub(GP, '#include "integrate.h"', '#include "integrate.h"\n#include "xpp_ui.h"\n#include "xpp_util.h"\n#include "menus.h"')
sub(GP, '''extern Display *display;
extern Window main_win,draw_win,info_pop;
''', "")
sub(GP, "extern char *info_message,*no_hint[]", "extern char *no_hint[]")
sub(GP, "extern CURVE frz[MAXFRZ];\n", "")
sub(GP, "   film_clip();", "   xpp_ui.film_clip();")
sub(GP, "if(rubber(&i1,&j1,&i2,&j2,draw_win,RUBBOX)==0)break;",
    "if(rubber_band(&i1,&j1,&i2,&j2,RUBBOX)==0)break;", count=2)
sub(GP, "if(get_mouse_xy(&x,&y,draw_win)){", "if(GetMouseXY(&x,&y)){")
sub(GP, '''void draw_frozen_cline(index,w)
     int index;
     Window w;''', '''void draw_frozen_cline(index,w)
     int index;
     XppWinId w;''')
sub(GP, '''void draw_freeze(w)
Window w;''', '''void draw_freeze(w)
XppWinId w;''')
sub(GP, '''void draw_bd(w)
     Window w;''', '''void draw_bd(w)
     XppWinId w;''')
sub(GP, '''int get_frz_index(w)
     Window w;''', '''int get_frz_index(w)
     XppWinId w;''')
sub(GP, "  char key[MAXFRZ],ch;", '''  char key[MAXFRZ],ch;
  XppMenu m={"freeze_curves","Curves",0,NULL,NULL,NULL,-1,12,8};''')
sub(GP, "  Window temp=main_win;\n  for(i=0;i<MAXFRZ;i++){", "  for(i=0;i<MAXFRZ;i++){")
sub(GP, '''   ch=(char)pop_up_list(&temp,"Curves",n,key,count,12,0,10,8*DCURY+8,
			no_hint,info_pop,info_message);''', '''   m.n=count; m.items=n; m.keys=key; m.hints=no_hint;
   ch=(char)menu_choose(&m,0);''')

# 3. header: no more X-guarded declarations
H = "core/graf_par.h"
sub(H, 'void get_3d_par_noper(void);', 'void get_3d_par_noper(void);\nvoid update_view(float xlo, float xhi, float ylo, float yhi);')
sub(H, '#include <stdio.h>','#include <stdio.h>\n#include "xpp_types.h"')
sub(H, '''#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
typedef struct {
  float *x[MAXBIFCRV],*y[MAXBIFCRV];
  int color[MAXBIFCRV],npts[MAXBIFCRV],nbifcrv;
  Window w;
} BD;



#endif /* Xlib.h */''', '''typedef struct {
  float *x[MAXBIFCRV],*y[MAXBIFCRV];
  int color[MAXBIFCRV],npts[MAXBIFCRV],nbifcrv;
  XppWinId w;
} BD;''')
sub(H, '''#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_frozen_cline(int index, Window w);
void draw_freeze(Window w);
#endif /* Xlib.h */''', '''void draw_frozen_cline(int index, XppWinId w);
void draw_freeze(XppWinId w);''')
sub(H, '''#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void draw_bd(Window w);
#endif /* Xlib.h */''', '''void draw_bd(XppWinId w);''')
sub(H, '''#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
int get_frz_index(Window w);
#endif /* Xlib.h */''', '''int get_frz_index(XppWinId w);''')

# 4. shared state: draw_win and frz
sub("core/main.c", "\nWindow draw_win;", "\nextern Window draw_win;")
s, nl = rd("core/many_pops.c")
m = re.search(r"^CURVE frz\[MAXFRZ\];", s, re.M)
if not m:
    sys.exit("many_pops.c: frz definition not found")
wr("core/many_pops.c", s[:m.start()] + "extern CURVE frz[MAXFRZ];" + s[m.end():])
sub("core/xpp_globals.c", "int help_menu;", '''CURVE frz[MAXFRZ];  /* frozen curves of every plot window */
XppWinId draw_win;  /* the plot window being drawn into (an X11 Window) */
int help_menu;''')
sub("core/xpp_globals.h", "extern int help_menu;", '''extern int help_menu;
extern CURVE frz[MAXFRZ];
extern XppWinId draw_win;''')

# 5. X11 definitions that core now reaches through the seam get x11_ names
prefix("core/menu.c", ["draw_help"])
prefix("core/color.c", ["NewColormap"])
prefix("core/arrayplot.c", ["make_my_aplot", "edit_aplot"])
prefix("core/aniparse.c", ["new_vcr"])

# 6. the seam
X = "core/xpp_ui.h"
sub(X, "    void (*freeze_curve)(void);   /* auto-freeze after a range run */\n", "")
sub(X, '''    void (*xi_vs_t)(void);
    void (*get_3d_par_com)(void);
    void (*new_parameter)(void);
    void (*window_zoom_com)(int c);
    void (*change_view_com)(int c);
    void (*add_a_curve_com)(int c);
    void (*freeze_com)(int c);
    void (*change_cmap_com)(int c);
    void (*key_frz_com)(int c);
''', '''    void (*new_parameter)(void);
''')
sub(X, '''    void (*show_menu)(int which);''', '''    void (*show_menu)(int which);
    void (*redraw_menu)(void);''')
sub(X, '''    /* raw drawing primitives''', '''    /* mouse interaction in the plot window. rubber_band returns 1 and the
       corners in pixels when the user drew a box (flag RUBBOX) or line
       (RUBLINE), 0 when cancelled. scroll_window lets the user drag the
       view until a key is pressed (calling update_view). */
    int (*rubber_band)(int *i1, int *j1, int *i2, int *j2, int flag);
    void (*scroll_window)(void);

    /* colormap changed (custom_color); X11 reallocates its colours */
    void (*new_colormap)(int type);

    /* raw drawing primitives''')
sub(X, '''    void (*aplot_init)(void);''', '''    void (*aplot_init)(void);
    void (*aplot_make)(char *name); /* open the array plot window */
    void (*aplot_edit)(void);       /* its settings dialog */''')
sub(X, '''    /* misc front-end hooks called while loading an ODE file */''', '''    /* animation (toon) window */
    void (*new_vcr)(void);

    /* misc front-end hooks called while loading an ODE file */''')
sub(X, "void auto_freeze_it(void);\n", "")
sub(X, '''void xi_vs_t(void);
void get_3d_par_com(void);
void new_parameter(void);
void window_zoom_com(int c);
void change_view_com(int c);
void add_a_curve_com(int c);
void freeze_com(int c);
void change_cmap_com(int c);
void key_frz_com(int c);
''', '''void new_parameter(void);
void draw_help(void);
int rubber_band(int *i1, int *j1, int *i2, int *j2, int flag);
void scroll_window(void);
void NewColormap(int type);
void make_my_aplot(char *name);
void edit_aplot(void);
void new_vcr(void);
void redraw_the_graph(void);
''')

C = "core/xpp_ui.c"
sub(C, "    .freeze_curve = hl_void,\n", "")
sub(C, '''    .xi_vs_t = hl_void,
    .get_3d_par_com = hl_void,
    .new_parameter = hl_void,
    .window_zoom_com = hl_int,
    .change_view_com = hl_int,
    .add_a_curve_com = hl_int,
    .freeze_com = hl_int,
    .change_cmap_com = hl_int,
    .key_frz_com = hl_int,
''', '''    .new_parameter = hl_void,
    .redraw_menu = hl_void,
    .rubber_band = hl_auto_rubber,
    .scroll_window = hl_void,
    .new_colormap = hl_int,
    .aplot_make = hl_str,
    .aplot_edit = hl_void,
    .new_vcr = hl_void,
''')
sub(C, "void auto_freeze_it(void) { xpp_ui.freeze_curve(); }\n", "")
sub(C, '''void xi_vs_t(void) { xpp_ui.xi_vs_t(); }
void get_3d_par_com(void) { xpp_ui.get_3d_par_com(); }
void new_parameter(void) { xpp_ui.new_parameter(); }
void window_zoom_com(int c) { xpp_ui.window_zoom_com(c); }
void change_view_com(int c) { xpp_ui.change_view_com(c); }
void add_a_curve_com(int c) { xpp_ui.add_a_curve_com(c); }
void freeze_com(int c) { xpp_ui.freeze_com(c); }
void change_cmap_com(int c) { xpp_ui.change_cmap_com(c); }
void key_frz_com(int c) { xpp_ui.key_frz_com(c); }
''', '''void new_parameter(void) { xpp_ui.new_parameter(); }
void draw_help(void) { xpp_ui.redraw_menu(); }
int rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    return xpp_ui.rubber_band(i1, j1, i2, j2, flag);
}
void scroll_window(void) { xpp_ui.scroll_window(); }
void NewColormap(int type) { xpp_ui.new_colormap(type); }
void make_my_aplot(char *name) { xpp_ui.aplot_make(name); }
void edit_aplot(void) { xpp_ui.aplot_edit(); }
void new_vcr(void) { xpp_ui.new_vcr(); }
void redraw_the_graph(void) { xpp_ui.redraw_graph(); }
''')

U = "core/ui_x11.c"
sub(U, "void redraw_the_graph(void);", '''void x11_redraw_the_graph(void);
void x11_draw_help(void);
void x11_scroll_window(void);
void x11_NewColormap(int type);
void x11_make_my_aplot(char *name);
void x11_edit_aplot(void);
void x11_new_vcr(void);
int rubber(int *x1, int *y1, int *x2, int *y2, Window w, int f);

static int x11_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    return rubber(i1, j1, i2, j2, draw_win, flag);
}''')
sub(U, "    .redraw_graph = redraw_the_graph,", "    .redraw_graph = x11_redraw_the_graph,")
sub(U, "    .freeze_curve = x11_auto_freeze_it,\n", "")
sub(U, "void x11_auto_freeze_it(void);\n", "")
for n in ["xi_vs_t", "get_3d_par_com"]:
    sub(U, "void x11_%s(void);\n" % n, "")
    sub(U, "    .%s = x11_%s,\n" % (n, n), "")
for n in ["window_zoom_com", "change_view_com", "add_a_curve_com", "freeze_com",
          "change_cmap_com", "key_frz_com"]:
    sub(U, "void x11_%s(int c);\n" % n, "")
    sub(U, "    .%s = x11_%s,\n" % (n, n), "")
sub(U, "    .new_parameter = x11_new_parameter,", '''    .new_parameter = x11_new_parameter,
    .redraw_menu = x11_draw_help,
    .rubber_band = x11_rubber_band,
    .scroll_window = x11_scroll_window,
    .new_colormap = x11_NewColormap,
    .aplot_make = x11_make_my_aplot,
    .aplot_edit = x11_edit_aplot,
    .new_vcr = x11_new_vcr,''')

# 7. build: graf_par.c is no longer an X11 source
sub("Makefile", "ggets.c graf_par.c graphics_x11.c", "ggets.c graphics_x11.c")
print("phase3 step 2a applied")
