#include "auto_unknowns.h"
/* xpp_io.h first: it pulls in <optional>/<string_view>/<format>, which
   auto_f2c.h's own min/max macros (included transitively below, through
   auto_nox.h) break if they are already defined first. */
#include "xpp_files.h"
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
#include "picture.h"
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
xpp::DIAGRAM *new_point(AutoDiagram &dg, std::size_t n)
{
  xpp::DIAGRAM &p=dg.points.emplace_back();
  p.size_variables(n);
  p.index=static_cast<int>(dg.points.size())-1;
  return &p;
}

/* point d set to p, a point of AUTO's run, with flag2 (s: the run's
   kind of calculation and torus period) */
void edit_diagram(const xpp::Session &s, xpp::DIAGRAM *d, const xpp::DIAGRAM &p, xpp::AutoCalculation flag2)
{
  d->calc=s.auto_state.type_of_calc;
  d->ibr=p.ibr;
  d->ntot=p.ntot;
  d->itp=p.itp;
  d->lab=p.lab;
  d->nfpar=p.nfpar;
  d->norm=p.norm;
  d->par=p.par;
  d->parameter_indices=p.parameter_indices;
  d->branch=p.branch;

  d->per=p.per;
 
  d->icp1=p.icp1;
  d->icp2=p.icp2;
  d->icp3=p.icp3;
  d->icp4=p.icp4;
  d->flag2=flag2;
  d->ulo=p.ulo;
  d->uhi=p.uhi;
  d->ubar=p.ubar;
  d->u0=p.u0;
  d->evr=p.evr;
  d->evi=p.evi;
  d->torper=s.auto_state.blrtn.torper;
  d->jacobian=p.jacobian;
}
} // namespace

double diagram_parameter(const xpp::DIAGRAM &point, int parameter)
{
  for(std::size_t i=0;i<point.parameter_indices.size();i++)
    if(point.parameter_indices[i]==parameter)return point.par[i];
  if(point.branch && parameter>=0 && static_cast<std::size_t>(parameter)<point.branch->parameters.size())
    return point.branch->parameters[parameter];
  return 0.0;
}

double diagram_axis_value(const xpp::DIAGRAM &point, xpp::AutoAxis axis)
{
  if(axis.is_period())return point.per;
  return axis.is_parameter()?diagram_parameter(point,axis.index):0.0; /* unused numerical slots have no model value */
}

std::vector<double> diagram_parameters(const xpp::DIAGRAM &point)
{
  std::vector<double> values=point.branch->parameters;
  for(std::size_t i=0;i<point.parameter_indices.size();i++)values[point.parameter_indices[i]]=point.par[i];
  return values;
}

void diagram_record_branch(xpp::Session &s, xpp::DIAGRAM &point)
{
  const xpp::DIAGRAM *last=last_diagram(s.diagram);
  if(std::abs(point.ntot)!=1 && last && last->branch && last->ibr==point.ibr){
    point.branch=last->branch;
    return;
  }
  auto branch=std::make_shared<xpp::DiagramBranch>();
  for(int i=0;i<s.model().nupar;i++)branch->parameters.push_back(s.parser.constants[s.model().upar_con[i]]);
  for(std::size_t i=0;i<point.parameter_indices.size();i++)
    if(point.parameter_indices[i]<s.model().nupar)branch->parameters[point.parameter_indices[i]]=point.par[i];
  point.branch=std::move(branch);
}

int diagram_count(const AutoDiagram &diagram)
{
  return static_cast<int>(diagram.points.size());
}

void diagram_restore(xpp::Session &s, std::deque<xpp::DIAGRAM> points)
{
  if(points.empty()){
    start_diagram(s,s.model().node);
    return;
  }
  s.diagram.points=std::move(points);
  int index=0;
  for(xpp::DIAGRAM &p:s.diagram.points)p.index=index++;
  s.auto_state.diag_flag=true;
}

void start_diagram(xpp::Session &s, int n)
{
  s.diagram.points.clear();
  new_point(s.diagram,static_cast<std::size_t>(n));
  s.auto_state.diag_flag=false;
}

void edit_start(xpp::Session &s, const xpp::DIAGRAM &p)
{
  edit_diagram(s,&s.diagram.points.front(),p,p.flag2);
}

void add_diagram(xpp::Session &s, const xpp::DIAGRAM &p)
{
  xpp::DIAGRAM *dnew=new_point(s.diagram,p.u0.size());
  edit_diagram(s,dnew,p,p.flag2);
}

xpp::DIAGRAM *last_diagram(AutoDiagram &diagram)
{
  return diagram.points.empty() ? NULL : &diagram.points.back();
}

const xpp::DIAGRAM *diagram_of_label(const xpp::Session &s, int lab)
{
  if(lab<=0||!s.auto_state.diag_flag)return NULL; /* not yet: the first point is not filled in yet */
  for(const xpp::DIAGRAM &p:s.diagram.points)
    if(p.lab==lab)return &p;
  return NULL;
}

namespace {
/* d is AUTO's point ntot of branch ibr, whichever sign either has */
bool is_point(const xpp::DIAGRAM &d, int ibr, int ntot) { return d.ibr==ibr&&abs(d.ntot)==abs(ntot); }
}

bool diagram_has(const AutoDiagram &diagram, int index,int ibr,int ntot)
{
  return index>=0&&index<diagram_count(diagram)&&is_point(diagram.points[index],ibr,ntot);
}

const xpp::DIAGRAM *diagram_find(const AutoDiagram &diagram, int ibr, int ntot)
{
  for(const xpp::DIAGRAM &d:diagram.points)
    if(is_point(d,ibr,ntot))return &d;
  return NULL;
}

void redraw_diagram(xpp::Session &s)
{
  xpp::DiagramPointType type;
  int flag=0;
  draw_bif_axes(s);
  if(diagram_count(s.diagram)<2)return;
  xpp::auto_data_hold(s,1); /* plotting again leaves the strip and circle as they were */
  for(xpp::DIAGRAM &point:s.diagram.points){
    xpp::DIAGRAM *d=&point;
    type=xpp::get_bif_type(d->ibr,d->ntot);
 
    if(d->ntot==1)flag=0;
    else flag=1;
    add_point(s,*d,type,flag);
  }
  xpp::auto_data_hold(s,0);
}

std::string diagram_default_file(const xpp::Session &s, std::string_view what, std::string_view extension)
{
  return xpp::files::output_name(s.model().this_file,extension,what);
}

namespace {

/* a diagram export needs points */
xpp::Result<> need_points(const xpp::Session &s)
{
  if(diagram_count(s.diagram)<2)return xpp::fail("auto","The diagram has no points to write",xpp::command_place());
  return {};
}

/* where an export goes: the file given (an argument), else the one the dialog asks for with the model's default name;
   nothing when declined */
std::optional<std::string> export_target(const xpp::Session &s, const std::optional<std::string> &given, const char *title, const char *what, const char *extension)
{
  if(given)return *given;
  std::string filename=diagram_default_file(s,what,extension);
  if(!xpp::file_selector(title,filename,xpp::format("*{}",extension)))return std::nullopt;
  return filename;
}

/* one of the diagram's text exports: its file (asked for or given), opened only when there are points to write, so an
   empty diagram leaves an existing file alone, written by `write`, committed. Nothing is written when declined. */
template <class Write> xpp::Result<> export_text(xpp::Session &s, const std::optional<std::string> &file, const char *title, const char *what, Write write)
{
  if(const xpp::Result<> points=need_points(s);!points)return points;
  const auto target=export_target(s,file,title,what,".dat");
  if(!target)return {};
  xpp::Result<> opened;
  xpp::Writer w=xpp::open_writer_asking(*target,false,&opened);
  if(!opened)return opened;
  if(!w)return {};
  write(w);
  return xpp::commit_save(w);
}

/* the diagram as a picture in the vector format f, in the file given or asked for */
xpp::Result<> export_diagram(xpp::Session &s, const std::optional<std::string> &file, const xpp::ImageFormat &f)
{
  if(const xpp::Result<> points=need_points(s);!points)return points;
  const auto target=export_target(s,file,f.name,"diagram",xpp::format(".{}",f.extension).c_str());
  if(!target)return {};
  if(const xpp::Result<> begun=xpp::begin_picture(s,f,*target,s.plot_export.color);!begun)return begun;
  if(!s.plot_file.writer)return {};
  draw_export_axes(s);
  int flag=0;
  for(xpp::DIAGRAM &point:s.diagram.points){
    xpp::DIAGRAM *d=&point;
    const xpp::DiagramPointType type=xpp::get_bif_type(d->ibr,d->ntot);
    if(d->ntot==1)flag=0;
    else flag=1;
    add_ps_point(s,*d,type,flag);
  }
  xpp::end_picture(s);
  set_normal_scale(s);
  return {};
}

} // namespace

xpp::Result<> write_info_out(xpp::Session &s, const std::optional<std::string> &file)
{
  return export_text(s,file,"Write all info","allinfo",[&](xpp::Writer &w){
    for(xpp::DIAGRAM &point:s.diagram.points){
      xpp::DIAGRAM *d=&point;
      const xpp::DiagramPointType type=xpp::get_bif_type(d->ibr,d->ntot);
      const xpp::AutoAxis icp1=d->icp1,icp2=d->icp2;
      auto parameters=diagram_parameters(*d);
      const double *par=parameters.data();
      const double per=d->per;
      const std::vector<double> &uhigh=d->uhi,&ulow=d->ulo;
      const double par1=par[icp1.index];
      const double par2=icp2.is_parameter()?par[icp2.index]:par1;
      std::string line=xpp::format("{} {} {} {:g} {:g} {:g} ",
              xpp::diagram_point_type_code(type),d->ibr,auto_calculation_name(d->flag2),par1,par2,per);
      for(int i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",uhigh[i]);
      for(int i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",ulow[i]);
      for(int i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} {:g} ",d->evr[i],d->evi[i]);
      line+='\n';
      w.print("{}",line);
    }
  });
}

void load_browser_with_branch(xpp::Session &s, int ibr,int pts,int pte)
{
   int i,j,pt;
  xpp::AutoAxis icp1;
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
  if(diagram_count(s.diagram)<2)return;
  j=0;
 for(xpp::DIAGRAM &point:s.diagram.points){
   xpp::DIAGRAM *d=&point;
    pt=abs(d->ntot);
    if((d->ibr==ibr) && (pt>=first) && (pt<=last)){
      icp1=d->icp1;
      auto parameters=diagram_parameters(*d);
      par=parameters.data();
      u0=d->u0.data();

      par1=par[icp1.index];
      s.data_store.col[0][j]=par1;
      for(i=0;i<s.model().node;i++)
	s.data_store.col[i+1][j]=u0[i];
      j++;
    }
 }
 s.data_store.rows=nrows;
 refresh_browser(s,nrows);
}
xpp::Result<> write_init_data_file(xpp::Session &s, const std::optional<std::string> &file)
{
  return export_text(s,file,"Write init data file","initdata",[&](xpp::Writer &w){
    for(xpp::DIAGRAM &point:s.diagram.points){
      xpp::DIAGRAM *d=&point;
      auto parameters=diagram_parameters(*d);
      std::string line=xpp::format("{:g} ",parameters.data()[d->icp1.index]);
      for(int i=0;i<s.model().node;i++)
        line+=xpp::format("{:g} ",d->u0[i]);
      line+='\n';
      w.print("{}",line);
    }
  });
}

xpp::Result<> write_pts(xpp::Session &s, const std::optional<std::string> &file)
{
  return export_text(s,file,"Write points","diagram",[&](xpp::Writer &w){
    for(xpp::DIAGRAM &point:s.diagram.points){
      xpp::DIAGRAM *d=&point;
      const xpp::DiagramPointType type=xpp::get_bif_type(d->ibr,d->ntot);
      const xpp::AutoAxis icp1=d->icp1,icp2=d->icp2;
      auto parameters=diagram_parameters(*d);
      const double *par=parameters.data();
      const double par1=par[icp1.index];
      const double par2=icp2.is_parameter()?par[icp2.index]:0;
      /* now we have to check is the diagram parameters correspond to the 
         current view 
      */
      if(check_plot_type(s,d->flag2,icp1,icp2)==1){
        double x,y1,y2;
        auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,d->per,d->uhi,d->ulo,d->ubar,d->norm);
        w.print("{:g} {:g} {:g} {} {} {}\n",
                x,y1,y2,xpp::diagram_point_type_code(type),abs(d->ibr),auto_calculation_name(d->flag2));
      }
    }
  });
}

/* the AUTO File menu's PDF/SVG entries (W53, issue #101): one
   function through the image_format.h registry in place of the former
   post_auto/svg_auto pair, with model-based default names and
   "*.pdf"/"*.svg" filter */
xpp::Result<> export_auto_picture(xpp::Session &s, int fmt, const std::optional<std::string> &file)
{
  return export_diagram(s,file,xpp::image_formats[fmt]);
}

void bound_diagram(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi)
{
  
  double x,y1,y2,par1,par2=0.0;
  if(diagram_count(s.diagram)<2)return;
  *xlo=1.e16;
  *ylo=*xlo;
  *xhi=-*xlo;
  *yhi=-*ylo;
  for(xpp::DIAGRAM &point:s.diagram.points){
    xpp::DIAGRAM *d=&point;
    par1=diagram_axis_value(*d,d->icp1);
    if(d->icp2.is_parameter())par2=diagram_axis_value(*d,d->icp2);
    auto_xy_plot(&s.auto_state.axes(),&x,&y1,&y2,par1,par2,d->per,d->uhi,d->ulo,d->ubar,d->norm);
    if(x<*xlo)*xlo=x;
    if(x>*xhi)*xhi=x;
    if(y2<*ylo)*ylo=y2;
    if(y1>*yhi)*yhi=y1;
  }
}
