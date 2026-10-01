#include "graphics.h"
#include "session.h"
#include "xpp_util.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "marks_data.h"
#include "image_format.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "graf_par.h"
#include "xpp_io.h"
#include "load_eqn.h"

namespace xpp {

#define MAXPERPLOT 10
#define DEGTORAD .0174532
#define EP1 1.000001
#define max(a,b) ((a>b) ? a : b)
#define SYMSIZE .00175



/*  This is an improved graphics driver for XPP  
    It requires only a few commands
    All positions are integers
   
    point(x,y)        Draws point to (x,y) with pointtype PointStyle
    line(x1,y1,x2,y2) Draws line with linetype LineStyle
    put_text(x1,y,text)  Draws text with TextAngle, Justify
    init_device()     Sets up the default for tics, plotting area,
                      and anything else 

    close_device()  closes files, etc

    The device is assumed to go from (0,0) to (XDMax,YDMax)
    
    DLeft,DRight,DTop,DBottom are the actual graph areas
    VTic Htic are the actual sizes of the tics in device pixels
    VChar HChar are the height and width used for character spacing
 
    linetypes   -2  thick plain lines
                -1  thin plain lines

*/

static int XDMax,YDMax;
/* text is never turned (my_ps.cpp's rotation stays for it) */
const int TextAngle=0;

void get_scale(const xpp::Session &s, double *x1, double *y1, double *x2, double *y2)
{
  *x1=s.drawing.x_min;
  *y1=s.drawing.y_min;
  *x2=s.drawing.x_max;
  *y2=s.drawing.y_max;
}

void set_scale(xpp::Session &s, double x1, double y1, double x2, double y2)
{
  s.drawing.x_min=x1;
  s.drawing.y_min=y1;
  s.drawing.x_max=x2;
  s.drawing.y_max=y2;
}

/* SLUGGISH??? */

void get_draw_area(xpp::Session &s)
{
  get_draw_area_flag(s,1);
}
void get_draw_area_flag(xpp::Session &s, int flag)
{
  unsigned int w,h;
  if(flag==1)
    {
      ui.get_draw_size(s,&w,&h);
      s.plot_windows.current->x11Wid=w;
      s.plot_windows.current->x11Hgt=h;
    }
  else
    {
    
    w=s.plot_windows.current->x11Wid;
    h=s.plot_windows.current->x11Hgt;
  }
  XDMax=w;
  YDMax=h;
  s.drawing.v_tic=max(h/100,1);
  s.drawing.h_tic=max(w/150,1);
  s.drawing.v_char=text_metrics.small_height;
  s.drawing.h_char=text_metrics.small_width;
  
  s.drawing.d_left=12*s.drawing.h_char;
  s.drawing.d_right=XDMax-3*s.drawing.h_char-s.drawing.h_tic;
  s.drawing.d_bottom=YDMax-1-s.drawing.v_char*7/2;
  s.drawing.d_top=s.drawing.v_char*5/2+1;
 set_normal_scale(s);
}

void change_current_linestyle(xpp::Session &s, int newstyle, int *old)
{
 *old=s.plot_windows.current->color[0];
  s.plot_windows.current->color[0]=newstyle;
}

void set_normal_scale(xpp::Session &s)
{
s.drawing.x_min=s.plot_windows.current->xlo;
 s.drawing.y_min=s.plot_windows.current->ylo;
 s.drawing.x_max=s.plot_windows.current->xhi;
 s.drawing.y_max=s.plot_windows.current->yhi;
}

namespace {
/* the picture file being drawn into, as image_format.h's registry index
   (W53, issue #101), or -1 while drawing to the screen (SCRNFMT) */
int active_image_format(const xpp::Session &s)
{
  switch(s.plot_file.plt_fmt_flag){
  case PSFMT: return xpp::IMAGE_FORMAT_PS;
  case SVGFMT: return xpp::IMAGE_FORMAT_SVG;
  default: return -1;
  }
}
} // namespace

void point(xpp::Session &s, int x, int y)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_point(s,x,y);
  else ui.draw_point(x,y);
}

void line(xpp::Session &s, int x1, int y1, int x2, int y2)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_line(s,x1,y1,x2,y2);
  else ui.draw_line(x1,y1,x2,y2);
}
/* draw a little filled circle */

void bead(xpp::Session &s, int x1, int y1)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_bead(s,x1,y1);
  else ui.draw_bead(x1,y1);
}

void frect(xpp::Session &s, int x1, int y1, int w, int h)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_frect(s,x1,y1,w,h);
  else ui.draw_frect(x1,y1,w,h);
}

void put_text(xpp::Session &s, int x, int y, const char *str)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_text(s,x,y,str);
  else ui.draw_text(x,y,str);
}

void init_x11(xpp::Session &s)
{
 get_draw_area(s);
}

void init_ps(xpp::Session &s)
{
  if(!s.drawing.ps_port){
 XDMax=7200;
 YDMax=5040;
 s.drawing.v_tic=63;
 s.drawing.h_tic=63;
 s.drawing.v_char=140;
 s.drawing.h_char=84;
 s.drawing.d_left=12*s.drawing.h_char;
 s.drawing.d_right=XDMax-3*s.drawing.h_char-s.drawing.h_tic;
 s.drawing.d_top=YDMax-1-s.drawing.v_char*7/2;
 s.drawing.d_bottom=s.drawing.v_char*5/2+1;
  }
  else
    {
 YDMax=7200;
 XDMax=5040;
 s.drawing.v_tic=63;
 s.drawing.h_tic=63;
 s.drawing.v_char=140;
 s.drawing.h_char=84;
 s.drawing.d_left=12*s.drawing.h_char;
 s.drawing.d_right=XDMax-3*s.drawing.h_char-s.drawing.h_tic;
 s.drawing.d_top=YDMax-1-s.drawing.v_char*7/2;
 s.drawing.d_bottom=s.drawing.v_char*5/2+1;

    }

}

void init_svg(xpp::Session &s)
{
  XDMax=640;
  YDMax=400;
  s.drawing.v_tic=9;
  s.drawing.h_tic=9;
  s.drawing.v_char=20;
  s.drawing.h_char=12;
  s.drawing.d_left=12*s.drawing.h_char;
  s.drawing.d_right=XDMax-3*s.drawing.h_char-s.drawing.h_tic;
  s.drawing.d_bottom=YDMax-1-s.drawing.v_char*7/2;
  s.drawing.d_top=s.drawing.v_char*5/2+1;
}

void set_linestyle(xpp::Session &s, int ls)
{
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_linetype(s,ls);
  else ui.draw_linestyle(ls);
}

void scale_dxdy(const xpp::Session &s, float x, float y, double *i, double *j)
{
  float dx=(s.drawing.d_right-s.drawing.d_left)/(s.drawing.x_max-s.drawing.x_min);
  float dy=(s.drawing.d_top-s.drawing.d_bottom)/(s.drawing.y_max-s.drawing.y_min);
  *i=x*dx;
  *j=y*dy;
}  
     
void scale_to_screen(const xpp::Session &s, float x, float y, int *i, int *j)  /* not really the screen!  */
{
  float dx=(s.drawing.d_right-s.drawing.d_left)/(s.drawing.x_max-s.drawing.x_min);
  float dy=(s.drawing.d_top-s.drawing.d_bottom)/(s.drawing.y_max-s.drawing.y_min);
  *i=static_cast<int>((x-s.drawing.x_min)*dx)+s.drawing.d_left;
  *j=static_cast<int>((y-s.drawing.y_min)*dy)+s.drawing.d_bottom;
}

void scale_to_real(xpp::Session &s, int i, int j, float *x, float *y)  /* Not needed except for X */
{
  int i1,j1;
  float x1,y1;
  get_draw_area(s);
  i1=i-s.drawing.d_left;
  j1=j-s.drawing.d_bottom;
  x1=static_cast<float>(i1);
  y1=static_cast<float>(j1);
  *x=(s.plot_windows.current->xhi-s.plot_windows.current->xlo)*x1/static_cast<float>(s.drawing.d_right-s.drawing.d_left)+s.plot_windows.current->xlo;
  *y=(s.plot_windows.current->yhi-s.plot_windows.current->ylo)*y1/static_cast<float>(s.drawing.d_top-s.drawing.d_bottom)+s.plot_windows.current->ylo;
  
 }

void reset_all_line_type(xpp::Session &s)
{
	int j,k;
	for(j=0;j<MAXPOP;j++)
	{
		for(k=0;k<MAXPERPLOT;k++)
		{
			s.plot_windows.graph[j].line[k]=s.plot_settings.start_line_type;
		}
	}

}

void init_all_graph(xpp::Session &s)
{
 int i;
 for(i=0;i<MAXPOP;i++)
 init_graph(s,i);
 s.plot_windows.current=&s.plot_windows.graph[0];
 set_normal_scale(s);

}

void set_extra_graphs(xpp::Session &s)
{
  int i;
  if(s.plot_settings.npltv<2)return;
  if(s.plot_settings.npltv>8){
    s.plot_settings.npltv=8;
  }
  if(s.plot_settings.multi_win==0){
    s.plot_windows.current->nvars=s.plot_settings.npltv;
    for(i=1;i<s.plot_settings.npltv;i++){
      s.plot_windows.current->xv[i]=s.plot_settings.ix_plt[i+1];
    s.plot_windows.current->yv[i]=s.plot_settings.iy_plt[i+1];
    s.plot_windows.current->zv[i]=s.plot_settings.iz_plt[i+1];
    s.plot_windows.current->color[i]=i;
    }
    return;
  }
  if(program.interactive){
  for(i=1;i<s.plot_settings.npltv;i++){
    create_a_pop(s);
    s.plot_windows.graph[i].xv[0]=s.plot_settings.ix_plt[i+1];
    s.plot_windows.graph[i].yv[0]=s.plot_settings.iy_plt[i+1];
    s.plot_windows.graph[i].zv[0]=s.plot_settings.iz_plt[i+1]; /* irrelevant probably */
    s.plot_windows.graph[i].grtype=0; /* force 2D */
    s.plot_windows.graph[i].xlo=s.plot_settings.x_lo[i+1];
    s.plot_windows.graph[i].xhi=s.plot_settings.x_hi[i+1];
    s.plot_windows.graph[i].ylo=s.plot_settings.y_lo[i+1];
    s.plot_windows.graph[i].yhi=s.plot_settings.y_hi[i+1];
  }
  xpp::set_active_windows(s);
  xpp::make_active(s,0,1); 
  }
}

void reset_graph(xpp::Session &s)
{
  if(s.plot_settings.axes>=5)
    s.plot_settings.plot_3d=1;
  else
    s.plot_settings.plot_3d=0;
  s.plot_windows.current->xv[0]=s.plot_settings.ixplt;
  s.plot_windows.current->yv[0]=s.plot_settings.iyplt;
  s.plot_windows.current->zv[0]=s.plot_settings.izplt;
   s.plot_windows.current->xmax=s.plot_settings.x_3d[1];
    s.plot_windows.current->ymax=s.plot_settings.y_3d[1];
    s.plot_windows.current->zmax=s.plot_settings.z_3d[1];
    s.plot_windows.current->xbar=.5*(s.plot_settings.x_3d[1]+s.plot_settings.x_3d[0]);
    s.plot_windows.current->ybar=.5*(s.plot_settings.y_3d[1]+s.plot_settings.y_3d[0]);
    s.plot_windows.current->zbar=.5*(s.plot_settings.z_3d[1]+s.plot_settings.z_3d[0]);
    s.plot_windows.current->dx=2./(s.plot_settings.x_3d[1]-s.plot_settings.x_3d[0]);
    s.plot_windows.current->dy=2./(s.plot_settings.y_3d[1]-s.plot_settings.y_3d[0]);
    s.plot_windows.current->dz=2./(s.plot_settings.z_3d[1]-s.plot_settings.z_3d[0]);
    s.plot_windows.current->xmin=s.plot_settings.x_3d[0];
    s.plot_windows.current->ymin=s.plot_settings.y_3d[0];
    s.plot_windows.current->zmin=s.plot_settings.z_3d[0];
    s.plot_windows.current->xlo=s.plot_settings.my_xlo;
    s.plot_windows.current->ylo=s.plot_settings.my_ylo;
    s.plot_windows.current->xhi=s.plot_settings.my_xhi;
    s.plot_windows.current->yhi=s.plot_settings.my_yhi;
    s.plot_windows.current->grtype=s.plot_settings.axes;
    xpp::check_windows(s);
    set_normal_scale(s);
    ui.redraw_graph(s);
}

void get_graph(xpp::Session &s)
{
 s.plot_settings.x_3d[0]=s.plot_windows.current->xmin;
 s.plot_settings.x_3d[1]=s.plot_windows.current->xmax;
s.plot_settings.y_3d[0]=s.plot_windows.current->ymin;
 s.plot_settings.y_3d[1]=s.plot_windows.current->ymax;
s.plot_settings.z_3d[0]=s.plot_windows.current->zmin;
 s.plot_settings.z_3d[1]=s.plot_windows.current->zmax;
s.plot_settings.my_xlo=s.plot_windows.current->xlo;
s.plot_settings.my_ylo=s.plot_windows.current->ylo;
s.plot_settings.my_xhi=s.plot_windows.current->xhi;
s.plot_settings.my_yhi=s.plot_windows.current->yhi;
s.plot_settings.ixplt=s.plot_windows.current->xv[0];
s.plot_settings.iyplt=s.plot_windows.current->yv[0];
s.plot_settings.izplt=s.plot_windows.current->zv[0];
s.plot_settings.plot_3d=s.plot_windows.current->ThreeDFlag;
if(s.plot_settings.plot_3d)
  s.plot_settings.axes=5;
else
  s.plot_settings.axes=0;
s.plot_settings.axes=s.plot_windows.current->grtype;  
}

void init_graph(xpp::Session &s, int i)
{
 int j,k;
 if(s.plot_settings.axes<=3)s.plot_settings.axes=0;
 for(j=0;j<3;j++)
  for(k=0;k<3;k++)
        if(k==j)s.plot_windows.graph[i].rm[k][j]=1.0;
	else s.plot_windows.graph[i].rm[k][j]=0.0;
 s.plot_windows.graph[i].nvars=1;
  for(j=0;j<MAXPERPLOT;j++){
        s.plot_windows.graph[i].xv[j]=s.plot_settings.ixplt;
	s.plot_windows.graph[i].yv[j]=s.plot_settings.iyplt;
	s.plot_windows.graph[i].zv[j]=s.plot_settings.izplt;
        s.plot_windows.graph[i].line[j]=s.plot_settings.start_line_type;
	s.plot_windows.graph[i].color[j]=0;
        }
     
    s.plot_windows.graph[i].xlabel.clear();
    s.plot_windows.graph[i].ylabel.clear();
    s.plot_windows.graph[i].zlabel.clear();
    
    s.plot_windows.graph[i].Use=0;
    s.plot_windows.graph[i].Nullrestore=0;
    s.plot_windows.graph[i].ZPlane=-1000.0;
    s.plot_windows.graph[i].ZView=1000.0;
    s.plot_windows.graph[i].PerspFlag=0;
    s.plot_windows.graph[i].ThreeDFlag=s.plot_settings.plot_3d;
    s.plot_windows.graph[i].TimeFlag=s.plot_settings.timplot;
    s.plot_windows.graph[i].ColorFlag=0;
    s.plot_windows.graph[i].grtype=s.plot_settings.axes;
    s.plot_windows.graph[i].color_scale=1.0;
    s.plot_windows.graph[i].min_scale=0.0;
    s.plot_windows.graph[i].xmax=s.plot_settings.x_3d[1];
    s.plot_windows.graph[i].ymax=s.plot_settings.y_3d[1];
    s.plot_windows.graph[i].zmax=s.plot_settings.z_3d[1];
    s.plot_windows.graph[i].xbar=.5*(s.plot_settings.x_3d[1]+s.plot_settings.x_3d[0]);
    s.plot_windows.graph[i].ybar=.5*(s.plot_settings.y_3d[1]+s.plot_settings.y_3d[0]);
    s.plot_windows.graph[i].zbar=.5*(s.plot_settings.z_3d[1]+s.plot_settings.z_3d[0]);
    s.plot_windows.graph[i].dx=2./(s.plot_settings.x_3d[1]-s.plot_settings.x_3d[0]);
    s.plot_windows.graph[i].dy=2./(s.plot_settings.y_3d[1]-s.plot_settings.y_3d[0]);
    s.plot_windows.graph[i].dz=2./(s.plot_settings.z_3d[1]-s.plot_settings.z_3d[0]);
    s.plot_windows.graph[i].xmin=s.plot_settings.x_3d[0];
    s.plot_windows.graph[i].ymin=s.plot_settings.y_3d[0];
    s.plot_windows.graph[i].zmin=s.plot_settings.z_3d[0];
    s.plot_windows.graph[i].xorg=0.0;
    s.plot_windows.graph[i].yorg=0.0;
    s.plot_windows.graph[i].yorg=0.0;
    s.plot_windows.graph[i].xorgflag=1;
    s.plot_windows.graph[i].yorgflag=1;
    s.plot_windows.graph[i].zorgflag=1;
    s.plot_windows.graph[i].Theta=s.drawing.theta0;
    s.plot_windows.graph[i].Phi=s.drawing.phi0;
    s.plot_windows.graph[i].xshft=0;
    s.plot_windows.graph[i].yshft=0;
    s.plot_windows.graph[i].zshft=0;
    s.plot_windows.graph[i].xlo=s.plot_settings.my_xlo;
    s.plot_windows.graph[i].ylo=s.plot_settings.my_ylo;
    s.plot_windows.graph[i].oldxlo=s.plot_settings.my_xlo;
    s.plot_windows.graph[i].oldylo=s.plot_settings.my_ylo;
    s.plot_windows.graph[i].xhi=s.plot_settings.my_xhi;
    s.plot_windows.graph[i].yhi=s.plot_settings.my_yhi;
    s.plot_windows.graph[i].oldxhi=s.plot_settings.my_xhi;
    s.plot_windows.graph[i].oldyhi=s.plot_settings.my_yhi;
    s.plot_windows.current=&s.plot_windows.graph[i];
    make_rot(s,s.drawing.theta0,s.drawing.phi0);
    
  }

void copy_graph(xpp::Session &s, int i, int l)  /*  Graph[i]=Graph[l]  */
{
 int j,k;
 s.plot_windows.graph[i].Use=s.plot_windows.graph[l].Use;
 s.plot_windows.graph[i].Nullrestore=s.plot_windows.graph[l].Nullrestore;
 for(j=0;j<3;j++)
  for(k=0;k<3;k++)
        s.plot_windows.graph[i].rm[k][j]=s.plot_windows.graph[l].rm[k][j];
 s.plot_windows.graph[i].nvars=s.plot_windows.graph[l].nvars;
  for(j=0;j<MAXPERPLOT;j++){
        s.plot_windows.graph[i].xv[j]=s.plot_windows.graph[l].xv[j];
	s.plot_windows.graph[i].yv[j]=s.plot_windows.graph[l].yv[j];
	s.plot_windows.graph[i].zv[j]=s.plot_windows.graph[l].zv[j];
        s.plot_windows.graph[i].line[j]=s.plot_windows.graph[l].line[j];
	s.plot_windows.graph[i].color[j]=s.plot_windows.graph[l].color[j];
        }

    s.plot_windows.graph[i].ZPlane=s.plot_windows.graph[l].ZPlane;
    s.plot_windows.graph[i].ZView=s.plot_windows.graph[l].ZView;
    s.plot_windows.graph[i].PerspFlag=s.plot_windows.graph[l].PerspFlag;
    s.plot_windows.graph[i].ThreeDFlag=s.plot_windows.graph[l].ThreeDFlag;
    s.plot_windows.graph[i].TimeFlag=s.plot_windows.graph[l].TimeFlag;
    s.plot_windows.graph[i].ColorFlag=s.plot_windows.graph[l].ColorFlag;
    s.plot_windows.graph[i].grtype=s.plot_windows.graph[l].grtype;
    s.plot_windows.graph[i].color_scale=s.plot_windows.graph[l].color_scale;
    s.plot_windows.graph[i].min_scale=s.plot_windows.graph[l].min_scale;

    s.plot_windows.graph[i].xmax=s.plot_windows.graph[l].xmax;
    s.plot_windows.graph[i].xmin=s.plot_windows.graph[l].xmin;
    s.plot_windows.graph[i].ymax=s.plot_windows.graph[l].ymax;
    s.plot_windows.graph[i].ymin=s.plot_windows.graph[l].ymin;
    s.plot_windows.graph[i].zmax=s.plot_windows.graph[l].zmax;
    s.plot_windows.graph[i].zmin=s.plot_windows.graph[l].zmin;
    s.plot_windows.graph[i].xbar=s.plot_windows.graph[l].xbar;
    s.plot_windows.graph[i].dx  =s.plot_windows.graph[l].dx  ;
    s.plot_windows.graph[i].ybar=s.plot_windows.graph[l].ybar;
    s.plot_windows.graph[i].dy  =s.plot_windows.graph[l].dy  ;
    s.plot_windows.graph[i].zbar=s.plot_windows.graph[l].zbar;
    s.plot_windows.graph[i].dz  =s.plot_windows.graph[l].dz  ;

    s.plot_windows.graph[i].Theta=s.plot_windows.graph[l].Theta;
    s.plot_windows.graph[i].Phi=s.plot_windows.graph[l].Phi;
    s.plot_windows.graph[i].xshft=s.plot_windows.graph[l].xshft;
    s.plot_windows.graph[i].yshft=s.plot_windows.graph[l].yshft;
    s.plot_windows.graph[i].zshft=s.plot_windows.graph[l].zshft;
    s.plot_windows.graph[i].xlo=s.plot_windows.graph[l].xlo;
    s.plot_windows.graph[i].ylo=s.plot_windows.graph[l].ylo;
    s.plot_windows.graph[i].oldxlo=s.plot_windows.graph[l].oldxlo;
    s.plot_windows.graph[i].oldylo=s.plot_windows.graph[l].oldylo;
    s.plot_windows.graph[i].xhi=s.plot_windows.graph[l].xhi;
    s.plot_windows.graph[i].yhi=s.plot_windows.graph[l].yhi;
    s.plot_windows.graph[i].oldxhi=s.plot_windows.graph[l].oldxhi;
    s.plot_windows.graph[i].oldyhi=s.plot_windows.graph[l].oldyhi;
  }

void make_rot(xpp::Session &s, double theta, double phi)
{
 double ct=cos(DEGTORAD*theta),st=sin(DEGTORAD*theta);
 double sp=sin(DEGTORAD*phi),cp=cos(DEGTORAD*phi);
 s.plot_windows.current->Theta=theta;
 s.plot_windows.current->Phi=phi;
 s.plot_windows.current->rm[0][0]=ct;
 s.plot_windows.current->rm[0][1]=st;
 s.plot_windows.current->rm[0][2]=0.0;
 s.plot_windows.current->rm[1][0]=-cp*st;
 s.plot_windows.current->rm[1][1]=cp*ct;
 s.plot_windows.current->rm[1][2]=sp;
 s.plot_windows.current->rm[2][0]=st*sp;
 s.plot_windows.current->rm[2][1]=-sp*ct;
 s.plot_windows.current->rm[2][2]=cp;
}

void scale3d(const xpp::Session &s, float x, float y, float z, float *xp, float *yp, float *zp)
{
 *xp=(x-s.plot_windows.current->xbar)*s.plot_windows.current->dx;
 *yp=(y-s.plot_windows.current->ybar)*s.plot_windows.current->dy;
 *zp=(z-s.plot_windows.current->zbar)*s.plot_windows.current->dz;
}

int threedproj(const xpp::Session &s, float x2p, float y2p, float z2p, float *xp, float *yp)
{
  float x1p,y1p,z1p,k;
 rot_3dvec(s,x2p,y2p,z2p,&x1p,&y1p,&z1p);

 if(s.plot_windows.current->PerspFlag==0){
 *xp=x1p;
 *yp=y1p;
  return(1);
 }
  if((z1p>=static_cast<float>(s.plot_windows.current->ZView))||(z1p<static_cast<float>(s.plot_windows.current->ZPlane)))return(0);
  k=static_cast<float>(s.plot_windows.current->ZView-s.plot_windows.current->ZPlane)/(static_cast<float>(s.plot_windows.current->ZView)-z1p);
  x1p=k*x1p;
  y1p=k*y1p;
  *xp=x1p;
 *yp=y1p;
  return(1);
}

void text3d(xpp::Session &s, float x, float y, float z, const char *str)
{
 float xp,yp;
if(threedproj(s,x,y,z,&xp,&yp)) text_abs(s,xp,yp,str);
}

int threed_proj(const xpp::Session &s, float x, float y, float z, float *xp, float *yp)
{
  float x1p,y1p,z1p,k;
 float x2p,y2p,z2p;
 scale3d(s,x,y,z,&x2p,&y2p,&z2p);  /* scale to a cube  */
 rot_3dvec(s,x2p,y2p,z2p,&x1p,&y1p,&z1p);

 if(s.plot_windows.current->PerspFlag==0){
 *xp=x1p;
 *yp=y1p;
  return(1);
 }
  if((z1p>=static_cast<float>(s.plot_windows.current->ZView))||(z1p<static_cast<float>(s.plot_windows.current->ZPlane)))return(0);
  k=static_cast<float>(s.plot_windows.current->ZView-s.plot_windows.current->ZPlane)/(static_cast<float>(s.plot_windows.current->ZView)-z1p);
  x1p=k*x1p;
  y1p=k*y1p;
  *xp=x1p;
 *yp=y1p;
  return(1);
}

void point_3d(xpp::Session &s, float x, float y, float z)
{
 float xp,yp;
 if(threed_proj(s,x,y,z,&xp,&yp))point_abs(s,xp,yp);
}

void line3dn(xpp::Session &s, float xs1, float ys1, float zs1, float xsp1, float ysp1, float zsp1)  /* unscaled version  unclipped   */
{
 float xs,ys,zs;
 float xsp,ysp,zsp;
 rot_3dvec(s,xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(s,xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(s.plot_windows.current->PerspFlag)pers_line(s,xs,ys,zs,xsp,ysp,zsp);
 else
     line_nabs(s,xs,ys,xsp,ysp);
 }

void line3d(xpp::Session &s, float x01, float y01, float z01, float x02, float y02, float z02)  /* unscaled version     */
{
 float xs,ys,zs;
 float xs1,ys1,zs1;
 float xsp,ysp,zsp;
 float xsp1,ysp1,zsp1;
if(!clip3d(x01,y01,z01,x02,y02,z02,&xs1,&ys1,&zs1,&xsp1,&ysp1,&zsp1))return;
 rot_3dvec(s,xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(s,xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(s.plot_windows.current->PerspFlag)pers_line(s,xs,ys,zs,xsp,ysp,zsp);
 else
     line_abs(s,xs,ys,xsp,ysp);
 }

void line_3d(xpp::Session &s, float x, float y, float z, float xp, float yp, float zp)
{
 float xs,ys,zs;
float xs1,ys1,zs1;
 float xsp,ysp,zsp;
 float xsp1,ysp1,zsp1;
 float x01,x02,y01,y02,z01,z02;
 scale3d(s,x,y,z,&x01,&y01,&z01);          /* scale to a cube  */
 scale3d(s,xp,yp,zp,&x02,&y02,&z02);
 if(!clip3d(x01,y01,z01,x02,y02,z02,&xs1,&ys1,&zs1,&xsp1,&ysp1,&zsp1))return;
 rot_3dvec(s,xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(s,xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(s.plot_windows.current->PerspFlag)pers_line(s,xs,ys,zs,xsp,ysp,zsp);
 else
     line_abs(s,xs,ys,xsp,ysp);
 }

void pers_line(xpp::Session &s, float x, float y, float z, float xp, float yp, float zp)
{
 float Zv=static_cast<float>(s.plot_windows.current->ZView),Zp=static_cast<float>(s.plot_windows.current->ZPlane);
 float d=Zv-Zp,k;
 float eps=.005*d;

 if(((zp>=Zv)&&(z>=Zv))||((zp<Zp)&&(z<Zp)))return;
 if(zp>Zv)
 {
  k=(Zv-eps-z)/(zp-z);
  zp=Zv-eps;
  yp=y+k*(yp-y);
  xp=x+k*(xp-x);
 }
 if(z>Zv)
 {
  k=(Zv-eps-zp)/(z-zp);
  z=Zv-eps;
  y=yp+k*(y-yp);
  x=xp+k*(x-xp);
 }
 if(zp<Zp)
 {
  k=(Zp-z)/(zp-z);
  zp=Zp;
  yp=y+k*(yp-y);
  xp=x+k*(xp-x);
 }
 if(z<Zp)
 {
  k=(Zp-zp)/(z-zp);
  z=Zp;
  y=yp+k*(y-yp);
  x=xp+k*(x-xp);
 }
 k=d/(Zv-zp);
 xp=xp*k;
 yp=yp*k;
 k=d/(Zv-z);
 x=k*x;
 y=k*y;
 line_abs(s,x,y,xp,yp);
}

void rot_3dvec(const xpp::Session &s, float x, float y, float z, float *xp, float *yp, float *zp)
{
 int i,j;
 double vt[3],vnew[3];
 vt[0]=x;
 vt[1]=y;
 vt[2]=z;

 for(i=0;i<3;i++){
	vnew[i]=0.0;
	for(j=0;j<3;j++)vnew[i]=vnew[i]+s.plot_windows.current->rm[i][j]*vt[j];
	}
	*xp=vnew[0];
	*yp=vnew[1];
	*zp=vnew[2];

}

void point_abs(xpp::Session &s, float x1, float y1)
{
  int xp,yp;

  float x_left=s.drawing.x_min;
  float x_right=s.drawing.x_max;
  float y_top=s.drawing.y_max;
  float y_bottom=s.drawing.y_min;
   if((x1>x_right)||(x1<x_left)||(y1>y_top)||(y1<y_bottom))return; 
  scale_to_screen(s,x1,y1,&xp,&yp);
  point(s,xp,yp);
}

void line_nabs(xpp::Session &s, float x1_out, float y1_out, float x2_out, float y2_out)
{

  int xp1,yp1,xp2,yp2;

    scale_to_screen(s,x1_out,y1_out,&xp1,&yp1);
    scale_to_screen(s,x2_out,y2_out,&xp2,&yp2);
    line(s,xp1,yp1,xp2,yp2);
  }

void bead_abs(xpp::Session &s, float x1, float y1)
{
  int i1,j1;
  float x_left=s.drawing.x_min;
  float x_right=s.drawing.x_max;
  float y_top=s.drawing.y_max;
  float y_bottom=s.drawing.y_min;
   if((x1>x_right)||(x1<x_left)||(y1>y_top)||(y1<y_bottom))return; 
  scale_to_screen(s,x1,y1,&i1,&j1);
  bead(s,i1,j1);
}

void frect_abs(xpp::Session &s, float x1, float y1, float w, float h)
{
 int i1,i2,j1,j2;
 int ih,iw;
 float x2=x1+w;
 float y2=y1+h;
 scale_to_screen(s,x1,y1,&i1,&j1);
 scale_to_screen(s,x2,y2,&i2,&j2);
 iw=abs(i2-i1);
 ih=abs(j2-j1);
 frect(s,i1,j1,iw+1,ih+1);
}

void line_abs(xpp::Session &s, float x1, float y1, float x2, float y2)
{
  float x1_out,y1_out,x2_out,y2_out;

  int xp1,yp1,xp2,yp2;
  if(clip(s.drawing,x1,x2,y1,y2,&x1_out,&y1_out,&x2_out,&y2_out)){
    scale_to_screen(s,x1_out,y1_out,&xp1,&yp1);
    scale_to_screen(s,x2_out,y2_out,&xp2,&yp2);
    line(s,xp1,yp1,xp2,yp2);
  }
}

void text_abs(xpp::Session &s, float x, float y, const char *text)
{
 int xp,yp;
 scale_to_screen(s,x,y,&xp,&yp);
 put_text(s,xp,yp,text);
}

/* old with each \{expr} replaced by the expression's value (%g), ? for
   one that does not evaluate (the rest up to the next } then joins the
   expression), and a ? ending an unclosed one */
std::string fill_in_text(xpp::Session &s, std::string_view old)
{
  std::string out;
  const size_t l=old.size();
  if(l==0)return out;
  size_t i=0;
  for(;;){
    const char c=old[i];
    if(c=='\\'&&i+1<l&&old[i+1]=='{'){
      std::string name;
      i+=2;
      for(;;){
        const char c2=i<l?old[i]:'\0';
        if(c2=='}'){
          double z;
          if(xpp::do_calc(s,name,&z)!=-1){
            out+=xpp::format("{:g}",z);
            break;
          }
          out+='?';
        }
        else
          name+=c2;
        i++;
        if(i>=l){ /* oops - end of string */
          out+='?';
          return out;
        }
      }
    } /* ok - we have found matching and are done */
    else
      out+=c;
    i++;
    if(i>=l)
      break;
  }
  return out;
}

void fancy_text_abs(xpp::Session &s, float x, float y, const char *old, int size, int font)
{
  int xp,yp;
  scale_to_screen(s,x,y,&xp,&yp);
  const std::string text=fill_in_text(s,old);
  int f=active_image_format(s);
  if(f>=0)xpp::image_formats[f].draw_special_text(s,xp,yp,text.c_str(),size);
  else ui.draw_special_text(xp,yp,text.c_str(),size);
    
}

int clip3d(float x1, float y1, float z1, float x2, float y2, float z2, float *x1p, float *y1p, float *z1p, float *x2p, float *y2p, float *z2p)
{
  int istack,ix1=0,ix2=0,iy1=0,iy2=0,iz1=0,iz2=0,iflag=0;

  float wh,wv,wo,xhat,yhat,zhat,del;

  istack=1;
  *x1p=x1;
  *y1p=y1;
  *z1p=z1;
  *x2p=x2;
  *y2p=y2;
  *z2p=z2;
  if(x1<-1.)ix1=-1;
  if(x1>1.)ix1=1;
  if(x2<-1.)ix2=-1;
  if(x2>1.)ix2=1;
  if(y1<-1.)iy1=-1;
  if(y1>1.)iy1=1;
  if(y2<-1.)iy2=-1;
  if(y2>1.)iy2=1;
   if(z1<-1.)iz1=-1;
  if(z1>1.)iz1=1;
  if(z2<-1.)iz2=-1;
  if(z2>1.)iz2=1;

  if((abs(ix1)+abs(ix2)+abs(iy1)+abs(iy2)+abs(iz1)+abs(iz2))==0)return(1);

/*  Both are outside the cube  */

if((ix1==ix2)&&(ix1!=0))return(0);
if((iy1==iy2)&&(iy1!=0))return(0);
 if((iz1==iz2)&&(iz1!=0))return(0);

if(ix1==0)goto C2;
wv=-1;
if(ix1>0)wv=1;
*x1p=wv;
del=(wv-x2)/(x1-x2);
yhat=(y1-y2)*del+y2;
zhat=(z1-z2)*del+z2;
if(fabs(zhat)<=EP1&&fabs(yhat)<=EP1){
*y1p=yhat;
*z1p=zhat;
iflag=1;
goto C3;
 }
istack=0;
C2:
if(iy1==0)goto C22;
  wh=-1;
  if(iy1>0)wh=1;
  *y1p=wh;
 del=(wh-y2)/(y1-y2);
 xhat=(x1-x2)*del+x2;
 zhat=(z1-z2)*del+z2;
 if(fabs(zhat)<=EP1&&fabs(xhat)<=EP1){
 *x1p=xhat;
 *z1p=zhat;
 iflag=1;
 goto C3;
}
istack=0;
C22:
if(iz1==0)goto C3;
wo=-1;
if(iz1>0)wo=1;
*z1p=wo;

del=(wo-z2)/(z1-z2);
xhat=del*(x1-x2)+x2;
yhat=del*(y1-y2)+y2;

if(fabs(xhat)<=EP1&&fabs(yhat)<=EP1){
*x1p=xhat;
*y1p=yhat;
iflag=1;}
else istack=0;
C3:
istack+=iflag;
if((ix2==0)||(istack==0))goto C44;
wv=-1;
if(ix2>0)wv=1;
*x2p=wv;
del=(wv-x1)/(x2-x1);
yhat=(y2-y1)*del+y1;
zhat=(z2-z1)*del+z1;
if(fabs(yhat)<=EP1&&fabs(zhat)<=EP1){
*y2p=yhat;
*z2p=zhat;
return(1);
 }
C44:
 if(iy2==0||istack==0)goto C4;
wh=-1;
if(iy2>0)wh=1;
*y2p=wh;
del=(wh-y1)/(y2-y1);
xhat=(x2-x1)*del+x1;
zhat=(z2-z1)*del+z1;
if(fabs(xhat)<=EP1&&fabs(zhat)<=EP1){
 *z2p=zhat;
 *x2p=xhat;
 return(1);
 }
C4:
 if(iz2==0||istack==0)return(iflag);
 wo=-1;
 if(iz2>0)wo=1;
 *z2p=wo;
 del=(wo-z1)/(z2-z1);
 xhat=(x2-x1)*del+x1;
 yhat=(y2-y1)*del+y1;
 if(fabs(xhat)<=EP1&&fabs(yhat)<=EP1){
 *x2p=xhat;
 *y2p=yhat;
 return(1);
 }
 return(iflag);
}

/************************************************************ *
*  Clipping algorithm                                         *
*   on input,                                                 *
*           (x1,y1) and (x2,y2) are endpoints for line        *
*           (x_left,y_bottom) and (x_right,y_top) is window                     *
*   output:                                                   *
*           value is 1 for drawing, 0 for no drawing          *
*           (x1_out,y1_out),(x2_out,y2_out) are endpoints     *
*            of clipped line                                  *
***************************************************************/
int clip(const DrawingState &d, float x1, float x2, float y1, float y2, float *x1_out, float *y1_out, float *x2_out, float *y2_out)
{
   int istack,ix1,ix2,iy1,iy2,isum,iflag;
   float  wh,xhat,yhat,wv;
   float x_left=d.x_min;
   float x_right=d.x_max;
   float y_top=d.y_max;
   float y_bottom=d.y_min;
   istack=1;
   ix1=ix2=iy1=iy2=iflag=0;
   *y1_out=y1;
   *y2_out=y2;
   *x1_out=x1;
   *x2_out=x2;
   if(x1<x_left)ix1=-1;
   if(x1>x_right)ix1=1;
   if(x2<x_left)ix2=-1;
   if(x2>x_right)ix2=1;
   if(y2<y_bottom)iy2=-1;
   if(y2>y_top)iy2=1;
   if(y1<y_bottom)iy1=-1;
   if(y1>y_top)iy1=1;
   isum=abs(ix1)+abs(ix2)+abs(iy1)+abs(iy2);
   if(isum==0)return(1); /* both inside window so plottem' */

   if(((ix1==ix2)&&(ix1!=0))||((iy1==iy2)&&(iy1!=0)))return(0); 
   if(ix1==0) goto C2;
   wv=x_left;
   if(ix1>0) wv=x_right;
   *x1_out=wv;
   yhat=(y1-y2)*(wv-x2)/(x1-x2)+y2;
   if((yhat<=y_top)&&(yhat>=y_bottom)) {
     *y1_out=yhat;
     iflag=1;
   goto C3; }
   istack=0;
C2:
   if(iy1==0) goto C3;
   wh=y_bottom;
   if(iy1>0)wh=y_top;
   *y1_out=wh;
   xhat=(x1-x2)*(wh-y2)/(y1-y2)+x2;
   if((xhat<=x_right)&&(xhat>=x_left)) {
     *x1_out=xhat;
     iflag=1; }
   else istack=0;
C3:
   istack+=iflag;
   if((ix2==0)||(istack==0)) goto C4;
   wv=x_left;
   if(ix2>0) wv=x_right;
   *x2_out=wv;
   yhat=(y2-y1)*(wv-x1)/(x2-x1)+y1;
   if((yhat<=y_top)&&(yhat>=y_bottom)) {
   *y2_out=yhat;
   return(1);
   }
C4:
  if((iy2==0)||(istack==0))return(iflag);
  wh=y_bottom;
  if(iy2>0) wh=y_top;
  *y2_out=wh;
  xhat=(x2-x1)*(wh-y1)/(y2-y1)+x1;
  if((xhat<=x_right)&&(xhat>=x_left)) {
  *x2_out=xhat;
  return(1);
  }
  return(iflag);
}

void eq_symb(xpp::Session &s, double *x, int type)
{

  float dx=6.0*static_cast<float>(s.plot_windows.current->xhi-s.plot_windows.current->xlo)*SYMSIZE;
  float dy=6.0*static_cast<float>(s.plot_windows.current->yhi-s.plot_windows.current->ylo)*SYMSIZE;
 int ix=s.plot_windows.current->xv[0]-1,iy=s.plot_windows.current->yv[0]-1,iz=s.plot_windows.current->zv[0]-1;
 if(!program.interactive)return;
  if(s.plot_windows.current->TimeFlag)return;
  set_color(0); 
  if(s.plot_windows.current->ThreeDFlag)
  {
   dx=6.0*SYMSIZE/s.plot_windows.current->dx;
   dy=6.0*SYMSIZE/s.plot_windows.current->dy;
   line_3d(s,static_cast<float>(x[ix])+dx,static_cast<float>(x[iy]),static_cast<float>(x[iz]),
           static_cast<float>(x[ix])-dx,static_cast<float>(x[iy]),static_cast<float>(x[iz]));
   line_3d(s,static_cast<float>(x[ix]),static_cast<float>(x[iy])+dy,static_cast<float>(x[iz]),
           static_cast<float>(x[ix]),static_cast<float>(x[iy])-dy,static_cast<float>(x[iz]));
  return;
  }
  draw_symbol(s,static_cast<float>(x[ix]),static_cast<float>(x[iy]),SYMSIZE,type);
  point_abs(s,static_cast<float>(x[ix]),static_cast<float>(x[iy]));
  if(ix>=0&&iy>=0)marks_data_equilibrium(s.plot_windows,x[ix],x[iy],type); /* the mark as data */
 
}

void draw_symbol(xpp::Session &s, float x, float y, float size, int my_symb)
{
 float dx=static_cast<float>(s.plot_windows.current->xhi-s.plot_windows.current->xlo)*size;
 float dy=static_cast<float>(s.plot_windows.current->yhi-s.plot_windows.current->ylo)*size;
 static int sym_dir[4][48] = {
 /*          box              */
    {0, -6, -6,1, 12,  0,1,  0, 12,1,-12,  0,
    1,  0,-12,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0},

 /*          triangle         */
    {0, -6, -6,1, 12,  0,1, -6, 12,1, -6,-12,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0},

 /*          cross            */
    {0, -6,  0,1, 12,  0,0, -6, -6,1,  0, 12,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
    3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0},

 /*          circle           */
    {0,  6,  0,1, -1,  3,1, -2,  2,1, -3,  1,
    1, -3, -1,1, -2, -2,1, -1, -3,1,  1, -3,
    1,  2, -2,1,  3, -1,1,  3,  1,1,  2,  2,
    1,  1,  3,3,  0,  0,3,  0,  0,3,  0,  0},
    };
  int ind=0,pen=0;
  float x1=x,y1=y,x2,y2;
 
   while(pen!=3)
   {
    x2=sym_dir[my_symb][3*ind+1]*dx+x1;
    y2=sym_dir[my_symb][3*ind+2]*dy+y1;
    pen=sym_dir[my_symb][3*ind];
    if(pen!=0) line_abs(s,x1,y1,x2,y2);
    x1=x2;
    y1=y2;
    ind++;
   }

}

} // namespace xpp
