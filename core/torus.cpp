#include "model.h"
#include "session.h"
#include "torus.h"
#include "xpp_ui.h"
#include "form_ode.h"
#include "load_eqn.h"

#include <array>

void do_torus_com(int c)
{
 int i;
 xpp::session().numerics.torus=0;
 if(c==0||c==2){
   new_float("Period :",&xpp::session().numerics.tor_period);
   if(xpp::session().numerics.tor_period<=0.0){
     err_msg("Choose positive period");
     return;
   }
   if(c==0){
     for(i=0;i<MAXODE;i++)xpp::session().itor[i]=1;
     xpp::session().numerics.torus=1;
     return;
   }
   /* Choose them   */
   choose_torus();
   return;
 }
 for(i=0;i<MAXODE;i++)xpp::session().itor[i]=0;
 xpp::session().numerics.torus=0;
}

void choose_torus()
{
 int i;
 std::array<const char *, MAXODE> names{};
 for(i=0;i<xpp::model().neq;i++)names[i]=xpp::model().uvar_names[i].c_str();
 xpp_ui.checklist("Fold which",names.data(),xpp::session().itor.data(),xpp::model().neq);
 for(i=0;i<xpp::model().neq;i++)if(xpp::session().itor[i]==1)xpp::session().numerics.torus=1;
}
