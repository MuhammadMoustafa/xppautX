/* Known-answer tests for nUmerics > stocHast's Fourier features (menus.cpp
   stoch_hint[]): Fourier series ('f', new_four), power spectrum ('p',
   just_fourier), correlations subtracting the mean ('x', new_hist which=2,
   mycor2) and windowed spectral density ('e', just_sd/spectrum), plus the
   network layer's "fftcon" special function (simplenet.cpp), against
   values computed from first principles here. Nothing tested these
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
   tests/test_lunch.c and this task's brief.

   fftcon is only reachable by loading a model that uses it: tools/models/
   fftcon_test.ode has a static (v_i'=0) 4-cell periodic ("p") network
   whose weight table is zero except one tap, run the way `xppautX
   -silent` runs it (xpp_batch_main), then read back through its aux
   variables (k(i)) in output.dat -- never through fftn/fft_conv
   directly. */
#include "xpptest.h"
#include "xpp_batch.h"
#include "browse.h"
#include "graphics.h"
#include "histogram.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
extern float **storage;
extern int storind;
}
/* my_four/my_hist/HIST_HERE/FOUR_HERE/hist_len/four_len and the spec_*
   globals are histogram.cpp's own (declared the way every other core
   file that uses them already does, e.g. load_eqn.cpp's "extern int
   spec_col,..."); there is no header for them since they are file-scope
   working state, not part of histogram.h's function API. */
extern float *my_four[];
extern float *my_hist[];
extern int FOUR_HERE, HIST_HERE, four_len, hist_len;
extern int spec_col, spec_wid, spec_win, spec_col2, spec_type;

static const double PI = 3.14159265358979323846;

/* relative error, falling back to absolute when want is ~0 */
static double relerr(double got, double want)
{
    double scale = std::fabs(want) > 1e-9 ? std::fabs(want) : 1.0;
    return std::fabs(got - want) / scale;
}

/* read one whitespace-separated double field from a text line */
static double field(const char *line, int col)
{
    const char *p = line;
    for (int i = 0; i < col; i++) {
        while (*p == ' ' || *p == '\t') p++;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    return atof(p);
}

/* the fftcon network: load tools/models/fftcon_test.ode, run it the way
   -silent does, and read k(0..3) back from output.dat's second row (the
   first, t=0, row is written before the network's first evaluation, so
   its aux columns are still the zero the model started with -- not
   fftcon's fault, see the report). Returns 1 on success. */
static int run_fftcon(double k[4])
{
    char *argv[] = {const_cast<char *>("test_stochast_fourier"),
                    const_cast<char *>("tools/models/fftcon_test.ode"),
                    const_cast<char *>("-outfile"),
                    const_cast<char *>("build/test_stochast_fftcon_output.dat"),
                    NULL};
    if (xpp_batch_main(4, argv) != 0) return 0;
    FILE *fp = fopen("build/test_stochast_fftcon_output.dat", "r");
    if (!fp) return 0;
    char line[512];
    int row = 0;
    int ok = 0;
    while (fgets(line, sizeof line, fp)) {
        row++;
        if (row == 2) { /* t=1 */
            for (int i = 0; i < 4; i++) k[i] = field(line, 5 + i);
            ok = 1;
            break;
        }
    }
    fclose(fp);
    return ok;
}

int main(void)
{
    char *argv[] = {const_cast<char *>("test_stochast_fourier"),
                     const_cast<char *>("tools/models/stochast_fourier_test.ode"), NULL};
    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();

    const int N = 64;
    storind = N;
    for (int i = 0; i < N; i++) storage[0][i] = static_cast<float>(i);

    /* --- Fourier series (f): a pure cosine at mode m has ct[m]=A,
       st[m]=0, and every other mode near 0 (new_four is the exact
       function compute_fourier() calls after its column prompt). */
    {
        const double A = 3.0;
        const int m = 5;
        for (int i = 0; i < N; i++)
            storage[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / N));
        int nmodes = N / 2 - 1;
        new_four(nmodes, 1);
        CHECK(FOUR_HERE == 1);
        CHECK(four_len == nmodes);
        CHECK(relerr(my_four[1][m], A) < 1e-4);
        CHECK(std::fabs(my_four[2][m]) < 1e-3);
        /* a bin away from the signal's mode carries none of it */
        CHECK(std::fabs(my_four[1][m + 3]) < 1e-3);
        CHECK(std::fabs(my_four[2][m + 3]) < 1e-3);
    }

    /* --- Power spectrum/phase (p): magnitude sqrt(c^2+s^2) is the
       Parseval amplitude sqrt(A^2+B^2) regardless of the sign
       convention fft() gives the sine term (just_fourier(1) is what
       compute_power() calls after compute_fourier(), doing the same
       column). */
    {
        const double A = 3.0, B = 4.0;
        const int m = 5;
        spec_col = 1;
        for (int i = 0; i < N; i++)
            storage[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / N) +
                                                B * std::sin(2 * PI * m * i / N));
        just_fourier(1);
        CHECK(FOUR_HERE == 1);
        /* just_fourier(1) converts my_four[1]/[2] from (cos,sin) coefficients
           to (magnitude,phase) in place (four_back() inside new_four() points
           get_data_col() at my_four first): my_four[1][m] IS the magnitude. */
        CHECK(relerr(my_four[1][m], std::sqrt(A * A + B * B)) < 1e-4);
        /* neighbouring bins hold none of this single-mode signal's power */
        CHECK(my_four[1][m + 3] < 1e-3);
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
            storage[1][i] = static_cast<float>(x[i]);
        }
        for (int i = 0; i < N; i++)
            storage[2][i] = static_cast<float>(x[((i - L) % N + N) % N]);
        int nbins = N;
        new_hist(nbins, 0.0, 1.0, 1, 2, "", 2);
        CHECK(HIST_HERE == 1);
        int lag = nbins / 2;
        int worst_ok = 1;
        for (int j = 0; j <= nbins; j++) {
            double want = (A * A / 2.0) * std::cos(2 * PI * m * (j - lag - L) / N);
            if (relerr(my_hist[1][j], want) > 5e-4 &&
                std::fabs(my_hist[1][j] - want) > 5e-4)
                worst_ok = 0;
        }
        CHECK(worst_ok);
        /* the peak (lag == L) reaches the full A^2/2 */
        CHECK(relerr(my_hist[1][lag + L], A * A / 2.0) < 5e-4);
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
        storind = total;
        for (int i = 0; i < total; i++)
            storage[1][i] = static_cast<float>(A * std::cos(2 * PI * m * i / win));
        spec_col = 1;
        spec_wid = win;
        spec_win = 0; /* square */
        just_sd(0);
        CHECK(HIST_HERE == 1);
        CHECK(hist_len == win / 2);
        CHECK(relerr(my_hist[1][m], A) < 1e-3);
        CHECK(my_hist[1][m + 3] < 1e-2); /* no leakage to a bin away */
    }

    /* --- fftcon: periodic network convolution vs. a direct circular sum
       computed here. tools/models/fftcon_test.ode's weight table is zero
       except a single tap (2 at t=3 of 0..4); by the weight-table layout
       simplenet.cpp's update_fft documents (fftr[i]=w[i+n2] for
       i=0..n2, fftr[n2+i+1]=w[i] for i=0..n2-1, n2=n/2), that tap lands
       at kernel offset +1, so k(i) should equal c*v[(i-1) mod n]. */
    {
        double k[4];
        CHECK(run_fftcon(k));
        double v[4] = {1, 2, 3, 4};
        double c = 2.0;
        for (int i = 0; i < 4; i++) {
            double want = c * v[((i - 1) % 4 + 4) % 4];
            CHECK(relerr(k[i], want) < 1e-9);
        }
    }

    TEST_REPORT("stochast fourier/fftcon");
}
