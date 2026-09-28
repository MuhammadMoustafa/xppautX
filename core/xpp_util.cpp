/* Pure helpers that used to live in X11 source files (init_conds.c,
   aniparse.c, graf_par.c, calc.c, many_pops.c, main.c). Nothing here
   touches a window. */
#include "model.h"
#include "session.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "axes2.h"
#include "graphics.h"
#include "xpp_globals.h"
#include "expr.h"
#include "browse.h"
#include "graf_par.h"
#include "integrate.h"
#include "nullcline.h"
#include "my_ps.h"
#include "my_svg.h"
#include "image_format.h"
#include "tabular.h"
#include "volterra2.h"
#include "derived.h"
#include "lunch-new.h"
#include "delay_handle.h"
#include "numerics.h"
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
#include "auto_nox.h"

#define DELAYBOX 3
#define BCBOX 4
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6

/* ---- graph bookkeeping (was many_pops.c / main.c) ----------------------- */

void make_active(int i, int flag)
{
    xpp::session().plot_windows.active = i;
    xpp::session().plot_windows.current = &xpp::session().plot_windows.graph[xpp::session().plot_windows.active];
    xpp_ui.activate_graph(i, flag);
}

void clr_scrn(void)
{
    xpp_ui.blank_draw_window();
    do_axes();
}

/* ---- moved function bodies follow (appended by tools/move_funcs.py) ---- */

/* new_parameter, set_default_params, clone_ode: from init_conds.c */

std::string ind_to_sym(int ind)
{
 return browse_column_name(ind);
}

void  get_max(int index, double *vmin, double *vmax)
{
   float x0,x1,z;
   double temp;
   int i;
   x0=xpp::session().browser.view.data[index][0];
   x1=x0;
   for(i=0;i<xpp::session().browser.view.maxrow;i++)
   {
    z=xpp::session().browser.view.data[index][i];
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

int find_user_name(int type, std::string_view oname)
{
 std::string name; /* oname without blanks, of any length */
 int i=-1;
 for(char ch : oname)
   if(!isspace(static_cast<unsigned char>(ch)))name+=ch;

 for(i=0;i<xpp::model().nupar;i++)
         if((type==PARAMBOX)&&xpp::equal_ignoring_case(xpp::model().upar_names[i],name))break;
 if(i<xpp::model().nupar)return(i);
 for(i=0;i<xpp::model().neq;i++)
	 if((type==ICBOX)&&xpp::equal_ignoring_case(xpp::model().uvar_names[i],name))break;
   if(i<xpp::model().neq)return(i);
	return(-1);
 }

int do_calc(const char *temp, double *z)
{
 std::string val; /* the name before ':' */
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

    xpp::session().last_ic[i]=newz;
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

int has_eq(std::string_view z, std::string &name, int *where)
{
  size_t i=z.find(':');
  if(i==std::string_view::npos)return(0);
  name.assign(z.substr(0,i));
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
  z=evaluate(com);
 *ok=1;
bye:
  xpp::session().parser.ncon=xpp::model().ncon_start;
  xpp::session().parser.nsym=xpp::model().nsym_start;
  return(z);
 }

void set_active_windows()
{
  int i,np=0;
   for(i=0;i<MAXPOP;i++){
   if(xpp::session().plot_windows.graph[i].Use==1){
     xpp::session().plot_windows.open[np]=i;
     np++;
   }
 }
 xpp::session().plot_windows.count=np;
}  

void check_windows()
{
 xpp::Session &s=xpp::session();
 double zip,zap;
 check_val(&s.plot_windows.current->xmin,&s.plot_windows.current->xmax,&s.plot_windows.current->xbar,&s.plot_windows.current->dx);
 check_val(&s.plot_windows.current->ymin,&s.plot_windows.current->ymax,&s.plot_windows.current->ybar,&s.plot_windows.current->dy);
 check_val(&s.plot_windows.current->zmin,&s.plot_windows.current->zmax,&s.plot_windows.current->zbar,&s.plot_windows.current->dz);
 check_val(&s.plot_windows.current->xlo,&s.plot_windows.current->xhi,&zip,&zap);
 check_val(&s.plot_windows.current->ylo,&s.plot_windows.current->yhi,&zip,&zap);
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
  const std::string &file=xpp::model().this_file,&set=xpp::session().this_internset;
  const std::string &format=xpp::session().plot_export.format;
  std::string filename=i<0?xpp::format("{:.100}{:.100}.{:.10}",file,set,format)
                          :xpp::format("{:.100}{:.100}_{:04d}.{:.10}",file,set,i,format);

   const xpp::ImageFormat *fmt=xpp::find_image_format_by_extension(xpp::session().plot_export.format);
   if(fmt && fmt->begin(filename.c_str(),xpp::session().plot_export.color))
     fmt->restore();
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
  for(int j=0;j<xpp::model().nfun;j++){
    std::string line=xpp::format("{}(",xpp::model().ufun_names[j]);
    for(int i=0;i<xpp::model().narg_fun[j];i++)
      line+=xpp::format("{}{}",xpp::model().ufun_args[j][i],i<xpp::model().narg_fun[j]-1?",":"");
    line+=xpp::format(") = {}\n",xpp::model().ufun_defs[j]);
    fwrite(line.data(),1,line.size(),fp);
  }
}

void ps_restore()
{
  if(program.interactive){
 redraw_dfield();
 ps_do_color(0);
 if(xpp::session().plot_windows.current->Nullrestore){restore_nullclines();ps_stroke();}
  }

  restore(0,xpp::session().browser.view.maxrow);  
 
  do_batch_nclines();
  do_batch_dfield(); 
 do_axes(); 
  
 ps_do_color(0); 
 if(program.interactive){
 draw_label(xpp::session().plot_windows.draw_win);
 xpp_ui.draw_freeze();
 }
 ps_end();
}

void svg_restore()
{

  redraw_dfield();
 if(xpp::session().plot_windows.current->Nullrestore){restore_nullclines();}
 restore(0,xpp::session().browser.view.maxrow);
 do_axes();
 if(program.interactive){
 draw_label(xpp::session().plot_windows.draw_win);
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
  const char *s;
  time_t ttt;
  double z;
  if(!file_selector("Clone ODE file",clone,"*.ode"))return;
  xpp::Writer fp(clone.c_str());
  if(!fp){
      err_msg(" Cant open clone file");
      return;
    }
  ttt=time(0);
  fp.print("# clone of {} on {}",xpp::model().this_file,ctime(&ttt));
  for(i=0;i<xpp::model().nlines();i++){
    s=xpp::model().source[i].c_str();

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
  for(i=0;i<(xpp::model().node+xpp::model().nmarkov);i++){
    if(j==8){
      fp.print("\ninit ");
      j=0;
    }
    fp.print("{}={:g} ",xpp::model().uvar_names[i],xpp::session().last_ic[i]);
    j++;
  }
  fp.print("\n");

  /* BDRY conds */
  if(xpp::session().bcs[0].string.data()[0]!='0'){
    for(i=0;i<xpp::model().node;i++)
      fp.print("bdry {}\n",xpp::session().bcs[i].string.data());
  }
  j=0;
  if(xpp::model().nupar>0){
    fp.print("par ");
    for(i=0;i<xpp::model().nupar;i++){
      if(j==8){
	fp.print("\npar ");
        j=0;
      }
      get_val(xpp::model().upar_names[i],&z);
      fp.print("{}={:g} ",xpp::model().upar_names[i],z);
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

    else {
      index=find_user_name(PARAMBOX,name.data());
      if(index>=0){
	get_val(xpp::model().upar_names[index],&z);
	done=new_float(xpp::format("{} :",name.data()).c_str(),&z);
	if(done==0){
	  set_val(xpp::model().upar_names[index],z);
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

 for(int i=0;i<xpp::model().nupar;i++){
   set_val(xpp::model().upar_names[i],xpp::model().default_val[i]);
   xpp_ui.param_box_set(i,xpp::format("{:.16g}",xpp::model().default_val[i]).c_str());
 }
 
 redraw_params();
 re_evaluate_kernels();
 redo_all_fun_tables(); 
 }

/* ---- the values behind the IC, parameter, BC and delay boxes and the
   parameter sliders (logic from init_conds.c; the widgets stay there) ---- */

void   set_default_ics()
{
  int i;
  for(i=0;i<xpp::model().node+xpp::model().nmarkov;i++)
    xpp::session().last_ic[i]=xpp::model().default_ic[i];
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
    z=xpp::session().last_ic[index];
    done=new_float(xpp::format("{} :",xpp::model().uvar_names[index]).c_str(),&z);
    if(done==0){
      xpp::session().last_ic[index]=z;
      xpp_ui.ic_box_set(index,xpp::format("{:.16g}",z).c_str());
      xpp_ui.ic_box_redraw(index);
      index++;
      if(index>=xpp::model().node+xpp::model().nmarkov)return;
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
    xpp::session().last_ic[i]=*z;
    return 1;
  case PARAMBOX:
    if(to_float(s,z)==-1)return -1;
    set_val(xpp::model().upar_names[i],*z);
    return 1;
  case BCBOX:
    set_bc_formula(i,s);
    return 0;
  case DELAYBOX:
    xpp::session().delay_string[i]=s;
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
   do_init_delay(xpp::session().numerics.delay);
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
    xpp::session().last_ic[index]=val;
}

/* ---- the equilibrium window's Import button and its label (logic from
   eig_list.c) ---- */
/* which of the left/right equilibria eq_import saves next */
static int sparity=0;

/* make equilibrium y (n values) the initial data; for small systems it is
   also saved alternately as the left/right equilibrium for homoclinics */
void eq_import(double *y,int n)
{
  int i;
  for(i=0;i<n;i++)
    xpp::session().last_ic[i]=y[i];

  if(n<20){
    if(sparity==0){
      for(i=0;i<n;i++)
	xpp::session().auto_state.homo_l[i]=y[i];
      xpp_log(XPP_LOG_INFO, "Saved to left equilibrium\n");
    }
    if(sparity==1){
      for(i=0;i<n;i++)
	xpp::session().auto_state.homo_r[i]=y[i];
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

/* ---- AUTO's private scratch directory (session.h: AutoState::dir),
   made by xpp_files_make_temp_dir ---- */
void xpp_cleanup_auto_dir(void)
{
  std::string &dir=xpp::session().auto_state.dir;
  if (!dir.empty()) {
    xpp_files_remove_temp_dir(dir.c_str());
    dir.clear();
  }
}
