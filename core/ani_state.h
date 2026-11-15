#pragma once
/* The animator's state as data (aniparse.h has its .ani language and frames, ani_data.h the frames as events): what a Session
   holds of the animation window, with the model's animation option and an .anix's own, so the Session, the options and the
   plot windows name it without including the animator. Split from aniparse.h at W737. */
#include <optional>
#include <string>
#include <vector>

#include "colormap.h"
#include "expr_program.h"
#include "line_style.h"
#include "odex.h"

namespace xpp {

/* The animator's options: the animation (the model's animation option or a .anix's own) to load at the start, and whether
   the animation follows an integration as it runs */
struct XppAniOptions {
    int use_file = 0;   /* an animation is named */
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
/* a comet's last n positions, in the animation's coordinates, and their colors */
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
    /* the color of the command: fixed (a named color, or none: the foreground), or the color of
       the color map at the value of the compiled expression (col_expr, empty when fixed) */
    Color color = foreground_color;
    Program col_expr;
    Program x1, y1, x2, y2, who;
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
    Program rhs;
};

struct GrabTask {
    std::vector<GrabEvent> events; /* at most MAX_GEVENTS */
    int runnow = 0;
};

struct AniGrab {
    double zx = 0, zy = 0, tol = 0;
    Program x, y;
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
  /* the color eval_ani_color found last: a comet's next point's */
  Color last_color = foreground_color;
  /* the `dimension` box, and where the last line ended (rline) */
  double xlo = 0, xhi = 1, ylo = 0, yhi = 1;
  double lastx = 0, lasty = 0;
  AniPen pen;
};

/* The frames drawn, as their events' text: a Session's (Session::ani_shown,
   its animation's), which ani_data.cpp fills; what the client got of them
   is ani_data.cpp's own, the client's. */
struct AniShown {
    std::string prims; /* the frame being drawn: its primitives, comma separated */
    std::string frame; /* the last frame finished, as its event; empty before any */
};

} // namespace xpp
