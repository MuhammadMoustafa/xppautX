/* Pure helpers that used to live in X11 source files (init_conds.c,
   aniparse.c, graf_par.c, calc.c, many_pops.c, main.c). Nothing here
   touches a window. */
#include "xpp_util.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "parserslow.h"
#include "browse.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PARAMBOX 1
#define ICBOX 2
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6
#define lmax(a, b) ((a) > (b) ? (a) : (b))

extern int NUPAR, NEQ;
extern char upar_names[MAXPAR][11], uvar_names[MAXODE][12];
extern double last_ic[MAXODE];
extern int NCON, NSYM, NCON_START, NSYM_START;
extern BROWSER my_browser;
void do_axes(void);

/* ---- graph bookkeeping (was many_pops.c / main.c) ----------------------- */

void restore_off(void) { MyGraph->Restore = 0; }
void restore_on(void) { MyGraph->Restore = 1; }

void make_active(int i, int flag)
{
    current_pop = i;
    MyGraph = &graph[current_pop];
    xpp_ui.activate_graph(i, flag);
}

void clr_scrn(void)
{
    xpp_ui.blank_draw_window();
    restore_off();
    do_axes();
}

/* ---- moved function bodies follow (appended by tools/move_funcs.py) ---- */

void ind_to_sym(ind,str)
 char *str;
 int ind;
{
 if(ind==0)strcpy(str,"T");
 else strcpy(str,uvar_names[ind-1]);
} 

void  get_max(index, vmin,vmax)
  double *vmax,*vmin;
  int index;
  {
   float x0,x1,z;
   double temp;
   int i;
   x0=my_browser.data[index][0];
   x1=x0;
   for(i=0;i<my_browser.maxrow;i++)
   {
    z=my_browser.data[index][i];
    if(z<x0)x0=z;
    if(z>x1)x1=z;
   }
   *vmin=(double)x0;
   *vmax=(double)x1;
    if(fabs(*vmin-*vmax)<REAL_SMALL){
      temp=.05*lmax(fabs(*vmin),1.0);
     *vmin=*vmin-temp;
     *vmax=*vmax+temp;
    }
 
 }

void de_space(s)
     char *s;
{
  int n=strlen(s);
  int i,j=0;
  char ch;
  for(i=0;i<n;i++){
    ch=s[i];
    if(!isspace(ch)){
      s[j]=ch;
      j++;
    }
  }
  s[j]=0;
}

int find_user_name(type,oname)
int type;
char *oname;
{
 char name[25];
 int j=0,k=0,i=-1;
 for(j=0;j<strlen(oname);j++){
 if(!isspace(oname[j])){name[k]=oname[j];k++;}
}
 name[k]=0;
  
 
 for(i=0;i<NUPAR;i++)
         if((type==PARAMBOX)&&(strcasecmp(upar_names[i],name)==0))break;
 if(i<NUPAR)return(i);
 for(i=0;i<NEQ;i++)
	 if((type==ICBOX)&&(strcasecmp(uvar_names[i],name)==0))break;	
   if(i<NEQ)return(i);
	return(-1);
 }

int do_calc(temp,z)
char *temp;
double *z;
 {
 char val[15];
 int ok; 
 int i;
 double newz;
 if(strlen(temp)==0){
	*z=0.0;
	return(1);
	}
 if(has_eq(temp,val,&i))
 {
 
 
  newz=calculate(&temp[i],&ok);  /*  calculate quantity  */
 
  if(ok==0)return(-1);
  i=find_user_name(PARAM,val);
  if(i>-1){
    set_val(val,newz); /* a parameter set to value  */
    *z=newz;
    redraw_params();
  }
  else {
    i=find_user_name(IC,val);
    if(i<0){
      err_msg("No such name!");
      return(-1);
    }
    set_val(val,newz);

    last_ic[i]=newz;
    *z=newz;
    redraw_ics();
  }
    return(0);
}
	    
  newz=calculate(temp,&ok);
  if(ok==0)return(-1);
 *z=newz;
 return(1);
}

int has_eq(z, w, where)
 int *where;
 char *z,*w;
 {
  int i;
  for(i=0;i<strlen(z);i++)
   if(z[i]==':')break;
  if(i==strlen(z))return(0);
  strncpy(w,z,i);
  w[i]=0;
  *where=i+1;
  return(1);
 }

 double calculate(expr,ok)
 char *expr;
 int *ok;
 {
  int com[400],i;
  double z=0.0;
    if(add_expr(expr,com,&i)){
     err_msg("Illegal formula ..");
     *ok=0;
      goto bye;
   }
    /* fpr_command(com); */
  z=evaluate(com);
 *ok=1;
bye:
  /* plintf(" old=%d %d  new = %d %d \n",NCON,NSYM,NCON_START,NSYM_START);  */
  NCON=NCON_START;
  NSYM=NSYM_START;
  return(z);
 }
