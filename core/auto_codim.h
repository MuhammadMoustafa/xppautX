#ifndef AUTO_CODIM_H
#define AUTO_CODIM_H
#include <vector>
#include "auto_codes.h"

namespace xpp {
// Run-owned work for codimension-two tests; sized before continuation.
struct AutoCodim {
    std::vector<double> matrix, left, previous_left, qr, qi, pr, pi,
        a, b, c, r, sr, si, x, plus, minus, work, rhs, solution;
    std::vector<double> cycle_matrix, cycle_p0, cycle_p1;
    std::vector<int> cycle_rows, cycle_pivots;
    AutoPointKind map_kind=AutoPointKind::Regular;
    void prepare(std::size_t n);
    void prepare_cycle(std::size_t dimension, std::size_t original);
};
AutoPointKind auto_map_secondary_kind(AutoPointKind start, double angle, double tolerance);
// The nearest strong resonance of the continued (unwrapped) Floquet angle.
AutoPointKind auto_cycle_resonance_kind(AutoPointKind start, double angle);
// Whether a torus curve's angle, taken modulo 2*pi, is nearest its 1:1 resonance.
bool auto_torus_near_one_to_one(double angle);

}
#endif
