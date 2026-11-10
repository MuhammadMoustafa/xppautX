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

} // namespace xpp
