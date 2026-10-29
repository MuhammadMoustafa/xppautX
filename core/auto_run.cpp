#include "auto_unknowns.h"
/* A run of AUTO (auto_run.h): the kind-by-point-type table, the check of a model's equations against t, the set-up of
   each kind of run (XPPAUT's Run menus' items, moved here from auto_nox.cpp), the one function that starts a run and
   the Run key's questions. */
#include <algorithm>
#include <bit>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "auto_run.h"
#include "auto_settings.h"
#include "autevd.h"
#include "diagram.h"
#include "expr.h"
#include "expr_decode.h"
#include "integrate.h"
#include "model.h"
#include "model_files.h"
#include "pp_shoot.h"
#include "session.h"
#include "solver.h"
#include "xpp_io.h"
#include "menus.h"
#include "xpp_log.h"
#include "xpp_ui.h"

namespace xpp {

namespace {

/* a Hopf point of infinite period (a neutral saddle's curve end) has no orbit to start from */
std::optional<Error> infinite_period(const GRABPT &point)
{
  if (std::isinf(point.per)) return Error{"auto", "Can't continue infinite period Hopf.", command_place()};
  return std::nullopt;
}

/*  RUN AUTO HERE */
/*  these are for setting the parameters to run for different choices    */

/*  Just a short recall of the AUTO parameters
   NBC = 0 unless it really is a BVP problem (not periodics or heteroclinics)
   NICP = 1 for 1 parameter and 2 fro 2 parameter and the rest will 
            be taken care of in AUTO  and I think 2 for hetero?
   ILP = 1 (0) detection (no) of folds usually 1
   ISP = 2  detect all special points! but I think maybe set to 0, 1 for 
            BVP I think 
            for 2 parameter continuation ?
   ISW = -1 branch switching 1 is for normal 2 for two parameter of folds, tori,HB, PD!!

   IPS   1 - std for steady states of ODEs
         -1 maps
         2 periodic orbits
         4 BVP  (set NBC=NODE)
         9 Homoclinic

for example   2 P continuation of HB
              IPS=1 ILP=1 NICP=2 ISP=0 ISW=2 
BVP problem   IPS=4, NICP=1 NBC=NODE ISP=1 ISW=1 ILP=1

discrete dynamical system with two par of Hopf
first IPS=-1 ISP=ISW=1  then 
NICP=2, ISW=2 at Hopf

*/   

Result<> auto_start_at_bvp(Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  if(const Result<> compiled=xpp::compile_bvp(s);!compiled)return compiled;
  s.auto_state.type_of_calc=BV1;
 s.auto_state.bifur.ips=AutoProblem::BoundaryValue;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
  if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
    
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=no_calculation;
  s.auto_state.new_period_flag=true;
  return do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

Result<> auto_start_at_per(Session &s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.ips=AutoProblem::Periodic;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;

  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.two_param=no_calculation;
  s.auto_state.new_period_flag=true;
  return do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

Result<> auto_new_ss(Session &s, bool replace_diagram)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=false;

  if(replace_diagram)yes_reset_auto(s);
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.ips=AutoProblem::SteadyState;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
      if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=no_calculation;
  return do_auto(s, opn,cls,s.auto_state.bifur.itp);
}

Result<> auto_new_discrete(Session &s, bool replace_diagram)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.new_period_flag=false;
  if(replace_diagram)yes_reset_auto(s);
  s.auto_state.type_of_calc=DI1;
  s.auto_state.bifur.ips=AutoProblem::DiscreteMap;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=AutoPointCode{};
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nfpar=1;
   s.auto_state.two_param=no_calculation;
  return do_auto(s, opn,cls,s.auto_state.bifur.itp);
}
 
Result<> auto_extend_ss(Session &s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.auto_state.grabpt))return std::unexpected(*infinite);
      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.ips=AutoProblem::SteadyState;
  if(solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=AutoProblem::DiscreteMap;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
    
  s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_extend_homoclinic(Session &s)
{
   s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;

      s.auto_state.type_of_calc=HO2;
  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=true;
  s.auto_state.bifur.ips=AutoProblem::Homoclinic;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==HomoclinicRun::Homoclinic)
    s.auto_state.run.iequib=1;
  if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)
    s.auto_state.run.iequib=-2;

  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);

}

Result<> auto_start_at_homoclinic(Session &s, const HomoclinicStart &start)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.auto_state.bifur.irs=0;
  s.auto_state.bifur.itp=AutoPointCode{};
    s.auto_state.type_of_calc=HO2;

  s.auto_state.two_param=HO2;
  s.auto_state.new_period_flag=true;
  s.auto_state.bifur.ips=AutoProblem::Homoclinic;

  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.nbc=0;
  
  if(s.auto_state.homo_flag==HomoclinicRun::Homoclinic)
    s.auto_state.run.iequib=1;
  if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)
    s.auto_state.run.iequib=-2;
  s.auto_state.run.nunstab=start.unstable;
  s.auto_state.run.nstab=start.stable;
  s.auto_state.homo_l=start.left;
  if(s.auto_state.homo_flag==HomoclinicRun::Heteroclinic)s.auto_state.homo_r=start.right;
  /* the shift is measured from the equilibrium just given */
  if(s.auto_state.homo_flag==HomoclinicRun::Homoclinic)find_best_homo_shift(s, s.model().node);
  return do_auto(s, opn,cls,s.auto_state.bifur.itp);

}
    
Result<> auto_new_per(Session &s) /* same for extending periodic  */
{
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  
  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.auto_state.grabpt))return std::unexpected(*infinite);
      s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=1;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1; /* -1 */
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::Periodic;
    s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_extend_bvp(Session &s) /* extending bvp */
{
      s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::BoundaryValue;
    s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_switch_per(Session &s)
{
      s.auto_state.type_of_calc=PE1;
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=1; /*s.auto_state.grabpt.nfpar;*/
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::Periodic;
  s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_switch_bvp(Session &s)
{
     s.auto_state.type_of_calc=BV1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::BoundaryValue;
  s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_switch_ss(Session &s)
{

      s.auto_state.type_of_calc=EQ1;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=s.auto_state.grabpt.nfpar;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.bifur.isp=1;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::SteadyState;
  if(solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=AutoProblem::DiscreteMap;
  s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_2p_limit(Session &s, AutoProblem ips)
{
  AutoProblem ipsuse=AutoProblem::SteadyState;
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  const AutoPointCode grabbed=s.auto_state.grabpt.itp;
  s.auto_state.bifur.itp=grabbed;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=1; /* was 2 */
  /* fix ips now */
  if(ips==AutoProblem::DiscreteMap) ipsuse=ips;
  if(ips==AutoProblem::BoundaryValue)
    ipsuse=AutoProblem::BoundaryValue;
  else {
    if(grabbed.kind==AutoPointKind::CycleFold||grabbed.start==AutoPointKind::CycleFold)
      ipsuse=AutoProblem::Periodic;
  }

  if(!xpp::problem_is_steady_or_map(ipsuse))
    s.auto_state.bifur.isp=ipsuse==AutoProblem::Periodic && !s.auto_state.suppress_bp ? 2 : 0;
  s.auto_state.bifur.ips=ipsuse;
  s.auto_state.two_param=LPP2;
  if(xpp::problem_is_steady_or_map(ipsuse)){
    s.auto_state.type_of_calc=LPE2;
    s.auto_state.two_param=LPE2;
  }
  else{
    s.auto_state.type_of_calc=LPP2;
    s.auto_state.two_param=LPP2;
  }
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

/* continue a grabbed period doubling (PD2) or torus bifurcation (TR2) in
   two parameters: the same periodic restart, told apart by its kind */
Result<> auto_2p_periodic(Session &s, AutoCalculation kind)
{
  s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.two_param=kind;
  s.auto_state.type_of_calc=kind;
  s.auto_state.bifur.ips=solver_info(s.numerics.method).traits.discrete ? AutoProblem::DiscreteMap : AutoProblem::Periodic;
  s.auto_state.bifur.ilp=0;
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=s.auto_state.suppress_bp ? 0 : (s.auto_state.bifur.ips==AutoProblem::DiscreteMap ? 1 : 2);
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_2p_branch(Session &s, AutoProblem ips)
{
 AutoProblem ipsuse=AutoProblem::SteadyState;
 s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  const AutoPointCode grabbed=s.auto_state.grabpt.itp;
  s.auto_state.bifur.itp=grabbed;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=0; /* was 2 */
  if(ips==AutoProblem::BoundaryValue)
    ipsuse=AutoProblem::BoundaryValue;
  else {
    if(grabbed.kind==AutoPointKind::CycleBranch||grabbed.start==AutoPointKind::CycleBranch)
      ipsuse=AutoProblem::Periodic;
  }

  s.auto_state.bifur.ips=ipsuse;
  if(solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=AutoProblem::DiscreteMap;
  s.auto_state.two_param=BR2;
      s.auto_state.type_of_calc=BR2;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_2p_fixper(Session &s)
{
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=1; /* was1 */
  s.auto_state.bifur.isw=1;
  s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::Periodic;
  s.auto_state.two_param=FP2;
  s.auto_state.type_of_calc=FP2;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_2p_hopf(Session &s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.auto_state.grabpt))return std::unexpected(*infinite);
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.nfpar=2;
  s.auto_state.bifur.ilp=0; /* was 1 */
  s.auto_state.bifur.isw=2;
  s.auto_state.bifur.isp=1;
  s.auto_state.bifur.ips=AutoProblem::SteadyState;
  if(solver_info(s.numerics.method).traits.discrete)
    s.auto_state.bifur.ips=AutoProblem::DiscreteMap;
  if(s.auto_state.bifur.ips!=AutoProblem::SteadyState) s.auto_state.bifur.isp=0;
  s.auto_state.two_param=auto_hopf_curve;
    s.auto_state.type_of_calc=auto_hopf_curve;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}

Result<> auto_period_double(Session &s)
{

 s.auto_state.blrtn.torper=s.auto_state.grabpt.torper;
  s.auto_state.bifur.ntst=2*s.auto_state.bifur.ntst;
  s.auto_state.bifur.irs=s.auto_state.grabpt.lab;
  s.auto_state.bifur.nfpar=1; /* s.auto_state.grabpt.nfpar; */

  s.auto_state.bifur.itp=s.auto_state.grabpt.itp;
  s.auto_state.bifur.ilp=1;
  s.auto_state.bifur.isw=-1;
  s.auto_state.type_of_calc=PE1;
  s.auto_state.bifur.isp=2;
    if(s.auto_state.suppress_bp) s.auto_state.bifur.isp=0;
  s.auto_state.bifur.ips=AutoProblem::Periodic;
  s.auto_state.two_param=no_calculation;
  return do_auto(s, OPEN_3,APPEND,s.auto_state.bifur.itp);
}


/* ---- the table's lookups ---- */

/* the name of the table row a point falls under: the point or the point a two-parameter curve started from, in the
   order XPPAUT's Run key tried them */
std::string_view effective_point(const GRABPT &point, AutoFamily family)
{
  if (point.curve == auto_neutral_saddle_curve) return "neutral_saddle";
  /* an AUTO-own code with a negative kind (UZ, MX) names no start */
  const bool plain = point.itp.kind >= AutoPointKind::Regular;
  const auto involves = [&](AutoPointKind kind) { return plain && (point.itp.kind == kind || point.itp.start == kind); };
  if (involves(AutoPointKind::Hopf)) return "hopf";
  if (involves(AutoPointKind::PeriodDoubling)) return "period_doubling";
  if (family == AutoFamily::homoclinic) return "other";
  if (involves(AutoPointKind::Fold)) return "fold";
  if (involves(AutoPointKind::CycleFold)) return "cycle_fold";
  if (involves(AutoPointKind::CycleBranch)) return "cycle_branch_point";
  if (involves(AutoPointKind::BranchPoint)) return "branch_point";
  if (involves(AutoPointKind::NeimarkSacker)) return "neimark_sacker";
  return "other";
}

/* a name as a kind lists it: "a, b or c" */
std::string joined(const std::vector<std::string> &names)
{
  std::string text;
  for (std::size_t i = 0; i < names.size(); ++i)
    text += xpp::format("{}{}", i == 0 ? "" : i + 1 == names.size() ? " or " : ", ", names[i]);
  return text;
}

/* why a start of this type does not allow kind */
Error kind_does_not_fit(const GRABPT &point, AutoFamily family, AutoRunKind kind)
{
  std::vector<std::string> types;
  std::vector<std::string> ordinary;
  for (const AutoRunRow &row : auto_run_rows) {
    if (!(row.kinds & auto_run_bit(kind))) continue;
    if (std::string_view(row.point) == "other") ordinary.emplace_back(auto_family_name(row.family));
    else if (std::string_view(row.point) != "neutral_saddle" &&
             std::find(types.begin(), types.end(), row.point) == types.end()) types.emplace_back(row.point);
  }
  if (!ordinary.empty()) types.emplace_back(xpp::format("an ordinary point of a {} branch", joined(ordinary)));
  const std::string_view type = auto_point_kind_name(point.itp.kind);
  return Error{"auto",
               xpp::format("{} needs a point of type {}; label {} is {} {} point of a {} branch", auto_run_kind_name(kind),
                           joined(types), point.lab, std::string_view("aeiou").find(type.front()) == std::string_view::npos ? "a" : "an",
                           type, auto_family_name(family)),
               command_place()};
}

/* ---- what the start point is, and the problem each kind sets up ---- */

AutoProblem second_problem(const Session &s, std::string_view point, AutoFamily family)
{
  const bool discrete = solver_info(s.numerics.method).traits.discrete;
  if (point == "fold" || point == "hopf" || point == "neutral_saddle")
    return discrete ? AutoProblem::DiscreteMap : family == AutoFamily::boundary_value ? AutoProblem::BoundaryValue : AutoProblem::SteadyState;
  if (point == "cycle_fold") return family == AutoFamily::boundary_value ? AutoProblem::BoundaryValue : AutoProblem::Periodic;
  return family == AutoFamily::boundary_value ? AutoProblem::BoundaryValue : family == AutoFamily::periodic ? AutoProblem::Periodic : AutoProblem::SteadyState;
}

/* the run from the grabbed point, its kind already checked against the table */
Result<> run_from_point(Session &s, const AutoRunRequest &request, std::string_view point, AutoFamily family)
{
  AutoState &a = s.auto_state;
  switch (request.kind) {
  case AutoRunKind::equilibrium:
    return request.switch_branch ? auto_switch_ss(s) : auto_extend_ss(s);
  case AutoRunKind::periodic:
    if (request.switch_branch) return point == "period_doubling" ? auto_period_double(s) : auto_switch_per(s);
    return auto_new_per(s);
  case AutoRunKind::bvp:
    return request.switch_branch ? auto_switch_bvp(s) : auto_extend_bvp(s);
  case AutoRunKind::fixed_period:
    return auto_2p_fixper(s);
  case AutoRunKind::homoclinic:
  case AutoRunKind::heteroclinic:
    a.homo_flag = request.kind == AutoRunKind::homoclinic ? HomoclinicRun::Homoclinic : HomoclinicRun::Heteroclinic;
    return auto_extend_homoclinic(s);
  case AutoRunKind::two_parameter:
    if (point == "hopf" || point == "neutral_saddle") return auto_2p_hopf(s);
    if (point == "fold" || point == "cycle_fold") return auto_2p_limit(s, second_problem(s, point, family));
    if (point == "period_doubling") return auto_2p_periodic(s, PD2);
    if (point == "neimark_sacker") return auto_2p_periodic(s, TR2);
    return auto_2p_branch(s, second_problem(s, point, family));
  }
  return {};
}

/* the equilibria of a homoclinic run: the request's, else the session's own, and they must fit the model */
Result<HomoclinicStart> homoclinic_start(const Session &s, const AutoRunRequest &request)
{
  const AutoState &a = s.auto_state;
  const std::size_t n = static_cast<std::size_t>(s.model().node);
  HomoclinicStart start;
  if (request.homoclinic) start = *request.homoclinic;
  else start = HomoclinicStart{a.run.nunstab, a.run.nstab, a.homo_l, a.homo_r};
  if (start.left.empty()) start.left = a.homo_l;
  if (start.right.empty()) start.right = a.homo_r;
  /* HomCont's projection takes that many directions at each end: one condition for each of the equilibrium's dimensions */
  if (start.unstable < 0 || start.stable < 0 || static_cast<std::size_t>(start.unstable + start.stable) != n)
    return fail("auto", xpp::format("unstable_dimension ({}) and stable_dimension ({}) must add up to the number of variables, {}",
                                    start.unstable, start.stable, n), command_place());
  if (start.left.size() != n || start.right.size() != n)
    return fail("auto", xpp::format("the equilibria need one value for each of the {} variables", n), command_place());
  return start;
}

}  // namespace

AutoFamily auto_family_of(const GRABPT &point)
{
  if (point.ibr < 0) return AutoFamily::periodic;
  if (point.calc == BV1) return AutoFamily::boundary_value;
  if (point.calc == HO2) return AutoFamily::homoclinic;
  return AutoFamily::steady;
}

const AutoRunRow &auto_run_row_for(const GRABPT &point)
{
  const AutoFamily family = auto_family_of(point);
  const std::string_view point_name = effective_point(point, family);
  for (const AutoRunRow &row : auto_run_rows)
    if (point_name == row.point && (row.family == AutoFamily::any || row.family == family)) return row;
  return auto_run_rows.back(); /* every `other` family has a row, so this is the last */
}

Result<> auto_time_check(const Session &s)
{
  const Model &m = s.model();
  std::vector<char> checked_fixed(static_cast<std::size_t>(m.fix_var), 0), checked_function(static_cast<std::size_t>(m.nfun), 0);
  /* whether the program reads t (the variable 0), directly or through a fixed variable or a user function */
  const auto reads_time = [&](const auto &self, const Program &program, int arguments) -> bool {
    const auto code = expr::decode(s, program.rpn, arguments);
    if (!code) return false; /* a program the compiler refused never runs */
    for (const expr::Instr &i : *code) {
      if (i.type == VARTYPE && i.in == 0) return true;
      if (i.type == VARTYPE && i.in > m.node && i.in <= m.node + m.fix_var) {
        char &seen = checked_fixed[static_cast<std::size_t>(i.in - m.node - 1)];
        if (seen) continue;
        seen = 1;
        if (self(self, m.programs[static_cast<std::size_t>(i.in - 1)], -1)) return true;
      }
      if (i.type == UFUNTYPE && i.in >= 0 && i.in < m.nfun) {
        char &seen = checked_function[static_cast<std::size_t>(i.in)];
        if (seen) continue;
        seen = 1;
        if (self(self, m.ufun_programs[static_cast<std::size_t>(i.in)], m.narg_fun[static_cast<std::size_t>(i.in)])) return true;
      }
    }
    return false;
  };
  std::vector<std::string> equations;
  Place first;
  for (int i = 0; i < m.node; ++i) {
    std::fill(checked_fixed.begin(), checked_fixed.end(), 0);
    std::fill(checked_function.begin(), checked_function.end(), 0);
    if (!reads_time(reads_time, m.programs[static_cast<std::size_t>(i)], -1)) continue;
    const std::string &name = m.uvar_names[static_cast<std::size_t>(i)];
    const Place place = model_place(m, name);
    if (equations.empty()) first = place;
    equations.push_back(xpp::format("{}' ({}:{})", name, place.file.empty() ? m.this_file : place.file, place.line));
  }
  if (equations.empty()) return {};
  std::string list;
  for (const std::string &equation : equations) list += xpp::format("{}{}", list.empty() ? "" : ", ", equation);
  return fail("auto", xpp::format("AUTO runs equations that do not depend on t, and evaluates them at t = 0; {} use t. Add t as a state "
                                  "(an oscillator, see \"Equations that use t\" in the AUTO manual and examples/tstauto/frcp.odex)", list),
              first.line > 0 ? first : command_place());
}

Result<> auto_run_check(const Session &s, const AutoRunRequest &request, const GRABPT *point, bool settling)
{
  const char *kind=auto_run_kind_name(request.kind);
  if (auto autonomous=auto_time_check(s); !autonomous) return autonomous;
  if(!request.from_point){
    if(request.switch_branch)return fail("auto",xpp::format("{} with switch starts from a stored point (a branch point or a period doubling)",kind),command_place());
    if(!(auto_run_current_kinds & auto_run_bit(request.kind)))return fail("auto",xpp::format("{} starts from a stored point: give its label",kind),command_place());
    if(request.kind==AutoRunKind::homoclinic || request.kind==AutoRunKind::heteroclinic){
      if(auto start=homoclinic_start(s,request); !start)return std::unexpected(start.error());
    }
    if(request.kind!=AutoRunKind::equilibrium && s.data_store.rows<2 && !settling)
      return fail("auto",xpp::format("Integrate first: a {} run from the current state takes its orbit from the last integration",kind),command_place());
    return {};
  }
  const GRABPT &g=point?*point:s.auto_state.grabpt;
  if(g.flag==0 || g.lab==0)return fail("auto",xpp::format("{} starts from a labelled stored point: none is grabbed (auto.grab a label first)",kind),command_place());
  const AutoFamily family=auto_family_of(g);
  const AutoRunRow &row=auto_run_row_for(g);
  const bool needs_period = (!request.switch_branch && (request.kind==AutoRunKind::equilibrium || request.kind==AutoRunKind::periodic)) || (request.kind==AutoRunKind::two_parameter && (std::string_view(row.point)=="hopf" || std::string_view(row.point)=="neutral_saddle"));
  if(needs_period)if(auto infinite=infinite_period(g); infinite)return std::unexpected(*infinite);
  if(!(row.kinds & auto_run_bit(request.kind)))return std::unexpected(kind_does_not_fit(g,family,request.kind));
  if(request.switch_branch && !(row.switch_kinds & auto_run_bit(request.kind)))
    return fail("auto",xpp::format("{} with switch needs a branch point or a period doubling; label {} is {} point",kind,g.lab,auto_point_kind_name(g.itp.kind)),command_place());
  return {};
}

Result<> auto_run_start(Session &s, const AutoRunRequest &request)
{
  AutoState &a = s.auto_state;
  if(auto checked=auto_run_check(s,request); !checked)return checked;
  if (!request.from_point) {
    /* a homoclinic run's equilibria must fit the model, whatever else is wrong */
    const bool homoclinic = request.kind == AutoRunKind::homoclinic || request.kind == AutoRunKind::heteroclinic;
    std::optional<HomoclinicStart> start;
    if (homoclinic) {
      const Result<HomoclinicStart> checked = homoclinic_start(s, request);
      if (!checked) return std::unexpected(checked.error());
      start = *checked;
    }
    a.run_kind = request.kind;
    a.homo_flag = HomoclinicRun::None;
    switch (request.kind) {
    case AutoRunKind::equilibrium:
      return solver_info(s.numerics.method).traits.discrete ? auto_new_discrete(s, request.replace_diagram) : auto_new_ss(s, request.replace_diagram);
    case AutoRunKind::periodic:
      return auto_start_at_per(s);
    case AutoRunKind::bvp:
      a.bifur.nbc = s.model().node;
      return auto_start_at_bvp(s);
    case AutoRunKind::homoclinic:
    case AutoRunKind::heteroclinic:
      a.homo_flag = request.kind == AutoRunKind::homoclinic ? HomoclinicRun::Homoclinic : HomoclinicRun::Heteroclinic;
      return auto_start_at_homoclinic(s, *start);
    case AutoRunKind::two_parameter:
    case AutoRunKind::fixed_period:
      break;
    }
    return {};
  }
  const GRABPT &g = a.grabpt;
  const AutoFamily family = auto_family_of(g);
  const AutoRunRow &row = auto_run_row_for(g);
  a.run_kind = request.kind;
  return run_from_point(s, request, row.point, family);
}

/* ---- the Run key: XPPAUT's questions ---- */

namespace {

}  // namespace

/* what XPPAUT's "Homoclinic info" form asked: the equilibria's dimensions and values */




/* ---- the table as data ---- */

namespace {

void append_kinds(std::string &out, unsigned kinds)
{
  out += '[';
  bool first = true;
  for (std::size_t k = 0; k < auto_run_kind_names.size(); ++k) {
    if (!(kinds & (1u << k))) continue;
    if (!first) out += ',';
    first = false;
    json_append_string(out, auto_run_kind_names[k]);
  }
  out += ']';
}

}  // namespace

std::string auto_run_table_json()
{
  std::string out = "{\"kinds\":[";
  for (std::size_t k = 0; k < auto_run_kind_names.size(); ++k) {
    if (k) out += ',';
    json_append_string(out, auto_run_kind_names[k]);
  }
  out += "],\"current\":";
  append_kinds(out, auto_run_current_kinds);
  out += ",\"rows\":[";
  for (std::size_t i = 0; i < auto_run_rows.size(); ++i) {
    const AutoRunRow &row = auto_run_rows[i];
    out += xpp::format("{}{{\"point\":", i ? "," : "");
    json_append_string(out, row.point);
    out += ",\"family\":";
    json_append_string(out, auto_family_name(row.family));
    out += ",\"kinds\":";
    append_kinds(out, row.kinds);
    out += ",\"switch\":";
    append_kinds(out, row.switch_kinds);
    out += '}';
  }
  out += "]}";
  return out;
}

std::string auto_run_start_json(const Session &s)
{
  const GRABPT &g = s.auto_state.grabpt;
  if (g.flag == 0) return "null";
  const AutoRunRow &row = auto_run_row_for(g);
  std::string out = xpp::format("{{\"label\":{},\"type\":", g.lab);
  json_append_string(out, auto_point_kind_name(g.itp.kind));
  out += ",\"family\":";
  json_append_string(out, auto_family_name(auto_family_of(g)));
  out += ",\"kinds\":";
  append_kinds(out, g.lab == 0 ? 0 : row.kinds);
  out += ",\"switch\":";
  append_kinds(out, g.lab == 0 ? 0 : row.switch_kinds);
  return out + '}';
}

}  // namespace xpp
