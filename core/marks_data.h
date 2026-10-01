#ifndef MARKS_DATA_H
#define MARKS_DATA_H

#include <array>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_types.h"
#include "many_pops.h"
#include "struct.h" /* MAXFRZ */

namespace xpp {
struct Session; /* session.h */

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

   marks_data.cpp; nothing escapes it. */

/* What each plot window was drawn since it was last blanked: a Session's
   (Session::marks_shown), which marks_data.cpp fills; what the client got
   of it is marks_data.cpp's own, the client's. */
struct MarksShown {
    struct Equilibrium {
        double x, y;
        int symbol; /* eq_symb's: 0 box, 1 triangle, 3 circle */
        bool operator==(const Equilibrium &o) const
        {
            return std::memcmp(&x, &o.x, sizeof x) == 0 && std::memcmp(&y, &o.y, sizeof y) == 0 && symbol == o.symbol;
        }
    };
    struct Record {
        std::vector<Equilibrium> eqs;
        std::map<int, std::string> labels;    /* lb[] slot -> the text drawn */
        std::vector<bool> grobs;              /* grob[] slots drawn */
        std::map<int, unsigned long> frozen;  /* frozen_curves.curve[] slot -> its generation */
    };
    std::array<Record, MAXPOP> windows;
    /* each frozen_curves.curve[] slot's generation, bumped when a curve is
       made in it (marks_data_frozen_new): a record keeps the generation it
       drew, so a new curve in the slot is not the one drawn */
    std::array<unsigned long, MAXFRZ> generation{};
    unsigned long generations = 0;
};

typedef void (*MarksDataEmit)(std::string_view line);

/* the front end that sends the events; nothing is recorded before this */
void marks_data_init(MarksDataEmit emit);

/* {"cmd":"data"}: whether "marks" is wanted from now on (sent for every
   window at the next update) and whether values go as base64 float32 */
void marks_data_subscribe(int on, int f32);

/* plot window pop was blanked: it shows none of its marks any more */
void marks_data_cleared(Session &s, int pop);

/* the end of a command on s: the events of every window whose marks
   changed */
void marks_data_update(Session &s);

/* the active plot window of s marks an equilibrium at (x, y)
   (plot coordinates) with eq_symb's symbol: 0 box (unstable), 1 triangle
   (saddle), 3 circle (stable) */
void marks_data_equilibrium(Session &s, double x, double y, int symbol);

/* plot window w of s shows label lb[slot] as `text` (its \{expr} filled in) */
void marks_data_label(Session &s, XppWinId w, int slot, std::string_view text);

/* plot window w of s shows graphic object grob[slot] */
void marks_data_grob(Session &s, XppWinId w, int slot);

/* plot window w of s shows frozen curve frozen_curves.curve[slot]; frozen_new:
   s's curve was just made (a new curve, even in a slot used before), in the
   window it names */
void marks_data_frozen(Session &s, XppWinId w, int slot);
void marks_data_frozen_new(Session &s, int slot);

} // namespace xpp
#endif
