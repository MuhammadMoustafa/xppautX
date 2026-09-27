#include "arrayplot.h"
#include "storage.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "array_print.h"

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
#include "lunch-new.h"
#include "load_eqn.h"

#include "xpplim.h"
#define READEM 1
#include "browse.h"
#include "xpp_io.h"
#include "integrate.h"
#include "pop_list.h"

/* the core's globals that have no header of their own */
int aplot_range_count=0;
int aplot_range;
std::string aplot_range_stem="rangearray";
int aplot_still=1,aplot_tag=0;
APLOT aplot;
int plot3d_auto_redraw=0;
FILE *ap_fp;





void set_up_aplot_range(void)
{ 
  static const char *n[]={"Basename","Still(1/0)","Tag(0/1)"};
  std::array<std::string, 3> values;
  int status;
  double *x;
 values[0] = xpp::format("{:.24}", aplot_range_stem);
 values[1] = xpp::format("{:d}", aplot_still);
 values[2] = xpp::format("{:d}", aplot_tag);
 static const int kinds[]={XPP_FIELD_FILE,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
 status=do_string_box_of(3,1,"Array range saving",n,values,28,kinds);
 if(status!=0){
   aplot_range_stem=values[0];
   aplot_still=atoi(values[1].c_str());
   aplot_tag=atoi(values[2].c_str());
 aplot_range=1;
 aplot_range_count=0;
 x=&data_store.current[0];
 do_range(x,0);
 }
}
void fit_aplot(void)
{
double zmax,zmin;
 scale_aplot(&aplot,&zmax,&zmin);
  aplot.zmin=zmin;
  aplot.zmax=zmax;
  xpp_ui.aplot_redraw();

}
void optimize_aplot(int *plist)
{
  int i0=plist[0]-1;
  int i1=plist[1]-1;
  int nr,ns;
  double zmax,zmin;
  int nrows=my_browser.maxrow;
  int ncol=i1+1-i0;
  if(ncol<2||nrows<2)return;
  make_my_aplot("Array!");

  aplot.index0=i0+1;
  aplot.name=uvar_names[i0];
  aplot.nacross=ncol;
  nr=201;
  if(nrows<nr)
    nr=nrows;
  aplot.ndown=nr;
  ns=nrows/nr;
  aplot.nskip=ns;
  aplot.ncskip=1;
  scale_aplot(&aplot,&zmax,&zmin);
  aplot.zmin=zmin;
  aplot.zmax=zmax;
  aplot.plotdef=1;
  xpp_ui.aplot_reset_axes();
  xpp_ui.aplot_redraw();
}
  
  
  
void scale_aplot(APLOT *ap, double *zmax, double *zmin)
{
  int i,j,ib,jb,row0=ap->nstart,col0=ap->index0;
  int nrows=my_browser.maxrow;
  double z;
  ib=col0;
  jb=row0;
  *zmax=my_browser.data[ib][jb];
  *zmin=*zmax;
  for(i=0;i<ap->nacross/ap->ncskip;i++){
      ib=col0+i*ap->ncskip;
      if(ib<=my_browser.maxcol){
	for(j=0;j<ap->ndown;j++){
	  jb=row0+ap->nskip*j;
	  if(jb<nrows&&jb>=0){
	    z=my_browser.data[ib][jb];
	    if(z<*zmin)*zmin=z;
	    if(z>*zmax)*zmax=z;
	  }
	}
      }
  }
  if(*zmin>=*zmax)
    *zmax=fabs(*zmin)+1+*zmin;
 
}

void init_my_aplot(void)
{
 APLOT *ap=&aplot;
 ap->height=400;
 ap->width=400;
 ap->zmin=0.0;
 ap->zmax=1.0;
 ap->alive=0;
 ap->plotdef=0;
 ap->index0=1;
 ap->indexn=0;
 ap->nacross=1;
 ap->ndown=50;
 ap->nstart=0;
 ap->nskip=8;
 ap->ncskip=1;
 ap->tstart=0.0;
 ap->tend=20.0;
 ap->filename="output.ps";
 ap->xtitle="index";
 ap->ytitle="time";
 ap->bottom="";
 ap->type=-1;
}


void print_aplot(APLOT *ap)
{
  double tlo,thi;
  int status,errflag;
  static const char *n[]={"Filename","Top label","Side label","Bottom label", 
	       "Render(-1,0,1,2)"};
   std::array<std::string, 5> values;
  int nrows=my_browser.maxrow;
  int row0=ap->nstart;
  int col0=ap->index0;
  int jb;
  if(nrows<=2)return;
  if(ap->plotdef==0||ap->nacross<2||ap->ndown<2)return;
  jb=row0;
  tlo=0.0;
  thi=20.0;
  if(jb>0&&jb<nrows)tlo=my_browser.data[0][jb];
  jb=row0+ap->nskip*(ap->ndown-1);
  if(jb>=nrows)jb=nrows-1;
  if(jb>=0)thi=my_browser.data[0][jb];
  values[0] = xpp::format("{:.24}", ap->filename);
  values[1] = xpp::format("{:.24}", ap->xtitle);
  values[2] = xpp::format("{:.24}", ap->ytitle);
    values[3] = xpp::format("{:.24}", ap->bottom);
  values[4] = xpp::format("{:d}", ap->type);
  static const int kinds[]={XPP_FIELD_FILE,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_INTEGER};
  status=do_string_box_of(5,1,"Print arrayplot",n,values,40,kinds);
 if(status!=0){
   ap->filename=values[0];
   ap->xtitle=values[1];
   ap->ytitle=values[2];
   ap->bottom=values[3];
   ap->type=atoi(values[4].c_str());
   if(ap->type<-1||ap->type>2)ap->type=-1;
   errflag=array_print(ap->filename.c_str(),ap->xtitle.c_str(),ap->ytitle.c_str(),ap->bottom.c_str(),
		       ap->nacross,
		       ap->ndown,col0,row0,ap->nskip,ap->ncskip,
		       nrows,my_browser.maxcol,
		      my_browser.data,ap->zmin,ap->zmax,tlo,thi,ap->type);
   if(errflag==-1)err_msg("Couldn't open file");
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

void dump_aplot(FILE *fp, int f)
{
  if(f==READEM){
    xpp::TokenReader r=xpp::TokenReader::attach(fp);
    if(!r.skip_line())return;
  }
  else
    xpp::print(fp,"# Array plot stuff\n");
  io_string(aplot.name,fp,f);
  io_int(&aplot.nacross ,fp,f,"NCols");
  io_int(&aplot.nstart ,fp,f,"Row 1");
  io_int(&aplot.ndown ,fp,f,"NRows");
  io_int(&aplot.nskip ,fp,f,"RowSkip");
  io_double(&aplot.zmin,fp,f,"Zmin");
  io_double(&aplot.zmax,fp,f,"Zmax");
}

int editaplot(APLOT *ap)
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
 values[7] = xpp::format("{:d}", plot3d_auto_redraw);
values[8] = xpp::format("{:d}", ap->ncskip);
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,
                           XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
 status=do_string_box_of(9,1,"Edit arrayplot",n,values,40,kinds);
 if(status!=0){
   find_variable(values[0].c_str(),&i);
   if(i>-1){
     ap->index0=i;
     ap->name=values[0].substr(0,XPP_NAME_MAX);
   }
   else
     {
       err_msg("No such columns");
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
    plot3d_auto_redraw=atoi(values[7].c_str());
    ap->plotdef=1;
    ap->ncskip=atoi(values[8].c_str());
    if(ap->ncskip<1)
      ap->ncskip=1;
    xpp_ui.aplot_reset_axes();
 }
   return 1;
}
void close_aplot_files(void)
{
  if(aplot_still==0){
    xpp::UniqueFile movie(ap_fp); /* closes it */
    ap_fp=nullptr;
  }
}






















