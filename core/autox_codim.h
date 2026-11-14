#pragma once
#include <vector>
#include <cstddef>
#include "autox_codes.h"

namespace xpp {
// Run-owned work for codimension-two tests; sized before continuation.
struct AutoxCodim {
    std::vector<double> matrix, left, previous_left, qr, qi, pr, pi,
        a, b, c, r, sr, si, x, plus, minus, work, rhs, solution;
    std::vector<double> cycle_matrix, cycle_p0, cycle_p1;
    std::vector<int> cycle_rows, cycle_pivots;
    AutoxPointKind map_kind=AutoxPointKind::Regular;
    void prepare(std::size_t n);
    void prepare_cycle(std::size_t dimension, std::size_t original);
};
AutoxPointKind autox_map_secondary_kind(AutoxPointKind start, double angle, double tolerance);
// The nearest strong resonance of the continued Floquet angle, taken modulo 2*pi.
AutoxPointKind autox_cycle_resonance_kind(AutoxPointKind start, double angle);
// Whether a torus curve's angle, taken modulo 2*pi, is nearest its 1:1 resonance.
bool autox_torus_near_one_to_one(double angle);
// Whether it is nearest its 1:1 or 1:2 resonance, where sin(theta) = 0 and the
// torus equations are singular (W405, W419).
bool autox_torus_singular(double angle);

}
