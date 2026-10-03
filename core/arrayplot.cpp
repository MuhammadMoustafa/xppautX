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

void set_up_aplot_range(xpp::Session &s)
{ 
  static const char *const n[]={"Still(1/0)","Tag(0/1)"};
  std::array<std::string, 2> values;
  int status;
  double *x;
 values[0] = xpp::format("{:d}", s.array_plot.still);
 values[1] = xpp::format("{:d}", s.array_plot.tag);
 static const int kinds[]={xpp::XPP_FIELD_INTEGER,xpp::XPP_FIELD_INTEGER};
 status=xpp::do_string_box_of(2,1,"Array range saving",n,values,kinds);
 if(status!=0){
   std::string root=xpp::files::output_name(s.model().this_file,".gif","array");
   if(!xpp::file_selector("Save array frames",root,"*.gif"))return;
   s.array_plot.range_stem=root;
   s.array_plot.still=atoi(values[0].c_str());
   s.array_plot.tag=atoi(values[1].c_str());
 s.array_plot.range=1;
 s.array_plot.range_count=0;
 s.array_plot.save_cancelled=false;
 x=&s.data_store.current[0];
 xpp::do_range(s,x,0);
 }
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
 ap->type=xpp::ARRAY_GREYSCALE;
}

void print_aplot(const xpp::Session &s, APLOT *ap)
{
  double tlo,thi;
  int status;
  static const char *const n[]={"Top label","Side label","Bottom label",
	       "Render(-1,0,1,2)"};
   std::array<std::string, 4> values;
  int nrows=s.browser.view.maxrow;
  int row0=ap->nstart;
  int col0=ap->index0;
  int jb;
  if(!xpp::save_ready(nrows>2&&ap->plotdef!=0&&ap->nacross>=2&&ap->ndown>=2))return;
  jb=row0;
  tlo=0.0;
  thi=20.0;
  if(jb>0&&jb<nrows)tlo=s.browser.view.data[0][jb];
  jb=row0+ap->nskip*(ap->ndown-1);
  if(jb>=nrows)jb=nrows-1;
  if(jb>=0)thi=s.browser.view.data[0][jb];
  values[0] = ap->xtitle;
  values[1] = ap->ytitle;
    values[2] = ap->bottom;
  values[3] = xpp::format("{:d}", ap->type);
  static const int kinds[]={xpp::XPP_FIELD_TEXT,xpp::XPP_FIELD_TEXT,xpp::XPP_FIELD_TEXT,xpp::XPP_FIELD_INTEGER};
  status=xpp::do_string_box_of(4,1,"Print arrayplot",n,values,kinds);
 if(status!=0){
   ap->xtitle=values[0];
   ap->ytitle=values[1];
   ap->bottom=values[2];
   ap->type=atoi(values[3].c_str());
   if(ap->type<xpp::ARRAY_GREYSCALE||ap->type>xpp::ARRAY_PERIODIC){xpp::command_error("Print arrayplot","Render must be -1, 0, 1 or 2");return;}
   if(ap->filename.empty())ap->filename=xpp::files::output_name(s.model().this_file,".ps","array");
   if(!xpp::file_selector("Save array plot",ap->filename,"*.ps"))return;
   const xpp::ArrayPicture picture{
     .filename=ap->filename.c_str(), .xtitle=ap->xtitle.c_str(),
     .ytitle=ap->ytitle.c_str(), .bottom=ap->bottom.c_str(),
     .nacross=ap->nacross, .ndown=ap->ndown, .col0=col0, .row0=row0,
     .nskip=ap->nskip, .ncskip=ap->ncskip, .maxrow=nrows,
     .maxcol=s.browser.view.maxcol, .data=s.browser.view.data,
     .zmin=ap->zmin, .zmax=ap->zmax, .tlo=tlo, .thi=thi, .type=ap->type};
   xpp::ok_or_show(xpp::image_formats[xpp::IMAGE_FORMAT_PS].array(picture));
 }
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


int editaplot(xpp::Session &s, APLOT *ap)
{
 int i,status;
 double zmax,zmin;
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
 status=xpp::do_string_box_of(9,1,"Edit arrayplot",n,values,kinds);
 if(status!=0){
   find_variable(s,values[0].c_str(),&i);
   if(i>-1){
     ap->index0=i;
     ap->name=values[0];
   }
   else
     {
       xpp::command_error("array plot", "No such columns");
       ap->plotdef=0;
       return 0;
     }
    zmax=atof(values[6].c_str());
    zmin=atof(values[5].c_str());
    if(zmin<zmax){
      ap->zmin=zmin;
      ap->zmax=zmax;
    }
    ap->nacross=atoi(values[1].c_str());
    ap->nstart=atoi(values[2].c_str());
    ap->ndown=atoi(values[3].c_str());
    ap->nskip=atoi(values[4].c_str());
    s.array_plot.auto_redraw=atoi(values[7].c_str());
    ap->plotdef=1;
    ap->ncskip=atoi(values[8].c_str());
    if(ap->ncskip<1)
      ap->ncskip=1;
    xpp::ui.aplot_reset_axes(s);
 }
   return 1;
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
