#include "xpp_ui.h"
#include "xpp_util.h"
#include  "axes2.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
 /* All new improved axes !!  */


#include "my_ps.h"
#include "my_svg.h"
#include "xpplim.h"
#include "struct.h"
#include "graphics.h"
#include "many_pops.h"
#include "graf_par.h"
#include "xpp_globals.h"
#include "xpp_io.h"
#include "pop_list.h"


#define NOAXES 0
#define CROSS 1
#define TAXIS 3
#define ELAXIS 2
#define BOX 4
#define CROSS3D 5
#define T3D 7
#define EL3D 6
#define CUBE 8

#define SIGNIF (0.01)		/* less than one hundredth of a tic mark */
#define CheckZero(x,tic) (fabs(x) < ((tic) * SIGNIF) ? 0.0 : (x))




int DOING_AXES=0;
int AxisVarLabels = 0;
int DOING_BOX_AXES=0;

namespace {

void Frame_3d();
void draw_ytics(const char *s1, double start, double incr, double end);
void draw_xtics(const char *s2, double start, double incr, double end);

/* "y vs x", or "z vs y vs x" in 3D */
std::string make_title()
{
  const auto *g=plot_windows.current;
  if(g->grtype>=5)
    return xpp::format("{} vs {} vs {}",ind_to_sym(g->zv[0]),ind_to_sym(g->yv[0]),ind_to_sym(g->xv[0]));
  return xpp::format("{} vs {}",ind_to_sym(g->yv[0]),ind_to_sym(g->xv[0]));
}

double dbl_raise(double x, int y)
{
int i;
double val;

	val = 1.0;
	for (i=0; i < abs(y); i++)
		val *= x;
	if (y < 0 ) return (1.0/val);
	return(val);
}


double make_tics(double tmin, double tmax)
{
  double xr,xnorm,tics,tic,l10;
  
  xr = fabs(tmin-tmax);
  
  l10 = log10(xr);
  xnorm = pow(10.0,l10-static_cast<double>((l10 >= 0.0 ) ? static_cast<int>(l10) : (static_cast<int>(l10)-1)));
  if (xnorm <= 2)
    tics = 0.2;
  else if (xnorm <= 5)
    tics = 0.5;
  else tics = 1.0;	
  tic = tics * dbl_raise(10.0,(l10 >= 0.0 ) ? static_cast<int>(l10) : (static_cast<int>(l10)-1));
  return(tic);
}

void find_max_min_tic(double *tmin, double *tmax, double tic)
{
  double t1=*tmin;
  t1=tic*floor(*tmin/tic);
  if(t1<*tmin)t1+=tic;
  *tmin=t1;
  t1=tic*ceil(*tmax/tic);
  if(t1>*tmax)t1-=tic;
  *tmax=t1;
}
 
} // namespace

void re_title()
{
  title_text(make_title().c_str());
}

void redraw_cube_pt(double theta,double phi)
{
  set_linestyle(0);
  make_rot(theta,phi);
  clr_scrn();
  canvas_xy(xpp::format("theta={:g} phi={:g}",theta,phi).c_str());
}

void do_axes()
{
    const std::string s1(ind_to_sym(plot_windows.current->xv[0]));
    const std::string s2(ind_to_sym(plot_windows.current->yv[0]));
    set_linestyle(0);
    if(program.interactive){  re_title();
    SmallGr();
    }

    switch(plot_windows.current->grtype)
    {
    case 0: Box_axis(plot_windows.current->xlo,plot_windows.current->xhi,plot_windows.current->ylo,plot_windows.current->yhi,
		       (plot_windows.current->xlabel[0]||!AxisVarLabels)?plot_windows.current->xlabel:s1.c_str(),
		       (plot_windows.current->ylabel[0]||!AxisVarLabels)?plot_windows.current->ylabel:s2.c_str(),1); break;
    case 5: Frame_3d(); break;

   }
    if(program.interactive)SmallBase();
 
 }

namespace {

void Frame_3d()
{

	
  double tx,ty,tz;
  float x1,y1,z1,x2,y2,z2,dt=.03;
  float x0=plot_windows.current->xorg,y0=plot_windows.current->yorg,z0=plot_windows.current->zorg;
  double xmin=plot_windows.current->xmin,xmax=plot_windows.current->xmax,ymin=plot_windows.current->ymin;
  double ymax=plot_windows.current->ymax,zmin=plot_windows.current->zmin,zmax=plot_windows.current->zmax;
  float x4=xmin,y4=ymin,z4=zmin,x5=xmax,y5=ymax,z5=zmax;
  float x3,y3,z3,x6,y6,z6;
  
  DOING_AXES=1;
  
  tx=make_tics(xmin,xmax);
  ty=make_tics(ymin,ymax);
  tz=make_tics(zmin,zmax);
  find_max_min_tic(&xmin,&xmax,tx);
  find_max_min_tic(&zmin,&zmax,tz);
  find_max_min_tic(&ymin,&ymax,ty);
  scale3d(static_cast<float>(xmin),static_cast<float>(ymin),static_cast<float>(zmin),&x1,&y1,&z1);
  scale3d(static_cast<float>(xmax),static_cast<float>(ymax),static_cast<float>(zmax),&x2,&y2,&z2);
 
  scale3d(x4,y4,z4,&x3,&y3,&z3);
  scale3d(x5,y5,z5,&x6,&y6,&z6);
  set_linestyle(-2);
  line3d(-1.,-1.,-1.,1.,-1.,-1.);
  line3d(1.,-1.,-1.,1.,1.,-1.);
  line3d(1.,1.,-1.,-1.,1.,-1.);
  line3d(-1.,1.,-1.,-1.,-1.,-1.);
  line3d(-1.,-1.,1.,1.,-1.,1.);
  line3d(1.,-1.,1.,1.,1.,1.);
  line3d(1.,1.,1.,-1.,1.,1.);
  line3d(-1.,1.,1.,-1.,-1.,1.);
  line3d(1.,1.,1.,1.,1.,-1.);
  line3d(-1.,1.,1.,-1.,1.,-1.);
  line3d(-1.,-1.,1.,-1.,-1.,-1.);
  line3d(1.,-1.,1.,1.,-1.,-1.);
    
  line3dn(-1.-dt,-1.,z2,-1.+dt,-1.,z2);  
  line3dn(-1.-dt,-1.,z1,-1.+dt,-1.,z1);  
  line3dn(x2,-1.-dt,-1.0,x2,-1.0+dt,-1.0);
  line3dn(x1,-1.-dt,-1.0,x1,-1.0+dt,-1.0);
  line3dn(1.0-dt,y1,-1.0,1.0+dt,y1,-1.0);
  line3dn(1.0-dt,y2,-1.0,1.0+dt,y2,-1.0);
  

    
  set_linestyle(-1);
  
  if(plot_windows.current->zorgflag)line_3d(x0,y0,z4,x0,y0,z5);
  if(plot_windows.current->yorgflag)line_3d(x0,y4,z0,x0,y5,z0);
  if(plot_windows.current->xorgflag)line_3d(x4,y0,z0,x5,y0,z0);

  dt=.06;
  TextJustify=2;
  text3d(x1,-1-2.*dt,-1.0,xpp::format("{:g}",xmin).c_str());
  text3d(x2,-1-2.*dt,-1.0,xpp::format("{:g}",xmax).c_str());
  text3d(0.0,-1-dt,-1.0,plot_windows.current->xlabel);
  TextJustify=0;
  text3d(1+dt,y1,-1.0,xpp::format("{:g}",ymin).c_str());
  text3d(1+dt,y2,-1.0,xpp::format("{:g}",ymax).c_str());
  text3d(1+dt,0.0,-1.0,plot_windows.current->ylabel);
  TextJustify=2;
  text3d(-1.-dt,-1-dt,z1,xpp::format("{:g}",zmin).c_str());
  text3d(-1.-dt,-1-dt,z2,xpp::format("{:g}",zmax).c_str());
  text3d(-1.-dt,-1.-dt,0.0,plot_windows.current->zlabel);
  TextJustify=0;
  
  DOING_AXES=0;
   
}




} // namespace

void Box_axis(double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy, int flag)
{
  double ytic,xtic;
  
  int xaxis_y,yaxis_x;
 
  int ybot=DBottom,ytop=DTop;
  int xleft=DLeft,xright=DRight;
  
  DOING_AXES=1;
  
  if(ybot>ytop){
    ytop=ybot;
    ybot=DTop;
  }
 
  ytic=make_tics(y_min,y_max);
  xtic=make_tics(x_min,x_max);
 scale_to_screen(static_cast<float>(plot_windows.current->xorg),static_cast<float>(plot_windows.current->yorg),&yaxis_x,&xaxis_y);
  set_linestyle(-1);
  if(plot_windows.current->xorgflag&&flag)
    if(xaxis_y>=ybot&&xaxis_y<=ytop)
      line(xleft,xaxis_y,xright,xaxis_y);
  if(plot_windows.current->yorgflag&&flag)
    if(yaxis_x>=xleft&&yaxis_x<=xright)
      line(yaxis_x,ybot,yaxis_x,ytop);
 set_linestyle(-2);
  DOING_BOX_AXES=1;
  line(xleft,ybot,xright,ybot);
  line(xright,ybot,xright,ytop);
  DOING_BOX_AXES=0;
  line(xright,ytop,xleft,ytop);
  line(xleft,ytop,xleft,ybot);
  draw_ytics(sy,ytic*floor(y_min/ytic),ytic,ytic*ceil(y_max/ytic));
  draw_xtics(sx,xtic*floor(x_min/xtic),xtic,xtic*ceil(x_max/xtic));
  TextJustify=0;
  set_linestyle(0);
  
  DOING_AXES=0;
}


namespace {

void draw_ytics(const char *s1, double start, double incr, double end)
{
  double ticvalue,place;
  double y_min=YMin,y_max=YMax,
  x_min=XMin;
  int xt,yt,s=1;
  TextJustify=2; /* Right justification  */
  for(ticvalue=start;ticvalue<=end;ticvalue+=incr){
    place=CheckZero(ticvalue,incr);
    if(ticvalue<y_min||ticvalue>y_max)continue;
    scale_to_screen(static_cast<float>(x_min),static_cast<float>(place),&xt,&yt);
    DOING_BOX_AXES=0;
    line(DLeft,yt,DLeft+HTic,yt);
    DOING_BOX_AXES=1;
    line(DRight,yt,DRight-HTic,yt);
    DOING_BOX_AXES=0;
    put_text(DLeft-static_cast<int>(1.25*HChar),yt,xpp::format("{:g}",place).c_str());
  }
   scale_to_screen(static_cast<float>(x_min),static_cast<float>(y_max),&xt,&yt);
   if(DTop<DBottom)s=-1;
   if (PltFmtFlag==SVGFMT)
     svg_y_axis_label(DLeft-HChar,yt+2*s*VChar,s1);
   else
     put_text(DLeft-HChar,yt+2*s*VChar,s1);

}


void draw_xtics(const char *s2, double start, double incr, double end)
{
  double ticvalue,place;
  double y_min=YMin,
  x_min=XMin,x_max=XMax;

  int xt,yt;
  int s=1;
  if(DTop<DBottom)s=-1;
  TextJustify=1; /* Center justification  */
  for(ticvalue=start;ticvalue<=end;ticvalue+=incr){
    place=CheckZero(ticvalue,incr);
    if(ticvalue<x_min||ticvalue>x_max)continue;
    scale_to_screen(static_cast<float>(place),y_min,&xt,&yt);
    DOING_BOX_AXES=0;
    line(xt,DBottom,xt,DBottom+s*VTic); 
    DOING_BOX_AXES=1;
    line(xt,DTop,xt,DTop-s*VTic);
    DOING_BOX_AXES=0;
    put_text(xt,yt-static_cast<int>(1.25*VChar*s),xpp::format("{:g}",place).c_str());
  }
  put_text((DLeft+DRight)/2,yt-static_cast<int>(2.5*VChar*s),s2);    


}

} // namespace
