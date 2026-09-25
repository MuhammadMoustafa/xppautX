#include "graf_par.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "arrayplot.h"
#include "xpp_globals.h"
#include "marks_data.h"

#include "integrate.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "menus.h"

#include "menudrive.h"

#include "graphics.h"
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "xpplim.h"
#include "struct.h"
#include "browse.h"
#include "mykeydef.h"
#include "nullcline.h"
#include "axes2.h"
#include "my_ps.h"
#include "my_svg.h"
#include "load_eqn.h"
#include <libgen.h>
#include "xpp_io.h"
#include "many_pops.h"

NCLINE nclines[MAXNCLINE];
extern int storind;
extern int PS_FONTSIZE;
extern int PS_Port;
/*extern char PS_FONT[100];*/
extern char PS_FONT[100]; /* my_ps.c */
extern double PS_LW;
extern BROWSER my_browser;
extern double x_3d[2],y_3d[2],z_3d[2];
/*Default is now color*/



#define SPER 3
#define UPER 4
#define SEQ 1
#define UEQ 2

#define lsSEQ 0
#define lsUEQ 1
#define lsSPER 8
#define lsUPER 9



MOV3D mov3d = { "theta","N",45,45,7};



BD my_bd;
XppFrozenCurves frozen_curves;
XppPlotExport plot_export = {"", 1};

extern int DLeft,DRight,DTop,DBottom,VTic,HTic,VChar,HChar;

extern double T0,TEND;
extern float **storage;

double FreezeKeyX,FreezeKeyY;
int FreezeKeyFlag;
int CurrentCurve=0;
extern char this_file[XPP_MAX_NAME];
extern char this_internset[XPP_MAX_NAME];

extern int PltFmtFlag;
extern char uvar_names[MAXODE][XPP_NAME_MAX+1];

extern const char *no_hint[],*wind_hint[],*view_hint[],*frz_hint[];
extern const char *graf_hint[], *cmap_hint[]; 

int colorline[]={0,20,21,22,23,24,25,26,27,28,29,0};
const char *color_names[]={"WHITE","RED","REDORANGE","ORANGE","YELLOWORANGE",
                    "YELLOW","YELLOWGREEN","GREEN","BLUEGREEN",
		      "BLUE","PURPLE","BLACK"};


void change_view_com(int com)
{
 
 if(com==2){
   make_my_aplot("Array!");
   edit_aplot(); 
   return;
 }
 if(com==3){
   new_vcr();
   return;
 }

  plot_windows.current->grtype=5*com; 
 if(plot_windows.current->grtype<5)get_2d_view(CurrentCurve);
 else get_3d_view(CurrentCurve);
 check_flags();
 redraw_the_graph();
} 

 

void check_flags()
{
  if(plot_windows.current->grtype>4)plot_windows.current->ThreeDFlag=1;
  else plot_windows.current->ThreeDFlag=0;
  if((plot_windows.current->xv[0]==0)||(plot_windows.current->yv[0]==0)||
     ((plot_windows.current->zv[0]==0)&&(plot_windows.current->ThreeDFlag==1)))plot_windows.current->TimeFlag=1;
  else plot_windows.current->TimeFlag=0;
}
  

void get_2d_view(int ind)
{
 static const char *n[]={"*0X-axis","*0Y-axis","Xmin", "Ymin",
		   "Xmax", "Ymax", "Xlabel","Ylabel"};
 char values[8][MAX_LEN_SBOX];
 int  status,i; 
 int i1=plot_windows.current->xv[ind],i2=plot_windows.current->yv[ind];
 char n1[XPP_NAME_MAX+1],n2[XPP_NAME_MAX+1];
 ind_to_sym(i1,n1);
 ind_to_sym(i2,n2);
 XPP_SPRINTF(values[0],"%s",n1);
 XPP_SPRINTF(values[1],"%s",n2);
 XPP_SPRINTF(values[2],"%g",plot_windows.current->xmin);
 XPP_SPRINTF(values[3],"%g",plot_windows.current->ymin);
 XPP_SPRINTF(values[4],"%g",plot_windows.current->xmax);
 XPP_SPRINTF(values[5],"%g",plot_windows.current->ymax);
 snprintf(values[6],sizeof(values[6]),"%s",plot_windows.current->xlabel);
 snprintf(values[7],sizeof(values[7]),"%s",plot_windows.current->ylabel);
 plot_windows.current->ThreeDFlag=0;
 static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(8,4,2,"2D View",n,values,31,kinds);
 if(status!=0){
		/*  get variable names  */
             find_variable(values[0],&i);
              if(i>-1)
		plot_windows.current->xv[ind]=i;
	     find_variable(values[1],&i);
              if(i>-1)
		plot_windows.current->yv[ind]=i;

	      plot_windows.current->xmin=atof(values[2]);
	      plot_windows.current->ymin=atof(values[3]);
	      plot_windows.current->xmax=atof(values[4]);
	      plot_windows.current->ymax=atof(values[5]);
	      plot_windows.current->xlo=plot_windows.current->xmin;
	      plot_windows.current->ylo=plot_windows.current->ymin;
	      plot_windows.current->xhi=plot_windows.current->xmax;
	      plot_windows.current->yhi=plot_windows.current->ymax;
	     XPP_SPRINTF(plot_windows.current->xlabel,"%s",values[6]);
	     XPP_SPRINTF(plot_windows.current->ylabel,"%s",values[7]);
	      check_windows();
/*	      plintf(" x=%d y=%d xlo=%f ylo=%f xhi=%f yhi=%f \n",
		     MyGraph->xv[ind],MyGraph->yv[ind],MyGraph->xlo,
		     MyGraph->ylo,MyGraph->xhi,MyGraph->yhi);
*/
		     
	      }
}
	      

void axes_opts()
{
  static const char *n[]={"X-origin","Y-origin","Z-origin",
		   "X-org(1=on)","Y-org(1=on)","Z-org(1=on",
		    "PSFontSize"};
  char values[7][MAX_LEN_SBOX];
  int status;
  XPP_SPRINTF(values[0],"%g",plot_windows.current->xorg);
  XPP_SPRINTF(values[1],"%g",plot_windows.current->yorg);
  XPP_SPRINTF(values[2],"%g",plot_windows.current->zorg);
  XPP_SPRINTF(values[3],"%d",plot_windows.current->xorgflag);
  XPP_SPRINTF(values[4],"%d",plot_windows.current->yorgflag);
  XPP_SPRINTF(values[5],"%d",plot_windows.current->zorgflag);
  XPP_SPRINTF(values[6],"%d",PS_FONTSIZE);
  static const int kinds[]={XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                            XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER};
  status=do_string_box_of(7,7,1,"Axes options",n,values,25,kinds);
 if(status!=0){
   plot_windows.current->xorg=atof(values[0]);
   plot_windows.current->yorg=atof(values[1]);
   plot_windows.current->zorg=atof(values[2]);
   plot_windows.current->xorgflag=atoi(values[3]);
   plot_windows.current->yorgflag=atoi(values[4]);
   plot_windows.current->zorgflag=atoi(values[5]);
   PS_FONTSIZE=atoi(values[6]);
   redraw_the_graph();
 }
   
}


void get_3d_view(int ind)
{
 static const char *n[]={"*0X-axis","*0Y-axis", "*0Z-axis",
		   "Xmin", "Xmax", "Ymin",
		   "Ymax", "Zmin","Zmax",
		   "XLo", "XHi", "YLo", "YHi","Xlabel","Ylabel","Zlabel"};
 char values[16][MAX_LEN_SBOX];
 int  status,i,i1=plot_windows.current->xv[ind],i2=plot_windows.current->yv[ind],i3=plot_windows.current->zv[ind];
 char n1[XPP_NAME_MAX+1],n2[XPP_NAME_MAX+1],n3[XPP_NAME_MAX+1];
 ind_to_sym(i1,n1);
 ind_to_sym(i2,n2);
 ind_to_sym(i3,n3);
 XPP_SPRINTF(values[0],"%s",n1);
 XPP_SPRINTF(values[1],"%s",n2);
 XPP_SPRINTF(values[2],"%s",n3);
 XPP_SPRINTF(values[3],"%g",plot_windows.current->xmin);
 XPP_SPRINTF(values[5],"%g",plot_windows.current->ymin);
 XPP_SPRINTF(values[7],"%g",plot_windows.current->zmin);
 XPP_SPRINTF(values[4],"%g",plot_windows.current->xmax);
 XPP_SPRINTF(values[6],"%g",plot_windows.current->ymax);
 XPP_SPRINTF(values[8],"%g",plot_windows.current->zmax);
 XPP_SPRINTF(values[9],"%g",plot_windows.current->xlo);
 XPP_SPRINTF(values[11],"%g",plot_windows.current->ylo);
 XPP_SPRINTF(values[10],"%g",plot_windows.current->xhi);
 XPP_SPRINTF(values[12],"%g",plot_windows.current->yhi);
 snprintf(values[13],sizeof(values[13]),"%s",plot_windows.current->xlabel);
 snprintf(values[14],sizeof(values[14]),"%s",plot_windows.current->ylabel);
 snprintf(values[15],sizeof(values[15]),"%s",plot_windows.current->zlabel);
 plot_windows.current->ThreeDFlag=1;
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),
                           XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(16,6,3,"3D View",n,values,31,kinds);
 if(status!=0){
		/*  get variable names  */
              find_variable(values[0],&i);
 	      if(i>-1)
		plot_windows.current->xv[ind]=i;
              find_variable(values[1],&i);
              if(i>-1)
		plot_windows.current->yv[ind]=i;
              find_variable(values[2],&i);
  		if(i>-1)
		  plot_windows.current->zv[ind]=i;
	      XPP_SPRINTF(plot_windows.current->xlabel,"%s",values[13]);
	      XPP_SPRINTF(plot_windows.current->ylabel,"%s",values[14]);
	      XPP_SPRINTF(plot_windows.current->zlabel,"%s",values[15]);


	      plot_windows.current->xmin=atof(values[3]);
	      plot_windows.current->ymin=atof(values[5]);
	      plot_windows.current->zmin=atof(values[7]);
	      plot_windows.current->xmax=atof(values[4]);
	      plot_windows.current->ymax=atof(values[6]);
	      plot_windows.current->zmax=atof(values[8]);
	      plot_windows.current->xlo=atof(values[9]);
	      plot_windows.current->ylo=atof(values[11]);
	      plot_windows.current->xhi=atof(values[10]);
	      plot_windows.current->yhi=atof(values[12]);
              check_windows();
	/*      plintf("%f %f %f %f %f %f \n %f %f %f %f",
		     MyGraph->xmin,MyGraph->xmax,
		     MyGraph->ymin,MyGraph->ymax,
		     MyGraph->zmin,MyGraph->zmax,
		     MyGraph->xlo,MyGraph->xhi,
		     MyGraph->ylo,MyGraph->yhi);
*/

	      }
}
	      


void pretty(double *x1, double *x2)  /* this was always pretty ugly */
{
/* if(fabs(*x1-*x2)<1.e-12)
 *x2=*x1+max(.1*fabs(*x2),1.0); */
}

void corner_cube(double *xlo, double *xhi, double *ylo, double *yhi)
{
 float x,y;
 float x1,x2,y1,y2;
 threedproj(-1.,-1.,-1.,&x,&y);
 x1=x;
 x2=x;
 y1=y;
 y2=y;
 threedproj(-1.,-1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(-1.,1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(-1.,1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(1.,-1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(1.,-1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(1.,1.,1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 threedproj(1.,1.,-1.,&x,&y);
 if(x<x1)x1=x;
 if(x>x2)x2=x;
 if(y<y1)y1=y;
 if(y>y2)y2=y;
 *xlo=x1;
 *ylo=y1;
 *xhi=x2;
 *yhi=y2;
}
 
 
 
void default_window()
{
 	if(plot_windows.current->ThreeDFlag){
	      plot_windows.current->xmax=x_3d[1];
    	      plot_windows.current->ymax=y_3d[1];
    	      plot_windows.current->zmax=z_3d[1];
              plot_windows.current->xmin=x_3d[0];
              plot_windows.current->ymin=y_3d[0];
              plot_windows.current->zmin=z_3d[0];  
	      
	      pretty(&(plot_windows.current->ymin),&(plot_windows.current->ymax));
	      pretty(&(plot_windows.current->xmin),&(plot_windows.current->xmax));
	      pretty(&(plot_windows.current->zmin),&(plot_windows.current->zmax));
	      corner_cube(&(plot_windows.current->xlo),&(plot_windows.current->xhi),&(plot_windows.current->ylo),&(plot_windows.current->yhi));
	      pretty(&(plot_windows.current->xlo),&(plot_windows.current->xhi));
	      pretty(&(plot_windows.current->ylo),&(plot_windows.current->yhi));
	      check_windows(); 
	}
	else  
    	{
	      plot_windows.current->xmax=x_3d[1];
    	      plot_windows.current->ymax=y_3d[1];
    	      
              plot_windows.current->xmin=x_3d[0];
              plot_windows.current->ymin=y_3d[0];
	      
	      pretty(&(plot_windows.current->ymin),&(plot_windows.current->ymax)); 
	      pretty(&(plot_windows.current->xmin),&(plot_windows.current->xmax));
	      plot_windows.current->xlo=plot_windows.current->xmin;
	      plot_windows.current->ylo=plot_windows.current->ymin;
	      plot_windows.current->xhi=plot_windows.current->xmax;
	      plot_windows.current->yhi=plot_windows.current->ymax;
	      check_windows();
	}
	
	redraw_the_graph();
             
}


void fit_window()
{
  double Mx=-1.e25,My=-1.e25,Mz=-1.e25,mx=-Mx,my=-My,mz=-Mz;
  int i,n=plot_windows.current->nvars;
  if(storind<2)return;
  if(plot_windows.current->ThreeDFlag){
    for(i=0;i<n;i++){
      
      get_max(plot_windows.current->xv[i],&(plot_windows.current->xmin),&(plot_windows.current->xmax));
      Mx=lmax(plot_windows.current->xmax,Mx);
      mx=-lmax(-plot_windows.current->xmin,-mx);
      
      get_max(plot_windows.current->yv[i],&(plot_windows.current->ymin),&(plot_windows.current->ymax));
      My=lmax(plot_windows.current->ymax,My);
      my=-lmax(-plot_windows.current->ymin,-my);
      
      get_max(plot_windows.current->zv[i],&(plot_windows.current->zmin),&(plot_windows.current->zmax));
      Mz=lmax(plot_windows.current->zmax,Mz);
      mz=-lmax(-plot_windows.current->zmin,-mz);
      
    }
    plot_windows.current->xmax=Mx;
    plot_windows.current->ymax=My;
    plot_windows.current->zmax=Mz;
    plot_windows.current->xmin=mx;
    plot_windows.current->ymin=my;
    plot_windows.current->zmin=mz;
    
    
    pretty(&(plot_windows.current->ymin),&(plot_windows.current->ymax));
    pretty(&(plot_windows.current->xmin),&(plot_windows.current->xmax));
    pretty(&(plot_windows.current->zmin),&(plot_windows.current->zmax));
    corner_cube(&(plot_windows.current->xlo),&(plot_windows.current->xhi),&(plot_windows.current->ylo),&(plot_windows.current->yhi));
    pretty(&(plot_windows.current->xlo),&(plot_windows.current->xhi));
    pretty(&(plot_windows.current->ylo),&(plot_windows.current->yhi));
    check_windows();
  }
  else  
    {
      for(i=0;i<n;i++){
	get_max(plot_windows.current->xv[i],&(plot_windows.current->xmin),&(plot_windows.current->xmax));
	Mx=lmax(plot_windows.current->xmax,Mx);
	mx=-lmax(-plot_windows.current->xmin,-mx);
	
       get_max(plot_windows.current->yv[i],&(plot_windows.current->ymin),&(plot_windows.current->ymax));
	My=lmax(plot_windows.current->ymax,My);
	my=-lmax(-plot_windows.current->ymin,-my);
	
      }
      plot_windows.current->xmax=Mx;
      plot_windows.current->ymax=My;
      
      plot_windows.current->xmin=mx;
      plot_windows.current->ymin=my;
      
      
      pretty(&(plot_windows.current->ymin),&(plot_windows.current->ymax)); 
      pretty(&(plot_windows.current->xmin),&(plot_windows.current->xmax));
      plot_windows.current->xlo=plot_windows.current->xmin;
      plot_windows.current->ylo=plot_windows.current->ymin;
      plot_windows.current->xhi=plot_windows.current->xmax;
      plot_windows.current->yhi=plot_windows.current->ymax;
      check_windows();
    }
  redraw_the_graph();
}
  






void user_window()
{
 static const char *n[]={"X Lo","X Hi","Y Lo","Y Hi"};
 char values[4][MAX_LEN_SBOX];
 int status;
 XPP_SPRINTF(values[0],"%g",plot_windows.current->xlo);
 XPP_SPRINTF(values[2],"%g",plot_windows.current->ylo);
 XPP_SPRINTF(values[1],"%g",plot_windows.current->xhi);
 XPP_SPRINTF(values[3],"%g",plot_windows.current->yhi);
 static const int kinds[]={XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER};
 status=do_string_box_of(4,2,2,"Window",n,values,28,kinds);
 if(status!=0){
             
	      plot_windows.current->xlo=atof(values[0]);
	      plot_windows.current->ylo=atof(values[2]);
	      plot_windows.current->xhi=atof(values[1]);
	      plot_windows.current->yhi=atof(values[3]);
	      if(plot_windows.current->grtype<5){
	      plot_windows.current->xmin=plot_windows.current->xlo;
	      plot_windows.current->xmax=plot_windows.current->xhi;
	      plot_windows.current->ymin=plot_windows.current->ylo;
	      plot_windows.current->ymax=plot_windows.current->yhi;
	      }
	      check_windows();
             }
 redraw_the_graph();
}

void xi_vs_t() /*  a short cut   */
{
 char name[20],value[256]; /* new_string edits up to 255 characters */
 int i=plot_windows.current->yv[0];
 

 ind_to_sym(i,value);
 XPP_SPRINTF(name,"Plot vs t: ");
 new_string_of(name,value,XPP_FIELD_NAME_IN(0));
 find_variable(value,&i);
 
 if(i>-1){
   plot_windows.current->yv[0]=i;
   plot_windows.current->grtype=0;
   plot_windows.current->xv[0]=0;
   if(storind>=2){
      get_max(plot_windows.current->xv[0],&(plot_windows.current->xmin),&(plot_windows.current->xmax));
   pretty(&(plot_windows.current->xmin),&(plot_windows.current->xmax));
    get_max(plot_windows.current->yv[0],&(plot_windows.current->ymin),&(plot_windows.current->ymax));
     pretty(&(plot_windows.current->ymin),&(plot_windows.current->ymax)); 
   
    }
   else {
     plot_windows.current->xmin=T0;
     plot_windows.current->xmax=TEND;
        }
    plot_windows.current->xlo=plot_windows.current->xmin;
    plot_windows.current->ylo=plot_windows.current->ymin;
    plot_windows.current->xhi=plot_windows.current->xmax;
    plot_windows.current->yhi=plot_windows.current->ymax;
    check_windows();
    check_flags();
   set_normal_scale();
    redraw_the_graph();
 }
}
 
 



void movie_rot(double start, double increment, int nclip, int angle)
{
  int i;
  double thetaold=plot_windows.current->Theta,phiold=plot_windows.current->Phi;
  reset_film();
  for(i=0;i<=nclip;i++){
   
    if(angle==0)
      make_rot(start+i*increment,phiold);
    else
      make_rot(thetaold,start+i*increment);
    redraw_the_graph();
    xpp_ui.film_clip();
  }
  plot_windows.current->Theta=thetaold;
  plot_windows.current->Phi=phiold;
}

void get_3d_par_com()
{
  

 static const char *n[]={"Persp (1=On)","ZPlane","ZView","Theta","Phi","Movie(Y/N)",
                   "Vary (theta/phi)","Start angle", "Increment",
		   "Number increments"};
 char values[10][MAX_LEN_SBOX];
 int status;
 
 int nclip=8,angle=0;
 double start,increment=45; 
  if(plot_windows.current->grtype<5)return;


 XPP_SPRINTF(values[0],"%d",plot_windows.current->PerspFlag);
 XPP_SPRINTF(values[1],"%g",plot_windows.current->ZPlane);
 XPP_SPRINTF(values[2],"%g",plot_windows.current->ZView);
 XPP_SPRINTF(values[3],"%g",plot_windows.current->Theta);
 XPP_SPRINTF(values[4],"%g",plot_windows.current->Phi);
 XPP_SPRINTF(values[5],"%s",mov3d.yes);
 XPP_SPRINTF(values[6],"%s",mov3d.angle);
 XPP_SPRINTF(values[7],"%g",mov3d.start);
 XPP_SPRINTF(values[8],"%g",mov3d.incr);
 XPP_SPRINTF(values[9],"%d",mov3d.nclip);
 
 static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER};
 status=do_string_box_of(10,5,2,"3D Parameters",n,values,28,kinds);
 if(status!=0){
	      plot_windows.current->PerspFlag=atoi(values[0]);
	      plot_windows.current->ZPlane=atof(values[1]);
	      plot_windows.current->ZView=atof(values[2]);
	      plot_windows.current->Theta=atof(values[3]);
	      plot_windows.current->Phi=atof(values[4]);
             if(values[5][0]=='y'|| values[5][0]=='Y'){  
	      snprintf(mov3d.yes,sizeof(mov3d.yes),"%.*s",(int)sizeof(mov3d.yes)-1,values[5]);
	      snprintf(mov3d.angle,sizeof(mov3d.angle),"%.*s",(int)sizeof(mov3d.angle)-1,values[6]);
              start=atof(values[7]);
	      increment=atof(values[8]);
	      nclip=atoi(values[9]);
	      mov3d.start=start;
	      mov3d.incr=increment;
	      mov3d.nclip=nclip;
	      angle=0;
              if(mov3d.angle[0]=='p'||mov3d.angle[0]=='P')
		angle=1;
	      /*     XRaiseWindow(display,MyGraph->w); */
	      movie_rot(start,increment,nclip,angle);
	     }
	       
                make_rot(plot_windows.current->Theta,plot_windows.current->Phi);   
	    /*  Redraw the picture   */	
	       redraw_the_graph();
         
	     }
	     
}


void update_view(float xlo,float xhi, float ylo, float yhi)
{
              plot_windows.current->xlo=xlo;
	      plot_windows.current->ylo=ylo;
	      plot_windows.current->xhi=xhi;
	      plot_windows.current->yhi=yhi;
	      if(plot_windows.current->grtype<5){
	      plot_windows.current->xmin=plot_windows.current->xlo;
	      plot_windows.current->xmax=plot_windows.current->xhi;
	      plot_windows.current->ymin=plot_windows.current->ylo;
	      plot_windows.current->ymax=plot_windows.current->yhi;
	      }
	      check_windows();
            
 redraw_the_graph();

}
    
void window_zoom_com(int c)
{
 int i1,i2,j1,j2;
  switch(c){
	    case 0:user_window(); break;
	    case 1:
	    
	   /*  XSelectInput(display,w,
   KeyPressMask|ButtonPressMask|ButtonReleaseMask|
		PointerMotionMask|ButtonMotionMask|ExposureMask);
	    	   while(1)
		   {
		   	XNextEvent(display,&ev);
   			switch(ev.type){ 
		   } */
	    	if(rubber_band(&i1,&j1,&i2,&j2,RUBBOX)==0)break;
		     zoom_in(i1,j1,i2,j2);
		 
		     break;
       	    case 2: if(rubber_band(&i1,&j1,&i2,&j2,RUBBOX)==0)break;
		     zoom_out(i1,j1,i2,j2);
		     break;
 	    case 3: fit_window();
		      break; 
	    case 4: default_window();
		      break;
            case 5: scroll_window();
                      break;
            }
 set_normal_scale();
}



void zoom_in(int i1, int j1, int i2, int j2)
{
 float x1,y1,x2,y2;
 float dx=plot_windows.current->xhi-plot_windows.current->xlo;
 float dy=plot_windows.current->yhi-plot_windows.current->ylo;
 scale_to_real(i1,j1,&x1,&y1);
 scale_to_real(i2,j2,&x2,&y2);
   if(x1==x2||y1==y2)
   {
   
   	if (dx < 0){dx=-dx;}
	if (dy < 0){dy=-dy;}
	dx = dx/2;
	dy = dy/2;
	
	/*Shrink by thirds and center (track) about the point clicked*/
	plot_windows.current->xlo=x1-dx/2;
	plot_windows.current->xhi=x1+dx/2;

	plot_windows.current->ylo=y1-dy/2;
	plot_windows.current->yhi=y1+dy/2;
 	

 }
 else
 {           
	      plot_windows.current->xlo=x1;
	      plot_windows.current->ylo=y1;
	      plot_windows.current->xhi=x2;
	      plot_windows.current->yhi=y2;
  }
  	if(plot_windows.current->grtype<5){
	      plot_windows.current->xmin=plot_windows.current->xlo;
	      plot_windows.current->xmax=plot_windows.current->xhi;
	      plot_windows.current->ymin=plot_windows.current->ylo;
	      plot_windows.current->ymax=plot_windows.current->yhi;
	      }
	      check_windows();
              redraw_the_graph();
	      draw_help(); 
}

void zoom_out(int i1, int j1, int i2, int j2)
{
 
 float x1,y1,x2,y2;
 float bx,mux,by,muy;
 float dx=plot_windows.current->xhi-plot_windows.current->xlo;
 float dy=plot_windows.current->yhi-plot_windows.current->ylo;
 scale_to_real(i1,j1,&x1,&y1);
 scale_to_real(i2,j2,&x2,&y2);

 /*
 if(x1==x2||y1==y2)return;
 */
 /*
 plintf("%f %f %f %f \n ",x1,y1,x2,y2);
 plintf("%f %f %f %f \n",MyGraph->xlo,MyGraph->ylo,MyGraph->xhi,MyGraph->yhi);
*/
 if(x1==x2||y1==y2)
 {
 
 	if (dx < 0){dx=-dx;}
	if (dy < 0){dy=-dy;}
	/*Grow by thirds and center (track) about the point clicked*/
	dx = dx*2;
	dy = dy*2;
	
	plot_windows.current->xlo=x1-dx/2;
	plot_windows.current->xhi=x1+dx/2;

	plot_windows.current->ylo=y1-dy/2;
	plot_windows.current->yhi=y1+dy/2;
 }
 else
 {
  	if(x1>x2){bx=x1;x1=x2;x2=bx;}
 	if(y1>y2){by=y1;y1=y2;y2=by;}
	
	 bx=dx*dx/(x2-x1);
	 mux=(x1-plot_windows.current->xlo)/dx;
	 plot_windows.current->xlo=plot_windows.current->xlo-bx*mux;
	 plot_windows.current->xhi=plot_windows.current->xlo+bx;

	 by=dy*dy/(y2-y1);
	 muy=(y1-plot_windows.current->ylo)/dy;
	 plot_windows.current->ylo=plot_windows.current->ylo-by*muy;
	 plot_windows.current->yhi=plot_windows.current->ylo+by;

	    
}
	if(plot_windows.current->grtype<5){
		      plot_windows.current->xmin=plot_windows.current->xlo;
		      plot_windows.current->xmax=plot_windows.current->xhi;
		      plot_windows.current->ymin=plot_windows.current->ylo;
		      plot_windows.current->ymax=plot_windows.current->yhi;
		      }
	      check_windows();
              redraw_the_graph();
              draw_help(); 
}
 



void graph_all(int *list, int n, int type)
{
  int i;
  if(type==0){
    for(i=0;i<n;i++){
      plot_windows.current->xv[i]=0;
      plot_windows.current->yv[i]=list[i];
      plot_windows.current->line[i]=plot_windows.current->line[0];
      plot_windows.current->color[i]=i;
    }
    plot_windows.current->nvars=n;
    plot_windows.current->grtype=0;
    plot_windows.current->ThreeDFlag=0;
  }
  if(type==1){
   plot_windows.current->nvars=1;
   plot_windows.current->xv[0]=list[0];
   plot_windows.current->yv[0]=list[1];
    plot_windows.current->grtype=0;
    plot_windows.current->ThreeDFlag=0;
    if(n==3){
      plot_windows.current->zv[0]=list[2];
      plot_windows.current->grtype=5;
      plot_windows.current->ThreeDFlag=1;
    }
  }
  check_flags();
  fit_window();

}


int alter_curve(const char *title, int in_it, int n)
{
 static const char *nn[]={"*0X-axis","*0Y-axis","*0Z-axis","*4Color","Line type"};
 char values[5][MAX_LEN_SBOX];
 int status,i;
 int i1=plot_windows.current->xv[in_it],i2=plot_windows.current->yv[in_it],i3=plot_windows.current->zv[in_it];
 char n1[XPP_NAME_MAX+1],n2[XPP_NAME_MAX+1],n3[XPP_NAME_MAX+1];


 ind_to_sym(i1,n1);
 ind_to_sym(i2,n2);
 ind_to_sym(i3,n3);
 XPP_SPRINTF(values[0],"%s",n1);
 XPP_SPRINTF(values[1],"%s",n2);
 XPP_SPRINTF(values[2],"%s",n3);
 XPP_SPRINTF(values[3],"%d",plot_windows.current->color[in_it]);
 XPP_SPRINTF(values[4],"%d",plot_windows.current->line[in_it]);
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),XPP_FIELD_NAME_IN(0),
                           XPP_FIELD_NAME_IN(4),XPP_FIELD_INTEGER};
 status=do_string_box_of(5,5,1,title,nn,values,25,kinds);
 if(status!=0){
		    find_variable(values[0],&i);
 	      if(i>-1)
		plot_windows.current->xv[n]=i;
              find_variable(values[1],&i);
              if(i>-1)
		plot_windows.current->yv[n]=i;
              find_variable(values[2],&i);
  		if(i>-1)
		  plot_windows.current->zv[n]=i;

	       plot_windows.current->line[n]=atoi(values[4]);
               i=atoi(values[3]);
		    if(i<0||i>10)i=0;
		    plot_windows.current->color[n]=i;
		   
		  return(1);
              
              }
 return(0);
}


void edit_curve()
{
 char bob[32];
 int crv=0;
 snprintf(bob,sizeof(bob),"Edit 0-%d :",plot_windows.current->nvars-1);
 ping();
 new_int(bob,&crv);
 if(crv>=0&&crv<plot_windows.current->nvars)
   {
     snprintf(bob,sizeof(bob),"Edit curve %d",crv);
     alter_curve(bob,crv,crv);
   }
}

void new_curve()
{
 if(alter_curve("New Curve",0,plot_windows.current->nvars))
   plot_windows.current->nvars=plot_windows.current->nvars+1;
  
 }  

void create_ps()
{
 /*char filename[256];*/
 char filename[XPP_MAX_NAME];
 static const char *nn[]={"BW-0/Color-1","Land(0)/Port(1)","Axes fontsize","Font","Linewidth"};
 int status;
 char values[5][MAX_LEN_SBOX];
 XPP_SPRINTF(values[0],"%d",plot_export.color);
 XPP_SPRINTF(values[1],"%d",PS_Port);
 XPP_SPRINTF(values[2],"%d",PS_FONTSIZE);
 snprintf(values[3],sizeof(values[3]),"%.24s",PS_FONT);
 XPP_SPRINTF(values[4],"%g",PS_LW);
 static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_TEXT,XPP_FIELD_NUMBER};
 status=do_string_box_of(5,5,1,"Postscript parameters",nn,values,25,kinds);
 if(status!=0){
         plot_export.color=atoi(values[0]);
	 PS_Port=atoi(values[1]);
	 PS_FONTSIZE=atoi(values[2]);
	 PS_LW=atof(values[4]);
         XPP_SPRINTF(PS_FONT,"%s",values[3]);
	 snprintf(filename,sizeof(filename),"%.250s.ps",this_file);
	 ping();
 
	 if(!file_selector("Print postscript",filename,"*.ps"))return;
	 if(ps_init(filename,plot_export.color)){
	   ps_restore(); 
	   ping();
	 }
 }
}

void create_svg()
{

 char filename[XPP_MAX_NAME];
 XPP_STRCPY(filename,this_file);
 filename[strlen(filename)-4]='\0';
 strcat(filename,".svg");	
 /*sprintf(filename,"%s.svg",tmp);*/
 if(!file_selector("Print svg",filename,"*.svg"))return;
 if(svg_init(filename,plot_export.color)){
	   svg_restore(); 
	   ping();
	 }
 
}

/* 
ps_test()
{
 double xlo=MyGraph->xlo,xhi=MyGraph->xhi,ylo=MyGraph->ylo,yhi=MyGraph->yhi;
 text_abs((float)xlo,(float)ylo,"lolo");
 text_abs((float)xlo,(float)yhi,"lohi");
 text_abs((float)xhi,(float)ylo,"hilo");
 text_abs((float)xhi,(float)yhi,"hihi");
 ps_end();
}
 
*/


void change_cmap_com(int i)
{
      NewColormap(i);

}

void freeze_com(int c)
{

 switch(c){
 case 0: 
   freeze_crv(0);
   break;
 case 1:
   delete_frz();
   break;
 case 2:
   edit_frz();
   break;
 case 3:
   kill_frz();
   break;
   /*case 4:
   key_frz();
   break; */
 case 5:
   frz_bd();
   break;
 case 6:
   free_bd();
   break;
 case 7:
   frozen_curves.auto_freeze=1-frozen_curves.auto_freeze;
   break;
   
 }
}

void set_key(int x, int y)
{
  float xp,yp;
  scale_to_real(x,y,&xp,&yp);
  FreezeKeyX=xp;
  FreezeKeyY=yp;
  FreezeKeyFlag=1;
}

void draw_freeze_key()
{
  int ix,iy;
  int i,y0;
  int ix2;
  int dy=2*HChar;
  if(FreezeKeyFlag==SCRNFMT)return;
  if(PltFmtFlag==PSFMT)dy=-dy;
  scale_to_screen((float)FreezeKeyX,(float)FreezeKeyY,&ix,&iy);
  ix2=ix+4*HChar;
  y0=iy;
  for(i=0;i<MAXFRZ;i++){
    if(frozen_curves.curve[i].use==1&&frozen_curves.curve[i].w==plot_windows.draw_win&&strlen(frozen_curves.curve[i].key)>0){
      set_linestyle(abs(frozen_curves.curve[i].color));
      line(ix,y0,ix2,y0);
      set_linestyle(0);
      put_text(ix2+HChar,y0,frozen_curves.curve[i].key);
      y0+=dy;
    }
  }
}

void key_frz_com(int c)
{
  int x,y;
  switch(c){
  case 0:
    FreezeKeyFlag=0;
    break;
  case 1:
    MessageBox("Position with mouse");
    if(GetMouseXY(&x,&y)){
      set_key(x,y);
      draw_freeze_key();
    }
    KillMessageBox();
  }
}
  
void edit_frz()
{
 int i;
 i=get_frz_index(plot_windows.draw_win);
 if(i<0)return;
 edit_frz_crv(i);
}


void delete_frz_crv(int i)
{
  if(frozen_curves.curve[i].use==0)return;
  frozen_curves.curve[i].use=0;
  frozen_curves.curve[i].name[0]=0;
  frozen_curves.curve[i].key[0]=0;
  xpp_free(frozen_curves.curve[i].xv);
  xpp_free(frozen_curves.curve[i].yv);
  if(frozen_curves.curve[i].type>0)
    xpp_free(frozen_curves.curve[i].zv);
}

void delete_frz()
{
 int i;
 i=get_frz_index(plot_windows.draw_win);
 if(i<0)return;
 delete_frz_crv(i);
}


void kill_frz()
{
  int i;
  for(i=0;i<MAXFRZ;i++){
    if(frozen_curves.curve[i].use==1&&frozen_curves.curve[i].w==plot_windows.draw_win)
      delete_frz_crv(i);
  }
}

int freeze_crv(int ind)
{
 int i;
 i=create_crv(ind);
 if(i<0)return(-1);
 edit_frz_crv(i);
 return(1);
}

void auto_freeze_it()
{
  if(frozen_curves.auto_freeze==0)return;
  create_crv(0);
}

int create_crv(int ind)
{
  int i,type,j;
  int ix,iy,iz;

  for(i=0;i<MAXFRZ;i++){
    if(frozen_curves.curve[i].use==0){
      ix=plot_windows.current->xv[ind];
      iy=plot_windows.current->yv[ind];
      iz=plot_windows.current->zv[ind];
      if(my_browser.maxrow<=2){
	err_msg("No Curve to freeze");
	return(-1);
      }
      frozen_curves.curve[i].xv=(float *) xpp_malloc(sizeof(float)*my_browser.maxrow);
      frozen_curves.curve[i].yv=(float *) xpp_malloc(sizeof(float)*my_browser.maxrow);
      if((type=plot_windows.current->grtype)>0)
	frozen_curves.curve[i].zv=(float *)xpp_malloc(sizeof(float)*my_browser.maxrow);
      if ((type>0&&frozen_curves.curve[i].zv==NULL)|| (type==0&&frozen_curves.curve[i].yv==NULL)){
	err_msg("Cant allocate storage for curve");
	return(-1);
      }
      frozen_curves.curve[i].use=1;
      frozen_curves.curve[i].len=my_browser.maxrow;
      for(j=0;j<my_browser.maxrow;j++){
	frozen_curves.curve[i].xv[j]=my_browser.data[ix][j];
	frozen_curves.curve[i].yv[j]=my_browser.data[iy][j];
	if(type>0)
	  frozen_curves.curve[i].zv[j]=my_browser.data[iz][j];
      }
      frozen_curves.curve[i].type=type;
      frozen_curves.curve[i].w=plot_windows.draw_win;
      XPP_SPRINTF(frozen_curves.curve[i].name,"crv%c",'a'+i);
      XPP_SPRINTF(frozen_curves.curve[i].key,"crv%c",'a'+i);
      marks_data_frozen_new(i); /* the window shows it: it is its current curve */
      return(i);
    }
  }
    err_msg("All curves used");
    return(-1);
}	
	

void edit_frz_crv(int i)
{
 static const char *nn[]={"*4Color","Key","Name"};
 char values[3][MAX_LEN_SBOX];
 int status;
 XPP_SPRINTF(values[0],"%d",frozen_curves.curve[i].color);
 XPP_SPRINTF(values[1],"%s",frozen_curves.curve[i].key);
 XPP_SPRINTF(values[2],"%s",frozen_curves.curve[i].name);
 static const int kinds[]={XPP_FIELD_NAME_IN(4),XPP_FIELD_TEXT,XPP_FIELD_TEXT};
 status=do_string_box_of(3,3,1,"Edit Freeze",nn,values,25,kinds);
 if(status!=0){
   frozen_curves.curve[i].color=atoi(values[0]);
   snprintf(frozen_curves.curve[i].key,sizeof(frozen_curves.curve[i].key),"%.19s",values[1]);
   snprintf(frozen_curves.curve[i].name,sizeof(frozen_curves.curve[i].name),"%.9s",values[2]);
 }
}

void draw_frozen_cline(int index, XppWinId w)
{
  if(nclines[index].use==0||nclines[index].w!=w)
    return;
}

void draw_freeze(XppWinId w)
{
  int i,j,type=plot_windows.current->grtype,lt=0;
  float oldxpl,oldypl,oldzpl=0.0,xpl,ypl,zpl=0.0;
  float *xv,*yv,*zv;
  for(i=0;i<MAXNCLINE;i++)
    draw_frozen_cline(i,w);
  for(i=0;i<MAXFRZ;i++){
    if(frozen_curves.curve[i].use==1&&frozen_curves.curve[i].w==w&&frozen_curves.curve[i].type==type){
      if(type==0)marks_data_frozen(w,i); /* the curve as data */
      if(frozen_curves.curve[i].color<0){
	set_linestyle(-frozen_curves.curve[i].color);
	lt=1;
      }
      else
	set_linestyle(frozen_curves.curve[i].color);
      xv=frozen_curves.curve[i].xv;
      yv=frozen_curves.curve[i].yv;
      zv=frozen_curves.curve[i].zv;
      oldxpl=xv[0];
      oldypl=yv[0];
      if(type>0)
	oldzpl=zv[0];
      for(j=0;j<frozen_curves.curve[i].len;j++){
	xpl=xv[j];
	ypl=yv[j];
	if(type>0)
	  zpl=zv[j];
        if(lt==0){
	if(type==0)
	  line_abs(oldxpl,oldypl,xpl,ypl);
	else
	  line_3d(oldxpl,oldypl,oldzpl,xpl,ypl,zpl);
	}
	else {
	  if(type==0)
	  point_abs(xpl,ypl);
	else
	  point_3d(xpl,ypl,zpl);
	}
	oldxpl=xpl;
	oldypl=ypl;
	if(type>0)
	  oldzpl=zpl;
      }
    }
  }
  draw_freeze_key();
  draw_bd(w);
} 

/*  Bifurcation curve importing */

void init_bd()
{
 my_bd.nbifcrv=0;
}

void draw_bd(XppWinId w)
{
 int i,j,len;
 float oldxpl,oldypl,xpl,ypl,*x,*y;
 if(w==my_bd.w&&my_bd.nbifcrv>0){
   for(i=0;i<my_bd.nbifcrv;i++){
     set_linestyle(my_bd.color[i]);
     len=my_bd.npts[i];
     x=my_bd.x[i];
     y=my_bd.y[i];
     xpl=x[0];
     ypl=y[0];
     for(j=0;j<len;j++){
       oldxpl=xpl;
       oldypl=ypl;
       xpl=x[j];
       ypl=y[j];
       line_abs(oldxpl,oldypl,xpl,ypl);
     }
   }
 }
}

void free_bd()
{
  int i;
  if(my_bd.nbifcrv>0){
    for(i=0;i<my_bd.nbifcrv;i++){
      xpp_free(my_bd.x[i]);
      xpp_free(my_bd.y[i]);
    }
    my_bd.nbifcrv=0;
  }
}


void add_bd_crv(float *x, float *y, int len, int type, int ncrv)
{
  int i;
  if(ncrv>=MAXBIFCRV)return;
  my_bd.x[ncrv]=(float *)xpp_malloc(sizeof(float)*len);
  my_bd.y[ncrv]=(float *)xpp_malloc(sizeof(float)*len);
  for(i=0;i<len;i++){
    my_bd.x[ncrv][i]=x[i];
    my_bd.y[ncrv][i]=y[i];
  }
  my_bd.npts[ncrv]=len;
  i=lsSEQ;
  if(type==UPER)i=lsUPER;
  if(type==SPER)i=lsSPER;
  if(type==UEQ)i=lsUEQ;
  my_bd.color[ncrv]=i;
}

void frz_bd()
{
  FILE *fp;
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  XPP_SPRINTF(filename,"diagram.dat");
  ping();
  if(!file_selector("Import Diagram",filename,"*.dat"))return;
  /* if(new_string("Diagram to import: ",filename)==0)return; */
  if((fp=fopen(filename,"r"))==NULL){
    err_msg("Couldn't open file");
    return;
  }
  read_bd(fp);
}
void read_bd(FILE *fp)
{
  int oldtype,type,oldbr,br,ncrv=0,len,f2;
  float x[8000],ylo[8000],yhi[8000];
  len=0;
  if(fscanf(fp,"%g %g %g %d %d %d",&x[len],&ylo[len],&yhi[len],&oldtype,&oldbr,&f2)!=6){
    fclose(fp);
    return;
  }
  len++;
  while(!feof(fp)){
    if(fscanf(fp,"%g %g %g %d %d %d",&x[len],&ylo[len],&yhi[len],&type,&br,&f2)!=6)
      break;
    if(type==oldtype&&br==oldbr)
      len++; 
    else {
    /* if(oldbr==br)len++; */ /* extend to point of instability */
     add_bd_crv(x,ylo,len,oldtype,ncrv); 
      ncrv++;
      if(oldtype==UPER||oldtype==SPER){
     add_bd_crv(x,yhi,len,oldtype,ncrv); 	
	ncrv++;
      }
      if(oldbr==br)len--;
      x[0]=x[len];
      ylo[0]=ylo[len];
      yhi[0]=yhi[len];
   
      len=1;
    }
    oldbr=br;
    oldtype=type;
  }
 /*  save this last one */
   if(len>1){
     add_bd_crv(x,ylo,len,oldtype,ncrv); 
     ncrv++;
     if(oldtype==UPER||oldtype==SPER){
       add_bd_crv(x,yhi,len,oldtype,ncrv); 	
       ncrv++;
     }
   }
  xpp_log(XPP_LOG_INFO, " got %d bifurcation curves\n",ncrv);
 fclose(fp);
 my_bd.nbifcrv=ncrv;
 my_bd.w=plot_windows.draw_win;
} 

int get_frz_index(XppWinId w)
{
  char *n[MAXFRZ];
  char key[MAXFRZ],ch;
  XppMenu m={"freeze_curves","Curves",0,NULL,NULL,NULL,-1,12,8};
  
  int i;
  int count=0;
  for(i=0;i<MAXFRZ;i++){
    if(frozen_curves.curve[i].use==1&&w==frozen_curves.curve[i].w){
	n[count]=(char *)xpp_malloc(20);
      /* n[count] is a pointer, allocated 20 bytes just above. */
      xpp_snprintf(n[count],20,"%s",frozen_curves.curve[i].name);
      key[count]='a'+i;
      
      count++;
    }
    
  }
  if(count==0)return(-1);
  key[count]=0;
   m.n=count; m.items=n; m.keys=key; m.hints=no_hint;
   ch=(char)menu_choose(&m,0);
   for(i=0;i<count;i++)xpp_free(n[i]);
  return((int)(ch-'a'));
       
}
      





void export_graf_data()
{
 FILE *fp;
 /*char filename[256];*/
 char filename[XPP_MAX_NAME];
 XPP_SPRINTF(filename,"curve.dat");
 ping();
if(!file_selector("Export graph data",filename,"*.dat"))return;
/* if(new_string("Data filename:",filename)==0)return; */
if((fp=fopen(filename,"w"))==NULL){
    err_msg("Couldn't open file");
    return;
  }
 export_data(fp);
 fclose(fp);
}

void add_a_curve_com(int c)
{

 switch(c){
 case 0: if(plot_windows.current->nvars>=MAXPERPLOT)
   {
     err_msg("Too many plots!");
     return;
   }
   new_curve();
   break;
 case 1:if(plot_windows.current->nvars>1)plot_windows.current->nvars=plot_windows.current->nvars-1;
   break;
 case 2:plot_windows.current->nvars=1;
   break;
 case 3: edit_curve();
   break;
 case 4: create_ps();
   break;
 case 5: create_svg();
   break;
   /* case 6: freeze();
      break; */
 case 7: axes_opts();
   break;
 case 8: export_graf_data();
   break;
   /*  case 9: change_cmap();
       break; */
 }
 check_flags();
 redraw_the_graph();
   
}






















