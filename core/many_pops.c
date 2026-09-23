#include <X11/Xlib.h>
#include "xpplim.h"
#include "many_pops.h"
#include "grobs.h"
#include "xpp_globals.h"

#include "menudrive.h"
#include "pop_list.h"
#include "graphics.h"
#include <stdlib.h> 
#include <string.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <math.h>
#include "help_defs.h"
#include "graph.bitmap"
#include "struct.h"
#include "browse.h"
#include "init_conds.h"
#include "main.h"
#include "aniparse.h"

#include "load_eqn.h"
#include "rubber.h"
#include "ggets.h"
#include "axes2.h"
#include "txtread.h"
#include "menu.h"
#include "color.h"
#include "nullcline.h"
#include "my_ps.h"
#include "my_svg.h"
#include "graf_par.h"
#include "numerics.h"
#include "auto_x11.h"
#include "integrate.h"
#include "arrayplot.h"
#include "eig_list.h"



int manual_expose=0;
extern char *info_message;
extern BROWSER my_browser;
extern Atom deleteWindowAtom;
extern CURVE frz[MAXFRZ];
extern NCLINE nclines[MAXNCLINE];
extern int help_menu,screen;
extern int SCALEY,CURY_OFF,CURY_OFFs,DCURYs,DCURXs,DCURYb;
extern int storind;
extern int PltFmtFlag;
extern char *text_hint[];
extern char *edit_hint[];
extern char *no_hint[];
extern Display *display;
extern Window main_win,draw_win,command_pop,info_pop;
extern unsigned int MyBackColor,MyForeColor,MyMainWinColor,MyDrawWinColor,GrFore,GrBack;
extern GC gc, gc_graph,small_gc;
extern int COLOR,color_min;
extern int xor_flag,DCURX,DCURY;
int MINI_H=300;
int MINI_W=450;

extern int Xup;
double signum();

Window make_window();




typedef struct {
  char *name;
  char *does;
  unsigned int use;
} INTERN_SET;


typedef struct {
  double xlo,xhi,dx;
  double *y,*x;
  int n,flag,interp,autoeval;
  int xyvals;   
/* flag=0 if virgin array, flag=1 if already allocated; flag=2 for function
		         interp=0 for normal interpolation, interp=1 for 'step'
                         interp=2 for cubic spline
    table   and finally, xyvals=1 if both x and y vals are needed (xyvals=0
    is faster lookup )*/
  char filename[128],name[XPP_NAME_MAX+1];
}TABULAR;

extern TABULAR my_table[MAX_TAB];

extern int NTable;

extern INTERN_SET intern_set[MAX_INTERN_SET];
extern int Nintern_set;

void make_icon(icon,wid,hgt,w)
char *icon;
Window w;
int wid,hgt;
{
Pixmap icon_map; 
XWMHints wm_hints;
 icon_map=XCreateBitmapFromData(display,w,icon,wid,hgt);
wm_hints.initial_state=NormalState;
wm_hints.input=True;
wm_hints.icon_pixmap=icon_map;
 wm_hints.flags=StateHint|IconPixmapHint|InputHint;

XClassHint class_hints;
class_hints.res_name="";
class_hints.res_class="";
XSetWMProperties(display,w,NULL,NULL,NULL,0,NULL,&wm_hints,&class_hints);

}

void x11_title_text(string)
 char *string;
{
   gtitle_text(string,draw_win);
}


void gtitle_text(string,win)
 Window win;
 char *string;
 {
  XTextProperty wname,iname;
  GrCol();
  if(win!=graph[0].w){
  XStringListToTextProperty(&string,1,&wname);
  XStringListToTextProperty(&string,1,&iname);
 XSetWMProperties(display,win,&wname,&iname,NULL,0,NULL,NULL,NULL);
  }
  else
  {
  int len=strlen(string);
  int x,y;
  unsigned int w,h,bw,de;
 int xs,ys=2;
 Window root;
 XGetGeometry(display,win,&root,&x,&y,&w,&h,&bw,&de);
 xs=(w-len*DCURX)/2;
 if(xs<0)xs=0;
 Ftext(xs,ys,string,win);
 set_color(0);
 xline(0,18,w,18,win);
 }
BaseCol();
 }




  








  



void x11_destroy_a_pop()
 {
  int i;
  if(draw_win==graph[0].w)
  {
   respond_box("Okay","Can't destroy big window!");
   /*respond_box(main_win,0,0,"Okay","Can't destroy big window!");*/
   return;
  }
  for(i=1;i<MAXPOP;i++)
   if(graph[i].w==draw_win)break;
  if(i>=MAXPOP)return;
   select_window(graph[0].w);
  graph[i].Use=0;
  destroy_label(graph[i].w);
  destroy_grob(graph[i].w);
  waitasec(ClickTime);
  XDestroySubwindows(display,graph[i].w);
  XDestroyWindow(display,graph[i].w);
  num_pops--;
  }


void init_grafs(x,y,w,h)
int x,y,w,h;
{
 int i;
 GrCol();
 for(i=0;i<MAXLAB;i++)
 {
  lb[i].use=0;
  lb[i].w=(Window)0;
 } 
 for(i=0;i<MAXGROB;i++){
   grob[i].w=(Window)0;
   grob[i].use=0;
 }
 init_bd();
 for(i=0;i<MAXFRZ;i++)
   frz[i].use=0;
 /* for(i=0;i<MAXNCLINE ... */
 for(i=0;i<MAXPOP;i++)
   graph[i].Use=0;
 ActiveWinList[0]=0;
 init_all_graph();
 
  graph[0].w=XCreateSimpleWindow(display,main_win,x,y+4,w,h,2,GrFore,MyDrawWinColor);
 graph[0].w_info=info_pop;

 info_message=graph[0].gr_info;
 graph[0].Use=1;
 graph[0].Restore=1;
 graph[0].Nullrestore=1;
 graph[0].x0=x;
 graph[0].y0=y;
 graph[0].Height=h;
 graph[0].Width=w;
 XSelectInput(display,graph[0].w,KeyPressMask|ButtonPressMask|ExposureMask|
	      ButtonReleaseMask|ButtonMotionMask| StructureNotifyMask);
 num_pops=1;
 XMapWindow(display,graph[0].w);
 draw_win=graph[0].w;
 current_pop=0;
 get_draw_area();
 select_sym(graph[0].w);
 BaseCol();
}
/*
 draw_help()
{
 	switch(help_menu){
		case MAIN_HELP: help(); break;
		case NUM_HELP : help_num();break;
	
		case FILE_HELP: help_file();break;
	
}
}
*/



int rotate3dcheck(ev)
     XEvent ev;
{
  Window w=ev.xbutton.window;
  XEvent z;
  int xini,yini,dx,dy;
  double theta,phi;
  if(w==draw_win&&MyGraph->ThreeDFlag){
    xini=ev.xbutton.x;
    yini=ev.xbutton.y;
     phi=MyGraph->Phi;
     theta=MyGraph->Theta;
    while(1){
       XNextEvent(display,&z);
       if(z.type==ButtonRelease){
	 do_axes();
	 redraw_all();
	 hi_lite(draw_win);
	  return 1;
       }
       if(z.type==MotionNotify){
	 dx=z.xmotion.x-xini;
	 dy=z.xmotion.y-yini;
	  MyGraph->Phi=phi-(double)dy;
	  MyGraph->Theta=theta-(double)dx;
	  redraw_cube_pt(MyGraph->Theta,MyGraph->Phi);
	 
       }
    }
  }
  return 0;
}


void do_motion_events(ev)
     XEvent ev;
{
  int i=ev.xmotion.x;
  int j=ev.xmotion.y;
  float x,y;
  char buf[256];
  slider_motion(ev);
#ifdef AUTO
  auto_motion(ev);
#endif
  if(ev.xmotion.window==draw_win){
    scale_to_real(i,j,&x,&y);
    sprintf(buf,"x=%f y=%f ",x,y);
    canvas_xy(buf);
  }
}
 
void do_expose(ev)
 XEvent ev;
 {
  int i;
  int cp=current_pop;
  Window temp;
  
  temp=draw_win;
  top_button_draw(ev.xany.window);
  expose_aplot(ev.xany.window); 
  /* redraw_txtview(ev.xany.window);  */
  ani_expose(ev.xany.window); 
  expose_my_browser(ev);
  /* draw_info_pop(ev.xany.window); */
  RedrawMessageBox(ev.xany.window);
  draw_eq_list(ev.xany.window);
  draw_eq_box(ev.xany.window);
  do_box_expose(ev.xany.window);
  expose_slides(ev.xany.window);
  menu_expose(ev.xany.window);
#ifdef AUTO
  display_auto(ev.xany.window);
#endif 
  /* if(ev.xexpose.window==menu_pop){
	draw_help();
 	
   return;
    }
    */
  if(manual_expose==0){
  GrCol();

     for(i=0;i<MAXPOP;i++){
       if((graph[i].Use)&&(ev.xexpose.window==graph[i].w_info)){
	 XClearWindow(display,graph[i].w_info);
	 if(i==0){
	   BaseCol();
	   XDrawString(display,graph[i].w_info,gc,5,CURY_OFF,
		       graph[i].gr_info,strlen(graph[i].gr_info));
	 }
	 else{
	   SmallBase();
	   XDrawString(display,graph[i].w_info,small_gc,0,CURY_OFFs,
		     graph[i].gr_info,strlen(graph[i].gr_info));
	    SmallGr(); 
	 }
	
       }
	if((ev.type==Expose)&&(graph[i].Use)&&(ev.xexpose.window==graph[i].w)){
	  /* redraw_dfield(); */

		current_pop=i;
		MyGraph=&graph[i];
		draw_win=graph[i].w;
  		get_draw_area();
 		do_axes();
  		if(graph[i].Restore)restore(0,my_browser.maxrow);
		draw_label(graph[i].w);
		draw_freeze(graph[i].w);
		if(graph[i].Nullrestore)restore_nullclines();
	}
     }
  } /* namual expose */  
   draw_win=temp;
   MyGraph=&graph[cp];
   current_pop=cp;
   hi_lite(draw_win);
   get_draw_area();
   BaseCol();
   SmallBase();
 }


void resize_all_pops(wid,hgt)
int wid,hgt;
{
 int nw=wid-16-16*DCURX+7,nh=hgt-3*DCURYb-4*DCURYs-24;
 nw=4*((nw/4));
 nh=4*((nh/4));
 XResizeWindow(display,graph[0].w,nw,nh);
 graph[0].Width=nw;
 graph[0].Height=nh;
  get_draw_area();
}

void x11_kill_all_pops()
{
 int i;
 select_window(graph[0].w);

 for(i=1;i<MAXPOP;i++)
 if(graph[i].Use){
   graph[i].Use=0; 
   destroy_label(graph[i].w);
   destroy_grob(graph[i].w);
   
	XDestroySubwindows(display,graph[i].w);
        XDestroyWindow(display,graph[i].w);
      
 }
 num_pops=1;
}



void x11_create_a_pop()
 {
  int i,index;

  for(i=1;i<MAXPOP;i++)
  if(graph[i].Use==0)break;
  if(i>=MAXPOP)
  {
   /*respond_box(main_win,0,0,"Okay","Too many windows!");*/
   respond_box("Okay","Too many windows!");
   return;
  }
index=i;


graph[index].w=XCreateSimpleWindow(display,RootWindow(display,screen),0,0,MINI_W,MINI_H,2,GrFore,GrBack);
 graph[index].w_info=make_window(graph[index].w,10,0,40*DCURXs,DCURYs,0);
  XSetWindowBackground(display,graph[i].w,MyDrawWinColor);
  
  copy_graph(index,current_pop);
  graph[index].Width=MINI_W;
  graph[index].Height=MINI_H;
  graph[index].x0=0;
  graph[index].y0=0;
  num_pops++;
  make_icon((char*)graph_bits,graph_width,graph_height,graph[index].w);
  XSelectInput(display,graph[index].w,KeyPressMask|ButtonPressMask
	       |ExposureMask|ButtonReleaseMask|ButtonMotionMask);
  XMapWindow(display,graph[index].w);
  XRaiseWindow(display,graph[index].w); 
  XSetWMProtocols(display, graph[index].w, &deleteWindowAtom, 1);
      select_window(graph[index].w);
      /*  select_window(graph[0].w);
	  select_window(graph[index].w); */
   XRaiseWindow(display,graph[0].w);
 /*  XDestroyWindow(display,temp); */
}

void x11_GrCol()
{
 XSetForeground(display,gc,GrFore);
 XSetBackground(display,gc,GrBack);
 
}

void x11_BaseCol()
{
 XSetForeground(display,gc,MyForeColor);
 XSetBackground(display,gc,MyBackColor);
}

void x11_SmallGr()
{
 XSetForeground(display,small_gc,GrFore);
 XSetBackground(display,small_gc,GrBack);
}

void x11_SmallBase()
{
 XSetForeground(display,small_gc,MyForeColor);
 XSetBackground(display,small_gc,MyBackColor);
}

void select_window(w)
Window w;
{
 int i;

 if(w==draw_win)return;
 GrCol();
 if(w==graph[0].w)current_pop=0;
 else{
 
  for(i=1;i<MAXPOP;i++)if((graph[i].Use)&&(w==graph[i].w))current_pop=i;
 }
 MyGraph=&graph[current_pop];
 lo_lite(draw_win);
 draw_win=w;
 hi_lite(w);
  XRaiseWindow(display,w); 
 get_draw_area();
BaseCol();
}
 
void set_gr_fore()
{
 XSetForeground(display,gc,GrFore);
}

void set_gr_back()
{
 XSetForeground(display,gc,GrBack);
}

void hi_lite(wi)
Window wi;
{
  set_gr_fore();
  select_sym(wi);
 
}

void lo_lite(wi)
Window wi;
{
 set_gr_back();
 bar(0,0,5,5,wi);
 
}

void select_sym(w)
Window w;
{
 bar(0,0,5,5,w);
}
 
void x11_canvas_xy(buf)
char *buf;
{
  XClearWindow(display,MyGraph->w_info);
  strcpy(MyGraph->gr_info,buf);
  if(MyGraph->w_info==info_pop){
    BaseCol();
    XDrawString(display,MyGraph->w_info,gc,5,CURY_OFF,buf,strlen(buf));
  }
  else{
    SmallBase();
    XDrawString(display,MyGraph->w_info,small_gc,0,CURY_OFFs,buf,strlen(buf));
    /* SmallGr(); */
  }
}  

void check_draw_button(ev)
XEvent ev;
{
 int k;
 char buf[256];

 int button;
 int i,j;
 float x,y;
 int flag=0;
 Window w;
 button=ev.xbutton.button;
 w=ev.xbutton.window;
 i=ev.xbutton.x;
 j=ev.xbutton.y;
 if(button==1){          /* select window   */

 for(k=1;k<MAXPOP;k++)
 if((graph[k].Use)&&(w==graph[k].w))flag=1;
   if((w==graph[0].w)||(flag==1))select_window(w);
  }
 else  /* any other button   */
 {
   if(w!=draw_win)return;
   scale_to_real(i,j,&x,&y);
 sprintf(buf,"x=%f y=%f ",x,y);
 canvas_xy(buf);
  }
}  

 
   







