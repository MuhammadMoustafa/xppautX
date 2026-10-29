#include "model.h"
#include "session.h"
#include "torus.h"
#include "xpp_ui.h"
#include "form_ode.h"
#include "load_eqn.h"

#include <algorithm>
#include <array>

namespace xpp {

Result<> set_torus(Session &s, TorusMode mode, std::optional<double> period, std::optional<std::vector<int>> chosen)
{
 if(mode==TorusMode::none){
   std::ranges::fill(s.itor,0);
   s.numerics.torus=false;
   return {};
 }
 const double length=period.value_or(s.numerics.torus_period);
 if(!(length>0.0))
   return fail("torus",xpp::format("Choose positive period, not {}",xpp::number(length)),command_place());
 if(mode==TorusMode::all)std::ranges::fill(s.itor,1);
 else if(chosen){
   std::ranges::fill(s.itor,0);
   for(const int i:*chosen)s.itor[static_cast<std::size_t>(i)]=1;
 }
 s.numerics.torus_period=length;
 s.numerics.torus=mode==TorusMode::all||std::ranges::any_of(s.itor,[](int on){return on==1;});
 return {};
}

void do_torus_com(Session &s, int c)
{
 /* the Phase space items of an imported button sequence (kbs.h): the questions the registry's analysis.phase_space_torus takes as arguments are asked here, then the command's own function runs */
 const TorusMode mode=c==0?TorusMode::all:c==1?TorusMode::none:TorusMode::choose;
 if(mode==TorusMode::none){
   xpp::ok_or_show(set_torus(s,mode,{},{}));
   return;
 }
 double period=s.numerics.torus_period;
 new_float(s,"Period :",&period);
 if(mode==TorusMode::all){
   xpp::ok_or_show(set_torus(s,mode,period,{}));
   return;
 }
 /* Choose them   */
 std::vector<int> on(s.itor.begin(),s.itor.begin()+s.model().neq);
 std::vector<const char *> names(s.model().neq);
 for(int i=0;i<s.model().neq;i++)names[i]=s.model().uvar_names[i].c_str();
 xpp::ui.checklist("Fold which",names.data(),on.data(),s.model().neq);
 std::vector<int> chosen;
 for(int i=0;i<s.model().neq;i++)if(on[i]==1)chosen.push_back(i);
 xpp::ok_or_show(set_torus(s,mode,period,chosen));
}

} // namespace xpp
