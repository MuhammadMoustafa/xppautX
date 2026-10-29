#pragma once

#include "xpp_types.h"
#include "load_eqn.h"
#include "odex.h"
#include "line_style.h"
#include <string>
#include <vector>

namespace xpp {
struct Session; /* session.h */
int run_now_grab(const Session &s);

void reset_comets(Session &s);

/* The animator's options: the --anifile to load at the start, and whether
   the animation follows an integration as it runs */
struct XppAniOptions {
    int use_file = 0;   /* --anifile was given */
    std::string file;   /* the .ani file it named */
    int on_the_fly = 0; /* animate while integrating */
};

/* the animation window: its size in the core's pixels, the row shown */
struct VCR {
    int hgt = 0, wid = 0, iexist = 0, ok = 0;
    int pos = 0, inc = 0;
    std::string file; /* the .ani file, or the folder to pick one in */
};

/* the animator (aniparse.cpp: the .ani language and its frames), a
   Session's (session.h): the number of commands of the loaded animation
   (ncom), its speed and grab state, its options, window and frame
   saving */
/* a comet's last n positions, in the animation's coordinates, and their colours */
struct Comet {
    int n = 0;
    std::vector<double> x, y;
    std::vector<Color> col;
    int i = 0;
};

/* one command of the .ani file: its compiled expressions (add_expr's
   tokens, each ending in ENDEXP) and their values in the current frame */
struct AniCom {
    Comet c;
    odex::ArrayCopy array; /* the legacy source range, for native conversion */
    int type = 0, flag = 0;
    LineDash dash = LineDash::solid;
    std::optional<double> null_width; /* absent preserves the imported nullcline pen width */
    bool own_text_style = false;
    /* the colour of the command: fixed (a named colour, or none: the foreground), or the colour of
       the colour map at the value of the compiled expression (col_expr, empty when fixed) */
    Color color = foreground_color;
    std::vector<int> col_expr;
    std::vector<int> x1, y1, x2, y2, who;
    std::string text; /* text and vtext's string */
    double zcol = 0, zx1 = 0, zy1 = 0, zx2 = 0, zy2 = 0, zrad = 0, zval = 0;
    double zthick = 0;
    int tfont = 0, tsize = 0;
    Color tcolor = foreground_color;
};

/**************  the Grabber ***************************/
constexpr int MAX_GEVENTS = 20; /* maximum variables you can change per grabbable */
constexpr int MAX_ANI_GRAB = 50; /* max grabbable objects  */
/* the most points a comet keeps: its tail is drawn every frame */
constexpr int MAX_COMET_POINTS = 10000;
/* the sizes a text command takes (0 to this), and the speeds (0 to this, in ms per frame of Go) */
constexpr int ANI_TEXT_SIZES = 4;
constexpr int MAX_ANI_SPEED = 1000;
/* a grab's tolerance when its file gives none above zero */
constexpr double GRAB_TOLERANCE = .02;

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

/* the pen the front end's callbacks keep between primitives */
struct AniPen {
    Color color = foreground_color;
    double thick = 0;
    LineDash dash = LineDash::solid;
    int size = 0, font = 0;
};

struct AnimationState {
  int ncom = 0, speed = 10, grab_flag = 0;
  XppAniOptions options;
  VCR vcr;
  /* the loaded .ani's commands (ncom of them while it is loaded) and
     what can be grabbed */
  std::vector<AniCom> commands;
  std::vector<AniGrab> grabs;
  int show_grab_points = 0, who_was_grabbed = 0;
  /* the mouse while grabbing */
  AniMotionInfo motion{};
  /* TRANSIENT or PERMANENT: the kind the next command parsed is */
  int aniflag = 0;
  /* the colour eval_ani_color found last: a comet's next point's */
  Color last_color = foreground_color;
  /* the `dimension` box, and where the last line ended (rline) */
  double xlo = 0, xhi = 1, ylo = 0, yhi = 1;
  double lastx = 0, lasty = 0;
  AniPen pen;
};

/* The animation window of the session s: its buttons, the mouse, a frame
   drawn (render_ani), an animation file chosen and loaded (get_ani_file), and the
   frames shown while integrating (on_the_fly) */
void update_ani_motion_stuff(Session &s, double x, double y);
void on_the_fly(Session &s, int task);
void ani_view_created(Session &s);
void ani_grab_start(Session &s);
void ani_reset(Session &s);
void ani_grab_mouse(Session &s, int flag, double x, double y);
void ani_flip1(Session &s, int n);
void ani_zero(Session &s);
/* animation.load's work: the animation file `file` loaded, and its first frame shown when there is data to show
   (`file` by value, like load_animation's) */
Result<> load_chosen_animation(Session &s, std::string file);
/* Go's frames also written: as numbered PPM files from `file`, or as one animated GIF `file` */
enum class AniExportKind { none, ppm, gif };
struct AniExport {
    AniExportKind kind = AniExportKind::none;
    std::string file;
};
/* the animation loaded and data stored to play, else why not (`where` names the command) */
Result<> animation_ready(const Session &s, const std::string &where);
/* Go from the frame shown to the last, also written when `out` says so: the frames shown, or why none were */
Result<int> play_animation(Session &s, const AniExport &out);
/* the frame of stored row `row` */
void ani_seek(Session &s, int row);
/* Native parsing and legacy import both return a whole value before applying it. */
Result<AnimationState> read_animation(Session &s, std::string_view text, const std::string &file);
/* the model's animation file `file` loaded; `file` by value, since a caller passes the animation's own
   name, which the load replaces */
Result<> load_animation(Session &s, std::string file);
Result<std::string> animation_model(const std::string &file);
Result<std::string> convert_animation(Session &s, std::string_view text, const std::string &file);
int convert_animation_file(const std::string &file, const std::string &model = {});
void render_ani(Session &s);
void set_ani_perm(Session &s);

} // namespace xpp
