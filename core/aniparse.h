#ifndef _aniparse_h_
#define _aniparse_h_


#include "xpplim.h"
#include "xpp_types.h"
#include "load_eqn.h"
#ifdef __cplusplus
#include <string>
extern "C" {
#endif

/* aniparse.cpp: the animator (the .ani language and its frames) */
extern int n_anicom, ani_speed, ani_speed_inc, ani_grab_flag;

void update_ani_motion_stuff(int x, int y);
void ani_create_mpeg(void);
void ani_newskip(void);
void on_the_fly(int task);
void ani_view_created(void);
void ani_grab_start(void);
void ani_reset(void);
void ani_grab_mouse(int flag, int ix, int iy);
void ani_flip1(int n);
void ani_zero(void);
int get_ani_file(const char *fname); /* 1 when a file was loaded */
void reset_comets(void);
void render_ani(void);
void set_ani_perm(void);

/* The animator's options: the -anifile to load at the start, and whether
   the animation follows an integration as it runs */
typedef struct {
    int use_file;             /* -anifile was given */
    char file[XPP_MAX_NAME];  /* the .ani file it named (comline.cpp writes it by its size) */
    int on_the_fly;           /* animate while integrating */
} XppAniOptions;
extern XppAniOptions ani_options;

#ifdef __cplusplus
}

/* the animation window: its size in the core's pixels, the row shown */
struct VCR {
    int hgt = 0, wid = 0, iexist = 0, ok = 0;
    int pos = 0, inc = 0;
    std::string file; /* the .ani file, or the folder to pick one in */
};
extern VCR vcr;

/* Frame saving (the ppm frames, or an animated gif) */
struct MPEG_SAVE {
    int flag = 0;
    int skip = 0;
    std::string root; /* the frames' base name */
    int aviflag = 0;
};
/* C linkage: json_ani.cpp declares it so itself */
extern "C" MPEG_SAVE mpeg;
#endif
#endif
