#ifndef _aniparse_h_
#define _aniparse_h_

#include "xpplim.h"
#include "xpp_types.h"
#include "load_eqn.h"
#include <string>
#include <vector>

namespace xpp {
struct Session; /* session.h */

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

/* Frame saving (the ppm frames, or an animated gif) */
struct MPEG_SAVE {
    int flag = 0;
    int skip = 0;
    std::string root; /* the frames' base name */
    int aviflag = 0;
};

/* the animator (aniparse.cpp: the .ani language and its frames), a
   Session's (session.h): the number of commands of the loaded animation
   (ncom), its speed and grab state, its options, window and frame
   saving */
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

/* the pen the front end's callbacks keep between primitives */
struct AniPen {
    int color = 0; /* palette index: 0 black, 20..29 the named colours, 30.. the colour map */
    int thick = 0;
    int size = 0, font = 0;
};

struct AnimationState {
  int ncom = 0, speed = 10, grab_flag = 0;
  XppAniOptions options;
  VCR vcr;
  MPEG_SAVE mpeg;
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
  int last_color = 0;
  /* the .ani file's line being parsed */
  int lineno = 0;
  /* the `dimension` box, and where the last line ended (rline) */
  double xlo = 0, xhi = 1, ylo = 0, yhi = 1;
  double lastx = 0, lasty = 0;
  AniPen pen;
};

/* The animation window of the session s: its buttons, the mouse, a frame
   drawn (render_ani), the model's animation file loaded (get_ani_file: 1
   when one was), and the frames shown while integrating (on_the_fly) */
void update_ani_motion_stuff(Session &s, int x, int y);
void ani_create_mpeg(Session &s);
void ani_newskip(Session &s);
void on_the_fly(Session &s, int task);
void ani_view_created(Session &s);
void ani_grab_start(Session &s);
void ani_reset(Session &s);
void ani_grab_mouse(Session &s, int flag, int ix, int iy);
void ani_flip1(Session &s, int n);
void ani_zero(Session &s);
int get_ani_file(Session &s, const char *fname);
void render_ani(Session &s);
void set_ani_perm(Session &s);

} // namespace xpp
#endif
