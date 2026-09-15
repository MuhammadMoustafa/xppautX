/* Text labels, arrows, pointers and markers drawn on the plot windows, and
   the Text,etc and Makewindow commands. Moved out of many_pops.c (X11);
   the window handling itself stays in the front end. */
#include "grobs.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "browse.h"
#include "graf_par.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN_SBOX 25
#define POINTER 0
#define ARROW 1
#define MARKER 2 /* markers start at 2  there are several of them */

#define WDMARK .001
#define HTMARK .0016

typedef struct {
  int type,color;
  int number,start,skip;
  double size;
} MARKINFO;

MARKINFO markinfo={2,0,1,0,1,1.0};

LABEL lb[MAXLAB];
GROB grob[MAXGROB];
extern char *no_hint[];

void add_label(s,x,y,size,font)
char *s;
int x,y,size,font;
{ int i;
 float xp,yp;
 scale_to_real(x,y,&xp,&yp);
 for(i=0;i<MAXLAB;i++)
 {
  if(lb[i].use==0){
	lb[i].use=1;
	lb[i].x=xp;
	lb[i].y=yp;
	lb[i].w=draw_win;
        lb[i].font=font;
        lb[i].size=size;
	strcpy(lb[i].s,s);
        return;
  }
 }
}

void draw_marker(x,y,size,type)
     float x,y,size;
     int type;
{
int pen=0;
float x1=x,y1=y,x2,y2;
int ind=0;
int offset;

static int sym_dir[] = {
  /*          box              */
  0, -6, -6,1, 12,  0,1,  0, 12,1,-12,  0,
  1,  0,-12,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  
  /*          diamond             */
  0, 8, 0,1, -8,  -8,1,  8, -8,1,8,  8,
  1, -8, 8,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  /*          triangle         */
  0, -6, -6,1, 12,  0,1, -6, 12,1, -6,-12,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  
  /*          plus            */
  0, -6,  0,1, 12,  0,0, -6, -6,1,  0, 12,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  
  /*          cross            */
  0, -6,  6,1, 12, -12,0, -12, 0,1,  12, 12,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  3,  0,  0,3,  0,  0,3,  0,  0,3,  0,  0,
  
  /*          circle           */
  0,  6,  0,1, -1,  3,1, -2,  2,1, -3,  1,
  1, -3, -1,1, -2, -2,1, -1, -3,1,  1, -3,
  1,  2, -2,1,  3, -1,1,  3,  1,1,  2,  2,
  1,  1,  3,3,  0,  0,3,  0,  0,3,  0,  0,
  
};
float dx=(MyGraph->xhi-MyGraph->xlo)*WDMARK*size;
float dy=(MyGraph->yhi-MyGraph->ylo)*HTMARK*size;
while(1)
  {
    offset=48*type+3*ind;
    pen=sym_dir[offset];
    if(pen==3)break;
    x2=dx*sym_dir[offset+1]+x1;
    y2=dy*sym_dir[offset+2]+y1;
    if(pen==1)line_abs(x1,y1,x2,y2);
    x1=x2;
    y1=y2;
    ind++;
  }

}

void draw_grob(i)
     int i;
{
 float xs=grob[i].xs,ys=grob[i].ys,xe=grob[i].xe,ye=grob[i].ye;
 set_linestyle(grob[i].color);
 if(grob[i].type==POINTER)
   line_abs(xs,ys,xe,ye);
 if(grob[i].type==ARROW||grob[i].type==POINTER)
   arrow_head(xs,ys,xe,ye,grob[i].size);
 if(grob[i].type>=MARKER)
   draw_marker(xs,ys,grob[i].size,grob[i].type-2);
}

void arrow_head(xs,ys,xe,ye,size)
     float xs,ys,xe,ye;
     double size;
{
 float l=xe-xs,h=ye-ys;
 float ar=(MyGraph->xhi-MyGraph->xlo)/(MyGraph->yhi-MyGraph->ylo);
 float x0=xs+size*l,y0=ys+size*h;
 /* float tot=(float)sqrt((double)(l*l+h*h)); */
 
 float xp=x0+.5*size*h*ar,yp=y0-.5*size*l/ar;
 float xm=x0-.5*size*h*ar,ym=y0+.5*size*l/ar; 
 line_abs(xs,ys,xp,yp);
 line_abs(xs,ys,xm,ym);
}

void destroy_grob(w)
XppWinId w;
{
  int i;
  for(i=0;i<MAXGROB;i++){
  if((grob[i].use==1)&&(grob[i].w==w)){
	grob[i].use=0;
	grob[i].w=(XppWinId)0;
   }
  }
}

void destroy_label(w)
XppWinId w;
{
  int i;
  for(i=0;i<MAXLAB;i++){
  if((lb[i].use==1)&&(lb[i].w==w)){
	lb[i].use=0;
	lb[i].w=(XppWinId)0;
   }
  }
}

void draw_label(w)
XppWinId w;
{
 int i;
 GrCol();
 for(i=0;i<MAXLAB;i++){
	if((lb[i].use==1)&&(lb[i].w==w))
	fancy_text_abs(lb[i].x,lb[i].y,lb[i].s,lb[i].size,lb[i].font);
 }
 for(i=0;i<MAXGROB;i++){
   if((grob[i].use==1)&&(grob[i].w==w))
     draw_grob(i);
 }
 BaseCol();
}

void add_grob(xs,ys,xe,ye,size,type,color)
     double size;
     float xs,ys,xe,ye;
     int type,color;
{
  int i;
  for(i=0;i<MAXGROB;i++){
    if(grob[i].use==0){
      grob[i].use=1;
      grob[i].xs=xs;
      grob[i].xe=xe;
      grob[i].ys=ys;
      grob[i].ye=ye;
      grob[i].w=draw_win;
      grob[i].size=size;
      grob[i].color=color;
      grob[i].type=type;
 /*     redraw_all(); */
      return;
    }
  }
}

int select_marker_type(type)
     int *type;
{
  int ival=*type-MARKER;
  int i;
  char *list[]={"Box","Diamond","Triangle","Plus","X","Circle"};
  static char key[]="bdtpxc";
  XppMenu m={"markers","Markers",6,NULL,key,NULL,-1,9,4};
  char ch;
  m.items=list; m.hints=no_hint;
  ch=(char)menu_choose(&m,ival);
  if(ch==27)return(0);
  for(i=0;i<6;i++){
    if(ch==key[i])
      ival=i;
  }
  if(ival<6)*type=MARKER+ival;

  return(1);
}

int man_xy(xe,ye)
float *xe, *ye;
{
  double x=0,y=0;
  if(new_float("x: ",&x))
    return 0;
  if(new_float("y: ",&y))
    return 0;
  *xe=x;
  *ye=y;
  return 1;
}

int get_marker_info()
{
  static char *n[]={"*5Type","*4Color","Size"};
  char values[3][MAX_LEN_SBOX];
  int status;
  sprintf(values[0],"%d",markinfo.type);
  sprintf(values[1],"%d",markinfo.color);
  sprintf(values[2],"%g",markinfo.size);
  status=do_string_box(3,3,1,"Add Marker",n,values,25);
  if(status!=0){
    markinfo.type=atoi(values[0]);
    markinfo.size=atof(values[2]);
    markinfo.color=atoi(values[1]);
    return 1;
  }
  return 0;
}

int get_markers_info()
{
  static char *n[]={"*5Type","*4Color","Size","Number","Row1","Skip"};
  char values[6][MAX_LEN_SBOX];
  int status;
  sprintf(values[0],"%d",markinfo.type);
  sprintf(values[1],"%d",markinfo.color);
  sprintf(values[2],"%g",markinfo.size);
  sprintf(values[3],"%d",markinfo.number);
  sprintf(values[4],"%d",markinfo.start);
  sprintf(values[5],"%d",markinfo.skip);
  status=do_string_box(6,6,1,"Add Markers",n,values,25);
  if(status!=0){
    markinfo.type=atoi(values[0]);
    markinfo.size=atof(values[2]);
    markinfo.color=atoi(values[1]);
    markinfo.number=atoi(values[3]);
    markinfo.start=atoi(values[4]);
    markinfo.skip=atoi(values[5]);

    return 1;
  }
  return 0;
}

void add_marker()
{
   int flag,i1,j1,status;
  float xe=0.0,ye=0.0,xs,ys;
  status=get_marker_info();
  if(status==0)return;
  MessageBox("Position");
  flag=GetMouseXY(&i1,&j1);
  KillMessageBox();
  FlushDisplay();
  if(flag==0)return;
  scale_to_real(i1,j1,&xs,&ys);
  add_grob(xs,ys,xe,ye,markinfo.size,markinfo.type,markinfo.color);
  redraw_all();
  
}

void add_marker_old()
{
  double size=1;
  int i1,j1,color=0,flag;
  float xe=0.0,ye=0.0,xs,ys;
  /*Window temp=main_win;*/
  int type=MARKER;
  if(select_marker_type(&type)==0)return;
  if(new_float("Size: ",&size))return;
  if(new_int("Color: ",&color))return;
  /* message_box(&temp,0,SCALEY-5*DCURY,"Position"); */
  MessageBox("Position");
  flag=GetMouseXY(&i1,&j1);
  /* XDestroyWindow(display,temp); */
  KillMessageBox();
  FlushDisplay();
  if(flag==0)return;
 /* if(flag==-2){
    top_store(&xs,&ys);
    add_grob(xs,ys,xe,ye,size,type,color);
    return;
  }
  */
  if(flag==-3){
    if(man_xy(&xs,&ys))
      add_grob(xs,ys,xe,ye,size,type,color);
     redraw_all(); 
    return;
  }
  
  scale_to_real(i1,j1,&xs,&ys);

  add_grob(xs,ys,xe,ye,size,type,color);
 redraw_all(); 
}

void add_markers()
{
  int i;
  float xe=0.0,ye=0.0,xs,ys,x,y,z;
  
  if(get_markers_info()==0)return;
  for(i=0;i<markinfo.number;i++){
    get_data_xyz(&x,&y,&z,MyGraph->xv[0],MyGraph->yv[0],MyGraph->zv[0],
		 markinfo.start+i*markinfo.skip);
    if(MyGraph->ThreeDFlag==0){
      xs=x;
      ys=y;
    }
    else{
      threed_proj(x,y,z,&xs,&ys);

    }
    add_grob(xs,ys,xe,ye,markinfo.size,markinfo.type,markinfo.color);
  }
  redraw_all(); 
}

void add_markers_old()
{
  double size=1;
  int i;
  int color=0;
  int nm=1,nskip=1,nstart=0; 
  float xe=0.0,ye=0.0,xs,ys,x,y,z;
  
  int type=MARKER;
  if(select_marker_type(&type)==0)return;
  if(new_float("Size: ",&size))return;
  if(new_int("Color: ",&color))return;
  if(new_int("Number of markers: ",&nm))return;
  if(new_int("Starting at: ", &nstart))return;
  if(new_int("Skip between: ", &nskip))return;
  for(i=0;i<nm;i++){
    get_data_xyz(&x,&y,&z,MyGraph->xv[0],MyGraph->yv[0],MyGraph->zv[0],
		 nstart+i*nskip);
    if(MyGraph->ThreeDFlag==0){
      xs=x;
      ys=y;
    }
    else{
      threed_proj(x,y,z,&xs,&ys);

    }
    add_grob(xs,ys,xe,ye,size,type,color);
  }
  redraw_all(); 
}

void add_pntarr(type)
     int type;
{
  double size=.1;
  int i1,j1,i2,j2,color=0;
  float xe,ye,xs,ys;
  /*Window temp;*/
  int flag;
  /*temp=main_win;*/
  if(new_float("Size: ",&size))return;
  if(new_int("Color: ",&color))return;
  /* message_box(&temp,0,SCALEY-5*DCURY,"Choose start/end"); */
  MessageBox("Choose start/end");
  flag=rubber_band(&i1,&j1,&i2,&j2,1);
/*  XDestroyWindow(display,temp); */
  KillMessageBox();
  FlushDisplay();
  if(flag){
    scale_to_real(i1,j1,&xs,&ys);
    scale_to_real(i2,j2,&xe,&ye);
    if(i1==i2&&j1==j2)return;
    add_grob(xs,ys,xe,ye,size,type,color);
    redraw_all(); 
  }
}

void edit_object_com(int com)
{
  char ans,str[80];
  int i,j,ilab=-1,flag,type;
  float x,y;
  float dist=1e20,dd;

  MessageBox("Choose Object");
  flag=GetMouseXY(&i,&j);

 KillMessageBox();
  FlushDisplay();
  if(flag){
      scale_to_real(i,j,&x,&y);
      /* now search all labels to find the best */
      type=0;  /* label =  0, arrows, etc =1 */
      for(i=0;i<MAXLAB;i++){
	if(lb[i].use==1&&lb[i].w==draw_win){
	  dd=(x-lb[i].x)*(x-lb[i].x)+(y-lb[i].y)*(y-lb[i].y);
	  if(dd<dist){
	    ilab=i;
	    dist=dd;
	  }
	}
      }
      for(i=0;i<MAXGROB;i++){
	if(grob[i].use==1&&grob[i].w==draw_win){
	  dd=(x-grob[i].xs)*(x-grob[i].xs)+(y-grob[i].ys)*(y-grob[i].ys);
	  if(dd<dist){
	    ilab=i;
	    dist=dd;
	    type=1;
	  }
	}
      }
      if(ilab>=0&&type==0){
	switch(com){
	case 0:
	  sprintf(str,"Move %s ?", lb[ilab].s);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){


            MessageBox("Click on new position");
	    flag=GetMouseXY(&i,&j);

	    KillMessageBox();    
	    FlushDisplay();
	    if(flag){
	      scale_to_real(i,j,&x,&y);
	      lb[ilab].x=x;
	      lb[ilab].y=y;
	      clr_scrn();
	      redraw_all();
	    }
	  }
	  break;
	case 1 :
	  sprintf(str,"Change %s ?", lb[ilab].s);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){
	    new_string("Text: ",lb[ilab].s);
            new_int("Size 0-4 :",&lb[ilab].size);
	    /* new_int("Font  0-times/1-symbol :",&lb[ilab].font); */
	    if(lb[ilab].size>4)lb[ilab].size=4;
	    if(lb[ilab].size<0)lb[ilab].size=0;
	    clr_scrn();
	    redraw_all();
	  }
	  break;
	case 2:
	  sprintf(str,"Delete %s ?", lb[ilab].s);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){
	    lb[ilab].w=0;
	    lb[ilab].use=0;
	    clr_scrn();
	    redraw_all();
	  }
	  break;
	}
      }
      if(ilab>=0&&type==1){
	switch(com){
	case 0:
	  sprintf(str,"Move graphic at (%f,%f)",
		  grob[ilab].xs,grob[ilab].ys);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){


            MessageBox("Reposition");
	    flag=GetMouseXY(&i,&j);

            KillMessageBox();
	    FlushDisplay();
	    if(flag){
	      scale_to_real(i,j,&x,&y);
	      grob[ilab].xe=grob[ilab].xe-grob[ilab].xs+x;
	      grob[ilab].ye=grob[ilab].ye-grob[ilab].ys+y;
	      grob[ilab].xs=x;
	      grob[ilab].ys=y;
	      clr_scrn();
	      redraw_all();
	    }
	  }
	  break;
	case 1:
	  sprintf(str,"Change graphic at (%f,%f)",
		  grob[ilab].xs,grob[ilab].ys);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){
            if(grob[ilab].type>=MARKER)
	      select_marker_type(&grob[ilab].type);
	    new_float("Size ",&grob[ilab].size);
            new_int("Color :", &grob[ilab].color);
	    clr_scrn();
	    redraw_all();
	  }
	  break;
	case 2:
          sprintf(str,"Delete graphic at (%f,%f)",
		  grob[ilab].xs,grob[ilab].ys);
	  ans=(char)TwoChoice("Yes","No",str,"yn");
	  if(ans=='y'){
	    grob[ilab].w=0;
	    grob[ilab].use=0;
	    clr_scrn();
	    redraw_all();
	  }
	  break;
	}
      }

    }

}

void do_gr_objs_com(int com)
{
  switch(com){
  case 0: 
    cput_text();
    break;
  case 1:
     add_pntarr(ARROW);
    break;
  case 2:
     add_pntarr(POINTER);
    break;
  case 3:
     add_marker();
    break;
  case 6:
    add_markers();
    break;
    /*case 4:
    edit_object();
    break; */
  case 5:
    destroy_label(draw_win);
    destroy_grob(draw_win);
    clr_scrn();
    redraw_all();
    break;
  }
}

void do_windows_com(int c)
{
 switch(c){
	
	case 0: create_a_pop();
         		break;
 	case 1: 
		if(yes_no_box())kill_all_pops();
		break;
	case 3: 
		 xpp_ui.lower_plot_window();
		break;
	case 2: destroy_a_pop();
		break;
	case 5: set_restore(0);
		 break;
        case 4: set_restore(1);
		 break;
        case 6:
                SimulPlotFlag=1-SimulPlotFlag;
		break;
		/*  default: create_a_pop();
		    break; */
	}

 set_active_windows();
}

void set_restore(int flag)
  {
   int i;
      for(i=0;i<MAXPOP;i++){
    if(graph[i].w==draw_win){
	    graph[i].Restore=flag;
	    graph[i].Nullrestore=flag;
	return;
	}
  }
  }

int is_col_plotted(nc)
int nc;
{
  int i;
  int j,nv;

  for(i=0;i<MAXPOP;i++){
    if(graph[i].Use==1){
      nv=graph[i].nvars;
      for(j=0;j<nv;j++){
	if(graph[i].xv[j]==nc||graph[i].yv[j]==nc||graph[i].zv[j]==nc){

	  return 1;}
      }
    }
  }
  return 0;
}

void change_plot_vars(int k)
{
 int i,ip;
  int np;
  for(i=0;i<MAXPOP;i++){
    if(graph[i].Use){
      np=graph[i].nvars;
      for(ip=0;ip<np;ip++){
	if(graph[i].xv[ip]>k)
	  graph[i].xv[ip]=graph[i].xv[ip]-1;
	if(graph[i].yv[ip]>k)
	  graph[i].yv[ip]=graph[i].yv[ip]-1;
	if(graph[i].zv[ip]>k)
	  graph[i].zv[ip]=graph[i].zv[ip]-1;
      }
    }
  }
}

int check_active_plot(int k)
{
  int i,ip;
  int np;
  for(i=0;i<MAXPOP;i++){
    if(graph[i].Use){
      np=graph[i].nvars;
      for(ip=0;ip<np;ip++){
	if(graph[i].xv[ip]==k||graph[i].yv[ip]==k||graph[i].zv[ip]==k)
	  return 1;
      }
    }
  }
    return 0;
}

int graph_used(int i)
{
 return graph[i].Use;
} 
