#ifndef _aniparse_h_
#define _aniparse_h_


#include "xpplim.h"
#include "xpp_types.h"
#include "load_eqn.h"
#ifdef __cplusplus
#include <string>
extern "C" {
#endif


void reset_comets(void);


#ifdef __cplusplus
}

/* The animator's options: the -anifile to load at the start, and whether
   the animation follows an integration as it runs */
struct XppAniOptions {
    int use_file = 0;   /* -anifile was given */
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
struct AnimationState {
  int ncom = 0, speed = 10, grab_flag = 0;
  XppAniOptions options;
  VCR vcr;
  MPEG_SAVE mpeg;
};

namespace xpp {
struct Session; /* session.h */
}

/* The animation window of the session s: its buttons, the mouse, a frame
   drawn (render_ani), the model's animation file loaded (get_ani_file: 1
   when one was), and the frames shown while integrating (on_the_fly) */
void update_ani_motion_stuff(xpp::Session &s, int x, int y);
void ani_create_mpeg(xpp::Session &s);
void ani_newskip(xpp::Session &s);
void on_the_fly(xpp::Session &s, int task);
void ani_view_created(xpp::Session &s);
void ani_grab_start(xpp::Session &s);
void ani_reset(xpp::Session &s);
void ani_grab_mouse(xpp::Session &s, int flag, int ix, int iy);
void ani_flip1(xpp::Session &s, int n);
void ani_zero(xpp::Session &s);
int get_ani_file(xpp::Session &s, const char *fname);
void render_ani(xpp::Session &s);
void set_ani_perm(xpp::Session &s);
#endif
#endif
