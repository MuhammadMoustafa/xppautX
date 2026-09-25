/* The stability values of AUTO's points: what AUTO computed, for which
   point, and what a run's first point takes (auto_stability.h). */
#include <cmath>
#include <cstdlib>
#include <vector>

#include "auto_stability.h"

namespace {

/* one point's values, re and im */
struct Values {
    int br = 0, pt = 0; /* the point (abs), 0: none */
    std::vector<double> re, im;
};

Values computed; /* the last point AUTO checked */
Values first;    /* the running restart's first point (pt 1), when it has one */
bool has_first;

/* the n values v into evr/evi, zeros past what v holds */
void copy_out(const Values &v, int n, double *evr, double *evi)
{
    for (int i = 0; i < n; i++) {
        bool in = static_cast<size_t>(i) < v.re.size();
        evr[i] = in ? v.re[static_cast<size_t>(i)] : 0.0;
        evi[i] = in ? v.im[static_cast<size_t>(i)] : 0.0;
    }
}

void forget(Values &v)
{
    v.br = v.pt = 0;
    v.re.clear();
    v.im.clear();
}

/* AUTO's type of a period doubling (itp mod 10) */
constexpr int ITP_PD = 7;

} // namespace

extern "C" void auto_stability_computed(int br, int pt, int n, const double *values, int kind)
{
    try {
        computed.br = std::abs(br);
        computed.pt = std::abs(pt);
        computed.re.assign(static_cast<size_t>(n > 0 ? n : 0), 0.0);
        computed.im.assign(computed.re.size(), 0.0);
        for (int i = 0; i < n; i++) {
            double r = values[2 * i], im = values[2 * i + 1];
            if (kind == AUTO_STABILITY_STEADY) { /* e^lambda */
                double er = std::exp(r);
                computed.re[static_cast<size_t>(i)] = er * std::cos(im);
                computed.im[static_cast<size_t>(i)] = er * std::sin(im);
            } else {
                computed.re[static_cast<size_t>(i)] = r;
                computed.im[static_cast<size_t>(i)] = im;
            }
        }
    } catch (...) {
        forget(computed);
    }
}

extern "C" int auto_stability_run_start(int run, int isw, int label, int label_itp, int n, const double *evr,
                                        const double *evi)
{
    forget(computed);
    forget(first);
    has_first = false;
    bool same = label != AUTO_STABILITY_NONE && label == run &&
                (run == AUTO_STABILITY_STEADY || run == AUTO_STABILITY_PERIODIC) &&
                !(run == AUTO_STABILITY_PERIODIC && isw == -1 && std::abs(label_itp) % 10 == ITP_PD);
    if (!same || !evr || !evi || n <= 0) return 0;
    try {
        first.pt = 1;
        first.re.assign(evr, evr + n);
        first.im.assign(evi, evi + n);
        has_first = true;
    } catch (...) {
        forget(first);
    }
    return has_first ? 1 : 0;
}

extern "C" int auto_stability_for(int br, int pt, int n, double *evr, double *evi)
{
    if (computed.pt != 0 && computed.br == std::abs(br) && computed.pt == std::abs(pt)) {
        copy_out(computed, n, evr, evi);
        return 1;
    }
    if (has_first && std::abs(pt) == first.pt) {
        copy_out(first, n, evr, evi);
        return 1;
    }
    copy_out(Values{}, n, evr, evi);
    return 0;
}
