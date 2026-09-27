#ifndef _graphics_h
#define _graphics_h
#ifdef __cplusplus
extern "C" {
#endif

extern const int TextAngle;

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
void put_text(int x, int y, const char *str);
void init_x11(void);
void init_ps(void);
void init_svg(void);
void set_linestyle(int ls);
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
void text3d(float x, float y, float z, const char *s);
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
void text_abs(float x, float y, const char *text);
void fancy_text_abs(float x, float y, const char *old, int size, int font);
int clip3d(float x1, float y1, float z1, float x2, float y2, float z2, float *x1p, float *y1p, float *z1p, float *x2p, float *y2p, float *z2p);
int clip(float x1, float x2, float y1, float y2, float *x1_out, float *y1_out, float *x2_out, float *y2_out);
void eq_symb(double *x, int type);
void draw_symbol(float x, float y, float size, int my_symb);
void reset_all_line_type();

#ifdef __cplusplus
}

#include <string>
#include <string_view>
/* text with each \{expr} replaced by the expression's value */
std::string fill_in_text(std::string_view old);

/* the drawing state (graphics.cpp, axes2.cpp), a Session's (session.h):
   the drawing area in device units, its tick and character sizes, its
   world coordinates, the current point's type and size, the text's
   justification, a PostScript picture's orientation (ps_port) and the 3D
   view's angles; set while the axes (the box's own sides) are drawn, for
   the SVG classes; label unlabelled 2D axes with the plotted variables
   (front ends that ask) */
struct DrawingState {
  int d_left = 0, d_right = 0, d_top = 0, d_bottom = 0;
  int v_tic = 0, h_tic = 0, v_char = 0, h_char = 0;
  double x_min = 0, y_min = 0, x_max = 0, y_max = 0;
  int point_type = -1, point_radius = 0, text_justify = 0, ps_port = 0;
  double theta0 = 45, phi0 = 45;
  int doing_axes = 0, doing_box_axes = 0;
  int axis_var_labels = 0;
};
#endif
#endif
