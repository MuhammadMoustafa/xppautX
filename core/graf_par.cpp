#include <climits>
#include <cmath>
#include "model.h"
#include "session.h"
#include "graf_par.h"
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

#define lsSEQ 0
#define lsUEQ 1
#define lsSPER 8
#define lsUPER 9

namespace {

void draw_bd(xpp::Session &s, XppWinId w);
void frz_bd(xpp::Session &s);

} // namespace

constexpr int CurrentCurve=0;

const int colorline[]={0,20,21,22,23,24,25,26,27,28,29,0};
const char *const color_names[]={"WHITE","RED","REDORANGE","ORANGE","YELLOWORANGE",
                    "YELLOW","YELLOWGREEN","GREEN","BLUEGREEN",
		      "BLUE","PURPLE","BLACK"};

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

  GRAPH &g=*s.plot_windows.current;
 const int oldtype=g.grtype,oldflag=g.ThreeDFlag;
 g.grtype=5*com;
 g.ThreeDFlag=com==1?1:0;
 if(!axes_dialog(s,CurrentCurve,com==1)){
   g.grtype=oldtype;
   g.ThreeDFlag=oldflag;
   return;
 }
 check_flags(s);
 redraw_the_graph(s);
} 

void check_flags(xpp::Session &s)
{
  if(s.plot_windows.current->grtype>4)s.plot_windows.current->ThreeDFlag=1;
  else s.plot_windows.current->ThreeDFlag=0;
  if((s.plot_windows.current->xv[0]==0)||(s.plot_windows.current->yv[0]==0)||
     ((s.plot_windows.current->zv[0]==0)&&(s.plot_windows.current->ThreeDFlag==1)))s.plot_windows.current->TimeFlag=1;
  else s.plot_windows.current->TimeFlag=0;
}

namespace {

/* One reading of what was typed in the axes dialog and 3D params: the
   first fault is kept, with the field it belongs to, and no later field is
   read once there is one. */
class TypedFields {
 public:
  explicit TypedFields(xpp::Session &s) : s_(s) {}
  double number(std::string_view field, const std::string &text)
  {
    if (error_) return 0;
    const xpp::Result<double> r = xpp::typed_number(s_, text, field);
    if (r) return *r;
    fail(field, r.error().what);
    return 0;
  }
  int integer(std::string_view field, const std::string &text)
  {
    int v = 0;
    if (!error_ && !xpp::parse_int(text, v)) fail(field, xpp::format("\"{}\" is not a whole number", text));
    return v;
  }
  void fail(std::string_view field, std::string_view what)
  {
    if (!error_) error_ = xpp::format("{}: {}", field, what);
  }
  const std::optional<std::string> &error() const { return error_; }

 private:
  xpp::Session &s_;
  std::optional<std::string> error_;
};

/* the 3D view's parameters, in 3D params and in the axes dialog of a 3D plot */
struct ViewParams {
  int persp = 0;
  double zplane = 0, zview = 0, theta = 0, phi = 0;
  bool movie = false;
  std::string yes, vary;
  double start = 0, incr = 0;
  int nclip = 0;
};

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
ViewParams read_view_params(TypedFields &f, std::span<const std::string> v)
{
  ViewParams p;
  p.persp = f.integer(view_param_names[0], v[0]);
  p.zplane = f.number(view_param_names[1], v[1]);
  p.zview = f.number(view_param_names[2], v[2]);
  p.theta = f.number(view_param_names[3], v[3]);
  p.phi = f.number(view_param_names[4], v[4]);
  p.movie = !v[5].empty() && (v[5][0] == 'y' || v[5][0] == 'Y');
  if (!p.movie) return p;
  p.yes = v[5].substr(0, 2);
  p.vary = v[6].substr(0, 19);
  const char first = p.vary.empty() ? '\0' : static_cast<char>(std::tolower(static_cast<unsigned char>(p.vary[0])));
  if (first != 't' && first != 'p') f.fail(view_param_names[6], "theta or phi");
  p.start = f.number(view_param_names[7], v[7]);
  p.incr = f.number(view_param_names[8], v[8]);
  p.nclip = f.integer(view_param_names[9], v[9]);
  return p;
}

/* the one place the 3D view's parameters are applied: set, the movie if asked, the rotation made, redrawn */
void apply_view_params(xpp::Session &s, const ViewParams &p)
{
  GRAPH &g = *s.plot_windows.current;
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
}

/* The axes dialog: a column per axis, a row for each of the variable, limits and label */
constexpr int AXIS_ROW_COUNT = 4;
const char *const axis_rows[AXIS_ROW_COUNT] = {"Variable", "Min", "Max", "Label"};
const char *const axis_columns[3] = {"X", "Y", "Z"};
/* the 3D view's window on the screen, after the grid */
const char *const view_window_names[4] = {"XLo", "XHi", "YLo", "YHi"};

} // namespace

bool axes_dialog(xpp::Session &s, int ind, bool three)
{
  GRAPH &g = *s.plot_windows.current;
  const int n = three ? 3 : 2;
  int *const var[3] = {&g.xv[ind], &g.yv[ind], &g.zv[ind]};
  double *const lo[3] = {&g.xmin, &g.ymin, &g.zmin};
  double *const hi[3] = {&g.xmax, &g.ymax, &g.zmax};
  std::string *const label[3] = {&g.xlabel, &g.ylabel, &g.zlabel};
  double *const view_window[4] = {&g.xlo, &g.xhi, &g.ylo, &g.yhi};

  /* the grid row by row (n fields of each of axis_rows), then the 3D view's own fields */
  std::vector<std::string> names, values;
  std::vector<int> kinds;
  const auto add = [&](std::string name, std::string value, int kind) {
    names.push_back(std::move(name));
    values.push_back(std::move(value));
    kinds.push_back(kind);
  };
  for (int i = 0; i < n; i++) add(xpp::format("*0{}-axis", axis_columns[i]), xpp::ind_to_sym(s, *var[i]), XPP_FIELD_NAME_IN(0));
  for (int i = 0; i < n; i++) add(xpp::format("{}min", axis_columns[i]), xpp::format("{:g}", *lo[i]), XPP_FIELD_NUMBER);
  for (int i = 0; i < n; i++) add(xpp::format("{}max", axis_columns[i]), xpp::format("{:g}", *hi[i]), XPP_FIELD_NUMBER);
  for (int i = 0; i < n; i++) add(xpp::format("{}label", axis_columns[i]), *label[i], XPP_FIELD_TEXT);
  if (three) {
    for (int i = 0; i < 4; i++) add(view_window_names[i], xpp::format("{:g}", *view_window[i]), XPP_FIELD_NUMBER);
    const auto params = view_param_values(s);
    for (int i = 0; i < VIEW_PARAM_COUNT; i++) add(view_param_names[i], params[i], view_param_kinds[i]);
  }
  std::vector<const char *> shown(names.size());
  for (std::size_t i = 0; i < names.size(); i++) shown[i] = names[i].c_str();
  if (!xpp::do_grid_box_of(three ? "3D View" : "2D View", std::span(axis_columns, n), axis_rows, shown.data(), values,
                           kinds.data()))
    return false;

  /* everything read and checked before anything is set */
  TypedFields f(s);
  int newvar[3] = {0, 0, 0};
  double newlo[3] = {0, 0, 0}, newhi[3] = {0, 0, 0}, newwindow[4] = {0, 0, 0, 0};
  for (int i = 0; i < n; i++) {
    find_variable(s, values[i], &newvar[i]);
    if (newvar[i] < 0) f.fail(xpp::format("{}-axis", axis_columns[i]), xpp::format("no variable named \"{}\"", values[i]));
    newlo[i] = f.number(names[n + i], values[n + i]);
    newhi[i] = f.number(names[2 * n + i], values[2 * n + i]);
    if (!f.error() && newlo[i] >= newhi[i])
      f.fail(names[2 * n + i], xpp::format("{:g} is not above {}", newhi[i], names[n + i]));
  }
  ViewParams params;
  if (three) {
    for (int i = 0; i < 4; i++) newwindow[i] = f.number(view_window_names[i], values[4 * n + i]);
    for (int i = 0; i < 4; i += 2)
      if (!f.error() && newwindow[i] >= newwindow[i + 1])
        f.fail(view_window_names[i + 1], xpp::format("{:g} is not above {}", newwindow[i + 1], view_window_names[i]));
    params = read_view_params(f, std::span(values).subspan(4 * n + 4));
  }
  if (f.error()) {
    xpp::command_error("Plot axes", *f.error());
    return false;
  }

  for (int i = 0; i < n; i++) {
    *var[i] = newvar[i];
    *lo[i] = newlo[i];
    *hi[i] = newhi[i];
    *label[i] = values[3 * n + i];
  }
  if (three) {
    for (int i = 0; i < 4; i++) *view_window[i] = newwindow[i];
  } else {
    g.xlo = g.xmin;
    g.ylo = g.ymin;
    g.xhi = g.xmax;
    g.yhi = g.ymax;
  }
  xpp::check_windows(s);
  if (three) apply_view_params(s, params);
  return true;
}
void axes_opts(xpp::Session &s)
{
  static const char *const n[]={"X-origin","Y-origin","Z-origin",
		   "X-org(1=on)","Y-org(1=on)","Z-org(1=on",
		    "PSFontSize"};
  std::array<std::string, 7> values;
  int status;
  values[0] = xpp::format("{:g}", s.plot_windows.current->xorg);
  values[1] = xpp::format("{:g}", s.plot_windows.current->yorg);
  values[2] = xpp::format("{:g}", s.plot_windows.current->zorg);
  values[3] = xpp::format("{:d}", s.plot_windows.current->xorgflag);
  values[4] = xpp::format("{:d}", s.plot_windows.current->yorgflag);
  values[5] = xpp::format("{:d}", s.plot_windows.current->zorgflag);
  values[6] = xpp::format("{:d}", s.plot_file.ps_font_size);
  static const int kinds[]={XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
  status=do_string_box_of(7,1,"Axes options",n,values,kinds);
 if(status!=0){
   s.plot_windows.current->xorg=atof(values[0].c_str());
   s.plot_windows.current->yorg=atof(values[1].c_str());
   s.plot_windows.current->zorg=atof(values[2].c_str());
   s.plot_windows.current->xorgflag=atoi(values[3].c_str());
   s.plot_windows.current->yorgflag=atoi(values[4].c_str());
   s.plot_windows.current->zorgflag=atoi(values[5].c_str());
   s.plot_file.ps_font_size=atoi(values[6].c_str());
   redraw_the_graph(s);
 }
   
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
             
	      s.plot_windows.current->xlo=atof(values[0].c_str());
	      s.plot_windows.current->ylo=atof(values[2].c_str());
	      s.plot_windows.current->xhi=atof(values[1].c_str());
	      s.plot_windows.current->yhi=atof(values[3].c_str());
	      if(s.plot_windows.current->grtype<5){
	      s.plot_windows.current->xmin=s.plot_windows.current->xlo;
	      s.plot_windows.current->xmax=s.plot_windows.current->xhi;
	      s.plot_windows.current->ymin=s.plot_windows.current->ylo;
	      s.plot_windows.current->ymax=s.plot_windows.current->yhi;
	      }
	      xpp::check_windows(s);
             }
 redraw_the_graph(s);
}

void xi_vs_t(xpp::Session &s) /*  a short cut   */
{
 int i=s.plot_windows.current->yv[0];

 std::string value=xpp::ind_to_sym(s,i);
 new_string_of("Plot vs t: ",value,XPP_FIELD_NAME_IN(0));
 find_variable(s,value.c_str(),&i);
 
 if(i>-1){
   s.plot_windows.current->yv[0]=i;
   s.plot_windows.current->grtype=0;
   s.plot_windows.current->xv[0]=0;
   if(s.data_store.rows>=2){
      xpp::get_max(s,s.plot_windows.current->xv[0],&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
   pretty(&(s.plot_windows.current->xmin),&(s.plot_windows.current->xmax));
    xpp::get_max(s,s.plot_windows.current->yv[0],&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax));
     pretty(&(s.plot_windows.current->ymin),&(s.plot_windows.current->ymax)); 
   
    }
   else {
     s.plot_windows.current->xmin=s.numerics.start_time;
     s.plot_windows.current->xmax=s.numerics.total_time;
        }
    s.plot_windows.current->xlo=s.plot_windows.current->xmin;
    s.plot_windows.current->ylo=s.plot_windows.current->ymin;
    s.plot_windows.current->xhi=s.plot_windows.current->xmax;
    s.plot_windows.current->yhi=s.plot_windows.current->ymax;
    xpp::check_windows(s);
    check_flags(s);
   set_normal_scale(s);
    redraw_the_graph(s);
 }
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
  TypedFields f(s);
  const ViewParams p=read_view_params(f,values);
  if(f.error()){
    xpp::command_error("3D params",*f.error());
    return;
  }
  apply_view_params(s,p);
}
void update_view(xpp::Session &s, float xlo,float xhi, float ylo, float yhi)
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
    
void window_zoom_com(xpp::Session &s, int c)
{
 int i1,i2,j1,j2;
  switch(c){
	    case 0:user_window(s); break;
	    case 1:
	    
	    	if(rubber_band(s,&i1,&j1,&i2,&j2,RUBBOX)==0)break;
		     zoom_in(s,i1,j1,i2,j2);
		 
		     break;
       	    case 2: if(rubber_band(s,&i1,&j1,&i2,&j2,RUBBOX)==0)break;
		     zoom_out(s,i1,j1,i2,j2);
		     break;
 	    case 3: fit_window(s);
		      break; 
	    case 4: default_window(s);
		      break;
            case 5: scroll_window(s);
                      break;
            }
 set_normal_scale(s);
}

void zoom_in(xpp::Session &s, int i1, int j1, int i2, int j2)
{
 float x1,y1,x2,y2;
 float dx=s.plot_windows.current->xhi-s.plot_windows.current->xlo;
 float dy=s.plot_windows.current->yhi-s.plot_windows.current->ylo;
 scale_to_real(s,i1,j1,&x1,&y1);
 scale_to_real(s,i2,j2,&x2,&y2);
   if(x1==x2||y1==y2)
   {
   
   	if (dx < 0){dx=-dx;}
	if (dy < 0){dy=-dy;}
	dx = dx/2;
	dy = dy/2;
	
	/*Shrink by thirds and center (track) about the point clicked*/
	s.plot_windows.current->xlo=x1-dx/2;
	s.plot_windows.current->xhi=x1+dx/2;

	s.plot_windows.current->ylo=y1-dy/2;
	s.plot_windows.current->yhi=y1+dy/2;

 }
 else
 {           
	      s.plot_windows.current->xlo=x1;
	      s.plot_windows.current->ylo=y1;
	      s.plot_windows.current->xhi=x2;
	      s.plot_windows.current->yhi=y2;
  }
  	if(s.plot_windows.current->grtype<5){
	      s.plot_windows.current->xmin=s.plot_windows.current->xlo;
	      s.plot_windows.current->xmax=s.plot_windows.current->xhi;
	      s.plot_windows.current->ymin=s.plot_windows.current->ylo;
	      s.plot_windows.current->ymax=s.plot_windows.current->yhi;
	      }
	      xpp::check_windows(s);
              redraw_the_graph(s);
	      draw_help(); 
}

void zoom_out(xpp::Session &s, int i1, int j1, int i2, int j2)
{
 
 float x1,y1,x2,y2;
 float bx,mux,by,muy;
 float dx=s.plot_windows.current->xhi-s.plot_windows.current->xlo;
 float dy=s.plot_windows.current->yhi-s.plot_windows.current->ylo;
 scale_to_real(s,i1,j1,&x1,&y1);
 scale_to_real(s,i2,j2,&x2,&y2);

 if(x1==x2||y1==y2)
 {
 
 	if (dx < 0){dx=-dx;}
	if (dy < 0){dy=-dy;}
	/*Grow by thirds and center (track) about the point clicked*/
	dx = dx*2;
	dy = dy*2;
	
	s.plot_windows.current->xlo=x1-dx/2;
	s.plot_windows.current->xhi=x1+dx/2;

	s.plot_windows.current->ylo=y1-dy/2;
	s.plot_windows.current->yhi=y1+dy/2;
 }
 else
 {
  	if(x1>x2){bx=x1;x1=x2;x2=bx;}
 	if(y1>y2){by=y1;y1=y2;y2=by;}
	
	 bx=dx*dx/(x2-x1);
	 mux=(x1-s.plot_windows.current->xlo)/dx;
	 s.plot_windows.current->xlo=s.plot_windows.current->xlo-bx*mux;
	 s.plot_windows.current->xhi=s.plot_windows.current->xlo+bx;

	 by=dy*dy/(y2-y1);
	 muy=(y1-s.plot_windows.current->ylo)/dy;
	 s.plot_windows.current->ylo=s.plot_windows.current->ylo-by*muy;
	 s.plot_windows.current->yhi=s.plot_windows.current->ylo+by;

}
	if(s.plot_windows.current->grtype<5){
		      s.plot_windows.current->xmin=s.plot_windows.current->xlo;
		      s.plot_windows.current->xmax=s.plot_windows.current->xhi;
		      s.plot_windows.current->ymin=s.plot_windows.current->ylo;
		      s.plot_windows.current->ymax=s.plot_windows.current->yhi;
		      }
	      xpp::check_windows(s);
              redraw_the_graph(s);
              draw_help(); 
}

void graph_all(xpp::Session &s, int *list, int n, int type)
{
  int i;
  if(type==0){
    for(i=0;i<n;i++){
      s.plot_windows.current->xv[i]=0;
      s.plot_windows.current->yv[i]=list[i];
      s.plot_windows.current->line[i]=s.plot_windows.current->line[0];
      s.plot_windows.current->color[i]=i;
    }
    s.plot_windows.current->nvars=n;
    s.plot_windows.current->grtype=0;
    s.plot_windows.current->ThreeDFlag=0;
  }
  if(type==1){
   s.plot_windows.current->nvars=1;
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
  check_flags(s);
  fit_window(s);

}

int alter_curve(xpp::Session &s, std::string_view title, int in_it, int n)
{
 static const char *const nn[]={"*0X-axis","*0Y-axis","*0Z-axis","*4Color","Line type"};
 std::array<std::string, 5> values;
 int status,i;
 int i1=s.plot_windows.current->xv[in_it],i2=s.plot_windows.current->yv[in_it],i3=s.plot_windows.current->zv[in_it];
 values[0] = xpp::ind_to_sym(s,i1);
 values[1] = xpp::ind_to_sym(s,i2);
 values[2] = xpp::ind_to_sym(s,i3);
 values[3] = xpp::format("{:d}", s.plot_windows.current->color[in_it]);
 values[4] = xpp::format("{:d}", s.plot_windows.current->line[in_it]);
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),
                           XPP_FIELD_NAME_IN(4),XPP_FIELD_INTEGER};
 status=do_string_box_of(5,1,title,nn,values,kinds);
 if(status!=0){
		    find_variable(s,values[0].c_str(),&i);
 	      if(i>-1)
		s.plot_windows.current->xv[n]=i;
              find_variable(s,values[1].c_str(),&i);
              if(i>-1)
		s.plot_windows.current->yv[n]=i;
              find_variable(s,values[2].c_str(),&i);
  		if(i>-1)
		  s.plot_windows.current->zv[n]=i;

	       s.plot_windows.current->line[n]=atoi(values[4].c_str());
               i=atoi(values[3].c_str());
		    if(i<0||i>10)i=0;
		    s.plot_windows.current->color[n]=i;
		   
		  return(1);
              
              }
 return(0);
}

void edit_curve(xpp::Session &s)
{
 int crv=0;
 ping();
 new_int(xpp::format("Edit 0-{} :",s.plot_windows.current->nvars-1),&crv);
 if(crv>=0&&crv<s.plot_windows.current->nvars)
   alter_curve(s,xpp::format("Edit curve {}",crv),crv,crv);
}

void new_curve(xpp::Session &s)
{
 if(alter_curve(s,"New Curve",0,s.plot_windows.current->nvars))
   s.plot_windows.current->nvars=s.plot_windows.current->nvars+1;
  
 }  

/* the main plot window's picture export (W53, issue #101): one entry
   point through image_format.h's registry in place of the former
   create_ps/create_svg pair -- each format's own extra step (PostScript's
   parameter dialog; SVG asks nothing) is that row's ask_params, the rest
   (file_selector, begin, restore) is shared. Same dialog, default name,
   title and wildcard per format as before. */
void export_plot_picture(xpp::Session &s, int fmt)
{
 const xpp::ImageFormat &f=xpp::image_formats[fmt];
 if(f.ask_params && !f.ask_params(s))return;
 std::string filename=f.plot_filename(s);
 if(!file_selector(f.plot_title,filename,xpp::format("*.{}",f.extension)))return;
 if(xpp::ok_or_show(f.begin(s,filename.c_str(),s.plot_export.color))&&s.plot_file.writer){
   f.restore(s);
   ping();
 }
}

void change_cmap_com(xpp::Session &s, int i)
{
  s.colormap=i;
  NewColormap(i);
}

void freeze_com(xpp::Session &s, int c)
{

 switch(c){
 case 0: 
   freeze_crv(s,0);
   break;
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
   s.frozen_curves.bif_diagram.curves.clear();
   break;
 case 7:
   s.frozen_curves.auto_freeze=1-s.frozen_curves.auto_freeze;
   break;
   
 }
}

void set_key(xpp::Session &s, int x, int y)
{
  float xp,yp;
  scale_to_real(s,x,y,&xp,&yp);
  s.frozen_curves.key_x=xp;
  s.frozen_curves.key_y=yp;
  s.frozen_curves.key_flag=1;
}

void draw_freeze_key(xpp::Session &s)
{
  int ix,iy;
  int i,y0;
  int ix2;
  int dy=2*s.drawing.h_char;
  if(s.frozen_curves.key_flag==SCRNFMT)return;
  if (const xpp::ImageFormat *f=xpp::active_image_format(s)) dy*=f->key_direction;
  scale_to_screen(s,static_cast<float>(s.frozen_curves.key_x),static_cast<float>(s.frozen_curves.key_y),&ix,&iy);
  ix2=ix+4*s.drawing.h_char;
  y0=iy;
  for(i=0;i<MAXFRZ;i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==s.plot_windows.draw_win&&!s.frozen_curves.curve[i].key.empty()){
      set_linestyle(s,abs(s.frozen_curves.curve[i].color));
      line(s,ix,y0,ix2,y0);
      set_linestyle(s,0);
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
    s.frozen_curves.key_flag=0;
    break;
  case 1:
    MessageBox("Position with mouse");
    if(GetMouseXY(s,&x,&y)){
      set_key(s,x,y);
      draw_freeze_key(s);
    }
    KillMessageBox();
  }
}

void delete_frz_crv(xpp::Session &s, int i)
{
  if(s.frozen_curves.curve[i].use==0)return;
  s.frozen_curves.curve[i].use=0;
  s.frozen_curves.curve[i].name.clear();
  s.frozen_curves.curve[i].key.clear();
  for(std::vector<float> &v:s.frozen_curves.points[i])
    std::vector<float>().swap(v);
  s.frozen_curves.curve[i].xv=nullptr;
  s.frozen_curves.curve[i].yv=nullptr;
  s.frozen_curves.curve[i].zv=nullptr;
}

void kill_frz(xpp::Session &s)
{
  int i;
  for(i=0;i<MAXFRZ;i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==s.plot_windows.draw_win)
      delete_frz_crv(s,i);
  }
}

int freeze_crv(xpp::Session &s, int ind)
{
 int i;
 i=create_crv(s,ind);
 if(i<0)return(-1);
 edit_frz_crv(s,i);
 return(1);
}

void auto_freeze_it(xpp::Session &s)
{
  if(s.frozen_curves.auto_freeze==0)return;
  create_crv(s,0);
}

namespace {

/* frozen curve slot i holds these points (z empty unless type>0: 3D) in
   window w, under its default name and key */
void fill_frozen_curve(xpp::Session &s, int i, std::vector<float> x, std::vector<float> y, std::vector<float> z, int type, XppWinId w)
{
  CURVE &c=s.frozen_curves.curve[i];
  std::array<std::vector<float>,3> &pts=s.frozen_curves.points[i];
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
  c.name=xpp::format("crv{}",static_cast<char>('a'+i));
  c.key=c.name;
  marks_data_frozen_new(s,i); /* the window shows it: it is its current curve */
}

} // namespace

int create_crv(xpp::Session &s, int ind)
{
  for(int i=0;i<MAXFRZ;i++){
    if(s.frozen_curves.curve[i].use==0){
      const int ix=s.plot_windows.current->xv[ind];
      const int iy=s.plot_windows.current->yv[ind];
      const int iz=s.plot_windows.current->zv[ind];
      if(s.browser.view.maxrow<=2){
	command_error("freeze", "No Curve to freeze");
	return(-1);
      }
      const int type=s.plot_windows.current->grtype;
      const int n=s.browser.view.maxrow;
      float *const *d=s.browser.view.data;
      fill_frozen_curve(s,i,std::vector<float>(d[ix],d[ix]+n),std::vector<float>(d[iy],d[iy]+n),
                        type>0?std::vector<float>(d[iz],d[iz]+n):std::vector<float>(),type,s.plot_windows.draw_win);
      return(i);
    }
  }
    command_error("freeze", "All curves used");
    return(-1);
}	

bool restore_frozen_curve(xpp::Session &s, int i, XppWinId w, int type, int color, std::string key, std::string name,
                          std::vector<float> x, std::vector<float> y, std::vector<float> z)
{
  if(i<0||i>=MAXFRZ||s.frozen_curves.curve[i].use||x.size()!=y.size()||
     (type>0&&z.size()!=x.size()))return false;
  fill_frozen_curve(s,i,std::move(x),std::move(y),std::move(z),type,w);
  CURVE &c=s.frozen_curves.curve[i];
  c.color=color;
  c.key=std::move(key);
  c.name=std::move(name);
  return true;
}

void edit_frz_crv(xpp::Session &s, int i)
{
 static const char *const nn[]={"*4Color","Key","Name"};
 std::array<std::string, 3> values;
 int status;
 values[0] = xpp::format("{:d}", s.frozen_curves.curve[i].color);
 values[1] = s.frozen_curves.curve[i].key;
 values[2] = s.frozen_curves.curve[i].name;
 static const int kinds[]={XPP_FIELD_NAME_IN(4),XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(3,1,"Edit Freeze",nn,values,kinds);
 if(status!=0){
   s.frozen_curves.curve[i].color=atoi(values[0].c_str());
   s.frozen_curves.curve[i].key=xpp::format("{:.19}",values[1]);
   s.frozen_curves.curve[i].name=xpp::format("{:.9}",values[2]);
 }
}

void draw_freeze(xpp::Session &s, XppWinId w)
{
  int i,j,type=s.plot_windows.current->grtype,lt=0;
  float oldxpl,oldypl,oldzpl=0.0,xpl,ypl,zpl=0.0;
  float *xv,*yv,*zv;
  for(i=0;i<MAXFRZ;i++){
    if(s.frozen_curves.curve[i].use==1&&s.frozen_curves.curve[i].w==w&&s.frozen_curves.curve[i].type==type){
      if(type==0)marks_data_frozen(s,w,i); /* the curve as data */
      if(s.frozen_curves.curve[i].color<0){
	set_linestyle(s,-s.frozen_curves.curve[i].color);
	lt=1;
      }
      else
	set_linestyle(s,s.frozen_curves.curve[i].color);
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
        if(lt==0){
	if(type==0)
	  line_abs(s,oldxpl,oldypl,xpl,ypl);
	else
	  line_3d(s,oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
	}
	else {
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
   set_linestyle(s,c.color);
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
  int i=lsSEQ;
  if(type==UNSTABLE_PERIODIC)i=lsUPER;
  if(type==STABLE_PERIODIC)i=lsSPER;
  if(type==UNSTABLE_EQ)i=lsUEQ;
  c.color=i;
  s.frozen_curves.bif_diagram.curves.push_back(std::move(c));
}

/* a diagram.dat (AUTO's Write pts): x ylo yhi type branch 2par per line;
   each run of points of one type and branch is a curve (two, ylo and yhi,
   for periodic orbits) */
void read_bd(xpp::Session &s, const xpp::DataTable &table)
{
  int oldtype,type,oldbr,br,ncrv=0,len;
  std::vector<float> x(1),ylo(1),yhi(1);
  len=0;
  if (table.rows() == 0) return;
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
}

void frz_bd(xpp::Session &s)
{
  std::string filename="diagram.dat";
  ping();
  if(!file_selector("Import Diagram",filename,"*"))return;
  xpp::Result<xpp::DataTable> read=xpp::read_data_table(filename.c_str());
  if (!xpp::ok_or_show(read)) return;
  const xpp::DataTable &table=*read;
  // AUTO's Write pts has six columns: x, low, high, type, branch, two-parameter flag.
  constexpr std::size_t diagram_columns=6;
  if (table.columns.size()!=diagram_columns) {
    err_reading(filename,"a diagram needs six columns",1);
    return;
  }
  for (std::size_t row=0; row<table.rows(); ++row) {
    for (std::size_t col=0; col<diagram_columns; ++col) {
      const double value=table.columns[col][row];
      if (!std::isfinite(value) || (col>=3 && (value!=std::trunc(value) || value<INT_MIN || value>INT_MAX))) {
        err_reading(filename,xpp::format("invalid diagram value {}",value),table.line(row));
        return;
      }
    }
  }
  read_bd(s,table);
}

} // namespace

int get_frz_index(xpp::Session &s, XppWinId w)
{
  std::vector<std::string> labels;
  std::vector<const char *> items;
  std::string key;
  int i;
  int count=0;
  for(i=0;i<MAXFRZ;i++){
    if(s.frozen_curves.curve[i].use==1&&w==s.frozen_curves.curve[i].w){
      labels.push_back(xpp::format("{}", s.frozen_curves.curve[i].name));
      key.push_back(static_cast<char>('a'+i));
      count++;
    }
  }
  if(count==0)return(-1);
  for(const auto &l : labels) items.push_back(l.c_str());
  XppMenu m={"freeze_curves","Curves",count,items.data(),key.c_str(),no_hint,-1};
  char ch=static_cast<char>(menu_choose(&m,0));
  return(static_cast<int>(ch-'a'));
}

/* Graphic stuff > exp(O)rt: Save data of what the plot shows, the format
   asked from the data formats' registry (data_formats.h) */
void export_graf_data(xpp::Session &s)
{
 data_write(s,&s.browser.view,"plot","","");
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
    t.columns[0].push_back(static_cast<float>(id));
    t.columns[1].push_back(x);
    t.columns[2].push_back(y);
    if(three)t.columns[3].push_back(z);
  };
  const auto stored=[&](int col){return col>=0&&col<b.maxcol;};
  /* the window's curves: point j is x[j-xshft], y[j-yshft], z[j-zshft],
     from the first j every shift allows (as they are drawn) */
  const int first=std::max({0,g.xshft,g.yshft,g.zshft});
  int id=1;
  for(int c=0;c<g.nvars&&c<MAXPERPLOT;c++,id++){
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
 case 0: if(s.plot_windows.current->nvars>=MAXPERPLOT)
   {
     command_error("plots", "Too many plots!");
     return;
   }
   new_curve(s);
   break;
 case 1:if(s.plot_windows.current->nvars>1)s.plot_windows.current->nvars=s.plot_windows.current->nvars-1;
   break;
 case 2:s.plot_windows.current->nvars=1;
   break;
 case 3: edit_curve(s);
   break;
 case 4: export_plot_picture(s,xpp::IMAGE_FORMAT_PS);
   break;
 case 5: export_plot_picture(s,xpp::IMAGE_FORMAT_SVG);
   break;
 case 7: axes_opts(s);
   break;
 case 8: export_graf_data(s);
   break;
 }
 check_flags(s);
 redraw_the_graph(s);
   
}

} // namespace xpp
