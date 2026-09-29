/* The network layer's "fftcon" special function (simplenet.cpp) against
   the matching direct conv/conv0 network on the same weight table (W38:
   this is the documented relationship fftcon's table layout must hold;
   W31a/W32a: run the way `xppautX -silent` runs it, so this also guards
   W32a's fftn->pocketfft swap). fftcon is only reachable by loading a
   model that uses it: tools/models/fftcon_test.ode has a static
   (v_i'=0) 5-cell periodic ("p") fftcon network (k) and a zero-padded
   3-cell one (m), each beside the conv/conv0 network (kc, mc) that
   should equal it on the same table; read back through their aux
   variables' stored columns, never through the FFT directly. */
#include "xpptest.h"
#include "xpp_batch.h"
#include "session.h"
#include "storage.h"
#include "menudrive.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

/* relative error, falling back to absolute when want is ~0 */
static double relerr(double got, double want)
{
    double scale = std::fabs(want) > 1e-9 ? std::fabs(want) : 1.0;
    return std::fabs(got - want) / scale;
}

/* the model's row 2 (t=1) holds t, v0..v4 (columns 0..5), then the aux
   columns in declaration order: k0..k4 (6..10), kc0..kc4 (11..15),
   m0..m2 (16..18), mc0..mc2 (19..21) (the first, t=0, row is stored
   before the networks' first evaluation, so its aux columns are still
   the zero the model started with -- not fftcon's fault, see the
   report). Integrated as -silent integrates it (Initialconds/Go), read
   from the stored columns. Returns 1 on success. */
static int run_fftcon(double k[5], double kc[5], double m[3], double mc[3])
{
    char *argv[] = {const_cast<char *>("test_fftcon"),
                    const_cast<char *>("tools/models/fftcon_test.ode"),
                    NULL};
    if (!xpp_load_model(2, argv, 1)) return 0;
    xpp_batch_start();
    run_the_commands(M_IG);
    const DataStore &d = xpp::session().data_store;
    if (d.rows < 2) return 0;
    auto at = [&d](int col) { return static_cast<double>(d.col[col][1]); }; /* t=1 */
    for (int i = 0; i < 5; i++) k[i] = at(6 + i);
    for (int i = 0; i < 5; i++) kc[i] = at(11 + i);
    for (int i = 0; i < 3; i++) m[i] = at(16 + i);
    for (int i = 0; i < 3; i++) mc[i] = at(19 + i);
    return 1;
}

int main(void)
{
    /* --- fftcon: the periodic (k) and zero-padded (m) network
       convolutions vs. the matching conv/conv0 network (kc, mc) on the
       same weight table (tools/models/fftcon_test.ode); see its header
       comment and simplenet.cpp's update_fft for why they must agree. */
    {
        double k[5], kc[5], m[3], mc[3];
        CHECK(run_fftcon(k, kc, m, mc));
        for (int i = 0; i < 5; i++) CHECK(relerr(k[i], kc[i]) < 1e-9);
        for (int i = 0; i < 3; i++) CHECK(relerr(m[i], mc[i]) < 1e-9);
        /* not all-zero, so a broken update_fft (e.g. a stray zero kernel)
           would not pass by accident */
        double ksum = 0, msum = 0;
        for (int i = 0; i < 5; i++) ksum += std::fabs(k[i]);
        for (int i = 0; i < 3; i++) msum += std::fabs(m[i]);
        CHECK(ksum > 1e-6);
        CHECK(msum > 1e-6);
    }

    TEST_REPORT("fftcon");
}
