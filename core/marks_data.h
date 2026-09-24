#ifndef MARKS_DATA_H
#define MARKS_DATA_H
#include <stddef.h>
#include "xpp_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* What a plot window shows on top of its curves, as data for a front end
   that draws it itself: the "marks" event (docs/protocol.md "The plot as
   data", docs/ui-v2.md event 6):

   - the equilibria Sing pts marked on the plot (eq_symb in graphics.c),
     with the stability its symbol says;
   - Text,etc's text labels, arrows, pointers and markers (grobs.cpp);
   - Graphic stuff/Freeze's frozen curves (graf_par.c).

   As phase_data.h does for nullclines, each window's record is what the
   core drew in it since it was last blanked: the drawing code reports what
   it draws, the front end reports a blanked window. Labels, graphic
   objects and frozen curves are kept by their slot (lb[], grob[], frozen_curves.curve[]),
   so drawing one again changes nothing, and at the end of a command a slot
   no longer in use (deleted) or moved to another window is left out; their
   content is read from the slot then. Events go out only to a client that
   subscribed, at the end of a command, one per window whose content
   changed since the one it last got.

   marks_data.cpp; C++ with a C API, nothing escapes it. */

typedef void (*MarksDataEmit)(const char *line, size_t len);

/* the front end that sends the events; nothing is recorded before this */
void marks_data_init(MarksDataEmit emit);

/* {"cmd":"data"}: whether "marks" is wanted from now on (sent for every
   window at the next update) and whether values go as base64 float32 */
void marks_data_subscribe(int on, int f32);

/* the end of a command: the events of every window whose marks changed */
void marks_data_update(void);

/* plot window pop was blanked: it shows none of its marks any more */
void marks_data_cleared(int pop);

/* the current window marks an equilibrium at (x, y) (plot coordinates)
   with eq_symb's symbol: 0 box (unstable), 1 triangle (saddle), 3 circle
   (stable) */
void marks_data_equilibrium(double x, double y, int symbol);

/* window w shows label lb[slot] as `text` (its \{expr} filled in) */
void marks_data_label(XppWinId w, int slot, const char *text);

/* window w shows graphic object grob[slot] */
void marks_data_grob(XppWinId w, int slot);

/* window w shows frozen curve frozen_curves.curve[slot]; frozen_new: it was just
   made (a new curve, even in a slot used before), in the window it names */
void marks_data_frozen(XppWinId w, int slot);
void marks_data_frozen_new(int slot);

#ifdef __cplusplus
}
#endif
#endif
