#include "graphics.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "marks_data.h"
#include "my_ps.h"
#include "my_svg.h"

#include <stdlib.h> 
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "struct.h"
#include "color.h"
#include "graf_par.h"
#include "calc.h"
#include "many_pops.h"
#include "xpp_io.h"
#include "colormap.h"



#define MAXPERPLOT 10
#define MAXPLOTS 20
#define DEGTORAD .0174532
#define EP1 1.000001
#define TAXIS 2
#define max(a,b) ((a>b) ? a : b)
#define min(a,b) ((a<b) ? a : b)
#define PS_XMAX 7200
#define PS_YMAX 5040
#define SYMSIZE .00175

double THETA0=45,PHI0=45;
extern double x_3d[2],y_3d[2],z_3d[2];
extern int IXPLT,IYPLT,IZPLT;
extern int AXES,TIMPLOT,PLOT_3D;
extern int START_LINE_TYPE;
extern double MY_XLO,MY_YLO,MY_XHI,MY_YHI;
extern int colorline[]; 
extern int PltFmtFlag;
extern unsigned int GrFore,GrBack;
extern int SCALEX,SCALEY,xor_flag;

int PS_Port=0;
int D_FLAG;
int PointRadius=0;
extern float **storage;
extern int storind;



extern int IX_PLT[10],IY_PLT[10],IZ_PLT[10],NPltV;
extern double X_LO[10],Y_LO[10],X_HI[10],Y_HI[10];
extern int MultiWin;

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


int DLeft,DRight,DTop,DBottom,VTic,HTic,VChar,HChar,XDMax,YDMax;
double XMin,YMin,XMax,YMax;
int PointType=-1,TextJustify,TextAngle;

void get_scale(double *x1, double *y1, double *x2, double *y2)
{
  *x1=XMin;
  *y1=YMin;
  *x2=XMax;
  *y2=YMax;
}

void set_scale(double x1, double y1, double x2, double y2)
{
  XMin=x1;
  YMin=y1;
  XMax=x2;
  YMax=y2;
}


/* SLUGGISH??? */

void get_draw_area()
{
  get_draw_area_flag(1);
}
void get_draw_area_flag(int flag)
{
  unsigned int w,h;
  if(flag==1)
    {
      xpp_ui.get_draw_size(&w,&h);
      plot_windows.current->x11Wid=w;
      plot_windows.current->x11Hgt=h;
    }
  else
    {
    
    w=plot_windows.current->x11Wid;
    h=plot_windows.current->x11Hgt;
  }
  /* plintf(" geom:+%d+%d:%dx%d\n",x,y,w,h); */
  XDMax=w;
  YDMax=h;
  VTic=max(h/100,1);
  HTic=max(w/150,1);
  VChar=text_metrics.small_height;  /*max(h/25,1);*/
  HChar=text_metrics.small_width; /* max(w/80,1); */
  
  DLeft=12*HChar;
  DRight=XDMax-3*HChar-HTic;
  DBottom=YDMax-1-VChar*7/2;
  DTop=VChar*5/2+1;
  h=DBottom-DTop;
  w=DRight-DLeft;
  if(h>0&&w>0)D_FLAG=1;
 else D_FLAG=0;
 plot_windows.current->Width=w;
 plot_windows.current->Height=h;
 plot_windows.current->x0=DLeft;
 plot_windows.current->y0=DTop;
 set_normal_scale();
}


void change_current_linestyle(int newstyle, int *old)
{
 *old=plot_windows.current->color[0];
  plot_windows.current->color[0]=newstyle;
}
     

void set_normal_scale()
{
XMin=plot_windows.current->xlo;
 YMin=plot_windows.current->ylo;
 XMax=plot_windows.current->xhi;
 YMax=plot_windows.current->yhi;
}

void point(int x, int y)
{
  if(PltFmtFlag==PSFMT)ps_point(x,y);
  else if(PltFmtFlag==SVGFMT)svg_point(x,y);
  else xpp_ui.draw_point(x,y);
}

void line(int x1, int y1, int x2, int y2)
{
  /* plintf("l %d %d %d %d \n",x1,y1,x2,y2); */
  if(PltFmtFlag==PSFMT)ps_line(x1,y1,x2,y2);
  else if(PltFmtFlag==SVGFMT)svg_line(x1,y1,x2,y2);
  else xpp_ui.draw_line(x1,y1,x2,y2);
}
/* draw a little filled circle */

void bead(int x1, int y1)
{
 if(PltFmtFlag==PSFMT)ps_bead(x1,y1);
 else if(PltFmtFlag==SVGFMT)svg_bead(x1,y1);
 else xpp_ui.draw_bead(x1,y1);
}

void frect(int x1, int y1, int w, int h)
{
  if(PltFmtFlag==PSFMT)ps_frect(x1,y1,w,h);
  else if(PltFmtFlag==SVGFMT)svg_frect(x1,y1,w,h);
  else xpp_ui.draw_frect(x1,y1,w,h);
}

void put_text(int x, int y, const char *str)
{
  if(PltFmtFlag==PSFMT)ps_text(x,y,str);
  else if(PltFmtFlag==SVGFMT)svg_text(x,y,str);
  else xpp_ui.draw_text(x,y,str);
}


void init_x11()
{
 get_draw_area();
}

void init_ps()
{
  if(!PS_Port){
 XDMax=7200;
 YDMax=5040;
 VTic=63;
 HTic=63;
 VChar=140;
 HChar=84;
 DLeft=12*HChar;
 DRight=XDMax-3*HChar-HTic;
 DTop=YDMax-1-VChar*7/2;
 DBottom=VChar*5/2+1;
  }
  else
    {
 YDMax=7200;
 XDMax=5040;
 VTic=63;
 HTic=63;
 VChar=140;
 HChar=84;
 DLeft=12*HChar;
 DRight=XDMax-3*HChar-HTic;
 DTop=YDMax-1-VChar*7/2;
 DBottom=VChar*5/2+1;


    }

}

void init_svg()
{
  XDMax=640;
  YDMax=400;
  VTic=9;
  HTic=9;
  VChar=20;
  HChar=12;
  DLeft=12*HChar;
  DRight=XDMax-3*HChar-HTic;
  DBottom=YDMax-1-VChar*7/2;
  DTop=VChar*5/2+1;
}

void set_linestyle(int ls)
{
  if(PltFmtFlag==PSFMT)ps_linetype(ls);
  else if(PltFmtFlag==SVGFMT)svg_linetype(ls);
  else xpp_ui.draw_linestyle(ls);
}

  
      
      
    
void scale_dxdy(float x, float y, double *i, double *j)
{
  float dx=(DRight-DLeft)/(XMax-XMin);
  float dy=(DTop-DBottom)/(YMax-YMin);
  *i=x*dx;
  *j=y*dy;
}  
     
void scale_to_screen(float x, float y, int *i, int *j)  /* not really the screen!  */
{
  float dx=(DRight-DLeft)/(XMax-XMin);
  float dy=(DTop-DBottom)/(YMax-YMin);
  *i=(int)((x-XMin)*dx)+DLeft;
  *j=(int)((y-YMin)*dy)+DBottom;
}

void scale_to_real(int i, int j, float *x, float *y)  /* Not needed except for X */
{
  int i1,j1;
  float x1,y1;
  get_draw_area();
  i1=i-DLeft;
  j1=j-DBottom;
  x1=(float)i1;
  y1=(float)j1;
  *x=(plot_windows.current->xhi-plot_windows.current->xlo)*x1/((float)(DRight-DLeft))+plot_windows.current->xlo;
  *y=(plot_windows.current->yhi-plot_windows.current->ylo)*y1/((float)(DTop-DBottom))+plot_windows.current->ylo;
  
 }


void reset_all_line_type()
{
	int j,k;
	for(j=0;j<MAXPOP;j++)
	{
		for(k=0;k<MAXPERPLOT;k++)
		{
			plot_windows.graph[j].line[k]=START_LINE_TYPE;
		}
	}

}


void init_all_graph()
{
 int i;
 for(i=0;i<MAXPOP;i++)
 init_graph(i);
 plot_windows.current=&plot_windows.graph[0];
 /*set_extra_graphs();*/
 set_normal_scale();

 
}

void set_extra_graphs()
{
  int i;
  if(NPltV<2)return;
  if(NPltV>8){
    NPltV=8;
  }
  if(MultiWin==0){
    plot_windows.current->nvars=NPltV;
    for(i=1;i<NPltV;i++){
      plot_windows.current->xv[i]=IX_PLT[i+1];
    plot_windows.current->yv[i]=IY_PLT[i+1];
    plot_windows.current->zv[i]=IZ_PLT[i+1];
    plot_windows.current->color[i]=i;
    }
    return;
  }
  if(program.interactive){
  for(i=1;i<NPltV;i++){
    create_a_pop();
    plot_windows.graph[i].xv[0]=IX_PLT[i+1];
    plot_windows.graph[i].yv[0]=IY_PLT[i+1];
    plot_windows.graph[i].zv[0]=IZ_PLT[i+1]; /* irrelevant probably */
    plot_windows.graph[i].grtype=0; /* force 2D */
    plot_windows.graph[i].xlo=X_LO[i+1];
    plot_windows.graph[i].xhi=X_HI[i+1];
    plot_windows.graph[i].ylo=Y_LO[i+1];
    plot_windows.graph[i].yhi=Y_HI[i+1];
    /*  printf(" %g %g %g %g \n",X_LO[i+1],X_HI[i+1],Y_LO[i+1],Y_HI[i+1]); */
  }
  set_active_windows();
  make_active(0,1); 
  }
}

void reset_graph()
{
  if(AXES>=5)
    PLOT_3D=1;
  else
    PLOT_3D=0;
  plot_windows.current->xv[0]=IXPLT;
  plot_windows.current->yv[0]=IYPLT;
  plot_windows.current->zv[0]=IZPLT;
   plot_windows.current->xmax=x_3d[1];
    plot_windows.current->ymax=y_3d[1];
    plot_windows.current->zmax=z_3d[1];
    plot_windows.current->xbar=.5*(x_3d[1]+x_3d[0]);
    plot_windows.current->ybar=.5*(y_3d[1]+y_3d[0]);
    plot_windows.current->zbar=.5*(z_3d[1]+z_3d[0]);
    plot_windows.current->dx=2./(x_3d[1]-x_3d[0]);
    plot_windows.current->dy=2./(y_3d[1]-y_3d[0]);
    plot_windows.current->dz=2./(z_3d[1]-z_3d[0]);
    plot_windows.current->xmin=x_3d[0];
    plot_windows.current->ymin=y_3d[0];
    plot_windows.current->zmin=z_3d[0];
    plot_windows.current->xlo=MY_XLO;
    plot_windows.current->ylo=MY_YLO;
    plot_windows.current->xhi=MY_XHI;
    plot_windows.current->yhi=MY_YHI;
    plot_windows.current->grtype=AXES;
    check_windows();
    set_normal_scale();
    xpp_ui.redraw_graph();
}


void get_graph()
{
 x_3d[0]=plot_windows.current->xmin;
 x_3d[1]=plot_windows.current->xmax;
y_3d[0]=plot_windows.current->ymin;
 y_3d[1]=plot_windows.current->ymax;
z_3d[0]=plot_windows.current->zmin;
 z_3d[1]=plot_windows.current->zmax;
MY_XLO=plot_windows.current->xlo;
MY_YLO=plot_windows.current->ylo;
MY_XHI=plot_windows.current->xhi;
MY_YHI=plot_windows.current->yhi;
IXPLT=plot_windows.current->xv[0];
IYPLT=plot_windows.current->yv[0];
IZPLT=plot_windows.current->zv[0];
PLOT_3D=plot_windows.current->ThreeDFlag;
if(PLOT_3D)
  AXES=5;
else
  AXES=0;
AXES=plot_windows.current->grtype;  
}

void init_graph(int i)
{
 int j,k;
 if(AXES<=3)AXES=0;
 for(j=0;j<3;j++)
  for(k=0;k<3;k++)
        if(k==j)plot_windows.graph[i].rm[k][j]=1.0;
	else plot_windows.graph[i].rm[k][j]=0.0;
 plot_windows.graph[i].nvars=1;
  for(j=0;j<MAXPERPLOT;j++){
        plot_windows.graph[i].xv[j]=IXPLT;
	plot_windows.graph[i].yv[j]=IYPLT;
	plot_windows.graph[i].zv[j]=IZPLT;
        plot_windows.graph[i].line[j]=START_LINE_TYPE;
	plot_windows.graph[i].color[j]=0;
        }
     
    /*sprintf(graph[i].xlabel,"");
    sprintf(graph[i].ylabel,"");
    sprintf(graph[i].zlabel,"");
    */
    plot_windows.graph[i].xlabel[0]='\0';
    plot_windows.graph[i].ylabel[0]='\0';
    plot_windows.graph[i].zlabel[0]='\0';
    
    plot_windows.graph[i].Use=0;
    plot_windows.graph[i].state=0;
    plot_windows.graph[i].Restore=1;
    plot_windows.graph[i].Nullrestore=0;
    plot_windows.graph[i].ZPlane=-1000.0;
    plot_windows.graph[i].ZView=1000.0;
    plot_windows.graph[i].PerspFlag=0;
    plot_windows.graph[i].ThreeDFlag=PLOT_3D;
    plot_windows.graph[i].TimeFlag=TIMPLOT;
    plot_windows.graph[i].ColorFlag=0;
    plot_windows.graph[i].grtype=AXES;
    plot_windows.graph[i].color_scale=1.0;
    plot_windows.graph[i].min_scale=0.0;
    XPP_STRCPY(plot_windows.graph[i].gr_info,"");
    plot_windows.graph[i].xmax=x_3d[1];
    plot_windows.graph[i].ymax=y_3d[1];
    plot_windows.graph[i].zmax=z_3d[1];
    plot_windows.graph[i].xbar=.5*(x_3d[1]+x_3d[0]);
    plot_windows.graph[i].ybar=.5*(y_3d[1]+y_3d[0]);
    plot_windows.graph[i].zbar=.5*(z_3d[1]+z_3d[0]);
    plot_windows.graph[i].dx=2./(x_3d[1]-x_3d[0]);
    plot_windows.graph[i].dy=2./(y_3d[1]-y_3d[0]);
    plot_windows.graph[i].dz=2./(z_3d[1]-z_3d[0]);
    plot_windows.graph[i].xmin=x_3d[0];
    plot_windows.graph[i].ymin=y_3d[0];
    plot_windows.graph[i].zmin=z_3d[0];
    plot_windows.graph[i].xorg=0.0;
    plot_windows.graph[i].yorg=0.0;
    plot_windows.graph[i].yorg=0.0;
    plot_windows.graph[i].xorgflag=1;
    plot_windows.graph[i].yorgflag=1;
    plot_windows.graph[i].zorgflag=1;
    plot_windows.graph[i].Theta=THETA0;
    plot_windows.graph[i].Phi=PHI0;
    plot_windows.graph[i].xshft=0;
    plot_windows.graph[i].yshft=0;
    plot_windows.graph[i].zshft=0;
    plot_windows.graph[i].xlo=MY_XLO;
    plot_windows.graph[i].ylo=MY_YLO;
    plot_windows.graph[i].oldxlo=MY_XLO;
    plot_windows.graph[i].oldylo=MY_YLO;
    plot_windows.graph[i].xhi=MY_XHI;
    plot_windows.graph[i].yhi=MY_YHI;
    plot_windows.graph[i].oldxhi=MY_XHI;
    plot_windows.graph[i].oldyhi=MY_YHI;
    plot_windows.current=&plot_windows.graph[i];
    make_rot(THETA0,PHI0);
    
  }



void copy_graph(int i, int l)  /*  Graph[i]=Graph[l]  */
{
 int j,k;
 plot_windows.graph[i].Use=plot_windows.graph[l].Use;
 plot_windows.graph[i].Restore=plot_windows.graph[l].Restore;
 plot_windows.graph[i].Nullrestore=plot_windows.graph[l].Nullrestore;
 for(j=0;j<3;j++)
  for(k=0;k<3;k++)
        plot_windows.graph[i].rm[k][j]=plot_windows.graph[l].rm[k][j];
 plot_windows.graph[i].nvars=plot_windows.graph[l].nvars;
  for(j=0;j<MAXPERPLOT;j++){
        plot_windows.graph[i].xv[j]=plot_windows.graph[l].xv[j];
	plot_windows.graph[i].yv[j]=plot_windows.graph[l].yv[j];
	plot_windows.graph[i].zv[j]=plot_windows.graph[l].zv[j];
        plot_windows.graph[i].line[j]=plot_windows.graph[l].line[j];
	plot_windows.graph[i].color[j]=plot_windows.graph[l].color[j];
        }

    plot_windows.graph[i].ZPlane=plot_windows.graph[l].ZPlane;
    plot_windows.graph[i].ZView=plot_windows.graph[l].ZView;
    plot_windows.graph[i].PerspFlag=plot_windows.graph[l].PerspFlag;
    plot_windows.graph[i].ThreeDFlag=plot_windows.graph[l].ThreeDFlag;
    plot_windows.graph[i].TimeFlag=plot_windows.graph[l].TimeFlag;
    plot_windows.graph[i].ColorFlag=plot_windows.graph[l].ColorFlag;
    plot_windows.graph[i].grtype=plot_windows.graph[l].grtype;
    plot_windows.graph[i].color_scale=plot_windows.graph[l].color_scale;
    plot_windows.graph[i].min_scale=plot_windows.graph[l].min_scale;

    plot_windows.graph[i].xmax=plot_windows.graph[l].xmax;
    plot_windows.graph[i].xmin=plot_windows.graph[l].xmin;
    plot_windows.graph[i].ymax=plot_windows.graph[l].ymax;
    plot_windows.graph[i].ymin=plot_windows.graph[l].ymin;
    plot_windows.graph[i].zmax=plot_windows.graph[l].zmax;
    plot_windows.graph[i].zmin=plot_windows.graph[l].zmin;
    plot_windows.graph[i].xbar=plot_windows.graph[l].xbar;
    plot_windows.graph[i].dx  =plot_windows.graph[l].dx  ;
    plot_windows.graph[i].ybar=plot_windows.graph[l].ybar;
    plot_windows.graph[i].dy  =plot_windows.graph[l].dy  ;
    plot_windows.graph[i].zbar=plot_windows.graph[l].zbar;
    plot_windows.graph[i].dz  =plot_windows.graph[l].dz  ;

    plot_windows.graph[i].Theta=plot_windows.graph[l].Theta;
    plot_windows.graph[i].Phi=plot_windows.graph[l].Phi;
    plot_windows.graph[i].xshft=plot_windows.graph[l].xshft;
    plot_windows.graph[i].yshft=plot_windows.graph[l].yshft;
    plot_windows.graph[i].zshft=plot_windows.graph[l].zshft;
    plot_windows.graph[i].xlo=plot_windows.graph[l].xlo;
    plot_windows.graph[i].ylo=plot_windows.graph[l].ylo;
    plot_windows.graph[i].oldxlo=plot_windows.graph[l].oldxlo;
    plot_windows.graph[i].oldylo=plot_windows.graph[l].oldylo;
    plot_windows.graph[i].xhi=plot_windows.graph[l].xhi;
    plot_windows.graph[i].yhi=plot_windows.graph[l].yhi;
    plot_windows.graph[i].oldxhi=plot_windows.graph[l].oldxhi;
    plot_windows.graph[i].oldyhi=plot_windows.graph[l].oldyhi;
  }




void make_rot(double theta, double phi)
{
 double ct=cos(DEGTORAD*theta),st=sin(DEGTORAD*theta);
 double sp=sin(DEGTORAD*phi),cp=cos(DEGTORAD*phi);
 plot_windows.current->Theta=theta;
 plot_windows.current->Phi=phi;
 plot_windows.current->rm[0][0]=ct;
 plot_windows.current->rm[0][1]=st;
 plot_windows.current->rm[0][2]=0.0;
 plot_windows.current->rm[1][0]=-cp*st;
 plot_windows.current->rm[1][1]=cp*ct;
 plot_windows.current->rm[1][2]=sp;
 plot_windows.current->rm[2][0]=st*sp;
 plot_windows.current->rm[2][1]=-sp*ct;
 plot_windows.current->rm[2][2]=cp;
}

void scale3d(float x, float y, float z, float *xp, float *yp, float *zp)
{
 *xp=(x-plot_windows.current->xbar)*plot_windows.current->dx;
 *yp=(y-plot_windows.current->ybar)*plot_windows.current->dy;
 *zp=(z-plot_windows.current->zbar)*plot_windows.current->dz;
}


int threedproj(float x2p, float y2p, float z2p, float *xp, float *yp)
{
  float x1p,y1p,z1p,s;
 /*  if(fabs(x2p)>1||fabs(y2p)>1||fabs(z2p)>1)return(0); */
 rot_3dvec(x2p,y2p,z2p,&x1p,&y1p,&z1p);

 if(plot_windows.current->PerspFlag==0){
 *xp=x1p;
 *yp=y1p;
  return(1);
 }
  if((z1p>=(float)(plot_windows.current->ZView))||(z1p<(float)(plot_windows.current->ZPlane)))return(0);
  s=(float)(plot_windows.current->ZView-plot_windows.current->ZPlane)/((float)(plot_windows.current->ZView)-z1p);
  x1p=s*x1p;
  y1p=s*y1p;
  *xp=x1p;
 *yp=y1p;
  return(1);
}


void text3d(float x, float y, float z, const char *s)
{
 float xp,yp;
if(threedproj(x,y,z,&xp,&yp)) text_abs(xp,yp,s);
}





int threed_proj(float x, float y, float z, float *xp, float *yp)
{
  float x1p,y1p,z1p,s;
 float x2p,y2p,z2p;
 scale3d(x,y,z,&x2p,&y2p,&z2p);  /* scale to a cube  */
 /* if(fabs(x2p)>1||fabs(y2p)>1||fabs(z2p)>1)return(0); */
 rot_3dvec(x2p,y2p,z2p,&x1p,&y1p,&z1p);

 if(plot_windows.current->PerspFlag==0){
 *xp=x1p;
 *yp=y1p;
  return(1);
 }
  if((z1p>=(float)(plot_windows.current->ZView))||(z1p<(float)(plot_windows.current->ZPlane)))return(0);
  s=(float)(plot_windows.current->ZView-plot_windows.current->ZPlane)/((float)(plot_windows.current->ZView)-z1p);
  x1p=s*x1p;
  y1p=s*y1p;
  *xp=x1p;
 *yp=y1p;
  return(1);
}

void point_3d(float x, float y, float z)
{
 float xp,yp;
 if(threed_proj(x,y,z,&xp,&yp))point_abs(xp,yp);
}

void line3dn(float xs1, float ys1, float zs1, float xsp1, float ysp1, float zsp1)  /* unscaled version  unclipped   */
{
 float xs,ys,zs;
 float xsp,ysp,zsp;
 rot_3dvec(xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(plot_windows.current->PerspFlag)pers_line(xs,ys,zs,xsp,ysp,zsp);
 else
     line_nabs(xs,ys,xsp,ysp);
 }




void line3d(float x01, float y01, float z01, float x02, float y02, float z02)  /* unscaled version     */
{
 float xs,ys,zs;
 float xs1,ys1,zs1;
 float xsp,ysp,zsp;
 float xsp1,ysp1,zsp1;
if(!clip3d(x01,y01,z01,x02,y02,z02,&xs1,&ys1,&zs1,&xsp1,&ysp1,&zsp1))return;
 rot_3dvec(xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(plot_windows.current->PerspFlag)pers_line(xs,ys,zs,xsp,ysp,zsp);
 else
     line_abs(xs,ys,xsp,ysp);
 }




void line_3d(float x, float y, float z, float xp, float yp, float zp)
{
 float xs,ys,zs;
float xs1,ys1,zs1;
 float xsp,ysp,zsp;
 float xsp1,ysp1,zsp1;
 float x01,x02,y01,y02,z01,z02;
 scale3d(x,y,z,&x01,&y01,&z01);          /* scale to a cube  */
 scale3d(xp,yp,zp,&x02,&y02,&z02);
 if(!clip3d(x01,y01,z01,x02,y02,z02,&xs1,&ys1,&zs1,&xsp1,&ysp1,&zsp1))return;
 rot_3dvec(xs1,ys1,zs1,&xs,&ys,&zs);   /* rotate the line */
 rot_3dvec(xsp1,ysp1,zsp1,&xsp,&ysp,&zsp);
 if(plot_windows.current->PerspFlag)pers_line(xs,ys,zs,xsp,ysp,zsp);
 else
     line_abs(xs,ys,xsp,ysp);
 }

void pers_line(float x, float y, float z, float xp, float yp, float zp)
{
 float Zv=(float)plot_windows.current->ZView,Zp=(float)plot_windows.current->ZPlane;
 float d=Zv-Zp,s;
 float eps=.005*d;

 if(((zp>=Zv)&&(z>=Zv))||((zp<Zp)&&(z<Zp)))return;
 if(zp>Zv)
 {
  s=(Zv-eps-z)/(zp-z);
  zp=Zv-eps;
  yp=y+s*(yp-y);
  xp=x+s*(xp-x);
 }
 if(z>Zv)
 {
  s=(Zv-eps-zp)/(z-zp);
  z=Zv-eps;
  y=yp+s*(y-yp);
  x=xp+s*(x-xp);
 }
 if(zp<Zp)
 {
  s=(Zp-z)/(zp-z);
  zp=Zp;
  yp=y+s*(yp-y);
  xp=x+s*(xp-x);
 }
 if(z<Zp)
 {
  s=(Zp-zp)/(z-zp);
  z=Zp;
  y=yp+s*(y-yp);
  x=xp+s*(x-xp);
 }
 s=d/(Zv-zp);
 xp=xp*s;
 yp=yp*s;
 s=d/(Zv-z);
 x=s*x;
 y=s*y;
 line_abs(x,y,xp,yp);
}





void rot_3dvec(float x, float y, float z, float *xp, float *yp, float *zp)
{
 int i,j;
 double vt[3],vnew[3];
 vt[0]=x;
 vt[1]=y;
 vt[2]=z;

 for(i=0;i<3;i++){
	vnew[i]=0.0;
	for(j=0;j<3;j++)vnew[i]=vnew[i]+plot_windows.current->rm[i][j]*vt[j];
	}
	*xp=vnew[0];
	*yp=vnew[1];
	*zp=vnew[2];

}








void point_abs(float x1, float y1)
{
  int xp,yp;

  float x_left=XMin;
  float x_right=XMax;
  float y_top=YMax;
  float y_bottom=YMin;
   if((x1>x_right)||(x1<x_left)||(y1>y_top)||(y1<y_bottom))return; 
  scale_to_screen(x1,y1,&xp,&yp);
  point(xp,yp);
}

void line_nabs(float x1_out, float y1_out, float x2_out, float y2_out)
{
  
  
  

  int xp1,yp1,xp2,yp2;

    scale_to_screen(x1_out,y1_out,&xp1,&yp1);
    scale_to_screen(x2_out,y2_out,&xp2,&yp2);
    line(xp1,yp1,xp2,yp2);
  }

void bead_abs(float x1, float y1)
{
  int i1,j1;
  float x_left=XMin;
  float x_right=XMax;
  float y_top=YMax;
  float y_bottom=YMin;
   if((x1>x_right)||(x1<x_left)||(y1>y_top)||(y1<y_bottom))return; 
  scale_to_screen(x1,y1,&i1,&j1);
  bead(i1,j1);
}

void frect_abs(float x1, float y1, float w, float h)
{
 int i1,i2,j1,j2;
 int ih,iw;
 float x2=x1+w;
 float y2=y1+h;
 scale_to_screen(x1,y1,&i1,&j1);
 scale_to_screen(x2,y2,&i2,&j2);
 iw=abs(i2-i1);
 ih=abs(j2-j1);
 frect(i1,j1,iw+1,ih+1);
}

void line_abs(float x1, float y1, float x2, float y2)
{
  float x1_out,y1_out,x2_out,y2_out;



  int xp1,yp1,xp2,yp2;
  if(clip(x1,x2,y1,y2,&x1_out,&y1_out,&x2_out,&y2_out)){
    scale_to_screen(x1_out,y1_out,&xp1,&yp1);
    scale_to_screen(x2_out,y2_out,&xp2,&yp2);
    line(xp1,yp1,xp2,yp2);
  }
}

void text_abs(float x, float y, const char *text)
{
 int xp,yp;
 scale_to_screen(x,y,&xp,&yp);
 put_text(xp,yp,text);
}

void fillintext(const char *old,char *newname)
{
 int i,l=strlen(old);
 int j,m,k,ans;
 char name[256],c,c2;
 double z;
 char val[25];
 i=0;
 j=0;
 while(1){
   c=old[i];

   if(c=='\\'){
     c2=old[i+1];
     if(c2!='{')goto na;
     if(c2=='{'){
       m=0;
       i=i+2;
       while(1){
	 c2=old[i];
	 if(c2=='}'){
	   name[m]=0;
	   ans=do_calc(name,&z);
	   if(ans!=-1){
	     XPP_SPRINTF(val,"%g",z);

	     for(k=0;k<(int)strlen(val);k++){
	       newname[j]=val[k];
	       j++;
	     }

	     break;
	   }
	   else {
	     newname[j]='?';
	     j++;
	   }
	 }
	 else {
	   name[m]=c2;
	   m++;
	 }
	 i++;
	 if(i>=l){ /* oops - end of string */
	   newname[j]='?';
	   newname[j+1]=0;
	   return;
	 }
       }
     } /* ok - we have found matching and are done */
     goto nc; /* sometimes its just easier to use the !#$$# goto */
   }
 na:
   newname[j]=c;
   j++;
 nc:  /* normal characters */
   i++;
   if(i>=l)
     break;
 }
 newname[j]=0;
 return;
}

void fancy_text_abs(float x, float y, const char *old, int size, int font)
{
  int xp,yp;
  char text[256];
  scale_to_screen(x,y,&xp,&yp);
  fillintext(old,text);
  if(PltFmtFlag==PSFMT)special_put_text_ps(xp,yp,text,size); 
  else if(PltFmtFlag==SVGFMT)special_put_text_svg(xp,yp,text,size);
  else xpp_ui.draw_special_text(xp,yp,text,size);
/* fancy_put_text_x11(xp,yp,text,size,font); */
    
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
int clip(float x1, float x2, float y1, float y2, float *x1_out, float *y1_out, float *x2_out, float *y2_out)
{
   int istack,ix1,ix2,iy1,iy2,isum,iflag;
   float  wh,xhat,yhat,wv;
   float x_left=XMin;
   float x_right=XMax;
   float y_top=YMax;
   float y_bottom=YMin;
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



void eq_symb(double *x, int type)
{

  float dx=6.0*(float)(plot_windows.current->xhi-plot_windows.current->xlo)*SYMSIZE;
  float dy=6.0*(float)(plot_windows.current->yhi-plot_windows.current->ylo)*SYMSIZE;
 int ix=plot_windows.current->xv[0]-1,iy=plot_windows.current->yv[0]-1,iz=plot_windows.current->zv[0]-1;
 if(!program.interactive)return;
  if(plot_windows.current->TimeFlag)return;
  set_color(0); 
  if(plot_windows.current->ThreeDFlag)
  {
   dx=6.0*SYMSIZE/plot_windows.current->dx;
   dy=6.0*SYMSIZE/plot_windows.current->dy;
   line_3d((float)x[ix]+dx,(float)x[iy],(float)x[iz],
           (float)x[ix]-dx,(float)x[iy],(float)x[iz]);
   line_3d((float)x[ix],(float)x[iy]+dy,(float)x[iz],
           (float)x[ix],(float)x[iy]-dy,(float)x[iz]);
  return;
  }
  draw_symbol((float)x[ix],(float)x[iy],SYMSIZE,type);
  point_abs((float)x[ix],(float)x[iy]);
  if(ix>=0&&iy>=0)marks_data_equilibrium(x[ix],x[iy],type); /* the mark as data */
 
}

void draw_symbol(float x, float y, float size, int my_symb)
{
 float dx=(float)(plot_windows.current->xhi-plot_windows.current->xlo)*size;
 float dy=(float)(plot_windows.current->yhi-plot_windows.current->ylo)*size;
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
    if(pen!=0) line_abs(x1,y1,x2,y2);
    x1=x2;
    y1=y2;
    ind++;
   }

}






















	
   




