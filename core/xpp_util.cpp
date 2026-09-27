/* Pure helpers that used to live in X11 source files (init_conds.c,
   aniparse.c, graf_par.c, calc.c, many_pops.c, main.c). Nothing here
   touches a window. */
#include "xpp_util.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_ui.h"
#include "grobs.h"
#include "axes2.h"
#include "graphics.h"
#include "edit_rhs.h"
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
#include "numerics.h"
#include "pop_list.h" /* NUPAR, NEQ, upar_names, uvar_names */
#include <array>
#include <string>
#include <string_view>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "load_eqn.h"

#define PARAMBOX 1
#define ICBOX 2
#define DELAYBOX 3
#define BCBOX 4
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6

extern BROWSER my_browser;

/* ---- graph bookkeeping (was many_pops.c / main.c) ----------------------- */

XppPlotWindows plot_windows;

void restore_off(void) { plot_windows.current->Restore = 0; }
void restore_on(void) { plot_windows.current->Restore = 1; }

void make_active(int i, int flag)
{
    plot_windows.active = i;
    plot_windows.current = &plot_windows.graph[plot_windows.active];
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
#define READEM 1
#define WRITEM 0


/* the name of plotted variable ind: T (0) or a variable; stays valid
   while the model is loaded (uvar_names' own storage) */
const char *ind_to_sym(int ind)
{
 return ind==0 ? "T" : uvar_names[ind-1];
}

void  get_max(int index, double *vmin, double *vmax)
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
   *vmin=static_cast<double>(x0);
   *vmax=static_cast<double>(x1);
    if(fabs(*vmin-*vmax)<REAL_SMALL){
      temp=.05*lmax(fabs(*vmin),1.0);
     *vmin=*vmin-temp;
     *vmax=*vmax+temp;
    }
 
 }

std::string short_name(std::string_view name, int width)
{
  const size_t w=width>0?static_cast<size_t>(width):0;
  if(name.size()<=w)
    return std::string(name);
  return w?std::string(name.substr(0,w-1))+'~':std::string();
}

void de_space(char *s)
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

int find_user_name(int type, const char *oname)
{
 std::string name; /* at most XPP_NAME_MAX: no allocation past the string's own room */
 int i=-1;
 for(const char *p=oname;*p;p++){
   if(!isspace(*p)){
     if(name.size()>=XPP_NAME_MAX)return(-1); /* longer than any name */
     try{
       name+=*p;
     }catch(...){
       return(-1); /* out of memory: no name matches */
     }
   }
 }

 for(i=0;i<NUPAR;i++)
         if((type==PARAMBOX)&&(strcasecmp(upar_names[i],name.c_str())==0))break;
 if(i<NUPAR)return(i);
 for(i=0;i<NEQ;i++)
	 if((type==ICBOX)&&(strcasecmp(uvar_names[i],name.c_str())==0))break;
   if(i<NEQ)return(i);
	return(-1);
 }

int do_calc(const char *temp, double *z)
{
 std::array<char, 256> val; /* has_eq writes the name there */
 int ok; 
 int i;
 double newz;
 if(strlen(temp)==0){
	*z=0.0;
	return(1);
	}
 if(has_eq(temp,val.data(),&i))
 {
 
 
  newz=calculate(&temp[i],&ok);  /*  calculate quantity  */
 
  if(ok==0)return(-1);
  i=find_user_name(PARAM,val.data());
  if(i>-1){
    set_val(val.data(),newz); /* a parameter set to value  */
    *z=newz;
    redraw_params();
  }
  else {
    i=find_user_name(IC,val.data());
    if(i<0){
      err_msg("No such name!");
      return(-1);
    }
    set_val(val.data(),newz);

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

int has_eq(const char *z, char *w, int *where)
{
  std::string_view s(z);
  size_t i=s.find(':');
  if(i==std::string_view::npos)return(0);
  if(i>255)return(0); /* w holds 256 bytes; no name is that long */
  s.copy(w,i);
  w[i]=0;
  *where=static_cast<int>(i)+1;
  return(1);
 }

 double calculate(const char *expr, int *ok)
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
   if(plot_windows.graph[i].Use==1){
     plot_windows.open[np]=i;
     np++;
   }
 }
 plot_windows.count=np;
}  

void check_windows()
{
 double zip,zap;
 check_val(&plot_windows.current->xmin,&plot_windows.current->xmax,&plot_windows.current->xbar,&plot_windows.current->dx);
 check_val(&plot_windows.current->ymin,&plot_windows.current->ymax,&plot_windows.current->ybar,&plot_windows.current->dy);
 check_val(&plot_windows.current->zmin,&plot_windows.current->zmax,&plot_windows.current->zbar,&plot_windows.current->dz);
 check_val(&plot_windows.current->xlo,&plot_windows.current->xhi,&zip,&zap);
 check_val(&plot_windows.current->ylo,&plot_windows.current->yhi,&zip,&zap);
} 

void check_val(double *x1, double *x2, double *xb, double *xd)
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
  const char *file=this_file,*set=this_internset,*format=plot_export.format;
  std::string filename=i<0?xpp::format("{:.100}{:.100}.{:.10}",file,set,format)
                          :xpp::format("{:.100}{:.100}_{:04d}.{:.10}",file,set,i,format);

   if (strcmp(plot_export.format,"ps")==0)
   {
     if(ps_init(filename.c_str(),plot_export.color))
     {
       ps_restore();
     }
   }
   else if (strcmp(plot_export.format,"svg")==0)
   {
     if(svg_init(filename.c_str(),plot_export.color))
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

void user_fun_info(FILE *fp)
{
  for(int j=0;j<NFUN;j++){
    std::string line=xpp::format("{}(",static_cast<const char *>(ufun_names[j]));
    for(int i=0;i<narg_fun[j];i++)
      line+=xpp::format("{}{}",static_cast<const char *>(ufun_arg[j].args[i]),i<narg_fun[j]-1?",":"");
    line+=xpp::format(") = {}\n",ufun_def[j]);
    fwrite(line.data(),1,line.size(),fp);
  }
}

void ps_restore()
{
  if(program.interactive){
 redraw_dfield();
 ps_do_color(0);
 if(plot_windows.current->Nullrestore){restore_nullclines();ps_stroke();}
  }
 ps_last_pt_off(); 

  restore(0,my_browser.maxrow);  
 
  do_batch_nclines();
  do_batch_dfield(); 
 do_axes(); 
  
 ps_do_color(0); 
 if(program.interactive){
 draw_label(plot_windows.draw_win);
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
 if(plot_windows.current->Nullrestore){restore_nullclines();}
  svg_last_pt_off();
 /*ps_do_color(0);*/ 
 restore(0,my_browser.maxrow);
 do_axes();
 if(program.interactive){
 draw_label(plot_windows.draw_win);
 xpp_ui.draw_freeze();
 }
  do_batch_nclines();
  do_batch_dfield(); 
 svg_end();
}

void clone_ode()
{
  int i,j,x,y;
  std::string clone;
  char *s;
  time_t ttt;
  double z;
  if(!file_selector("Clone ODE file",clone,"*.ode"))return;
  xpp::Writer fp(clone.c_str());
  if(!fp){
      err_msg(" Cant open clone file");
      return;
    }
  ttt=time(0);
  fp.print("# clone of {} on {}",static_cast<const char *>(this_file),ctime(&ttt));
  for(i=0;i<NLINES;i++){
    s=save_eqn[i];

    if(s[0]=='p'||s[0]=='P'||s[0]=='b'||s[0]=='B'){
      x=find_char(s,"'",0,&j);
      y=find_char(s,"=",0,&j);

      if(x!=0||y!=0){
	fp.print("# original\n# {}\n",s);
	continue;
      }
    }
    if(strncasecmp("done",s,4)==0)continue;
    fp.print("{}\n",s);
  }
  fp.print("# Cloned parameters etc here\n");
  /* now we do parameters boundary conds and ICs */
  j=0;
  fp.print("init ");
  for(i=0;i<(NODE+NMarkov);i++){
    if(j==8){
      fp.print("\ninit ");
      j=0;
    }
    fp.print("{}={:g} ",static_cast<const char *>(uvar_names[i]),last_ic[i]);
    j++;
  }
  fp.print("\n");

  /* BDRY conds */
  if(my_bc[0].string[0]!='0'){
    for(i=0;i<NODE;i++)
      fp.print("bdry {}\n",my_bc[i].string);
  }
  j=0;
  if(NUPAR>0){
    fp.print("par ");
    for(i=0;i<NUPAR;i++){
      if(j==8){
	fp.print("\npar ");
        j=0;
      }
      get_val(upar_names[i],&z);
      fp.print("{}={:g} ",static_cast<const char *>(upar_names[i]),z);
      j++;
    }
  }
  fp.print("\n");
  fp.print("done \n");
  fp.commit();
}

void new_parameter()
{
  int done,index;
  double z;
  std::string name;
  while(1){
    name.clear();
    done=new_string_of("Parameter:",name,XPP_FIELD_NAME_IN(2));
    if(name.empty()||done==0){redo_stuff(); return;}
    if(strncasecmp(name.data(),"DEFAULT",7  )==0){
      set_default_params();
      continue;
    }

    if(strncasecmp(name.data(),"!LOAD", 5 )==0){
      io_parameter_file(name.data(),READEM);
      continue;
    }
    if(strncasecmp(name.data(),"!SAVE", 5 )==0){
      io_parameter_file(name.data(),WRITEM);
      continue;
    }

    else {
      index=find_user_name(PARAMBOX,name.data());
      if(index>=0){
	get_val(upar_names[index],&z);
	done=new_float(xpp::format("{} :",name.data()).c_str(),&z);
	if(done==0){
	  set_val(upar_names[index],z);
	  xpp_ui.param_box_set(index,xpp::format("{:.16g}",z).c_str());
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

 for(int i=0;i<NUPAR;i++){
   set_val(upar_names[i],default_val[i]);
   xpp_ui.param_box_set(i,xpp::format("{:.16g}",default_val[i]).c_str());
 }
 
 redraw_params();
 re_evaluate_kernels();
 redo_all_fun_tables(); 
 }

/* ---- the values behind the IC, parameter, BC and delay boxes and the
   parameter sliders (logic from init_conds.c; the widgets stay there) ---- */
extern int SuppressBounds;

void   set_default_ics()
{
  int i;
  for(i=0;i<NODE+NMarkov;i++)
    last_ic[i]=default_ic[i];
   redraw_ics();
}

int to_float(const char *s, double *z)
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
  while(1){
    z=last_ic[index];
    done=new_float(xpp::format("{} :",static_cast<const char *>(uvar_names[index])).c_str(),&z);
    if(done==0){
      last_ic[index]=z;
      xpp_ui.ic_box_set(index,xpp::format("{:.16g}",z).c_str());
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
int box_set_value(int type,int i,const char *s,double *z)
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
    /* my_bc[i].string is a pointer, allocated 256 bytes (form_ode.cpp,
       both allocation sites). */
    xpp_strlcpy(my_bc[i].string,s,256);
    return 0;
  case DELAYBOX:
    delay_string[i]=s;
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
int find_par_or_var(const char *name,int *type,int *index)
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

void set_par_or_var(const char *name,int type,int index,double val)
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
  draw_label(plot_windows.draw_win);
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
      xpp_log(XPP_LOG_INFO, "Saved to left equilibrium\n");
    }
    if(sparity==1){
      for(i=0;i<n;i++)
	homo_r[i]=y[i];
      xpp_log(XPP_LOG_INFO, "Saved to right equilibrium\n");
    }
    sparity=1-sparity;
  }
   redraw_ics();
}

/* cp/rp: complex/real eigenvalues with positive real part, im: imaginary */
const char *eq_stability(int cp, int rp, int im)
{
 if(cp>0||rp>0)return "UNSTABLE";
 else if(im>0)return "NEUTRAL";
 else return "STABLE";
}

/* ---- a comment with an action in the ODE file was picked (logic from
   txtread.c): run its "name=value ..." settings ---- */
void extract_action(const char *ptr); /* load_eqn.c */

void do_txt_action(const char *s)
{
 get_graph();
 extract_action(s);
 ping();
  chk_delay();
  redraw_params();
  redraw_ics();
  reset_graph();
}

/* ---- AUTO's private scratch directory (xpp_globals.h: program.auto_dir),
   made by xpp_files_make_temp_dir ---- */
void xpp_cleanup_auto_dir(void)
{
  if (!program.auto_dir.empty()) {
    xpp_files_remove_temp_dir(program.auto_dir.c_str());
    program.auto_dir.clear();
  }
}
