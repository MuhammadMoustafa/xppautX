/* xpp_io.h first: it pulls in <optional>/<string_view>/<format>, which
   auto_f2c.h's own min/max macros (included transitively below, through
   auto_nox.h) break if they are already defined first. */
#include <algorithm>
#include <array>
#include <deque>
#include <vector>
#include "storage.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_log.h"
#include "diagram.h"
#include "auto_data.h"
#include "autevd.h"

#include "my_svg.h"
#include "my_ps.h"
#include "graphics.h"
#include "image_format.h"
#include <stdlib.h>
#include <stdio.h>
#include "browse.h"
#include "graf_par.h"
#include "form_ode.h"
#include "model.h"

namespace {
/* a new last point of n variables, zeroed */
xpp::DIAGRAM *new_point(AutoDiagram &dg, int n)
{
  DiagramPoint &p=dg.points.emplace_back();
  for(std::vector<double> *v:{&p.uhi,&p.ulo,&p.u0,&p.ubar,&p.evr,&p.evi})
    v->assign(n,0.0);
  p.d.uhi=p.uhi.data();
  p.d.ulo=p.ulo.data();
  p.d.u0=p.u0.data();
  p.d.ubar=p.ubar.data();
  p.d.evr=p.evr.data();
  p.d.evi=p.evi.data();
  p.d.index=static_cast<int>(dg.points.size())-1;
  return &p.d;
}

/* point d set to p, a point of AUTO's run, with flag2 (s: the run's
   kind of calculation and torus period) */
void edit_diagram(const xpp::Session &s, xpp::DIAGRAM *d, const xpp::DIAGRAM &p, int n, int flag2)
{
  int i;
  d->calc=s.auto_state.type_of_calc;
  d->ibr=p.ibr;
  d->ntot=p.ntot;
  d->itp=p.itp;
  d->lab=p.lab;
  d->nfpar=p.nfpar;
  d->norm=p.norm;
  for(i=0;i<8;i++){
    d->par[i]=p.par[i];
  }

  d->per=p.per;
 
  d->icp1=p.icp1;
  d->icp2=p.icp2;
  d->icp3=p.icp3;
  d->icp4=p.icp4;
  d->flag2=flag2;
  for(i=0;i<n;i++){
    d->ulo[i]=p.ulo[i];
    d->uhi[i]=p.uhi[i];
    d->ubar[i]=p.ubar[i];
    d->u0[i]=p.u0[i];
    d->evr[i]=p.evr[i];
    d->evi[i]=p.evi[i];
   }
  d->torper=s.auto_state.blrtn.torper;
}
} // namespace

int diagram_count(const AutoDiagram &diagram)
{
  return static_cast<int>(diagram.points.size());
}

xpp::DIAGRAM *diagram_point(AutoDiagram &diagram, int index)
{
  if(index<0||index>=diagram_count(diagram))return NULL;
  return &diagram.points[index].d;
}

xpp::DIAGRAM *diagram_first(AutoDiagram &diagram)
{
  return diagram_point(diagram,0);
}

xpp::DIAGRAM *diagram_next(AutoDiagram &diagram, const xpp::DIAGRAM *d)
{
  return diagram_point(diagram,d->index+1);
}

xpp::DIAGRAM *diagram_prev(AutoDiagram &diagram, const xpp::DIAGRAM *d)
{
  return diagram_point(diagram,d->index-1);
}

void diagram_restore(xpp::Session &s, std::deque<DiagramPoint> points)
{
  if(points.empty()){
    start_diagram(s,s.model().node);
    return;
  }
  s.diagram.points=std::move(points);
  int index=0;
  for(DiagramPoint &p:s.diagram.points){
    p.d.uhi=p.uhi.data();
    p.d.ulo=p.ulo.data();
    p.d.u0=p.u0.data();
    p.d.ubar=p.ubar.data();
    p.d.evr=p.evr.data();
    p.d.evi=p.evi.data();
    p.d.index=index++;
  }
  s.auto_state.diag_flag=1;
}

void start_diagram(xpp::Session &s, int n)
{
  s.diagram.points.clear();
  new_point(s.diagram,n);
  s.auto_state.diag_flag=0;
}

void edit_start(xpp::Session &s, const xpp::DIAGRAM &p, int n)
{
  edit_diagram(s,diagram_first(s.diagram),p,n,s.auto_state.two_param);
}

void add_diagram(xpp::Session &s, const xpp::DIAGRAM &p, int n)
{
  xpp::DIAGRAM *dnew=new_point(s.diagram,n);
  edit_diagram(s,dnew,p,n,p.flag2);
}

xpp::DIAGRAM *last_diagram(AutoDiagram &diagram)
{
  return diagram_point(diagram,diagram_count(diagram)-1);
}

const xpp::DIAGRAM *diagram_of_label(const xpp::Session &s, int lab)
{
  if(lab<=0||s.auto_state.diag_flag==0)return NULL; /* DiagFlag 0: the first point is not filled in yet */
  for(const DiagramPoint &p:s.diagram.points)
    if(p.d.lab==lab)return &p.d;
  return NULL;
}

int diagram_has(const AutoDiagram &diagram, int index,int ibr,int ntot)
{
  if(index<0||index>=diagram_count(diagram))return 0;
  const xpp::DIAGRAM *d=&diagram.points[index].d;
  return d!=NULL&&d->ibr==ibr&&abs(d->ntot)==abs(ntot);
}

void kill_diagrams(xpp::Session &s)
{
  start_diagram(s,s.model().node);
}

void redraw_diagram(xpp::Session &s)
{
  xpp::DIAGRAM *d;
  int type,flag=0;
  draw_bif_axes(s);
  d=diagram_first(s.diagram);
  if(diagram_next(s.diagram,d)==NULL)return;
  xpp::auto_data_hold(s,1); /* plotting again leaves the strip and circle as they were */
  while(1){
    type=xpp::get_bif_type(d->ibr,d->ntot,d->lab);
 
    if(d->ntot==1)flag=0;
    else flag=1;
    add_point(s,*d,d->par,type,flag);
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
  }
  xpp::auto_data_hold(s,0);
}

namespace {

/* the file one of the diagram's text exports writes, asked for with
   `name` as the default; opened only when there are points to write, so
   an empty diagram leaves an existing file alone. Empty when cancelled,
   or when it cannot be written (err_msg says so). */
xpp::Writer diagram_file(const xpp::Session &s, const char *title, const char *name)
{
  std::string filename=name;
  if(!xpp::file_selector(title,filename,"*.dat"))return xpp::Writer();
  if(diagram_count(s.diagram)<2)return xpp::Writer();
  xpp::Writer w(filename.c_str());
  if(!w)xpp::command_error("write diagram",xpp::format("Cannot write {}",filename));
  return w;
}

/* the diagram as a picture: PostScript or SVG, whichever begin (ps_init
   or svg_init) opened, finished with end */
void export_diagram(xpp::Session &s, const char *title, const char *name, const char *wild,
                    xpp::Result<> (*begin)(xpp::Session &, const char *, int), void (*end)(xpp::Session &))
{
  xpp::DIAGRAM *d;
  int type,flag=0;
  std::string filename=name;
  if(!xpp::file_selector(title,filename,wild))return;
  if(!xpp::ok_or_show(begin(s,filename.c_str(),s.plot_export.color)))
    return;
  draw_export_axes(s);
  d=diagram_first(s.diagram);
  if(diagram_next(s.diagram,d)==NULL)return;
  while(1){
    type=xpp::get_bif_type(d->ibr,d->ntot,d->lab);
    if (type < 0)
    {
    	xpp::log(XPP_LOG_WARN, "Unable to get bifurcation type.\n");
    }
    if(d->ntot==1)flag=0;
    else flag=1;
    add_ps_point(s,*d,type,flag);
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
  }
  end(s);
  set_normal_scale(s);
}

} // namespace

void write_info_out(xpp::Session &s)
{
  xpp::DIAGRAM *d;
  int type,i;
  /*int flag=0
  */
  int icp1,icp2;
  double *par;
  double par1,par2=0,*uhigh,*ulow,per;
  xpp::Writer w=diagram_file(s,"Write all info","allinfo.dat");
  if(!w)return;
  d=diagram_first(s.diagram);
 while(1){
    type=xpp::get_bif_type(d->ibr,d->ntot,d->lab);
    
    icp1=d->icp1;
    icp2=d->icp2;
    par=d->par;
    per=d->per;
    uhigh=d->uhi;
    ulow=d->ulo;
    /*ubar=d->ubar; Not used*/
   /* u0=d->u0; Not used*/
    /* a=d->norm; Not used*/
    par1=par[icp1];
    if(icp2<s.auto_state.npar)
      par2=par[icp2];
    else 
      par2=par1;
     
    {
      std::string line=xpp::format("{} {} {} {:g} {:g} {:g} ",
	      type,d->ibr,d->flag2,par1,par2,per);
      for(i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",uhigh[i]);
      for(i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",ulow[i]);
      for(i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} {:g} ",d->evr[i],d->evi[i]);
      line+='\n';
      w.print("{}",line);
    }
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
  }
  w.commit();

}

void load_browser_with_branch(xpp::Session &s, int ibr,int pts,int pte)
{
   xpp::DIAGRAM *d;
   int i,j,pt;
  int icp1;
  double *par;
  double par1,*u0;
  int first,last,nrows;
  first=abs(pts);
  last=abs(pte);
  if(last<first){ /* reorder the points so that we will store in right range*/
    i=first;
    first=last;
    last=i;
  }
  nrows=last-first+1;
   d=diagram_first(s.diagram);
  if(diagram_next(s.diagram,d)==NULL)return;
  j=0;
 while(1){
    pt=abs(d->ntot);
    if((d->ibr==ibr) && (pt>=first) && (pt<=last)){
      icp1=d->icp1;
      par=d->par;
      u0=d->u0;

      par1=par[icp1];
      s.data_store.col[0][j]=par1;
      for(i=0;i<s.model().node;i++)
	s.data_store.col[i+1][j]=u0[i];
      j++;
    }
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
        
 }
 s.data_store.rows=nrows;
 refresh_browser(s,nrows);
}
void write_init_data_file(xpp::Session &s)
{
  xpp::DIAGRAM *d;
  int i;
  int icp1;
  double *par;
  double par1,*u0;
  xpp::Writer w=diagram_file(s,"Write init data file","initdata.dat");
  if(!w)return;
  d=diagram_first(s.diagram);
 while(1){
    icp1=d->icp1;
    par=d->par;
    u0=d->u0;

    par1=par[icp1];

    {
      std::string line=xpp::format("{:g} ",par1);
      for(i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",u0[i]);
      line+='\n';
      w.print("{}",line);
    }
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
  }
  w.commit();

}

void write_pts(xpp::Session &s)
{
  xpp::DIAGRAM *d;
  int type;
  int icp1,icp2;
  double *par;
  double x,y1,y2,par1,par2=0,a,*uhigh,*ulow,*ubar,per;
  xpp::Writer w=diagram_file(s,"Write points","diagram.dat");
  if(!w)return;
  d=diagram_first(s.diagram);
  while(1){
    type=xpp::get_bif_type(d->ibr,d->ntot,d->lab);
    
    icp1=d->icp1;
    icp2=d->icp2;
    par=d->par;
    per=d->per;
    uhigh=d->uhi;
    ulow=d->ulo;
    ubar=d->ubar;
    a=d->norm;
    par1=par[icp1];
    if(icp2<s.auto_state.npar)
      par2=par[icp2];

    /* now we have to check is the diagram parameters correspond to the 
       current view 
    */
    if(check_plot_type(s,d->flag2,icp1,icp2)==1){
      auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
      w.print("{:g} {:g} {:g} {} {} {}\n",
	      x,y1,y2,type,abs(d->ibr),d->flag2);
    }
      d=diagram_next(s.diagram,d);
      if(d==NULL)break;
  }
  w.commit();
}

/* the AUTO File menu's Postscript/SVG entries (W53, issue #101): one
   function through the image_format.h registry in place of the former
   post_auto/svg_auto pair, same "auto.ps"/"auto.svg" default name and
   "*.ps"/"*.svg" filter as before */
void export_auto_picture(xpp::Session &s, int fmt)
{
  const xpp::ImageFormat &f=xpp::image_formats[fmt];
  std::string name=xpp::format("auto.{}",f.extension);
  std::string wild=xpp::format("*.{}",f.extension);
  export_diagram(s,f.name,name.c_str(),wild.c_str(),f.begin,f.end);
}

void bound_diagram(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi)
{
  xpp::DIAGRAM *d;
  int type;
  
  double x,y1,y2,par1,par2=0.0;
  d=diagram_first(s.diagram);
  if(diagram_next(s.diagram,d)==NULL)return;
  *xlo=1.e16;
  *ylo=*xlo;
  *xhi=-*xlo;
  *yhi=-*ylo;
  while(1){
    type=xpp::get_bif_type(d->ibr,d->ntot,d->lab);
    if (type <1)
    {
        xpp::log(XPP_LOG_WARN, "Unable to get bifurcation type.\n");
    }
    par1=d->par[d->icp1];
    if(d->icp2<s.auto_state.npar)par2=d->par[d->icp2];
    auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,d->per,d->uhi,d->ulo,d->ubar,d->norm);
    if(x<*xlo)*xlo=x;
    if(x>*xhi)*xhi=x;
    if(y2<*ylo)*ylo=y2;
    if(y1>*yhi)*yhi=y1;
    d=diagram_next(s.diagram,d);
    if(d==NULL)break;
  }
}

int load_diagram(xpp::Session &s, FILE *fp, int node)
{
  std::array<double,NAUTO> u0,uhi,ulo,ubar,evr,evi;
  std::array<double,8> par;
  double norm,per;
  int i,flag=0;
  int n;
  int calc,ibr,ntot,itp,lab,index,nfpar,icp1,icp2,icp3,icp4,flag2;
  xpp::TokenReader tr=xpp::TokenReader::attach(fp);
  if (!tr.read(n)) return -1;
  if(n==0){
    return(-1);
  }

  while(1){
    if (!tr.read(calc) || !tr.read(ibr) || !tr.read(ntot) || !tr.read(itp) || !tr.read(lab)
	|| !tr.read(index) || !tr.read(nfpar) || !tr.read(icp1) || !tr.read(icp2) || !tr.read(icp3)
	|| !tr.read(icp4) || !tr.read(flag2)) break;
    for(i=0;i<8;i++) if (!tr.read(par[i])) break;
    if (i<8) break;
    if (!tr.read(norm) || !tr.read(per)) break;
    for(i=0;i<node;i++) if (!tr.read(u0[i]) || !tr.read(uhi[i]) || !tr.read(ulo[i]) || !tr.read(ubar[i])
			      || !tr.read(evr[i]) || !tr.read(evi[i])) break;
    if (i<node) break;
    xpp::DIAGRAM p{};
    p.ibr=ibr;
    p.ntot=ntot;
    p.itp=itp;
    p.lab=lab;
    p.nfpar=nfpar;
    p.norm=norm;
    p.uhi=uhi.data();
    p.ulo=ulo.data();
    p.u0=u0.data();
    p.ubar=ubar.data();
    p.evr=evr.data();
    p.evi=evi.data();
    std::copy(par.begin(),par.end(),p.par);
    p.per=per;
    p.icp1=icp1;
    p.icp2=icp2;
    p.icp3=icp3;
    p.icp4=icp4;
    p.flag2=flag2;
    if(flag==0){
      edit_start(s,p,node);
      flag=1;
      s.auto_state.diag_flag=1;
    }
    else
      add_diagram(s,p,node);
    if(index>=n)break;
  }
  return(1);
}

