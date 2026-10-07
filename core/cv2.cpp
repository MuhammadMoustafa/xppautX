#include "cv2.h"
#include "session.h"
#include "xpp_ui.h"

#include "flags.h"
#include "my_rhs.h"

                      /* integer (set to int), and the constant FALSE      */
                      /* constants OPT_SIZE, BDF, NEWTON, SV, SUCCESS,     */
                      /* NST, NFE, NSETUPS, NNI, NCFN, NETF                */
#include "cvdense.h"  /* prototype for CVDense, constant DENSE_NJE         */

                      /* prototypes for N_VNew, N_VFree                    */
#include "cvband.h"
#include "xpp_io.h"
#include "load_eqn.h"
#include "numerics.h"
#include <string>
#include "model.h"

namespace xpp {
static_assert(cvode_opt_size==OPT_SIZE);

namespace {
/* CVODE's right-hand side: fdata is the Session start_cv gave it */
void cvf(int n, double t, N_Vector y, N_Vector ydot, void *fdata)
{
  my_rhs(*static_cast<xpp::Session *>(fdata),t,y->data,ydot->data,n);
}
}

struct CvodeRun::Memory {
  N_Vector y;
  void *cvode;
  Memory(xpp::Session &s, CvodeRun &run, double *y0, double t, int n, double *atol, double *rtol)
    : y(N_VNew(n,NULL))
  {
    for(int i=0;i<n;i++)y->data[i]=y0[i];
    cvode=CVodeMalloc(n, cvf, t, y, BDF, NEWTON, SS, rtol, atol,
                      &s, FALSE, run.iopt.data(), run.ropt.data(), NULL);
    if(s.numerics.cv_bandflag==1)
      CVBand(cvode,s.numerics.cv_bandupper,s.numerics.cv_bandlower,NULL,NULL);
    else
      CVDense(cvode, NULL, NULL);
  }
  ~Memory()
  {
    N_VFree(y);
    CVodeFree(cvode);
  }
  Memory(const Memory &)=delete;
  Memory &operator=(const Memory &)=delete;
};

CvodeRun::CvodeRun()=default;
CvodeRun::~CvodeRun()=default;

namespace {
/* a fresh integration of run from y at t: the memory of one before, if
   it never ended, freed first */
void start_cv(xpp::Session &s, CvodeRun &run, double *y, double t, int n, double *atol, double *rtol)
{
  run.memory.reset();
  run.memory=std::make_unique<CvodeRun::Memory>(s,run,y,t,n,atol,rtol);
}
}

namespace {
/* why CVODE's integration failed, from its memory before it is freed */
void keep_failure(CvodeRun &run)
{
  const CVodeMem m=static_cast<CVodeMem>(run.memory->cvode);
  run.error=m->cv_error;
  run.error_var=m->cv_error_var;
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
  case -4: text = xpp::format("Tolerance too low-- try tolerance={} abs_tolerance={}",
	s.numerics.tolerance*run.ropt[ROPT_TOLSF], s.numerics.abs_tolerance*run.ropt[ROPT_TOLSF]);
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
/* rtol is like our TOLER and atol is something else ?? */
int ccvode(xpp::Session &s, CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
  int i,flag;
  *kflag=0;
  if(*command==2){
    end_cv(run);
    return(1);
  }
  if(*command==1||!run.memory){ /* continuing an integration that has
                                    ended is starting one */
    start_cv(s,run,y,*t,n,atol,rtol);
    flag=CVode(run.memory->cvode, tout, run.memory->y, t, NORMAL);
    if(flag != SUCCESS){
     
     *kflag=flag;
     keep_failure(run);
     end_cv(run);
     *command=1;
      return(-1);
    }
    *command=0;
    for(i=0;i<n;i++)y[i]=run.memory->y->data[i];
    return(0);
  } 
  flag=CVode(run.memory->cvode,tout,run.memory->y,t,NORMAL);
  if(flag != SUCCESS){
      *kflag=flag;
      keep_failure(run);
      end_cv(run);
      *command=1;
     
      return(-1);
  }
  for(i=0;i<n;i++)y[i]=run.memory->y->data[i];
  return(0);
}

} // namespace xpp
