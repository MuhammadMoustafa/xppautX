/* Print arrayplot: the array plot as a PostScript picture of shaded bars
   with its colour scale, titles and ranges. tests/golden/lecar_array.ps
   guards the output byte for byte (tools/goldencheck.py). */
#include <cmath>
#include <string>
#include <string_view>
#include "array_print.h"
#include "xpp_io.h"

namespace xpp {


#define GREYSCALE -1
#define REDBLUE  0
#define ROYGBIV  1
#define PERIODIC 2

namespace {

struct DevScale {
  float xmin,xmax,ymin,ymax;
  float xscale,yscale,xoff,yoff;
  float tx,ty,angle,slant;  /* text attributes   */
  float linecol;
  int linewid;
};

xpp::Writer plot_writer; /* the file being written, in place at ps_close */
DevScale ps_scale;

void ps_convert(float x, float y, float *xs, float *ys)
{
  *xs=(x-ps_scale.xmin)*ps_scale.xscale+ps_scale.xoff;
  *ys=(y-ps_scale.ymin)*ps_scale.yscale+ps_scale.yoff;
}

void ps_setline(float fill, int thick)
{
  plot_writer.print("{:f} G\n {} setlinewidth \n",fill,thick);
  ps_scale.linewid=thick;
  ps_scale.linecol=fill;
}

void ps_set_text(float angle, float slant, float x_size, float y_size)
{
  ps_scale.tx=x_size*5.0;
  ps_scale.ty=y_size*5.0;
  ps_scale.angle=angle;
  ps_scale.slant=slant;
}

void ps_rect(float x, float y, float wid, float len)
{
  float x1,y1,x2,y2;
  ps_convert(x,y,&x1,&y1);
  ps_convert(x+wid,y+len,&x2,&y2);
  const int i1=static_cast<int>(x1),j1=static_cast<int>(y1),i2=static_cast<int>(x2),j2=static_cast<int>(y2);
  plot_writer.print("{} {} m \n {} {} l \n {} {} l \n {} {} l \n {} {} l \n S \n",
                    i1,j1,i2,j1,i2,j2,i1,j2,i1,j1);
}

/* a bar filled with colour (a PostScript colour command), and outlined
   in black when flag */
void ps_bar(std::string_view colour, float x, float y, float wid, float len, int flag)
{
  float x1,y1,x2,y2;
  plot_writer.print("{}\n",colour);
  ps_convert(x,y,&x1,&y1);
  ps_convert(x+wid,y+len,&x2,&y2);
  const int i1=static_cast<int>(x1),j1=static_cast<int>(y1),i2=static_cast<int>(x2),j2=static_cast<int>(y2);
  plot_writer.print("{} {} m \n {} {} l \n {} {} l \n {} {} l \n FS\n",i1,j1,i2,j1,i2,j2,i1,j2);
  if(flag){
    plot_writer.print("0 G\n");
    ps_rect(x,y,wid,len);
  }
}

/* the colour command of fill (0..1) in the scale type */
std::string ps_colour(float fill, int type)
{
  if(type==GREYSCALE)
    return xpp::format("{:f} G",fill);
  if(type==PERIODIC)
    return xpp::format("{:f} 1.0 1.0 HSB",fill);
  float r=0.0,g=0.0,b=0.0;
  if(fill<0.0)fill=0.0;
  if(fill>1.0)fill=1.0;
  switch(type)
    {
    case REDBLUE:
      fill=1.-fill;
      b=static_cast<float>(sqrt(static_cast<double>(1.0-fill*fill)));
      r=static_cast<float>(sqrt(static_cast<double>(fill*(2.0-fill))));
      break;
    case ROYGBIV:
      if(fill>.4999)r=0.0;
      else r=static_cast<float>(sqrt(static_cast<float>(1.-4*fill*fill)));
      g=static_cast<float>(2)*sqrt(static_cast<double>(fill)*(1.-fill));

      if(fill<.5001)b=0.0;
      else b=static_cast<float>(sqrt(static_cast<float>(4*(fill-.5)*(1.5-fill))));
      break;
    }
  return xpp::format("{:f} {:f} {:f} RGB",r,g,b);
}

void ps_text2(std::string_view str, float xr, float yr, int icent)
{
  double slant=.0174532*ps_scale.slant;
  float x,y;
  float sizex=ps_scale.tx,sizey=ps_scale.ty,rot=ps_scale.angle;
  double a=sizex*cos(slant),b=sizey*sin(slant),
    c=-sizex*sin(slant),d=sizey*cos(slant);
  ps_convert(xr,yr,&x,&y);
  plot_writer.print("{} {} m\n",static_cast<int>(x),static_cast<int>(y));
  plot_writer.print("gsave \n {:f} rotate \n",rot);
  plot_writer.print("basefont [{:.4f} {:.4f} {:.4f} {:.4f} 0 0] makefont setfont\n",a,b,c,d);
  switch(icent){
  case 0:
    plot_writer.print("( {} ) show \n grestore\n",str);
    break;
  case 1:  /* centered */
    plot_writer.print("({}) dup stringwidth pop -2 div 0 rmoveto show \n grestore\n",str);
    break;
  case 2: /* left edge */
    plot_writer.print("({}) dup stringwidth pop neg 0 rmoveto show \n grestore\n",str);
    break;
  case 3: /* right edge */
    plot_writer.print("({}) dup stringwidth pop  0 rmoveto show \n grestore\n",str);
    break;
  }
}

void ps_begin(double xlo, double ylo, double xhi, double yhi, float sx, float sy)
{
  float x0,y0,x1,y1;
  ps_scale.xmin=xlo;
  ps_scale.ymin=ylo;
  ps_scale.ymax=yhi;
  ps_scale.xmax=xhi;
  ps_scale.xoff=300;
  ps_scale.yoff=300;
  ps_scale.angle=-90.;
  ps_scale.xscale=1800.*sx*.2/(xhi-xlo);
  ps_scale.yscale=1800.*sy*.2/(yhi-ylo);

  ps_set_text(-90.,0.0,18.0,18.0);
  ps_convert(xlo,ylo,&x0,&y0);
  ps_convert(xhi,yhi,&x1,&y1);
  plot_writer.print("%!\n");
  plot_writer.print("%%BoundingBox:  {:g} {:g} {:g} {:g}\n",.2*x0,.2*y0,.2*x1,.2*y1);
  plot_writer.print("{}",
    "20 dict begin\n"
    "gsave\n"
    "/m {moveto} def\n"
    "/l {lineto} def\n"
    "/Cshow { currentpoint stroke moveto\n"
    "  dup stringwidth pop -2 div vshift rmoveto show } def\n"
    "/Lshow { currentpoint stroke moveto\n"
    "  0 vshift rmoveto show } def\n"
    "/Rshow { currentpoint stroke moveto\n"
    "  dup stringwidth pop neg vshift rmoveto show } def\n"
    "/C {setrgbcolor} def\n"
    "/G {setgray} def\n"
    "/S {stroke} def\n"
    "/HSB {sethsbcolor} def\n"
    "/RGB {setrgbcolor} def\n"
    "/FS {fill stroke} def\n"
    "630 -20 translate\n"
    "90 rotate\n"
    ".2 .2 scale\n"
    "/basefont /Times-Roman findfont def\n");
  ps_setline(0.0,4);
}

void ps_replot(float **z, int col0, int row0, int nskip, int ncskip, int maxrow, int maxcol, int nacross, int ndown, double zmin, double zmax, int type)
{
  float dx=(ps_scale.xmax-ps_scale.xmin);
  float dy=(ps_scale.ymax-ps_scale.ymin);
  float xhi=.95*dx,yhi=.85*dy;
  float delx=.8*dx/static_cast<float>(ndown);
  float dely=.8*dy/static_cast<float>(nacross/ncskip);
  for(int i=0;i<nacross/ncskip;i++){
    int ib=col0+i*ncskip;
    if(ib>maxcol)return;
    for(int j=0;j<ndown;j++){
      int jb=row0+j*nskip;
      if(jb<maxrow&&jb>=0){
        float fill=(z[ib][jb]-zmin)/(zmax-zmin);
        if(fill<0.0)fill=0.0;
        if(fill>1.0)fill=1.0;
        fill=1-fill;
        float x=xhi-delx-j*delx;
        float y=yhi-dely-i*dely;
        ps_bar(ps_colour(fill,type),x,y,delx,dely,0);
      }
    }
  }
}

void ps_col_scale(double y0, double x0, double dy, double dx, int n, double zlo, double zhi, int type)
{
  float dz=1./static_cast<float>(n-1);

  for(int i=0;i<n;i++)
    ps_bar(ps_colour(1-static_cast<float>(i)*dz,type),x0,y0-(i+1)*dy,dx,dy,0);
  plot_writer.print("0 G\n");
  ps_text2(xpp::format("{:g}",zlo),x0+.5*dx,y0+.01*dx,2);
  ps_text2(xpp::format("{:g}",zhi),x0+.5*dx,y0-n*dy-dy/2,0);
}

void ps_boxit(double tlo, double thi, double jlo, double jhi, double zlo, double zhi, const char *sx, const char *sy, const char *sb, int type)
{
  int i=ps_scale.linewid;
  float z=ps_scale.linecol;
  float dx=ps_scale.xmax-ps_scale.xmin;
  float dy=ps_scale.ymax-ps_scale.ymin;
  float xlo=.15*dx,ylo=.05*dy,xhi=.95*dx,yhi=.85*dy;
  ps_setline(0.0,10);
  ps_rect(xlo,ylo,.8*dx,.8*dy);
  ps_setline(z,i);

  ps_text2(sx,xhi+.01*dx,.5*(yhi+ylo),1);
  ps_text2(sy,.5*(xhi+xlo),yhi+.01*dy,2);
  ps_text2(xpp::format("{:g}",tlo),xhi-.01*dx,yhi+.01*dy,2);
  ps_text2(xpp::format("{:g}",thi),xlo,yhi+.01*dy,2);
  ps_text2(xpp::format("{:g}",jlo),xhi+.01*dx,yhi,0);
  ps_text2(xpp::format("{:g}",jhi),xhi+.01*dx,ylo+.01,2);
  ps_col_scale(yhi-.15*dy,xlo-.1*dx,.025*dy,.05*dx,20,zlo,zhi,type);
  ps_text2(sb, xlo-.035*dx,.5*(yhi+ylo),1);
}

void ps_close()
{
  plot_writer.print("showpage\ngrestore\nend\n");
  plot_writer.commit();
}

} // namespace

int array_print(const char *filename, const char *xtitle, const char *ytitle, const char *bottom, int nacross, int ndown, int col0, int row0, int nskip, int ncskip, int maxrow, int maxcol, float **data, double zmin, double zmax, double tlo, double thi, int type)
{
  float xx=static_cast<float>(ndown);
  float yy=static_cast<float>(nacross/ncskip);
  plot_writer=xpp::Writer(filename);
  if(!plot_writer)
    return -1;
  ps_begin(0.0,0.0,xx,yy,10.,7.);
  ps_replot(data,col0,row0,nskip,ncskip,maxrow,maxcol,nacross,ndown,zmin,zmax,type);
  ps_boxit(tlo,thi,0.0,yy,zmin,zmax,xtitle,ytitle,bottom,type);
  ps_close();
  return 0;
}

} // namespace xpp
