#include "xpp_ui.h"
#include "session.h"
#include "xpp_util.h"
#include  "axes2.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
 /* All new improved axes !!  */


#include "my_ps.h"
#include "my_svg.h"
#include "graphics.h"
#include "graf_par.h"
#include "xpp_globals.h"
#include "xpp_io.h"



#define SIGNIF (0.01)		/* less than one hundredth of a tic mark */
#define CheckZero(x,tic) (fabs(x) < ((tic) * SIGNIF) ? 0.0 : (x))





namespace {

void Frame_3d();
void draw_ytics(const char *s1, double start, double incr, double end);
void draw_xtics(const char *s2, double start, double incr, double end);

/* "y vs x", or "z vs y vs x" in 3D */
std::string make_title()
{
  const auto *g=xpp::session().plot_windows.current;
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
    xpp::Session &s=xpp::session();
    const std::string s1(ind_to_sym(s.plot_windows.current->xv[0]));
    const std::string s2(ind_to_sym(s.plot_windows.current->yv[0]));
    set_linestyle(0);
    if(program.interactive){  re_title();
    SmallGr();
    }

    switch(s.plot_windows.current->grtype)
    {
    case 0: Box_axis(s.plot_windows.current->xlo,s.plot_windows.current->xhi,s.plot_windows.current->ylo,s.plot_windows.current->yhi,
		       (!s.plot_windows.current->xlabel.empty()||!s.drawing.axis_var_labels)?s.plot_windows.current->xlabel.c_str():s1.c_str(),
		       (!s.plot_windows.current->ylabel.empty()||!s.drawing.axis_var_labels)?s.plot_windows.current->ylabel.c_str():s2.c_str(),1); break;
    case 5: Frame_3d(); break;

   }
    if(program.interactive)SmallBase();
 
 }

namespace {

void Frame_3d()
{
  xpp::Session &s=xpp::session();

	
  double tx,ty,tz;
  float x1,y1,z1,x2,y2,z2,dt=.03;
  float x0=s.plot_windows.current->xorg,y0=s.plot_windows.current->yorg,z0=s.plot_windows.current->zorg;
  double xmin=s.plot_windows.current->xmin,xmax=s.plot_windows.current->xmax,ymin=s.plot_windows.current->ymin;
  double ymax=s.plot_windows.current->ymax,zmin=s.plot_windows.current->zmin,zmax=s.plot_windows.current->zmax;
  float x4=xmin,y4=ymin,z4=zmin,x5=xmax,y5=ymax,z5=zmax;
  float x3,y3,z3,x6,y6,z6;
  
  s.drawing.doing_axes=1;
  
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
  
  if(s.plot_windows.current->zorgflag)line_3d(x0,y0,z4,x0,y0,z5);
  if(s.plot_windows.current->yorgflag)line_3d(x0,y4,z0,x0,y5,z0);
  if(s.plot_windows.current->xorgflag)line_3d(x4,y0,z0,x5,y0,z0);

  dt=.06;
  s.drawing.text_justify=2;
  text3d(x1,-1-2.*dt,-1.0,xpp::format("{:g}",xmin).c_str());
  text3d(x2,-1-2.*dt,-1.0,xpp::format("{:g}",xmax).c_str());
  text3d(0.0,-1-dt,-1.0,s.plot_windows.current->xlabel.c_str());
  s.drawing.text_justify=0;
  text3d(1+dt,y1,-1.0,xpp::format("{:g}",ymin).c_str());
  text3d(1+dt,y2,-1.0,xpp::format("{:g}",ymax).c_str());
  text3d(1+dt,0.0,-1.0,s.plot_windows.current->ylabel.c_str());
  s.drawing.text_justify=2;
  text3d(-1.-dt,-1-dt,z1,xpp::format("{:g}",zmin).c_str());
  text3d(-1.-dt,-1-dt,z2,xpp::format("{:g}",zmax).c_str());
  text3d(-1.-dt,-1.-dt,0.0,s.plot_windows.current->zlabel.c_str());
  s.drawing.text_justify=0;
  
  s.drawing.doing_axes=0;
   
}




} // namespace

void Box_axis(double x_min, double x_max, double y_min, double y_max, const char *sx, const char *sy, int flag)
{
  xpp::Session &s=xpp::session();
  double ytic,xtic;
  
  int xaxis_y,yaxis_x;
 
  int ybot=s.drawing.d_bottom,ytop=s.drawing.d_top;
  int xleft=s.drawing.d_left,xright=s.drawing.d_right;
  
  s.drawing.doing_axes=1;
  
  if(ybot>ytop){
    ytop=ybot;
    ybot=s.drawing.d_top;
  }
 
  ytic=make_tics(y_min,y_max);
  xtic=make_tics(x_min,x_max);
 scale_to_screen(static_cast<float>(s.plot_windows.current->xorg),static_cast<float>(s.plot_windows.current->yorg),&yaxis_x,&xaxis_y);
  set_linestyle(-1);
  if(s.plot_windows.current->xorgflag&&flag)
    if(xaxis_y>=ybot&&xaxis_y<=ytop)
      line(xleft,xaxis_y,xright,xaxis_y);
  if(s.plot_windows.current->yorgflag&&flag)
    if(yaxis_x>=xleft&&yaxis_x<=xright)
      line(yaxis_x,ybot,yaxis_x,ytop);
 set_linestyle(-2);
  s.drawing.doing_box_axes=1;
  line(xleft,ybot,xright,ybot);
  line(xright,ybot,xright,ytop);
  s.drawing.doing_box_axes=0;
  line(xright,ytop,xleft,ytop);
  line(xleft,ytop,xleft,ybot);
  draw_ytics(sy,ytic*floor(y_min/ytic),ytic,ytic*ceil(y_max/ytic));
  draw_xtics(sx,xtic*floor(x_min/xtic),xtic,xtic*ceil(x_max/xtic));
  s.drawing.text_justify=0;
  set_linestyle(0);
  
  s.drawing.doing_axes=0;
}


namespace {

void draw_ytics(const char *s1, double start, double incr, double end)
{
  double ticvalue,place;
  double y_min=xpp::session().drawing.y_min,y_max=xpp::session().drawing.y_max,
  x_min=xpp::session().drawing.x_min;
  int xt,yt,s=1;
  xpp::session().drawing.text_justify=2; /* Right justification  */
  for(ticvalue=start;ticvalue<=end;ticvalue+=incr){
    place=CheckZero(ticvalue,incr);
    if(ticvalue<y_min||ticvalue>y_max)continue;
    scale_to_screen(static_cast<float>(x_min),static_cast<float>(place),&xt,&yt);
    xpp::session().drawing.doing_box_axes=0;
    line(xpp::session().drawing.d_left,yt,xpp::session().drawing.d_left+xpp::session().drawing.h_tic,yt);
    xpp::session().drawing.doing_box_axes=1;
    line(xpp::session().drawing.d_right,yt,xpp::session().drawing.d_right-xpp::session().drawing.h_tic,yt);
    xpp::session().drawing.doing_box_axes=0;
    put_text(xpp::session().drawing.d_left-static_cast<int>(1.25*xpp::session().drawing.h_char),yt,xpp::format("{:g}",place).c_str());
  }
   scale_to_screen(static_cast<float>(x_min),static_cast<float>(y_max),&xt,&yt);
   if(xpp::session().drawing.d_top<xpp::session().drawing.d_bottom)s=-1;
   if (xpp::session().plot_file.plt_fmt_flag==SVGFMT)
     svg_y_axis_label(xpp::session().drawing.d_left-xpp::session().drawing.h_char,yt+2*s*xpp::session().drawing.v_char,s1);
   else
     put_text(xpp::session().drawing.d_left-xpp::session().drawing.h_char,yt+2*s*xpp::session().drawing.v_char,s1);

}


void draw_xtics(const char *s2, double start, double incr, double end)
{
  double ticvalue,place;
  double y_min=xpp::session().drawing.y_min,
  x_min=xpp::session().drawing.x_min,x_max=xpp::session().drawing.x_max;

  int xt,yt;
  int s=1;
  if(xpp::session().drawing.d_top<xpp::session().drawing.d_bottom)s=-1;
  xpp::session().drawing.text_justify=1; /* Center justification  */
  for(ticvalue=start;ticvalue<=end;ticvalue+=incr){
    place=CheckZero(ticvalue,incr);
    if(ticvalue<x_min||ticvalue>x_max)continue;
    scale_to_screen(static_cast<float>(place),y_min,&xt,&yt);
    xpp::session().drawing.doing_box_axes=0;
    line(xt,xpp::session().drawing.d_bottom,xt,xpp::session().drawing.d_bottom+s*xpp::session().drawing.v_tic); 
    xpp::session().drawing.doing_box_axes=1;
    line(xt,xpp::session().drawing.d_top,xt,xpp::session().drawing.d_top-s*xpp::session().drawing.v_tic);
    xpp::session().drawing.doing_box_axes=0;
    put_text(xt,yt-static_cast<int>(1.25*xpp::session().drawing.v_char*s),xpp::format("{:g}",place).c_str());
  }
  put_text((xpp::session().drawing.d_left+xpp::session().drawing.d_right)/2,yt-static_cast<int>(2.5*xpp::session().drawing.v_char*s),s2);    


}

} // namespace
