
#ifndef _axes2_h_
#define _axes2_h_
#ifdef __cplusplus
extern "C" {
#endif


/* axes2.cpp: the plot's axes, tick labels and title */
void re_title(void);
void redraw_cube_pt(double theta, double phi);
void do_axes(void);
void Box_axis(double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy, int flag);
/* set while the axes (the box's own sides) are drawn, for the SVG classes */
extern int DOING_AXES, DOING_BOX_AXES;
/* label unlabelled 2D axes with the plotted variables (front ends that ask) */
extern int AxisVarLabels;

#ifdef __cplusplus
}
#endif
#endif
