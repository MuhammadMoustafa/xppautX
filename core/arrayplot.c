#include "arrayplot.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "array_print.h"

#include <stdlib.h> 
#include <string.h>
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

#include <stdio.h>
#include <math.h>
#include <math.h>
#ifndef WCTYPE
#include <ctype.h>
#else
#include <wctype.h>
#endif
#include "xpplim.h"
#define READEM 1
#include "browse.h"
#include "xpp_io.h"
#define FIRSTCOLOR 30
#define FIX_MIN_SIZE 2
/*extern char this_file[100];*/
extern char this_file[XPP_MAX_NAME];
double atof();
extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern BROWSER my_browser;
int aplot_range_count=0;
char aplot_range_stem[256]="rangearray";
int aplot_still=1,aplot_tag=0;
APLOT aplot;
int plot3d_auto_redraw=0;
FILE *ap_fp;
int do_range(double *, int);
extern double MyData[MAXODE];





void set_up_aplot_range()
{ 
  static char *n[]={"Basename","Still(1/0)","Tag(0/1)"};
  char values[3][MAX_LEN_SBOX];
  int status;
  double *x;
 snprintf(values[0],sizeof(values[0]),"%.24s",aplot_range_stem);
 XPP_SPRINTF(values[1],"%d",aplot_still);
 XPP_SPRINTF(values[2],"%d",aplot_tag);
 status=do_string_box(3,3,1,"Array range saving",n,values,28); 
 if(status!=0){
   XPP_SPRINTF(aplot_range_stem,"%s",values[0]);
   aplot_still=atoi(values[1]);
   aplot_tag=atoi(values[2]);
 aplot_range=1;
 aplot_range_count=0;
 x=&MyData[0];
 do_range(x,0);
 }
}
void fit_aplot()
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
  XPP_STRCPY(aplot.name,uvar_names[i0]);
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
  
  
  
void scale_aplot(ap,zmax,zmin)
APLOT *ap;
double *zmax,*zmin;
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

void init_arrayplot(ap)
APLOT *ap;
{
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
 XPP_STRCPY(ap->filename,"output.ps");
 XPP_STRCPY(ap->xtitle,"index");
 XPP_STRCPY(ap->ytitle,"time");
 XPP_STRCPY(ap->bottom,"");
 ap->type=-1;
}


void init_my_aplot()
{
 init_arrayplot(&aplot);
}


void print_aplot(ap)
     APLOT *ap;
{
  double tlo,thi;
  int status,errflag;
  static char *n[]={"Filename","Top label","Side label","Bottom label", 
	       "Render(-1,0,1,2)"};
   char values[5][MAX_LEN_SBOX];
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
  snprintf(values[0],sizeof(values[0]),"%.24s",ap->filename);
  snprintf(values[1],sizeof(values[1]),"%.24s",ap->xtitle);
  snprintf(values[2],sizeof(values[2]),"%.24s",ap->ytitle);
    snprintf(values[3],sizeof(values[3]),"%.24s",ap->bottom);
  XPP_SPRINTF(values[4],"%d",ap->type);
  status=do_string_box(5,5,1,"Print arrayplot",n,values,40);
 if(status!=0){
   XPP_STRCPY(ap->filename,values[0]);
   XPP_STRCPY(ap->xtitle,values[1]);
   XPP_STRCPY(ap->ytitle,values[2]);
   XPP_STRCPY(ap->bottom,values[3]);
   ap->type=atoi(values[4]);
   if(ap->type<-1||ap->type>2)ap->type=-1;
   errflag=array_print(ap->filename,ap->xtitle,ap->ytitle,ap->bottom,
		       ap->nacross,
		       ap->ndown,col0,row0,ap->nskip,ap->ncskip,
		       nrows,my_browser.maxcol,
		      my_browser.data,ap->zmin,ap->zmax,tlo,thi,ap->type);
   if(errflag==-1)err_msg("Couldn't open file");
 }
}

void edit_aplot()
{
  editaplot(&aplot);
}

void get_root(s,sroot,num)
     char *s,*sroot;
     int *num;
{
  int n=strlen(s);
    int i=n-1,j;

  char me[100];
  *num=0;
  while(1){
   
    if(!isdigit(s[i])){
   
      break;
    }
    i--;
    if(i<0)break;
  }
  /* sroot is a pointer here (get_root's one caller, ui_json.cpp, passes
     its own char sroot[100]): XPP_STRCPY's sizeof(dst) trick does not
     apply, so pass that real size directly. */
  if(i<0)xpp_strlcpy(sroot,s,100);
  else {
    for(j=0;j<=i;j++)
      sroot[j]=s[j];
    sroot[i+1]=0;
  }
  if(i>=0&&i<n){
    for(j=i+1;j<n;j++)
      me[j-i-1]=s[j];
    me[n-i]=0;
   /* plintf(" i=%d me=%s sroot=%s \n",i,me,sroot); */  
    *num=atoi(me);
  }
}
  
void dump_aplot(fp,f)
     FILE *fp;
     int f;
{
  char bob[256];
  if(f==READEM){
    if(fgets(bob,255,fp)==NULL)return;
  }
  else
    fprintf(fp,"# Array plot stuff\n");
  io_string(aplot.name,sizeof(aplot.name),fp,f);
  io_int(&aplot.nacross ,fp,f,"NCols");
    io_int(&aplot.nstart ,fp,f,"Row 1");
  io_int(&aplot.ndown ,fp,f,"NRows");
  io_int(&aplot.nskip ,fp,f,"RowSkip");
  io_double(&aplot.zmin,fp,f,"Zmin");
    io_double(&aplot.zmax,fp,f,"Zmax");

}

int editaplot(ap)
     APLOT *ap;
{
 int i,status;
 double zmax,zmin;
  char *n[]={"*0Column 1","NCols","Row 1","NRows","RowSkip",
  "Zmin","Zmax","Autoplot(0/1)","ColSkip"};
 char values[9][MAX_LEN_SBOX];
 XPP_SPRINTF(values[0],"%s",ap->name);
 XPP_SPRINTF(values[1],"%d",ap->nacross);
 XPP_SPRINTF(values[2],"%d",ap->nstart);
 XPP_SPRINTF(values[3],"%d",ap->ndown);
 XPP_SPRINTF(values[4],"%d",ap->nskip);
 XPP_SPRINTF(values[5],"%g",ap->zmin);
 XPP_SPRINTF(values[6],"%g",ap->zmax);
 XPP_SPRINTF(values[7],"%d",plot3d_auto_redraw);
XPP_SPRINTF(values[8],"%d",ap->ncskip);
 status=do_string_box(9,9,1,"Edit arrayplot",n,values,40);
 if(status!=0){
   find_variable(values[0],&i);
   if(i>-1){
     ap->index0=i;
     snprintf(ap->name,sizeof(ap->name),"%.*s",XPP_NAME_MAX,values[0]);
   }
   else
     {
       err_msg("No such columns");
       ap->plotdef=0;
       return 0;
     }
    zmax=atof(values[6]);
    zmin=atof(values[5]);
    if(zmin<zmax){
      ap->zmin=zmin;
      ap->zmax=zmax;
    }
    ap->nacross=atoi(values[1]);
    ap->nstart=atoi(values[2]);
    ap->ndown=atoi(values[3]);
    ap->nskip=atoi(values[4]);
    plot3d_auto_redraw=atoi(values[7]);
    ap->plotdef=1;
    ap->ncskip=atoi(values[8]);
    if(ap->ncskip<1)
      ap->ncskip=1;
    xpp_ui.aplot_reset_axes();
 }
   return 1;
}
void close_aplot_files()
{
  if(aplot_still==0)
    fclose(ap_fp);
}






















