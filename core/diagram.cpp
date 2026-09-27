/* xpp_io.h first: it pulls in <optional>/<string_view>/<format>, which
   auto_f2c.h's own min/max macros (included transitively below, through
   auto_nox.h) break if they are already defined first. */
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
#include <stdlib.h>
#include <stdio.h>
#include "browse.h"
#include "graf_par.h"
#include "form_ode.h"
#include "model.h"

namespace {
/* a new last point of n variables, zeroed */
DIAGRAM *new_point(int n)
{
  DiagramPoint &p=xpp::session().diagram.points.emplace_back();
  for(std::vector<double> *v:{&p.uhi,&p.ulo,&p.u0,&p.ubar,&p.evr,&p.evi})
    v->assign(n,0.0);
  p.d.uhi=p.uhi.data();
  p.d.ulo=p.ulo.data();
  p.d.u0=p.u0.data();
  p.d.ubar=p.ubar.data();
  p.d.evr=p.evr.data();
  p.d.evi=p.evi.data();
  p.d.index=static_cast<int>(xpp::session().diagram.points.size())-1;
  return &p.d;
}
} // namespace

int diagram_count(void)
{
  return static_cast<int>(xpp::session().diagram.points.size());
}

DIAGRAM *diagram_point(int index)
{
  if(index<0||index>=diagram_count())return NULL;
  return &xpp::session().diagram.points[index].d;
}

DIAGRAM *diagram_first(void)
{
  return diagram_point(0);
}

DIAGRAM *diagram_next(const DIAGRAM *d)
{
  return diagram_point(d->index+1);
}

DIAGRAM *diagram_prev(const DIAGRAM *d)
{
  return diagram_point(d->index-1);
}

void start_diagram(int n)
{
  xpp::session().diagram.points.clear();
  new_point(n);
  xpp::session().auto_state.diag_flag=0;
}

void edit_start(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, double *evr, double *evi)
{
  edit_diagram(diagram_first(),ibr,ntot,itp,lab,nfpar,a,uhi,ulo,u0,ubar,
	       par,per,n,icp1,icp2,icp3,icp4,xpp::session().auto_state.two_param,evr,evi,xpp::session().auto_state.blrtn.torper);
}

void edit_diagram(DIAGRAM *d, int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, int flag2, double *evr, double *evi, double tp)
{
  int i;
  d->calc=xpp::session().auto_state.type_of_calc;
  d->ibr=ibr;
  d->ntot=ntot;
  d->itp=itp;
  d->lab=lab;
  d->nfpar=nfpar;
  d->norm=a;
  for(i=0;i<8;i++){
    d->par[i]=par[i];
  }

  d->per=per;
 
  d->icp1=icp1;
  d->icp2=icp2;
  d->icp3=icp3;
  d->icp4=icp4;
  d->flag2=flag2;
  for(i=0;i<n;i++){
    d->ulo[i]=ulo[i];
    d->uhi[i]=uhi[i];
    d->ubar[i]=ubar[i];
    d->u0[i]=u0[i];
    d->evr[i]=evr[i];
    d->evi[i]=evi[i];
   }
  d->torper=tp;
}
  
void add_diagram(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, int flag2, double *evr, double *evi)
{
 DIAGRAM *dnew=new_point(n);
 edit_diagram(dnew,ibr,ntot,itp,lab,nfpar,a,uhi,ulo,u0,ubar,par,per,n,
	      icp1,icp2,icp3,icp4,flag2,evr,evi,xpp::session().auto_state.blrtn.torper);
}

DIAGRAM *last_diagram(void)
{
  return diagram_point(diagram_count()-1);
}

void set_last_diagram_from(int from)
{
  last_diagram()->from=from;
}

const DIAGRAM *diagram_of_label(int lab)
{
  if(lab<=0||xpp::session().auto_state.diag_flag==0)return NULL; /* DiagFlag 0: the first point is not filled in yet */
  for(const DiagramPoint &p:xpp::session().diagram.points)
    if(p.d.lab==lab)return &p.d;
  return NULL;
}

int diagram_has(int index,int ibr,int ntot)
{
  const DIAGRAM *d=diagram_point(index);
  return d!=NULL&&d->ibr==ibr&&abs(d->ntot)==abs(ntot);
}

void kill_diagrams()
{
  start_diagram(xpp::model().node);
}

void redraw_diagram()
{
  DIAGRAM *d;
  int type,flag=0;
  draw_bif_axes();
  d=diagram_first();
  if(diagram_next(d)==NULL)return;
  auto_data_hold(1); /* plotting again leaves the strip and circle as they were */
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
 
    if(d->ntot==1)flag=0;
    else flag=1;
    auto_point_id(d->ibr,d->ntot,d->itp,d->index,d->from);
    add_point(d->par,d->per,d->uhi,d->ulo,d->ubar,d->norm,type,flag,
	      d->lab,d->nfpar,d->icp1,d->icp2,d->icp3,d->icp4,d->flag2,d->evr,d->evi);
    d=diagram_next(d);
    if(d==NULL)break;
  }
  auto_data_hold(0);
}

namespace {

/* the file one of the diagram's text exports writes, asked for with
   `name` as the default; opened only when there are points to write, so
   an empty diagram leaves an existing file alone. Empty when cancelled,
   or when it cannot be written (err_msg says so). */
xpp::Writer diagram_file(const char *title, const char *name)
{
  std::string filename=name;
  if(!file_selector(title,filename,"*.dat"))return xpp::Writer();
  if(diagram_count()<2)return xpp::Writer();
  xpp::Writer w(filename.c_str());
  if(!w)err_msg("Can't open file");
  return w;
}

/* the diagram as a picture: PostScript or SVG, whichever begin (ps_init
   or svg_init) opened, finished with end */
void export_diagram(const char *title, const char *name, const char *wild,
                    int (*begin)(const char *, int), void (*end)(void))
{
  DIAGRAM *d;
  int type,flag=0;
  std::string filename=name;
  if(!file_selector(title,filename,wild))return;
  if(!begin(filename.c_str(),xpp::session().plot_export.color))
    return;
  draw_export_axes();
  d=diagram_first();
  if(diagram_next(d)==NULL)return;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    if (type < 0)
    {
    	xpp::log(XPP_LOG_WARN, "Unable to get bifurcation type.\n");
    }
    if(d->ntot==1)flag=0;
    else flag=1;
    add_ps_point(d->par,d->per,d->uhi,d->ulo,d->ubar,d->norm,type,flag,
	      d->lab,d->nfpar,d->icp1,d->icp2,d->flag2,d->evr,d->evi);
    d=diagram_next(d);
    if(d==NULL)break;
  }
  end();
  set_normal_scale();
}

} // namespace

void write_info_out()
{
  DIAGRAM *d;
  int type,i;
  /*int flag=0
  */
  int icp1,icp2;
  double *par;
  double par1,par2=0,*uhigh,*ulow,per;
  xpp::Writer w=diagram_file("Write all info","allinfo.dat");
  if(!w)return;
  d=diagram_first();
 while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    
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
    if(icp2<xpp::session().auto_state.npar)
      par2=par[icp2];
    else 
      par2=par1;
     
    {
      std::string line=xpp::format("{} {} {} {:g} {:g} {:g} ",
	      type,d->ibr,d->flag2,par1,par2,per);
      for(i=0;i<xpp::model().node;i++)
        line+=xpp::format("{:g} ",uhigh[i]);
      for(i=0;i<xpp::model().node;i++)
        line+=xpp::format("{:g} ",ulow[i]);
      for(i=0;i<xpp::model().node;i++)
        line+=xpp::format("{:g} {:g} ",d->evr[i],d->evi[i]);
      line+='\n';
      w.print("{}",line);
    }
    d=diagram_next(d);
    if(d==NULL)break;
  }
  w.commit();

}

extern "C" void load_browser_with_branch(int ibr,int pts,int pte)
{
   DIAGRAM *d;
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
   d=diagram_first();
  if(diagram_next(d)==NULL)return;
  j=0;
 while(1){
    pt=abs(d->ntot);
    if((d->ibr==ibr) && (pt>=first) && (pt<=last)){
      icp1=d->icp1;
      par=d->par;
      u0=d->u0;

      par1=par[icp1];
      xpp::session().data_store.col[0][j]=par1;
      for(i=0;i<xpp::model().node;i++)
	xpp::session().data_store.col[i+1][j]=u0[i];
      j++;
    }
    d=diagram_next(d);
    if(d==NULL)break;
        
 }
 xpp::session().data_store.rows=nrows;
 refresh_browser(nrows);
}
void write_init_data_file()
{
  DIAGRAM *d;
  int i;
  int icp1;
  double *par;
  double par1,*u0;
  xpp::Writer w=diagram_file("Write init data file","initdata.dat");
  if(!w)return;
  d=diagram_first();
 while(1){
    icp1=d->icp1;
    par=d->par;
    u0=d->u0;

    par1=par[icp1];

    {
      std::string line=xpp::format("{:g} ",par1);
      for(i=0;i<xpp::model().node;i++)
        line+=xpp::format("{:g} ",u0[i]);
      line+='\n';
      w.print("{}",line);
    }
    d=diagram_next(d);
    if(d==NULL)break;
  }
  w.commit();

}

void write_pts()
{
  DIAGRAM *d;
  int type;
  int icp1,icp2;
  double *par;
  double x,y1,y2,par1,par2=0,a,*uhigh,*ulow,*ubar,per;
  xpp::Writer w=diagram_file("Write points","diagram.dat");
  if(!w)return;
  d=diagram_first();
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    
    icp1=d->icp1;
    icp2=d->icp2;
    par=d->par;
    per=d->per;
    uhigh=d->uhi;
    ulow=d->ulo;
    ubar=d->ubar;
    a=d->norm;
    par1=par[icp1];
    if(icp2<xpp::session().auto_state.npar)
      par2=par[icp2];

    /* now we have to check is the diagram parameters correspond to the 
       current view 
    */
    if(check_plot_type(d->flag2,icp1,icp2)==1){
      auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
      w.print("{:g} {:g} {:g} {} {} {}\n",
	      x,y1,y2,type,abs(d->ibr),d->flag2);
    }
      d=diagram_next(d);
      if(d==NULL)break;
  }
  w.commit();
}

void post_auto()
{
  export_diagram("Postscript","auto.ps","*.ps",ps_init,ps_end);
}

void svg_auto()
{
  export_diagram("SVG","auto.svg","*.svg",svg_init,svg_end);
}

void bound_diagram(double *xlo, double *xhi, double *ylo, double *yhi)
{
  DIAGRAM *d;
  int type;
  
  double x,y1,y2,par1,par2=0.0;
  d=diagram_first();
  if(diagram_next(d)==NULL)return;
  *xlo=1.e16;
  *ylo=*xlo;
  *xhi=-*xlo;
  *yhi=-*ylo;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    if (type <1)
    {
        xpp::log(XPP_LOG_WARN, "Unable to get bifurcation type.\n");
    }
    par1=d->par[d->icp1];
    if(d->icp2<xpp::session().auto_state.npar)par2=d->par[d->icp2];
    auto_xy_plot(&x,&y1,&y2,par1,par2,d->per,d->uhi,d->ulo,d->ubar,d->norm);
    if(x<*xlo)*xlo=x;
    if(x>*xhi)*xhi=x;
    if(y2<*ylo)*ylo=y2;
    if(y1>*yhi)*yhi=y1;
    d=diagram_next(d);
    if(d==NULL)break;
  }
}

int save_diagram(FILE *fp, int n)
{
  int i;
  DIAGRAM *d;
  xpp::print(fp,"{}\n",diagram_count()-1);
  if(diagram_count()==1)
    return(-1);
  d=diagram_first();
  while(1){
    std::string line=xpp::format("{} {} {} {} {} {} {} {} {} {} {} {}\n",
	    d->calc,d->ibr,d->ntot,d->itp,d->lab,d->index,d->nfpar,
	    d->icp1,d->icp2,d->icp3,d->icp4,d->flag2);
    for(i=0;i<8;i++)line+=xpp::format("{:g} ",d->par[i]);
    line+=xpp::format("{:g} {:g} \n",d->norm,d->per);
    xpp::print(fp,"{}",line);

    for(i=0;i<n;i++)
      xpp::print(fp,"{:f} {:f} {:f} {:f} {:f} {:f}\n",d->u0[i],d->uhi[i],d->ulo[i],
		 d->ubar[i],d->evr[i],d->evi[i]);
    d=diagram_next(d);
    if(d==NULL)break;
  }
  return(1);
}

int load_diagram(FILE *fp, int node)
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
    if(flag==0){
      edit_start(ibr,ntot,itp,lab,nfpar,norm,uhi.data(),ulo.data(),u0.data(),ubar.data(),par.data(),per,node,
		 icp1,icp2,icp3,icp4,evr.data(),evi.data());
      flag=1;
      xpp::session().auto_state.diag_flag=1;
    }
    else
      add_diagram(ibr,ntot,itp,lab,nfpar,norm,uhi.data(),ulo.data(),u0.data(),ubar.data(),par.data(),per,node,
		  icp1,icp2,icp3,icp4,flag2,evr.data(),evi.data());
    if(index>=n)break;
  }
  return(1);
}

