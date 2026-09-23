#ifndef PLOT_DATA_H
#define PLOT_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The plot windows as data, for a front end that draws them itself
   (docs/protocol.md "The plot as data", docs/ui-v2.md events 1-3):

   - "plots": every plot window (win, title, 2D/3D, axes, labels, 3D box and
     angles, curves) and which one is active;
   - "series": a window's curves as numbers, one event per window, and while
     an integration runs "append" parts for the active window.

   Each is sent only to a client that subscribed, at the end of a command
   and only when what it says changed. The events go out through the
   function given to plot_data_init() (ui_json.c: after the pending drawing,
   one line each).

   plot_data.cpp; C++ with a C API, nothing escapes it. */

typedef void (*PlotDataEmit)(const char *line, size_t len);

void plot_data_init(PlotDataEmit emit);

/* {"cmd":"data"}: which events the client wants from now on (each is sent at
   the next plot_data_update(), whatever changed) and whether values go as
   base64 float32 (f32) or JSON numbers */
void plot_data_subscribe(int series, int plots, int f32);

/* the "f32" the client last asked for in its "data" command: for value
   arrays other events send outside the subscription list above (the aplot
   event's "values", docs/ui-v2.md T12) */
int plot_data_want_f32(void);

/* the stored data changed (xpp_ui.h data_changed) */
void plot_data_changed(void);

/* the integrator stored row nrows-1 (xpp_ui.h rows_stored): appends, at
   most ten a second */
void plot_data_rows_stored(int nrows);

/* the end of a command: plots, then the series of every window that changed */
void plot_data_update(void);

#ifdef __cplusplus
}
#endif
#endif
