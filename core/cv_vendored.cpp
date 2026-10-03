/* cv_backend.h's CVODE, the vendored one (cvode.h and its own C API) */
#include "cv_backend.h"
#include "cv2.h"
#include "session.h"
#include "xpp_log.h"

#include <array>
#include "my_rhs.h"

                      /* integer (set to int), and the constant FALSE      */
                      /* constants OPT_SIZE, BDF, NEWTON, SV, SUCCESS,     */
                      /* NST, NFE, NSETUPS, NNI, NCFN, NETF                */
#include "cvdense.h"  /* prototype for CVDense, constant DENSE_NJE         */

                      /* prototypes for N_VNew, N_VFree                    */
#include "cvband.h"
#include "numerics.h"
#include "model.h"

namespace xpp {

namespace {
/* both linear solvers count their Jacobian evaluations in the one slot */
static_assert(static_cast<int>(BAND_NJE)==static_cast<int>(DENSE_NJE));
void cvf(int n, double t, N_Vector y, N_Vector ydot, void *fdata);

struct VendoredMemory final : CvodeMemory {
  N_Vector y;
  void *cvode;
  /* CVODE's optional inputs and outputs (cvode.h's iopt, ropt), which it
     points at while it runs */
  std::array<int,OPT_SIZE> iopt{};
  std::array<double,OPT_SIZE> ropt{};
  Session &session;
  long rhs_calls=0;

  VendoredMemory(Session &s, const double *y0, double t, int n, double *atol, double *rtol)
    : y(N_VNew(n,NULL)), session(s)
  {
    for(int i=0;i<n;i++)y->data[i]=y0[i];
    cvode=CVodeMalloc(n, cvf, t, y, BDF, NEWTON, SS, rtol, atol,
                      this, FALSE, iopt.data(), ropt.data(), NULL);
    if(s.numerics.cv_bandflag==1)
      CVBand(cvode,s.numerics.cv_bandupper,s.numerics.cv_bandlower,NULL,NULL);
    else
      CVDense(cvode, NULL, NULL);
  }
  ~VendoredMemory() override
  {
    xpp::log(XPP_LOG_DEBUG, "cvode stats: nst={} nfe={} rhs={} nje={} nsetups={} nni={} ncfn={} netf={}\n",
             iopt[NST],iopt[NFE],rhs_calls,iopt[DENSE_NJE],iopt[NSETUPS],iopt[NNI],iopt[NCFN],iopt[NETF]);
    N_VFree(y);
    CVodeFree(cvode);
  }

  int step(CvodeRun &run, double tout, double *t, double *yout) override
  {
    const int flag=CVode(cvode,tout,y,t,NORMAL);
    if(flag != SUCCESS){
      /* why it failed, from CVODE's memory before it is freed */
      const CVodeMem m=static_cast<CVodeMem>(cvode);
      run.error=m->cv_error;
      run.error_var=m->cv_error_var;
      run.tolerance_factor=ropt[ROPT_TOLSF];
      return flag;
    }
    for(long i=0;i<y->length;i++)yout[i]=y->data[i];
    return 0;
  }
};

void cvf(int n, double t, N_Vector y, N_Vector ydot, void *fdata)
{
  VendoredMemory &m=*static_cast<VendoredMemory *>(fdata);
  m.rhs_calls++;
  my_rhs(m.session,t,y->data,ydot->data,n);
}
}

std::unique_ptr<CvodeMemory> start_cvode_memory(Session &s, const double *y, double t, int n, double *atol, double *rtol)
{
  return std::make_unique<VendoredMemory>(s,y,t,n,atol,rtol);
}

} // namespace xpp
