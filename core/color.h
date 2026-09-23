#ifndef _color_h_
#define _color_h_
#ifdef __cplusplus
extern "C" {
#endif


#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void tst_color(Window w);
#endif /* Xlib.h */
void set_scolor(int col);
void set_color(int col);
void make_cmaps(int *r, int *g, int *b, int n, int type);
int rfun(double y, int per);
int gfun(double y, int per);
int bfun(double y, int per);
void NewColormap(int type);
void get_ps_color(int i, float *r, float *g, float *b);
void get_svg_color(int i,int *r,int *g,int *b);
void MakeColormap(void);
int ColorMap(int i);


#ifdef __cplusplus
}
#endif
#endif
