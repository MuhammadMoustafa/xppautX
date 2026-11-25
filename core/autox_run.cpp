/* A run of autoX (autox_run.h): the kind-by-point-type table, the check of a model's equations against t, the set-up of
   each kind of run (XPPAUT's Run menus' items, moved here from autox_nox.cpp), the one function that starts a run and
   the Run key's questions. */
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <expected>
#include <memory>
#include <span>

#include "autox_run.h"
#include "autox_jobs.h"
#include "autox_stop.h"
#include "diagram.h"
#include <cstdlib>
#include "expr_decode.h"
#include "model.h"
#include "model_place.h"
#include "pp_shoot.h"
#include "autox_state.h"
#include "storage.h"
#include "solver.h"
#include "xpp_text.h"
#include "xpp_ui.h"
#include "expr_program.h"
#include "diagram.h"
#include "xpp_log.h"

namespace xpp {

namespace {

/* a Hopf point of infinite period (a neutral saddle's curve end) has no orbit to start from */
std::optional<Error> infinite_period(const GRABPT &point)
{
  if (std::isinf(point.per)) return Error{"autox", "Can't continue infinite period Hopf.", command_place()};
  return std::nullopt;
}

/* A periodic start takes its period from the stored orbit's own return: the first time it comes back to its first row,
   found between rows. The return is where the orbit crosses, from behind, the plane through its first row perpendicular to
   its first step (the flow there); the crossing is interpolated between the two rows, and its gap to the first row, over
   the orbit's extent, must be at most this limit. A total time of several periods gives the first return; one that
   still holds a transient returns far from its first row and misses by a good part of the extent, while a start a
   little off the cycle misses by far less, and Newton's method takes it from there. */
constexpr double autox_orbit_return_limit = 0.1;

/* the first return of the stored orbit (rows >= 2): its time from the first row and its position among the rows, interpolated */
Result<AutoxOrbitReturn> autox_orbit_return(const AutoxParts &s)
{
  const std::size_t rows = static_cast<std::size_t>(s.data_store->rows);
  const std::size_t n = static_cast<std::size_t>(s.model().node);
  const Column t = s.data_store->col[0];
  double extent = 0, step = 0;
  for (std::size_t k = 0; k < n; ++k) {
    const Column c = s.data_store->col[k + 1];
    double low = c.get(0), high = low;
    for (std::size_t i = 1; i < rows; ++i) {
      low = std::min(low, c.get(static_cast<std::ptrdiff_t>(i)));
      high = std::max(high, c.get(static_cast<std::ptrdiff_t>(i)));
    }
    extent += s.data_store->round(high - low) * s.data_store->round(high - low);
    step += s.data_store->round(c.get(1) - c.get(0)) * s.data_store->round(c.get(1) - c.get(0));
  }
  if (extent == 0 || step == 0)
    return fail("autox", xpp::format("Periodic start stopped: the stored orbit ({} rows from t = {:.6g}) does not move at its start; it is not a cycle", rows, t.get(0)), command_place());
  /* the row's position along the first step: the orbit starts at > 0 and returns where it climbs through 0 */
  const auto along = [&](std::size_t i) {
    double sum = 0;
    for (std::size_t k = 0; k < n; ++k) {
      const Column c = s.data_store->col[k + 1];
      sum += (c.get(static_cast<std::ptrdiff_t>(i)) - c.get(0)) * (c.get(1) - c.get(0));
    }
    return sum;
  };
  double closest = INFINITY, behind = along(1);
  for (std::size_t i = 1; i + 1 < rows; ++i) {
    const double ahead = along(i + 1);
    if (behind < 0 && ahead >= 0) {
      const double lam = -behind / (ahead - behind);
      double gap = 0;
      for (std::size_t k = 0; k < n; ++k) {
        const Column c = s.data_store->col[k + 1];
        const double d = (1 - lam) * c.get(static_cast<std::ptrdiff_t>(i)) + lam * c.get(static_cast<std::ptrdiff_t>(i + 1)) - c.get(0);
        gap += d * d;
      }
      const double ratio = std::sqrt(gap / extent);
      if (ratio <= autox_orbit_return_limit)
        return AutoxOrbitReturn{(1 - lam) * t.get(static_cast<std::ptrdiff_t>(i)) + lam * t.get(static_cast<std::ptrdiff_t>(i + 1)) - t.get(0), static_cast<double>(i) + lam};
      closest = std::min(closest, ratio);
    }
    behind = ahead;
  }
  const std::string why = std::isinf(closest) ? "never comes back to its first row" : xpp::format("comes back no nearer than {:.3g} of the orbit's extent, more than {} allowed", closest, autox_orbit_return_limit);
  return fail("autox", xpp::format("Periodic start stopped: the stored orbit (t = {:.6g} to {:.6g}, {} rows) {}; integrate for at least one period after the transient and try again", t.get(0), t.get(static_cast<std::ptrdiff_t>(rows - 1)), rows, why), command_place());
}

/*  RUN autoX HERE */
/*  these are for setting the parameters to run for different choices    */

/*  Just a short recall of the autoX parameters
   NBC = 0 unless it really is a BVP problem (not periodics or heteroclinics)
   NICP = 1 for 1 parameter and 2 fro 2 parameter and the rest will 
            be taken care of in autoX  and I think 2 for hetero?
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

Result<> autox_start_at_bvp(AutoxParts s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  if(const Result<> compiled=xpp::compile_bvp(s.analysis);!compiled)return compiled;
  s.autox_state->type_of_calc=BV1;
 s.autox_state->bifur.ips=AutoxProblem::BoundaryValue;
  s.autox_state->bifur.irs=0;
  s.autox_state->bifur.itp=AutoxPointCode{};
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;

  s.autox_state->bifur.isp=2;
  if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
    
  s.autox_state->bifur.nfpar=1;
  s.autox_state->two_param=no_calculation;
  s.autox_state->new_period_flag=true;
  return do_autox(s, opn,cls,s.autox_state->bifur.itp);
}

Result<> autox_start_at_per(AutoxParts s)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  
  s.autox_state->type_of_calc=PE1;
  s.autox_state->bifur.ips=AutoxProblem::Periodic;
  s.autox_state->bifur.irs=0;
  s.autox_state->bifur.itp=AutoxPointCode{};
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;

  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.nfpar=1;
  s.autox_state->two_param=no_calculation;
  s.autox_state->new_period_flag=true;
  const Result<AutoxOrbitReturn> back=autox_orbit_return(s);
  if(!back)return std::unexpected(back.error());
  s.autox_state->start_return=*back;
  return autox_run_from_current_state(s, opn,cls);
}

Result<> autox_new_ss(AutoxParts s, bool replace_diagram)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.autox_state->new_period_flag=false;

  if(replace_diagram)yes_reset_autox(s);
      s.autox_state->type_of_calc=EQ1;
  s.autox_state->bifur.ips=AutoxProblem::SteadyState;
  s.autox_state->bifur.irs=0;
  s.autox_state->bifur.itp=AutoxPointCode{};
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=1;
      if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;;
  s.autox_state->bifur.nfpar=1;
   s.autox_state->two_param=no_calculation;
  return autox_run_from_current_state(s, opn,cls);
}

Result<> autox_new_discrete(AutoxParts s, bool replace_diagram)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.autox_state->new_period_flag=false;
  if(replace_diagram)yes_reset_autox(s);
  s.autox_state->type_of_calc=DI1;
  s.autox_state->bifur.ips=AutoxProblem::DiscreteMap;
  s.autox_state->bifur.irs=0;
  s.autox_state->bifur.itp=AutoxPointCode{};
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=1;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.nfpar=1;
   s.autox_state->two_param=no_calculation;
  return autox_run_from_current_state(s, opn,cls);
}
 
Result<> autox_extend_ss(AutoxParts s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.autox_state->grabpt))return std::unexpected(*infinite);
      s.autox_state->type_of_calc=EQ1;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=s.autox_state->grabpt.nfpar;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.ips=AutoxProblem::SteadyState;
  if(solver_info(s.numerics->method).traits.discrete)
    s.autox_state->bifur.ips=AutoxProblem::DiscreteMap;
  s.autox_state->bifur.isp=1;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
    
  s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_extend_homoclinic(AutoxParts s)
{
   s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;

      s.autox_state->type_of_calc=HO2;
  s.autox_state->two_param=HO2;
  s.autox_state->new_period_flag=true;
  s.autox_state->bifur.ips=AutoxProblem::Homoclinic;

  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=0;
  s.autox_state->bifur.nbc=0;
  
  if(s.autox_state->homo_flag==HomoclinicRun::Homoclinic)
    s.autox_state->homo_equilibria.iequib=1;
  if(s.autox_state->homo_flag==HomoclinicRun::Heteroclinic)
    s.autox_state->homo_equilibria.iequib=-2;

  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);

}

Result<> autox_start_at_homoclinic(AutoxParts s, const HomoclinicStart &start)
{
  int opn=NO_OPEN_3,cls=OVERWRITE;
  s.autox_state->bifur.irs=0;
  s.autox_state->bifur.itp=AutoxPointCode{};
    s.autox_state->type_of_calc=HO2;

  s.autox_state->two_param=HO2;
  s.autox_state->new_period_flag=true;
  s.autox_state->bifur.ips=AutoxProblem::Homoclinic;

  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=1; /* maybe 1 someday also in extend homo, but for now, no 3 param allowed    */
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=0;
  s.autox_state->bifur.nbc=0;
  
  if(s.autox_state->homo_flag==HomoclinicRun::Homoclinic)
    s.autox_state->homo_equilibria.iequib=1;
  if(s.autox_state->homo_flag==HomoclinicRun::Heteroclinic)
    s.autox_state->homo_equilibria.iequib=-2;
  s.autox_state->homo_equilibria.nunstab=start.unstable;
  s.autox_state->homo_equilibria.nstab=start.stable;
  s.autox_state->homo_l=start.left;
  if(s.autox_state->homo_flag==HomoclinicRun::Heteroclinic)s.autox_state->homo_r=start.right;
  /* the shift is measured from the equilibrium just given */
  if(s.autox_state->homo_flag==HomoclinicRun::Homoclinic)find_best_homo_shift(s, s.model().node);
  return do_autox(s, opn,cls,s.autox_state->bifur.itp);

}
    
Result<> autox_new_per(AutoxParts s) /* same for extending periodic  */
{
  /* a script's run from a Hopf point whose cycles a follow-all job already traced (W868, #950): the named run replaces that copy,
     with its own options, and the copy goes before it stores its branch; a branch a named run made is never replaced */
  if(s.autox_state->skip_run_on_branch&&s.autox_state->grabpt.itp.is(AutoxPointKind::Hopf)){
    const int label=s.autox_state->grabpt.lab;
    std::vector<int> copies;
    for(const DIAGRAM &held:s.diagram->points){
      const int ibr=std::abs(held.ibr);
      if(held.from==label&&held.calc==PE1&&held.job!=0&&ibr!=0&&!std::ranges::contains(copies,ibr))copies.push_back(ibr);
    }
    if(!copies.empty()){
      std::string named;
      for(const int ibr:copies){
        diagram_remove_branch(s,ibr);
        named+=(named.empty()?"":", ")+std::to_string(ibr);
      }
      redraw_diagram(s);
      const std::string why=xpp::format("branch {}, the cycles of the Hopf point labelled {} that follow all branches traced, is replaced by this run's own",
                                        named,label);
      xpp::log(XPP_LOG_WARN,"autoX: {}",why);
      s.autox_state->stop.notes.push_back(why);
    }
  }
  s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  
  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.autox_state->grabpt))return std::unexpected(*infinite);
      s.autox_state->type_of_calc=PE1;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=1;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1; /* -1 */
  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::Periodic;
    s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_extend_bvp(AutoxParts s) /* extending bvp */
{
      s.autox_state->type_of_calc=BV1;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=s.autox_state->grabpt.nfpar;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::BoundaryValue;
    s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_switch_per(AutoxParts s)
{
      s.autox_state->type_of_calc=PE1;
  s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=1; /*s.autox_state->grabpt.nfpar;*/
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=-1;
  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::Periodic;
  s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_switch_bvp(AutoxParts s)
{
     s.autox_state->type_of_calc=BV1;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=s.autox_state->grabpt.nfpar;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=-1;
  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::BoundaryValue;
  s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_switch_ss(AutoxParts s)
{

      s.autox_state->type_of_calc=EQ1;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=s.autox_state->grabpt.nfpar;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=-1;
  s.autox_state->bifur.isp=1;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::SteadyState;
  if(solver_info(s.numerics->method).traits.discrete)
    s.autox_state->bifur.ips=AutoxProblem::DiscreteMap;
  s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_2p_limit(AutoxParts s, AutoxProblem ips)
{
  AutoxProblem ipsuse=AutoxProblem::SteadyState;
  s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  const AutoxPointCode grabbed=s.autox_state->grabpt.itp;
  s.autox_state->bifur.itp=grabbed;
  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=0; /* was 1 */
  s.autox_state->bifur.isw=2;
  s.autox_state->bifur.isp=1; /* was 2 */
  /* fix ips now */
  if(ips==AutoxProblem::DiscreteMap) ipsuse=ips;
  if(ips==AutoxProblem::BoundaryValue)
    ipsuse=AutoxProblem::BoundaryValue;
  else {
    if(grabbed.kind==AutoxPointKind::CycleFold||grabbed.start==AutoxPointKind::CycleFold)
      ipsuse=AutoxProblem::Periodic;
  }

  if(!xpp::problem_is_steady_or_map(ipsuse))
    s.autox_state->bifur.isp=ipsuse==AutoxProblem::Periodic && !s.autox_state->suppress_bp ? 2 : 0;
  s.autox_state->bifur.ips=ipsuse;
  s.autox_state->two_param=LPP2;
  if(xpp::problem_is_steady_or_map(ipsuse)){
    s.autox_state->type_of_calc=LPE2;
    s.autox_state->two_param=LPE2;
  }
  else{
    s.autox_state->type_of_calc=LPP2;
    s.autox_state->two_param=LPP2;
  }
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

/* continue a grabbed period doubling (PD2) or torus bifurcation (TR2) in
   two parameters: the same periodic restart, told apart by its kind */
Result<> autox_2p_periodic(AutoxParts s, AutoxCalculation kind)
{
  s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=2;
  s.autox_state->two_param=kind;
  s.autox_state->type_of_calc=kind;
  s.autox_state->bifur.ips=solver_info(s.numerics->method).traits.discrete ? AutoxProblem::DiscreteMap : AutoxProblem::Periodic;
  s.autox_state->bifur.ilp=0;
  s.autox_state->bifur.isw=2;
  s.autox_state->bifur.isp=s.autox_state->suppress_bp ? 0 : (s.autox_state->bifur.ips==AutoxProblem::DiscreteMap ? 1 : 2);
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_2p_branch(AutoxParts s, AutoxProblem ips)
{
 AutoxProblem ipsuse=AutoxProblem::SteadyState;
 s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  const AutoxPointCode grabbed=s.autox_state->grabpt.itp;
  s.autox_state->bifur.itp=grabbed;
  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=0; /* was 1 */
  s.autox_state->bifur.isw=2;
  s.autox_state->bifur.isp=0; /* was 2 */
  if(ips==AutoxProblem::BoundaryValue)
    ipsuse=AutoxProblem::BoundaryValue;
  else {
    if(grabbed.kind==AutoxPointKind::CycleBranch||grabbed.start==AutoxPointKind::CycleBranch)
      ipsuse=AutoxProblem::Periodic;
  }

  s.autox_state->bifur.ips=ipsuse;
  if(solver_info(s.numerics->method).traits.discrete)
    s.autox_state->bifur.ips=AutoxProblem::DiscreteMap;
  s.autox_state->two_param=BR2;
      s.autox_state->type_of_calc=BR2;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_2p_fixper(AutoxParts s)
{
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=1; /* was1 */
  s.autox_state->bifur.isw=1;
  s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::Periodic;
  s.autox_state->two_param=FP2;
  s.autox_state->type_of_calc=FP2;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_2p_hopf(AutoxParts s)
{

  /*Prevent crash on hopf of infinite period. here
  
  Typical abort message after crash is currently something like:
  
  fmt: read unexpected character
  apparent state: unit 3 named ~/fort.3
  last format: (4x,1p7e18.10)
  lately reading sequential formatted external IO
  
  */
  
  if(const auto infinite=infinite_period(s.autox_state->grabpt))return std::unexpected(*infinite);
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.nfpar=2;
  s.autox_state->bifur.ilp=0; /* was 1 */
  s.autox_state->bifur.isw=2;
  s.autox_state->bifur.isp=1;
  s.autox_state->bifur.ips=AutoxProblem::SteadyState;
  if(solver_info(s.numerics->method).traits.discrete)
    s.autox_state->bifur.ips=AutoxProblem::DiscreteMap;
  if(s.autox_state->bifur.ips!=AutoxProblem::SteadyState) s.autox_state->bifur.isp=0;
  s.autox_state->two_param=autox_hopf_curve;
    s.autox_state->type_of_calc=autox_hopf_curve;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}

Result<> autox_period_double(AutoxParts s)
{

 s.autox_state->blrtn.torper=s.autox_state->grabpt.torper;
  s.autox_state->bifur.ntst=2*s.autox_state->bifur.ntst;
  s.autox_state->bifur.irs=s.autox_state->grabpt.lab;
  s.autox_state->bifur.nfpar=1; /* s.autox_state->grabpt.nfpar; */

  s.autox_state->bifur.itp=s.autox_state->grabpt.itp;
  s.autox_state->bifur.ilp=1;
  s.autox_state->bifur.isw=-1;
  s.autox_state->type_of_calc=PE1;
  s.autox_state->bifur.isp=2;
    if(s.autox_state->suppress_bp) s.autox_state->bifur.isp=0;
  s.autox_state->bifur.ips=AutoxProblem::Periodic;
  s.autox_state->two_param=no_calculation;
  return do_autox(s, OPEN_3,APPEND,s.autox_state->bifur.itp);
}


/* ---- the table's lookups ---- */

/* the name of the table row a point falls under: the point or the point a two-parameter curve started from, in the
   order XPPAUT's Run key tried them */
std::string_view effective_point(const GRABPT &point, AutoxFamily family)
{
  if (point.curve == autox_neutral_saddle_curve) return "neutral_saddle";
  /* an autoX-own code with a negative kind (UZ, MX) names no start */
  const bool plain = point.itp.kind >= AutoxPointKind::Regular;
  const auto involves = [&](AutoxPointKind kind) { return plain && (point.itp.kind == kind || point.itp.start == kind); };
  if (involves(AutoxPointKind::Hopf)) return "hopf";
  if (involves(AutoxPointKind::PeriodDoubling)) return "period_doubling";
  if (family == AutoxFamily::homoclinic) return "other";
  if (involves(AutoxPointKind::Fold)) return "fold";
  if (involves(AutoxPointKind::CycleFold)) return "cycle_fold";
  if (involves(AutoxPointKind::CycleBranch)) return "cycle_branch_point";
  if (involves(AutoxPointKind::BranchPoint)) return "branch_point";
  if (involves(AutoxPointKind::NeimarkSacker)) return "neimark_sacker";
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
Error kind_does_not_fit(const GRABPT &point, AutoxFamily family, AutoxRunKind kind)
{
  std::vector<std::string> types;
  std::vector<std::string> ordinary;
  for (const AutoxRunRow &row : autox_run_rows) {
    if (!(row.kinds & autox_run_bit(kind))) continue;
    if (std::string_view(row.point) == "other") ordinary.emplace_back(autox_family_name(row.family));
    else if (std::string_view(row.point) != "neutral_saddle" &&
             std::find(types.begin(), types.end(), row.point) == types.end()) types.emplace_back(row.point);
  }
  if (!ordinary.empty()) types.emplace_back(xpp::format("an ordinary point of a {} branch", joined(ordinary)));
  const std::string_view type = autox_point_kind_name(point.itp.kind);
  return Error{"autox",
               xpp::format("{} needs a point of type {}; label {} is {} {} point of a {} branch", autox_run_kind_name(kind),
                           joined(types), point.lab, std::string_view("aeiou").find(type.front()) == std::string_view::npos ? "a" : "an",
                           type, autox_family_name(family)),
               command_place()};
}

/* ---- what the start point is, and the problem each kind sets up ---- */

AutoxProblem second_problem(const AutoxParts &s, std::string_view point, AutoxFamily family)
{
  const bool discrete = solver_info(s.numerics->method).traits.discrete;
  if (point == "fold" || point == "hopf" || point == "neutral_saddle")
    return discrete ? AutoxProblem::DiscreteMap : family == AutoxFamily::boundary_value ? AutoxProblem::BoundaryValue : AutoxProblem::SteadyState;
  if (point == "cycle_fold") return family == AutoxFamily::boundary_value ? AutoxProblem::BoundaryValue : AutoxProblem::Periodic;
  return family == AutoxFamily::boundary_value ? AutoxProblem::BoundaryValue : family == AutoxFamily::periodic ? AutoxProblem::Periodic : AutoxProblem::SteadyState;
}

/* the run from the grabbed point, its kind already checked against the table */
Result<> run_from_point(AutoxParts s, const AutoxRunRequest &request, std::string_view point, AutoxFamily family)
{
  AutoxState &a = *s.autox_state;
  switch (request.kind) {
  case AutoxRunKind::equilibrium:
    return request.switch_branch ? autox_switch_ss(s) : autox_extend_ss(s);
  case AutoxRunKind::periodic:
    if (request.switch_branch) return point == "period_doubling" ? autox_period_double(s) : autox_switch_per(s);
    return autox_new_per(s);
  case AutoxRunKind::bvp:
    return request.switch_branch ? autox_switch_bvp(s) : autox_extend_bvp(s);
  case AutoxRunKind::fixed_period:
    return autox_2p_fixper(s);
  case AutoxRunKind::homoclinic:
  case AutoxRunKind::heteroclinic:
    a.homo_flag = request.kind == AutoxRunKind::homoclinic ? HomoclinicRun::Homoclinic : HomoclinicRun::Heteroclinic;
    return autox_extend_homoclinic(s);
  case AutoxRunKind::two_parameter:
    if (point == "hopf" || point == "neutral_saddle") return autox_2p_hopf(s);
    if (point == "fold" || point == "cycle_fold") return autox_2p_limit(s, second_problem(s, point, family));
    if (point == "period_doubling") return autox_2p_periodic(s, PD2);
    if (point == "neimark_sacker") return autox_2p_periodic(s, TR2);
    return autox_2p_branch(s, second_problem(s, point, family));
  }
  return {};
}

/* the equilibria of a homoclinic run: the request's, else the session's own, and they must fit the model */
Result<HomoclinicStart> homoclinic_start(const AutoxParts &s, const AutoxRunRequest &request)
{
  const AutoxState &a = *s.autox_state;
  const std::size_t n = static_cast<std::size_t>(s.model().node);
  HomoclinicStart start;
  if (request.homoclinic) start = *request.homoclinic;
  else start = HomoclinicStart{a.homo_equilibria.nunstab, a.homo_equilibria.nstab, a.homo_l, a.homo_r};
  if (start.left.empty()) start.left = a.homo_l;
  if (start.right.empty()) start.right = a.homo_r;
  /* HomCont's projection takes that many directions at each end: one condition for each of the equilibrium's dimensions */
  if (start.unstable < 0 || start.stable < 0 || static_cast<std::size_t>(start.unstable + start.stable) != n)
    return fail("autox", xpp::format("unstable_dimension ({}) and stable_dimension ({}) must add up to the number of variables, {}",
                                    start.unstable, start.stable, n), command_place());
  if (start.left.size() != n || start.right.size() != n)
    return fail("autox", xpp::format("the equilibria need one value for each of the {} variables", n), command_place());
  return start;
}

}  // namespace

AutoxFamily autox_family_of(const GRABPT &point)
{
  if (point.ibr < 0) return AutoxFamily::periodic;
  if (point.calc == BV1) return AutoxFamily::boundary_value;
  if (point.calc == HO2) return AutoxFamily::homoclinic;
  return AutoxFamily::steady;
}

const AutoxRunRow &autox_run_row_for(const GRABPT &point)
{
  const AutoxFamily family = autox_family_of(point);
  const std::string_view point_name = effective_point(point, family);
  for (const AutoxRunRow &row : autox_run_rows)
    if (point_name == row.point && (row.family == AutoxFamily::any || row.family == family)) return row;
  return autox_run_rows.back(); /* every `other` family has a row, so this is the last */
}

Result<> autox_time_check(const AutoxParts &s)
{
  const Model &m = s.model();
  std::vector<char> checked_fixed(static_cast<std::size_t>(m.fix_var), 0), checked_function(static_cast<std::size_t>(m.compiler.nfun), 0);
  /* whether the program reads t (the variable 0), directly or through a fixed variable or a user function */
  const auto reads_time = [&](const auto &self, const Program &program, int arguments) -> bool {
    const auto code = expr::decode(s.parser, program.rpn, arguments);
    if (!code) return false; /* a program the compiler refused never runs */
    for (const expr::Instr &i : *code) {
      if (i.type == VARTYPE && i.in == 0) return true;
      if (i.type == VARTYPE && i.in > m.node && i.in <= m.node + m.fix_var) {
        char &seen = checked_fixed[static_cast<std::size_t>(i.in - m.node - 1)];
        if (seen) continue;
        seen = 1;
        if (self(self, m.programs[static_cast<std::size_t>(i.in - 1)], -1)) return true;
      }
      if (i.type == UFUNTYPE && i.in >= 0 && i.in < m.compiler.nfun) {
        char &seen = checked_function[static_cast<std::size_t>(i.in)];
        if (seen) continue;
        seen = 1;
        if (self(self, m.compiler.ufun_programs[static_cast<std::size_t>(i.in)], m.compiler.narg_fun[static_cast<std::size_t>(i.in)])) return true;
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
  return fail("autox", xpp::format("autoX runs equations that do not depend on t, and evaluates them at t = 0; {} use t. Add t as a state "
                                  "(an oscillator, see \"Equations that use t\" in the autoX manual and examples/tstauto/frcp.odex)", list),
              first.line > 0 ? first : command_place());
}

Result<> autox_run_check(const AutoxParts &s, const AutoxRunRequest &request, const GRABPT *point, bool settling)
{
  const char *kind=autox_run_kind_name(request.kind);
  if (auto autonomous=autox_time_check(s); !autonomous) return autonomous;
  if(!request.from_point){
    if(request.switch_branch)return fail("autox",xpp::format("{} with switch starts from a stored point (a branch point or a period doubling)",kind),command_place());
    if(!(autox_run_current_kinds & autox_run_bit(request.kind)))return fail("autox",xpp::format("{} starts from a stored point: give its label",kind),command_place());
    if(request.kind==AutoxRunKind::homoclinic || request.kind==AutoxRunKind::heteroclinic){
      if(auto start=homoclinic_start(s,request); !start)return std::unexpected(start.error());
    }
    if(request.kind!=AutoxRunKind::equilibrium && s.data_store->rows<2 && !settling)
      return fail("autox",xpp::format("Integrate first: a {} run from the current state takes its orbit from the last integration, which stored {} row(s) (at least 2 are needed)",kind,s.data_store->rows),command_place());
    if(request.kind==AutoxRunKind::periodic && !settling){ /* rows >= 2 here; a run that settles first has no orbit yet */
      if(const Result<AutoxOrbitReturn> back=autox_orbit_return(s); !back)return std::unexpected(back.error());
    }
    return {};
  }
  const GRABPT &g=point?*point:s.autox_state->grabpt;
  if(g.flag==0 || g.lab==0)return fail("autox",xpp::format("{} starts from a labelled stored point: none is grabbed (autox.grab a label first)",kind),command_place());
  const AutoxFamily family=autox_family_of(g);
  const AutoxRunRow &row=autox_run_row_for(g);
  const bool needs_period = (!request.switch_branch && (request.kind==AutoxRunKind::equilibrium || request.kind==AutoxRunKind::periodic)) || (request.kind==AutoxRunKind::two_parameter && (std::string_view(row.point)=="hopf" || std::string_view(row.point)=="neutral_saddle"));
  if(needs_period)if(auto infinite=infinite_period(g); infinite)return std::unexpected(*infinite);
  if(!(row.kinds & autox_run_bit(request.kind)))return std::unexpected(kind_does_not_fit(g,family,request.kind));
  if(request.switch_branch && !(row.switch_kinds & autox_run_bit(request.kind)))
    return fail("autox",xpp::format("{} with switch needs a branch point or a period doubling; label {} is {} point",kind,g.lab,autox_point_kind_name(g.itp.kind)),command_place());
  return {};
}

/* the run itself: its start checked and set up, then autoX runs (autox_run_start says what it found) */
static Result<> start_run(AutoxParts s, const AutoxRunRequest &request)
{
  AutoxState &a = *s.autox_state;
  if(auto checked=autox_run_check(s,request); !checked)return checked;
  if (!request.from_point) {
    /* a homoclinic run's equilibria must fit the model, whatever else is wrong */
    const bool homoclinic = request.kind == AutoxRunKind::homoclinic || request.kind == AutoxRunKind::heteroclinic;
    std::optional<HomoclinicStart> start;
    if (homoclinic) {
      const Result<HomoclinicStart> checked = homoclinic_start(s, request);
      if (!checked) return std::unexpected(checked.error());
      start = *checked;
    }
    a.run_kind = request.kind;
    a.homo_flag = HomoclinicRun::None;
    switch (request.kind) {
    case AutoxRunKind::equilibrium:
      return solver_info(s.numerics->method).traits.discrete ? autox_new_discrete(s, request.replace_diagram) : autox_new_ss(s, request.replace_diagram);
    case AutoxRunKind::periodic:
      return autox_start_at_per(s);
    case AutoxRunKind::bvp:
      a.bifur.nbc = s.model().node;
      return autox_start_at_bvp(s);
    case AutoxRunKind::homoclinic:
    case AutoxRunKind::heteroclinic:
      a.homo_flag = request.kind == AutoxRunKind::homoclinic ? HomoclinicRun::Homoclinic : HomoclinicRun::Heteroclinic;
      return autox_start_at_homoclinic(s, *start);
    case AutoxRunKind::two_parameter:
    case AutoxRunKind::fixed_period:
      break;
    }
    return {};
  }
  const GRABPT &g = a.grabpt;
  const AutoxFamily family = autox_family_of(g);
  const AutoxRunRow &row = autox_run_row_for(g);
  a.run_kind = request.kind;
  return run_from_point(s, request, row.point, family);
}

std::vector<const char *> autox_run_kind_list(unsigned kinds)
{
  std::vector<const char *> names;
  for (std::size_t k = 0; k < autox_run_kind_names.size(); ++k)
    if (kinds & (1u << k)) names.push_back(autox_run_kind_names[k]);
  return names;
}

namespace {

/* a point type's name as the text says it: "branch point" */
std::string words(std::string_view name)
{
  std::string text(name);
  std::replace(text.begin(), text.end(), '_', ' ');
  return text;
}

/* a point found on a branch, in words: "fold (LP 23) at hfix = 0.2701" */
std::string found_text(const XppRunFound &f)
{
  std::string text = xpp::format("{} ({} {})", words(f.name), f.sym, f.lab);
  if (!f.par.empty()) text += xpp::format(" at {} = {}", f.par, xpp::number(f.x));
  return text;
}

/* what a run's end says (W860): per branch the points the run stored on it, the special ones among them with the value
   of their continuation parameter, why the branch ended (its own branch end), and what can be followed from each point
   found: the kinds its type starts in the table above. first is the diagram's first point of the run. */
XppRunSummary summarize_run(const AutoxParts &s, std::size_t first)
{
  XppRunSummary out;
  const AutoxState &a = *s.autox_state;
  bool followable = false; /* a found point a follow-all-branches run continues from: a branch point, a cycle's or a Hopf point */
  const std::size_t count = diagram_stored_points(s);
  for (std::size_t i = first; i < count; ++i) {
    const DIAGRAM &p = s.diagram->points[i];
    const long br = std::abs(static_cast<long>(p.ibr));
    auto branch = std::find_if(out.branches.begin(), out.branches.end(), [br](const XppRunBranch &b) { return b.br == br; });
    if (branch == out.branches.end()) {
      out.branches.push_back(XppRunBranch{br, {}});
      branch = out.branches.end() - 1;
    }
    const AutoxPointKind kind = p.itp.kind;
    if (kind == AutoxPointKind::Regular || kind == AutoxPointKind::Output || kind == AutoxPointKind::End || kind == AutoxPointKind::Pending) continue;
    followable = followable || kind == AutoxPointKind::BranchPoint || kind == AutoxPointKind::CycleBranch || kind == AutoxPointKind::Hopf;
    XppRunFound f;
    f.sym = std::string(autox_point_kind_mark(kind));
    f.lab = p.lab;
    f.name = std::string(autox_point_kind_name(kind));
    const char *par = p.parameter_indices.empty() ? nullptr : autox_par_name(s, p.parameter_indices[0]);
    f.par = par ? par : "";
    f.x = p.par.empty() ? NAN : p.par[0];
    branch->found.push_back(f);
    const GRABPT g = autox_point_data(p);
    const std::vector<const char *> kinds = autox_run_kind_list(autox_run_row_for(g).kinds);
    if (kinds.empty()) continue;
    XppRunFollow follow{f.sym, f.lab, {}};
    std::string kinds_text; /* "two-parameter", or "equilibrium or two-parameter" */
    for (const char *k : kinds) {
      follow.kinds.emplace_back(k);
      std::string name(k);
      std::replace(name.begin(), name.end(), '_', '-');
      kinds_text += (kinds_text.empty() ? "" : " or ") + name;
    }
    out.lines.push_back(xpp::format("From {} {}: {} {} continuation", f.sym, f.lab, kinds_text, words(autox_run_row_for(g).point)));
    out.follow.push_back(std::move(follow));
  }
  /* the branch ends of this run: each branch's last end says why it stopped */
  for (XppRunBranch &branch : out.branches) {
    for (const AutoxBranchEnd &end : a.stop.ends) {
      if (end.br != branch.br) continue;
      branch.stopped = true;
      branch.why = end.key;
      branch.pt = end.pt;
      branch.text = end.text;
    }
  }
  /* the lines autoX's Output shows, branch by branch, then what follows; the same text the event carries */
  std::vector<std::string> lines;
  for (const XppRunBranch &branch : out.branches) {
    std::string text = xpp::format("Branch {}: ", branch.br);
    if (branch.found.empty()) text += "no special point found";
    for (std::size_t k = 0; k < branch.found.size(); ++k) text += (k ? "; " : "") + found_text(branch.found[k]);
    if (branch.stopped) text += xpp::format("; stopped at point {}: {}", branch.pt, branch.text);
    lines.push_back(std::move(text));
  }
  lines.insert(lines.end(), out.lines.begin(), out.lines.end());
  if (a.follow_all_branches && a.stop.followed == 0 && !followable && !out.branches.empty())
    lines.push_back("No branch point or Hopf point to follow");
  out.lines = std::move(lines);
  return out;
}

} // namespace

/* a run ends: what it found and can be followed from is logged in autoX's Output and sent (autox.summary), whatever ended
   the run (its start refused, an error, a stop: what it stored says what it found) */
Result<> autox_run_start(AutoxParts s, const AutoxRunRequest &request)
{
  AutoxState &a = *s.autox_state;
  a.stop.ends.clear();
  a.stop.followed = 0;
  const std::size_t before = diagram_stored_points(s);
  const Result<> ran = start_run(s, request);
  if (diagram_stored_points(s) > before || !a.stop.ends.empty()) {
    const std::size_t first = request.replace_diagram && !request.from_point ? 0 : before;
    const XppRunSummary summary = summarize_run(s, first);
    for (const std::string &line : summary.lines) xpp::log_autox("{}\n", line);
    autox_summary(summary);
  }
  return ran;
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
  for (const char *name : autox_run_kind_list(kinds)) {
    if (!first) out += ',';
    first = false;
    json_append_string(out, name);
  }
  out += ']';
}

}  // namespace

std::string autox_run_table_json()
{
  std::string out = "{\"kinds\":[";
  for (std::size_t k = 0; k < autox_run_kind_names.size(); ++k) {
    if (k) out += ',';
    json_append_string(out, autox_run_kind_names[k]);
  }
  out += "],\"current\":";
  append_kinds(out, autox_run_current_kinds);
  out += ",\"rows\":[";
  for (std::size_t i = 0; i < autox_run_rows.size(); ++i) {
    const AutoxRunRow &row = autox_run_rows[i];
    out += xpp::format("{}{{\"point\":", i ? "," : "");
    json_append_string(out, row.point);
    out += ",\"family\":";
    json_append_string(out, autox_family_name(row.family));
    out += ",\"kinds\":";
    append_kinds(out, row.kinds);
    out += ",\"switch\":";
    append_kinds(out, row.switch_kinds);
    out += '}';
  }
  out += "]}";
  return out;
}

std::string autox_run_start_json(const AutoxParts &s)
{
  const GRABPT &g = s.autox_state->grabpt;
  if (g.flag == 0) return "null";
  const AutoxRunRow &row = autox_run_row_for(g);
  std::string out = xpp::format("{{\"label\":{},\"type\":", g.lab);
  json_append_string(out, autox_point_kind_name(g.itp.kind));
  out += ",\"family\":";
  json_append_string(out, autox_family_name(autox_family_of(g)));
  out += ",\"kinds\":";
  append_kinds(out, g.lab == 0 ? 0 : row.kinds);
  out += ",\"switch\":";
  append_kinds(out, g.lab == 0 ? 0 : row.switch_kinds);
  return out + '}';
}

}  // namespace xpp
