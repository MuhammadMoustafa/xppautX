
#ifndef _axes2_h_
#define _axes2_h_
#ifdef __cplusplus
extern "C" {
#endif


/* axes2.cpp: the plot's axes, tick labels and title */
void re_title(void);
void do_axes(void);
void Box_axis(double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy, int flag);

#ifdef __cplusplus
}
#endif
#endif
