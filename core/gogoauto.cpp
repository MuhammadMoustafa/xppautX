#include "xpp_mem.h"

#include <array>
#include <memory>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "autevd.h" /* xAuto (its own extern) */
#include "auto_nox.h" /* xpp::auto_fort_path() */


namespace {
/* AUTO's fort.3 (its restart data: read, or written from scratch), fort.7
   and fort.9 for one run, opened where open_auto put them, and the
   Session whose run it is: AUTO reads and writes them through its
   AutoLib (auto_state.h), which the run's iap_type points at. Closed on
   every way out of the run (Windows cannot rename or delete an open
   file, which close_auto does next), the pointers cleared first. */
struct RunUnits {
  AutoLib &lib;
  xpp::UniqueFile u3,u7,u9;
  RunUnits(xpp::Session &s, bool restart)
    : lib(s.auto_lib)
  {
    const auto open = [&s](int unit, const char *mode) {
      const std::string path=xpp::auto_fort_path(s,unit);
      xpp::UniqueFile file(xpp::files::open_stream(path,mode));
      if(!file)xpp::auto_fail((mode[0]=='r' ? xpp::files::open_error("AUTO",path)
                                         : xpp::files::write_error("AUTO",path)));
      return file;
    };
    u3=open(3,restart?"r":"w+");
    u7=open(7,"w");
    u9=open(9,"w");
    lib.session=&s;
    lib.fp3=u3.get();
    lib.fp7=u7.get();
    lib.fp9=u9.get();
  }
  ~RunUnits() { lib.fp3=lib.fp7=lib.fp9=nullptr; lib.session=nullptr; }
  RunUnits(const RunUnits &)=delete;
  RunUnits &operator=(const RunUnits &)=delete;
};
} // namespace

int go_go_auto(xpp::Session &s) /* this is the entry  at this point, xAuto has been set */
{
  s.auto_lib.parameter_count = s.auto_state.parameter_layout.size();
  std::vector<integer> icp(xpp::size_product(2, s.auto_lib.parameter_count));
  std::vector<doublereal> par(xpp::size_product(2, s.auto_lib.parameter_count));
  std::vector<doublereal> thl(s.auto_lib.parameter_count);
  std::vector<doublereal> thu;
  std::vector<AutoMark> marks(s.auto_state.run.marks.size());
  iap_type iap;
  rap_type rap;
  function_list list;
  RunUnits units(s,s.auto_state.run.irs>0);

  /* Initialization : */

  iap.lib = &s.auto_lib;

  /* here is the feeder code from xAuto structure */

  init(&iap, &rap, par.data(), icp.data(), thl.data(), thu, marks);

    /* Find restart label and determine type of restart point. */
    if (iap.irs > 0) {
      logical found = FALSE_;
     
      findlb(&iap, &rap, iap.irs, &(iap.nfpr), &found);
      if (! found) {
	xpp::log_auto("\nRestart label {:4d} not found\n",iap.irs);
	return(0);/* bad return: units closes the files */
      }
    }
    /* this is good for debugging and writes all the auto parameters */
    set_function_pointers(iap,&list);
    init1(&iap, &rap, icp.data(), par.data());
    chdim(&iap);
    if (list.type == AUTOBV)
      xpp::AutoParallel::select_run(s.auto_lib, iap.ndim, iap.ntst, iap.ncol);
    else s.auto_lib.collocation.reset();

    /* Create the allocations for the global structures used in 
       autlib3.c and autlib5.c.  There are purely an efficiency thing.
       The allocation and deallocation of these scratch areas takes
       up a nontrivial amount of time if done directly in the
       wrapper functions in autlib3.c*/
    allocate_global_memory(iap);

    /* ---------------------------------------------------------- */
    /* ---------------------------------------------------------- */
    /*  One-parameter continuations */
    /* ---------------------------------------------------------- */
    /* ---------------------------------------------------------- */

    if(list.type==AUTOAE)
      autoae(&iap, &rap, par.data(), icp.data(), list.aelist.funi, list.aelist.stpnt, list.aelist.pvli, thl.data(), thu.data(),
             marks);
    if(list.type==AUTOBV)
      autobv(&iap, &rap, par.data(), icp.data(), list.bvlist.funi, list.bvlist.bcni,
	     list.bvlist.icni, list.bvlist.stpnt, list.bvlist.pvli, thl.data(), thu.data(), marks);

  return 1;  /* normal return */
}

int set_function_pointers(const iap_type iap,function_list *data) {
  if ((iap.ips == xpp::AutoProblem::Algebraic || iap.ips == xpp::AutoProblem::SteadyState) && abs(iap.isw) != 2) {
    /*	** Algebraic systems. */
    if (iap.irs == 0) {
      data->type	   = AUTOAE;
      data->aelist.funi    = funi;
      data->aelist.stpnt   = stpnus;
      data->aelist.pvli    = pvlsae;
    } else {
      data->type	   = AUTOAE;
      data->aelist.funi    = funi;
      data->aelist.stpnt   = stpnae;
      data->aelist.pvli    = pvlsae;
    }
  } else if (iap.ips == xpp::AutoProblem::WaveSteady && abs(iap.isw) != 2) {
    /*	** Waves : Spatially homogeneous solutions, */
    if (iap.irs == 0) {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnws;
      data->aelist.stpnt   = stpnus;
      data->aelist.pvli    = pvlsae;
    } else {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnws;
      data->aelist.stpnt   = stpnae;
      data->aelist.pvli    = pvlsae;
    }
  } else if (iap.ips == xpp::AutoProblem::DiscreteMap && abs(iap.isw) != 2) {
    /*	** Discrete dynamical systems : fixed points. */
    if (iap.irs == 0) {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnds;
      data->aelist.stpnt   = stpnus;
      data->aelist.pvli    = pvlsae;
    } else {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnds;
      data->aelist.stpnt   = stpnae;
      data->aelist.pvli    = pvlsae;
    }
  } else if (iap.ips == xpp::AutoProblem::TimeIntegration) {
    /*	** Time integration. */
    if (iap.irs == 0) {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnti;
      data->aelist.stpnt   = stpnus;
      data->aelist.pvli    = pvlsae;
    } else {
      data->type	   = AUTOAE;
      data->aelist.funi    = fnti;
      data->aelist.stpnt   = stpnae;
      data->aelist.pvli    = pvlsae;
    }
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) != 2) {
    /*	** Periodic solutions */
    if (!iap.itp.is(xpp::AutoPointKind::Hopf) && !iap.itp.started_from(xpp::AutoPointKind::Hopf)) {
      if (iap.irs > 0) {
	data->type	   = AUTOBV;
	data->bvlist.funi  = fnps;
	data->bvlist.bcni  = bcps;
	data->bvlist.icni  = icps;
	data->bvlist.stpnt = stpnbv;
	data->bvlist.pvli  = pvlsbv;
      } else {
	data->type	   = AUTOBV;
	data->bvlist.funi  = fnps;
	data->bvlist.bcni  = bcps;
	data->bvlist.icni  = icps;
	data->bvlist.stpnt = stpnub;
	data->bvlist.pvli  = pvlsbv;
      }
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnps;
      data->bvlist.bcni  = bcps;
      data->bvlist.icni  = icps;
      data->bvlist.stpnt = stpnps;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::WavePeriodic && abs(iap.isw) != 2) {
    /*	** Wave train solutions to parabolic systems. */
    if (!iap.itp.is(xpp::AutoPointKind::Hopf)) {
      if (iap.irs > 0) {
	data->type	   = AUTOBV;
	data->bvlist.funi  = fnwp;
	data->bvlist.bcni  = bcps;
	data->bvlist.icni  = icps;
	data->bvlist.stpnt = stpnbv;
	data->bvlist.pvli  = pvlsbv;
      } else {
	data->type	   = AUTOBV;
	data->bvlist.funi  = fnwp;
	data->bvlist.bcni  = bcps;
	data->bvlist.icni  = icps;
	data->bvlist.stpnt = stpnub;
	data->bvlist.pvli  = pvlsbv;
      }
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi    = fnwp;
      data->bvlist.bcni    = bcps;
      data->bvlist.icni    = icps;
      data->bvlist.stpnt   = stpnwp;
      data->bvlist.pvli    = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::BoundaryValue && abs(iap.isw) != 2) {
    /*	** Boundary value problems. */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = funi;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icni;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = funi;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icni;
      data->bvlist.stpnt = stpnub;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::FloquetBoundaryValue && abs(iap.isw) != 2) {
    /*	** Boundary value problems with Floquet multipliers. */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = funi;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icni;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = funi;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icni;
      data->bvlist.stpnt = stpnub;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::Homoclinic && abs(iap.isw) != 2) {
    /*	** Homoclinic bifurcation analysis. */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnho;
      data->bvlist.bcni  = bcho;
      data->bvlist.icni  = icho;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsho;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnho;
      data->bvlist.bcni  = bcho;
      data->bvlist.icni  = icho;
      data->bvlist.stpnt = stpnho;
      data->bvlist.pvli  = pvlsho;
    }
  } else if (iap.ips == xpp::AutoProblem::ParabolicPeriodicEvolution) {
    /*	** Evolution calculations for parabolic systems. */
    /*	   (Periodic boundary conditions.) */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpe;
      data->bvlist.bcni  = bcps;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpe;
      data->bvlist.bcni  = bcps;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnub;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::PeriodicOptimization && abs(iap.isw) == 1) {
    /*	** Optimization of periodic solutions. */
    if (iap.nfpr < 6) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpo;
      data->bvlist.bcni  = bcpo;
      data->bvlist.icni  = icpo;
      data->bvlist.stpnt = stpnpo;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpo;
      data->bvlist.bcni  = bcpo;
      data->bvlist.icni  = icpo;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::ParabolicUserEvolution) {
    /*	** Evolution calculations for parabolic systems. */
    /*	   (User supplied boundary conditions.) */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpe;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnpe;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnub;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::ParabolicUserSteady) {
    /*	** Continuation of stationary states of parabolic systems.*/
    /*	   (User supplied boundary conditions.) */
    if (iap.irs > 0) {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnsp;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnbv;
      data->bvlist.pvli  = pvlsbv;
    } else {
      data->type	   = AUTOBV;
      data->bvlist.funi  = fnsp;
      data->bvlist.bcni  = bcni;
      data->bvlist.icni  = icpe;
      data->bvlist.stpnt = stpnub;
      data->bvlist.pvli  = pvlsbv;
    }
  } else if (iap.ips == xpp::AutoProblem::AlgebraicOptimization) {
    /*	** Algebraic optimization problems. */
    int nfpr = iap.nfpr;
    if (iap.itp.kind == xpp::AutoPointKind::Fold || iap.irs == 0) {
      nfpr++;
    }
    if (nfpr == 2) {
      if (iap.irs > 0) {
	data->type	   = AUTOAE;
	data->aelist.funi  = fnc1;
	data->aelist.stpnt  = stpnae;
	data->aelist.pvli    = pvlsae;
      } else {
	data->type	   = AUTOAE;
	data->aelist.funi  = fnc1;
	data->aelist.stpnt  = stpnc1;
	data->aelist.pvli    = pvlsae;
      }
    } else {
      if (iap.itp.kind != xpp::AutoPointKind::Fold) {
	data->type	   = AUTOAE;
	data->aelist.funi  = fnc2;
	data->aelist.stpnt  = stpnae;
	data->aelist.pvli    = pvlsae;
      } else {
	data->type	   = AUTOAE;
	data->aelist.funi  = fnc2;
	data->aelist.stpnt  = stpnc2;
	data->aelist.pvli    = pvlsae;
      }
    }
  }	   
  /* ---------------------------------------------------*/
  /* ---------------------------------------------------*/
  /*	Two-Parameter Continuation. */
  /* ---------------------------------------------------*/
  /* ---------------------------------------------------*/
      
  else if (xpp::problem_at_most_steady(iap.ips) && abs(iap.isw) == 2 && (iap.itp.is(xpp::AutoPointKind::BranchPoint) || iap.itp.is(xpp::AutoPointKind::Fold) || (iap.ips==xpp::AutoProblem::DiscreteMap && iap.itp.is(xpp::AutoPointKind::PeriodDoubling)))) {
    /*	** Fold continuation (algebraic problems). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnlp;
    data->aelist.stpnt  = stpnlp;
    data->aelist.pvli    = pvlsae;
  } else if (xpp::problem_at_most_steady(iap.ips) && abs(iap.isw) == 2 && (iap.itp.started_from(xpp::AutoPointKind::BranchPoint) || iap.itp.started_from(xpp::AutoPointKind::Fold) || (iap.ips==xpp::AutoProblem::DiscreteMap && iap.itp.started_from(xpp::AutoPointKind::PeriodDoubling)))) {
    /*	** Fold continuation (algebraic problems, restart). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnlp;
    data->aelist.stpnt  = stpnae;
    data->aelist.pvli    = pvlsae;
  } else if ((iap.ips == xpp::AutoProblem::Algebraic || iap.ips == xpp::AutoProblem::SteadyState) && abs(iap.isw) == 2 && iap.itp.is(xpp::AutoPointKind::Hopf)) {
    /*	** Hopf bifurcation continuation (ODE). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhb;
    data->aelist.stpnt  = stpnhb;
    data->aelist.pvli    = pvlsae;
  } else if ((iap.ips == xpp::AutoProblem::Algebraic || iap.ips == xpp::AutoProblem::SteadyState) && abs(iap.isw) == 2 && iap.itp.started_from(xpp::AutoPointKind::Hopf)) {
    /*	** Hopf bifurcation continuation (ODE, restart). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhb;
    data->aelist.stpnt  = stpnae;
    data->aelist.pvli    = pvlsae;
  } else if (iap.ips == xpp::AutoProblem::WaveSteady && abs(iap.isw) == 2 && iap.itp.is(xpp::AutoPointKind::Hopf)) {
    /*	** Hopf bifurcation continuation (Waves). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhw;
    data->aelist.stpnt  = stpnhw;
    data->aelist.pvli    = pvlsae;
  } else if (iap.ips == xpp::AutoProblem::WaveSteady && abs(iap.isw) == 2 && iap.itp.started_from(xpp::AutoPointKind::Hopf)) {
    /*	** Hopf bifurcation continuation (Waves, restart). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhw;
    data->aelist.stpnt  = stpnae;
    data->aelist.pvli    = pvlsae;
  } else if (iap.ips == xpp::AutoProblem::DiscreteMap && abs(iap.isw) == 2 && iap.itp.is(xpp::AutoPointKind::Torus)) {
    /*	** Hopf bifurcation continuation (Maps). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhb;
    data->aelist.stpnt  = stpnhb;
    data->aelist.pvli    = pvlsae;
  } else if (iap.ips == xpp::AutoProblem::DiscreteMap && abs(iap.isw) == 2 && iap.itp.started_from(xpp::AutoPointKind::Torus)) {
    /*	** Hopf bifurcation continuation (Maps). */
    data->type	   = AUTOAE;
    data->aelist.funi  = fnhb;
    data->aelist.stpnt  = stpnae;
    data->aelist.pvli    = pvlsae;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && (iap.itp.is(xpp::AutoPointKind::PeriodicFold) || iap.itp.is(xpp::AutoPointKind::PeriodicBranch))) {
    /*	** Fold continuation (Periodic solutions, start). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fnpl;
    data->bvlist.bcni  = bcpl;
    data->bvlist.icni  = icpl;
    data->bvlist.stpnt = stpnpl;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && (iap.itp.started_from(xpp::AutoPointKind::PeriodicFold) || iap.itp.started_from(xpp::AutoPointKind::PeriodicBranch))) {
    /*        ** Fold continuation (Periodic solutions, restart). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fnpl;
    data->bvlist.bcni  = bcpl;
    data->bvlist.icni  = icpl;
    data->bvlist.stpnt = stpnbv;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && iap.itp.is(xpp::AutoPointKind::PeriodDoubling)) {
    /*        ** Continuation of period doubling bifurcations (start). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fnpd;
    data->bvlist.bcni  = bcpd;
    data->bvlist.icni  = icpd;
    data->bvlist.stpnt = stpnpd;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && iap.itp.started_from(xpp::AutoPointKind::PeriodDoubling)) {
    /*        ** Continuation of period doubling bifurcations (restart).*/
    data->type         = AUTOBV;
    data->bvlist.funi  = fnpd;
    data->bvlist.bcni  = bcpd;
    data->bvlist.icni  = icpd;
    data->bvlist.stpnt = stpnbv;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && iap.itp.is(xpp::AutoPointKind::Torus)) {
    /*        ** Continuation of torus bifurcations (start). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fntr;
    data->bvlist.bcni  = bctr;
    data->bvlist.icni  = ictr;
    data->bvlist.stpnt = stpntr;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::Periodic && abs(iap.isw) == 2 && iap.itp.started_from(xpp::AutoPointKind::Torus)) {
    /*        ** Continuation of torus bifurcations (restart). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fntr;
    data->bvlist.bcni  = bctr;
    data->bvlist.icni  = ictr;
    data->bvlist.stpnt = stpnbv;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::BoundaryValue && abs(iap.isw) == 2 && (iap.itp.is(xpp::AutoPointKind::PeriodicFold) || iap.itp.is(xpp::AutoPointKind::PeriodicBranch))) {
    /*        ** Continuation of folds (BVP, start). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fnbl;
    data->bvlist.bcni  = bcbl;
    data->bvlist.icni  = icbl;
    data->bvlist.stpnt = stpnbl;
    data->bvlist.pvli  = pvlsbv;
  } else if (iap.ips == xpp::AutoProblem::BoundaryValue && abs(iap.isw) == 2 && (iap.itp.started_from(xpp::AutoPointKind::PeriodicFold) || iap.itp.started_from(xpp::AutoPointKind::PeriodicBranch))) {
    /*        ** Continuation of folds (BVP, restart). */
    data->type         = AUTOBV;
    data->bvlist.funi  = fnbl;
    data->bvlist.bcni  = bcbl;
    data->bvlist.icni  = icbl;
    data->bvlist.stpnt = stpnbv;
    data->bvlist.pvli  = pvlsbv;
  } else {
    /*        ** Error in INIT. */
    
      xpp::log_auto("\nInitialization Error CRASH!!\n");
   
      xpp::log_auto("itp={:d} ips={:d} isw={:d}\n",xpp::auto_point_encode(iap.itp),xpp::auto_problem_code(iap.ips),iap.isw);
  }
  /* -----------------------------------------------------------------------*/

  return 0;
}

