#include "torus.h"
#include "xpp_ui.h"
#include "xpplim.h"

extern int NEQ;
extern char uvar_names[MAXODE][12];
extern int TORUS;
extern double TOR_PERIOD;
extern int itor[MAXODE];

void do_torus_com(int c)
{
 int i;
 TORUS=0;
 if(c==0||c==2){
   new_float("Period :",&TOR_PERIOD);
   if(TOR_PERIOD<=0.0){
     err_msg("Choose positive period");
     return;
   }
   if(c==0){
     for(i=0;i<MAXODE;i++)itor[i]=1;
     TORUS=1;
     return;
   }
   /* Choose them   */
   choose_torus();
   return;
 }
 for(i=0;i<MAXODE;i++)itor[i]=0;
 TORUS=0;
}

void choose_torus()
{
 int i;
 char *names[MAXODE];
 for(i=0;i<NEQ;i++)names[i]=uvar_names[i];
 xpp_ui.checklist("Fold which",names,itor,NEQ);
 for(i=0;i<NEQ;i++)if(itor[i]==1)TORUS=1;
}
