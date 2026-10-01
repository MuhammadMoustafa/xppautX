#ifndef XPP_COLORMAP_H
#define XPP_COLORMAP_H

namespace xpp {

#define XPP_MAX_COLORS 256

extern unsigned short cmap_rgb[XPP_MAX_COLORS][3];

/* The colour scale's slots in cmap_rgb (xpp_build_colormap sets them)
   and whether the front end shows colour at all. */
typedef struct {
    int enabled; /* 0 headless: colour-by-variable is refused */
    int first;   /* the scale's first slot */
    int last;    /* its last slot */
    int count;   /* its number of colours */
} XppColorTable;
extern XppColorTable color_table;

int rfun(double y, int per);
int gfun(double y, int per);
int bfun(double y, int per);
void make_cmaps(int *r, int *g, int *b, int n, int type);
/* cmap_rgb and color_table for the colour scale of make_cmaps's type
   (a Session's colormap), at each model's start and on a change. (Its
   xpp_ prefix goes, as the others' did at W109e, once W119's
   xpp_batch.cpp, which calls it, has merged.) */
void xpp_build_colormap(int type);
void get_ps_color(int i, float *r, float *g, float *b);
void get_svg_color(int i, int *r, int *g, int *b);

} // namespace xpp
#endif
