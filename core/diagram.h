#ifndef _diagram_h_
#define _diagram_h_

#include <stdio.h>
#include "auto_nox.h"
#ifdef __cplusplus
extern "C" {
#endif

void start_diagram(int n);
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
void post_auto(void);
void svg_auto(void);
void bound_diagram(double *xlo, double *xhi, double *ylo, double *yhi);
int save_diagram(FILE *fp, int n);
int load_diagram(FILE *fp, int node);


#ifdef __cplusplus
}
#endif
#endif
