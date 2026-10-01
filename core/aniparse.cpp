/* The animator: the .ani language (parsing, evaluation per frame) and the
   drawing of a frame.

   A frame is computed in the animation's own coordinates (`dimension`,
   [0,1] x [0,1] by default, y up) and each primitive goes to two places
   (the "frame sinks" below): the pixel callbacks (xpp_ui ani_*, scaled
   to the vcr.wid x vcr.hgt window exactly as XPP always has; no front end
   draws them since protocol 2) and ani_data.h, which
   keeps it in unit coordinates for a front end that draws it itself
   (docs/protocol.md "The animation as data"). */
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model.h"
#include "model_files.h"
#include "session.h"
#include "getvar.h"
#include "ode_read.h"
#include "aniparse.h"
#include "ani_data.h"
#include "xpp_log.h"
#include "expr.h"
#include "xpp_io.h"
#include "form_ode.h"
#include "my_rhs.h"
#include "nullcline.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "integrate.h"
#include "browse.h"
#include "graf_par.h"
#include <libgen.h>
#include "colormap.h"

#define LINE 0
#define RLINE 1
#define CIRC 2
#define FCIRC 3
#define RECT 4
#define FRECT 5
#define TEXT 6
#define VTEXT 7
#define ELLIP 9
#define FELLIP 10
#define COMET 11
#define AXNULL 13
#define AYNULL 14
#define GRAB 25
/*  not for drawing */

#define SETTEXT 8

/*  Not in command list   */

#define TRANSIENT 20
#define PERMANENT 21
#define END 50
#define DIMENSION 22
#define COMNT 30
#define SPEED 23

/* Colors
  no color given is default black on white background or white on black
  $name is named color -- red ... purple
  otherwise evaluated - if between 0 and 1 a spectral color
*/

/* scripting language is very simple:
 dimension xlo;ylo;xhi;yh
 transient
 permanent
 line x1;y1;x2;y2;col;thick --  last two optional
 rline x2;y2;col;thick  -- last two optional
 circle x1;x2;r;col;thick   -- last optional
 fcircle x1;x2;r;col  -- last 2 optional
 rect x1;y1;x2;y2;col;thick -- last 2 optional
 frect x1;y1;x2;y2;col -- last optional
 ellip x1;y1;rx;ry;col;thick
 fellip x1;y1;rx;ry;col;thick
 text x1;y1;s
 vtext x1;y1;s;v
 settext size;font;color -- size 1-5,font roman symbol,color as above
 speed delay in msec
 comet x1;y1;type;n;color  -- use last n points to draw n objects at
                              x1,y1  of type  type>=0 draws a line
                              with thickness type
                              type<0 draws filled circles of
                              radius |type|
 *****
 rline is relative to end of last point
 fcircle filled circle
 rect rectangle
 frect filled rect
 text  string s at (x,y)  if v included then a number

 eg   text .3;.3;t=%g;t

 will do a sprintf(string,"t=%g",t);

 and put text at .3,.3
*/

namespace {

/* a comet's last n positions, in the animation's coordinates, and their colours */
struct Comet {
    int n = 0;
    std::vector<double> x, y;
    std::vector<int> col;
    int i = 0;
};

/* one command of the .ani file: its compiled expressions (add_expr's
   tokens, each ending in ENDEXP) and their values in the current frame */
struct AniCom {
    Comet c;
    int type = 0, flag = 0;
    /* col[0] <= 0 is a named colour, -index; else the compiled expression */
    std::vector<int> col{0};
    std::vector<int> x1, y1, x2, y2, who;
    std::string text; /* text and vtext's string */
    double zcol = 0, zx1 = 0, zy1 = 0, zx2 = 0, zy2 = 0, zrad = 0, zval = 0;
    int zthick = 0, tfont = 0, tsize = 0, tcolor = 0;
};

/**************  the Grabber ***************************/
constexpr int MAX_GEVENTS = 20; /* maximum variables you can change per grabbable */
constexpr int MAX_ANI_GRAB = 50; /* max grabbable objects  */

/* tasks have the form {name1=formula1;name2=formula2;...} */
struct GrabEvent {
    std::string lhs;
    std::vector<int> rhs;
};

struct GrabTask {
    std::vector<GrabEvent> events; /* at most MAX_GEVENTS */
    int runnow = 0;
};

struct AniGrab {
    double zx = 0, zy = 0, tol = 0;
    std::vector<int> x, y;
    GrabTask start, end;
};

struct AniMotionInfo {
    double x, y;
    double ox, oy;
    double t1, t2;
    double vx, vy;
};

AniMotionInfo ami;

std::vector<AniGrab> ani_grab;
int show_grab_points = 0;
int who_was_grabbed;

/************************  end grabber **********************/

constexpr int FIRSTCOLOR = 30;
constexpr int on_the_fly_speed = 10;

int aniflag;
int LastAniColor;
int ani_lineno; /* the .ani file's line being parsed */

double ani_xlo = 0, ani_xhi = 1, ani_ylo = 0, ani_yhi = 1;
double ani_lastx, ani_lasty;

std::vector<AniCom> my_ani;

void set_ani_font_stuff(int size, int font, int color);
void eval_ani_color(xpp::Session &s, int j);
void eval_ani_com(xpp::Session &s, int j);
void free_ani(xpp::Session &s);
void do_grab_tasks(xpp::Session &s, int which);
void draw_grab_points(xpp::Session &s);

/*************************  NEW ANIMaTION STUFF ***********************/

/* seconds, for the mouse's speeds (only differences are used) */
double get_current_time(void)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/**************   DRAWING ROUTINES   *******************/

void ani_rad2scale(const xpp::Session &s, double rx, double ry, int *ix, int *iy)
{
    double dx = static_cast<double>(s.animation.vcr.wid) / (ani_xhi - ani_xlo), dy = static_cast<double>(s.animation.vcr.hgt) / (ani_yhi - ani_ylo);
    double r1 = rx * dx, r2 = ry * dy;
    *ix = static_cast<int>(r1);
    *iy = static_cast<int>(r2);
}

void ani_radscale(const xpp::Session &s, double rad, int *ix, int *iy)
{
    double dx = static_cast<double>(s.animation.vcr.wid) / (ani_xhi - ani_xlo), dy = static_cast<double>(s.animation.vcr.hgt) / (ani_yhi - ani_ylo);
    double r1 = rad * dx, r2 = rad * dy;
    *ix = static_cast<int>(r1);
    *iy = static_cast<int>(r2);
}

void ani_ij_to_xy(const xpp::Session &s, int ix, int iy, double *x, double *y)
{
    double dx = (ani_xhi - ani_xlo) / static_cast<double>(s.animation.vcr.wid);
    double dy = (ani_yhi - ani_ylo) / static_cast<double>(s.animation.vcr.hgt);
    *x = ani_xlo + static_cast<double>(ix) * dx;
    *y = ani_ylo + static_cast<double>(s.animation.vcr.hgt - iy) * dy;
}

void ani_xyscale(const xpp::Session &s, double x, double y, int *ix, int *iy)
{
    double dx = static_cast<double>(s.animation.vcr.wid) / (ani_xhi - ani_xlo), dy = static_cast<double>(s.animation.vcr.hgt) / (ani_yhi - ani_ylo);
    double xx = (x - ani_xlo) * dx;
    double yy = s.animation.vcr.hgt - dy * (y - ani_ylo);
    *ix = static_cast<int>(xx);
    *iy = static_cast<int>(yy);
    if (*ix < 0) *ix = 0;
    if (*ix >= s.animation.vcr.wid) *ix = s.animation.vcr.wid - 1;
    if (*iy < 0) *iy = 0;
    if (*iy >= s.animation.vcr.hgt) *iy = s.animation.vcr.hgt - 1;
}

} // namespace

/* ---- the frame sinks ------------------------------------------------------------

   Every primitive of a frame is given here in the animation's own
   coordinates (the `dimension` box, y up). It goes to the front end's
   pixel callbacks, scaled to the vcr.wid x vcr.hgt window as XPP always
   has (ani_xyscale and friends: truncated to whole pixels, points clamped
   to the window), and to ani_data.h in unit coordinates, unclamped: u = 0
   at xlo and 1 at xhi, v = 0 at ylo and 1 at yhi. The pen (colour, line
   width, text font) is the one the callbacks keep between primitives, as
   an X11 graphics context does: the colour starts black each frame, the
   width and font carry over. */

namespace {

struct Pen {
    int color = 0; /* palette index: 0 black, 20..29 the named colours, 30.. the colour map */
    int thick = 0;
    int size = 0, font = 0;
};
Pen pen;

double unit_x(double x) { return (x - ani_xlo) / (ani_xhi - ani_xlo); }
double unit_y(double y) { return (y - ani_ylo) / (ani_yhi - ani_ylo); }

void pen_color(int icol)
{
    xpp_ui.ani_color(icol);
    pen.color = icol;
}

void pen_thick(int t)
{
    if (t < 0) t = 0;
    xpp_ui.ani_thick(t);
    pen.thick = t;
}

void pen_font(int size, int font, int color)
{
    xpp_ui.ani_font(size, font, color);
    pen.size = size;
    pen.font = font;
    pen.color = color;
}

void put_line(const xpp::Session &s, double x1, double y1, double x2, double y2)
{
    int i1, j1, i2, j2;
    ani_xyscale(s,x1, y1, &i1, &j1);
    ani_xyscale(s,x2, y2, &i2, &j2);
    xpp_ui.ani_line(i1, j1, i2, j2);
    ani_data_line(unit_x(x1), unit_y(y1), unit_x(x2), unit_y(y2), pen.color, pen.thick);
}

/* a rectangle with corners (x1,y1) and (x2,y2), in either order */
void put_rect(const xpp::Session &s, double x1, double y1, double x2, double y2, int fill)
{
    int i1, j1, i2, j2, h, w;
    ani_xyscale(s,x1, y1, &i1, &j1);
    ani_xyscale(s,x2, y2, &i2, &j2);
    h = abs(j2 - j1);
    w = abs(i2 - i1);
    if (i1 > i2) i1 = i2;
    if (j1 > j2) j1 = j2;
    xpp_ui.ani_rect(i1, j1, w, h, fill);
    ani_data_rect(unit_x(x1), unit_y(y1), unit_x(x2), unit_y(y2), pen.color, pen.thick, fill);
}

/* a circle of radius r: in pixels the mean of the radius scaled along x and along y */
void put_circle(const xpp::Session &s, double x, double y, double r, int fill)
{
    int i1, j1, i2, j2, ir;
    ani_xyscale(s,x, y, &i1, &j1);
    ani_radscale(s,r, &i2, &j2);
    ir = (i2 + j2) / 2;
    xpp_ui.ani_arc(i1 - ir, j1 - ir, 2 * ir, 2 * ir, fill);
    ani_data_circle(unit_x(x), unit_y(y), r / (ani_xhi - ani_xlo), r / (ani_yhi - ani_ylo), pen.color, pen.thick,
                    fill);
}

void put_ellipse(const xpp::Session &s, double x, double y, double rx, double ry, int fill)
{
    int i1, j1, i2, j2;
    ani_xyscale(s,x, y, &i1, &j1);
    ani_rad2scale(s,rx, ry, &i2, &j2);
    xpp_ui.ani_arc(i1 - i2, j1 - j2, 2 * i2, 2 * j2, fill);
    ani_data_ellipse(unit_x(x), unit_y(y), rx / (ani_xhi - ani_xlo), ry / (ani_yhi - ani_ylo), pen.color,
                     pen.thick, fill);
}

/* a filled circle of r pixels (a comet's), whatever the window's size */
void put_dot(const xpp::Session &s, double x, double y, int r)
{
    int i, j;
    ani_xyscale(s,x, y, &i, &j);
    xpp_ui.ani_arc(i - r, j - r, 2 * r, 2 * r, 1);
    ani_data_dot(unit_x(x), unit_y(y), r, pen.color);
}

/* text from its baseline's left end */
void put_text(const xpp::Session &s, double x, double y, const char *str)
{
    int i, j;
    ani_xyscale(s,x, y, &i, &j);
    xpp_ui.ani_text(i, j, str);
    ani_data_text(unit_x(x), unit_y(y), str, pen.color, pen.size, pen.font);
}

} // namespace

namespace {

/* One logical line of the .ani file: a line ending in a backslash (cut
   there) goes on with the next; the end of the line becomes a space and
   one more follows, as the parser has always seen it. eof once nothing is
   left to read. */
std::string read_ani_line(xpp::LineReader &fp, bool &eof)
{
    std::string s;
    for (;;) {
        std::optional<std::string_view> line = fp.next();
        if (!line) {
            eof = true;
            break;
        }
        const size_t hat = line->find('\\');
        if (hat == std::string_view::npos) {
            s.append(*line);
            s += ' ';
            break;
        }
        s.append(line->substr(0, hat));
    }
    s += ' ';
    return s;
}

/* an expression compiled for evaluate(); false when it does not parse */
bool compile_expr(xpp::Session &s, const char *x, std::vector<int> &c)
{
    std::array<int, 300> com;
    int n;
    if (add_expr(s,x, com.data(), &n) == 1) return false;
    c.assign(com.begin(), com.begin() + n);
    return true;
}

/* a named colour ($RED ...): its palette index, or none (s is left
   de-spaced and upper case, as the expression compiled from it) */
int chk_ani_color(std::string &s, int *index)
{
    *index = -1;
    de_space(s.data());
    s.resize(std::strlen(s.c_str()));
    xpp::to_upper(s.data());
    if (s.empty()) {
        *index = 0;
        return 1;
    }
    if (s[0] == '$') {
        for (int j = 0; j < 12; j++) {
            if (s.compare(1, std::string::npos, color_names[j]) == 0) {
                *index = colorline[j];
                return 1;
            }
        }
    }
    return 0;
}

/* the colour argument of a drawing command: a named colour ($RED) is kept
   as minus its palette index, anything else is compiled as an expression
   whose value (0..1) picks a colour of the colour map */
int add_ani_color(xpp::Session &s, AniCom &a, std::string &col)
{
    int index;
    if (chk_ani_color(col, &index) == 1) {
        a.col.assign(1, -index);
        return 0;
    }
    return compile_expr(s, col.c_str(), a.col) ? 0 : -1;
}

/*  the commands  */

int add_ani_rline(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, std::string &col, const std::string &thick)
{
    /* a named colour is -index like every other command's (it was +index,
       which read the index as a compiled expression) */
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = std::atoi(thick.c_str());
    if (a.zthick < 0) a.zthick = 0;
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    return 0;
}

/* the comet's newest position (in the animation's coordinates) and colour */
void roll_comet(AniCom &a, double xn, double yn, int col)
{
    Comet &c = a.c;
    const int n = c.n;
    if (c.i < n) { /* not loaded yet */
        c.x[c.i] = xn;
        c.y[c.i] = yn;
        c.col[c.i] = col;
        c.i++;
        return;
    }
    /* its full so push down eliminating last */
    for (int i = 1; i < n; i++) {
        c.x[i - 1] = c.x[i];
        c.y[i - 1] = c.y[i];
        c.col[i - 1] = c.col[i];
    }
    c.x[n - 1] = xn;
    c.y[n - 1] = yn;
    c.col[n - 1] = col;
}

int add_ani_comet(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2, std::string &col,
                  const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = std::atoi(thick.c_str());
    const int n = std::atoi(x2.c_str());
    if (n <= 0) {
        xpp::log(XPP_LOG_WARN, "4th argument of comet must be positive integer!\n");
        return (-1);
    }
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    a.c.n = n;
    a.c.x.assign(n, 0.0);
    a.c.y.assign(n, 0.0);
    a.c.col.assign(n, 0);
    a.c.i = 0;
    return 1;
}

/* line, and the boxes of rect, frect, ellip and fellip */
int add_ani_line(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2,
                 const std::string &y2, std::string &col, const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = std::atoi(thick.c_str());
    if (a.zthick < 0) a.zthick = 0;
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    if (!compile_expr(s, x2.c_str(), a.x2)) return -1;
    if (!compile_expr(s, y2.c_str(), a.y2)) return -1;
    return 0;
}

int add_ani_null(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2,
                 const std::string &y2, std::string &col, const std::string &who)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    if (!compile_expr(s, who.c_str(), a.who)) return -1;
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    if (!compile_expr(s, x2.c_str(), a.x2)) return -1;
    if (!compile_expr(s, y2.c_str(), a.y2)) return -1;
    return 0;
}

int add_ani_circle(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2, std::string &col,
                   const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = std::atoi(thick.c_str());
    if (a.zthick < 0) a.zthick = 0;
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    if (!compile_expr(s, x2.c_str(), a.x2)) return -1;
    return 0;
}

/* text x;y;text, and vtext's value v (x2) after the text */
int add_ani_text(xpp::Session &s, AniCom &a, const std::string &x1, const std::string &y1, const std::string *x2, const std::string &text)
{
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    if (x2 && !compile_expr(s, x2->c_str(), a.x2)) return -1;
    a.text = text;
    return 0;
}

int add_ani_settext(AniCom &a, const std::string &x1, std::string &y1, std::string &col)
{
    int size = std::atoi(x1.c_str());
    int font = 0;
    int index = 0;
    de_space(y1.data());
    if (y1[0] == 's' || y1[0] == 'S') font = 1;
    if (chk_ani_color(col, &index) != 1) index = 0;
    if (size < 0) size = 0;
    if (size > 4) size = 4;
    a.tsize = size;
    a.tfont = font;
    a.tcolor = index;
    return 0;
}

void set_ani_dimension(const std::string &x1, const std::string &y1, const std::string &x2, const std::string &y2)
{
    const double xx1 = std::atof(x1.c_str());
    const double xx2 = std::atof(x2.c_str());
    const double yy1 = std::atof(y1.c_str());
    const double yy2 = std::atof(y2.c_str());

    if ((xx1 < xx2) && (yy1 < yy2)) {
        ani_xlo = xx1;
        ani_xhi = xx2;
        ani_ylo = yy1;
        ani_yhi = yy2;
    }
}

int add_ani_com(xpp::Session &s, int type, const std::string &x1, std::string &y1, const std::string &x2, const std::string &y2,
                std::string &col, const std::string &thick)
{
    int err = 0;
    if (type == COMNT || type == DIMENSION || type == PERMANENT || type == TRANSIENT || type == END || type == SPEED)
        return 1;
    AniCom a;
    a.type = type;
    a.flag = aniflag;
    switch (type) {
    case AXNULL:
    case AYNULL:
        err = add_ani_null(s, a, x1, y1, x2, y2, col, thick);
        break;
    case COMET:
        err = add_ani_comet(s, a, x1, y1, x2, col, thick);
        break;
    case LINE:
    case RECT:
    case FRECT:
    case ELLIP:
    case FELLIP:
        err = add_ani_line(s, a, x1, y1, x2, y2, col, thick);
        break;
    case RLINE:
        err = add_ani_rline(s, a, x1, y1, col, thick);
        break;
    case CIRC:
    case FCIRC:
        err = add_ani_circle(s, a, x1, y1, x2, col, thick);
        break;
    case TEXT:
        err = add_ani_text(s, a, x1, y1, nullptr, y2);
        break;
    case VTEXT:
        err = add_ani_text(s, a, x1, y1, &x2, y2);
        break;
    case SETTEXT:
        err = add_ani_settext(a, x1, y1, col);
        break;
    }
    if (err < 0) {
        free_ani(s);
        return -1;
    }
    my_ani.push_back(std::move(a));
    s.animation.ncom = static_cast<int>(my_ani.size());
    return 1;
}

void init_ani_stuff(xpp::Session &s)
{
    ani_xlo = 0.0;
    ani_ylo = 0.0;
    ani_xhi = 1.0;
    ani_yhi = 1.0;
    aniflag = TRANSIENT;
    my_ani.clear();
    s.animation.ncom = 0;
    ani_lastx = 0.0;
    ani_lasty = 0.0;
    s.animation.vcr.pos = 0;
    s.animation.grab_flag = 0; /*********** GRABBER *******************/
    ani_grab.clear();
}

void free_ani(xpp::Session &s)
{
    init_ani_stuff(s);
}

/*************************  GRABBER CODE *****************************/

int add_grab_task(xpp::Session &s, const std::string &lhs, const std::string &rhs, AniGrab &g, int which)
{
    if (which != 1 && which != 2) return (-1);
    GrabTask &task = which == 1 ? g.start : g.end;
    if (which == 2 && lhs.compare(0, 6, "runnow") == 0) {
        task.runnow = std::atoi(rhs.c_str());
        return (1);
    }
    if (static_cast<int>(task.events.size()) >= MAX_GEVENTS) return (-1); /* too many events */
    GrabEvent e;
    e.lhs = lhs;
    if (!compile_expr(s, rhs.c_str(), e.rhs)) {
        xpp::log(XPP_LOG_WARN, "Bad right-hand side for grab event {}\n", rhs);
        return (-1);
    }
    task.events.push_back(std::move(e));
    return (1);
}

int ani_grab_tasks(xpp::Session &s, const std::string &line, AniGrab &g, int which)
{
    std::string form, lhs;
    for (const char c : line) {
        if (c == '{' || c == ' ') continue;
        if (c == ';' || c == '}') {
            if (add_grab_task(s, lhs, form, g, which) < 0) return (-1);
            form.clear();
            continue;
        }
        if (c == '=') {
            lhs = form;
            form.clear();
            continue;
        }
        form += c;
    }
    return (1);
}

/* grab x;y;tol, then its start and end tasks on the next two lines */
int add_grab_command(xpp::Session &s, const std::string &xs, const std::string &ys, const std::string &ts, xpp::LineReader &fp)
{
    bool eof = false;
    const std::string start = read_ani_line(fp, eof);
    const std::string end = read_ani_line(fp, eof);

    if (static_cast<int>(ani_grab.size()) >= MAX_ANI_GRAB) {
        xpp::log(XPP_LOG_WARN, "Too many grabbables! \n");
        return (-1);
    }
    AniGrab g;
    double z = std::atof(ts.c_str());
    if (z <= 0.0) z = .02;
    g.tol = z;
    if (!compile_expr(s, xs.c_str(), g.x)) {
        xpp::log(XPP_LOG_WARN, "Bad grab x {} \n", xs);
        return (-1);
    }
    if (!compile_expr(s, ys.c_str(), g.y)) {
        xpp::log(XPP_LOG_WARN, "Bad grab y {} \n", ys);
        return (-1);
    }
    if (ani_grab_tasks(s, start, g, 1) < 0) return (-1);
    if (ani_grab_tasks(s, end, g, 2) == (-1)) return (-1);
    ani_grab.push_back(std::move(g));
    return (1);
}

int run_now_grab(void)
{
    if (who_was_grabbed < 0 || who_was_grabbed >= static_cast<int>(ani_grab.size())) return (0);
    return (ani_grab[who_was_grabbed].end.runnow);
}

int search_for_grab(double x, double y)
{
    double dmin = 100000000;
    int imin = -1;
    for (int i = 0; i < static_cast<int>(ani_grab.size()); i++) {
        const double u = ani_grab[i].zx;
        const double v = ani_grab[i].zy;
        const double d = std::sqrt((x - u) * (x - u) + (y - v) * (y - v));
        if ((d < dmin) && (d < ani_grab[i].tol)) {
            dmin = d;
            imin = i;
        }
    }
    return (imin);
}

void do_grab_tasks(xpp::Session &s, int which) /* which=1 for start, 2 for end */
{
    const int i = who_was_grabbed;
    if (i < 0 || i >= static_cast<int>(ani_grab.size())) return; /*  no legal grab point */
    if (which != 1 && which != 2) return;
    for (GrabEvent &e : (which == 1 ? ani_grab[i].start : ani_grab[i].end).events)
        set_val(s,e.lhs.c_str(), evaluate(s,e.rhs.data()));
}

/* a command is known by its first two letters */
const struct {
    const char *prefix;
    int type;
} ani_commands[] = {
    {"GR", GRAB},     {"LI", LINE},         {"RL", RLINE},     {"RE", RECT},  {"FR", FRECT},   {"EL", ELLIP},
    {"FE", FELLIP},   {"CI", CIRC},         {"FC", FCIRC},     {"VT", VTEXT}, {"TE", TEXT},    {"SE", SETTEXT},
    {"TR", TRANSIENT}, {"PE", PERMANENT},   {"DI", DIMENSION}, {"EN", END},   {"DO", END},     {"SP", SPEED},
    {"CO", COMET},    {"XN", AXNULL},       {"YN", AYNULL},
};

/* one of a command's arguments: the next token up to one of delims; an
   optional one ends the command's arguments when it is missing */
struct Arg {
    std::string *dst;
    const char *delims;
    bool optional;
};

/* false when a required argument is missing */
bool read_args(xpp::Tokens &tokens, std::initializer_list<Arg> args)
{
    for (const Arg &a : args) {
        std::optional<std::string_view> nxt = tokens.next(a.delims);
        if (!nxt) return a.optional;
        *a.dst = *nxt;
    }
    return true;
}

/*  The reader is an argument since the GRAB command reads in two
    additional lines */
int parse_ani_string(xpp::Session &s, std::string &text, xpp::LineReader &fp)
{
    std::string x1, x2, x3, x4, col, thick;
    int type = COMNT;
    xpp::Tokens tokens(text);
    std::optional<std::string_view> first = tokens.next("; ");
    if (!first) return -1;
    std::string command(*first);
    xpp::to_upper(command.data());
    for (const auto &k : ani_commands)
        if (std::strncmp(k.prefix, command.c_str(), 2) == 0) type = k.type;
    bool ok = true;
    switch (type) {
    case GRAB:
        if (!read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x3, ";", false}})) return -1;
        return add_grab_command(s, x1, x2, x3, fp);
    case AXNULL:
    case AYNULL:
        ok = read_args(tokens, {{&x1, ";", false},
                        {&x2, ";", false},
                        {&x3, ";", false},
                        {&x4, ";\n", false},
                        {&col, ";\n", false},
                        {&thick, "\n", false}});
        break;
    case LINE:
    case RECT:
    case ELLIP:
    case FELLIP:
    case FRECT:
        ok = read_args(tokens, {{&x1, ";", false},
                        {&x2, ";", false},
                        {&x3, ";", false},
                        {&x4, ";\n", false},
                        {&col, ";\n", true},
                        {&thick, "\n", true}});
        break;
    case RLINE:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&col, ";\n", true}, {&thick, "\n", true}});
        break;
    case COMET:
        ok = read_args(tokens, {{&x1, ";", false},
                        {&x2, ";", false},
                        {&thick, ";", false},
                        {&x3, ";\n", false},
                        {&col, ";\n", true}});
        break;
    case CIRC:
    case FCIRC:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x3, ";", false}, {&col, ";\n", true}});
        break;
    case SETTEXT:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&col, ";", false}});
        break;
    case TEXT:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x4, ";", false}});
        break;
    case VTEXT:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x4, ";", false}, {&x3, ";\n", false}});
        break;
    case SPEED: {
        std::optional<std::string_view> nxt = tokens.next(" \n");
        if (!nxt) return -1;
        s.animation.speed = std::atoi(std::string(*nxt).c_str());
        if (s.animation.speed < 0) s.animation.speed = 0;
        if (s.animation.speed > 1000) s.animation.speed = 1000;
        return 1;
    }
    case DIMENSION:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x3, ";", false}, {&x4, ";\n", false}});
        break;
    }
    if (!ok) return -1;

    if (type == END) return 0;
    if (type == TRANSIENT) {
        aniflag = TRANSIENT;
        return 1;
    }
    if (type == COMNT) return 1;
    if (type == PERMANENT) {
        aniflag = PERMANENT;
        return 1;
    }

    if (type == DIMENSION) {
        set_ani_dimension(x1, x2, x3, x4);
        return 1;
    }
    return (add_ani_com(s,type, x1, x2, x3, x4, col, thick));
}

int load_ani_file(xpp::Session &s, xpp::LineReader &fp)
{
    std::string expanded, big;
    int jj1, jj2;
    int ans = 0, flag;
    bool eof = false;
    ani_lineno = 1;
    for (;;) {
        std::string old = read_ani_line(fp, eof);
        search_array(old.data(), expanded, &jj1, &jj2, &flag);
        for (int jj = jj1; jj <= jj2; jj++) {
            subsk(expanded.c_str(), big, jj, flag);
            ans = parse_ani_string(s,big, fp);
        }

        if (ans == 0 || eof) break;
        if (ans < 0) { /* error occurred !! */
            xpp::log(XPP_LOG_DEBUG, " error at line {}\n", ani_lineno);
            free_ani(s);
            return 0;
        }
        ani_lineno++;
    }
    return 1;
}

/* the .ani filename: -anifile's is one of the model's files (model_file,
   model_files.h), one picked in the animation window a file of the disk */
xpp::Result<> ani_new_file(xpp::Session &s, const char *filename, bool model_file)
{
    xpp::LineReader fp = model_file ? xpp::model_file_lines(s.model(),filename) : xpp::LineReader(filename);
    if (!fp) {
        return xpp::fail("animation", "Couldn't open ani-file");
    }
    if (s.animation.ncom > 0) free_ani(s);
    /* a new animation: its frames start again, nothing of the old one shows */
    ani_data_forget();
    if (load_ani_file(s,fp) == 0) {
        return xpp::fail("animation", xpp::format("Bad ani-file at line {}", ani_lineno));
    }
    return {};
}

void ani_frame(xpp::Session &s, int task)
{
    xpp_ui.ani_clear();
    if (task == 1) {
        set_ani_perm(s);
        reset_comets();
        return;
    }
    render_ani(s);
    xpp_ui.ani_show();
}

void set_to_init_data(xpp::Session &s)
{
    int i;
    for (i = 0; i < s.model().node; i++) s.last_ic[i] = getvar(s,i + 1);
    for (i = s.model().node + s.model().fix_var; i < s.model().node + s.model().fix_var + s.model().nmarkov; i++) s.last_ic[i - s.model().fix_var] = getvar(s,i + 1);
    redraw_ics();
}

void set_from_init_data(xpp::Session &s)
{
    std::array<double, MAXODE> y;
    for (int i = 0; i < s.model().node + s.model().nmarkov; i++) y[i] = s.last_ic[i];
    set_fix_rhs(s,s.numerics.t0, y.data());
}

void ani_disk_warn(xpp::Session &s)
{
    unsigned int total = (s.browser.view.maxrow * s.animation.vcr.wid * s.animation.vcr.hgt * 3) / (s.animation.mpeg.skip * s.animation.vcr.inc);
    total = total / (1024 * 1024);
    if (total > 10) {
        const std::string q = xpp::format(" {} Mb disk space needed! Continue?", total);
        const char ans = static_cast<char>(TwoChoice("YES", "NO", q.c_str(), "yn"));
        if (ans != 'y') s.animation.mpeg.flag = 0;
    }
}

void eval_ani_color(xpp::Session &s, int j)
{
    AniCom &a = my_ani[j];
    if (a.col[0] > 0) {
        double z = evaluate(s,a.col.data());
        if (z > 1) z = 1.0;
        if (z < 0) z = 0.0;
        a.zcol = z;
    }
}

void eval_ani_com(xpp::Session &s, int j)
{
    AniCom &a = my_ani[j];
    a.zx1 = evaluate(s,a.x1.data());
    a.zy1 = evaluate(s,a.y1.data());

    switch (a.type) {
    case LINE:
    case RECT:
    case FRECT:
    case ELLIP:
    case FELLIP:
    case AXNULL:
    case AYNULL:
        a.zx2 = evaluate(s,a.x2.data());
        a.zy2 = evaluate(s,a.y2.data());
        break;
    case CIRC:
    case FCIRC:
        a.zrad = evaluate(s,a.x2.data());
        break;
    case VTEXT:
        a.zval = evaluate(s,a.x2.data());
        break;
    }

    if (a.type == AXNULL || a.type == AYNULL) a.zval = evaluate(s,a.who.data());
}

void set_ani_font_stuff(int size, int font, int color) { pen_font(size, font, color); }

void set_ani_col(int j)
{
    const int c = my_ani[j].col[0];
    int icol;

    if (c <= 0)
        icol = -c;
    else
        icol = static_cast<int>(color_table.count * my_ani[j].zcol) + FIRSTCOLOR;
    pen_color(icol);
    LastAniColor = icol;
}

/* the comet keeps its last n positions (in the animation's coordinates)
   with their colours: filled circles of -thickness pixels, or lines */
void draw_ani_comet(const xpp::Session &s, int j)
{
    AniCom &a = my_ani[j];
    pen_thick(a.zthick);
    set_ani_col(j);
    roll_comet(a, a.zx1, a.zy1, LastAniColor);
    const int nn = a.c.i;
    if (a.zthick < 0) {
        const int ir = -a.zthick;
        for (int k = 0; k < nn; k++) {
            pen_color(a.c.col[k]);
            put_dot(s,a.c.x[k], a.c.y[k], ir);
        }
    } else {
        if (nn > 2) {
            for (int k = 1; k < nn; k++) {
                pen_color(a.c.col[k]);
                put_line(s,a.c.x[k - 1], a.c.y[k - 1], a.c.x[k], a.c.y[k]);
            }
        }
    }
}

/* a nullcline of the phase plane, its box [x1,x2] x [y1,y2] mapped to the
   animation's [0,1] x [0,1] (as XPP always has, whatever `dimension` says) */
void draw_ani_null(const xpp::Session &s, int j, int id)
{
    const AniCom &a = my_ani[j];
    double xl = a.zx1, xh = a.zx2, yl = a.zy1, yh = a.zy2;
    float *v;
    int n;
    float dx = xh - xl, dy = yh - yl;
    if (dx == 0.0 || dy == 0.0) return;

    set_ani_col(j);
    const int who = static_cast<int>(a.zval); /* the nullcline that you want  -1 is the default cline */
    if (get_nullcline_floats(&v, &n, who, id) == 1) return;
    for (int i = 0; i < n; i++) {
        const int i4 = 4 * i;
        const float x1 = (v[i4] - xl) / dx;
        const float y1 = (v[i4 + 1] - yl) / dy;
        const float x2 = (v[i4 + 2] - xl) / dx;
        const float y2 = (v[i4 + 3] - yl) / dy;
        put_line(s,x1, y1, x2, y2);
    }
}

void draw_ani_line(const xpp::Session &s, int j)
{
    const AniCom &a = my_ani[j];
    double x1 = a.zx1, x2 = a.zx2, y1 = a.zy1, y2 = a.zy2;
    pen_thick(a.zthick);
    set_ani_col(j);
    put_line(s,x1, y1, x2, y2);
    ani_lastx = x2;
    ani_lasty = y2;
}

void draw_ani_rline(const xpp::Session &s, int j)
{
    const AniCom &a = my_ani[j];
    double x1 = ani_lastx + a.zx1, y1 = ani_lasty + a.zy1;
    pen_thick(a.zthick);
    set_ani_col(j);
    put_line(s,ani_lastx, ani_lasty, x1, y1);
    ani_lastx = x1;
    ani_lasty = y1;
}

/* rect, ellip and circle, outlined or filled (frect, fellip, fcircle) */
void draw_ani_shape(const xpp::Session &s, int j)
{
    const AniCom &a = my_ani[j];
    pen_thick(a.zthick);
    set_ani_col(j);
    switch (a.type) {
    case RECT:
    case FRECT:
        put_rect(s,a.zx1, a.zy1, a.zx2, a.zy2, a.type == FRECT);
        break;
    case ELLIP:
    case FELLIP:
        put_ellipse(s,a.zx1, a.zy1, a.zx2, a.zy2, a.type == FELLIP);
        break;
    case CIRC:
    case FCIRC:
        put_circle(s,a.zx1, a.zy1, a.zrad, a.type == FCIRC);
        break;
    }
}

void draw_ani_text(const xpp::Session &s, int j) { put_text(s,my_ani[j].zx1, my_ani[j].zy1, my_ani[j].text.c_str()); }

void draw_ani_vtext(const xpp::Session &s, int j)
{
    put_text(s,my_ani[j].zx1, my_ani[j].zy1, xpp::format("{}{:g}", my_ani[j].text, my_ani[j].zval).c_str());
}

/* Draw little black x's where the grab points are */
void draw_grab_points(xpp::Session &s)
{
    pen_color(0);
    for (AniGrab &g : ani_grab) {
        const double xc = evaluate(s,g.x.data());
        const double yc = evaluate(s,g.y.data());
        g.zx = xc;
        g.zy = yc;
        const double z = g.tol;
        put_line(s,xc + z, yc + z, xc - z, yc - z);
        put_line(s,xc + z, yc - z, xc - z, yc + z);
    }
    show_grab_points = 0;
}

} // namespace

void update_ani_motion_stuff(xpp::Session &s, int x, int y)
{
    double dt;
    ami.t2 = ami.t1;
    ami.t1 = get_current_time();
    ami.ox = ami.x;
    ami.oy = ami.y;
    ani_ij_to_xy(s,x, y, &ami.x, &ami.y);
    dt = ami.t1 - ami.t2;
    if (dt == 0.0) dt = 10000000000;
    ami.vx = (ami.x - ami.ox) / dt;
    ami.vy = (ami.y - ami.oy) / dt;
    set_val(s,"mouse_x", ami.x);
    set_val(s,"mouse_y", ami.y);
    set_val(s,"mouse_vx", ami.vx);
    set_val(s,"mouse_vy", ami.vy);
    do_grab_tasks(s, 1);
    fix_only(s);
    ani_frame(s,0);
}

/*************************** End motion & speed stuff   ****************/

void ani_create_mpeg(xpp::Session &s)
{
    static const char *n[] = {"PPM 0/1", "Basename", "AniGif(0/1)"};
    std::array<std::string, 3> values;
    s.animation.mpeg.flag = 0;
    values[0] = xpp::format("{:d}", s.animation.mpeg.flag);
    values[1] = xpp::format("{:.24}", s.animation.mpeg.root);
    values[2] = xpp::format("{:d}", s.animation.mpeg.aviflag);
    static const int kinds[] = {XPP_FIELD_INTEGER, XPP_FIELD_FILE, XPP_FIELD_INTEGER};
    const int status = do_string_box_of(3, 1, "Frame saving", n, values, kinds);
    if (status != 0) {
        s.animation.mpeg.flag = std::atoi(values[0].c_str());
        if (s.animation.mpeg.flag > 0) s.animation.mpeg.flag = 1;
        s.animation.mpeg.aviflag = std::atoi(values[2].c_str());
        s.animation.mpeg.root = values[1];
        if (s.animation.mpeg.aviflag == 1) s.animation.mpeg.flag = 0;
    } else
        s.animation.mpeg.flag = 0;
    if (s.animation.mpeg.flag == 1) ani_disk_warn(s);
}

void ani_newskip(xpp::Session &s)
{
    std::string bob = xpp::format("{}", s.animation.vcr.inc);
    int status = get_dialog_of("Frame skip", "Increment:", bob, "Ok", "Cancel", XPP_FIELD_INTEGER);
    if (status != 0) {
        s.animation.vcr.inc = std::atoi(bob.c_str());
        if (s.animation.vcr.inc <= 0) s.animation.vcr.inc = 1;
    }
}

void on_the_fly(xpp::Session &s, int task)
{
    if (s.animation.vcr.iexist == 0 || s.animation.ncom == 0) return;
    ani_frame(s,task);
    waitasec(on_the_fly_speed);
}

void ani_flip1(xpp::Session &s, int n)
{
    if (s.animation.ncom == 0) return;
    if (s.browser.view.maxrow < 2) return;
    float **ss = s.browser.view.data;
    xpp_ui.ani_clear();
    if (s.animation.vcr.pos == 0) set_ani_perm(s);

    s.animation.vcr.pos = s.animation.vcr.pos + n;
    if (s.animation.vcr.pos >= s.browser.view.maxrow) s.animation.vcr.pos = s.browser.view.maxrow - 1;
    if (s.animation.vcr.pos < 0) s.animation.vcr.pos = 0;
    const int row = s.animation.vcr.pos;

    const double t = static_cast<double>(ss[0][row]);
    std::array<double, MAXODE> y;
    for (int i = 0; i < s.model().node + s.model().nmarkov; i++) y[i] = static_cast<double>(ss[i + 1][row]);
    set_fix_rhs(s,t, y.data());

    render_ani(s);
    xpp_ui.ani_show();
}

void ani_zero(xpp::Session &s)
{
    s.animation.vcr.iexist = 0;
    s.animation.vcr.ok = 0;
    s.animation.vcr.inc = 1;
    s.animation.vcr.pos = 0;
    s.animation.ncom = 0;
    s.animation.speed = 10;
    aniflag = TRANSIENT;
    s.animation.grab_flag = 0;
    if (s.animation.options.use_file)
        s.animation.vcr.file = s.animation.options.file;
    else {
        /* dirname() may write into its argument or return static storage:
           a copy of s.model().this_file */
        std::string dir = s.model().this_file;
        s.animation.vcr.file = dirname(dir.data());
        s.animation.vcr.file += '/';
    }
}

int get_ani_file(xpp::Session &s, const char *fname)
{
    if (fname == nullptr) {
        std::string file = s.animation.vcr.file;
        if (file_selector("Load animation", file, "*.ani") == 0) return 0;
        s.animation.vcr.file = file;
    } else if (fname != s.animation.vcr.file.c_str())
        s.animation.vcr.file = fname;
    const bool model_file = fname != nullptr && s.animation.options.use_file && s.animation.vcr.file == s.animation.options.file;
    if (!xpp::ok_or_show(ani_new_file(s,s.animation.vcr.file.c_str(), model_file))) return 0;
    s.animation.vcr.ok = 1; /* loaded and compiled */
    xpp::log(XPP_LOG_INFO, "Loaded {} lines successfully!\n", s.animation.ncom);
    s.animation.grab_flag = 0;
    return 1;
}

void reset_comets(void)
{
    for (AniCom &a : my_ani)
        if (a.type == COMET) a.c.i = 0;
}

void render_ani(xpp::Session &s)
{
    xpp_ui.ani_slider(s);
    pen.color = 0; /* ani_clear gave the frame a black pen */
    ani_data_begin();
    for (int i = 0; i < s.animation.ncom; i++) {
        const int type = my_ani[i].type;
        const int flag = my_ani[i].flag;
        if (type == LINE || type == RLINE || type == RECT || type == FRECT || type == CIRC || type == FCIRC ||
            type == ELLIP || type == FELLIP || type == COMET || type == AXNULL || type == AYNULL)
            eval_ani_color(s, i);
        if (type != SETTEXT && flag == TRANSIENT) eval_ani_com(s, i);
        switch (type) {
        case AXNULL:
        case AYNULL:
            draw_ani_null(s,i, type - AXNULL);
            break;
        case SETTEXT:
            set_ani_font_stuff(my_ani[i].tsize, my_ani[i].tfont, my_ani[i].tcolor);
            break;
        case TEXT:
            draw_ani_text(s,i);
            break;
        case VTEXT:
            draw_ani_vtext(s,i);
            break;
        case LINE:
            draw_ani_line(s,i);
            break;
        case COMET:
            draw_ani_comet(s,i);
            break;
        case RLINE:
            draw_ani_rline(s,i);
            break;
        case RECT:
        case FRECT:
        case ELLIP:
        case FELLIP:
        case CIRC:
        case FCIRC:
            draw_ani_shape(s,i);
            break;
        }
    }
    if (show_grab_points == 1) draw_grab_points(s);
    {
        AniDataFrame f;
        f.pos = s.animation.vcr.pos;
        f.rows = s.browser.view.maxrow;
        f.t = getvar(s,0);
        f.speed = s.animation.speed;
        f.skip = s.animation.vcr.inc;
        f.xlo = ani_xlo;
        f.ylo = ani_ylo;
        f.xhi = ani_xhi;
        f.yhi = ani_yhi;
        f.w = s.animation.vcr.wid;
        f.h = s.animation.vcr.hgt;
        ani_data_end(&f);
    }
}

void set_ani_perm(xpp::Session &s)
{
    set_from_init_data(s);
    for (int i = 0; i < s.animation.ncom; i++) {
        const int type = my_ani[i].type;
        if (my_ani[i].flag == PERMANENT) {
            if (my_ani[i].type != SETTEXT) eval_ani_com(s, i);
            if (type == LINE || type == RLINE || type == RECT || type == FRECT || type == CIRC || type == FCIRC ||
                type == ELLIP || type == FELLIP)
                eval_ani_color(s, i);
        }
    }
}

/* ---- what the animation window's buttons and mouse do (was spread over
   aniparse.c's X11 event handlers) ---- */

/* a new animation window of vcr.wid x vcr.hgt exists */
void ani_view_created(xpp::Session &s)
{
    s.animation.mpeg.flag = 0;
    s.animation.mpeg.root = "frame";
    s.animation.mpeg.skip = 1;
    s.animation.vcr.pos = 0;
    if (s.animation.options.use_file) get_ani_file(s,s.animation.vcr.file.c_str());
}

/* Grab: show the first frame with the grab points and wait for the mouse */
void ani_grab_start(xpp::Session &s)
{
    if (ani_grab.empty()) return;
    if (s.animation.vcr.ok) {
        s.animation.vcr.pos = 0;

        show_grab_points = 1;
        ani_frame(s,1);
        ani_frame(s,0);
        s.animation.grab_flag = 1;
    }
}

/* Reset: back to the first frame */
void ani_reset(xpp::Session &s)
{
    s.animation.vcr.pos = 0;
    reset_comets();
    xpp_ui.ani_slider(s);
    ani_flip1(s,0);
}

/* the mouse went down (flag 1) or up (0) at pixel ix,iy while grabbing */
void ani_grab_mouse(xpp::Session &s, int flag, int ix, int iy)
{
    if (flag == 1) {
        ami.t1 = get_current_time();
        ani_ij_to_xy(s,ix, iy, &ami.x, &ami.y);
        who_was_grabbed = search_for_grab(ami.x, ami.y);
        if (who_was_grabbed < 0) xpp::log(XPP_LOG_INFO, "Nothing grabbed\n");
    }
    if (flag == 0) { /* This is BUTTON RELEASE  */
        if (who_was_grabbed < 0) return;
        do_grab_tasks(s, 2);
        set_to_init_data(s);
        s.animation.grab_flag = 0;
        redraw_params();
        if (run_now_grab()) {
            xpp::run_now(s);
            s.animation.grab_flag = 0;
        }
    }
}
