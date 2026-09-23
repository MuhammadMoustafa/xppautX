
#include "edit_rhs.h"
#include "xpp_mem.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "extra.h"
#include "parserslow.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef WCTYPE
#include <ctype.h>
#else
#include <wctype.h>
#endif

#include "xpplim.h"
#include "struct.h"
#include "shoot.h"
#include "load_eqn.h"





char *get_next(),*get_first();


extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern char *ode_names[MAXODE];
extern int METHOD,NEQ,NODE,NMarkov,FIX_VAR;

extern int *my_ode[];
extern int NUPAR;
extern double last_ic[MAXODE];

/*extern char upar_names[MAXPAR][XPP_NAME_MAX+1],this_file[100];*/

extern char upar_names[MAXPAR][XPP_NAME_MAX+1],this_file[XPP_MAX_NAME];
extern int EqType[MAXODE];

extern char *ufun_def[MAXUFUN];
extern char ufun_names[MAXUFUN][XPP_NAME_MAX+1];
extern int narg_fun[MAXUFUN], *ufun[MAXUFUN];



extern UFUN_ARG ufun_arg[MAXUFUN];
extern BC_STRUCT my_bc[MAXODE];

extern int NFUN;






  

	

void edit_rhs()
{
 char **names,**values;
 int **command;
 int i,status,err,len,i0,j;
 int n=NEQ;
 char fstr[20],msg[200];
 if(NEQ>NEQMAXFOREDIT) return;
 names=(char **)xpp_malloc(n*sizeof(char*));
 values=(char **)xpp_malloc(n*sizeof(char*));
 command=(int **)xpp_malloc(n*sizeof(int*));
 for(i=0;i<n;i++){
   values[i]=(char *)xpp_malloc(MAX_LEN_EBOX*sizeof(char));
   names[i]=(char *)xpp_malloc(MAX_LEN_EBOX+3*XPP_NAME_MAX);
   command[i]=(int *)xpp_malloc(200*sizeof(int));
   if(i<NODE &&METHOD>0)strcpy(fstr,"d%s/dT");
   if(i<NODE &&METHOD==0)strcpy(fstr,"%s(n+1)");
   if(i<NODE &&EqType[i]==1)strcpy(fstr,"%s(T)");
   if(i>=NODE)strcpy(fstr,"%s");
   sprintf(names[i],fstr,uvar_names[i]);
   strcpy(values[i],ode_names[i]);
 }
 status=do_edit_box(n,"Right Hand Sides",names,values);
 if(status!=0){
  
   for(i=0;i<n;i++){
     if(i<NODE||(i>=(NODE+NMarkov))){
      
       err=add_expr(values[i],command[i],&len);
       if(err==1)
	 {
	   snprintf(msg,sizeof(msg),"Bad rhs:%s=%s",names[i],values[i]);
	   err_msg(msg);
	 }
       else 
	 {
	   xpp_free(ode_names[i]);
	   ode_names[i]=(char *)xpp_malloc(strlen(values[i])+5);
	   strcpy(ode_names[i],values[i]);
	   i0=i;
	   if(i>=NODE)i0=i0+FIX_VAR-NMarkov;
         
	   for(j=0;j<len;j++)
	     my_ode[i0][j]=command[i][j];
	 }
     }
   }
 }
     

 for(i=0;i<n;i++){
   xpp_free(values[i]);
   xpp_free(names[i]);
   xpp_free(command[i]);
 }
 xpp_free(values);
 xpp_free(names);
 xpp_free(command);
}

void edit_functions()
{
 char **names,**values;
 int **command;
 int i,status,err,len,j;
 int n=NFUN;
 char msg[200];
 if(n==0||n>NEQMAXFOREDIT)return;
 names=(char **)xpp_malloc(n*sizeof(char*));
 values=(char **)xpp_malloc(n*sizeof(char*));
 command=(int **)xpp_malloc(n*sizeof(int*));
 for(i=0;i<n;i++){
   values[i]=(char *)xpp_malloc(MAX_LEN_EBOX*sizeof(char));
   names[i]=(char *)xpp_malloc(MAX_LEN_EBOX+3*XPP_NAME_MAX);
   command[i]=(int *)xpp_malloc(200*sizeof(int));
   sprintf(values[i],"%s",ufun_def[i]);

   if(narg_fun[i]==0){
     sprintf(names[i],"%s()",ufun_names[i]);
   }
   if(narg_fun[i]==1){
     sprintf(names[i],"%s(%s)",ufun_names[i],
			     ufun_arg[i].args[0]);
   }
   if(narg_fun[i]>1)sprintf(names[i],"%s(%s,...,%s)",ufun_names[i],
			    ufun_arg[i].args[0],
			    ufun_arg[i].args[narg_fun[i]-1]);

			   
 }

 status=do_edit_box(n,"Functions",names,values);
 if(status!=0){
  
   for(i=0;i<n;i++){
     set_new_arg_names(narg_fun[i],ufun_arg[i].args);
     err=add_expr(values[i],command[i],&len);
     set_old_arg_names(narg_fun[i]);
     if(err==1){
       snprintf(msg,sizeof(msg),"Bad func.:%s=%s",names[i],values[i]);
       err_msg(msg);
     }
     else {
       strcpy(ufun_def[i],values[i]);
       for(j=0;j<=len;j++){
         /* plintf("f(%d)[%d]=%d %d \n",i,j,command[i][j],ufun[i][j]); */
         ufun[i][j]=command[i][j];
	 
       }
              fixup_endfun(ufun[i],len,narg_fun[i]);

     }

   }
 }
 

 for(i=0;i<n;i++){
   xpp_free(values[i]);
   xpp_free(names[i]);
   xpp_free(command[i]);
 }
 xpp_free(values);
 xpp_free(names);
 xpp_free(command);

}

int save_as()
{
  int i,ok;
  FILE *fp;
  double z;
  char filename[256];
  snprintf(filename,sizeof(filename),"%.255s",this_file);
  ping();
  /* if(new_string("Filename: ",filename)==0)return; */
  if(!file_selector("Save As",filename,"*.ode"))return(-1);
  open_write_file(&fp,filename,&ok); 
   if(!ok)return(-1);
  fp=fopen(filename,"w");
  if(fp==NULL)return(0);
  fprintf(fp,"%d",NEQ);
  for(i=0;i<NODE;i++){
    if(i%5==0)fprintf(fp,"\nvariable ");
    fprintf(fp," %s=%.16g ",uvar_names[i],last_ic[i]);
  }
  fprintf(fp,"\n");
  for(i=NODE;i<NEQ;i++){
    if((i-NODE)%5==0)fprintf(fp,"\naux ");
    fprintf(fp," %s ",uvar_names[i]);
  }
  fprintf(fp,"\n");
  for(i=0;i<NUPAR;i++){
    if(i%5==0)fprintf(fp,"\nparam  ");
    get_val(upar_names[i],&z);
    fprintf(fp," %s=%.16g   ",upar_names[i],z);
  }
  fprintf(fp,"\n");
  for(i=0;i<NFUN;i++){
    fprintf(fp, "user %s %d %s\n",ufun_names[i],narg_fun[i],ufun_def[i]);
  }
  for(i=0;i<NODE;i++){
    if(EqType[i]==1)fprintf(fp,"i ");
    else fprintf(fp,"o ");
    fprintf(fp,"%s\n",ode_names[i]);
  }
  for(i=NODE;i<NEQ;i++)
    fprintf(fp,"o %s\n",ode_names[i]);
  for(i=0;i<NODE;i++)fprintf(fp,"b %s \n",my_bc[i].string);
  fprintf(fp,"done\n");
  fclose(fp);
  
  return(1);
}
