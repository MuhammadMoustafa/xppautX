#include <climits>
#include <cmath>
#include "model.h"
#include "model_options.h"
#include <tuple>
#include "session.h"
#include "graf_par.h"
#include "colormap.h"
#include "storage.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include <optional>
#include <span>
#include <array>
#include <string>
#include <vector>
#include "arrayplot.h"
#include "marks_data.h"

#include "integrate.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "many_pops.h"
#include "menus.h"


#include "graphics.h"
#include "data_formats.h"
#include <algorithm>
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "browse.h"
#include "image_format.h"
#include "load_eqn.h"
#include <libgen.h>

namespace xpp {

/*Default is now color*/

/* the colours of an imported diagram's curves by their kind (XPPAUT's own line types for them: 0, 1, 8 and 9) */
constexpr xpp::Color bd_stable_eq=xpp::xppaut_color(0),bd_unstable_eq=xpp::xppaut_color(1),
  bd_stable_periodic=xpp::xppaut_color(8),bd_unstable_periodic=xpp::xppaut_color(9);

namespace {

void draw_bd(xpp::Session &s, XppWinId w);
void frz_bd(xpp::Session &s);

} // namespace

constexpr int CurrentCurve=0;

void change_view_com(xpp::Session &s, int com)
{
 
 if(com==2){
   make_my_aplot(s,"Array!");
   editaplot(s,&s.array_plot.plot);
   return;
 }
 if(com==3){
   new_vcr(s);
   return;
 }

  axes_dialog(s,CurrentCurve,com==1);
}

bool phase_plane(const GRAPH &g)
{
  return !(g.ThreeDFlag || g.TimeFlag || g.xv[0]==g.yv[0]);
}

bool phase_plane_states(const Session &s, const GRAPH &g)
{
  return phase_plane(g) && g.xv[0]>0 && g.yv[0]>0 && g.xv[0]<=s.model().node && g.yv[0]<=s.model().node;
}

void check_flags(GRAPH &g)
{
  if(g.grtype>4)g.ThreeDFlag=1;
  else g.ThreeDFlag=0;
  if((g.xv[0]==0)||(g.yv[0]==0)||((g.zv[0]==0)&&(g.ThreeDFlag==1)))g.TimeFlag=1;
  else g.TimeFlag=0;
}

namespace {

constexpr int VIEW_PARAM_COUNT = 10;
const char *const view_param_names[VIEW_PARAM_COUNT] = {"Persp (1=On)", "ZPlane", "ZView", "Theta", "Phi", "Movie(Y/N)",
                                                        "Vary (theta/phi)", "Start angle", "Increment", "Number increments"};
const int view_param_kinds[VIEW_PARAM_COUNT] = {XPP_FIELD_INTEGER, XPP_FIELD_NUMBER, XPP_FIELD_NUMBER, XPP_FIELD_NUMBER,
                                                XPP_FIELD_NUMBER,  XPP_FIELD_TEXT,   XPP_FIELD_TEXT,   XPP_FIELD_NUMBER,
                                                XPP_FIELD_NUMBER,  XPP_FIELD_INTEGER};

std::array<std::string, VIEW_PARAM_COUNT> view_param_values(const xpp::Session &s)
{
  const GRAPH &g = *s.plot_windows.current;
  return {xpp::format("{:d}", g.PerspFlag), xpp::format("{:g}", g.ZPlane), xpp::format("{:g}", g.ZView),
          xpp::format("{:g}", g.Theta),     xpp::format("{:g}", g.Phi),     s.movie_3d.yes,
          s.movie_3d.angle,                 xpp::format("{:g}", s.movie_3d.start),
          xpp::format("{:g}", s.movie_3d.incr), xpp::format("{:d}", s.movie_3d.nclip)};
}

/* the fields of view_param_names, read whole; the movie's only when it is asked for */
ViewParams read_view_params(xpp::TypedFields &f, std::span<const std::string> v)
{
  ViewParams p;
  p.persp = f.integer(view_param_names[0], v[0]);
  p.zplane = f.number(view_param_names[1], v[1]);
  p.zview = f.number(view_param_names[2], v[2]);
  p.theta = f.number(view_param_names[3], v[3]);
  p.phi = f.number(view_param_names[4], v[4]);
  p.movie = !v[5].empty() && (v[5][0] == 'y' || v[5][0] == 'Y');
  if (!p.movie) return p;
  p.yes = v[5];
  p.vary = v[6];
  p.start = f.number(view_param_names[7], v[7]);
  p.incr = f.number(view_param_names[8], v[8]);
  p.nclip = f.integer(view_param_names[9], v[9]);
  return p;
}

} // namespace

ViewParams view_params(const xpp::Session &s)
{
  const GRAPH &g = *s.plot_windows.current;
  return {g.PerspFlag, g.ZPlane, g.ZView, g.Theta, g.Phi, false, s.movie_3d.yes, s.movie_3d.angle, s.movie_3d.start, s.movie_3d.incr,
          s.movie_3d.nclip};
}

/* the one place the 3D view's parameters are applied: set, the movie if asked, the rotation made, redrawn */
Result<> set_view_params(xpp::Session &s, const ViewParams &p)
{
  GRAPH &g = *s.plot_windows.current;
  if (p.movie && !xpp::equal_ignoring_case(p.vary, "theta") && !xpp::equal_ignoring_case(p.vary, "phi"))
    return fail("3D params", xpp::format("\"{}\" is not a way to vary the angle: theta or phi", p.vary), command_place());
  g.PerspFlag = p.persp;
  g.ZPlane = p.zplane;
  g.ZView = p.zview;
  g.Theta = p.theta;
  g.Phi = p.phi;
  if (p.movie) {
    s.movie_3d.yes = p.yes;
    s.movie_3d.angle = p.vary;
    s.movie_3d.start = p.start;
    s.movie_3d.incr = p.incr;
    s.movie_3d.nclip = p.nclip;
    movie_rot(s, p.start, p.incr, p.nclip, p.vary[0] == 'p' || p.vary[0] == 'P' ? 1 : 0);
  }
  make_rot(s, g.Theta, g.Phi);
  redraw_the_graph(s);
  return {};
}

namespace {

/* The axes dialog: a column per axis, a row for each of the variable, limits and label */
constexpr int AXIS_ROW_COUNT = 4;
const char *const axis_rows[AXIS_ROW_COUNT] = {"Variable", "Min", "Max", "Label"};
const char *const axis_columns[3] = {"X", "Y", "Z"};
/* the 3D view's window on the screen, after the grid */
const char *const view_window_names[4] = {"XLo", "XHi", "YLo", "YHi"};

} // namespace

AxesView axes_view(const xpp::Session &s, int ind)
{
  const GRAPH &g=*s.plot_windows.current;
  return {{g.xv[ind],g.yv[ind],g.zv[ind]},{g.xmin,g.ymin,g.zmin},{g.xmax,g.ymax,g.zmax},{g.xlabel,g.ylabel,g.zlabel},{g.xlo,g.xhi,g.ylo,g.yhi}};
}

Result<> set_axes_view(xpp::Session &s, int ind, bool three, const AxesView &v)
{
  const int n=three?3:2;
  const char *const lo_names[3]={"x_min","y_min","z_min"},*const hi_names[3]={"x_max","y_max","z_max"};
  for(int i=0;i<n;i++)
    if(const auto bad=order_problem(lo_names[i],v.lo[i],hi_names[i],v.hi[i],true))return fail("plot axes",*bad,command_place());
  if(three)
    for(const auto &[lo_name,lo,hi_name,hi]:{std::tuple{"window_x_min",v.window[0],"window_x_max",v.window[1]},std::tuple{"window_y_min",v.window[2],"window_y_max",v.window[3]}})
      if(const auto bad=order_problem(lo_name,lo,hi_name,hi,true))return fail("plot axes",*bad,command_place());

  GRAPH &g=*s.plot_windows.current;
  int *const var[3]={&g.xv[ind],&g.yv[ind],&g.zv[ind]};
  double *const lo[3]={&g.xmin,&g.ymin,&g.zmin};
  double *const hi[3]={&g.xmax,&g.ymax,&g.zmax};
  std::string *const label[3]={&g.xlabel,&g.ylabel,&g.zlabel};
  for(int i=0;i<n;i++){
    *var[i]=v.column[i];
    *lo[i]=v.lo[i];
    *hi[i]=v.hi[i];
    *label[i]=v.label[i];
  }
  if(three){
    double *const view_window[4]={&g.xlo,&g.xhi,&g.ylo,&g.yhi};
    for(int i=0;i<4;i++)*view_window[i]=v.window[i];
  } else {
    g.xlo=g.xmin;
    g.ylo=g.ymin;
    g.xhi=g.xmax;
    g.yhi=g.ymax;
  }
  g.grtype=three?5:0;
  xpp::check_windows(s);
  check_flags(g);
  if(three)make_rot(s,g.Theta,g.Phi);
  redraw_the_graph(s);
  return {};
}

bool axes_dialog(xpp::Session &s, int ind, bool three)
{
  const int n=three?3:2;
  const AxesView now=axes_view(s,ind);

  /* the grid row by row (n fields of each of axis_rows), then the 3D view's own fields */
  std::vector<std::string> names, values;
  std::vector<int> kinds;
  const auto add = [&](std::string name, std::string value, int kind) {
    names.push_back(std::move(name));
    values.push_back(std::move(value));
    kinds.push_back(kind);
  };
  for (int i = 0; i < n; i++) add(xpp::format("*0{}-axis", axis_columns[i]), xpp::ind_to_sym(s, now.column[i]), XPP_FIELD_NAME_IN(0));
  for (int i = 0; i < n; i++) add(xpp::format("{}min", axis_columns[i]), xpp::format("{:g}", now.lo[i]), XPP_FIELD_NUMBER);
  for (int i = 0; i < n; i++) add(xpp::format("{}max", axis_columns[i]), xpp::format("{:g}", now.hi[i]), XPP_FIELD_NUMBER);
  for (int i = 0; i < n; i++) add(xpp::format("{}label", axis_columns[i]), now.label[i], XPP_FIELD_TEXT);
  if (three) {
    for (int i = 0; i < 4; i++) add(view_window_names[i], xpp::format("{:g}", now.window[i]), XPP_FIELD_NUMBER);
    const auto params = view_param_values(s);
    for (int i = 0; i < VIEW_PARAM_COUNT; i++) add(view_param_names[i], params[i], view_param_kinds[i]);
  }
  std::vector<const char *> shown(names.size());
  for (std::size_t i = 0; i < names.size(); i++) shown[i] = names[i].c_str();
  if (!xpp::do_grid_box_of(three ? "3D View" : "2D View", std::span(axis_columns, n), axis_rows, shown.data(), values,
                           kinds.data()))
    return false;

  /* everything read and checked before anything is set */
  xpp::TypedFields f(s);
  AxesView view=now;
  for (int i = 0; i < n; i++) {
    find_variable(s, values[i], &view.column[i]);
    if (view.column[i] < 0) f.fail(xpp::format("{}-axis", axis_columns[i]), xpp::format("no variable named \"{}\"", values[i]));
    view.lo[i] = f.number(names[n + i], values[n + i]);
    view.hi[i] = f.number(names[2 * n + i], values[2 * n + i]);
    view.label[i] = values[3 * n + i];
  }
  ViewParams params;
  if (three) {
    for (int i = 0; i < 4; i++) view.window[i] = f.number(view_window_names[i], values[4 * n + i]);
    params = read_view_params(f, std::span(values).subspan(4 * n + 4));
  }
  if (f.error()) {
    xpp::command_error("Plot axes", *f.error());
    return false;
  }
  if (!xpp::ok_or_show(set_axes_view(s, ind, three, view))) return false;
  return !three || xpp::ok_or_show(set_view_params(s, params));
}

AxesOrigin axes_origin(const xpp::Session &s)
{
  const GRAPH &g=*s.plot_windows.current;
  return {{g.xorg,g.yorg,g.zorg},{g.xorgflag!=0,g.yorgflag!=0,g.zorgflag!=0},s.plot_export.font_size};
}

Result<> set_axes_origin(xpp::Session &s, const AxesOrigin &a)
{
  if(!(a.font_size>=axes_font_size_min&&a.font_size<=axes_font_size_max))
    return fail("axes options",xpp::format("Axes font size {:g} must be from {:g} to {:g}",a.font_size,axes_font_size_min,axes_font_size_max),command_place());
  GRAPH &g=*s.plot_windows.current;
  g.xorg=a.origin[0];
  g.yorg=a.origin[1];
  g.zorg=a.origin[2];
  g.xorgflag=a.shown[0];
  g.yorgflag=a.shown[1];
  g.zorgflag=a.shown[2];
  s.plot_export.font_size=a.font_size;
  redraw_the_graph(s);
  return {};
}

void axes_opts(xpp::Session &s)
{
  const AxesOrigin now=axes_origin(s);
  std::array<xpp::FormField,7> fields={
    xpp::FormField::real_number("X-origin",now.origin[0]),xpp::FormField::real_number("Y-origin",now.origin[1]),
    xpp::FormField::real_number("Z-origin",now.origin[2]),
    xpp::FormField::boolean("X origin shown",now.shown[0]),xpp::FormField::boolean("Y origin shown",now.shown[1]),
    xpp::FormField::boolean("Z origin shown",now.shown[2]),
    xpp::FormField::real_number("Axes font size",now.font_size,axes_font_size_min,axes_font_size_max)};
  if(!xpp::ask_form(s,"Axes options",fields))return;
  xpp::ok_or_show(set_axes_origin(s,{{fields[0].real,fields[1].real,fields[2].real},{fields[3].flag,fields[4].flag,fields[5].flag},fields[6].real}));
}

void pretty(double *x1, double *x2)  /* this was always pretty ugly */
{
}

void corner_cube(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi)
{
 float x,y;
 float x1,x2,y1,y2;
 threedproj(s,-1.,-1.,-1.,&x,&y);
 x1=x;
 x2=x;
 y1=y;
 y2=y;
 threedproj(s,-1.,-1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,-1.,1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,-1.,1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,1.,-1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,1.,-1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,1.,1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(s,1.,1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 *xlo=x1;
 *ylo=y1;
 *xhi=x2;
 *yhi=y2;
}

void default_window(xpp::Session &s)
{
 	if(s.plot_windows.current->ThreeDFlag){
	      s.plot_windows.current->xmax=s.plot_settings.x_3d[1];
    	      s.plot_windows.current->ymax=s.plot_settings.y_3d[1];
    	      s.plot_windows.current->zmax=s.plot_settings.z_3d[1];
              s.plot_windows.current->xmin=s.plot_settings.x_3d[0];
              s.plot_windows.current->ymin=s.plot_settings.y_3d[0];
              s.plot_windows.current->zmin=s.plot_settings.z_3d[0];  
	      
	      pretty(&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax));
	      pretty(&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
	      pretty(&(s.plot_windows.current->zmin),&(s.plot_windows.current->zmax));
	      corner_cube(s,&(s.plot_windows.current->xlo),&(s.plot_windows.current->xhi),&(s.plot_windows.current->ylo),&(s.plot_windows.current->yhi));
	      pretty(&(s.plot_windows.current->xlo),&(s.plot_windows.current->xhi));
	      pretty(&(s.plot_windows.current->ylo),&(s.plot_windows.current->yhi));
	      xpp::check_windows(s); 
	}
	else  
    	{
	      s.plot_windows.current->xmax=s.plot_settings.x_3d[1];
    	      s.plot_windows.current->ymax=s.plot_settings.y_3d[1];
    	      
              s.plot_windows.current->xmin=s.plot_settings.x_3d[0];
              s.plot_windows.current->ymin=s.plot_settings.y_3d[0];
	      
	      pretty(&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax)); 
	      pretty(&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
	      s.plot_windows.current->xlo=s.plot_windows.current->xmin;
	      s.plot_windows.current->ylo=s.plot_windows.current->ymin;
	      s.plot_windows.current->xhi=s.plot_windows.current->xmax;
	      s.plot_windows.current->yhi=s.plot_windows.current->ymax;
	      xpp::check_windows(s);
	}
	
	redraw_the_graph(s);
             
}

namespace {
/* The bounds Fit takes for the active window, 2D and 3D alike: every curve's
   axes over the live data, and over each retained run shown beside it
   (a run's own stored columns). Axis 2 (z) is used only when `three_d`. */
struct FitBounds {
  std::array<double,3> lo{1.e25,1.e25,1.e25},hi{-1.e25,-1.e25,-1.e25};
  void include(int axis,double value){
    lo[axis]=std::min(lo[axis],value);
    hi[axis]=std::max(hi[axis],value);
  }
};

FitBounds fit_bounds(const xpp::Session &s,bool three_d)
{
  FitBounds b;
  const auto &w=*s.plot_windows.current;
  const int axes=three_d?3:2;
  for(int i=0;i<w.nvars;i++){
    const std::array<int,3> cols{w.xv[i],w.yv[i],w.zv[i]};
    for(int axis=0;axis<axes;axis++){
      double lo,hi;
      xpp::get_max(s,cols[axis],&lo,&hi);
      b.include(axis,lo);
      b.include(axis,hi);
    }
  }
  const auto &display=s.plot_display[s.plot_windows.active];
  if(!display.show_runs)return b;
  for(const auto &run:display.runs)
    for(int curve=0;curve<run.curves.nvars;curve++){
      const std::array<int,3> cols{run.curves.xv[curve],run.curves.yv[curve],run.curves.zv[curve]};
      for(int axis=0;axis<axes;axis++){
        const auto found=std::find(run.cols.begin(),run.cols.end(),cols[axis]);
        if(found==run.cols.end())continue;
        for(float value:run.data[static_cast<std::size_t>(found-run.cols.begin())])
          if(std::isfinite(value))b.include(axis,value);
      }
    }
  return b;
}
}

void fit_window(xpp::Session &s)
{
  if(s.data_store.rows<2)return;
  const bool three_d=s.plot_windows.current->ThreeDFlag;
  const FitBounds fit=fit_bounds(s,three_d);
  const double mx=fit.lo[0],my=fit.lo[1],mz=fit.lo[2],Mx=fit.hi[0],My=fit.hi[1],Mz=fit.hi[2];
  if(three_d){
    s.plot_windows.current->xmax=Mx;
    s.plot_windows.current->ymax=My;
    s.plot_windows.current->zmax=Mz;
    s.plot_windows.current->xmin=mx;
    s.plot_windows.current->ymin=my;
    s.plot_windows.current->zmin=mz;

    pretty(&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax));
    pretty(&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
    pretty(&(s.plot_windows.current->zmin),&(s.plot_windows.current->zmax));
    corner_cube(s,&(s.plot_windows.current->xlo),&(s.plot_windows.current->xhi),&(s.plot_windows.current->ylo),&(s.plot_windows.current->yhi));
    pretty(&(s.plot_windows.current->xlo),&(s.plot_windows.current->xhi));
    pretty(&(s.plot_windows.current->ylo),&(s.plot_windows.current->yhi));
    xpp::check_windows(s);
  }
  else  
    {
      s.plot_windows.current->xmax=Mx;
      s.plot_windows.current->ymax=My;
      
      s.plot_windows.current->xmin=mx;
      s.plot_windows.current->ymin=my;

      pretty(&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax)); 
      pretty(&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
      s.plot_windows.current->xlo=s.plot_windows.current->xmin;
      s.plot_windows.current->ylo=s.plot_windows.current->ymin;
      s.plot_windows.current->xhi=s.plot_windows.current->xmax;
      s.plot_windows.current->yhi=s.plot_windows.current->ymax;
      xpp::check_windows(s);
    }
  redraw_the_graph(s);
}

void user_window(xpp::Session &s)
{
 static const char *const n[]={"X Lo","X Hi","Y Lo","Y Hi"};
 std::array<std::string, 4> values;
 int status;
 values[0] = xpp::format("{:g}", s.plot_windows.current->xlo);
 values[2] = xpp::format("{:g}", s.plot_windows.current->ylo);
 values[1] = xpp::format("{:g}", s.plot_windows.current->xhi);
 values[3] = xpp::format("{:g}", s.plot_windows.current->yhi);
 static const int kinds[]={XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
 status=do_string_box_of(2,2,"Window",n,values,kinds);
 if(status!=0){
             
	      xpp::TypedFields f(s);
	      const double xlo=f.number(n[0],values[0]),xhi=f.number(n[1],values[1]);
	      const double ylo=f.number(n[2],values[2]),yhi=f.number(n[3],values[3]);
	      if(f.error()){
	        xpp::command_error("Window",*f.error());
	        return;
	      }
	      for(const auto &[lo_name,lo,hi_name,hi]:{std::tuple{"X Lo",xlo,"X Hi",xhi},std::tuple{"Y Lo",ylo,"Y Hi",yhi}})
	        if(const auto bad=xpp::order_problem(lo_name,lo,hi_name,hi,true)){
	          xpp::command_error("Window",*bad);
	          return;
	        }
	      update_view(s,xlo,xhi,ylo,yhi);
	      return;
             }
 redraw_the_graph(s);
}

Result<> variable_vs_time(xpp::Session &s, std::string_view name)
{
 int i;
 find_variable(s,name,&i);
 if(i<0)return fail("plot vs t",xpp::format("no variable named \"{}\"",name),command_place());
 GRAPH &g=*s.plot_windows.current;
 g.yv[0]=i;
 g.grtype=0;
 g.xv[0]=0;
 if(s.data_store.rows>=2){
   xpp::get_max(s,g.xv[0],&g.xmin,&g.xmax);
   xpp::get_max(s,g.yv[0],&g.ymin,&g.ymax);
 }
 else {
   g.xmin=s.numerics.start_time;
   g.xmax=s.numerics.total_time;
 }
 g.xlo=g.xmin;
 g.ylo=g.ymin;
 g.xhi=g.xmax;
 g.yhi=g.ymax;
 xpp::check_windows(s);
 check_flags(g);
 set_normal_scale(s);
 redraw_the_graph(s);
 return {};
}

void xi_vs_t(xpp::Session &s) /*  a short cut   */
{
 std::string value=xpp::ind_to_sym(s,s.plot_windows.current->yv[0]);
 new_string_of("Plot vs t: ",value,XPP_FIELD_NAME_IN(0));
 xpp::ok_or_show(variable_vs_time(s,value));
}

void movie_rot(xpp::Session &s, double start, double increment, int nclip, int angle)
{
  int i;
  double thetaold=s.plot_windows.current->Theta,phiold=s.plot_windows.current->Phi;
  reset_film(s);
  xpp::Result<> film;
  for(i=0;i<=nclip;i++){
   
    if(angle==0)
      make_rot(s,start+i*increment,phiold);
    else
      make_rot(s,thetaold,start+i*increment);
    redraw_the_graph(s);
    if(film)film=ui.film_clip(s);
  }
  s.plot_windows.current->Theta=thetaold;
  s.plot_windows.current->Phi=phiold;
  if(!film)show_error(film.error());
}

void get_3d_par_com(xpp::Session &s)
{
  if(s.plot_windows.current->grtype<5)return;
  auto values=view_param_values(s);
  if(!do_string_box_of(5,2,"3D Parameters",view_param_names,values,view_param_kinds))return;
  xpp::TypedFields f(s);
  const ViewParams p=read_view_params(f,values);
  if(f.error()){
    xpp::command_error("3D params",*f.error());
    return;
  }
  xpp::ok_or_show(set_view_params(s,p));
}
void update_view(xpp::Session &s, double xlo,double xhi, double ylo, double yhi)
{
              s.plot_windows.current->xlo=xlo;
	      s.plot_windows.current->ylo=ylo;
	      s.plot_windows.current->xhi=xhi;
	      s.plot_windows.current->yhi=yhi;
	      if(s.plot_windows.current->grtype<5){
	      s.plot_windows.current->xmin=s.plot_windows.current->xlo;
	      s.plot_windows.current->xmax=s.plot_windows.current->xhi;
	      s.plot_windows.current->ymin=s.plot_windows.current->ylo;
	      s.plot_windows.current->ymax=s.plot_windows.current->yhi;
	      }
	      xpp::check_windows(s);
            
 redraw_the_graph(s);

}
    
/* the corners of a box dragged in the active window, as plot coordinates in order; a click (a box
   with no width or no height) is the point itself, and has zero extent */
namespace {
struct PlotBox { double x1, y1, x2, y2; };
PlotBox dragged_box(xpp::Session &s, int i1, int j1, int i2, int j2)
{
  float x1,y1,x2,y2;
  scale_to_real(s,i1,j1,&x1,&y1);
  scale_to_real(s,i2,j2,&x2,&y2);
  return {std::min(x1,x2),std::min(y1,y2),std::max(x1,x2),std::max(y1,y2)};
}
bool is_click(const PlotBox &b){return b.x1==b.x2||b.y1==b.y2;}
}

void window_zoom_com(xpp::Session &s, int c)
{
 int i1,i2,j1,j2;
  switch(c){
	    case 0:user_window(s); break;
	    case 1:{
	    	if(rubber_band(s,&i1,&j1,&i2,&j2,RUBBOX)==0)break;
	    	const PlotBox b=dragged_box(s,i1,j1,i2,j2);
	    	/*Shrink by half and center (track) about the point clicked*/
	    	if(is_click(b)){
	    	  const double dx=std::abs(s.plot_windows.current->xhi-s.plot_windows.current->xlo)/4,dy=std::abs(s.plot_windows.current->yhi-s.plot_windows.current->ylo)/4;
	    	  zoom_in(s,b.x1-dx,b.y1-dy,b.x1+dx,b.y1+dy);
	    	}
	    	else zoom_in(s,b.x1,b.y1,b.x2,b.y2);
	    	break;
	    }
       	    case 2:{
       	    	if(rubber_band(s,&i1,&j1,&i2,&j2,RUBBOX)==0)break;
       	    	const PlotBox b=dragged_box(s,i1,j1,i2,j2);
       	    	/*Grow by two and center (track) about the point clicked*/
       	    	if(is_click(b))zoom_out_by(s,b.x1,b.y1,ZOOM_OUT_CLICK_FACTOR);
       	    	else zoom_out(s,b.x1,b.y1,b.x2,b.y2);
       	    	break;
	    }
 	    case 3: fit_window(s);
		      break; 
	    case 4: default_window(s);
		      break;
            case 5: scroll_window(s);
                      break;
            }
 set_normal_scale(s);
}

void zoom_in(xpp::Session &s, double x1, double y1, double x2, double y2)
{
  update_view(s,x1,x2,y1,y2);
}

void zoom_out_by(xpp::Session &s, double cx, double cy, double factor)
{
  const double dx=std::abs(s.plot_windows.current->xhi-s.plot_windows.current->xlo)*factor;
  const double dy=std::abs(s.plot_windows.current->yhi-s.plot_windows.current->ylo)*factor;
  update_view(s,cx-dx/2,cx+dx/2,cy-dy/2,cy+dy/2);
}

void zoom_out(xpp::Session &s, double x1, double y1, double x2, double y2)
{
  /* the current view becomes the box inside the new one */
  const double xlo=s.plot_windows.current->xlo,ylo=s.plot_windows.current->ylo;
  const double dx=s.plot_windows.current->xhi-xlo,dy=s.plot_windows.current->yhi-ylo;
  const double bx=dx*dx/(x2-x1),mux=(x1-xlo)/dx;
  const double by=dy*dy/(y2-y1),muy=(y1-ylo)/dy;
  update_view(s,xlo-bx*mux,xlo-bx*mux+bx,ylo-by*muy,ylo-by*muy+by);
}

void graph_all(xpp::Session &s, int *list, int n, int type)
{
  int i;
  if(type==0){
    s.plot_windows.current->resize_curves(n);
    for(i=0;i<n;i++){
      s.plot_windows.current->xv[i]=0;
      s.plot_windows.current->yv[i]=list[i];
      s.plot_windows.current->style[i]=s.plot_windows.current->style[0];
      s.plot_windows.current->color[i]=xpp::xppaut_color(i%(LAST_PLOT_COLOR+1));
    }
    s.plot_windows.current->grtype=0;
    s.plot_windows.current->ThreeDFlag=0;
  }
  if(type==1){
   s.plot_windows.current->resize_curves(1);
   s.plot_windows.current->xv[0]=list[0];
   s.plot_windows.current->yv[0]=list[1];
    s.plot_windows.current->grtype=0;
    s.plot_windows.current->ThreeDFlag=0;
    if(n==3){
      s.plot_windows.current->zv[0]=list[2];
      s.plot_windows.current->grtype=5;
      s.plot_windows.current->ThreeDFlag=1;
    }
  }
  check_flags(*s.plot_windows.current);
  fit_window(s);

}

namespace {

template <std::size_t N> std::vector<std::string> words(const std::array<std::string_view, N> &names)
{
 return std::vector<std::string>(names.begin(),names.end());
}

/* the form fields of a colour and a line style (the curve dialogs'): their count and what they say */
constexpr std::size_t style_field_count=6;
std::array<xpp::FormField,style_field_count> style_fields(const xpp::Color &color, const xpp::LineStyle &style)
{
 return {xpp::FormField::color_field("Colour",color),
         xpp::FormField::choice("Draw",words(xpp::line_draw_names),static_cast<std::size_t>(style.draw)),
         xpp::FormField::choice("Dash",words(xpp::line_dash_names),static_cast<std::size_t>(style.dash)),
         xpp::FormField::real_number("Width",style.width,0.0),
         xpp::FormField::choice("Marker",words(xpp::line_marker_names),static_cast<std::size_t>(style.marker)),
         xpp::FormField::real_number("Marker size",style.marker_size,0.0)};
}

/* the colour and style the fields (an answered ask_form's) say */
void read_style_fields(std::span<const xpp::FormField> fields, xpp::Color &color, xpp::LineStyle &style)
{
 color=fields[0].color;
 style.draw=static_cast<xpp::LineDraw>(fields[1].whole);
 style.dash=static_cast<xpp::LineDash>(fields[2].whole);
 style.width=fields[3].real;
 style.marker=static_cast<xpp::LineMarker>(fields[4].whole);
 style.marker_size=fields[5].real;
}

} // namespace

Result<> check_line_style(std::string_view command, const xpp::LineStyle &style)
{
 if(!(style.width>0))return fail(std::string(command),"Width: must be above 0",command_place());
 if(!(style.marker_size>=0))return fail(std::string(command),"Marker size: must be 0 or more",command_place());
 return {};
}

CurveSettings curve_settings(const xpp::Session &s, int curve)
{
 const GRAPH &g=*s.plot_windows.current;
 const auto at=static_cast<std::size_t>(curve);
 return {g.xv[at],g.yv[at],g.zv[at],g.color[at],g.style[at]};
}

Result<> set_curve(xpp::Session &s, int curve, const CurveSettings &settings)
{
 GRAPH &g=*s.plot_windows.current;
 if(curve<0||curve>=g.nvars)return fail("plots",xpp::format("no curve {}",curve),command_place());
 if(const Result<> checked=check_line_style("plots",settings.style);!checked)return checked;
 const auto at=static_cast<std::size_t>(curve);
 g.xv[at]=settings.x;
 g.yv[at]=settings.y;
 g.zv[at]=settings.z;
 g.color[at]=settings.color;
 g.style[at]=settings.style;
 check_flags(g);
 redraw_the_graph(s);
 return {};
}

Result<int> add_curve(xpp::Session &s, const CurveSettings &settings)
{
 if(const Result<> checked=check_line_style("plots",settings.style);!checked)return std::unexpected<Error>(checked.error());
 GRAPH &g=*s.plot_windows.current;
 const int count=g.nvars;
 g.resize_curves(count+1);
 if(const Result<> set=set_curve(s,count,settings);!set){
   g.resize_curves(count);
   return std::unexpected<Error>(set.error());
 }
 return count;
}

Result<> delete_curve(xpp::Session &s, int curve)
{
 GRAPH &g=*s.plot_windows.current;
 if(curve<0||curve>=g.nvars)return fail("plots",xpp::format("no curve {}: the window has curves 0 to {}",curve,g.nvars-1),command_place());
 if(curve==0)return fail("plots","The first curve is the window's own and stays",command_place());
 g.erase_curve(curve);
 check_flags(g);
 redraw_the_graph(s);
 return {};
}

void delete_all_curves(xpp::Session &s)
{
 s.plot_windows.current->resize_curves(1);
 redraw_the_graph(s);
}

namespace {

/* the curve the New and Edit curve forms ask for, starting from the settings offered: false when cancelled or refused (shown) */
bool ask_curve(xpp::Session &s, std::string_view title, CurveSettings &curve)
{
 std::vector<xpp::FormField> fields={
   xpp::FormField{.name="*0X-axis",.kind=XPP_FIELD_NAME_IN(0),.value=xpp::ind_to_sym(s,curve.x)},
   xpp::FormField{.name="*0Y-axis",.kind=XPP_FIELD_NAME_IN(0),.value=xpp::ind_to_sym(s,curve.y)},
   xpp::FormField{.name="*0Z-axis",.kind=XPP_FIELD_NAME_IN(0),.value=xpp::ind_to_sym(s,curve.z)}};
 for(xpp::FormField &f:style_fields(curve.color,curve.style))fields.push_back(std::move(f));
 if(!xpp::ask_form(s,title,fields))return false;
 CurveSettings answer=curve;
 for(std::size_t i=0;i<3;i++){
   int *const column[3]={&answer.x,&answer.y,&answer.z};
   find_variable(s,fields[i].value,column[i]);
   if(*column[i]<0){
     command_error(title,xpp::format("no variable named \"{}\"",fields[i].value));
     return false;
   }
 }
 read_style_fields(std::span(fields).subspan(3),answer.color,answer.style);
 curve=answer;
 return true;
}

void edit_curve(xpp::Session &s)
{
 int crv=0;
 ping();
 new_int(xpp::format("Edit 0-{} :",s.plot_windows.current->nvars-1),&crv);
 if(crv<0||crv>=s.plot_windows.current->nvars){
   command_error("plots","no such curve");
   return;
 }
 CurveSettings curve=curve_settings(s,crv);
 if(ask_curve(s,xpp::format("Edit curve {}",crv),curve))xpp::ok_or_show(set_curve(s,crv,curve));
}

void new_curve(xpp::Session &s)
{
 CurveSettings curve=curve_settings(s,0);
 if(ask_curve(s,"New Curve",curve))xpp::ok_or_show(add_curve(s,curve));
}

} // namespace

/* the main plot window's picture export (W53, issue #101; PDF and SVG, W479): one entry point through
   image_format.h's registry: the plot drawn into the picture begun, written and renamed into place */
Result<bool> write_plot_picture(xpp::Session &s, int fmt, std::string_view filename, const XppPlotExport *given)
{
 const xpp::ImageFormat &f=xpp::image_formats[fmt];
 if(const Result<> begun=xpp::begin_picture(s,f,filename,given ? given->color : s.plot_export.color);!begun)return std::unexpected<Error>(begun.error());
 if(!s.plot_file.writer)return false;
 if(given)s.plot_file.picture.font_size=given->font_size;
 if(const Result<> done=vector_restore(s);!done)return std::unexpected<Error>(done.error());
 if(given)s.plot_export=*given;
 redraw_the_graph(s); /* the window draws again where the picture was */
 return true;
}

namespace {

/* the picture's parameters, the file, then the plot written into it */
void export_plot_picture(xpp::Session &s, int fmt)
{
 const xpp::ImageFormat &f=xpp::image_formats[fmt];
 if(!xpp::ask_picture_params(s))return;
 std::string filename=xpp::picture_filename(s,f);
 if(!file_selector(f.plot_title,filename,xpp::format("*.{}",f.extension)))return;
 const Result<bool> written=write_plot_picture(s,fmt,filename);
 if(xpp::ok_or_show(written)&&*written)ping();
}

} // namespace

void set_colormap(xpp::Session &s, Colormap colormap)
{
  s.colormap=colormap;
  xpp::ui.aplot_redraw(s); /* the array plot shows the new colours */
  redraw_dfield(s);
}

void freeze_com(xpp::Session &s, int c)
{

 switch(c){
 case 0: {
   const Result<int> slot=freeze_curve(s,0);
   if(xpp::ok_or_show(slot))edit_frz_crv(s,*slot);
   break;
 }
 case 1: /* delete, or edit, the graph's frozen curve (asked which) */
 case 2: {
   int i=get_frz_index(s,s.plot_windows.draw_win);
   if(i<0)break;
   if(c==1)delete_frz_crv(s,i);
   else edit_frz_crv(s,i);
   break;
 }
 case 3:
   kill_frz(s);
   break;
 case 5:
   frz_bd(s);
   break;
 case 6:
   clear_bifurcation(s);
   break;
 case 7:
   s.frozen_curves.auto_freeze=1-s.frozen_curves.auto_freeze;
   break;
   
 }
}

void set_frozen_legend(xpp::Session &s, bool visible, std::optional<std::pair<double,double>> at)
{
  if(at){
    s.frozen_curves.key_x=at->first;
    s.frozen_curves.key_y=at->second;
  }
  s.frozen_curves.key_flag=visible?1:0;
  redraw_the_graph(s);
}

void draw_freeze_key(xpp::Session &s)
{
  int ix,iy;
  int i,y0;
  int ix2;
  int dy=2*s.drawing.h_char;
  if(!s.frozen_curves.key_flag)return;
  scale_to_screen(s,static_cast<float>(s.frozen_curves.key_x),static_cast<float>(s.frozen_curves.key_y),&ix,&iy);
  ix2=ix+4*s.drawing.h_char;
  y0=iy;
  for(i=0;i<static_cast<int>(s.frozen_curves.curve.size());i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==s.plot_windows.draw_win&&!s.frozen_curves.curve[i].key.empty()){
      xpp::set_pen(s,s.frozen_curves.curve[i].color,s.frozen_curves.curve[i].style);
      line(s,ix,y0,ix2,y0);
      xpp::set_pen(s,xpp::foreground_color,xpp::LineStyle{});
      put_text(s,ix2+s.drawing.h_char,y0,s.frozen_curves.curve[i].key.c_str());
      y0+=dy;
    }
  }
}

void key_frz_com(xpp::Session &s, int c)
{
  int x,y;
  switch(c){
  case 0:
    set_frozen_legend(s,false,std::nullopt);
    break;
  case 1:
    MessageBox("Position with mouse");
    if(GetMouseXY(s,&x,&y)){
      float xp,yp;
      scale_to_real(s,x,y,&xp,&yp);
      set_frozen_legend(s,true,std::pair<double,double>{xp,yp});
    }
    KillMessageBox();
  }
}

void delete_frz_crv(xpp::Session &s, int i)
{
  if(i<0 || i>=static_cast<int>(s.frozen_curves.curve.size()) || s.frozen_curves.curve[i].use==0)return;
  s.frozen_curves.curve[i].use=0;
  s.frozen_curves.curve[i].name.clear();
  s.frozen_curves.curve[i].key.clear();
  for(std::vector<float> &v:s.frozen_curves.points[i])
    std::vector<float>().swap(v);
  s.frozen_curves.curve[i].xv=nullptr;
  s.frozen_curves.curve[i].yv=nullptr;
  s.frozen_curves.curve[i].zv=nullptr;
}

Result<> delete_frozen(xpp::Session &s, XppWinId w, int slot)
{
  if(const Result<> held=frozen_slot_in_window(s,w,slot,"freeze");!held)return held;
  delete_frz_crv(s,slot);
  redraw_the_graph(s);
  return {};
}

void kill_frz(xpp::Session &s)
{
  int i;
  for(i=0;i<static_cast<int>(s.frozen_curves.curve.size());i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==s.plot_windows.draw_win)
      delete_frz_crv(s,i);
  }
  redraw_the_graph(s);
}

void auto_freeze_it(xpp::Session &s)
{
  if(s.frozen_curves.auto_freeze==0)return;
  xpp::ok_or_show(freeze_curve(s,0));
}

namespace {

/* frozen curve slot i holds these points (z empty unless type>0: 3D) in
   window w, under its default name and key */
void fill_frozen_curve(xpp::Session &s, int i, std::vector<float> x, std::vector<float> y, std::vector<float> z, int type, XppWinId w)
{
  if(i==static_cast<int>(s.frozen_curves.curve.size())){
    s.frozen_curves.curve.emplace_back();
    s.frozen_curves.points.emplace_back();
    s.marks_shown.generation.emplace_back();
  }
  CURVE &c=s.frozen_curves.curve[i];
  auto &pts=s.frozen_curves.points[i];
  pts[0]=std::move(x);
  pts[1]=std::move(y);
  if(type>0)pts[2]=std::move(z);
  else std::vector<float>().swap(pts[2]);
  c.xv=pts[0].data();
  c.yv=pts[1].data();
  c.zv=type>0?pts[2].data():nullptr;
  c.use=1;
  c.len=static_cast<int>(pts[0].size());
  c.type=type;
  c.w=w;
  c.color=xpp::foreground_color;
  c.style=xpp::LineStyle{};
  constexpr std::string_view letters="abcdefghijklmnopqrstuvwxyz"; // Preserve XPPAUT's letter names, then number new curves.
  c.name=static_cast<std::size_t>(i)<letters.size()?xpp::format("crv{}",letters[i]):xpp::format("crv{}",i+1);
  c.key=c.name;
  marks_data_frozen_new(s,i); /* the window shows it: it is its current curve */
}

} // namespace

Result<int> freeze_curve(xpp::Session &s, int ind)
{
  if(ind<0 || ind>=s.plot_windows.current->nvars)return fail("freeze","no such curve",command_place());
  if(s.browser.view.maxrow<=2)return fail("freeze","No Curve to freeze",command_place());
  int i=0;
  while(i<static_cast<int>(s.frozen_curves.curve.size())&&s.frozen_curves.curve[i].use)++i;
  const GRAPH &g=*s.plot_windows.current;
  const int n=s.browser.view.maxrow;
  float *const *d=s.browser.view.data;
  fill_frozen_curve(s,i,std::vector<float>(d[g.xv[ind]],d[g.xv[ind]]+n),
                    std::vector<float>(d[g.yv[ind]],d[g.yv[ind]]+n),
                    g.grtype>0?std::vector<float>(d[g.zv[ind]],d[g.zv[ind]]+n):std::vector<float>(),
                    g.grtype,s.plot_windows.draw_win);
  return i;
}

Result<> frozen_slot_in_window(const xpp::Session &s, XppWinId w, int slot, std::string_view command)
{
  const auto &curves=s.frozen_curves.curve;
  if(slot<0||slot>=static_cast<int>(curves.size())||curves[static_cast<std::size_t>(slot)].use!=1||curves[static_cast<std::size_t>(slot)].w!=w)
    return fail(std::string(command),xpp::format("The window has no frozen curve in slot {}",slot),command_place());
  return {};
}

FrozenSettings frozen_settings(const xpp::Session &s, int slot)
{
  const CURVE &c=s.frozen_curves.curve[static_cast<std::size_t>(slot)];
  return {c.color,c.style,c.key,c.name};
}

Result<> check_frozen_settings(const FrozenSettings &f)
{
  if(const Result<> checked=check_line_style("freeze",f.style);!checked)return checked;
  if(f.key.size()>frozen_key_max)return fail("freeze",xpp::format("Key: at most {} bytes, not {}",frozen_key_max,f.key.size()),command_place());
  if(f.name.size()>frozen_name_max)return fail("freeze",xpp::format("Name: at most {} bytes, not {}",frozen_name_max,f.name.size()),command_place());
  return {};
}

Result<> set_frozen_curve(xpp::Session &s, int slot, const FrozenSettings &f)
{
  if(const Result<> checked=check_frozen_settings(f);!checked)return checked;
  CURVE &c=s.frozen_curves.curve[static_cast<std::size_t>(slot)];
  c.color=f.color;
  c.style=f.style;
  c.key=f.key;
  c.name=f.name;
  redraw_the_graph(s);
  return {};
}

bool restore_frozen_curve(xpp::Session &s, int i, XppWinId w, int type, const xpp::Color &color, const xpp::LineStyle &style, std::string key, std::string name,
                          std::vector<float> x, std::vector<float> y, std::vector<float> z)
{
  if(i<0||i>static_cast<int>(s.frozen_curves.curve.size())||
     (i<static_cast<int>(s.frozen_curves.curve.size())&&s.frozen_curves.curve[i].use)||x.empty()||x.size()!=y.size()||
     (type>0&&z.size()!=x.size()))return false;
  fill_frozen_curve(s,i,std::move(x),std::move(y),std::move(z),type,w);
  CURVE &c=s.frozen_curves.curve[i];
  c.color=color;
  c.style=style;
  c.key=std::move(key);
  c.name=std::move(name);
  return true;
}

void edit_frz_crv(xpp::Session &s, int i)
{
 FrozenSettings now=frozen_settings(s,i);
 std::vector<xpp::FormField> fields;
 for(xpp::FormField &f:style_fields(now.color,now.style))fields.push_back(std::move(f));
 fields.push_back(xpp::FormField{.name="Key",.value=now.key});
 fields.push_back(xpp::FormField{.name="Name",.value=now.name});
 if(!xpp::ask_form(s,"Edit Freeze",fields))return;
 FrozenSettings answer;
 read_style_fields(std::span(fields).first(style_field_count),answer.color,answer.style);
 answer.key=fields[style_field_count].value;
 answer.name=fields[style_field_count+1].value;
 xpp::ok_or_show(set_frozen_curve(s,i,answer));
}

void draw_freeze(xpp::Session &s, XppWinId w)
{
  int i,j,type=s.plot_windows.current->grtype;
  float oldxpl,oldypl,oldzpl=0.0,xpl,ypl,zpl=0.0;
  float *xv,*yv,*zv;
  for(i=0;i<static_cast<int>(s.frozen_curves.curve.size());i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==w&&s.frozen_curves.curve[i].type==type){
      if(type==0)marks_data_frozen(s,w,i); /* the curve as data */
      const xpp::LineStyle &style=s.frozen_curves.curve[i].style;
      xpp::set_pen(s,s.frozen_curves.curve[i].color,style);
      xv=s.frozen_curves.curve[i].xv;
      yv=s.frozen_curves.curve[i].yv;
      zv=s.frozen_curves.curve[i].zv;
      oldxpl=xv[0];
      oldypl=yv[0];
      if(type>0)
	oldzpl=zv[0];
      for(j=0;j<s.frozen_curves.curve[i].len;j++){
	xpl=xv[j];
	ypl=yv[j];
	if(type>0)
	  zpl=zv[j];
        if(style.draw!=xpp::LineDraw::points){
	if(type==0)
	  line_abs(s,oldxpl,oldypl,xpl,ypl);
	else
	  line_3d(s,oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
	}
        if(style.draw!=xpp::LineDraw::line){
	  if(type==0)
	  point_abs(s,xpl,ypl);
	else
	  point_3d(s,xpl,ypl,zpl);
	}
	oldxpl=xpl;
	oldypl=ypl;
	if(type>0)
	  oldzpl=zpl;
      }
    }
  }
  draw_freeze_key(s);
  draw_bd(s,w);
} 

/*  Bifurcation curve importing */

namespace {

void draw_bd(xpp::Session &s, XppWinId w)
{
 if(w!=s.frozen_curves.bif_diagram.w)return;
 for(const BifCurve &c:s.frozen_curves.bif_diagram.curves){
   xpp::set_pen(s,c.color,xpp::LineStyle{});
   const int len=static_cast<int>(c.x.size());
   float xpl=c.x[0],ypl=c.y[0];
   for(int j=0;j<len;j++){
     const float oldxpl=xpl,oldypl=ypl;
     xpl=c.x[j];
     ypl=c.y[j];
     line_abs(s,oldxpl,oldypl,xpl,ypl);
   }
 }
}

void add_bd_crv(xpp::Session &s, const float *x, const float *y, int len, int type)
{
  if(static_cast<int>(s.frozen_curves.bif_diagram.curves.size())>=MAXBIFCRV)return;
  BifCurve c;
  c.x.assign(x,x+len);
  c.y.assign(y,y+len);
  c.color=bd_stable_eq;
  if(type==UNSTABLE_PERIODIC)c.color=bd_unstable_periodic;
  if(type==STABLE_PERIODIC)c.color=bd_stable_periodic;
  if(type==UNSTABLE_EQ)c.color=bd_unstable_eq;
  s.frozen_curves.bif_diagram.curves.push_back(std::move(c));
}

/* a diagram.dat (AUTO's Write pts): x ylo yhi type branch 2par per line;
   each run of points of one type and branch is a curve (two, ylo and yhi,
   for periodic orbits) */
int read_bd(xpp::Session &s, const xpp::DataTable &table)
{
  int oldtype=0,type,oldbr=0,br,ncrv=0,len;
  std::vector<float> x(1),ylo(1),yhi(1);
  len=0;
  if (table.rows() == 0) return 0;
  std::size_t row=0;
  auto next = [&](int at, int &point_type, int &branch) {
    if (row == table.rows()) return false;
    x[at]=table.columns[0][row];
    ylo[at]=table.columns[1][row];
    yhi[at]=table.columns[2][row];
    point_type=static_cast<int>(table.columns[3][row]);
    branch=static_cast<int>(table.columns[4][row]);
    ++row;
    return true;
  };
  next(len,oldtype,oldbr);
  len++;
  s.frozen_curves.bif_diagram.curves.clear();
  for(;;){
    if(static_cast<int>(x.size())<=len){
      x.resize(len+1);
      ylo.resize(len+1);
      yhi.resize(len+1);
    }
    if(!next(len,type,br))
      break;
    if(type==oldtype&&br==oldbr)
      len++;
    else {
      add_bd_crv(s,x.data(),ylo.data(),len,oldtype);
      ncrv++;
      if(oldtype==UNSTABLE_PERIODIC||oldtype==STABLE_PERIODIC){
        add_bd_crv(s,x.data(),yhi.data(),len,oldtype);
        ncrv++;
      }
      if(oldbr==br)len--;
      x[0]=x[len];
      ylo[0]=ylo[len];
      yhi[0]=yhi[len];
      len=1;
    }
    oldbr=br;
    oldtype=type;
  }
  /*  save this last one */
  if(len>1){
    add_bd_crv(s,x.data(),ylo.data(),len,oldtype);
    ncrv++;
    if(oldtype==UNSTABLE_PERIODIC||oldtype==STABLE_PERIODIC){
      add_bd_crv(s,x.data(),yhi.data(),len,oldtype);
      ncrv++;
    }
  }
  xpp::log(XPP_LOG_INFO, " got {} bifurcation curves\n",ncrv);
  s.frozen_curves.bif_diagram.w=s.plot_windows.draw_win;
  return ncrv;
}

void frz_bd(xpp::Session &s)
{
  std::string filename="diagram.dat";
  ping();
  if(!file_selector("Import Diagram",filename,"*"))return;
  xpp::ok_or_show(import_bifurcation(s,filename));
}

} // namespace

Result<int> import_bifurcation(xpp::Session &s, std::string_view filename)
{
  xpp::Result<xpp::DataTable> read=xpp::read_data_table(std::string(filename).c_str());
  if (!read) return std::unexpected<Error>(read.error());
  const xpp::DataTable &table=*read;
  // AUTO's Write pts has six columns: x, low, high, type, branch, two-parameter flag.
  constexpr std::size_t diagram_columns=6;
  const auto bad=[&](std::string what,int line){return std::unexpected<Error>(Error{"diagram import",std::move(what),Place{std::string(filename),line}});};
  if (table.columns.size()!=diagram_columns) return bad("a diagram needs six columns",1);
  for (std::size_t row=0; row<table.rows(); ++row) {
    for (std::size_t col=0; col<diagram_columns; ++col) {
      const double value=table.columns[col][row];
      if (!std::isfinite(value) || (col>=3 && (value!=std::trunc(value) || value<INT_MIN || value>INT_MAX)))
        return bad(xpp::format("invalid diagram value {}",value),table.line(row));
    }
  }
  const int count=read_bd(s,table);
  redraw_the_graph(s);
  return count;
}

void clear_bifurcation(xpp::Session &s)
{
  s.frozen_curves.bif_diagram.curves.clear();
  redraw_the_graph(s);
}

int get_frz_index(xpp::Session &s, XppWinId w)
{
  /* window w's frozen curves: their names, and the slot each is in */
  std::vector<std::string> names;
  std::vector<int> slots;
  for(int i=0;i<static_cast<int>(s.frozen_curves.curve.size());i++){
    if(s.frozen_curves.curve[i].use==1&&w==s.frozen_curves.curve[i].w){
      names.push_back(s.frozen_curves.curve[i].name);
      slots.push_back(i);
    }
  }
  if(names.empty())return(-1);
  const int k=pick_from_list("freeze_curves","Curves",names);
  return k<0?-1:slots[static_cast<std::size_t>(k)];
}

/* Graphic stuff > exp(O)rt: Save data of what the plot shows, the format
   asked from the data formats' registry (data_formats.h) */
void export_graf_data(xpp::Session &s)
{
 data_write(s,DataTarget::plot_curves,"","");
}

xpp::DataTable plot_curves_table(const xpp::Session &s)
{
  const GRAPH &g=*s.plot_windows.current;
  const BROWSER &b=s.browser.view;
  const bool three=g.ThreeDFlag>0;
  xpp::DataTable t;
  t.curves=true;
  t.names={"curve","x","y"};
  if(three)t.names.emplace_back("z");
  t.columns.resize(t.names.size());
  const auto add=[&](int id,float x,float y,float z){
    t.columns[0].push_back(static_cast<double>(id));
    t.columns[1].push_back(x);
    t.columns[2].push_back(y);
    if(three)t.columns[3].push_back(z);
  };
  const auto stored=[&](int col){return col>=0&&col<b.maxcol;};
  /* the window's curves: point j is x[j-xshft], y[j-yshft], z[j-zshft],
     from the first j every shift allows (as they are drawn) */
  const int first=std::max({0,g.xshft,g.yshft,g.zshft});
  int id=1;
  for(int c=0;c<g.nvars;c++,id++){
    if(!stored(g.xv[c])||!stored(g.yv[c])||(three&&!stored(g.zv[c])))continue;
    for(int j=first;j<b.maxrow;j++)
      add(id,b.data[g.xv[c]][j-g.xshft],b.data[g.yv[c]][j-g.yshft],three?b.data[g.zv[c]][j-g.zshft]:0.0f);
  }
  /* then its frozen curves, in their slots' order */
  for(const CURVE &f : s.frozen_curves.curve){
    if(f.use!=1||f.w!=g.w||f.type!=g.grtype)continue;
    for(int j=0;j<f.len;j++)
      add(id,f.xv[j],f.yv[j],three&&f.zv?f.zv[j]:0.0f);
    id++;
  }
  return t;
}

void add_a_curve_com(xpp::Session &s, int c)
{

 switch(c){
 case 0: new_curve(s);
   break;
 case 1:xpp::ok_or_show(delete_curve(s,s.plot_windows.current->nvars-1));
   break;
 case 2:delete_all_curves(s);
   break;
 case 3: edit_curve(s);
   break;
 case 4: export_plot_picture(s,xpp::IMAGE_FORMAT_PDF);
   break;
 case 5: export_plot_picture(s,xpp::IMAGE_FORMAT_SVG);
   break;
 case 7: axes_opts(s);
   break;
 case 8: export_graf_data(s);
   break;
 }
 check_flags(*s.plot_windows.current);
 redraw_the_graph(s);
   
}

} // namespace xpp
