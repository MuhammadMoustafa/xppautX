#ifndef AUTO_DATA_H
#define AUTO_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* AUTO's info strip and stability circle as data, for a front end that
   draws them itself: the "autoinfo" event (docs/protocol.md "The AUTO
   diagram as data", docs/ui-v2.md event 7).

   - The info strip (window 103 of the classic front ends) is what
     traverse_out() in auto_nox.c shows for the point a grab's cursor is on:
     branch, point, type, label, the parameters, the norm, the plotted
     variable's value and the period.
   - The stability circle (window 102) is what plot_stab() last drew: for a
     steady state e^lambda of each eigenvalue lambda (AUTO's send_eigen
     stores them so: inside the unit circle is stable), for a periodic orbit
     its Floquet multipliers. Every point AUTO computes or a redraw plots
     draws it, so after a run it is the last point's, while grabbing the
     cursor's.

   auto_nox.c reports both as it draws them; the front end sends the event
   at the end of a command, before every prompt and at most ten times a
   second while AUTO runs, and only when it differs from the one it sent
   last. Nothing is recorded before auto_data_init(), so a program without
   such a front end (xppaut) pays nothing.

   auto_data.cpp; C++ with a C API, nothing escapes it. */

typedef void (*AutoDataEmit)(const char *line, size_t len);

/* the index in the diagram data the client holds of AUTO's diagram entry
   `node` (DIAGRAM.index), or -1 when the data do not have it */
typedef int (*AutoDataPointOf)(int node);

void auto_data_init(AutoDataEmit emit, AutoDataPointOf point_of);

/* {"cmd":"data"} with or without "autoinfo": the event is sent at the next
   update whatever it holds */
void auto_data_subscribe(int on);

/* a new AUTO window, or none: nothing to show */
void auto_data_forget(void);

/* the point the info strip shows; names are the model's, whole. p2name is
   NULL for a one-parameter point (the strip shows a blank name and 0) */
typedef struct AutoDataInfo {
    int ibr, pt;     /* AUTO's branch and point number, signed as AUTO has them */
    int itp, lab;    /* AUTO's point type and label */
    int type;        /* 1 stable steady state, 2 unstable, 3 stable periodic, 4 unstable periodic */
    int flag2;       /* two-parameter curve kind, 0 for one parameter */
    int node;        /* DIAGRAM.index of the point */
    const char *sym; /* get_bif_sym: EP, LP, HB, ... or blank */
    const char *p1name, *p2name;
    double p1, p2;
    double norm;
    const char *vname; /* the variable of the Axes setting and its value (u0) */
    double u;
    double per;
    double x, y, y2; /* where the diagram plots it (auto_xy_plot) */
} AutoDataInfo;

void auto_data_info(const AutoDataInfo *info);

/* the stability circle now shows these n values (evr + i evi); periodic:
   Floquet multipliers, else e^lambda of a steady state's eigenvalues */
void auto_data_stab(const double *evr, const double *evi, int n, int periodic);

/* send the event if it changed since the last one sent; final 0 (during a
   run) sends at most ten a second */
void auto_data_update(int final);

#ifdef __cplusplus
}
#endif
#endif
