#ifndef _graphics_h
#define _graphics_h
#ifdef __cplusplus
extern "C" {
#endif


void get_scale(double *x1, double *y1, double *x2, double *y2);
void set_scale(double x1, double y1, double x2, double y2);
void get_draw_area_flag(int flag);
void get_draw_area(void);
void change_current_linestyle(int newstyle, int *old);
void set_normal_scale(void);
void point(int x, int y);
void line(int x1, int y1, int x2, int y2);
void bead(int x1, int y1);
void frect(int x1, int y1, int w, int h);
void put_text(int x, int y, char *str);
void init_x11(void);
void init_ps(void);
void init_svg(void);
void point_x11(int xp, int yp);
void set_linestyle(int ls);
void set_line_style_x11(int ls);
void bead_x11(int x, int y);
void rect_x11(int x, int y, int w, int h);
void line_x11(int xp1, int yp1, int xp2, int yp2);
void put_text_x11(int x, int y, char *str);
void special_put_text_x11(int x, int y, char *str, int size);
void fancy_put_text_x11(int x, int y, char *str, int size, int font);
void scale_dxdy(float x, float y, double *i, double *j);
void scale_to_screen(float x, float y, int *i, int *j);
void scale_to_real(int i, int j, float *x, float *y);
void init_all_graph(void);
void set_extra_graphs(void);
void reset_graph(void);
void get_graph(void);
void init_graph(int i);
void copy_graph(int i, int l);
void make_rot(double theta, double phi);
void scale3d(float x, float y, float z, float *xp, float *yp, float *zp);
int threedproj(float x2p, float y2p, float z2p, float *xp, float *yp);
void text3d(float x, float y, float z, char *s);
int threed_proj(float x, float y, float z, float *xp, float *yp);
void point_3d(float x, float y, float z);
void line3dn(float xs1, float ys1, float zs1, float xsp1, float ysp1, float zsp1);
void line3d(float x01, float y01, float z01, float x02, float y02, float z02);
void line_3d(float x, float y, float z, float xp, float yp, float zp);
void pers_line(float x, float y, float z, float xp, float yp, float zp);
void rot_3dvec(float x, float y, float z, float *xp, float *yp, float *zp);
void point_abs(float x1, float y1);
void line_nabs(float x1_out, float y1_out, float x2_out, float y2_out);
void bead_abs(float x1, float y1);
void frect_abs(float x1, float y1, float w, float h);
void line_abs(float x1, float y1, float x2, float y2);
void text_abs(float x, float y, char *text);
void fillintext(char *old, char *newname);
void fancy_text_abs(float x, float y, char *old, int size, int font);
int clip3d(float x1, float y1, float z1, float x2, float y2, float z2, float *x1p, float *y1p, float *z1p, float *x2p, float *y2p, float *z2p);
int clip(float x1, float x2, float y1, float y2, float *x1_out, float *y1_out, float *x2_out, float *y2_out);
void eq_symb(double *x, int type);
void draw_symbol(float x, float y, float size, int my_symb);
void reset_all_line_type();

#ifdef __cplusplus
}
#endif
#endif
