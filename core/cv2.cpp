#include "cv2.h"
#include "cv_backend.h"
#include "session.h"
#include "xpp_ui.h"

#include "flags.h"

#include "xpp_io.h"
#include "load_eqn.h"
#include "numerics.h"
#include <string>
#include "model.h"

namespace xpp {

CvodeRun::CvodeRun()=default;
CvodeRun::~CvodeRun()=default;

namespace {
/* a fresh integration of run from y at t: the memory of one before, if
   it never ended, freed first */
void start_cv(xpp::Session &s, CvodeRun &run, double *y, double t, int n, double *atol, double *rtol)
{
  run.memory.reset();
  run.memory=start_cvode_memory(s,y,t,n,atol,rtol);
}
}

void end_cv(CvodeRun &run)
{
  run.memory.reset();
}

std::string cvode_error_text(const xpp::Session &s, const CvodeRun &run, int kflag)
{
  std::string text;
  switch(kflag){
  case 0: break;
  case -1: text = "No memory allocated";
    break;
  case -2: text = "Bad input to CVode";
    break;
  case -3: text = "Too much work -- try smaller DT";
    break;
  case -4: text = xpp::format("Tolerance too low-- try TOL={} ATOL={}",
	s.numerics.toler*run.tolerance_factor, s.numerics.atoler*run.tolerance_factor);
    break;
  case -5: text = "Error test failure too frequent ??";
    break;
  case -6: text = "Converg. failure -- oh well!";
    break;
  case -7: text = "Setup failed for linsolver in CVODE ???";
    break;
  case -8: text = "Singular matrix encountered. Hmmm?";
    break;
  case -9: text = "Flags error...";
    break;
  }
  return text;
}

int cvode(xpp::Session &s, CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
 int err=0;
 if(s.model().nflags==0)
   return(ccvode(s,run,command,y,t,n,tout,kflag,atol,rtol));
 err=one_flag_step_cvode(s,run,command,y,t,n,tout,kflag,atol,rtol);
 if(err==1)*kflag=-9;
 return 1;
}
/* CVODE's absolute tolerance is TOLER and its relative one ATOLER: the solver
   passes them so, as XPPAUT does, whatever its manual says (docs/xppaut-findings.md 33) */
int ccvode(xpp::Session &s, CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
  *kflag=0;
  if(*command==2){
    end_cv(run);
    return(1);
  }
  if(*command==1||!run.memory){ /* continuing an integration that has
                                    ended is starting one */
    start_cv(s,run,y,*t,n,atol,rtol);
  }
  const int flag=run.memory->step(run,tout,t,y);
  if(flag != 0){
    *kflag=flag;
    end_cv(run);
    *command=1;
    return(-1);
  }
  *command=0;
  return(0);
}

} // namespace xpp
