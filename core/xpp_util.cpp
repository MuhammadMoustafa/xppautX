/* Pure helpers that used to live in X11 source files (init_conds.c,
   aniparse.c, graf_par.c, calc.c, many_pops.c, main.c). Nothing here
   touches a window. */
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "xpp_files.h"
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

namespace xpp {

#define DELAYBOX 3
#define BCBOX 4
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6

/* ---- graph bookkeeping (was many_pops.c / main.c) ----------------------- */

void make_active(xpp::Session &s, int i, int flag)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    ui.activate_graph(s,i, flag);
}

int graph_of(const xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w == w) return i;
    return 0;
}

void clr_scrn(xpp::Session &s)
{
    ui.blank_draw_window(s);
    do_axes(s);
}

/* ---- moved function bodies follow (appended by tools/move_funcs.py) ---- */

/* new_parameter, set_default_params, clone_ode: from init_conds.c */

std::string ind_to_sym(const xpp::Session &s, int ind)
{
 return browse_column_name(s,ind);
}

void  get_max(const xpp::Session &s, int index, double *vmin, double *vmax)
{
   float x0,x1,z;
   double temp;
   int i;
   x0=s.browser.view.data[index][0];
   x1=x0;
   for(i=0;i<s.browser.view.maxrow;i++)
   {
    z=s.browser.view.data[index][i];
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

void de_space(std::string &s)
{
  de_space(s.data());
  s.resize(strlen(s.c_str()));
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

int find_user_name(const xpp::Model &m, int type, std::string_view oname)
{
 std::string name; /* oname without blanks, of any length */
 int i=-1;
 for(char ch : oname)
   if(!isspace(static_cast<unsigned char>(ch)))name+=ch;

 for(i=0;i<m.nupar;i++)
         if((type==PARAMBOX)&&xpp::equal_ignoring_case(m.upar_names[i],name))break;
 if(i<m.nupar)return(i);
 for(i=0;i<m.neq;i++)
	 if((type==ICBOX)&&xpp::equal_ignoring_case(m.uvar_names[i],name))break;
   if(i<m.neq)return(i);
	return(-1);
 }

int do_calc(xpp::Session &s, std::string_view temp, double *z)
{
 std::string val; /* the name before ':' */
 int ok; 
 int i;
 double newz;
 if(temp.empty()){
	*z=0.0;
	return(1);
	}
 if(has_eq(temp,val,&i))
 {

  newz=calculate(s,temp.substr(i),&ok);  /*  calculate quantity  */
 
  if(ok==0)return(-1);
  i=find_user_name(s.model(),PARAM,val);
  if(i>-1){
    set_val(s,val,newz); /* a parameter set to value  */
    *z=newz;
    redraw_params();
  }
  else {
    i=find_user_name(s.model(),IC,val);
    if(i<0){
      err_msg("No such name!");
      return(-1);
    }
    set_val(s,val,newz);

    s.last_ic[i]=newz;
    *z=newz;
    redraw_ics();
  }
    return(0);
}
	    
  newz=calculate(s,temp,&ok);
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

double calculate(xpp::Session &s, std::string_view expr, int *ok)
{
  int com[400],i;
  double z=0.0;
    if(add_expr(s,expr,com,&i)){
     err_msg("Illegal formula ..");
     *ok=0;
      goto bye;
   }
  z=evaluate(s,com);
 *ok=1;
bye:
  s.parser.ncon=s.model().ncon_start;
  s.parser.nsym=s.model().nsym_start;
  return(z);
 }

void set_active_windows(xpp::Session &s)
{
  int i,np=0;
   for(i=0;i<MAXPOP;i++){
   if(s.plot_windows.graph[i].Use==1){
     s.plot_windows.open[np]=i;
     np++;
   }
 }
 s.plot_windows.count=np;
}  

void check_windows(xpp::Session &s)
{
 double zip,zap;
 check_val(&s.plot_windows.current->xmin,&s.plot_windows.current->xmax,&s.plot_windows.current->xbar,&s.plot_windows.current->dx);
 check_val(&s.plot_windows.current->ymin,&s.plot_windows.current->ymax,&s.plot_windows.current->ybar,&s.plot_windows.current->dy);
 check_val(&s.plot_windows.current->zmin,&s.plot_windows.current->zmax,&s.plot_windows.current->zbar,&s.plot_windows.current->dz);
 check_val(&s.plot_windows.current->xlo,&s.plot_windows.current->xhi,&zip,&zap);
 check_val(&s.plot_windows.current->ylo,&s.plot_windows.current->yhi,&zip,&zap);
} 

/* graf_par.h's */
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

std::string batch_plot_name(const xpp::Session &s, int i)
{
  const std::string &file=s.model().this_file,&set=s.this_internset;
  const std::string &format=s.plot_export.format;
  return i<0?xpp::format("{:.100}{:.100}.{:.10}",file,set,format)
            :xpp::format("{:.100}{:.100}_{:04d}.{:.10}",file,set,i,format);
}

void dump_ps(xpp::Session &s, int i)
{
  const std::string filename=batch_plot_name(s,i);
   const xpp::ImageFormat *fmt=xpp::find_image_format_by_extension(s.plot_export.format);
   if(fmt && xpp::ok_or_show(fmt->begin(s,filename.c_str(),s.plot_export.color)))
     fmt->restore(s);
}

void   redo_stuff(xpp::Session &s)
    {
      evaluate_derived(s);
   xpp::re_evaluate_kernels(s);
	  xpp::ok_or_show(redo_all_fun_tables(s));
        evaluate_derived(s);
}

void user_fun_info(const xpp::Model &m, FILE *fp)
{
  for(int j=0;j<m.nfun;j++){
    std::string line=xpp::format("{}(",m.ufun_names[j]);
    for(int i=0;i<m.narg_fun[j];i++)
      line+=xpp::format("{}{}",m.ufun_args[j][i],i<m.narg_fun[j]-1?",":"");
    line+=xpp::format(") = {}\n",m.ufun_defs[j]);
    fwrite(line.data(),1,line.size(),fp);
  }
}

void ps_restore(xpp::Session &s)
{
  if(program.interactive){
 redraw_dfield(s);
 ps_do_color(s.plot_file,0);
 if(s.plot_windows.current->Nullrestore){restore_nullclines(s);ps_stroke();}
  }

  xpp::restore(s,0,s.browser.view.maxrow);  
 
  do_batch_nclines(s);
  do_batch_dfield(s); 
 do_axes(s); 
  
 ps_do_color(s.plot_file,0); 
 if(program.interactive){
 draw_label(s,s.plot_windows.draw_win);
 ui.draw_freeze(s);
 }
 ps_end(s);
}

void svg_restore(xpp::Session &s)
{

  redraw_dfield(s);
 if(s.plot_windows.current->Nullrestore){restore_nullclines(s);}
 xpp::restore(s,0,s.browser.view.maxrow);
 do_axes(s);
 if(program.interactive){
 draw_label(s,s.plot_windows.draw_win);
 ui.draw_freeze(s);
 }
  do_batch_nclines(s);
  do_batch_dfield(s); 
 svg_end(s);
}

void clone_ode(xpp::Session &s)
{
  int i,j,x,y;
  std::string clone;
  const char *line;
  time_t ttt;
  double z;
  if(!file_selector("Clone ODE file",clone,"*.ode"))return;
  xpp::Writer fp(clone.c_str());
  if(!fp){
      err_msg(" Cant open clone file");
      return;
    }
  ttt=time(0);
  fp.print("# clone of {} on {}",s.model().this_file,ctime(&ttt));
  for(i=0;i<s.model().nlines();i++){
    line=s.model().source[i].c_str();

    if(line[0]=='p'||line[0]=='P'||line[0]=='b'||line[0]=='B'){
      x=find_char(line,"'",0,&j);
      y=find_char(line,"=",0,&j);

      if(x!=0||y!=0){
	fp.print("# original\n# {}\n",line);
	continue;
      }
    }
    if(strncasecmp("done",line,4)==0)continue;
    fp.print("{}\n",line);
  }
  fp.print("# Cloned parameters etc here\n");
  /* now we do parameters boundary conds and ICs */
  j=0;
  fp.print("init ");
  for(i=0;i<(s.model().node+s.model().nmarkov);i++){
    if(j==8){
      fp.print("\ninit ");
      j=0;
    }
    fp.print("{}={:g} ",s.model().uvar_names[i],s.last_ic[i]);
    j++;
  }
  fp.print("\n");

  /* BDRY conds */
  if(s.bcs[0].string.data()[0]!='0'){
    for(i=0;i<s.model().node;i++)
      fp.print("bdry {}\n",s.bcs[i].string.data());
  }
  j=0;
  if(s.model().nupar>0){
    fp.print("par ");
    for(i=0;i<s.model().nupar;i++){
      if(j==8){
	fp.print("\npar ");
        j=0;
      }
      get_val(s,s.model().upar_names[i],&z);
      fp.print("{}={:g} ",s.model().upar_names[i],z);
      j++;
    }
  }
  fp.print("\n");
  fp.print("done \n");
  fp.commit();
}

void new_parameter(xpp::Session &s)
{
  int done,index;
  double z;
  std::string name;
  while(1){
    name.clear();
    done=new_string_of("Parameter:",name,XPP_FIELD_NAME_IN(2));
    if(name.empty()||done==0){redo_stuff(s); return;}
    if(strncasecmp(name.data(),"DEFAULT",7  )==0){
      set_default_params(s);
      continue;
    }

    else {
      index=find_user_name(s.model(),PARAMBOX,name.data());
      if(index>=0){
	get_val(s,s.model().upar_names[index],&z);
	done=new_float(s,xpp::format("{} :",name),&z);
	if(done==0){
	  set_val(s,s.model().upar_names[index],z);
	  ui.param_box_set(index,xpp::format("{:.16g}",z).c_str());
	  ui.param_box_redraw(index);
	}
        if(done==-1){
         redo_stuff(s);
	  return;
	}
      }
    }
  }
}

void   set_default_params(xpp::Session &s)
 {

 for(int i=0;i<s.model().nupar;i++){
   set_val(s,s.model().upar_names[i],s.model().default_val[i]);
   ui.param_box_set(i,xpp::format("{:.16g}",s.model().default_val[i]).c_str());
 }
 
 redraw_params();
 xpp::re_evaluate_kernels(s);
 xpp::ok_or_show(redo_all_fun_tables(s));
 }

/* ---- the values behind the IC, parameter, BC and delay boxes and the
   parameter sliders (logic from init_conds.c; the widgets stay there) ---- */

void   set_default_ics(xpp::Session &s)
{
  int i;
  for(i=0;i<s.model().node+s.model().nmarkov;i++)
    s.last_ic[i]=s.model().default_ic[i];
   redraw_ics();
}

int to_float(xpp::Session &s, std::string_view text, double *z)
{
  int flag;
  *z=0.0;
  if(text.starts_with('%'))
    {
      flag=do_calc(s,text.substr(1),z);
      if(flag==-1)return -1;
      return 0;
    }
  *z=atof(std::string(text).c_str());
  return(0);
}

void man_ic(xpp::Session &s)
{
  int done,index=0;
  double z;
  while(1){
    z=s.last_ic[index];
    done=new_float(s,xpp::format("{} :",s.model().uvar_names[index]),&z);
    if(done==0){
      s.last_ic[index]=z;
      ui.ic_box_set(index,xpp::format("{:.16g}",z).c_str());
      ui.ic_box_redraw(index);
      index++;
      if(index>=s.model().node+s.model().nmarkov)return;
    }
    if(done==-1)return;
  }
}

/* store the text s typed for entry i of a box of the given type. Numbers
   (ICs, parameters) come back in *z and the result is 1; BCs and delays are
   strings (0); -1 when a %formula does not evaluate. */
int box_set_value(xpp::Session &s, int type,int i,std::string_view text,double *z)
{
  *z=0.0;
  switch(type){
  case ICBOX:
    if(to_float(s,text,z)==-1)return -1;
    s.last_ic[i]=*z;
    return 1;
  case PARAMBOX:
    if(to_float(s,text,z)==-1)return -1;
    set_val(s,s.model().upar_names[i],*z);
    return 1;
  case BCBOX:
    set_bc_formula(s,i,text);
    return 0;
  case DELAYBOX:
    s.delay_string[i]=text;
    return 0;
  }
  return 0;
}

/* every entry of a box was just stored: recompute what depends on them */
void box_values_loaded(xpp::Session &s, int type)
{
  if(type==PARAMBOX){
    xpp::re_evaluate_kernels(s);
    xpp::ok_or_show(redo_all_fun_tables(s));
  }
  if(type==DELAYBOX){
   xpp::ok_or_show(xpp::do_init_delay(s,s.numerics.delay));
  }
}

/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot the checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(xpp::Session &s,int how,int *isck,int n)
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
    graph_all(s,plot_list,k,0);
  if(how==1&&k>1)
    graph_all(s,plot_list,k,1);
}

/* a slider names a parameter (PARAMBOX) or a variable (ICBOX); 0 if
   neither */
int find_par_or_var(const xpp::Model &m, std::string_view name,int *type,int *index)
{
  int status=find_user_name(m,PARAMBOX,name);
  if(status==-1){
    status=find_user_name(m,ICBOX,name);
    if(status==-1)return 0;
    *type=ICBOX;
  }
  else *type=PARAMBOX;
  *index=status;
  return 1;
}

void set_par_or_var(xpp::Session &s, std::string_view name,int type,int index,double val)
{
  set_val(s,name,val);
  if(type==ICBOX)
    s.last_ic[index]=val;
}

/* ---- the equilibrium window's Import button and its label (logic from
   eig_list.c) ---- */
/* which of the left/right equilibria eq_import saves next */
static int sparity=0;

/* make equilibrium y (n values) the initial data; for small systems it is
   also saved alternately as the left/right equilibrium for homoclinics */
void eq_import(xpp::Session &s, double *y,int n)
{
  int i;
  for(i=0;i<n;i++)
    s.last_ic[i]=y[i];

  if(n<20){
    if(sparity==0){
      for(i=0;i<n;i++)
	s.auto_state.homo_l[i]=y[i];
      xpp::log_printf(XPP_LOG_INFO, "Saved to left equilibrium\n");
    }
    if(sparity==1){
      for(i=0;i<n;i++)
	s.auto_state.homo_r[i]=y[i];
      xpp::log_printf(XPP_LOG_INFO, "Saved to right equilibrium\n");
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

void do_txt_action(xpp::Session &s, std::string_view action)
{
 get_graph(s);
 extract_action(s,action);
 ping();
  xpp::chk_delay(s);
  redraw_params();
  redraw_ics();
  reset_graph(s);
}

/* ---- AUTO's private scratch directory (session.h: AutoState::dir),
   made by xpp::files::make_temp_dir ---- */
void cleanup_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (!dir.empty()) {
    xpp::files::remove_temp_dir(dir.c_str());
    dir.clear();
  }
}

void renew_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (dir.empty()) return; /* none was made: AUTO writes beside the model */
  xpp::files::remove_temp_dir(dir.c_str());
  dir=xpp::files::make_temp_dir();
}

} // namespace xpp
