/* The network layer's "fftcon" special function (simplenet.cpp) against a
   direct circular convolution computed here (W31a; its own process, since
   the core loads one model per process). fftcon is only reachable by
   loading a model that uses it: tools/models/fftcon_test.ode has a static
   (v_i'=0) 4-cell periodic ("p") network whose weight table is zero except
   one tap, run the way `xppautX -silent` runs it (xpp_batch_main), then
   read back through its aux variables (k(i)) in output.dat -- never
   through the FFT directly, so W32a's change of FFT keeps this test. */
#include "xpptest.h"
#include "xpp_batch.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    char *argv[] = {const_cast<char *>("test_fftcon"),
                    const_cast<char *>("tools/models/fftcon_test.ode"),
                    const_cast<char *>("-outfile"),
                    const_cast<char *>("build/test_fftcon_output.dat"),
                    NULL};
    if (xpp_batch_main(4, argv) != 0) return 0;
    FILE *fp = fopen("build/test_fftcon_output.dat", "r");
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

    TEST_REPORT("fftcon");
}
