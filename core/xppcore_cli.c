/* xppcore-cli: run an ODE file headlessly using libxppcore only.
   Usage is the same as `xppaut file.ode -silent [options]`; batch mode is
   forced, so -silent may be omitted. */
#include "xpp_batch.h"

int main(int argc, char **argv)
{
    return xpp_batch_main(argc, argv);
}
