/* Pure helpers that used to live in X11 source files (init_conds.c,
   aniparse.c, graf_par.c, calc.c, many_pops.c, main.c). Nothing here
   touches a window. */
#include "xpp_util.h"
#include "xpp_ui.h"
#include "grobs.h"
#include "xpp_globals.h"
#include "parserslow.h"
#include "browse.h"
#include "graf_par.h"
#include "integrate.h"
#include "nullcline.h"
#include "many_pops.h"
#include "my_ps.h"
#include "my_svg.h"
#include "tabular.h"
#include "volterra2.h"
#include "derived.h"
#include "xpplim.h"
#include "form_ode.h"
#include "shoot.h"
#include "lunch-new.h"
#include "delay_handle.h"
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PARAMBOX 1
#define ICBOX 2
#define DELAYBOX 3
#define BCBOX 4
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6
#define lmax(a, b) ((a) > (b) ? (a) : (b))

extern int NUPAR, NEQ;
extern char upar_names[MAXPAR][11], uvar_names[MAXODE][12];
extern double last_ic[MAXODE];
extern int NCON, NSYM, NCON_START, NSYM_START;
extern BROWSER my_browser;
extern char this_file[XPP_MAX_NAME];
extern char this_internset[XPP_MAX_NAME];
extern char *ufun_def[MAXUFUN];
extern char ufun_names[MAXUFUN][12];
extern int narg_fun[MAXUFUN];
extern UFUN_ARG ufun_arg[MAXUFUN];
extern int NFUN;
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

/* new_parameter, set_default_params, clone_ode: from init_conds.c */
extern double default_val[MAXPAR];
extern char *save_eqn[MAXLINES];
extern int NLINES, NMarkov, NODE, NUPAR;
#define READEM 1
#define WRITEM 0
extern BC_STRUCT my_bc[MAXODE];


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

void set_active_windows()
{
  int i,np=0;
   for(i=0;i<MAXPOP;i++){
   if(graph[i].Use==1){
     ActiveWinList[np]=i;
     np++;
   }
 }
 num_pops=np;
}  

void check_windows()
{
 double zip,zap;
 check_val(&MyGraph->xmin,&MyGraph->xmax,&MyGraph->xbar,&MyGraph->dx);
 check_val(&MyGraph->ymin,&MyGraph->ymax,&MyGraph->ybar,&MyGraph->dy);
 check_val(&MyGraph->zmin,&MyGraph->zmax,&MyGraph->zbar,&MyGraph->dz);
 check_val(&MyGraph->xlo,&MyGraph->xhi,&zip,&zap);
 check_val(&MyGraph->ylo,&MyGraph->yhi,&zip,&zap);
} 

void check_val(x1,x2,xb,xd)
 double *x1,*x2,*xb,*xd;
{
 double temp;

/* 
  see get_max for details
*/   
      
 if(*x1==*x2){
   temp=.05*lmax(fabs(*x1),1.0);
   *x1=*x1-temp;
   *x2=*x2+temp;
 }
 if(*x1>*x2){
	     temp=*x2;
             *x2=*x1;
             *x1=temp;
	     
            }
	    *xb=.5*(*x1+*x2);
	    *xd=2.0/(*x2-*x1);

}

void dump_ps(int i)
{  
  char filename[XPP_MAX_NAME];
   if(i<0)
     {
       sprintf(filename,"%s%s.%s",this_file,this_internset,PlotFormat);
     }
   else 
     {
       /*   padnum(s,i,4); */
       sprintf(filename,"%s%s_%04d.%s",this_file,this_internset,i,PlotFormat);
     }   
      
   if (strcmp(PlotFormat,"ps")==0)
   {
     if(ps_init(filename,PS_Color))
     {
       ps_restore();
     }
   }
   else if (strcmp(PlotFormat,"svg")==0)
   {
     if(svg_init(filename,PS_Color))
     {
       svg_restore();
     }
   }
}

void   redo_stuff()
    {
      evaluate_derived();
   re_evaluate_kernels();
	  redo_all_fun_tables();
        evaluate_derived();
}

void user_fun_info(fp)
     FILE *fp;
{
  char fundef[256];
  int i,j;
  for(j=0;j<NFUN;j++){
    sprintf(fundef,"%s(",ufun_names[j]);
    for(i=0;i<narg_fun[j];i++){
      strcat(fundef,ufun_arg[j].args[i]);
      if(i<narg_fun[j]-1)
	strcat(fundef,",");
    }
    strcat(fundef,") = ");
    strcat(fundef,ufun_def[j]);
    fprintf(fp,"%s\n",fundef);
  }
}

void ps_restore()
{
  if(Xup){
 redraw_dfield();
 ps_do_color(0);
 if(MyGraph->Nullrestore){restore_nullclines();ps_stroke();}
  }
 ps_last_pt_off(); 

  restore(0,my_browser.maxrow);  
 
  do_batch_nclines();
  do_batch_dfield(); 
 do_axes(); 
  
 ps_do_color(0); 
 if(Xup){
 draw_label(draw_win);
 xpp_ui.draw_freeze();
 }
 ps_end();
}

void svg_restore()
{
 
/* restore(0,my_browser.maxrow);
*/
 /*ps_do_color(0);
 if(MyGraph->Nullrestore){restore_nullclines();ps_stroke();}
  */
  
  redraw_dfield();
 if(MyGraph->Nullrestore){restore_nullclines();}
  svg_last_pt_off();
 /*ps_do_color(0);*/ 
 restore(0,my_browser.maxrow);
 do_axes();
 if(Xup){
 draw_label(draw_win);
 xpp_ui.draw_freeze();
 }
  do_batch_nclines();
  do_batch_dfield(); 
 svg_end();
}

void clone_ode()
{
  int i,j,x,y;
  FILE *fp;
  char clone[256];
  
  char *s;
  time_t ttt;
  double z;
  clone[0]=0;
  if(!file_selector("Clone ODE file",clone,"*.ode"))return;
  if((fp=fopen(clone,"w"))==NULL){
      err_msg(" Cant open clone file");
      return;
    }
  ttt=time(0);
  fprintf(fp,"# clone of %s on %s",this_file,ctime(&ttt));
  for(i=0;i<NLINES;i++){
    s=save_eqn[i];
    
    if(s[0]=='p'||s[0]=='P'||s[0]=='b'||s[0]=='B'){
      x=find_char(s,"'",0,&j);
      y=find_char(s,"=",0,&j);

      if(x!=0||y!=0){
	fprintf(fp,"# original\n# %s\n",s);
	continue;
      }
    }
    if(strncasecmp("done",s,4)==0)continue;
    fprintf(fp,"%s\n",s);
  }
  fprintf(fp,"# Cloned parameters etc here\n");
  /* now we do parameters boundary conds and ICs */
  j=0;
  fprintf(fp,"init ");
  for(i=0;i<(NODE+NMarkov);i++){
    if(j==8){
      fprintf(fp,"\ninit ");
      j=0;
    }
    
    fprintf(fp,"%s=%g ",uvar_names[i],last_ic[i]);
    j++;
  }
  fprintf(fp,"\n");

  /* BDRY conds */
  if(my_bc[0].string[0]!='0'){
    for(i=0;i<NODE;i++)
      fprintf(fp,"bdry %s\n",my_bc[i].string);
  }
  j=0;
  if(NUPAR>0){
    
    fprintf(fp,"par ");
    for(i=0;i<NUPAR;i++){
      if(j==8){
	fprintf(fp,"\npar ");
      j=0;
    }
      get_val(upar_names[i],&z); 
      fprintf(fp,"%s=%g ",upar_names[i],z);
    j++;
    }
  }
    fprintf(fp,"\n");
  fprintf(fp,"done \n");
  fclose(fp);
}

void new_parameter()
{
  int done,index;
  double z;
  char name[256],value[256],junk[256];
  while(1){
    name[0]=0;
    done=new_string("Parameter:",name);
    if(strlen(name)==0||done==0){redo_stuff(); return;}
    if(strncasecmp(name,"DEFAULT",7  )==0){
      set_default_params();
      continue;
    }

    if(strncasecmp(name,"!LOAD", 5 )==0){
      io_parameter_file(name,READEM);
      continue;
    }
    if(strncasecmp(name,"!SAVE", 5 )==0){
      io_parameter_file(name,WRITEM);
      continue;
    }
    
    else {
      index=find_user_name(PARAMBOX,name);
      if(index>=0){
	get_val(upar_names[index],&z);
	sprintf(value,"%s :",name);
	done=new_float(value,&z);
	if(done==0){
	  set_val(upar_names[index],z);
	  sprintf(junk,"%.16g",z);
	  xpp_ui.param_box_set(index,junk);
	  xpp_ui.param_box_redraw(index);
	}
        if(done==-1){
         redo_stuff();
	  return;
	}
      }
    }
  }
}

void   set_default_params()
 {

 int i;
 char junk[256];
 for(i=0;i<NUPAR;i++){
   set_val(upar_names[i],default_val[i]);
   sprintf(junk,"%.16g",default_val[i]);
   xpp_ui.param_box_set(i,junk);
 }
 
 redraw_params();
 re_evaluate_kernels();
 redo_all_fun_tables(); 
 }

/* ---- the values behind the IC, parameter, BC and delay boxes and the
   parameter sliders (logic from init_conds.c; the widgets stay there) ---- */
extern char delay_string[MAXODE][80];
extern double default_ic[MAXODE];
extern double DELAY;
extern int SuppressBounds;

void   set_default_ics()
{
  int i;
  for(i=0;i<NODE+NMarkov;i++)
    last_ic[i]=default_ic[i];
   redraw_ics();
}

int to_float(s,z)
     char *s;
     double *z;
{
  int flag;
  *z=0.0;
  if(s[0]=='%')
    {
      flag=do_calc(&s[1],z);
      if(flag==-1)return -1;
      return 0;
    }
  *z=atof(s);
  return(0);
}

void man_ic()
{
  int done,index=0;
  double z;
  char name[256],junk[256];
  while(1){
    sprintf(name,"%s :",uvar_names[index]);
    z=last_ic[index];
    done=new_float(name,&z);
    if(done==0){
      last_ic[index]=z;
      sprintf(junk,"%.16g",z);
      xpp_ui.ic_box_set(index,junk);
      xpp_ui.ic_box_redraw(index);
      index++;
      if(index>=NODE+NMarkov)return;
    }
    if(done==-1)return;
  }
}

/* store the text s typed for entry i of a box of the given type. Numbers
   (ICs, parameters) come back in *z and the result is 1; BCs and delays are
   strings (0); -1 when a %formula does not evaluate. */
int box_set_value(int type,int i,char *s,double *z)
{
  *z=0.0;
  switch(type){
  case ICBOX:
    if(to_float(s,z)==-1)return -1;
    last_ic[i]=*z;
    return 1;
  case PARAMBOX:
    if(to_float(s,z)==-1)return -1;
    set_val(upar_names[i],*z);
    return 1;
  case BCBOX:
    strcpy(my_bc[i].string,s);
    return 0;
  case DELAYBOX:
    strcpy(delay_string[i],s);
    return 0;
  }
  return 0;
}

/* every entry of a box was just stored: recompute what depends on them */
void box_values_loaded(int type)
{
  if(type==PARAMBOX){
    re_evaluate_kernels();
    redo_all_fun_tables();
  }
  if(type==DELAYBOX){
   do_init_delay(DELAY);
  }
}

/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot the checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(int how,int *isck,int n)
{
  int i;
  int plot_list[10];
  int k=0,max=(how==0)?10:3;
  for(i=0;i<n;i++)
    if(isck[i]){
      if(k<max){
	plot_list[k]=i+1;
	k++;
      }
      isck[i]=0;
    }
  if(how==0&&k>0)
    graph_all(plot_list,k,0);
  if(how==1&&k>1)
    graph_all(plot_list,k,1);
}

/* a slider names a parameter (PARAMBOX) or a variable (ICBOX); 0 if
   neither */
int find_par_or_var(char *name,int *type,int *index)
{
  int status=find_user_name(PARAMBOX,name);
  if(status==-1){
    status=find_user_name(ICBOX,name);
    if(status==-1)return 0;
    *type=ICBOX;
  }
  else *type=PARAMBOX;
  *index=status;
  return 1;
}

void set_par_or_var(char *name,int type,int index,double val)
{
  set_val(name,val);
  if(type==ICBOX)
    last_ic[index]=val;
}

/* a slider was dragged: redraw and integrate again */
void slider_rerun(void)
{
  int sp=SuppressBounds;
  clr_all_scrns();
  redraw_dfield();
  create_new_cline();
  draw_label(draw_win);
  SuppressBounds=1;
  run_now();
  SuppressBounds=sp;
}

/* ---- the equilibrium window's Import button and its label (logic from
   eig_list.c) ---- */
extern int sparity;
extern double homo_l[100],homo_r[100];

/* make equilibrium y (n values) the initial data; for small systems it is
   also saved alternately as the left/right equilibrium for homoclinics */
void eq_import(double *y,int n)
{
  int i;
  for(i=0;i<n;i++)
    last_ic[i]=y[i];


  if(n<20){
    if(sparity==0){
      for(i=0;i<n;i++)
	homo_l[i]=y[i];
      printf("Saved to left equilibrium\n");
    }
    if(sparity==1){
      for(i=0;i<n;i++)
	homo_r[i]=y[i];
      printf("Saved to right equilibrium\n");
    }
    sparity=1-sparity;
  }
   redraw_ics();
}

/* cp/rp: complex/real eigenvalues with positive real part, im: imaginary */
char *eq_stability(int cp,int rp,int im)
{
 if(cp>0||rp>0)return "UNSTABLE";
 else if(im>0)return "NEUTRAL";
 else return "STABLE";
}
