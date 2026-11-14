#include "plot_settings.h"
#include "struct.h"
#include "xpp_util.h"

namespace xpp {

void get_graph(PlotSettings &settings, const GRAPH &window)
{
  settings.x_3d[0]=window.xmin;
  settings.x_3d[1]=window.xmax;
  settings.y_3d[0]=window.ymin;
  settings.y_3d[1]=window.ymax;
  settings.z_3d[0]=window.zmin;
  settings.z_3d[1]=window.zmax;
  if(!window.ThreeDFlag){ /* the 2D window is the limits' one pair */
    settings.x_3d={window.xlo,window.xhi};
    settings.y_3d={window.ylo,window.yhi};
  }
  settings.curves[0].x=window.xv[0];
  settings.curves[0].y=window.yv[0];
  settings.curves[0].z=window.zv[0];
  settings.plot_3d=window.ThreeDFlag;
  settings.axes=window.grtype;
}

void reset_graph(PlotSettings &settings, GRAPH &window)
{
  settings.plot_3d=settings.axes>=5?1:0;
  window.xv[0]=settings.curves[0].x;
  window.yv[0]=settings.curves[0].y;
  window.zv[0]=settings.curves[0].z;
  window.xmax=settings.x_3d[1];
  window.ymax=settings.y_3d[1];
  window.zmax=settings.z_3d[1];
  window.xbar=.5*(settings.x_3d[1]+settings.x_3d[0]);
  window.ybar=.5*(settings.y_3d[1]+settings.y_3d[0]);
  window.zbar=.5*(settings.z_3d[1]+settings.z_3d[0]);
  window.dx=2./(settings.x_3d[1]-settings.x_3d[0]);
  window.dy=2./(settings.y_3d[1]-settings.y_3d[0]);
  window.dz=2./(settings.z_3d[1]-settings.z_3d[0]);
  window.xmin=settings.x_3d[0];
  window.ymin=settings.y_3d[0];
  window.zmin=settings.z_3d[0];
  if(!settings.plot_3d){ /* a 2D window is its limits; a 3D one's zoom fits its box (default_window) */
    window.xlo=settings.x_3d[0];
    window.ylo=settings.y_3d[0];
    window.xhi=settings.x_3d[1];
    window.yhi=settings.y_3d[1];
  }
  window.grtype=settings.axes;
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
