#pragma once
/* The part of an axis the page shows (W65, docs/ui-v2.md "Display state"): a plot window's zoom (display_state.h) and
   autoX's views' (autox_state.h). Pure values. */

namespace xpp {

/* one axis' visible range; unset: the window's own */
struct AxisRange {
    bool set = false;
    double lo = 0, hi = 0;
    bool operator==(const AxisRange &) const = default;
};

/* the zoom shown: the part of a window's (or autoX's) axes the page displays */
struct Zoom {
    AxisRange x, y;
    bool any() const { return x.set || y.set; }
    bool operator==(const Zoom &) const = default;
};

} // namespace xpp
