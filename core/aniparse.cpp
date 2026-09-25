/* The animator: the .ani language (parsing, evaluation per frame) and the
   drawing of a frame.

   A frame is computed in the animation's own coordinates (`dimension`,
   [0,1] x [0,1] by default, y up) and each primitive goes to two places
   (the "frame sinks" below): the pixel callbacks (xpp_ui ani_*, scaled
   to the vcr.wid x vcr.hgt window exactly as XPP always has; no front end
   draws them since protocol 2) and ani_data.h, which
   keeps it in unit coordinates for a front end that draws it itself
   (docs/protocol.md "The animation as data"). */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "aniparse.h"
#include "ani_data.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_globals.h"
#include "parserslow.h"
#include "form_ode.h"
#include "my_rhs.h"
#include "nullcline.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "load_eqn.h"
#include "integrate.h"
#include "init_conds.h"
#include "browse.h"
#include "xpplim.h"
#include <sys/time.h>
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
#define PCURVE 12
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

extern "C" {
extern double last_ic[MAXODE], T0;
extern char *color_names[12]; /* graf_par.c */
extern int colorline[];
extern int NODE, FIX_VAR, NMarkov;
extern BROWSER my_browser;
extern char this_file[XPP_MAX_NAME];
}

/***************  stuff for grabber  *******************/
typedef struct {
    double x0, y0;
    double x, y;
    double ox, oy;
    double t1, t2, tstart;
    double vx, vy;
    double vax, vay;
} ANI_MOTION_INFO;

static ANI_MOTION_INFO ami;

ANI_GRAB ani_grab[MAX_ANI_GRAB];
int n_ani_grab = 0;
int show_grab_points = 0;
int ani_grab_flag = 0;
XppAniOptions ani_options = {0, "", 0};
int who_was_grabbed;

/************************  end grabber **********************/

#define FIRSTCOLOR 30
int on_the_fly_speed = 10;

int aniflag;
int LastAniColor;
int ani_line;

int ani_speed = 10;
int ani_speed_inc = 2;

double ani_xlo = 0, ani_xhi = 1, ani_ylo = 0, ani_yhi = 1;
double ani_lastx, ani_lasty;

MPEG_SAVE mpeg;

ANI_COM my_ani[MAX_ANI_LINES];

VCR vcr;

int n_anicom;

int ani_text_size;
int ani_text_color;
int ani_text_font;

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

/*************************  NEW ANIMaTION STUFF ***********************/

/* set_val() of a name given as a literal (it takes a char *) */
static void set_named(const char *name, double value)
{
    char s[XPP_NAME_MAX + 1];
    snprintf(s, sizeof s, "%s", name);
    set_val(s, value);
}

double get_current_time(void)
{
    struct timeval tim;
    gettimeofday(&tim, NULL);
    return tim.tv_sec + (tim.tv_usec / 1000000.0);
}

void update_ani_motion_stuff(int x, int y)
{
    double dt;
    ami.t2 = ami.t1;
    ami.t1 = get_current_time();
    ami.ox = ami.x;
    ami.oy = ami.y;
    ani_ij_to_xy(x, y, &ami.x, &ami.y);
    dt = ami.t1 - ami.t2;
    if (dt == 0.0) dt = 10000000000;
    ami.vx = (ami.x - ami.ox) / dt;
    ami.vy = (ami.y - ami.oy) / dt;

    dt = ami.tstart - ami.t2;
    if (dt == 0.0) dt = 100000000000;
    ami.vax = (ami.x0 - ami.x) / dt;
    ami.vay = (ami.y0 - ami.y) / dt;
    set_named("mouse_x", ami.x);
    set_named("mouse_y", ami.y);
    set_named("mouse_vx", ami.vx);
    set_named("mouse_vy", ami.vy);
    do_grab_tasks(1);
    fix_only();
    ani_frame(0);
}

/*************************** End motion & speed stuff   ****************/

void ani_create_mpeg(void)
{
    char n0[] = "PPM 0/1", n1[] = "Basename", n2[] = "AniGif(0/1)", title[] = "Frame saving";
    char *n[] = {n0, n1, n2};
    char values[3][MAX_LEN_SBOX];
    int status;
    mpeg.flag = 0;
    XPP_SPRINTF(values[0], "%d", mpeg.flag);
    snprintf(values[1], sizeof(values[1]), "%.24s", mpeg.root);
    XPP_SPRINTF(values[2], "%d", mpeg.aviflag);
    static const int kinds[] = {XPP_FIELD_INTEGER, XPP_FIELD_FILE, XPP_FIELD_INTEGER};
    status = do_string_box_of(3, 3, 1, title, n, values, 28, kinds);
    if (status != 0) {
        mpeg.flag = atoi(values[0]);
        if (mpeg.flag > 0) mpeg.flag = 1;
        mpeg.aviflag = atoi(values[2]);
        XPP_SPRINTF(mpeg.root, "%s", values[1]);
        if (mpeg.aviflag == 1) mpeg.flag = 0;
    } else
        mpeg.flag = 0;
    if (mpeg.flag == 1) ani_disk_warn();
}

void ani_newskip(void)
{
    char bob[20], title[] = "Frame skip", name[] = "Increment:", ok[] = "Ok", cancel[] = "Cancel";
    int status;
    XPP_SPRINTF(bob, "%d", vcr.inc);
    status = get_dialog_of(title, name, bob, ok, cancel, 20, XPP_FIELD_INTEGER);
    if (status != 0) {
        vcr.inc = atoi(bob);
        if (vcr.inc <= 0) vcr.inc = 1;
    }
}

void on_the_fly(int task)
{
    if (vcr.iexist == 0 || n_anicom == 0) return;
    ani_frame(task);
    waitasec(on_the_fly_speed);
}

void ani_frame(int task)
{
    xpp_ui.ani_clear();
    if (task == 1) {
        set_ani_perm();
        reset_comets();
        return;
    }
    render_ani();
    xpp_ui.ani_show();
}

void set_to_init_data(void)
{
    int i;
    for (i = 0; i < NODE; i++) last_ic[i] = get_ivar(i + 1);
    for (i = NODE + FIX_VAR; i < NODE + FIX_VAR + NMarkov; i++) last_ic[i - FIX_VAR] = get_ivar(i + 1);
    redraw_ics();
}

void set_from_init_data(void)
{
    double y[MAXODE];
    int i;
    for (i = 0; i < NODE + NMarkov; i++) y[i] = last_ic[i];
    set_fix_rhs(T0, y);
}

void ani_flip1(int n)
{
    int row;
    float **ss;
    double y[MAXODE];
    double t;
    int i;
    if (n_anicom == 0) return;
    if (my_browser.maxrow < 2) return;
    ss = my_browser.data;
    xpp_ui.ani_clear();
    if (vcr.pos == 0) set_ani_perm();

    vcr.pos = vcr.pos + n;
    if (vcr.pos >= my_browser.maxrow) vcr.pos = my_browser.maxrow - 1;
    if (vcr.pos < 0) vcr.pos = 0;
    row = vcr.pos;

    t = (double)ss[0][row];
    for (i = 0; i < NODE + NMarkov; i++) y[i] = (double)ss[i + 1][row];
    set_fix_rhs(t, y);

    render_ani();
    xpp_ui.ani_show();
}

void ani_disk_warn(void)
{
    unsigned int total = (my_browser.maxrow * vcr.wid * vcr.hgt * 3) / (mpeg.skip * vcr.inc);
    char junk[256], yes[] = "YES", no[] = "NO", keys[] = "yn";
    char ans;
    total = total / (1024 * 1024);
    if (total > 10) {
        XPP_SPRINTF(junk, " %u Mb disk space needed! Continue?", total);
        ans = (char)TwoChoice(yes, no, junk, keys);
        if (ans != 'y') mpeg.flag = 0;
    }
}

void ani_zero(void)
{
    vcr.iexist = 0;
    vcr.ok = 0;
    vcr.inc = 1;
    vcr.pos = 0;
    n_anicom = 0;
    ani_speed = 10;
    aniflag = TRANSIENT;
    ani_grab_flag = 0;
    if (ani_options.use_file)
        XPP_FORMAT_TO_BUF(vcr.file,"{}", ani_options.file);
    else {
        XPP_FORMAT_TO_BUF(vcr.file,"{}", this_file);
        XPP_SPRINTF(vcr.file, "%s/", dirname(vcr.file));
    }
}

int get_ani_file(char *fname)
{
    int status;
    int err;
    char title[] = "Load animation", wild[] = "*.ani";

    if (fname == NULL) {
        status = file_selector(title, vcr.file, wild);
        if (status == 0) return 0;
    } else {
        if (fname != vcr.file) XPP_FORMAT_TO_BUF(vcr.file,"{}", fname);
    }
    err = ani_new_file(vcr.file);
    if (err < 0) return 0;
    vcr.ok = 1; /* loaded and compiled */
    plintf("Loaded %d lines successfully!\n", n_anicom);
    ani_grab_flag = 0;
    return 1;
}

int ani_new_file(char *filename)
{
    FILE *fp;
    char bob[100];
    fp = fopen(filename, "r");
    if (fp == NULL) {
        snprintf(bob, sizeof bob, "Couldn't open ani-file");
        err_msg(bob);
        return -1;
    }
    if (n_anicom > 0) free_ani();
    /* a new animation: its frames start again, nothing of the old one shows */
    ani_data_forget();
    if (load_ani_file(fp) == 0) {
        fclose(fp);
        XPP_SPRINTF(bob, "Bad ani-file at line %d", ani_line);
        err_msg(bob);
        return -1;
    }
    fclose(fp);
    return 0;
}

int load_ani_file(FILE *fp)
{
    char old[300], expanded[300], big[300];
    int notdone = 1, jj1, jj2, jj;
    int ans = 0, flag;
    ani_line = 1;
    while (notdone) {
        read_ani_line(fp, old);
        search_array(old, expanded, &jj1, &jj2, &flag);
        for (jj = jj1; jj <= jj2; jj++) {
            subsk(expanded, big, jj, flag);
            ans = parse_ani_string(big, fp);
        }

        if (ans == 0 || feof(fp)) break;
        if (ans < 0) { /* error occurred !! */
            plintf(" error at line %d\n", ani_line);
            free_ani();
            return 0;
        }
        ani_line++;
    }
    return 1;
}

/* a command is known by its first two letters */
static const struct {
    const char *prefix;
    int type;
} ani_commands[] = {
    {"GR", GRAB},     {"LI", LINE},         {"RL", RLINE},     {"RE", RECT},  {"FR", FRECT},   {"EL", ELLIP},
    {"FE", FELLIP},   {"CI", CIRC},         {"FC", FCIRC},     {"VT", VTEXT}, {"TE", TEXT},    {"SE", SETTEXT},
    {"TR", TRANSIENT}, {"PE", PERMANENT},   {"DI", DIMENSION}, {"EN", END},   {"DO", END},     {"SP", SPEED},
    {"CO", COMET},    {"XN", AXNULL},       {"YN", AYNULL},
};

/*  This has changed to add the FILE fp to the arguments since the GRAB
    command requires that you load in two additional lines
*/
int parse_ani_string(char *s, FILE *fp)
{
    char x1[300], x2[300], x3[300], x4[300], col[300], thick[300];
    char *ptr, *nxt;
    char *command;
    int type = -1;
    int anss;
    x1[0] = 0;
    x2[0] = 0;
    x3[0] = 0;
    x4[0] = 0;
    col[0] = 0;
    thick[0] = 0;
    ptr = s;
    type = COMNT;
    command = get_first(ptr, "; ");
    if (command == NULL) return -1;
    strupr(command);
    /************** GRAB STUFF *****************/
    for (const auto &k : ani_commands)
        if (strncmp(k.prefix, command, 2) == 0) type = k.type;
    switch (type) {
    case GRAB:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        anss = add_grab_command(x1, x2, x3, fp);
        return (anss);
    case AXNULL:
    case AYNULL:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x4,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        nxt = get_next("\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(thick,"{}", nxt);
        break;
    case LINE:
    case RECT:
    case ELLIP:
    case FELLIP:
    case FRECT:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x4,"{}", nxt);
        nxt = get_next(";\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        nxt = get_next("\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(thick,"{}", nxt);
        break;
    case RLINE:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        nxt = get_next("\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(thick,"{}", nxt);
        break;
    case COMET:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(thick,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        nxt = get_next(";\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        break;
    case CIRC:
    case FCIRC:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        nxt = get_next(";\n");
        if ((nxt == NULL) || strlen(nxt) == 0) break;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        break;
    case SETTEXT:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(col,"{}", nxt);
        break;
    case TEXT:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x4,"{}", nxt);
        break;
    case VTEXT:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x4,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        break;
    case SPEED:
        nxt = get_next(" \n");
        if (nxt == NULL) return -1;
        ani_speed = atoi(nxt);
        if (ani_speed < 0) ani_speed = 0;
        if (ani_speed > 1000) ani_speed = 1000;
        return 1;
    case DIMENSION:
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x1,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x2,"{}", nxt);
        nxt = get_next(";");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x3,"{}", nxt);
        nxt = get_next(";\n");
        if (nxt == NULL) return -1;
        XPP_FORMAT_TO_BUF(x4,"{}", nxt);
        break;
    }

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
    return (add_ani_com(type, x1, x2, x3, x4, col, thick));
}

void set_ani_dimension(char *x1, char *y1, char *x2, char *y2)
{
    double xx1, yy1, xx2, yy2;
    xx1 = atof(x1);
    xx2 = atof(x2);
    yy1 = atof(y1);
    yy2 = atof(y2);

    if ((xx1 < xx2) && (yy1 < yy2)) {
        ani_xlo = xx1;
        ani_xhi = xx2;
        ani_ylo = yy1;
        ani_yhi = yy2;
    }
}

int add_ani_com(int type, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    int err = 0;
    if (type == COMNT || type == DIMENSION || type == PERMANENT || type == TRANSIENT || type == END || type == SPEED)
        return 1;
    my_ani[n_anicom].type = type;
    my_ani[n_anicom].flag = aniflag;
    my_ani[n_anicom].x1 = (int *)xpp_malloc(256 * sizeof(int));
    my_ani[n_anicom].y1 = (int *)xpp_malloc(256 * sizeof(int));
    my_ani[n_anicom].x2 = (int *)xpp_malloc(256 * sizeof(int));
    my_ani[n_anicom].y2 = (int *)xpp_malloc(256 * sizeof(int));
    my_ani[n_anicom].col = (int *)xpp_malloc(256 * sizeof(int));
    my_ani[n_anicom].who = (int *)xpp_malloc(256 * sizeof(int));
    switch (type) {
    case AXNULL:
    case AYNULL:
        err = add_ani_null(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;

    case COMET:
        err = add_ani_comet(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case LINE:
        err = add_ani_line(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case RLINE:
        err = add_ani_rline(&my_ani[n_anicom], x1, y1, col, thick);
        break;
    case RECT:
        err = add_ani_rect(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case FRECT:
        err = add_ani_frect(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case ELLIP:
        err = add_ani_ellip(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case FELLIP:
        err = add_ani_fellip(&my_ani[n_anicom], x1, y1, x2, y2, col, thick);
        break;
    case CIRC:
        err = add_ani_circle(&my_ani[n_anicom], x1, y1, x2, col, thick);
        break;
    case FCIRC:
        err = add_ani_circle(&my_ani[n_anicom], x1, y1, x2, col, thick);
        break;
    case TEXT:
        err = add_ani_text(&my_ani[n_anicom], x1, y1, y2);
        break;
    case VTEXT:
        err = add_ani_vtext(&my_ani[n_anicom], x1, y1, x2, y2);
        break;
    case SETTEXT:
        err = add_ani_settext(&my_ani[n_anicom], x1, y1, col);
        break;
    }
    if (err < 0) {
        /* free_ani() frees the commands before this one; this one's arrays too */
        xpp_free(my_ani[n_anicom].x1);
        xpp_free(my_ani[n_anicom].y1);
        xpp_free(my_ani[n_anicom].x2);
        xpp_free(my_ani[n_anicom].y2);
        xpp_free(my_ani[n_anicom].col);
        xpp_free(my_ani[n_anicom].who);
        free_ani();
        return -1;
    }
    n_anicom++;
    return 1;
}

void init_ani_stuff(void)
{
    ani_text_size = 1;
    ani_text_font = 0;
    ani_text_color = 0;
    ani_xlo = 0.0;
    ani_ylo = 0.0;
    ani_xhi = 1.0;
    ani_yhi = 1.0;
    aniflag = TRANSIENT;
    n_anicom = 0;
    ani_lastx = 0.0;
    ani_lasty = 0.0;
    vcr.pos = 0;
    ani_grab_flag = 0; /*********** GRABBER *******************/
    n_ani_grab = 0;
}

void free_ani(void)
{
    int i;
    for (i = 0; i < n_anicom; i++) {
        xpp_free(my_ani[i].x1);
        xpp_free(my_ani[i].y1);
        xpp_free(my_ani[i].x2);
        xpp_free(my_ani[i].y2);
        xpp_free(my_ani[i].who);
        xpp_free(my_ani[i].col);
        if (my_ani[i].type == COMET) {
            xpp_free(my_ani[i].c.x);
            xpp_free(my_ani[i].c.y);
            xpp_free(my_ani[i].c.col);
        }
    }
    n_anicom = 0;
    free_grabber();

    init_ani_stuff();
}

int chk_ani_color(char *s, int *index)
{
    int j;
    char *s2;

    *index = -1;
    de_space(s);
    strupr(s);
    if (strlen(s) == 0) {
        *index = 0;
        return 1;
    }
    if (s[0] == '$') {
        s2 = &s[1];
        for (j = 0; j < 12; j++) {
            if (strcmp(s2, color_names[j]) == 0) {
                *index = colorline[j];
                return 1;
            }
        }
    }
    return 0;
}

int add_ani_expr(char *x, int *c)
{
    int i, n;
    int com[300];
    int err;

    err = add_expr(x, com, &n);
    if (err == 1) return 1;
    for (i = 0; i < n; i++) c[i] = com[i];
    return 0;
}

/* the colour argument of a drawing command: a named colour ($RED) is kept
   as minus its palette index, anything else is compiled as an expression
   whose value (0..1) picks a colour of the colour map */
static int add_ani_color(ANI_COM *a, char *col)
{
    int index;
    if (chk_ani_color(col, &index) == 1) {
        a->col[0] = -index;
        return 0;
    }
    return add_ani_expr(col, a->col) == 1 ? -1 : 0;
}

/*  the commands  */

int add_ani_rline(ANI_COM *a, char *x1, char *y1, char *col, char *thick)
{
    int err;
    /* a named colour is -index like every other command's (it was +index,
       which read the index as a compiled expression) */
    if (add_ani_color(a, col) < 0) return -1;
    a->zthick = atoi(thick);
    if (a->zthick < 0) a->zthick = 0;

    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    return 0;
}

void reset_comets(void)
{
    int i;
    for (i = 0; i < n_anicom; i++)
        if (my_ani[i].type == COMET) my_ani[i].c.i = 0;
}

/* the comet's newest position (in the animation's coordinates) and colour */
void roll_comet(ANI_COM *a, double xn, double yn, int col)
{
    int i;

    int n = a->c.n;
    int ii = a->c.i;
    if (ii < n) { /* not loaded yet */
        a->c.x[ii] = xn;
        a->c.y[ii] = yn;
        a->c.col[ii] = col;
        a->c.i = a->c.i + 1;
        return;
    }
    /* its full so push down eliminating last */
    for (i = 1; i < n; i++) {
        a->c.x[i - 1] = a->c.x[i];
        a->c.y[i - 1] = a->c.y[i];
        a->c.col[i - 1] = a->c.col[i];
    }
    a->c.x[n - 1] = xn;
    a->c.y[n - 1] = yn;
    a->c.col[n - 1] = col;
}

int add_ani_comet(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    int err, n;
    (void)y2;
    if (add_ani_color(a, col) < 0) return -1;
    a->zthick = atoi(thick);
    n = atoi(x2);
    if (n <= 0) {
        plintf("4th argument of comet must be positive integer!\n");
        return (-1);
    }
    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    a->c.n = n;
    a->c.x = (double *)xpp_malloc(n * sizeof(double));
    a->c.y = (double *)xpp_malloc(n * sizeof(double));
    a->c.col = (int *)xpp_malloc(n * sizeof(int));
    a->c.i = 0;
    return 1;
}

int add_ani_line(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    int err;
    if (add_ani_color(a, col) < 0) return -1;
    a->zthick = atoi(thick);
    if (a->zthick < 0) a->zthick = 0;

    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    err = add_ani_expr(x2, a->x2);
    if (err) return -1;
    err = add_ani_expr(y2, a->y2);
    if (err) return -1;
    return 0;
}

int add_ani_null(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *who)
{
    int err;
    if (add_ani_color(a, col) < 0) return -1;

    err = add_ani_expr(who, a->who);
    if (err) return -1;
    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    err = add_ani_expr(x2, a->x2);
    if (err) return -1;
    err = add_ani_expr(y2, a->y2);
    if (err) return -1;
    return 0;
}

int add_ani_rect(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    return (add_ani_line(a, x1, y1, x2, y2, col, thick));
}

int add_ani_frect(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    return (add_ani_line(a, x1, y1, x2, y2, col, thick));
}

int add_ani_ellip(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    return (add_ani_line(a, x1, y1, x2, y2, col, thick));
}

int add_ani_fellip(ANI_COM *a, char *x1, char *y1, char *x2, char *y2, char *col, char *thick)
{
    return (add_ani_line(a, x1, y1, x2, y2, col, thick));
}

int add_ani_circle(ANI_COM *a, char *x1, char *y1, char *x2, char *col, char *thick)
{
    int err;
    if (add_ani_color(a, col) < 0) return -1;
    a->zthick = atoi(thick);
    if (a->zthick < 0) a->zthick = 0;

    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    err = add_ani_expr(x2, a->x2);
    if (err) return -1;
    return 0;
}

int add_ani_text(ANI_COM *a, char *x1, char *y1, char *y2)
{
    int err;
    char *s;
    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    s = (char *)(a->y2);
    snprintf(s, 256 * sizeof(int), "%s", y2);
    return 0;
}

int add_ani_vtext(ANI_COM *a, char *x1, char *y1, char *x2, char *y2)
{
    int err;
    char *s;
    err = add_ani_expr(x1, a->x1);
    if (err) return -1;
    err = add_ani_expr(y1, a->y1);
    if (err) return -1;
    err = add_ani_expr(x2, a->x2);
    if (err) return -1;
    s = (char *)(a->y2);
    snprintf(s, 256 * sizeof(int), "%s", y2);
    return 0;
}

int add_ani_settext(ANI_COM *a, char *x1, char *y1, char *col)
{
    int size = atoi(x1);
    int font = 0;
    int index = 0, err;
    de_space(y1);
    if (y1[0] == 's' || y1[0] == 'S') font = 1;
    err = chk_ani_color(col, &index);
    if (err != 1) index = 0;
    if (size < 0) size = 0;
    if (size > 4) size = 4;
    a->tsize = size;
    a->tfont = font;
    a->tcolor = index;
    return 0;
}

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

void put_line(double x1, double y1, double x2, double y2)
{
    int i1, j1, i2, j2;
    ani_xyscale(x1, y1, &i1, &j1);
    ani_xyscale(x2, y2, &i2, &j2);
    xpp_ui.ani_line(i1, j1, i2, j2);
    ani_data_line(unit_x(x1), unit_y(y1), unit_x(x2), unit_y(y2), pen.color, pen.thick);
}

/* a rectangle with corners (x1,y1) and (x2,y2), in either order */
void put_rect(double x1, double y1, double x2, double y2, int fill)
{
    int i1, j1, i2, j2, h, w;
    ani_xyscale(x1, y1, &i1, &j1);
    ani_xyscale(x2, y2, &i2, &j2);
    h = abs(j2 - j1);
    w = abs(i2 - i1);
    if (i1 > i2) i1 = i2;
    if (j1 > j2) j1 = j2;
    xpp_ui.ani_rect(i1, j1, w, h, fill);
    ani_data_rect(unit_x(x1), unit_y(y1), unit_x(x2), unit_y(y2), pen.color, pen.thick, fill);
}

/* a circle of radius r: in pixels the mean of the radius scaled along x and along y */
void put_circle(double x, double y, double r, int fill)
{
    int i1, j1, i2, j2, ir;
    ani_xyscale(x, y, &i1, &j1);
    ani_radscale(r, &i2, &j2);
    ir = (i2 + j2) / 2;
    xpp_ui.ani_arc(i1 - ir, j1 - ir, 2 * ir, 2 * ir, fill);
    ani_data_circle(unit_x(x), unit_y(y), r / (ani_xhi - ani_xlo), r / (ani_yhi - ani_ylo), pen.color, pen.thick,
                    fill);
}

void put_ellipse(double x, double y, double rx, double ry, int fill)
{
    int i1, j1, i2, j2;
    ani_xyscale(x, y, &i1, &j1);
    ani_rad2scale(rx, ry, &i2, &j2);
    xpp_ui.ani_arc(i1 - i2, j1 - j2, 2 * i2, 2 * j2, fill);
    ani_data_ellipse(unit_x(x), unit_y(y), rx / (ani_xhi - ani_xlo), ry / (ani_yhi - ani_ylo), pen.color,
                     pen.thick, fill);
}

/* a filled circle of r pixels (a comet's), whatever the window's size */
void put_dot(double x, double y, int r)
{
    int i, j;
    ani_xyscale(x, y, &i, &j);
    xpp_ui.ani_arc(i - r, j - r, 2 * r, 2 * r, 1);
    ani_data_dot(unit_x(x), unit_y(y), r, pen.color);
}

/* text from its baseline's left end */
void put_text(double x, double y, char *s)
{
    int i, j;
    ani_xyscale(x, y, &i, &j);
    xpp_ui.ani_text(i, j, s);
    ani_data_text(unit_x(x), unit_y(y), s, pen.color, pen.size, pen.font);
}

} // namespace

void render_ani(void)
{
    int i;
    int type, flag;
    xpp_ui.ani_slider();
    pen.color = 0; /* ani_clear gave the frame a black pen */
    ani_data_begin();
    for (i = 0; i < n_anicom; i++) {
        type = my_ani[i].type;
        flag = my_ani[i].flag;
        if (type == LINE || type == RLINE || type == RECT || type == FRECT || type == CIRC || type == FCIRC ||
            type == ELLIP || type == FELLIP || type == COMET || type == AXNULL || type == AYNULL)
            eval_ani_color(i);
        switch (type) {
        case AXNULL:
        case AYNULL:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_null(i, type - AXNULL);
            break;
        case SETTEXT:
            set_ani_font_stuff(my_ani[i].tsize, my_ani[i].tfont, my_ani[i].tcolor);
            break;
        case TEXT:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_text(i);
            break;
        case VTEXT:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_vtext(i);
            break;
        case LINE:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_line(i);
            break;
        case COMET:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_comet(i);
            break;
        case RLINE:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_rline(i);
            break;
        case RECT:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_rect(i);
            break;
        case FRECT:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_frect(i);
            break;
        case ELLIP:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_ellip(i);
            break;
        case FELLIP:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_fellip(i);
            break;
        case CIRC:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_circ(i);
            break;
        case FCIRC:
            if (flag == TRANSIENT) eval_ani_com(i);
            draw_ani_fcirc(i);
            break;
        }
    }
    if (show_grab_points == 1) draw_grab_points();
    {
        AniDataFrame f;
        f.pos = vcr.pos;
        f.rows = my_browser.maxrow;
        f.t = get_ivar(0);
        f.speed = ani_speed;
        f.skip = vcr.inc;
        f.xlo = ani_xlo;
        f.ylo = ani_ylo;
        f.xhi = ani_xhi;
        f.yhi = ani_yhi;
        f.w = vcr.wid;
        f.h = vcr.hgt;
        ani_data_end(&f);
    }
}

void set_ani_perm(void)
{
    int i, type;
    set_from_init_data();
    for (i = 0; i < n_anicom; i++) {
        type = my_ani[i].type;
        if (my_ani[i].flag == PERMANENT) {
            if (my_ani[i].type != SETTEXT) eval_ani_com(i);
            if (type == LINE || type == RLINE || type == RECT || type == FRECT || type == CIRC || type == FCIRC ||
                type == ELLIP || type == FELLIP)
                eval_ani_color(i);
        }
    }
}

void eval_ani_color(int j)
{
    double z;

    if (my_ani[j].col[0] > 0) {
        z = evaluate(my_ani[j].col);
        if (z > 1) z = 1.0;
        if (z < 0) z = 0.0;
        my_ani[j].zcol = z;
    }
}

void eval_ani_com(int j)
{
    my_ani[j].zx1 = evaluate(my_ani[j].x1);
    my_ani[j].zy1 = evaluate(my_ani[j].y1);

    switch (my_ani[j].type) {
    case LINE:
    case RECT:
    case FRECT:
    case ELLIP:
    case FELLIP:
    case AXNULL:
    case AYNULL:
        my_ani[j].zx2 = evaluate(my_ani[j].x2);
        my_ani[j].zy2 = evaluate(my_ani[j].y2);
        break;
    case CIRC:
    case FCIRC:
        my_ani[j].zrad = evaluate(my_ani[j].x2);
        break;
    case VTEXT:
        my_ani[j].zval = evaluate(my_ani[j].x2);
        break;
    }

    if (my_ani[j].type == AXNULL || my_ani[j].type == AYNULL) my_ani[j].zval = evaluate(my_ani[j].who);
}

void set_ani_font_stuff(int size, int font, int color) { pen_font(size, font, color); }

void set_ani_col(int j)
{
    int c = my_ani[j].col[0];
    int icol;

    if (c <= 0)
        icol = -c;
    else
        icol = (int)(color_table.count * my_ani[j].zcol) + FIRSTCOLOR;
    pen_color(icol);
    LastAniColor = icol;
}

void xset_ani_col(int icol) { pen_color(icol); }

/**************   DRAWING ROUTINES   *******************/

void ani_rad2scale(double rx, double ry, int *ix, int *iy)
{
    double dx = (double)vcr.wid / (ani_xhi - ani_xlo), dy = (double)vcr.hgt / (ani_yhi - ani_ylo);
    double r1 = rx * dx, r2 = ry * dy;
    *ix = (int)r1;
    *iy = (int)r2;
}

void ani_radscale(double rad, int *ix, int *iy)
{
    double dx = (double)vcr.wid / (ani_xhi - ani_xlo), dy = (double)vcr.hgt / (ani_yhi - ani_ylo);
    double r1 = rad * dx, r2 = rad * dy;
    *ix = (int)r1;
    *iy = (int)r2;
}

void ani_ij_to_xy(int ix, int iy, double *x, double *y)
{
    double dx = (ani_xhi - ani_xlo) / (double)vcr.wid;
    double dy = (ani_yhi - ani_ylo) / (double)vcr.hgt;
    *x = ani_xlo + (double)ix * dx;
    *y = ani_ylo + (double)(vcr.hgt - iy) * dy;
}

void ani_xyscale(double x, double y, int *ix, int *iy)
{
    double dx = (double)vcr.wid / (ani_xhi - ani_xlo), dy = (double)vcr.hgt / (ani_yhi - ani_ylo);
    double xx = (x - ani_xlo) * dx;
    double yy = vcr.hgt - dy * (y - ani_ylo);
    *ix = (int)xx;
    *iy = (int)yy;
    if (*ix < 0) *ix = 0;
    if (*ix >= vcr.wid) *ix = vcr.wid - 1;
    if (*iy < 0) *iy = 0;
    if (*iy >= vcr.hgt) *iy = vcr.hgt - 1;
}

/* the comet keeps its last n positions (in the animation's coordinates)
   with their colours: filled circles of -thickness pixels, or lines */
void draw_ani_comet(int j)
{
    int k, nn, ir;
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    roll_comet(&my_ani[j], my_ani[j].zx1, my_ani[j].zy1, LastAniColor);
    nn = my_ani[j].c.i;
    if (my_ani[j].zthick < 0) {
        ir = -my_ani[j].zthick;
        for (k = 0; k < nn; k++) {
            xset_ani_col(my_ani[j].c.col[k]);
            put_dot(my_ani[j].c.x[k], my_ani[j].c.y[k], ir);
        }
    } else {
        if (nn > 2) {
            for (k = 1; k < nn; k++) {
                xset_ani_col(my_ani[j].c.col[k]);
                put_line(my_ani[j].c.x[k - 1], my_ani[j].c.y[k - 1], my_ani[j].c.x[k], my_ani[j].c.y[k]);
            }
        }
    }
}

/* a nullcline of the phase plane, its box [x1,x2] x [y1,y2] mapped to the
   animation's [0,1] x [0,1] (as XPP always has, whatever `dimension` says) */
void draw_ani_null(int j, int id)
{
    double xl = my_ani[j].zx1, xh = my_ani[j].zx2, yl = my_ani[j].zy1, yh = my_ani[j].zy2;
    double z = my_ani[j].zval;
    float *v;
    int n, i, i4, who;
    float x1, y1, x2, y2, dx = xh - xl, dy = yh - yl;
    int err;
    if (dx == 0.0 || dy == 0.0) return;

    set_ani_col(j);
    who = (int)z; /* the nullcline that you want  -1 is the default cline */
    err = get_nullcline_floats(&v, &n, who, id);
    if (err == 1) return;
    for (i = 0; i < n; i++) {
        i4 = 4 * i;
        x1 = (v[i4] - xl) / dx;
        y1 = (v[i4 + 1] - yl) / dy;
        x2 = (v[i4 + 2] - xl) / dx;
        y2 = (v[i4 + 3] - yl) / dy;
        put_line(x1, y1, x2, y2);
    }
}

void draw_ani_line(int j)
{
    double x1 = my_ani[j].zx1, x2 = my_ani[j].zx2, y1 = my_ani[j].zy1, y2 = my_ani[j].zy2;
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_line(x1, y1, x2, y2);
    ani_lastx = x2;
    ani_lasty = y2;
}

void draw_ani_rline(int j)
{
    double x1 = ani_lastx + my_ani[j].zx1, y1 = ani_lasty + my_ani[j].zy1;
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_line(ani_lastx, ani_lasty, x1, y1);
    ani_lastx = x1;
    ani_lasty = y1;
}

void draw_ani_circ(int j)
{
    set_ani_col(j);
    pen_thick(my_ani[j].zthick);
    put_circle(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zrad, 0);
}

void draw_ani_fcirc(int j)
{
    set_ani_col(j);
    pen_thick(my_ani[j].zthick);
    put_circle(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zrad, 1);
}

void draw_ani_rect(int j)
{
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_rect(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zx2, my_ani[j].zy2, 0);
}

void draw_ani_frect(int j)
{
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_rect(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zx2, my_ani[j].zy2, 1);
}

void draw_ani_ellip(int j)
{
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_ellipse(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zx2, my_ani[j].zy2, 0);
}

void draw_ani_fellip(int j)
{
    pen_thick(my_ani[j].zthick);
    set_ani_col(j);
    put_ellipse(my_ani[j].zx1, my_ani[j].zy1, my_ani[j].zx2, my_ani[j].zy2, 1);
}

void draw_ani_text(int j) { put_text(my_ani[j].zx1, my_ani[j].zy1, (char *)my_ani[j].y2); }

void draw_ani_vtext(int j)
{
    char s2[256];
    snprintf(s2, sizeof s2, "%s%g", (char *)my_ani[j].y2, my_ani[j].zval);
    put_text(my_ani[j].zx1, my_ani[j].zy1, s2);
}

void read_ani_line(FILE *fp, char *s)
{
    char temp[256];
    int i, n, ok, ihat = 0;
    s[0] = 0;
    ok = 1;
    while (ok) {
        ok = 0;
        if (fgets(temp, 256, fp) == NULL) {
            temp[0] = 0;
            break;
        }
        n = strlen(temp);
        for (i = n - 1; i >= 0; i--) {
            if (temp[i] == '\\') {
                ok = 1;
                ihat = i;
            }
        }
        if (ok == 1) temp[ihat] = 0;
        strcat(s, temp);
    }
    n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = ' ';
    s[n] = ' ';
    s[n + 1] = 0;
}

/*************************  GRABBER CODE *****************************/

int add_grab_command(char *xs, char *ys, char *ts, FILE *fp)
{
    char start[256], end[256];
    int com[256];
    int nc, j, k, ans;
    double z;
    read_ani_line(fp, start);
    read_ani_line(fp, end);

    if (n_ani_grab >= MAX_ANI_GRAB) {
        plintf("Too many grabbables! \n");
        return (-1);
    }
    j = n_ani_grab;
    z = atof(ts);
    if (z <= 0.0) z = .02;
    ani_grab[j].tol = z;
    if (add_expr(xs, com, &nc)) {
        plintf("Bad grab x %s \n", xs);
        return (-1);
    }
    ani_grab[j].x = (int *)xpp_malloc(sizeof(int) * (nc + 1));
    for (k = 0; k <= nc; k++) ani_grab[j].x[k] = com[k];

    if (add_expr(ys, com, &nc)) {
        plintf("Bad grab y %s \n", ys);
        return (-1);
    }
    ani_grab[j].y = (int *)xpp_malloc(sizeof(int) * (nc + 1));
    for (k = 0; k <= nc; k++) ani_grab[j].y[k] = com[k];
    ans = ani_grab_tasks(start, j, 1);
    if (ans < 0) return (-1);
    if (ani_grab_tasks(end, j, 2) == (-1)) return (-1);
    n_ani_grab++;
    return (1);
}

int ani_grab_tasks(char *line, int igrab, int which)
{
    int i, k;
    int n = strlen(line);
    char form[256], c;
    char rhs[256], lhs[256];
    k = 0;
    lhs[0] = 0;
    for (i = 0; i < n; i++) {
        c = line[i];
        if (c == '{' || c == ' ') continue;
        if (c == ';' || c == '}') {
            form[k] = 0;
            XPP_FORMAT_TO_BUF(rhs,"{}", form);
            if (add_grab_task(lhs, rhs, igrab, which) < 0) return (-1);
            k = 0;
            continue;
        }
        if (c == '=') {
            form[k] = 0;
            XPP_FORMAT_TO_BUF(lhs,"{}", form);
            k = 0;
            continue;
        }
        form[k] = c;
        k++;
    }
    return (1);
}

int run_now_grab(void)
{
    if (who_was_grabbed < 0) return (0);
    return (ani_grab[who_was_grabbed].end.runnow);
}

int search_for_grab(double x, double y)
{
    int i;
    double d, u, v;
    double dmin = 100000000;
    int imin = -1;
    for (i = 0; i < n_ani_grab; i++) {
        u = ani_grab[i].zx;
        v = ani_grab[i].zy;
        d = sqrt((x - u) * (x - u) + (y - v) * (y - v));
        if ((d < dmin) && (d < ani_grab[i].tol)) {
            dmin = d;
            imin = i;
        }
    }
    return (imin);
}

void do_grab_tasks(int which) /* which=1 for start, 2 for end */
{
    int i = who_was_grabbed;
    int j, n;
    double z;
    if (i < 0) return; /*  no legal grab point */
    if (which == 1) {
        n = ani_grab[i].start.n;
        for (j = 0; j < n; j++) {
            z = evaluate(ani_grab[i].start.comrhs[j]);
            set_val(ani_grab[i].start.lhsname[j], z);
        }
        return;
    }
    if (which == 2) {
        n = ani_grab[i].end.n;
        for (j = 0; j < n; j++) {
            z = evaluate(ani_grab[i].end.comrhs[j]);
            set_val(ani_grab[i].end.lhsname[j], z);
        }
        return;
    }
}

int add_grab_task(char *lhs, char *rhs, int igrab, int which)
{
    int com[256];
    int i, nc, k;
    int rn;
    if (strlen(lhs) > XPP_NAME_MAX) {
        plintf("Grab event variable %s is too long\n", lhs);
        return (-1);
    }
    if (which == 1) {
        i = ani_grab[igrab].start.n;
        if (i >= MAX_GEVENTS) return (-1); /* too many events */
        XPP_FORMAT_TO_BUF(ani_grab[igrab].start.lhsname[i],"{}", lhs);
        if (add_expr(rhs, com, &nc)) {
            plintf("Bad right-hand side for grab event %s\n", rhs);
            return (-1);
        }
        ani_grab[igrab].start.comrhs[i] = (int *)xpp_malloc(sizeof(int) * (nc + 1));
        for (k = 0; k <= nc; k++) ani_grab[igrab].start.comrhs[i][k] = com[k];

        ani_grab[igrab].start.n = ani_grab[igrab].start.n + 1;
        return (1);
    }
    if (which == 2) {
        if (strncmp("runnow", lhs, 6) == 0) {
            rn = atoi(rhs);
            ani_grab[igrab].end.runnow = rn;
            return (1);
        }
        i = ani_grab[igrab].end.n;
        if (i >= MAX_GEVENTS) return (-1); /* too many events */

        XPP_FORMAT_TO_BUF(ani_grab[igrab].end.lhsname[i],"{}", lhs);
        if (add_expr(rhs, com, &nc)) {
            plintf("Bad right-hand side for grab event %s\n", rhs);
            return (-1);
        }
        ani_grab[igrab].end.comrhs[i] = (int *)xpp_malloc(sizeof(int) * (nc + 1));
        for (k = 0; k <= nc; k++) ani_grab[igrab].end.comrhs[i][k] = com[k];
        ani_grab[igrab].end.n = ani_grab[igrab].end.n + 1;
        return (1);
    }
    return (-1);
}

/* Draw little black x's where the grab points are */
void draw_grab_points(void)
{
    double xc, yc, z;
    int i;
    pen_color(0);
    for (i = 0; i < n_ani_grab; i++) {
        xc = evaluate(ani_grab[i].x);
        yc = evaluate(ani_grab[i].y);
        ani_grab[i].zx = xc;
        ani_grab[i].zy = yc;
        z = ani_grab[i].tol;
        put_line(xc + z, yc + z, xc - z, yc - z);
        put_line(xc + z, yc - z, xc - z, yc + z);
    }
    show_grab_points = 0;
}

void free_grabber(void)
{
    int i, j, m;
    for (i = 0; i < n_ani_grab; i++) {
        xpp_free(ani_grab[i].x);
        xpp_free(ani_grab[i].y);
        m = ani_grab[i].start.n;
        for (j = 0; j < m; j++) xpp_free(ani_grab[i].start.comrhs[j]);

        m = ani_grab[i].end.n;
        for (j = 0; j < m; j++) xpp_free(ani_grab[i].end.comrhs[j]);
        ani_grab[i].start.n = 0;
        ani_grab[i].end.n = 0;
    }
}

/* ---- what the animation window's buttons and mouse do (was spread over
   aniparse.c's X11 event handlers) ---- */

/* a new animation window of vcr.wid x vcr.hgt exists */
void ani_view_created(void)
{
    mpeg.flag = 0;
    mpeg.filflag = 0;
    XPP_FORMAT_TO_BUF(mpeg.root,"{}", "frame");
    mpeg.filter[0] = 0;
    mpeg.skip = 1;
    vcr.pos = 0;
    if (ani_options.use_file) get_ani_file(vcr.file);
}

/* Grab: show the first frame with the grab points and wait for the mouse */
void ani_grab_start(void)
{
    if (n_ani_grab == 0) return;
    if (vcr.ok) {
        vcr.pos = 0;

        show_grab_points = 1;
        ani_frame(1);
        ani_frame(0);
        ani_grab_flag = 1;
    }
}

/* Reset: back to the first frame */
void ani_reset(void)
{
    vcr.pos = 0;
    reset_comets();
    xpp_ui.ani_slider();
    ani_flip1(0);
}

/* the mouse went down (flag 1) or up (0) at pixel ix,iy while grabbing */
void ani_grab_mouse(int flag, int ix, int iy)
{
    if (flag == 1) {
        ami.t1 = get_current_time();
        ami.tstart = ami.t1;
        ani_ij_to_xy(ix, iy, &ami.x, &ami.y);
        ami.x0 = ami.x;
        ami.y0 = ami.y;
        who_was_grabbed = search_for_grab(ami.x, ami.y);
        if (who_was_grabbed < 0) xpp_log(XPP_LOG_INFO, "Nothing grabbed\n");
    }
    if (flag == 0) { /* This is BUTTON RELEASE  */
        if (who_was_grabbed < 0) return;
        do_grab_tasks(2);
        set_to_init_data();
        ani_grab_flag = 0;
        redraw_params();
        if (run_now_grab()) {
            run_now();
            ani_grab_flag = 0;
        }
    }
}
