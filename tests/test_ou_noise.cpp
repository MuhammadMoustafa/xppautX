/* The ou declaration's statistics (W380): an Ornstein-Uhlenbeck process
   with tau = 1 and sigma = 2, stepped at dt = 0.05 (dt << tau, tau and sigma
   parameters) and at dt = 1 (dt = tau, numbers), from fixed seeds, 100001
   samples each, one per step (tools/models/ou_small_dt.odex,
   ou_large_dt.odex). The process is stationary from its first sample, so
   the mean is 0, the variance sigma^2 tau/2 = 2 and the autocorrelation at
   lag k rho^k, with rho = exp(-dt/tau).

   The samples are correlated, so a tolerance is not the independent-sample
   one: it is 6 standard deviations of the estimate under an AR(1) process
   of that rho (the process sampled at dt is one, exactly), by the large-n
   formulas
     mean:        var (1 + rho) / ((1 - rho) n)
     variance:    2 var^2 (1 + rho^2) / ((1 - rho^2) n)  (Gaussian)
     lag k:       ((1 + rho^2)(1 - rho^(2k)) / (1 - rho^2) - 2 k rho^(2k)) / n  (Bartlett)
   so a run with another seed fails with a probability of about 1e-9 per
   check, never by chance. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "storage.h"
#include "browse.h"
#include "integrate.h"
#include "xpp_batch.h"
#include "menudrive.h"

#include <cmath>
#include <vector>

namespace {

constexpr double TAU = 1.0, SIGMA = 2.0;
constexpr double SIGMAS = 6.0; /* the tolerance, in standard deviations of the estimate */

/* the model run once; its eta samples */
std::vector<double> samples(const char *model)
{
    char arg0[] = "test_ou_noise";
    std::string path = model;
    char *argv[] = {arg0, path.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::batch_start(xpp::client_session());
    run_the_commands(xpp::client_session(), xpp::M_IG);
    xpp::Session &s = xpp::client_session();
    int col = -1;
    find_variable(s, "sample", &col);
    CHECK(col > 0);
    CHECK(s.data_store.rows > 100000);
    return std::vector<double>(s.data_store.col[col], s.data_store.col[col] + s.data_store.rows);
}

void check_statistics(const char *model, double dt)
{
    const std::vector<double> e = samples(model);
    const double n = static_cast<double>(e.size());
    const double rho = std::exp(-dt / TAU);
    const double var = SIGMA * SIGMA * TAU / 2;

    double mean = 0;
    for (double v : e) mean += v;
    mean /= n;
    CHECK(std::fabs(mean) < SIGMAS * std::sqrt(var * (1 + rho) / ((1 - rho) * n)));

    double c0 = 0;
    for (double v : e) c0 += (v - mean) * (v - mean);
    c0 /= n;
    CHECK(std::fabs(c0 - var) < SIGMAS * var * std::sqrt(2 * (1 + rho * rho) / ((1 - rho * rho) * n)));

    for (int k : {1, 2, 5}) {
        double ck = 0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(k) < e.size(); i++) ck += (e[i] - mean) * (e[i + static_cast<std::size_t>(k)] - mean);
        ck /= n;
        const double r2k = std::pow(rho, 2 * k);
        const double sd = std::sqrt(((1 + rho * rho) * (1 - r2k) / (1 - rho * rho) - 2 * k * r2k) / n);
        CHECK(std::fabs(ck / c0 - std::pow(rho, k)) < SIGMAS * sd);
    }
}

} // namespace

int main(void)
{
    check_statistics("tools/models/ou_small_dt.odex", 0.05);
    check_statistics("tools/models/ou_large_dt.odex", 1.0);
    TEST_REPORT("ou noise statistics");
}
