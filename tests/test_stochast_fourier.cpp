/* Known-answer tests for nUmerics > stocHast's Fourier features (menus.cpp
   stoch_hint[]): Fourier series ('f', new_four), power spectrum ('p',
   just_fourier), correlations subtracting the mean ('x', new_hist which=2,
   mycor2) and windowed spectral density ('e', just_sd/spectrum), against
   values computed from first principles here (fftcon: test_fftcon.cpp,
   since a process loads one model). Nothing tested these
   before; W32a will replace fftn's internals with pocketfft, and these
   checks (tolerances, not exact bytes) are the guard that its output
   still means the same thing.

   The Fourier/power/correlation/spectral-density checks call the same
   core functions the stocHast menu calls (do_stochast_com in
   markov.cpp), skipping only the interactive prompts: new_int/new_float/
   new_string_of are no-ops in headless mode (core/xpp_ui.cpp's
   hl_new_string returns 0 without touching the value), so the plain
   globals compute_fourier() & co. would have prompted for (spec_col,
   spec_wid, spec_win, ...) are simply set directly first. The signal
   lives in storage[][], filled directly per the harness style of
   tests/test_lunch.c. */
#include "xpptest.h"
#include "session.h"
#include "storage.h"
#include "xpp_batch.h"
#include "browse.h"
#include "graphics.h"
#include "histogram.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const double PI = 3.14159265358979323846;

/* relative error, falling back to absolute when want is ~0 */
static double relerr(double got, double want)
{
    double scale = std::fabs(want) > 1e-9 ? std::fabs(want) : 1.0;
    return std::fabs(got - want) / scale;
}

int main(void)
{
    char *argv[] = {const_cast<char *>("test_stochast_fourier"),
                     const_cast<char *>("tools/models/stochast_fourier_test.ode"), NULL};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    init_browser(xpp::client_session());
    init_all_graph(xpp::client_session());

    const int N = 64;
    xpp::client_session().data_store.rows = N;
    for (int i = 0; i < N; i++) xpp::client_session().data_store.col[0][i] = static_cast<float>(i);

    /* --- Fourier series (f): a pure cosine at mode m has ct[m]=A,
       st[m]=0, and every other mode near 0 (new_four is the exact
       function compute_fourier() calls after its column prompt). */
    {
        const double A = 3.0;
        const int m = 5;
        for (int i = 0; i < N; i++)
            xpp::client_session().data_store.col[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / N));
        int nmodes = N / 2 - 1;
        xpp::new_four(xpp::client_session(),nmodes, 1);
        CHECK(xpp::client_session().histogram.four_here == 1);
        CHECK(xpp::client_session().histogram.four_len == nmodes);
        CHECK(relerr(xpp::client_session().histogram.four()[1][m], A) < 1e-4);
        CHECK(std::fabs(xpp::client_session().histogram.four()[2][m]) < 1e-3);
        /* a bin away from the signal's mode carries none of it */
        CHECK(std::fabs(xpp::client_session().histogram.four()[1][m + 3]) < 1e-3);
        CHECK(std::fabs(xpp::client_session().histogram.four()[2][m + 3]) < 1e-3);
    }

    /* --- Power spectrum/phase (p): magnitude sqrt(c^2+s^2) is the
       Parseval amplitude sqrt(A^2+B^2) regardless of the sign
       convention fourier_modes() gives the sine term (just_fourier(1) is what
       compute_power() calls after compute_fourier(), doing the same
       column). */
    {
        const double A = 3.0, B = 4.0;
        const int m = 5;
        xpp::client_session().histogram.spec_col = 1;
        for (int i = 0; i < N; i++)
            xpp::client_session().data_store.col[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / N) +
                                                B * std::sin(2 * PI * m * i / N));
        xpp::just_fourier(xpp::client_session(),1);
        CHECK(xpp::client_session().histogram.four_here == 1);
        /* just_fourier(1) converts my_four[1]/[2] from (cos,sin) coefficients
           to (magnitude,phase) in place (four_back() inside new_four() points
           get_data_col() at my_four first): my_four[1][m] IS the magnitude. */
        CHECK(relerr(xpp::client_session().histogram.four()[1][m], std::sqrt(A * A + B * B)) < 1e-4);
        /* neighbouring bins hold none of this single-mode signal's power */
        CHECK(xpp::client_session().histogram.four()[1][m + 3] < 1e-3);
    }

    /* --- Correlations, subtracting the mean (x): new_hist(...,which=2)
       -> mycor2 is a circular cross-correlation of columns col/col2. With
       col2 a copy of col delayed by L samples, mycor2's own definition
       (k=(i+j-lag+n)%n, lag=nbins/2) gives, for a period-N/m sinusoid of
       amplitude A, R(j) = (A^2/2)*cos(2*pi*m*(j-lag-L)/N) -- the standard
       circular-autocorrelation identity, evaluated at the shifted lag. */
    {
        const double A = 2.0;
        const int m = 4, L = 10;
        std::vector<double> x(N);
        for (int i = 0; i < N; i++) {
            x[i] = A * std::sin(2 * PI * m * i / N);
            xpp::client_session().data_store.col[1][i] = static_cast<float>(x[i]);
        }
        for (int i = 0; i < N; i++)
            xpp::client_session().data_store.col[2][i] = static_cast<float>(x[((i - L) % N + N) % N]);
        int nbins = N;
        CHECK(xpp::new_hist(xpp::client_session(),nbins, 0.0, 1.0, 1, 2, "", 2).has_value());
        CHECK(xpp::client_session().histogram.hist_here == 1);
        int lag = nbins / 2;
        int worst_ok = 1;
        for (int j = 0; j <= nbins; j++) {
            double want = (A * A / 2.0) * std::cos(2 * PI * m * (j - lag - L) / N);
            if (relerr(xpp::client_session().histogram.hist()[1][j], want) > 5e-4 &&
                std::fabs(xpp::client_session().histogram.hist()[1][j] - want) > 5e-4)
                worst_ok = 0;
        }
        CHECK(worst_ok);
        /* the peak (lag == L) reaches the full A^2/2 */
        CHECK(relerr(xpp::client_session().histogram.hist()[1][lag + L], A * A / 2.0) < 5e-4);
    }

    /* --- Windowed spectral density (e): just_sd(0) is what compute_sd()
       calls after its prompts (PSDx, column, window length/type). A
       square window (type 0) applies no taper, so a signal periodic in
       the window length reproduces the plain FFT amplitude at its mode,
       exactly (Parseval, no window-normalisation correction needed). */
    {
        const double A = 2.5;
        const int win = 64, m = 5;
        const int total = 256;
        xpp::client_session().data_store.rows = total;
        for (int i = 0; i < total; i++)
            xpp::client_session().data_store.col[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / win));
        xpp::client_session().histogram.spec_col = 1;
        xpp::client_session().histogram.spec_wid = win;
        xpp::client_session().histogram.spec_win = 0; /* square */
        xpp::just_sd(xpp::client_session(),0);
        CHECK(xpp::client_session().histogram.hist_here == 1);
        CHECK(xpp::client_session().histogram.hist_len == win / 2);
        CHECK(relerr(xpp::client_session().histogram.hist()[1][m], A) < 1e-3);
        CHECK(xpp::client_session().histogram.hist()[1][m + 3] < 1e-2); /* no leakage to a bin away */
    }

    TEST_REPORT("stochast fourier");
}
