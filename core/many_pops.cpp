/* The plot windows' bookkeeping (many_pops.h's XppPlotWindows): creating, activating and destroying graphs, and checking their bounds. */
#include "many_pops.h"
#include "session.h"
#include "xpp_util.h"
#include "xpp_ui.h"
#include "graphics.h"
#include "graf_par.h"
#include "grobs.h"
#include "axes2.h"
#include "picture.h"
#include "image_format.h"
#include "integrate.h"
#include "derived.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "expr.h"
#include "nullcline.h"
#include "delay_handle.h"
#include "numerics.h"
#include "browse.h"
#include "form_ode.h"
#include "volterra2.h"
#include "tabular.h"

namespace xpp {

/* ---- graph bookkeeping ----------------------------------------------- */

void create_a_pop(Session &s)
{
    int i;
    for (i = 1; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use == 0) break;
    if (i == static_cast<int>(s.plot_windows.graph.size())) {
        s.plot_windows.graph.emplace_back();
        s.plot_display.emplace_back();
        s.marks_shown.windows.emplace_back();
        s.phase_shown.windows.emplace_back();
        s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    }
    copy_graph(s, i, s.plot_windows.active);
    s.plot_display[i]={};
    s.marks_shown.windows[i]={};
    s.phase_shown.windows[i]={};
    s.plot_windows.graph[i].x11Wid=extra_plot_width;
    s.plot_windows.graph[i].x11Hgt=extra_plot_height;
    s.plot_windows.graph[i].w = i + 1;
    // Protocol auxiliary windows occupy 101 through 105; plots skip this range.
    if(s.plot_windows.graph[i].w>=WIN_AUTO)s.plot_windows.graph[i].w+=WIN_APLOT-WIN_AUTO+1;
    set_active_windows(s);
    make_active(s, i, 1);
    ui.create_plot_window(s);
    get_draw_area(s);
}

void destroy_a_pop(Session &s)
{
    const int index=s.plot_windows.active;
    if(index==0){
        command_error("windows","Cannot destroy the main plot window");
        return;
    }
    kill_frz(s);
    destroy_labels_and_grobs(s,s.plot_windows.graph[index].w);
    ui.destroy_plot_window(s);
    s.plot_windows.graph[index].Use=0;
    make_active(s,0,1);
    set_active_windows(s);
}

void kill_all_pops(Session &s)
{
    for(int i=1;i<static_cast<int>(s.plot_windows.graph.size());i++){
        if(!s.plot_windows.graph[i].Use)continue;
        make_active(s,i,1);
        destroy_a_pop(s);
    }
    make_active(s,0,1);
}

void make_active(xpp::Session &s, int i, int flag)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    s.plot_windows.draw_win = s.plot_windows.current->w;
    get_draw_area_flag(s, flag);
}

int graph_of(const xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w == w) return i;
    return 0;
}

void clr_scrn(xpp::Session &s)
{
    ui.blank_draw_window(s);
    do_axes(s);
}
void set_active_windows(xpp::Session &s)
{
  int i,np=0;
  s.plot_windows.open.resize(s.plot_windows.graph.size());
   for(i=0;i<static_cast<int>(s.plot_windows.graph.size());i++){
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
   if (!fmt || !fmt->write) {
     xpp::command_error("mkplot",xpp::format("Unsupported batch picture format {}",s.plot_export.format));
     return;
   }
   if(xpp::ok_or_show(xpp::begin_picture(s,*fmt,filename,s.plot_export.color)) && s.plot_file.writer)
     vector_restore(s);
}

void   redo_stuff(xpp::Session &s)
    {
      evaluate_derived(s);
   xpp::re_evaluate_kernels(s);
	  xpp::ok_or_show(redo_all_fun_tables(s));
        evaluate_derived(s);
}
} // namespace xpp
