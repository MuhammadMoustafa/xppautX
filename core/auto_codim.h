#ifndef AUTO_CODIM_H
#define AUTO_CODIM_H
#include <vector>
#include "auto_codes.h"

namespace xpp {
// Run-owned work for equilibrium-curve tests; sized before continuation.
struct AutoCodim {
    std::vector<double> matrix, left, previous_left, qr, qi, pr, pi,
        a, b, c, r, sr, si, x, plus, minus, work, rhs, solution;
    void prepare(std::size_t n);
};
}
#endif
