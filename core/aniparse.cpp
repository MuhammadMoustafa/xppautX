/* The animator: the .ani language (parsing, evaluation per frame) and the
   drawing of a frame.

   A frame is computed in the animation's own coordinates (`dimension`,
   [0,1] x [0,1] by default, y up) and each primitive goes to ani_data.h
   (the "frame sinks" below), which keeps it in unit coordinates for a front
   end that draws it itself (docs/protocol.md "The animation as data"). */
#include "xpp_files.h"
#include <array>
#include <algorithm>
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
#include "xpp_zip.h"
#include "snapx.h"
#include "form_ode.h"
#include "my_rhs.h"
#include "nullcline.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "integrate.h"
#include "browse.h"
#include "graf_par.h"
#include <libgen.h>
#include "colormap.h"
#include "comline.h"
#include "xpp_batch.h"

namespace xpp {

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


/************************  end grabber **********************/

constexpr int on_the_fly_speed = 10;
/* A trail's point defaults to the smallest whole-pixel radius. */
constexpr int default_trail_radius = 1;
/* The existing text renderer's two fonts, in its index order. */
constexpr std::array<std::string_view, 2> ani_font_names{"roman", "symbol"};

void set_ani_font_stuff(xpp::Session &s, int size, int font, const xpp::Color &color);
void eval_ani_color(xpp::Session &s, int j);
void eval_ani_com(xpp::Session &s, int j);
void do_grab_tasks(xpp::Session &s, int which);
void draw_grab_points(xpp::Session &s);

/*************************  NEW ANIMaTION STUFF ***********************/

/* seconds, for the mouse's speeds (only differences are used) */
double get_current_time(void)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/**************   DRAWING ROUTINES   *******************/

void ani_ij_to_xy(xpp::Session &s, int ix, int iy, double *x, double *y)
{
    double dx = (s.animation.xhi - s.animation.xlo) / static_cast<double>(s.animation.vcr.wid);
    double dy = (s.animation.yhi - s.animation.ylo) / static_cast<double>(s.animation.vcr.hgt);
    *x = s.animation.xlo + static_cast<double>(ix) * dx;
    *y = s.animation.ylo + static_cast<double>(s.animation.vcr.hgt - iy) * dy;
}

} // namespace

/* ---- the frame sinks ------------------------------------------------------------

   Every primitive of a frame is given here in the animation's own
   coordinates (the `dimension` box, y up) and goes to ani_data.h in unit
   coordinates, unclamped: u = 0 at xlo and 1 at xhi, v = 0 at ylo and 1 at
   yhi. The s.animation.pen (colour, line width, text font) is the one kept
   between primitives, as an X11 graphics context was: the colour starts
   black each frame, the width and font carry over. */

namespace {


double unit_x(xpp::Session &s, double x) { return (x - s.animation.xlo) / (s.animation.xhi - s.animation.xlo); }
double unit_y(xpp::Session &s, double y) { return (y - s.animation.ylo) / (s.animation.yhi - s.animation.ylo); }

void pen_color(xpp::Session &s, const xpp::Color &color) { s.animation.pen.color = color; }

void pen_thick(xpp::Session &s, double t)
{
    if (t < 0) t = 0;
    s.animation.pen.thick = t;
}

void pen_font(xpp::Session &s, int size, int font, const xpp::Color &color)
{
    s.animation.pen.size = size;
    s.animation.pen.font = font;
    s.animation.pen.color = color;
}

void put_line(xpp::Session &s, double x1, double y1, double x2, double y2)
{
    ani_data_line(s, unit_x(s, x1), unit_y(s, y1), unit_x(s, x2), unit_y(s, y2), s.animation.pen.color, s.animation.pen.thick);
}

/* a rectangle with corners (x1,y1) and (x2,y2), in either order */
void put_rect(xpp::Session &s, double x1, double y1, double x2, double y2, int fill)
{
    ani_data_rect(s, unit_x(s, x1), unit_y(s, y1), unit_x(s, x2), unit_y(s, y2), s.animation.pen.color, s.animation.pen.thick, fill);
}

/* a circle of radius r: in pixels the mean of the radius scaled along x and along y */
void put_circle(xpp::Session &s, double x, double y, double r, int fill)
{
    ani_data_circle(s, unit_x(s, x), unit_y(s, y), r / (s.animation.xhi - s.animation.xlo), r / (s.animation.yhi - s.animation.ylo), s.animation.pen.color, s.animation.pen.thick,
                    fill);
}

void put_ellipse(xpp::Session &s, double x, double y, double rx, double ry, int fill)
{
    ani_data_ellipse(s, unit_x(s, x), unit_y(s, y), rx / (s.animation.xhi - s.animation.xlo), ry / (s.animation.yhi - s.animation.ylo), s.animation.pen.color,
                     s.animation.pen.thick, fill);
}

/* a filled circle of r pixels (a comet's), whatever the window's size */
void put_dot(xpp::Session &s, double x, double y, int r)
{
    ani_data_dot(s, unit_x(s, x), unit_y(s, y), r, s.animation.pen.color);
}

/* text from its baseline's left end */
void put_text(xpp::Session &s, double x, double y, const char *str)
{
    ani_data_text(s, unit_x(s, x), unit_y(s, y), str, s.animation.pen.color, s.animation.pen.size, s.animation.pen.font);
}

} // namespace

namespace {

/* One logical line of the .ani file: a line ending in a backslash (cut
   there) goes on with the next; the end of the line becomes a space and
   one more follows, as the parser has always seen it. eof once nothing is
   left to read. */
std::string read_ani_line(xpp::LineReader &fp, bool &eof, int &lines, Place &place)
{
    std::string s;
    place.line = lines + 1;
    place.source.clear();
    for (;;) {
        std::optional<std::string_view> line = fp.next();
        if (!line) {
            eof = true;
            break;
        }
        if (lines + 1 == place.line) place.source = *line;
        ++lines;
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
    int n;
    if (xpp::add_expr(s,x, c, &n) == 1) return false;
    return true;
}

/* a named colour of a .ani file ($RED ...) is XPPAUT's colour of that name (a foreign format: read on import only) */
constexpr std::array<std::string_view, 12> ani_color_names{"WHITE", "RED", "REDORANGE", "ORANGE", "YELLOWORANGE", "YELLOW",
                                                           "YELLOWGREEN", "GREEN", "BLUEGREEN", "BLUE", "PURPLE", "BLACK"};

/* a named colour ($RED ...) or none given (the foreground): true with its colour (s is left de-spaced and upper
   case, as the expression compiled from it) */
bool named_ani_color(std::string &s, xpp::Color &color)
{
    xpp::de_space(s.data());
    s.resize(std::strlen(s.c_str()));
    xpp::to_upper(s.data());
    color = xpp::foreground_color;
    if (s.empty()) return true;
    if (s[0] == '$') {
        for (std::size_t j = 0; j < ani_color_names.size(); j++) {
            if (std::string_view(s).substr(1) == ani_color_names[j]) {
                color = xpp::xppaut_color(j <= static_cast<std::size_t>(xpp::LAST_PLOT_COLOR) ? static_cast<int>(j) : 0);
                return true;
            }
        }
    }
    return false;
}

/* the colour argument of a drawing command: a named colour ($RED) is kept, anything else is compiled as an
   expression whose value (0..1) picks a colour of the colour map */
int add_ani_color(xpp::Session &s, AniCom &a, std::string &col)
{
    if (named_ani_color(col, a.color)) {
        a.col_expr.clear();
        return 0;
    }
    return compile_expr(s, col.c_str(), a.col_expr) ? 0 : -1;
}

/*  the commands  */

/* a command's thickness: optional (none is 0), a whole number of 0 or more */
int ani_thickness(xpp::TypedFields &f, const std::string &thick)
{
    return thick.empty() ? 0 : f.integer("thickness", thick, 0);
}

int add_ani_rline(xpp::Session &s, xpp::TypedFields &f, AniCom &a, const std::string &x1, const std::string &y1, std::string &col, const std::string &thick)
{
    /* a named colour is -index like every other command's (it was +index,
       which read the index as a compiled expression) */
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = ani_thickness(f, thick);
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    return 0;
}

/* the comet's newest position (in the animation's coordinates) and colour */
void roll_comet(AniCom &a, double xn, double yn, const xpp::Color &col)
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

int add_ani_comet(xpp::Session &s, xpp::TypedFields &f, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2, std::string &col,
                  const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    /* a comet's thickness may be negative: then its points are dots of -thickness */
    a.zthick = f.integer("thickness", thick);
    const int n = f.integer("points", x2, 1, MAX_COMET_POINTS);
    if (f.error()) return -1;
    if (!compile_expr(s, x1.c_str(), a.x1)) return -1;
    if (!compile_expr(s, y1.c_str(), a.y1)) return -1;
    a.c.n = n;
    a.c.x.assign(n, 0.0);
    a.c.y.assign(n, 0.0);
    a.c.col.assign(n, xpp::foreground_color);
    a.c.i = 0;
    return 1;
}

/* line, and the boxes of rect, frect, ellip and fellip */
int add_ani_line(xpp::Session &s, xpp::TypedFields &f, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2,
                 const std::string &y2, std::string &col, const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = ani_thickness(f, thick);
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

int add_ani_circle(xpp::Session &s, xpp::TypedFields &f, AniCom &a, const std::string &x1, const std::string &y1, const std::string &x2, std::string &col,
                   const std::string &thick)
{
    if (add_ani_color(s, a, col) < 0) return -1;
    a.zthick = ani_thickness(f, thick);
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

int add_ani_settext(xpp::TypedFields &f, AniCom &a, const std::string &x1, std::string &y1, std::string &col)
{
    const int size = f.integer("size", x1, 0, ANI_TEXT_SIZES);
    int font = 0;
    xpp::Color color;
    xpp::de_space(y1.data());
    if (y1[0] == 's' || y1[0] == 'S') font = 1;
    if (!named_ani_color(col, color)) color = xpp::foreground_color;
    if (f.error()) return -1;
    a.tsize = size;
    a.tfont = font;
    a.tcolor = color;
    return 0;
}

void set_ani_dimension(AnimationState &animation, xpp::TypedFields &f, const std::string &x1, const std::string &y1, const std::string &x2, const std::string &y2)
{
    const double xx1 = f.real("dimension", x1);
    const double xx2 = f.real("dimension", x2);
    const double yy1 = f.real("dimension", y1);
    const double yy2 = f.real("dimension", y2);
    if (f.error()) return;

    if ((xx1 < xx2) && (yy1 < yy2)) {
        animation.xlo = xx1;
        animation.xhi = xx2;
        animation.ylo = yy1;
        animation.yhi = yy2;
    }
}

int add_ani_com(xpp::Session &s, AnimationState &animation, xpp::TypedFields &f, int type, const std::string &x1, std::string &y1, const std::string &x2, const std::string &y2,
                std::string &col, const std::string &thick)
{
    int err = 0;
    if (type == COMNT || type == DIMENSION || type == PERMANENT || type == TRANSIENT || type == END || type == SPEED)
        return 1;
    AniCom a;
    a.type = type;
    a.flag = animation.aniflag;
    switch (type) {
    case AXNULL:
    case AYNULL:
        err = add_ani_null(s, a, x1, y1, x2, y2, col, thick);
        break;
    case COMET:
        err = add_ani_comet(s, f, a, x1, y1, x2, col, thick);
        break;
    case LINE:
    case RECT:
    case FRECT:
    case ELLIP:
    case FELLIP:
        err = add_ani_line(s, f, a, x1, y1, x2, y2, col, thick);
        break;
    case RLINE:
        err = add_ani_rline(s, f, a, x1, y1, col, thick);
        break;
    case CIRC:
    case FCIRC:
        err = add_ani_circle(s, f, a, x1, y1, x2, col, thick);
        break;
    case TEXT:
        err = add_ani_text(s, a, x1, y1, nullptr, y2);
        break;
    case VTEXT:
        err = add_ani_text(s, a, x1, y1, &x2, y2);
        break;
    case SETTEXT:
        err = add_ani_settext(f, a, x1, y1, col);
        break;
    }
    if (f.error()) err = -1;
    if (err < 0) return -1;
    animation.commands.push_back(std::move(a));
    animation.ncom = static_cast<int>(animation.commands.size());
    return 1;
}

void init_ani_stuff(AnimationState &animation)
{
    animation.xlo = 0.0;
    animation.ylo = 0.0;
    animation.xhi = 1.0;
    animation.yhi = 1.0;
    animation.aniflag = TRANSIENT;
    animation.commands.clear();
    animation.ncom = 0;
    animation.lastx = 0.0;
    animation.lasty = 0.0;
    animation.vcr.pos = 0;
    animation.grab_flag = 0; /*********** GRABBER *******************/
    animation.grabs.clear();
}

/*************************  GRABBER CODE *****************************/

const Model::Name *grab_target(const Model &m, std::string_view name)
{
    const auto *entry = m.name(name);
    if (!entry || !(entry->kind == Model::NameKind::Variable || entry->kind == Model::NameKind::Markov || entry->kind == Model::NameKind::Param)) return nullptr;
    return entry;
}

int add_grab_task(xpp::Session &s, xpp::TypedFields &f, const std::string &lhs, const std::string &rhs, AniGrab &g, int which)
{
    if (which != 1 && which != 2) return (-1);
    GrabTask &task = which == 1 ? g.start : g.end;
    if (which == 2 && lhs.compare(0, 6, "runnow") == 0) {
        task.runnow = f.integer("runnow", rhs);
        return f.error() ? -1 : 1;
    }
    if (static_cast<int>(task.events.size()) >= MAX_GEVENTS) {
        f.fail("grab", xpp::format("a grabbable changes at most {} variables", MAX_GEVENTS));
        return (-1);
    }
    if (!grab_target(s.model(), lhs)) {
        f.fail("grab", xpp::format("no variable or parameter named \"{}\"", lhs));
        return -1;
    }
    GrabEvent e;
    e.lhs = lhs;
    if (!compile_expr(s, rhs.c_str(), e.rhs)) {
        xpp::log(XPP_LOG_WARN, "Bad right-hand side for grab event {}\n", rhs);
        return (-1);
    }
    task.events.push_back(std::move(e));
    return (1);
}

int ani_grab_tasks(xpp::Session &s, xpp::TypedFields &f, const std::string &line, AniGrab &g, int which)
{
    std::string form, lhs;
    for (const char c : line) {
        if (c == '{' || c == ' ') continue;
        if (c == ';' || c == '}') {
            if (add_grab_task(s, f, lhs, form, g, which) < 0) return (-1);
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
int add_grab_command(xpp::Session &s, AnimationState &animation, xpp::TypedFields &f, const std::string &xs, const std::string &ys, const std::string &ts, xpp::LineReader &fp, int &lines, Place &place)
{
    if (static_cast<int>(animation.grabs.size()) >= MAX_ANI_GRAB) {
        f.fail("grab", xpp::format("an animation has at most {} grabbables", MAX_ANI_GRAB));
        return (-1);
    }
    AniGrab g;
    double z = f.real("tolerance", ts);
    if (f.error()) return (-1);
    if (z <= 0.0) z = GRAB_TOLERANCE;
    g.tol = z;
    if (!compile_expr(s, xs.c_str(), g.x)) {
        xpp::log(XPP_LOG_WARN, "Bad grab x {} \n", xs);
        return (-1);
    }
    if (!compile_expr(s, ys.c_str(), g.y)) {
        xpp::log(XPP_LOG_WARN, "Bad grab y {} \n", ys);
        return (-1);
    }
    for (int which : {1, 2}) {
        bool eof = false;
        const std::string tasks = read_ani_line(fp, eof, lines, place);
        if (eof) {
            f.fail("grab", "a grab needs both start and end task lines");
            return -1;
        }
        if (ani_grab_tasks(s, f, tasks, g, which) < 0) return -1;
    }
    animation.grabs.push_back(std::move(g));
    return (1);
}

int search_for_grab(xpp::Session &s, double x, double y)
{
    double dmin = 100000000;
    int imin = -1;
    for (int i = 0; i < static_cast<int>(s.animation.grabs.size()); i++) {
        const double u = s.animation.grabs[i].zx;
        const double v = s.animation.grabs[i].zy;
        const double d = std::sqrt((x - u) * (x - u) + (y - v) * (y - v));
        if ((d < dmin) && (d < s.animation.grabs[i].tol)) {
            dmin = d;
            imin = i;
        }
    }
    return (imin);
}

void do_grab_tasks(xpp::Session &s, int which) /* which=1 for start, 2 for end */
{
    const int i = s.animation.who_was_grabbed;
    if (i < 0 || i >= static_cast<int>(s.animation.grabs.size())) return; /*  no legal grab point */
    if (which != 1 && which != 2) return;
    for (GrabEvent &e : (which == 1 ? s.animation.grabs[i].start : s.animation.grabs[i].end).events)
        xpp::set_val(s,e.lhs.c_str(), xpp::evaluate(s,e.rhs.data()));
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
int parse_ani_string(xpp::Session &s, AnimationState &animation, xpp::TypedFields &f, std::string &text, xpp::LineReader &fp, int &lines, Place &place)
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
        return add_grab_command(s, animation, f, x1, x2, x3, fp, lines, place);
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
        animation.speed = f.integer("speed", std::string(*nxt), 0, MAX_ANI_SPEED);
        return f.error() ? -1 : 1;
    }
    case DIMENSION:
        ok = read_args(tokens, {{&x1, ";", false}, {&x2, ";", false}, {&x3, ";", false}, {&x4, ";\n", false}});
        break;
    }
    if (!ok) return -1;

    if (type == END) return 0;
    if (type == TRANSIENT) {
        animation.aniflag = TRANSIENT;
        return 1;
    }
    if (type == COMNT) return 1;
    if (type == PERMANENT) {
        animation.aniflag = PERMANENT;
        return 1;
    }

    if (type == DIMENSION) {
        set_ani_dimension(animation, f, x1, x2, x3, x4);
        return f.error() ? -1 : 1;
    }
    return (add_ani_com(s, animation, f, type, x1, x2, x3, x4, col, thick));
}

/* A parsed animation must fit the same memory budget as its saved session.
   Count each appended command once, including bytecode and trail buffers. */
bool animation_memory(std::size_t &used, const AnimationState &animation,
                      std::size_t commands_before, std::size_t grabs_before)
{
    for (std::size_t i = commands_before; i < animation.commands.size(); ++i) {
        const auto &a = animation.commands[i];
        if (!snapx::account_memory(used, 1, sizeof(AniCom) + a.text.size()) ||
            !snapx::account_memory(used, a.c.n, 2 * sizeof(double) + sizeof(Color))) return false;
        for (const auto *program : {&a.x1, &a.y1, &a.x2, &a.y2, &a.who, &a.col_expr})
            if (!snapx::account_memory(used, program->size(), sizeof(int))) return false;
    }
    for (std::size_t i = grabs_before; i < animation.grabs.size(); ++i) {
        const auto &grab = animation.grabs[i];
        if (!snapx::account_memory(used, 1, sizeof(AniGrab)) ||
            !snapx::account_memory(used, grab.x.size() + grab.y.size(), sizeof(int))) return false;
        for (const auto *task : {&grab.start, &grab.end})
            for (const auto &event : task->events)
                if (!snapx::account_memory(used, 1, sizeof(GrabEvent) + event.lhs.size()) ||
                    !snapx::account_memory(used, event.rhs.size(), sizeof(int))) return false;
    }
    return true;
}

/* Parse into a new value. No Session animation or displayed frame changes
   until the whole read succeeds, including the extra lines of each grab. */
xpp::Result<AnimationState> load_ani_file(xpp::Session &s, xpp::LineReader &fp, const char *filename)
{
    /* Keep window/options/runtime settings; only the parsed contents reset. */
    AnimationState animation = s.animation;
    init_ani_stuff(animation);
    animation.vcr.file = filename;
    animation.vcr.ok = 1;
    int lines = 0, commands = 0;
    std::size_t memory = 0;
    bool eof = false;
    for (;;) {
        Place place{filename, lines + 1};
        std::string old = read_ani_line(fp, eof, lines, place);
        if (eof && xpp::trim_blanks(old).empty()) break;
        std::string expanded, big;
        int first, last, flag;
        xpp::TypedFields fields(s);
        if (!xpp::search_array(old.data(), expanded, &first, &last, &flag, nullptr, &fields))
            return xpp::fail("animation", "Invalid animation array range", place);
        if (fields.error()) return xpp::fail("animation", *fields.error(), place);
        const auto count = static_cast<long long>(last) - first + 1;
        if (count <= 0 || count > odex::max_range_length - commands)
            return xpp::fail("animation", xpp::format("animation expansion must fit within {} commands", odex::max_range_length), place);
        commands += static_cast<int>(count);
        const Place command = place;
        for (int offset = 0; offset < count; ++offset) {
            place = command;
            xpp::subsk(expanded, big, first + offset, flag, &fields);
            if (fields.error()) return xpp::fail("animation", *fields.error(), place);
            const auto before = animation.commands.size(), grabs_before = animation.grabs.size();
            const int ans = parse_ani_string(s, animation, fields, big, fp, lines, place);
            if (flag && animation.commands.size() > before)
                animation.commands.back().array = {command.line, "j", first + offset, first, last, 1, false, {}};
            if (fields.error()) return xpp::fail("animation", *fields.error(), place);
            if (ans < 0) return xpp::fail("animation", "This line is not an animation command", place);
            if (!animation_memory(memory, animation, before, grabs_before))
                return fail("animation", "animation exceeds the session memory budget", place);
            if (ans == 0) return animation;
        }
        if (eof) break;
    }
    return animation;
}

/* the .ani filename: --anifile's is one of the model's files (model_file,
   model_files.h), one picked in the animation window a file of the disk */
xpp::Result<> ani_new_file(xpp::Session &s, const char *filename, bool model_file)
{
    if (model_file) {
        const xpp::Result<> allowed = xpp::approve_model_path(s.model(), filename, xpp::Place{});
        if (!allowed) return std::unexpected(allowed.error());
    }
    xpp::LineReader fp;
    if (model_file) fp = xpp::model_file_lines(s.model(), filename);
    else {
        std::string bytes;
        UniqueFile input(files::open_read_within(filename, files::split_path(filename).first, {}));
        if (!input || !xpp::read_bytes(input.get(), bytes, xpp::zip::archive_bytes_limit))
            return xpp::fail("animation", xpp::format("cannot be read in full within the {} byte input limit", xpp::zip::archive_bytes_limit), Place{filename});
        fp = xpp::LineReader::of_text(std::move(bytes));
    }
    if (!fp) {
        return std::unexpected(xpp::files::open_error("animation", filename));
    }
    std::string text;
    while (auto line = fp.next()) { text += *line; text += '\n'; }
    std::string native_file = filename;
    if (files::has_extension(filename, ".ani")) {
        auto converted = convert_animation(s, text, filename);
        if (!converted) return std::unexpected(converted.error());
        native_file = include_path(filename, files::output_name(filename, ".anix"));
        if (files::serving_reads()) {
            /* A recording supplies the foreign input itself. Conversion is an
               import in memory; replay never creates an unrecorded disk file. */
            text = std::move(*converted);
        } else {
            auto written = odex::write_converted(filename, native_file, *converted);
            if (!written) return std::unexpected(written.error());
            if (!read_bytes(native_file, text, zip::archive_bytes_limit)) return std::unexpected(files::open_error("animation", native_file));
        }
    }
    auto animation = read_animation(s, text, native_file);
    if (!animation) return std::unexpected(animation.error());
    if (!model_file || native_file != filename) {
        const auto recorded = remember_model_file(s.model(), native_file, text);
        if (!recorded) return std::unexpected(recorded.error());
    }
    s.animation = std::move(*animation);
    s.animation.options.use_file = 1;
    s.animation.options.file = native_file;
    if (native_file != filename)
        std::erase_if(s.model().files, [&](const ModelFile &f) { return f.name == filename; });
    ani_data_forget(s);
    return {};
}

void ani_frame(xpp::Session &s, int task)
{
    if (task == 1) {
        set_ani_perm(s);
        reset_comets(s);
        return;
    }
    render_ani(s);
    ui.ani_show();
}

void set_to_init_data(xpp::Session &s)
{
    int i;
    for (i = 0; i < s.model().node; i++) s.last_ic[i] = getvar(s,i + 1);
    for (i = s.model().node + s.model().fix_var; i < s.model().node + s.model().fix_var + s.model().nmarkov; i++) s.last_ic[i - s.model().fix_var] = getvar(s,i + 1);
    redraw_ics();
}

/* the animation's fixed variables at time t from a state: value(i) is the
   i-th of the node + nmarkov variables */
namespace {
template <class Value>
void set_fix_rhs_from(xpp::Session &s, double t, Value value)
{
    std::vector<double> y(s.model().node + s.model().nmarkov);
    for (size_t i = 0; i < y.size(); i++) y[i] = value(i);
    set_fix_rhs(s, t, y.data());
}
}

void set_from_init_data(xpp::Session &s)
{
    set_fix_rhs_from(s, s.numerics.start_time, [&](size_t i) { return s.last_ic[i]; });
}

void ani_disk_warn(xpp::Session &s)
{
    unsigned int total = (s.browser.view.maxrow * s.animation.vcr.wid * s.animation.vcr.hgt * 3) / (s.animation.mpeg.skip * s.animation.vcr.inc);
    total = total / (1024 * 1024);
    if (total > 10) {
        const std::string q = xpp::format(" {} Mb disk space needed! Continue?", total);
        const char ans = static_cast<char>(TwoChoice("YES", "NO", q, "yn"));
        if (ans != 'y') s.animation.mpeg.flag = 0;
    }
}

void eval_ani_color(xpp::Session &s, int j)
{
    AniCom &a = s.animation.commands[j];
    if (!a.col_expr.empty()) {
        double z = xpp::evaluate(s,a.col_expr.data());
        if (z > 1) z = 1.0;
        if (z < 0) z = 0.0;
        a.zcol = z;
    }
}

void eval_ani_com(xpp::Session &s, int j)
{
    AniCom &a = s.animation.commands[j];
    a.zx1 = xpp::evaluate(s,a.x1.data());
    a.zy1 = xpp::evaluate(s,a.y1.data());

    switch (a.type) {
    case LINE:
    case RECT:
    case FRECT:
    case ELLIP:
    case FELLIP:
    case AXNULL:
    case AYNULL:
        a.zx2 = xpp::evaluate(s,a.x2.data());
        a.zy2 = xpp::evaluate(s,a.y2.data());
        break;
    case CIRC:
    case FCIRC:
        a.zrad = xpp::evaluate(s,a.x2.data());
        break;
    case VTEXT:
        a.zval = xpp::evaluate(s,a.x2.data());
        break;
    }

    if (a.type == AXNULL || a.type == AYNULL) a.zval = xpp::evaluate(s,a.who.data());
}

void set_ani_font_stuff(xpp::Session &s, int size, int font, const xpp::Color &color) { pen_font(s, size, font, color); }

void set_ani_col(xpp::Session &s, int j)
{
    const AniCom &a=s.animation.commands[j];
    const xpp::Color color=a.col_expr.empty()?a.color:xpp::ramp_color(s.ramp(),a.zcol);
    pen_color(s, color);
    s.animation.last_color = color;
    s.animation.pen.dash = a.dash;
}

/* the comet keeps its last n positions (in the animation's coordinates)
   with their colours: filled circles of -thickness pixels, or lines */
void draw_ani_comet(xpp::Session &s, int j)
{
    AniCom &a = s.animation.commands[j];
    pen_thick(s, a.zthick);
    set_ani_col(s, j);
    roll_comet(a, a.zx1, a.zy1, s.animation.last_color);
    const int nn = a.c.i;
    if (a.zthick < 0) {
        const int ir = static_cast<int>(-a.zthick);
        for (int k = 0; k < nn; k++) {
            pen_color(s, a.c.col[k]);
            put_dot(s,a.c.x[k], a.c.y[k], ir);
        }
    } else {
        if (nn > 2) {
            for (int k = 1; k < nn; k++) {
                pen_color(s, a.c.col[k]);
                put_line(s,a.c.x[k - 1], a.c.y[k - 1], a.c.x[k], a.c.y[k]);
            }
        }
    }
}

/* a nullcline of the phase plane, its box [x1,x2] x [y1,y2] mapped to the
   animation's [0,1] x [0,1] (as XPP always has, whatever `dimension` says) */
void draw_ani_null(xpp::Session &s, int j, int id)
{
    const AniCom &a = s.animation.commands[j];
    double xl = a.zx1, xh = a.zx2, yl = a.zy1, yh = a.zy2;
    float *v;
    int n;
    float dx = xh - xl, dy = yh - yl;
    if (dx == 0.0 || dy == 0.0) return;

    set_ani_col(s, j);
    if (a.null_width) pen_thick(s, *a.null_width);
    const int who = static_cast<int>(a.zval); /* the nullcline that you want  -1 is the default cline */
    if (get_nullcline_floats(s, &v, &n, who, id) == 1) return;
    for (int i = 0; i < n; i++) {
        const int i4 = 4 * i;
        const float x1 = (v[i4] - xl) / dx;
        const float y1 = (v[i4 + 1] - yl) / dy;
        const float x2 = (v[i4 + 2] - xl) / dx;
        const float y2 = (v[i4 + 3] - yl) / dy;
        put_line(s,x1, y1, x2, y2);
    }
}

void draw_ani_line(xpp::Session &s, int j)
{
    const AniCom &a = s.animation.commands[j];
    double x1 = a.zx1, x2 = a.zx2, y1 = a.zy1, y2 = a.zy2;
    pen_thick(s, a.zthick);
    set_ani_col(s, j);
    put_line(s,x1, y1, x2, y2);
    s.animation.lastx = x2;
    s.animation.lasty = y2;
}

void draw_ani_rline(xpp::Session &s, int j)
{
    const AniCom &a = s.animation.commands[j];
    double x1 = s.animation.lastx + a.zx1, y1 = s.animation.lasty + a.zy1;
    pen_thick(s, a.zthick);
    set_ani_col(s, j);
    put_line(s,s.animation.lastx, s.animation.lasty, x1, y1);
    s.animation.lastx = x1;
    s.animation.lasty = y1;
}

/* rect, ellip and circle, outlined or filled (frect, fellip, fcircle) */
void draw_ani_shape(xpp::Session &s, int j)
{
    const AniCom &a = s.animation.commands[j];
    pen_thick(s, a.zthick);
    set_ani_col(s, j);
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

void draw_ani_text(xpp::Session &s, int j) { put_text(s,s.animation.commands[j].zx1, s.animation.commands[j].zy1, s.animation.commands[j].text.c_str()); }

void draw_ani_vtext(xpp::Session &s, int j)
{
    put_text(s,s.animation.commands[j].zx1, s.animation.commands[j].zy1, xpp::format("{}{:g}", s.animation.commands[j].text, s.animation.commands[j].zval).c_str());
}

/* Draw little black x's where the grab points are */
void draw_grab_points(xpp::Session &s)
{
    pen_color(s, xpp::foreground_color);
    for (AniGrab &g : s.animation.grabs) {
        const double xc = xpp::evaluate(s,g.x.data());
        const double yc = xpp::evaluate(s,g.y.data());
        g.zx = xc;
        g.zy = yc;
        const double z = g.tol;
        put_line(s,xc + z, yc + z, xc - z, yc - z);
        put_line(s,xc + z, yc - z, xc - z, yc + z);
    }
    s.animation.show_grab_points = 0;
}

} // namespace

namespace {

std::string animation_model_name(setx::Block &block)
{
    std::string model;
    block.text("model", model);
    if (!files::name_ok(model) || !odex::is_odex(model)) block.fail("model must be an .odex base name");
    return model;
}

/* A shape's formulas and literal fields are consumed separately; neither may
   leave an unknown key behind. The settings reader supplies all typed literals. */
class AnimationFields {
public:
    AnimationFields(Session &s, const odex::Statement &st, const setx::Value &fields,
                    const setx::Document &doc)
        : s_(s), st_(st), doc_(doc), fields_(doc, fields, false, st.name), used_(st.bindings.size(), false) {}
    setx::Block &literals() { return fields_; }
    const odex::Expr *expression(std::string_view key, bool required = true)
    {
        for (std::size_t i = 0; i < st_.bindings.size(); ++i)
            if (st_.bindings[i].name == key) { used_[i] = true; return &st_.bindings[i].value; }
        if (fields_.has(key)) bad(key, xpp::format("{} must be an expression", key));
        if (required) fail(st_.pos, xpp::format("{} needs the expression {}", st_.name, key));
        return nullptr;
    }
    void compile(std::string_view key, std::vector<int> &out)
    {
        const auto &e = *expression(key);
        const std::string text = odex::engine_text(e);
        if (!compile_expr(s_, text.c_str(), out)) fail(e.pos, xpp::format("cannot compile {}: {}", key, odex::print(e)));
    }
    void color(std::string_view key, AniCom &a)
    {
        if (fields_.has(key)) { fields_.color(key, a.color); return; }
        if (const auto *e = expression(key, false)) {
            const std::string text = odex::engine_text(*e);
            if (!compile_expr(s_, text.c_str(), a.col_expr)) fail(e->pos, "cannot compile the colour expression");
        }
    }
    bool has(std::string_view key) const
    {
        if (fields_.has(key)) return true;
        for (const auto &b : st_.bindings) if (b.name == key) return true;
        return false;
    }
    [[noreturn]] void bad(std::string_view key, std::string what)
    {
        if (const auto *m = fields_.member(key)) fields_.fail(*m, std::move(what));
        for (const auto &b : st_.bindings) if (b.name == key) fail(b.pos, std::move(what));
        fail(st_.pos, std::move(what));
    }
    void done()
    {
        fields_.done();
        for (std::size_t i = 0; i < used_.size(); ++i)
            if (!used_[i]) fail(st_.bindings[i].pos, xpp::format("{} has no expression field {}", st_.name, st_.bindings[i].name));
    }
    [[noreturn]] void fail(odex::Pos p, std::string what) const { doc_.fail(p.line, p.col, std::move(what)); }
private:
    Session &s_;
    const odex::Statement &st_;
    const setx::Document &doc_;
    setx::Block fields_;
    std::vector<bool> used_;
};

void native_shape(Session &s, AnimationState &animation, const odex::Statement &st,
                  const odex::AnimationStatement &written, const setx::Document &doc)
{
    AnimationFields f(s, st, written.fields, doc);
    auto &l = f.literals();
    AniCom a;
    a.flag = written.permanent ? PERMANENT : TRANSIENT;
    const auto &name = st.name;
    if (name == "line" || name == "rectangle" || name == "ellipse" || name == "circle" || name == "trail" || name == "nullcline") {
        f.color("color", a);
        l.real("width", a.zthick);
        if (a.zthick < 0) f.bad("width", "width must be 0 or more");
        std::string dash = "solid";
        l.word("dash", dash);
        const auto parsed = parse_line_dash(dash);
        if (!parsed) f.bad("dash", "dash must be solid, dashed, dotted or dash_dot");
        a.dash = *parsed;
    }
    if (name == "line" || name == "rectangle" || name == "nullcline") {
        f.compile("x1", a.x1); f.compile("y1", a.y1);
        bool relative = false;
        if (name == "line") l.flag("relative", relative);
        if (!relative) { f.compile("x2", a.x2); f.compile("y2", a.y2); }
        a.type = name == "line" ? (relative ? RLINE : LINE) : RECT;
        if (name == "nullcline") {
            if (l.has("width")) a.null_width = a.zthick;
            std::string variable;
            if (!l.text("variable", variable)) l.fail("nullcline needs variable in double quotes");
            const auto *entry = s.model().name(variable);
            if (variable != "t" && (!entry || entry->written != variable)) f.bad("variable", xpp::format("nullcline variable names no model quantity: \"{}\"", variable));
            int ix = -1;
            find_variable(s, variable, &ix);
            const auto &g = s.plot_windows.graph[s.plot_windows.active];
            if (ix == g.xv[0]) a.type = AXNULL;
            else if (ix == g.yv[0]) a.type = AYNULL;
            else f.bad("variable", xpp::format("nullcline variable \"{}\" is not an axis of the phase plane", variable));
            std::string source = "current";
            l.word("source", source);
            if (source == "current") compile_expr(s, "-1", a.who);
            else if (source == "frozen") f.compile("frame", a.who);
            else f.bad("source", "nullcline source must be current or frozen");
        }
    } else if (name == "circle" || name == "ellipse" || name == "trail" || name == "text" || name == "value" || name == "grab") {
        f.compile("x", a.x1); f.compile("y", a.y1);
        if (name == "circle") { a.type = CIRC; f.compile("radius", a.x2); }
        if (name == "ellipse") { a.type = ELLIP; f.compile("radius_x", a.x2); f.compile("radius_y", a.y2); }
        if (name == "trail") {
            a.type = COMET;
            if (!l.whole("length", a.c.n) || a.c.n < 1 || a.c.n > MAX_COMET_POINTS)
                f.bad("length", xpp::format("trail length must be 1..{}", MAX_COMET_POINTS));
            std::string draw = "line";
            l.word("draw", draw);
            if (draw == "points") {
                int radius = default_trail_radius;
                l.whole("radius_pixels", radius);
                if (radius <= 0) f.bad("radius_pixels", "radius_pixels must be above 0");
                a.zthick = -radius;
            } else if (draw != "line") f.bad("draw", "trail draw must be line or points");
            a.c.x.assign(a.c.n, 0); a.c.y.assign(a.c.n, 0); a.c.col.assign(a.c.n, foreground_color);
        }
        if (name == "text" || name == "value") {
            a.type = name == "text" ? TEXT : VTEXT;
            a.own_text_style = true;
            const auto label = name == "text" ? "text" : "label";
            if (!l.text(label, a.text)) l.fail(xpp::format("{} needs {} in double quotes", name, label));
            if (name == "value") f.compile("value", a.x2);
            l.whole("size", a.tsize);
            if (a.tsize < 0 || a.tsize > ANI_TEXT_SIZES) f.bad("size", "text size must be 0..4");
            std::string font(ani_font_names[a.tfont]);
            l.word("font", font);
            const auto selected = std::ranges::find(ani_font_names, font);
            if (selected == ani_font_names.end()) f.bad("font", "unknown animation font: " + font);
            a.tfont = static_cast<int>(selected - ani_font_names.begin());
            f.color("color", a);
            a.tcolor = a.color;
        }
        if (name == "grab") {
            if (animation.grabs.size() >= MAX_ANI_GRAB) l.fail("too many grabbable points");
            AniGrab grab;
            grab.x = std::move(a.x1); grab.y = std::move(a.y1);
            grab.tol = GRAB_TOLERANCE;
            l.real("tolerance", grab.tol);
            if (!(grab.tol > 0)) f.bad("tolerance", "grab tolerance must be above 0");
            bool run = false;
            l.flag("run", run);
            grab.end.runnow = run;
            for (const auto &b : st.bindings) {
                const bool start = b.name.starts_with("start.");
                if (!start && !b.name.starts_with("end.")) continue;
                auto &task = start ? grab.start : grab.end;
                if (task.events.size() >= MAX_GEVENTS) f.fail(b.pos, "too many assignments in a grab task");
                GrabEvent event;
                event.lhs = b.name.substr(start ? 6 : 4);
                const auto *entry = grab_target(s.model(), event.lhs);
                if (!entry || entry->written != event.lhs)
                    f.fail(b.pos, xpp::format("grab assignment names no variable or parameter \"{}\"", event.lhs));
                f.compile(b.name, event.rhs);
                task.events.push_back(std::move(event));
            }
            f.done();
            animation.grabs.push_back(std::move(grab));
            return;
        }
    } else {
        f.fail(st.pos, xpp::format("unknown animation statement {}", name));
    }
    if (name == "circle" || name == "rectangle" || name == "ellipse") {
        if (f.has("fill")) {
            if (f.has("color")) l.fail("a filled shape takes its colour from fill; omit color");
            f.color("fill", a);
            a.type = name == "circle" ? FCIRC : name == "rectangle" ? FRECT : FELLIP;
        }
    }
    f.done();
    animation.commands.push_back(std::move(a));
    animation.ncom = static_cast<int>(animation.commands.size());
}

} // namespace

Result<AnimationState> read_animation(Session &s, std::string_view text, const std::string &file)
{
    if (files::has_extension(file, ".ani")) {
        auto lines = LineReader::of_text(std::string(text));
        return load_ani_file(s, lines, file.c_str());
    }
    if (!files::has_extension(file, ".anix")) return fail_reading("animation", "expected an .anix animation", file);
    const setx::Document doc("animation", file, std::string(text));
    try {
        AnimationState animation = s.animation;
        init_ani_stuff(animation);
        animation.speed = on_the_fly_speed;
        animation.vcr.file = file;
        animation.vcr.ok = 1;
        bool model_seen = false, options_seen = false;
        std::size_t memory = 0;
        for (const auto &written : odex::parse_animation(text, file)) {
            const auto &st = written.expressions;
            if (st.name == "model" || st.name == "options") {
                setx::Block block(doc, written.fields, false, st.name);
                if (written.permanent) block.fail("model and options belong outside permanent");
                if (st.name == "model") {
                    if (model_seen) block.fail("model is written once");
                    model_seen = true;
                    const std::string model = animation_model_name(block);
                    if (files::split_path(s.model().this_file).second != model)
                        block.fail(xpp::format("animation names model \"{}\", but \"{}\" is loaded", model, s.model().this_file));
                } else {
                    if (options_seen) block.fail("options is written once");
                    options_seen = true;
                    block.whole("speed", animation.speed);
                    if (animation.speed < 0 || animation.speed > MAX_ANI_SPEED) block.fail(*block.member("speed"), "speed must be 0..1000");
                    if (auto bounds = block.group("bounds")) {
                        std::array<double, 2> x{0, 1}, y{0, 1};
                        if (!bounds->reals("x", std::span<double>(x)) || !bounds->reals("y", std::span<double>(y))) bounds->fail("bounds needs x and y pairs");
                        if (!(x[0] < x[1])) bounds->fail(*bounds->member("x"), "bounds must increase on x");
                        if (!(y[0] < y[1])) bounds->fail(*bounds->member("y"), "bounds must increase on y");
                        animation.xlo = x[0]; animation.xhi = x[1]; animation.ylo = y[0]; animation.yhi = y[1];
                        bounds->done();
                    }
                }
                block.done();
                continue;
            }
            odex::Parsed formulas;
            formulas.files.push_back(file);
            formulas.statements.push_back(st);
            auto expanded = odex::animation_formulas(s.model(), formulas);
            if (expanded.size() > static_cast<std::size_t>(odex::max_range_length - animation.ncom))
                doc.fail(st.pos.line, st.pos.col, "animation expansion exceeds the shared range limit");
            for (const auto &shape : expanded) {
                const auto before = animation.commands.size(), grabs_before = animation.grabs.size();
                native_shape(s, animation, shape, written, doc);
                if (!animation_memory(memory, animation, before, grabs_before))
                    doc.fail(st.pos.line, st.pos.col, "animation exceeds the session memory budget");
            }
        }
        return animation;
    } catch (ReadFailed &e) { return std::unexpected(std::move(e.error)); }
      catch (Error &e) {
        e.where = "animation";
        e.place.source = line_place(file, text, e.place.line).source;
        return std::unexpected(std::move(e));
    }
}

Result<> load_animation(Session &s, const std::string &file)
{
    return ani_new_file(s, file.c_str(), true);
}

Result<std::string> animation_model(const std::string &file)
{
    std::string text;
    UniqueFile input(files::open_read_within(file, files::split_path(file).first, {}));
    if (!input || !read_bytes(input.get(), text, zip::archive_bytes_limit)) return std::unexpected(files::open_error("animation", file));
    const setx::Document doc("animation", file, text);
    try {
        std::string model;
        for (const auto &st : odex::parse_animation(text, file)) {
            if (st.expressions.name != "model") continue;
            if (!model.empty()) doc.fail(st.fields, "model is written once");
            setx::Block block(doc, st.fields, true, "model");
            model = animation_model_name(block);
            block.done();
        }
        if (model.empty()) doc.fail(1, 1, "opening an animation needs model: \"name.odex\"");
        return include_path(file, model);
    } catch (ReadFailed &e) { return std::unexpected(std::move(e.error)); }
      catch (Error &e) { e.where = "animation"; return std::unexpected(std::move(e)); }
}

Result<std::string> convert_animation(Session &s, std::string_view text, const std::string &file)
{
    auto lines = LineReader::of_text(std::string(text));
    auto imported = load_ani_file(s, lines, file.c_str());
    if (!imported) return std::unexpected(imported.error());
    try {
        const auto &animation = *imported;
        std::string out = "model: " + odex::quote_string(files::split_path(s.model().this_file).second) + "\n";
        out += xpp::format("options {{ speed: {}, bounds: {{ x: [{}, {}], y: [{}, {}] }} }}\n",
                           animation.speed, odex::print_number(animation.xlo), odex::print_number(animation.xhi),
                           odex::print_number(animation.ylo), odex::print_number(animation.yhi));
        auto expression = [&](const std::vector<int> &p) { return odex::print(odex::program_expression(s, p)); };
        auto color = [&](const AniCom &a) { return a.col_expr.empty() ? quoted_color(a.color) : expression(a.col_expr); };
        int size = s.animation.pen.size, font = s.animation.pen.font;
        std::string text_color = quoted_color(foreground_color);
        bool permanent = false;
        std::vector<std::string> range_texts;
        std::vector<int> range_values;
        odex::ArrayCopy range;
        auto flush = [&] {
            if (!range_texts.empty()) out += odex::print_range(s, range_texts, range_values, range);
            range_texts.clear(); range_values.clear(); range = {};
        };
        for (const auto &a : animation.commands) {
            if (a.array.group != range.group) flush();
            if (a.type == SETTEXT) {
                size = a.tsize; font = a.tfont; text_color = quoted_color(a.tcolor);
                continue;
            }
            if (permanent != (a.flag == PERMANENT)) {
                flush();
                if (permanent) out += "}\n";
                permanent = a.flag == PERMANENT;
                if (permanent) out += "permanent {\n";
            }
            const bool filled = a.type == FCIRC || a.type == FRECT || a.type == FELLIP;
            std::string shape, fields;
            auto field = [&](std::string_view key, std::string value) {
                if (!fields.empty()) fields += ", ";
                fields += std::string(key) + ": " + value;
            };
            auto point = [&] { field("x", expression(a.x1)); field("y", expression(a.y1)); };
            auto box = [&] { field("x1", expression(a.x1)); field("y1", expression(a.y1)); field("x2", expression(a.x2)); field("y2", expression(a.y2)); };
            switch (a.type) {
            case LINE: shape = "line"; box(); break;
            case RLINE: shape = "line"; field("x1", expression(a.x1)); field("y1", expression(a.y1)); field("relative", "true"); break;
            case RECT: case FRECT: shape = "rectangle"; box(); break;
            case CIRC: case FCIRC: shape = "circle"; point(); field("radius", expression(a.x2)); break;
            case ELLIP: case FELLIP: shape = "ellipse"; point(); field("radius_x", expression(a.x2)); field("radius_y", expression(a.y2)); break;
            case COMET:
                shape = "trail"; point(); field("length", std::to_string(a.c.n));
                if (a.zthick < 0) { field("draw", "points"); field("radius_pixels", odex::print_number(-a.zthick)); }
                break;
            case TEXT: case VTEXT:
                shape = a.type == TEXT ? "text" : "value"; point();
                field(a.type == TEXT ? "text" : "label", odex::quote_string(a.text));
                if (a.type == VTEXT) field("value", expression(a.x2));
                field("size", std::to_string(size)); field("font", std::string(ani_font_names[font])); field("color", text_color);
                break;
            case AXNULL: case AYNULL: {
                shape = "nullcline"; box();
                const auto &g = s.plot_windows.graph[s.plot_windows.active];
                const int variable = a.type == AXNULL ? g.xv[0] : g.yv[0];
                const auto *entry = variable == 0 ? nullptr : s.model().name(s.model().uvar_names[variable - 1]);
                field("variable", odex::quote_string(entry ? entry->written : "t"));
                field("source", "frozen"); field("frame", expression(a.who));
                break;
            }
            default: return fail_reading("animation", "cannot convert this animation command", file);
            }
            if (a.type != TEXT && a.type != VTEXT) {
                field(filled ? "fill" : "color", color(a));
                if (a.zthick >= 0 && a.type != AXNULL && a.type != AYNULL) field("width", odex::print_number(a.zthick));
                text_color = color(a);
            }
            const std::string statement = (permanent ? "  " : "") + shape + " { " + fields + " }\n";
            if (a.array.group) {
                range = a.array;
                range_texts.push_back(statement);
                range_values.push_back(a.array.value);
            } else out += statement;
        }
        flush();
        if (permanent) out += "}\n";
        for (const auto &grab : animation.grabs) {
            out += "grab { x: " + expression(grab.x) + ", y: " + expression(grab.y) + ", tolerance: " + odex::print_number(grab.tol);
            for (const bool start : {true, false}) {
                out += start ? ", start: { " : ", end: { ";
                bool first = true;
                for (const auto &event : (start ? grab.start : grab.end).events) {
                    if (!first) out += ", ";
                    const auto *name = s.model().name(event.lhs);
                    if (!name) return fail_reading("animation", "a grab assignment names no model quantity: " + event.lhs, file);
                    out += name->written + ": " + expression(event.rhs);
                    first = false;
                }
                out += " }";
            }
            out += xpp::format(", run: {} }}\n", grab.end.runnow ? "true" : "false");
        }
        const std::string native_file = include_path(file, files::output_name(file, ".anix"));
        const auto checked = read_animation(s, out, native_file);
        if (!checked) return std::unexpected(checked.error());
        return out;
    } catch (const Error &e) { return std::unexpected(e); }
      catch (const ReadFailed &e) { return std::unexpected(e.error); }
}

int convert_animation_file(const std::string &file, const std::string &model)
{
    const std::string model_file = model.empty() ? include_path(file, files::output_name(file, ".odex")) : model;
    auto line = parse_command_line({"xppautX", model_file, "--convert"});
    if (!line) { show_error(line.error()); return 1; }
    auto loaded = load_model(std::move(*line), 1);
    if (!loaded) return 1;
    std::string text;
    UniqueFile input(files::open_read_within(file, files::split_path(file).first, {}));
    if (!input || !read_bytes(input.get(), text, zip::archive_bytes_limit)) { show_error(files::open_error("animation", file)); return 1; }
    auto native = convert_animation(**loaded, text, file);
    if (!native) { show_error(native.error()); return 1; }
    const auto written = odex::write_converted(file, include_path(file, files::output_name(file, ".anix")), *native);
    if (!written) { show_error(written.error()); return 1; }
    return 0;
}

void update_ani_motion_stuff(xpp::Session &s, int x, int y)
{
    double dt;
    s.animation.motion.t2 = s.animation.motion.t1;
    s.animation.motion.t1 = get_current_time();
    s.animation.motion.ox = s.animation.motion.x;
    s.animation.motion.oy = s.animation.motion.y;
    ani_ij_to_xy(s,x, y, &s.animation.motion.x, &s.animation.motion.y);
    dt = s.animation.motion.t1 - s.animation.motion.t2;
    if (dt == 0.0) dt = 10000000000;
    s.animation.motion.vx = (s.animation.motion.x - s.animation.motion.ox) / dt;
    s.animation.motion.vy = (s.animation.motion.y - s.animation.motion.oy) / dt;
    xpp::set_val(s,"mouse_x", s.animation.motion.x);
    xpp::set_val(s,"mouse_y", s.animation.motion.y);
    xpp::set_val(s,"mouse_vx", s.animation.motion.vx);
    xpp::set_val(s,"mouse_vy", s.animation.motion.vy);
    do_grab_tasks(s, 1);
    fix_only(s);
    ani_frame(s,0);
}

/*************************** End motion & speed stuff   ****************/

void ani_create_mpeg(xpp::Session &s)
{
    static const char *const n[] = {"PPM 0/1", "AniGif(0/1)"};
    std::array<std::string, 2> values;
    s.animation.mpeg.flag = 0;
    values[0] = xpp::format("{:d}", s.animation.mpeg.flag);
    values[1] = xpp::format("{:d}", s.animation.mpeg.aviflag);
    static const int kinds[] = {XPP_FIELD_INTEGER, XPP_FIELD_INTEGER};
    const int status = do_string_box_of(2, 1, "Frame saving", n, values, kinds);
    if (status != 0) {
        xpp::TypedFields f(s);
        const int flag = f.integer(n[0], values[0]), aviflag = f.integer(n[1], values[1]);
        if (f.error()) {
            command_error("Frame saving", *f.error());
            return;
        }
        s.animation.mpeg.flag = flag > 0 ? 1 : flag;
        s.animation.mpeg.aviflag = aviflag;
        if(s.animation.mpeg.flag>0 && s.animation.mpeg.aviflag!=1){
            std::string root=xpp::files::output_name(s.model().this_file,".ppm","animation");
            if(!file_selector("Save animation frames",root,"*.ppm")){s.animation.mpeg.flag=0;return;}
            s.animation.mpeg.root=root;
        }
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
        xpp::TypedFields f(s);
        const int inc = f.integer("Increment", bob);
        if (f.error()) {
            command_error("Frame skip", *f.error());
            return;
        }
        s.animation.vcr.inc = inc <= 0 ? 1 : inc;
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
    if (s.animation.vcr.pos == 0) set_ani_perm(s);

    s.animation.vcr.pos = s.animation.vcr.pos + n;
    if (s.animation.vcr.pos >= s.browser.view.maxrow) s.animation.vcr.pos = s.browser.view.maxrow - 1;
    if (s.animation.vcr.pos < 0) s.animation.vcr.pos = 0;
    const int row = s.animation.vcr.pos;

    const double t = static_cast<double>(ss[0][row]);
    set_fix_rhs_from(s, t, [&](size_t i) { return static_cast<double>(ss[i + 1][row]); });

    render_ani(s);
    ui.ani_show();
}

void ani_zero(xpp::Session &s)
{
    s.animation.vcr.iexist = 0;
    s.animation.vcr.ok = !s.animation.commands.empty();
    s.animation.vcr.inc = 1;
    s.animation.vcr.pos = 0;
    s.animation.ncom = static_cast<int>(s.animation.commands.size());
    s.animation.aniflag = TRANSIENT;
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
    std::string file = fname ? fname : s.animation.vcr.file;
    if (fname == nullptr && file_selector("Load animation", file, "*.anix *.ani") == 0) return 0;
    if (!xpp::ok_or_show(ani_new_file(s, file.c_str(), false))) return 0;
    xpp::log(XPP_LOG_INFO, "Loaded {} lines successfully!\n", s.animation.ncom);
    return 1;
}

void reset_comets(xpp::Session &s)
{
    for (AniCom &a : s.animation.commands)
        if (a.type == COMET) a.c.i = 0;
}

void render_ani(xpp::Session &s)
{
    ui.ani_slider(s);
    s.animation.pen.color = xpp::foreground_color; /* each frame starts with the foreground */
    ani_data_begin(s);
    for (int i = 0; i < s.animation.ncom; i++) {
        const int type = s.animation.commands[i].type;
        const int flag = s.animation.commands[i].flag;
        if (flag == TRANSIENT) eval_ani_color(s, i);
        if (type != SETTEXT && flag == TRANSIENT) eval_ani_com(s, i);
        if ((type == TEXT || type == VTEXT) && s.animation.commands[i].own_text_style) {
            const auto &a = s.animation.commands[i];
            set_ani_font_stuff(s, a.tsize, a.tfont, a.col_expr.empty() ? a.tcolor : ramp_color(s.ramp(), a.zcol));
        }
        switch (type) {
        case AXNULL:
        case AYNULL:
            draw_ani_null(s,i, type - AXNULL);
            break;
        case SETTEXT:
            set_ani_font_stuff(s, s.animation.commands[i].tsize, s.animation.commands[i].tfont, s.animation.commands[i].tcolor);
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
    if (s.animation.show_grab_points == 1) draw_grab_points(s);
    {
        AniDataFrame f;
        f.pos = s.animation.vcr.pos;
        f.rows = s.browser.view.maxrow;
        f.t = getvar(s,0);
        f.speed = s.animation.speed;
        f.skip = s.animation.vcr.inc;
        f.xlo = s.animation.xlo;
        f.ylo = s.animation.ylo;
        f.xhi = s.animation.xhi;
        f.yhi = s.animation.yhi;
        f.w = s.animation.vcr.wid;
        f.h = s.animation.vcr.hgt;
        ani_data_end(s, &f);
    }
}

void set_ani_perm(xpp::Session &s)
{
    set_from_init_data(s);
    for (int i = 0; i < s.animation.ncom; i++) {
        if (s.animation.commands[i].flag == PERMANENT) {
            if (s.animation.commands[i].type != SETTEXT) eval_ani_com(s, i);
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
    s.animation.mpeg.root = xpp::files::output_name(s.model().this_file,".ppm","animation");
    s.animation.mpeg.skip = 1;
    s.animation.vcr.pos = 0;
    if (s.animation.ncom) set_ani_perm(s);
}

/* Grab: show the first frame with the grab points and wait for the mouse */
void ani_grab_start(xpp::Session &s)
{
    if (s.animation.grabs.empty()) return;
    if (s.animation.vcr.ok) {
        s.animation.vcr.pos = 0;

        s.animation.show_grab_points = 1;
        ani_frame(s,1);
        ani_frame(s,0);
        s.animation.grab_flag = 1;
    }
}

/* Reset: back to the first frame */
void ani_reset(xpp::Session &s)
{
    s.animation.vcr.pos = 0;
    reset_comets(s);
    ui.ani_slider(s);
    ani_flip1(s,0);
}

int run_now_grab(const xpp::Session &s)
{
    if (s.animation.who_was_grabbed < 0 || s.animation.who_was_grabbed >= static_cast<int>(s.animation.grabs.size())) return (0);
    return (s.animation.grabs[s.animation.who_was_grabbed].end.runnow);
}

/* the mouse went down (flag 1) or up (0) at pixel ix,iy while grabbing */
void ani_grab_mouse(xpp::Session &s, int flag, int ix, int iy)
{
    if (flag == 1) {
        s.animation.motion.t1 = get_current_time();
        ani_ij_to_xy(s,ix, iy, &s.animation.motion.x, &s.animation.motion.y);
        s.animation.who_was_grabbed = search_for_grab(s, s.animation.motion.x, s.animation.motion.y);
        if (s.animation.who_was_grabbed < 0) xpp::log(XPP_LOG_INFO, "Nothing grabbed\n");
    }
    if (flag == 0) { /* This is BUTTON RELEASE  */
        if (s.animation.who_was_grabbed < 0) return;
        do_grab_tasks(s, 2);
        set_to_init_data(s);
        s.animation.grab_flag = 0;
        redraw_params();
        if (run_now_grab(s)) {
            xpp::run_now(s);
            s.animation.grab_flag = 0;
        }
    }
}

} // namespace xpp
