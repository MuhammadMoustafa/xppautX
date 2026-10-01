#ifndef PLOT_DATA_H
#define PLOT_DATA_H

#include <string_view>

namespace xpp {
struct Session; /* session.h */

/* The plot windows as data, for a front end that draws them itself
   (docs/protocol.md "The plot as data", docs/ui-v2.md events 1-3):

   - "plots": every plot window (win, title, 2D/3D, axes, labels, 3D box and
     angles, curves) and which one is active;
   - "series": a window's curves as numbers, one event per window, and while
     an integration runs "append" parts for the active window.

   Each is sent only to a client that subscribed, at the end of a command
   and only when what it says changed. The events go out through the
   function given to plot_data_init() (ui_json.cpp: after the pending drawing,
   one line each).

   plot_data.cpp; nothing escapes it. */

typedef void (*PlotDataEmit)(std::string_view line);

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

/* the Erase (redraw 0) or Redraw (1) command on the session s: an "erase"
   or "redraw" event for each window it blanked or drew again, to a client
   that asked for series (docs/protocol.md "The plot as data"). Only these
   commands: other blanks (a slider's rerun, a resize, a zoom) keep what
   the client shows. */
void plot_data_picture(Session &s, int redraw);

/* the integrator stored row nrows-1 of s (xpp_ui.h rows_stored): appends,
   at most about one a display frame (60 a second) */
void plot_data_rows_stored(Session &s, int nrows);

/* the end of a command on s: plots, then the series of every window that
   changed */
void plot_data_update(Session &s);

} // namespace xpp
#endif
