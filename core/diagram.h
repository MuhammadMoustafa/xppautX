#ifndef _diagram_h_
#define _diagram_h_

#include <stdio.h>
#include "auto_nox.h"
#ifdef __cplusplus
extern "C" {
#endif

/* AUTO's bifurcation diagram: a list of points in the order they were
   stored (diagram.cpp owns it). start_diagram empties it to one point of
   n variables, which the run's first point (edit_start) fills in place;
   add_diagram appends. Point i has index i. */
void start_diagram(int n);
/* how many points (1 for an empty diagram, 0 before start_diagram) */
int diagram_count(void);
/* the point of that index, or NULL */
DIAGRAM *diagram_point(int index);
/* the first point, or NULL before start_diagram */
DIAGRAM *diagram_first(void);
/* the point after / before d, or NULL at the end / the start */
DIAGRAM *diagram_next(const DIAGRAM *d);
DIAGRAM *diagram_prev(const DIAGRAM *d);
void edit_start(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4,double *evr, double *evi);
void edit_diagram(DIAGRAM *d, int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4,int flag2, double *evr, double *evi, double tp);
void add_diagram(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3,int icp4,int flag2, double *evr, double *evi);
void kill_diagrams(void);
/* the entry add_diagram or edit_start made last */
DIAGRAM *last_diagram(void);
/* the entry add_diagram or edit_start made last started its run from label `from` */
void set_last_diagram_from(int from);
/* the stored point labelled lab (the first, as AUTO reads its .s file), or NULL */
const DIAGRAM *diagram_of_label(int lab);
/* entry `index` of the diagram list is AUTO's point ntot of branch ibr (either sign of ntot) */
int diagram_has(int index, int ibr, int ntot);
void redraw_diagram(void);
void write_info_out(void);
void write_init_data_file(void);
void write_pts(void);
/* the diagram as a picture, in image_format.h's IMAGE_FORMAT_PS or
   IMAGE_FORMAT_SVG (W53, issue #101; GIF has no vector picture to draw
   here) */
void export_auto_picture(int fmt);
void bound_diagram(double *xlo, double *xhi, double *ylo, double *yhi);
int save_diagram(FILE *fp, int n);
int load_diagram(FILE *fp, int node);


#ifdef __cplusplus
}

#include <deque>
#include <vector>

/* a diagram point and the arrays its DIAGRAM entry points at */
struct DiagramPoint {
  DIAGRAM d{};
  std::vector<double> uhi,ulo,u0,ubar,evr,evi;
};

/* AUTO's bifurcation diagram, a Session's (session.h): its points in the
   order they were stored, point i with index i (so next/prev are index
   +/- 1). A deque, so a point's address stays valid while points are
   added. */
struct AutoDiagram {
  std::deque<DiagramPoint> points;
};
#endif
#endif
