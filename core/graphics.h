#ifndef _graphics_h
#define _graphics_h
#ifdef __cplusplus
extern "C" {
#endif

extern const int TextAngle;

void get_scale(double *x1, double *y1, double *x2, double *y2);
void set_scale(double x1, double y1, double x2, double y2);
void change_current_linestyle(int newstyle, int *old);
void set_linestyle(int ls);
int clip3d(float x1, float y1, float z1, float x2, float y2, float z2, float *x1p, float *y1p, float *z1p, float *x2p, float *y2p, float *z2p);
void eq_symb(double *x, int type);

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

namespace xpp {
struct Session; /* session.h */
}

/* The drawing into the session s's active plot window, or into the
   picture file begun (s.plot_file): its scale and drawing area, the
   device primitives, the plot's world coordinates' lines, points and
   text, 2D and 3D (point_abs, line_abs, line_3d ...), and the plot
   windows' settings (init_all_graph, reset_graph, get_graph ...) */
void get_draw_area_flag(xpp::Session &s, int flag);
void get_draw_area(xpp::Session &s);
void set_normal_scale(xpp::Session &s);
void point(xpp::Session &s, int x, int y);
void line(xpp::Session &s, int x1, int y1, int x2, int y2);
void bead(xpp::Session &s, int x1, int y1);
void frect(xpp::Session &s, int x1, int y1, int w, int h);
void put_text(xpp::Session &s, int x, int y, const char *str);
void init_x11(xpp::Session &s);
void init_ps(xpp::Session &s);
void init_svg(xpp::Session &s);
void scale_dxdy(const xpp::Session &s, float x, float y, double *i, double *j);
void scale_to_screen(const xpp::Session &s, float x, float y, int *i, int *j);
void scale_to_real(xpp::Session &s, int i, int j, float *x, float *y);
void init_all_graph(xpp::Session &s);
void set_extra_graphs(xpp::Session &s);
void reset_graph(xpp::Session &s);
void get_graph(xpp::Session &s);
void init_graph(xpp::Session &s, int i);
void copy_graph(xpp::Session &s, int i, int l);
void make_rot(xpp::Session &s, double theta, double phi);
void reset_all_line_type(xpp::Session &s);
void scale3d(const xpp::Session &s, float x, float y, float z, float *xp, float *yp, float *zp);
int threedproj(const xpp::Session &s, float x2p, float y2p, float z2p, float *xp, float *yp);
void text3d(xpp::Session &s, float x, float y, float z, const char *str);
int threed_proj(const xpp::Session &s, float x, float y, float z, float *xp, float *yp);
void point_3d(xpp::Session &s, float x, float y, float z);
void line3dn(xpp::Session &s, float xs1, float ys1, float zs1, float xsp1, float ysp1, float zsp1);
void line3d(xpp::Session &s, float x01, float y01, float z01, float x02, float y02, float z02);
void line_3d(xpp::Session &s, float x, float y, float z, float xp, float yp, float zp);
void pers_line(xpp::Session &s, float x, float y, float z, float xp, float yp, float zp);
void rot_3dvec(const xpp::Session &s, float x, float y, float z, float *xp, float *yp, float *zp);
void point_abs(xpp::Session &s, float x1, float y1);
void line_nabs(xpp::Session &s, float x1_out, float y1_out, float x2_out, float y2_out);
void bead_abs(xpp::Session &s, float x1, float y1);
void frect_abs(xpp::Session &s, float x1, float y1, float w, float h);
void line_abs(xpp::Session &s, float x1, float y1, float x2, float y2);
void text_abs(xpp::Session &s, float x, float y, const char *text);
void fancy_text_abs(xpp::Session &s, float x, float y, const char *old, int size, int font);
void draw_symbol(xpp::Session &s, float x, float y, float size, int my_symb);
/* the line (x1,y1)-(x2,y2) clipped to the drawing d's world box: 1 with
   its ends when any of it is inside */
int clip(const DrawingState &d, float x1, float x2, float y1, float y2, float *x1_out, float *y1_out, float *x2_out,
         float *y2_out);
#endif
#endif
