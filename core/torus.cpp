#include "model.h"
#include "session.h"
#include "torus.h"
#include "xpp_ui.h"
#include "form_ode.h"
#include "load_eqn.h"

#include <array>

void do_torus_com(xpp::Session &s, int c)
{
 int i;
 s.numerics.torus=0;
 if(c==0||c==2){
   new_float(s,"Period :",&s.numerics.torus_period);
   if(s.numerics.torus_period<=0.0){
     xpp::command_error("torus", "Choose positive period");
     return;
   }
   if(c==0){
     for(i=0;i<MAXODE;i++)s.itor[i]=1;
     s.numerics.torus=1;
     return;
   }
   /* Choose them   */
   choose_torus(s);
   return;
 }
 for(i=0;i<MAXODE;i++)s.itor[i]=0;
 s.numerics.torus=0;
}

void choose_torus(xpp::Session &s)
{
 int i;
 std::array<const char *, MAXODE> names{};
 for(i=0;i<s.model().neq;i++)names[i]=s.model().uvar_names[i].c_str();
 xpp::ui.checklist("Fold which",names.data(),s.itor.data(),s.model().neq);
 for(i=0;i<s.model().neq;i++)if(s.itor[i]==1)s.numerics.torus=1;
}
