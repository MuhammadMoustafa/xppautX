#include "diagram_export.h"
#include "autox_evd.h"
#include "autox_nox.h"
#include "autox_state.h"
#include "axes2.h"
#include "diagram.h"
#include "graphics.h"
#include "image_format.h"
#include "picture.h"
#include "session.h"
#include "xpp_ui.h"
#include <cstdlib>
#include <span>
#include <string>

namespace xpp {

namespace {

/* the diagram's axes in a PDF or SVG export, the picture begin_picture began */
void draw_export_axes(Session &s)
{
 set_scale(s,s.autox_state->axes().xmin,s.autox_state->axes().ymin,s.autox_state->axes().xmax,s.autox_state->axes().ymax);
 std::string xlabel,ylabel;
 get_autox_str(xpp::autox_parts(s),s.autox_state->axes(),xlabel,ylabel);
 Box_axis(s,s.autox_state->axes().xmin,s.autox_state->axes().xmax,s.autox_state->axes().ymin,s.autox_state->axes().ymax,xlabel.c_str(),ylabel.c_str(),0);
}

/* the stored point d into the picture being written (type its get_bif_type, flag 0 for a branch's first point) */
void add_ps_point(xpp::Session &s, const DIAGRAM &d, DiagramPointType type, int flg)
{
  const auto parameters=diagram_parameters(d);
  const double *par=parameters.data();
  const double per=d.per,a=d.norm;
  const std::span<const double> uhigh=d.uhi,ulow=d.ulo,ubar=d.ubar;
  const xpp::AutoxAxis icp1=d.icp1,icp2=d.icp2;
  const AutoxCalculation flag2=d.flag2;
  double x,y1,y2,par1,par2=0;
  DiagramPointType type1=type;
  par1=par[icp1.index];
  if(icp2.is_parameter())par2=par[icp2.index];
  autox_xy_plot(&s.autox_state->axes(),&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a);
  if(flg==0){
    s.autox_state->bifur.lastx=x;
    s.autox_state->bifur.lasty=y1;
  }
  if(flag2==no_calculation&&s.autox_state->axes().plot==AutoxProjection::two_parameter)
    {
  
       return;
     }
  if(flag2!=no_calculation&&s.autox_state->axes().plot!=AutoxProjection::two_parameter){
  
    return;
  }

  if((flag2!=no_calculation)&&(s.autox_state->axes().plot==AutoxProjection::two_parameter))
   type1=STABLE_EQ;
  /* a stable equilibrium is drawn thick, an unstable one dashed, in the options' colors, a two-parameter curve in its kind's */
  const auto color_of=[&](const xpp::Color &option){return flag2!=no_calculation?two_parameter_color(flag2):option;};
  xpp::LineStyle line_style;
  line_style.width=2;
  xpp::LineStyle dots;
  dots.draw=xpp::LineDraw::points;
  dots.marker_size=3;
  switch(type1){
  case STABLE_EQ:
  case UNSTABLE_EQ:
    if(s.autox_state->axes().plot==AutoxProjection::period||s.autox_state->axes().plot==AutoxProjection::frequency)break;
    if(!icp1.is_parameter(s.autox_state->axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.autox_state->axes().icp2))break;
    if(type1==UNSTABLE_EQ){
      line_style.width=1;
      line_style.dash=xpp::LineDash::dashed;
    }
    xpp::set_pen(s,color_of(type1==STABLE_EQ?s.colors.stable_equilibrium:s.colors.unstable_equilibrium),line_style);
    line_abs(s,static_cast<float>(x),static_cast<float>(y1),static_cast<float>(s.autox_state->bifur.lastx),static_cast<float>(s.autox_state->bifur.lasty));
    break;
  case UNSTABLE_PERIODIC:
  case STABLE_PERIODIC:
    if(!icp1.is_parameter(s.autox_state->axes().icp1))break;
    if(flag2!=no_calculation&&!icp2.is_parameter(s.autox_state->axes().icp2))break;
    /* a stable periodic orbit's extremes are filled circles, an unstable one's open */
    xpp::set_pen(s,type1==STABLE_PERIODIC?s.colors.stable_periodic:s.colors.unstable_periodic,dots);
    s.plot_file->pen.hollow=type1==UNSTABLE_PERIODIC;
    point_abs(s,static_cast<float>(x),static_cast<float>(y1));
    point_abs(s,static_cast<float>(x),static_cast<float>(y2));
    break;
  }

  s.autox_state->bifur.lastx=x;
  s.autox_state->bifur.lasty=y1;
}

} // namespace

/* the autoX File menu's PDF/SVG entries (W53, issue #101): the diagram as a picture in the picture.h registry's format fmt, in the
   file given or asked for, with model-based default names and "*.pdf"/"*.svg" filter */
Result<> export_autox_picture(Session &s, int fmt, const std::optional<std::string> &file, std::optional<int> view)
{
  const ImageFormat &f=image_formats[fmt];
  if(const Result<> points=diagram_need_points(xpp::autox_parts(s));!points)return points;
  const auto target=diagram_export_target(xpp::autox_parts(s),file,f.name,"diagram",(std::string(".")+f.extension).c_str());
  if(!target)return {};
  if(const Result<> begun=begin_picture(s,f,*target);!begun)return begun;
  if(!s.plot_file->writer)return {};
  if(view)autox_activate_view(xpp::autox_parts(s),*view);
  draw_export_axes(s);
  for(DIAGRAM &point:s.diagram->points){
    const DiagramPointType type=get_bif_type(point.ibr,point.ntot);
    add_ps_point(s,point,type,std::abs(point.ntot)==1?0:1); /* a stable first point is signed -1 (W858, #939) */
  }
  ok_or_show(end_picture(s));
  set_normal_scale(s);
  return {};
}

} // namespace xpp
