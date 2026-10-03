/* cv_backend.h's CVODE, SUNDIALS' (W34, an evaluation: `make SUNDIALS=DIR`):
   the calls the vendored CVODE's C API gets in cv_vendored.cpp, mapped to
   SUNDIALS' own. BDF with a Newton iteration and a dense or banded
   difference-quotient Jacobian, scalar tolerances, 2000 steps per call,
   order 5: the vendored CVODE's own settings. */
#include "cv_backend.h"
#include "cv2.h"
#include "session.h"
#include "xpp_log.h"
#include "xpp_io.h"

#include "my_rhs.h"
#include "numerics.h"
#include "model.h"

#include <cvode/cvode.h>
#include <cvode/cvode_ls.h>
#include <nvector/nvector_serial.h>
#include <sunlinsol/sunlinsol_band.h>
#include <sunlinsol/sunlinsol_dense.h>
#include <sunmatrix/sunmatrix_band.h>
#include <sunmatrix/sunmatrix_dense.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace xpp {

namespace {
/* the vendored CVODE's own limit of internal steps per call (cvode.cpp's
   MXSTEP_DEFAULT; cvode.h's comment says 500) */
constexpr long max_steps_per_call = 2000;

/* what a SUNDIALS failure flag is, as the flag cvode_error_text words
   (the vendored CVODE's own numbering) */
int vendored_flag(int flag)
{
  switch(flag){
  case CV_TOO_MUCH_WORK: return -3;
  case CV_TOO_MUCH_ACC: return -4;
  case CV_ERR_FAILURE: return -5;
  case CV_CONV_FAILURE: return -6;
  case CV_LSETUP_FAIL: return -7;
  case CV_LSOLVE_FAIL: return -8;
  default: return -2; /* bad input and the rest */
  }
}

struct SundialsMemory final : CvodeMemory {
  SUNContext ctx=nullptr;
  N_Vector y=nullptr;
  SUNMatrix matrix=nullptr;
  SUNLinearSolver solver=nullptr;
  void *cvode=nullptr;
  Session &session;
  long rhs_calls=0;
  /* the text of SUNDIALS' last error message, for the failure */
  std::string message;

  static int rhs(sunrealtype t, N_Vector y, N_Vector ydot, void *data)
  {
    SundialsMemory &m=*static_cast<SundialsMemory *>(data);
    m.rhs_calls++;
    my_rhs(m.session,t,N_VGetArrayPointer(y),N_VGetArrayPointer(ydot),static_cast<int>(N_VGetLength(y)));
    return 0;
  }
  /* SUNDIALS' errors and warnings, kept for the failure and logged */
  static void on_message(int, const char *func, const char *, const char *msg, SUNErrCode code, void *data, SUNContext)
  {
    SundialsMemory &m=*static_cast<SundialsMemory *>(data);
    m.message=msg;
    xpp::log(code<0?XPP_LOG_WARN:XPP_LOG_INFO, "{}: {}\n", func, msg);
  }

  SundialsMemory(Session &s, const double *y0, double t, int n, double *atol, double *rtol)
    : session(s)
  {
    SUNContext_Create(SUN_COMM_NULL,&ctx);
    y=N_VNew_Serial(n,ctx);
    std::copy(y0,y0+n,N_VGetArrayPointer(y));
    cvode=CVodeCreate(CV_BDF,ctx);
    CVodeInit(cvode,rhs,t,y);
    CVodeSetUserData(cvode,this);
    SUNContext_PushErrHandler(ctx,on_message,this);
    /* the vendored CVODE keeps pointers to the two tolerances of the
       Session; a run never changes them, so the values are the same */
    CVodeSStolerances(cvode,*rtol,*atol);
    CVodeSetMaxNumSteps(cvode,max_steps_per_call);
    if(s.numerics.cv_bandflag==1){
      matrix=SUNBandMatrix(n,s.numerics.cv_bandupper,s.numerics.cv_bandlower,ctx);
      solver=SUNLinSol_Band(y,matrix,ctx);
    }else{
      matrix=SUNDenseMatrix(n,n,ctx);
      solver=SUNLinSol_Dense(y,matrix,ctx);
    }
    CVodeSetLinearSolver(cvode,solver,matrix);
  }
  ~SundialsMemory() override
  {
    long nst=0,nfe=0,nje=0,nsetups=0,nni=0,ncfn=0,netf=0;
    CVodeGetNumSteps(cvode,&nst);
    CVodeGetNumRhsEvals(cvode,&nfe);
    CVodeGetNumJacEvals(cvode,&nje);
    CVodeGetNumLinSolvSetups(cvode,&nsetups);
    CVodeGetNumNonlinSolvIters(cvode,&nni);
    CVodeGetNumNonlinSolvConvFails(cvode,&ncfn);
    CVodeGetNumErrTestFails(cvode,&netf);
    xpp::log(XPP_LOG_DEBUG, "cvode stats: nst={} nfe={} rhs={} nje={} nsetups={} nni={} ncfn={} netf={}\n",
             nst,nfe,rhs_calls,nje,nsetups,nni,ncfn,netf);
    CVodeFree(&cvode);
    SUNLinSolFree(solver);
    SUNMatDestroy(matrix);
    N_VDestroy(y);
    SUNContext_Free(&ctx);
  }

  /* the index of the variable the weighted local error is the largest at */
  long worst_variable()
  {
    N_Vector ele=N_VClone(y),ewt=N_VClone(y);
    long worst=-1;
    if(CVodeGetEstLocalErrors(cvode,ele)==CV_SUCCESS&&CVodeGetErrWeights(cvode,ewt)==CV_SUCCESS){
      double best=-1;
      const sunindextype n=N_VGetLength(y);
      for(sunindextype i=0;i<n;i++){
        const double e=std::fabs(N_VGetArrayPointer(ele)[i]*N_VGetArrayPointer(ewt)[i]);
        if(e>best){best=e;worst=static_cast<long>(i);}
      }
    }
    N_VDestroy(ele);
    N_VDestroy(ewt);
    return worst;
  }

  int step(CvodeRun &run, double tout, double *t, double *yout) override
  {
    message.clear();
    const int flag=CVode(cvode,tout,y,t,CV_NORMAL);
    if(flag<0){
      run.error=message;
      run.error_var=(flag==CV_ERR_FAILURE||flag==CV_CONV_FAILURE)?worst_variable():-1;
      sunrealtype tolsf=1;
      CVodeGetTolScaleFactor(cvode,&tolsf);
      run.tolerance_factor=tolsf;
      return vendored_flag(flag);
    }
    std::copy(N_VGetArrayPointer(y),N_VGetArrayPointer(y)+N_VGetLength(y),yout);
    return 0;
  }
};
}

std::unique_ptr<CvodeMemory> start_cvode_memory(Session &s, const double *y, double t, int n, double *atol, double *rtol)
{
  return std::make_unique<SundialsMemory>(s,y,t,n,atol,rtol);
}

} // namespace xpp
