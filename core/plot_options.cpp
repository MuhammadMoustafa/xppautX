#include "plot_options.h"
#include "struct.h"
#include "xpp_util.h"

namespace xpp {

void get_graph(PlotSettings &options, const GRAPH &window)
{
  options.x_3d[0]=window.xmin;
  options.x_3d[1]=window.xmax;
  options.y_3d[0]=window.ymin;
  options.y_3d[1]=window.ymax;
  options.z_3d[0]=window.zmin;
  options.z_3d[1]=window.zmax;
  if(!window.ThreeDFlag){ /* the 2D window is the limits' one pair */
    options.x_3d={window.xlo,window.xhi};
    options.y_3d={window.ylo,window.yhi};
  }
  options.curves[0].x=window.xv[0];
  options.curves[0].y=window.yv[0];
  options.curves[0].z=window.zv[0];
  options.plot_3d=window.ThreeDFlag;
  options.axes=window.grtype;
}

void reset_graph(PlotSettings &options, GRAPH &window)
{
  options.plot_3d=options.axes>=5?1:0;
  window.xv[0]=options.curves[0].x;
  window.yv[0]=options.curves[0].y;
  window.zv[0]=options.curves[0].z;
  window.xmax=options.x_3d[1];
  window.ymax=options.y_3d[1];
  window.zmax=options.z_3d[1];
  window.xbar=.5*(options.x_3d[1]+options.x_3d[0]);
  window.ybar=.5*(options.y_3d[1]+options.y_3d[0]);
  window.zbar=.5*(options.z_3d[1]+options.z_3d[0]);
  window.dx=2./(options.x_3d[1]-options.x_3d[0]);
  window.dy=2./(options.y_3d[1]-options.y_3d[0]);
  window.dz=2./(options.z_3d[1]-options.z_3d[0]);
  window.xmin=options.x_3d[0];
  window.ymin=options.y_3d[0];
  window.zmin=options.z_3d[0];
  if(!options.plot_3d){ /* a 2D window is its limits; a 3D one's zoom fits its box (default_window) */
    window.xlo=options.x_3d[0];
    window.ylo=options.y_3d[0];
    window.xhi=options.x_3d[1];
    window.yhi=options.y_3d[1];
  }
  window.grtype=options.axes;
  check_windows(window);
}

void check_windows(GRAPH &window)
{
  double zip,zap;
  check_val(&window.xmin,&window.xmax,&window.xbar,&window.dx);
  check_val(&window.ymin,&window.ymax,&window.ybar,&window.dy);
  check_val(&window.zmin,&window.zmax,&window.zbar,&window.dz);
  check_val(&window.xlo,&window.xhi,&zip,&zap);
  check_val(&window.ylo,&window.yhi,&zip,&zap);
}

} // namespace xpp
