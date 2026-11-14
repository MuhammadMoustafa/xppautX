#pragma once

#include "xpp_error.h"
#include "plot_state.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
namespace xpp { namespace odex { class Block; } namespace odex { class Out; } }

namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */

/* the nullclines (current, or frozen number who) as segments: 4 floats each */
int get_nullcline_floats(Session &s, float **v, int *n, int who, int type);




/* The nullclines and the direction field of the session s's active plot
   window: its commands, and redrawn with it */
/* The current nullclines and the frozen ones as the group `nullclines` of a session statement being written: per
   pair, the variables on its axes and its x and y segments, four finite coordinates each. Saved segments are read
   whole (odex_block.h's Block over that group, every number checked, at most a cell of the largest mesh a segment)
   before the session applies any of it. */
void write_nullclines(odex::Out &out, const Session &s);
NullclineState read_nullclines(const Model &m, odex::Block &nullclines);
void create_new_cline(Session &s);
void freeze_nullclines(Session &s);
void clear_frozen_nullclines(Session &s);
void redraw_dfield(Session &s);
Result<> draw_direction_field(Session &s, int c, int grid);
void range_nullclines(Session &s);
bool animate_nullclines(Session &s, int delay_ms);
Result<int> export_nullclines(Session &s, const std::string &file, std::string_view format, bool confirm);
bool restore_nullclines(Session &s);
void compute_nullclines(Session &s);
void do_batch_nclines(Session &s);
void do_batch_dfield(Session &s);
/* the direction field the current window shows, one arrow a line (x y
   and the arrow's end), in PostScript's frame (--silent's dirfields.dat,
   the protocol's `dfield` `write`); an error message when it shows none */
Result<> write_dfield(Session &s, std::string_view name, bool confirm = false);
/* the orbits' coloring (@ colorize=, colorvia=) of s in use */
void set_colorization_stuff(Session &s);

} // namespace xpp
