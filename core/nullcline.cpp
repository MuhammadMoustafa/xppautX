/* Nullclines and direction fields (Nullcline, Dir.field/flow): the current
   pair of nullclines, the frozen ones, and the direction field's grid, drawn
   through graphics.cpp's primitives and recorded for the front end by
   phase_data.cpp. */
#include "nullcline.h"
#include "session.h"
#include "getvar.h"
#include "xpp_util.h"
#include "xpp_log.h"
#include "odesol2.h"
#include "numerics.h"
#include "my_rhs.h"
#include "browse.h"
#include "integrate.h"
#include "load_eqn.h"
#include "graf_par.h"
#include "phase_data.h"
#include "my_ps.h"
#include "my_svg.h"

#include "expr.h"
#include "xpp_ui.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "graphics.h"
#include "menudrive.h"
#include "xpp_batch.h"
#include "form_ode.h"
#include "model.h"

#define MAX_NULL 10000


constexpr int NullStyle=0; /* 1 is with little vertical/horizontal lines */

static int DF_IX=-1,DF_IY=-1;
static int DFIELD_TYPE=0;


/* load_eqn.cpp's @ colorvia= writes it through sizeof: kept an array */

namespace {

struct Pt {
  float x,y,z;
};

/* A frozen pair of nullclines: 4 floats (a segment) per point. */
struct FrozenCline {
  std::vector<float> xn,yn;
  int nmx=0,nmy=0;
  int n_ix=-5,n_iy=-5;
};

/* Range clines' dialog values */
struct {
  std::string rv;
  int nstep=0;
  double xlo=0,xhi=0;
} ncrange;

/* the current nullclines, as segments (4 floats each) */
std::vector<float> x_null,y_null;
int num_x_n,num_y_n;
int null_ix,null_iy,WHICH_CRV;
/* the contour's two rows of the grid, and where new segments go */
std::vector<float> n_top,n_bot;
std::vector<float> *saver;
int num_index;

/* the frozen nullclines; started is Freeze's first use (start_ncline) */
std::vector<FrozenCline> frozen;
bool frozen_started=false;

/* Walks the frozen nullclines up to the first empty one (the original
   linked list's end marker, also where one with no points stops it). */
template <class F>
void for_each_frozen(F &&f)
{
  for(FrozenCline &z:frozen){
    if(z.nmx==0&&z.nmy==0)break;
    f(z);
  }
}

/* the frozen nullclines the current window shows (phase_data.h): those of
   its axes, as redraw_froz_cline draws them */
void note_frozen(xpp::Session &s)
{
  phase_data_frozen_begin(s.plot_windows);
  if(!frozen_started)return;
  for_each_frozen([&](FrozenCline &z){
    if(s.plot_windows.current->xv[0]==z.n_ix&&s.plot_windows.current->yv[0]==z.n_iy&&s.plot_windows.current->ThreeDFlag==0)
      phase_data_frozen(s.plot_windows,z.xn.data(),z.nmx,z.yn.data(),z.nmy);
  });
}

void start_ncline()
{
  frozen_started=true;
  frozen.clear();
  ncrange.xlo=0;
  ncrange.xhi=1;
  ncrange.nstep=10;
  ncrange.rv=" ";
}

void clear_froz_cline()
{
  frozen.clear();
}

void add_froz_cline(const float *xn, int nmx, int n_ix, const float *yn, int nmy, int n_iy)
{
  FrozenCline z;
  z.xn.assign(xn,xn+4*nmx);
  z.yn.assign(yn,yn+4*nmy);
  z.nmx=nmx;
  z.nmy=nmy;
  z.n_ix=n_ix;
  z.n_iy=n_iy;
  frozen.push_back(std::move(z));
}

void dump_clines(xpp::Writer &fp, const float *x, int nx, const float *y, int ny)  /* gnuplot format */
{
  fp.print("# X-nullcline\n");
  for(int i=0;i<nx-1;i++){
    fp.print("{:g} {:g} 1 \n",static_cast<double>(x[4*i]),static_cast<double>(x[4*i+1]));
    fp.print("{:g} {:g} 1 \n",static_cast<double>(x[4*i+2]),static_cast<double>(x[4*i+3]));
    fp.print("\n");
  }
  fp.print("\n# Y-nullcline\n");
  for(int i=0;i<ny-1;i++){
    fp.print("{:g} {:g} 2 \n",static_cast<double>(y[4*i]),static_cast<double>(y[4*i+1]));
    fp.print("{:g} {:g} 2 \n",static_cast<double>(y[4*i+2]),static_cast<double>(y[4*i+3]));
    fp.print("\n");
  }
}

void save_frozen_clines(const std::string &fn)
{
  if(!frozen_started)return;
  const char ch=static_cast<char>(TwoChoice("YES","NO","Save Frozen Clines?","yn"));
  if(ch=='n')return;
  int i=1;
  for(const FrozenCline &z:frozen){
    if(z.nmx==0&&z.nmy==0)return;
    xpp::Writer fp(xpp::format("{}.{}",fn,i).c_str());
    if(!fp){
      err_msg("Cant open file!");
      return;
    }
    dump_clines(fp,z.xn.data(),z.nmx,z.yn.data(),z.nmy);
    fp.commit();
    i++;
  }
}

void restor_null(xpp::Session &s, const float *v, int n, int d)  /* d=1 for x and 2 for y  */
{
  if (s.plot_file.plt_fmt_flag==SVGFMT)
    svg_write("<g>");
  for(int i=0;i<n;i++){
    const int i4=4*i;
    line_abs(s,v[i4],v[i4+1],v[i4+2],v[i4+3]);
    if(NullStyle==1){
      const float xm=.5*(v[i4]+v[i4+2]);
      const float ym=.5*(v[i4+1]+v[i4+3]);
      int x1,y1;
      scale_to_screen(s,xm,ym,&x1,&y1);
      switch(d){
      case 1:
        line(s,x1,y1-4,x1,y1+4);
        break;
      case 2:
        line(s,x1-4,y1,x1+4,y1);
        break;
      }
    }
  }
  if (s.plot_file.plt_fmt_flag==SVGFMT)
    svg_write("</g>");
}

void redraw_froz_cline(xpp::Session &s, int flag)
{
  const int col1=s.nullclines.x_null_color,col2=s.nullclines.y_null_color;
  if(!frozen_started)return;
  phase_data_frozen_begin(s.plot_windows);
  for_each_frozen([&](FrozenCline &z){
    if(s.plot_windows.current->xv[0]==z.n_ix&&s.plot_windows.current->yv[0]==z.n_iy
       &&s.plot_windows.current->ThreeDFlag==0){
      if(flag>0){
        waitasec(flag);
        clr_scrn(s);
      }
      set_linestyle(s,col1);
      restor_null(s,z.xn.data(),z.nmx,1);
      set_linestyle(s,col2);
      restor_null(s,z.yn.data(),z.nmy,2);
      phase_data_frozen(s.plot_windows,z.xn.data(),z.nmx,z.yn.data(),z.nmy);
      if(flag>0)
        FlushDisplay();
    }
  });
}

/* The current nullclines' storage: made once (NULL_HERE), the contour's
   two grid rows for this mesh every time. */
void null_storage(xpp::Session &s, int course)
{
  if(s.numerics.null_here==0){
    x_null.assign(4*MAX_NULL,0.0f);
    y_null.assign(4*MAX_NULL,0.0f);
    s.numerics.null_here=1;
  }
  n_top.assign(course+1,0.0f);
  n_bot.assign(course+1,0.0f);
}

void stor_null(float x1, float y1, float x2, float y2)
{
  if(num_index>=MAX_NULL)return;
  const int i=4*num_index;
  (*saver)[i]=x1;
  (*saver)[i+1]=y1;
  (*saver)[i+2]=x2;
  (*saver)[i+3]=y2;
  num_index++;
}

float fnull(xpp::Session &s, float x, float y)
{
  std::array<double,MAXODE> y1,ydot;
  for(int i=0;i<s.model().node;i++)y1[i]=s.last_ic[i];
  y1[null_ix-1]=static_cast<double>(x);
  y1[null_iy-1]=static_cast<double>(y);
  s.integrator.rhs(0.0,y1.data(),ydot.data(),s.model().node);
  return(static_cast<float>(ydot[WHICH_CRV-1]));
}

int interpolate(Pt p1, Pt p2, float z, float *x, float *y)
{
  if(p1.z==p2.z)return(0);
  const float scale=(z-p1.z)/(p2.z-p1.z);
  *x=p1.x+scale*(p2.x-p1.x);
  *y=p1.y+scale*(p2.y-p1.y);
  return(1);
}

void quad_contour(xpp::Session &s, Pt p1, Pt p2, Pt p3, Pt p4)
{
  std::array<float,4> x,y;
  int count=0;
  if(p1.z*p2.z<=0.0)
    if(interpolate(p1,p2,0.0,&x[count],&y[count]))count++;
  if(p2.z*p3.z<=0.0)
    if(interpolate(p3,p2,0.0,&x[count],&y[count]))count++;
  if(p3.z*p4.z<=0.0)
    if(interpolate(p3,p4,0.0,&x[count],&y[count]))count++;
  if(p1.z*p4.z<=0.0)
    if(interpolate(p1,p4,0.0,&x[count],&y[count]))count++;

  if(count==2){
    line_abs(s,x[0],y[0],x[1],y[1]);
    stor_null(x[0],y[0],x[1],y[1]);
  }
}

void do_cline(xpp::Session &s, int ngrid, float x1, float y1, float x2, float y2)
{
  const float dx=(x2-x1)/static_cast<float>(ngrid);
  const float dy=(y2-y1)/static_cast<float>(ngrid);
  const int nx=ngrid+1;
  const int ny=ngrid+1;
  std::array<Pt,4> p;

  float y=y2;
  for(int i=0;i<nx;i++){
    const float x=x1+i*dx;
    n_bot[i]=fnull(s,x,y);
  }

  for(int j=1;j<ny;j++){
    y=y2-j*dy;
    n_top[0]=n_bot[0];
    n_bot[0]=fnull(s,x1,y);
    for(int i=1;i<nx;i++){
      const float x=x1+i*dx;
      n_top[i]=n_bot[i];
      n_bot[i]=fnull(s,x,y);
      p[0].x=x-dx;
      p[0].y=y+dy;
      p[0].z=n_top[i-1];
      p[1].x=x;
      p[1].y=y+dy;
      p[1].z=n_top[i];
      p[3].x=x-dx;
      p[3].y=y;
      p[3].z=n_bot[i-1];
      p[2].x=x;
      p[2].y=y;
      p[2].z=n_bot[i];
      quad_contour(s,p[0],p[1],p[2],p[3]);
    }
  }
}

void new_nullcline(xpp::Session &s, int course, float xlo, float ylo, float xhi, float yhi, std::vector<float> &stor, int *npts)
{
  num_index=0;
  saver=&stor;
  do_cline(s,course,xlo,ylo,xhi,yhi);
  *npts=num_index;
}

void do_range_clines(xpp::Session &s)
{
  static const char *n[]={"*2Range parameter","Steps","Low","High"};
  std::array<std::string, 4> values;
  const int col1=s.nullclines.x_null_color,col2=s.nullclines.y_null_color;
  const int course=s.numerics.nmesh;
  values[0] = ncrange.rv;
  values[1] = xpp::format("{:d}", ncrange.nstep);
  values[2] = xpp::format("{:g}", ncrange.xlo);
  values[3] = xpp::format("{:g}", ncrange.xhi);
  static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
  const int status=do_string_box_of(4,1,"Range Clines",n,values,kinds);
  if(status==0)return;
  ncrange.rv=values[0];
  ncrange.nstep=std::atoi(values[1].c_str());
  ncrange.xlo=std::atof(values[2].c_str());
  ncrange.xhi=std::atof(values[3].c_str());
  if(ncrange.nstep<=0)return;
  const double dz=(ncrange.xhi-ncrange.xlo)/static_cast<double>(ncrange.nstep);
  if(dz<=0.0)return;
  double zold;
  get_val(s,ncrange.rv.c_str(),&zold);

  for(int i=s.model().node;i<s.model().node+s.model().nmarkov;i++)setvar(s,i+1+s.model().fix_var,s.last_ic[i]);
  const float xmin=static_cast<float>(s.plot_windows.current->xmin);
  const float xmax=static_cast<float>(s.plot_windows.current->xmax);
  const float y_tp=static_cast<float>(s.plot_windows.current->ymax);
  const float y_bot=static_cast<float>(s.plot_windows.current->ymin);
  null_ix=s.plot_windows.current->xv[0];
  null_iy=s.plot_windows.current->yv[0];

  for(int i=0;i<=ncrange.nstep;i++){
    const double z=static_cast<double>(i)*dz+ncrange.xlo;
    set_val(s,ncrange.rv.c_str(),z);
    null_storage(s,course);

    WHICH_CRV=null_ix;
    set_linestyle(s,col1);
    new_nullcline(s,course,xmin,y_bot,xmax,y_tp,x_null,&num_x_n);

    WHICH_CRV=null_iy;
    set_linestyle(s,col2);
    new_nullcline(s,course,xmin,y_bot,xmax,y_tp,y_null,&num_y_n);
    add_froz_cline(x_null.data(),num_x_n,null_ix,y_null.data(),num_y_n,null_iy);
  }
  set_val(s,ncrange.rv.c_str(),zold);
  phase_data_nullclines(s.plot_windows,x_null.data(),num_x_n,y_null.data(),num_y_n,null_ix,null_iy,col1,col2);
  note_frozen(s);
}

void get_max_dfield(xpp::Session &s, double *y, double *ydot, double u0, double v0, double du, double dv, int n, int inx, int iny, double *mdf)
{
  double dxp,dyp;
  *mdf=0.0;
  for(int i=0;i<=n;i++){
    y[inx]=u0+du*i;
    for(int j=0;j<=n;j++){
      y[iny]=v0+dv*j;
      s.integrator.rhs(0.0,y,ydot,s.model().node);
      extra(s,y,0.0,s.model().node,s.model().neq);
      scale_dxdy(s,ydot[inx],ydot[iny],&dxp,&dyp);
      const double amp=hypot(dxp,dyp);
      if(amp>*mdf)*mdf=amp;
    }
  }
}

/* The direction field's grid (DF_FLAG 1 or 4: arrows, 2: colored boxes),
   drawn and recorded for the front end; dump, when only computed
   (write_dfield), gets each arrow instead of the screen. */
void dfield_grid(xpp::Session &s, int grid, double u0, double v0, double du, double dv, double dz,
                 int inx, int iny, xpp::Writer *dump)
{
  std::array<double,MAXODE> y,ydot;
  std::array<float,MAXODE> v1,v2;
  double mdf,dxp,dyp;
  const bool suppress=dump!=nullptr;
  xpp::get_ic(s,2,y.data());
  get_max_dfield(s,y.data(),ydot.data(),u0,v0,du,dv,grid,inx,iny,&mdf);
  if(!suppress&&(s.nullclines.df_flag==1||s.nullclines.df_flag==4))
    phase_data_dfield_begin(s.plot_windows,grid+1,du,dv,DFIELD_TYPE==0,s.plot_windows.current->color[0]);
  if (s.plot_file.plt_fmt_flag==SVGFMT){
    s.nullclines.doing_dfield=1;
    svg_write("<g>");
  }
  for(int i=0;i<=grid;i++){
    y[inx]=u0+du*i;
    for(int j=0;j<=grid;j++){
      y[iny]=v0+dv*j;
      s.integrator.rhs(0.0,y.data(),ydot.data(),s.model().node);
      extra(s,y.data(),0.0,s.model().node,s.model().neq);
      if(s.plot_windows.current->ColorFlag||s.nullclines.df_flag==2){
        v1[0]=0.0;
        v2[0]=0.0;
        for(int k=0;k<s.model().neq;k++){
          v1[k+1]=static_cast<float>(y[k]);
          v2[k+1]=v1[k+1]+static_cast<float>(ydot[k]);
        }
        if(!suppress)xpp::comp_color(s,v1.data(),v2.data(),s.model().node,1.0);
      }
      if(s.nullclines.df_flag==1||s.nullclines.df_flag==4){
        if(!suppress)phase_data_arrow(s.plot_windows,y[inx],y[iny],ydot[inx],ydot[iny]);
        scale_dxdy(s,ydot[inx],ydot[iny],&dxp,&dyp);
        if(DFIELD_TYPE==1){
          ydot[inx]/=mdf;
          ydot[iny]/=mdf;
        }
        else{
          const double amp=hypot(dxp,dyp);
          if(amp!=0.0){
            ydot[inx]/=amp;
            ydot[iny]/=amp;
          }
        }
        const double xv1=y[inx]+ydot[inx]*dz;
        const double xv2=y[iny]+ydot[iny]*dz;
        if(!suppress){
          bead_abs(s,static_cast<float>(xv1),static_cast<float>(xv2));
          line_abs(s,static_cast<float>(y[inx]),static_cast<float>(y[iny]),static_cast<float>(xv1),static_cast<float>(xv2));
        }
        else
          dump->print("{:g} {:g} {:g} {:g}\n",y[inx],y[iny],xv1,xv2);
      }
      if(s.nullclines.df_flag==2&&j>0&&i<grid)
        frect_abs(s,static_cast<float>(y[inx]),static_cast<float>(y[iny]),static_cast<float>(du),static_cast<float>(dv));
    }
  }
  if (s.plot_file.plt_fmt_flag==SVGFMT){
    s.nullclines.doing_dfield=0;
    svg_write("</g>");
  }
}

void save_the_nullclines(xpp::Session &s)
{
  if(s.numerics.null_here==0)return;
  std::string filename="nc.dat";
  ping();
  if(!file_selector("Save nullclines",filename,"*.dat"))return;
  xpp::Writer fp(filename.c_str());
  if(!fp){
    err_msg("Cant open file!");
    return;
  }
  dump_clines(fp,x_null.data(),num_x_n,y_null.data(),num_y_n);
  fp.commit();
  save_frozen_clines(filename);
}

} // namespace

void froz_cline_stuff_com(xpp::Session &s, int i)
{
  int delay=200;
  if(!frozen_started)start_ncline();
  switch(i){
  case 0:
    if(s.numerics.null_here==0)return;
    add_froz_cline(x_null.data(),num_x_n,null_ix,y_null.data(),num_y_n,null_iy);
    note_frozen(s);
    break;
  case 1:
    clear_froz_cline();
    note_frozen(s);
    break;
  case 3:
    new_int("Delay (msec)",&delay);
    if(delay<=0)delay=0;
    redraw_froz_cline(s,delay);
    break;
  case 2:
    do_range_clines(s);
    break;
  }
}

int get_nullcline_floats(float **v,int *n,int who,int type) /* type=0,1 */
{
  if(who<0){
    if(type==0){
      *v=x_null.empty()?nullptr:x_null.data();
      *n=num_x_n;
    }
    else {
      *v=y_null.empty()?nullptr:y_null.data();
      *n=num_y_n;
    }
    return 0;
  }
  if(!frozen_started||who>static_cast<int>(frozen.size()))return 1;
  if(who==static_cast<int>(frozen.size())){
    /* the list's empty end */
    *v=nullptr;
    *n=0;
    return 0;
  }
  FrozenCline &z=frozen[who];
  if(type==0){
    *v=z.xn.empty()?nullptr:z.xn.data();
    *n=z.nmx;
  }
  else {
    *v=z.yn.empty()?nullptr:z.yn.data();
    *n=z.nmy;
  }
  return 0;
}

/*  all the nifty 2D stuff here    */

void do_batch_nclines(xpp::Session &s)
{
  if(!batch_options.enabled)return;
  if(!s.nullclines.nc_batch)return;
  if(s.nullclines.nc_batch==1){
    new_clines_com(s,0);
    return;
  }
}

void set_colorization_stuff(xpp::Session &s)
{
  xpp::user_set_color_par(s,s.nullclines.colorize_flag,s.nullclines.color_via.c_str(),s.nullclines.color_via_lo,s.nullclines.color_via_hi);
}

void do_batch_dfield(xpp::Session &s)
{
  if(!batch_options.enabled)return;
  switch(s.nullclines.df_batch){
  case 0:
    return;
  case 1:
  case 4:
    s.nullclines.df_flag=1;
    DFIELD_TYPE=1;
    break;
  case 2:
  case 5:
    s.nullclines.df_flag=1;
    DFIELD_TYPE=0;
    break;
  case 3:
    s.nullclines.df_flag=2;
    DFIELD_TYPE=0;
    break;
  default:
    return;
  }
  DF_IX=s.plot_windows.current->xv[0];
  DF_IY=s.plot_windows.current->yv[0];
  redraw_dfield(s);
}

namespace {

/* the current window shows the direction field set last */
bool dfield_shown(xpp::Session &s)
{
  return !(s.nullclines.df_flag==0||
     s.plot_windows.current->TimeFlag||s.plot_windows.current->xv[0]==s.plot_windows.current->yv[0]||s.plot_windows.current->ThreeDFlag
     || DF_IX!=s.plot_windows.current->xv[0]||DF_IY!=s.plot_windows.current->yv[0]);
}

/* the current window's direction field, drawn, or into dump */
void dfield_of_window(xpp::Session &s, xpp::Writer *dump)
{
  const int inx=s.plot_windows.current->xv[0]-1;
  const int iny=s.plot_windows.current->yv[0]-1;
  const int grid=s.nullclines.df_grid;
  const double du=(s.plot_windows.current->xhi-s.plot_windows.current->xlo)/static_cast<double>(grid);
  const double dv=(s.plot_windows.current->yhi-s.plot_windows.current->ylo)/static_cast<double>(grid);

  const double dup=static_cast<double>(s.drawing.d_right-s.drawing.d_left)/static_cast<double>(grid);
  const double dvp=static_cast<double>(s.drawing.d_top-s.drawing.d_bottom)/static_cast<double>(grid);
  const double dz=hypot(dup,dvp)*(.25+.75*DFIELD_TYPE);
  const double u0=s.plot_windows.current->xlo;
  const double v0=s.plot_windows.current->ylo;
  if(!dump)set_linestyle(s,s.plot_windows.current->color[0]);
  dfield_grid(s,grid,u0,v0,du,dv,dz,inx,iny,dump);
}

} // namespace

void redraw_dfield(xpp::Session &s)
{
  if(dfield_shown(s))dfield_of_window(s,nullptr);
}

void write_dfield(xpp::Session &s, const char *name)
{
  if(!dfield_shown(s)){
    err_msg("No direction field in this window");
    return;
  }
  /* in PostScript's frame, whatever the window's, as -silent always
     wrote it: the arrows' lengths do not depend on the window's size */
  const DrawingState drawn=s.drawing;
  init_ps(s);
  xpp::Writer w(name);
  if(w){
    dfield_of_window(s,&w);
    w.commit();
  }
  s.drawing=drawn;
}

void direct_field_com(xpp::Session &s, int c)
{
  const int inx=s.plot_windows.current->xv[0]-1;
  const int iny=s.plot_windows.current->yv[0]-1;
  const double dtold=s.numerics.delta_t;
  const double oldtrans=s.numerics.trans;
  int grid=s.nullclines.df_grid;

  if(s.plot_windows.current->TimeFlag||s.plot_windows.current->xv[0]==s.plot_windows.current->yv[0]||s.plot_windows.current->ThreeDFlag)
    return;

  if(c==2){
    s.nullclines.df_flag=0;
    return;
  }
  if(c==0)DFIELD_TYPE=1;
  if(c==4)DFIELD_TYPE=0;
  new_int("Grid:",&grid);
  if(grid<=1)return;
  s.nullclines.df_grid=grid;
  double du=(s.plot_windows.current->xhi-s.plot_windows.current->xlo)/static_cast<double>(grid);
  double dv=(s.plot_windows.current->yhi-s.plot_windows.current->ylo)/static_cast<double>(grid);

  const double dup=static_cast<double>(s.drawing.d_right-s.drawing.d_left)/static_cast<double>(grid);
  const double dvp=static_cast<double>(s.drawing.d_top-s.drawing.d_bottom)/static_cast<double>(grid);
  const double dz=hypot(dup,dvp)*(.25+.75*DFIELD_TYPE) ;
  const double u0=s.plot_windows.current->xlo;
  const double v0=s.plot_windows.current->ylo;
  set_linestyle(s,s.plot_windows.current->color[0]);
  if(c!=1){
    s.nullclines.df_flag=1;
    if(c==3){
      s.nullclines.df_flag=2;
      du=(s.plot_windows.current->xhi-s.plot_windows.current->xlo)/static_cast<double>(grid+1);
      dv=(s.plot_windows.current->yhi-s.plot_windows.current->ylo)/static_cast<double>(grid+1);
    }
    DF_IX=inx+1;
    DF_IY=iny+1;
    dfield_grid(s,grid,u0,v0,du,dv,dz,inx,iny,nullptr);
    s.numerics.trans=oldtrans;
    return;
  }
  s.numerics.storflag=0;

  s.integrator.suppress_bounds=1;
  phase_data_flow_start();
  std::array<double,MAXODE> y;
  xpp::FirstError failure; /* a trajectory's, shown once the flow is drawn */
  for(int k=0;k<2;k++){
    for(int i=0;i<=grid;i++)
      for(int j=0;j<=grid;j++){
        xpp::get_ic(s,2,y.data());
        y[inx]=u0+du*i;
        y[iny]=v0+dv*j;
        double t=0.0;
        int start=1;
        phase_data_flow_next();
        failure.keep(xpp::integrate(s,&t,y.data(),s.numerics.tend,s.numerics.delta_t,1,s.numerics.njmp,&start));
      }
    s.numerics.delta_t=-s.numerics.delta_t;
  }
  phase_data_flow_stop();
  s.integrator.suppress_bounds=0;
  s.numerics.delta_t=dtold;
  if (s.plot_file.plt_fmt_flag==SVGFMT){
    s.nullclines.doing_dfield=0;
    svg_write("</g>");
  }
  if(const xpp::Result<> r=failure.result();!r)xpp::show_error(r.error());
}

/* animated nullclines stuff
   added Aug 31 97
   just redraws them
   It will allow you to either freeze a range of them
   or just one at a time

   freeze   -   store the current set
   range_freeze - compute over some range of parameters
   clear - delete all but the current set
   animate - replay all frozen ones (not current set )
   */

void restore_nullclines(xpp::Session &s)
{
  const int col1=s.nullclines.x_null_color,col2=s.nullclines.y_null_color;
  if(s.numerics.null_here==0)return;
  if(s.plot_windows.current->xv[0]==null_ix&&s.plot_windows.current->yv[0]==null_iy&&s.plot_windows.current->ThreeDFlag==0){
    set_linestyle(s,col1);
    restor_null(s,x_null.data(),num_x_n,1);
    set_linestyle(s,col2);
    restor_null(s,y_null.data(),num_y_n,2);
    phase_data_nullclines(s.plot_windows,x_null.data(),num_x_n,y_null.data(),num_y_n,null_ix,null_iy,col1,col2);
  }
  redraw_froz_cline(s,0);
}

void create_new_cline(xpp::Session &s)
{
  if(s.numerics.null_here)
    new_clines_com(s,0);
}

void new_clines_com(xpp::Session &s, int c)
{
  const int course=s.numerics.nmesh;
  const int col1=s.nullclines.x_null_color,col2=s.nullclines.y_null_color;

  if(s.plot_windows.current->ThreeDFlag||s.plot_windows.current->TimeFlag||s.plot_windows.current->xv[0]==s.plot_windows.current->yv[0])return;

  switch(c){
  case 1:
    restore_nullclines(s);
    return;
  case 2:
    s.plot_windows.current->Nullrestore=1;
    return;
  case 3:
    s.plot_windows.current->Nullrestore=0;
    return;
  case 4:
    froz_cline_stuff(s);
    return;
  case 5:
    save_the_nullclines(s);
    return;
  case 0:
    break;
  default:
    return;
  }
  for(int i=s.model().node;i<s.model().node+s.model().nmarkov;i++)setvar(s,i+1+s.model().fix_var,s.last_ic[i]);
  const float xmin=static_cast<float>(s.plot_windows.current->xmin);
  const float xmax=static_cast<float>(s.plot_windows.current->xmax);
  const float y_tp=static_cast<float>(s.plot_windows.current->ymax);
  const float y_bot=static_cast<float>(s.plot_windows.current->ymin);
  null_ix=s.plot_windows.current->xv[0];
  null_iy=s.plot_windows.current->yv[0];
  null_storage(s,course);

  WHICH_CRV=null_ix;
  set_linestyle(s,col1);
  new_nullcline(s,course,xmin,y_bot,xmax,y_tp,x_null,&num_x_n);
  ping();

  WHICH_CRV=null_iy;
  set_linestyle(s,col2);
  new_nullcline(s,course,xmin,y_bot,xmax,y_tp,y_null,&num_y_n);
  ping();
  phase_data_nullclines(s.plot_windows,x_null.data(),num_x_n,y_null.data(),num_y_n,null_ix,null_iy,col1,col2);
}
