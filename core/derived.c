#include "derived.h"
#include "xpp_mem.h"

#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include "ggets.h"
#include "parserslow.h"
#include "calc.h"
#include "xpp_io.h"

/* Derived parameter stuff !!  */
#define MAXDERIVED 200
extern double constants[];
extern int NCON;
typedef struct {
  int index,*form;
  char *rhs;
  double value;
} DERIVED;

DERIVED derived[MAXDERIVED];
int nderived=0; 

/* This compiles all of the formulae 
It is called only once during the session
*/
int compile_derived()
{
  int i,k;
  int f[256],n;
  for(i=0;i<nderived;i++){
    if(add_expr(derived[i].rhs,f,&n)==1){
    plintf(" Bad right-hand side for derived parameters \n");
    return(1);
    }
    derived[i].form=(int *)xpp_malloc(sizeof(int)*(n+2));
    for(k=0;k<n;k++)
      derived[i].form[k]=f[k];
  }
 evaluate_derived();
 return 0;
}

/* This evaluates all derived quantities in order of definition 
called before any integration or numerical computation
and after changing parameters and constants
*/
void evaluate_derived()
{
  int i;
  for(i=0;i<nderived;i++){
    derived[i].value=evaluate(derived[i].form);
    constants[derived[i].index]=derived[i].value;
  
  }
}

/* this adds a derived quantity  */
int add_derived(char *name, char *rhs)
{
  int n=strlen(rhs)+2;
  int i0;
  if(nderived>=MAXDERIVED){
    plintf(" Too many derived constants! \n");
    return(1);
  }
  i0=nderived;
  derived[i0].rhs=(char *)xpp_malloc(n);
  /* save the right hand side; derived[i0].rhs is a pointer, allocated
     n = strlen(rhs)+2 bytes just above. */
  xpp_strlcpy(derived[i0].rhs,rhs,n);
  /* this is the constant to which it addresses */
  derived[i0].index=NCON;
  /* add the name to the recognized symbols */
  plintf(" derived constant[%d] is %s = %s\n",NCON,name,rhs);
  nderived++;
  return(add_con(name,0.0));
}
  
    




