#ifndef XPP_COLORMAP_H
#define XPP_COLORMAP_H
#ifdef __cplusplus
extern "C" {
#endif

#define XPP_MAX_COLORS 256

extern int periodic, spectral;
extern int custom_color;
extern unsigned short xpp_cmap_rgb[XPP_MAX_COLORS][3];

/* The colour scale's slots in xpp_cmap_rgb (xpp_build_colormap sets them)
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
int read_cmap_from_file(char *fname, int n, int *rr, int *gg, int *bb);
void xpp_build_colormap(void);
void get_ps_color(int i, float *r, float *g, float *b);
void get_svg_color(int i, int *r, int *g, int *b);

#ifdef __cplusplus
}
#endif
#endif
