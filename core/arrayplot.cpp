#include "model.h"
#include "session.h"
#include "arrayplot.h"
#include "array_print.h"
#include "storage.h"
#include "xpp_ui.h"
#include "image_format.h"
#include "browse.h"
#include "xpp_files.h"
#include "xpp_error.h"
#include "xpp_job.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "command_def.h"
#include "command_registry.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
/*   routines for plotting arrays as functions of time  

     makes a window 
     of  N X M pixels 
     user specifies   starting variable  x0 and ending variable xn
                      starting time  ending time 
                      max var  min var

                                TITLE

                   [Kill]  [Edit]  [Print]  [Style] [Fit] [Range]
   ________________________________________________________
           1 |  |      tic marks              |  | N
            ---------------------------------------
     T0
          -                                                   MAX
          -                                                   ---
          -                                                   | |
                                                              | |
                                                              | |
                                                              | | 
                                                              | | 
                                                              | |  
          - 
          -                                                   MIN
     TN     ---------------------------------------                   

    and it creates a color plot 

*/

#include "browse.h"
#include "xpp_files.h"
#include "xpp_error.h"
#include "integrate.h"

void begin_array_range(xpp::Session &s, bool still, bool tag, std::string_view file)
{
  s.array_plot.range_stem=file;
  s.array_plot.still=still;
  s.array_plot.tag=tag;
  s.array_plot.range=1;
  s.array_plot.range_count=0;
  s.array_plot.save_cancelled=false;
}

void fit_aplot(xpp::Session &s)
{
double zmax,zmin;
 scale_aplot(s,&s.array_plot.plot,&zmax,&zmin);
  s.array_plot.plot.zmin=zmin;
  s.array_plot.plot.zmax=zmax;
  xpp::ui.aplot_redraw(s);

}
void optimize_aplot(xpp::Session &s, int *plist)
{
  int i0=plist[0]-1;
  int i1=plist[1]-1;
  int nr,ns;
  double zmax,zmin;
  int nrows=s.browser.view.maxrow;
  int ncol=i1+1-i0;
  if(ncol<2||nrows<2)return;
  make_my_aplot(s,"Array!");

  s.array_plot.plot.index0=i0+1;
  s.array_plot.plot.name=s.model().uvar_names[i0];
  s.array_plot.plot.nacross=ncol;
  nr=201;
  if(nrows<nr)
    nr=nrows;
  s.array_plot.plot.ndown=nr;
  ns=nrows/nr;
  s.array_plot.plot.nskip=ns;
  s.array_plot.plot.ncskip=1;
  scale_aplot(s,&s.array_plot.plot,&zmax,&zmin);
  s.array_plot.plot.zmin=zmin;
  s.array_plot.plot.zmax=zmax;
  s.array_plot.plot.plotdef=1;
  xpp::ui.aplot_reset_axes(s);
  xpp::ui.aplot_redraw(s);
}

void scale_aplot(const xpp::Session &s, APLOT *ap, double *zmax, double *zmin)
{
  int i,j,ib,jb,row0=ap->nstart,col0=ap->index0;
  int nrows=s.browser.view.maxrow;
  double z;
  ib=col0;
  jb=row0;
  *zmax=s.browser.view.data[ib][jb];
  *zmin=*zmax;
  for(i=0;i<ap->nacross/ap->ncskip;i++){
      ib=col0+i*ap->ncskip;
      if(ib<=s.browser.view.maxcol){
	for(j=0;j<ap->ndown;j++){
	  jb=row0+ap->nskip*j;
	  if(jb<nrows&&jb>=0){
	    z=s.browser.view.data[ib][jb];
	    if(z<*zmin)*zmin=z;
	    if(z>*zmax)*zmax=z;
	  }
	}
      }
  }
  if(*zmin>=*zmax)
    *zmax=fabs(*zmin)+1+*zmin;
 
}

void init_my_aplot(xpp::Session &s)
{
 APLOT *ap=&s.array_plot.plot;
 ap->height=400;
 ap->width=400;
 ap->zmin=0.0;
 ap->zmax=1.0;
 ap->alive=0;
 ap->plotdef=0;
 ap->index0=1;
 ap->nacross=1;
 ap->ndown=50;
 ap->nstart=0;
 ap->nskip=8;
 ap->ncskip=1;
 ap->filename.clear();
 ap->xtitle="index";
 ap->ytitle="time";
 ap->bottom="";
}

AplotTimes aplot_times(const xpp::Session &s, const APLOT &ap)
{
  const int nrows=s.browser.view.maxrow;
  const auto time_of=[&](int row){return static_cast<double>(s.browser.view.data[0][std::clamp(row,0,nrows-1)]);};
  return {time_of(ap.nstart),time_of(ap.nstart+ap.nskip*(ap.ndown-1))};
}

xpp::Result<> aplot_ready(const xpp::Session &s, const std::string &where, bool needs_data)
{
  const APLOT &ap=s.array_plot.plot;
  if(!ap.alive)return xpp::fail(where,"No array plot is open: open one first (array_plot.open)",xpp::command_place());
  if(ap.plotdef==0)return xpp::fail(where,"The array plot shows no columns yet: configure it first (array_plot.configure)",xpp::command_place());
  if(needs_data&&(s.browser.view.maxrow<=2||ap.nacross<2||ap.ndown<2))
    return xpp::fail(where,"The array plot has nothing to show: integrate first, and show at least 2 columns and 2 rows",xpp::command_place());
  return {};
}

xpp::Result<bool> write_aplot_pdf(xpp::Session &s, std::string_view file)
{
  APLOT *ap=&s.array_plot.plot;
  if(const auto ready=aplot_ready(s,"array_plot.export_pdf");!ready)return std::unexpected(ready.error());
  const AplotTimes times=aplot_times(s,*ap);
  ap->filename=file;
  const xpp::ArrayPicture picture{
    .filename=ap->filename.c_str(), .xtitle=ap->xtitle.c_str(),
    .ytitle=ap->ytitle.c_str(), .bottom=ap->bottom.c_str(),
    .nacross=ap->nacross, .ndown=ap->ndown, .col0=ap->index0, .row0=ap->nstart,
    .nskip=ap->nskip, .ncskip=ap->ncskip, .maxrow=s.browser.view.maxrow,
    .maxcol=s.browser.view.maxcol, .data=s.browser.view.data,
    .zmin=ap->zmin, .zmax=ap->zmax, .tlo=times.first, .thi=times.last};
  return xpp::array_print(s,picture);
}

/* splits an array plot's first column name at its trailing digits:
   "u10" gives the root "u" and 10; a name with no digits gives itself and
   0. */
std::string get_root(std::string_view s, int *num)
{
  size_t i=s.size();
  while(i>0&&std::isdigit(static_cast<unsigned char>(s[i-1])))
    i--;
  *num=0;
  if(i==0)
    return std::string(s);
  if(i<s.size())
    *num=std::atoi(std::string(s.substr(i)).c_str());
  return std::string(s.substr(0,i));
}


/* the Edit key: the form asked, then array_plot.configure given what it said */
void editaplot(xpp::Session &s, APLOT *ap)
{
  const char *n[]={"*0Column 1","NCols","Row 1","NRows","RowSkip",
  "Zmin","Zmax","Autoplot(0/1)","ColSkip"};
 std::array<std::string, 9> values;
 values[0] = ap->name;
 values[1] = xpp::format("{:d}", ap->nacross);
 values[2] = xpp::format("{:d}", ap->nstart);
 values[3] = xpp::format("{:d}", ap->ndown);
 values[4] = xpp::format("{:d}", ap->nskip);
 values[5] = xpp::format("{:g}", ap->zmin);
 values[6] = xpp::format("{:g}", ap->zmax);
 values[7] = xpp::format("{:d}", s.array_plot.auto_redraw);
values[8] = xpp::format("{:d}", ap->ncskip);
 static const int kinds[]={XPP_FIELD_NAME_IN(0),xpp::XPP_FIELD_INTEGER,xpp::XPP_FIELD_INTEGER,xpp::XPP_FIELD_INTEGER,
                           xpp::XPP_FIELD_INTEGER,xpp::XPP_FIELD_NUMBER,xpp::XPP_FIELD_NUMBER,xpp::XPP_FIELD_INTEGER,xpp::XPP_FIELD_INTEGER};
 if(xpp::do_string_box_of(9,1,"Edit arrayplot",n,values,kinds)==0)return;
 /* every field read before any is set */
 xpp::TypedFields f(s);
 const int nacross=f.integer(n[1],values[1]),nstart=f.integer(n[2],values[2]),ndown=f.integer(n[3],values[3]);
 const int nskip=f.integer(n[4],values[4]);
 const double zmin=f.real(n[5],values[5]),zmax=f.real(n[6],values[6]);
 const int auto_redraw=f.integer(n[7],values[7]),ncskip=f.integer(n[8],values[8]);
 if(f.error()){
   xpp::command_error("array plot",*f.error());
   return;
 }
 xpp::ok_or_show(xpp::command::run_button(s,xpp::command::ResultObject().text("cmd","array_plot.configure").text("first_column",values[0])
   .integer("columns",nacross).integer("first_row",nstart).integer("rows",ndown).integer("row_skip",nskip).real("z_min",zmin).real("z_max",zmax)
   .boolean("auto_redraw",auto_redraw!=0).integer("column_skip",ncskip).done(),xpp::command_place()));
}
void close_aplot_files(xpp::Session &s, bool complete)
{
  if(s.array_plot.still==0){
    if(s.array_plot.movie){
      if (!complete || s.array_plot.save_cancelled || xpp::job::cancelled()) xpp::abort_save(s.array_plot.movie);
      else {
        xpp::image_formats[xpp::IMAGE_FORMAT_GIF].finish_movie(s.array_plot.movie);
        xpp::ok_or_show(xpp::commit_save(s.array_plot.movie));
      }
    }
  }
}
