#ifndef _graf_par_h_
#define _graf_par_h_


#include "xpplim.h"
#define RUBBOX 0

#define SCRNFMT 0
#define PSFMT 1
#define SVGFMT 2


#define REAL_SMALL 1.e-6

#define MAXBIFCRV 100
#define lmax(a,b) ((a) > (b) ? (a) : (b))

#include "xpp_types.h"
#include "struct.h" /* CURVE, MAXFRZ */
#ifdef __cplusplus
extern "C" {
#endif

/* graf_par.cpp: the palette index of each named colour (pop_list.h's color_names) */
extern int colorline[];

/* The frozen curves of every plot window (Graphic stuff > Freeze) */
typedef struct {
    CURVE curve[MAXFRZ]; /* .use: the slot holds one; .w: its window */
    int auto_freeze;     /* freeze the curve after every integration */
} XppFrozenCurves;
extern XppFrozenCurves frozen_curves;


void change_view_com(int com);
void check_flags(void);
void get_2d_view(int ind);
void axes_opts(void);
void get_3d_view(int ind);
void check_val(double *x1, double *x2, double *xb, double *xd);
void corner_cube(double *xlo, double *xhi, double *ylo, double *yhi);
void fit_window(void);
void check_windows(void);
void user_window(void);
void pretty(double *x1, double *x2);
void xi_vs_t(void);
void movie_rot(double start, double increment, int nclip, int angle);
void get_3d_par_com(void);
void update_view(float xlo, float xhi, float ylo, float yhi);
void window_zoom_com(int c);
void zoom_in(int i1, int j1, int i2, int j2);
void zoom_out(int i1, int j1, int i2, int j2);
void graph_all(int *list, int n, int type);
int alter_curve(const char *title, int in_it, int n);
void edit_curve(void);
void new_curve(void);
void create_ps(void);
void change_cmap_com(int i);
void freeze_com(int c);
void set_key(int x, int y);
void draw_freeze_key(void);
void key_frz_com(int c);
void delete_frz_crv(int i);
void kill_frz(void);
int freeze_crv(int ind);
void auto_freeze_it(void);
int create_crv(int ind);
void edit_frz_crv(int i);
void draw_freeze(XppWinId w);
void init_bd(void);
int get_frz_index(XppWinId w);
void export_graf_data(void);
void add_a_curve_com(int c);
void default_window();
void dump_ps( int i);

#ifdef __cplusplus
}

#include <string>
/* How plots are written to files (PostScript, SVG) */
struct XppPlotExport {
    std::string format; /* a batch run's plot files: "ps" or "svg" (@ plotfmt=) */
    int color = 1;      /* in colour (1) or black and white */
};
extern XppPlotExport plot_export;
#endif
#endif
