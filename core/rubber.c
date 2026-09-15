#include <X11/Xlib.h>
#include <stdlib.h> 
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <stdio.h>
#define RUBBOX 0
#define RUBLINE 1
#include "rubber.h"
#include "xpp_globals.h"
#include "axes2.h"
#include "graf_par.h"
#include "graphics.h"
#include "struct.h"
#include "mykeydef.h"
#include "ggets.h"
#include "many_pops.h"

extern Window draw_win;
extern Display *display;
extern int screen;
extern int xor_flag,xorfix;
extern GC gc,gc_graph;
extern unsigned int MyBackColor,MyForeColor,MyMainWinColor,MyDrawWinColor,GrFore,GrBack;


int rubber(x1,y1,x2,y2,w,f)
 int *x1,*y1,*x2,*y2,f;
 Window w;
 {
   XEvent ev;
   int there=0;
   int error=0;
  int dragx=0,dragy=0;
    int oldx=0,oldy=0;
  int state=0;
  xor_flag=1;
  XFlush(display);
  chk_xor();
   if(xorfix)
   {
   	XSetForeground(display,gc,MyDrawWinColor);
 	XSetBackground(display,gc,MyForeColor);  
   	/*XSetForeground(display,gc,GrFore);*/ 
   }
   
   XSelectInput(display,w,
   KeyPressMask|ButtonPressMask|ButtonReleaseMask|
		PointerMotionMask|ButtonMotionMask|ExposureMask);
  while(!there)
  {
   XNextEvent(display,&ev);
   switch(ev.type){ 
       case Expose: do_expose(ev);
        	xor_flag=1;
  		chk_xor();
   		if(xorfix)
   		{
   			XSetForeground(display,gc,MyDrawWinColor);
 			XSetBackground(display,gc,MyForeColor);  
     			/*XSetForeground(display,gc,GrFore);*/
     		} 
		break;
     
        case KeyPress:
		if(state>0)break;  /* too late Bozo   */
		there=1;
                error=1;
		break;
	case ButtonPress: 
		if(state>0)break;
		state=1;
		dragx=ev.xkey.x;
		dragy=ev.xkey.y;
		oldx=dragx;
		oldy=dragy;
		rbox(dragx,dragy,oldx,oldy,w,f);
		break;
	case MotionNotify:
		if(state==0)break;
		rbox(dragx,dragy,oldx,oldy,w,f);
		oldx=ev.xmotion.x;
		oldy=ev.xmotion.y;
		rbox(dragx,dragy,oldx,oldy,w,f);
		break;
	case ButtonRelease:
		if(state==0)break;
		there=1;
		rbox(dragx,dragy,oldx,oldy,w,f);
		break;
     }
   }
	xor_flag=0;
	chk_xor();

   if(xorfix)
   {
   	/*XSetForeground(display,gc,GrBack); */
  	XSetForeground(display,gc,MyForeColor);   
  	XSetBackground(display,gc,MyDrawWinColor);
   }

        if(!error){
	*x1=dragx;
	*y1=dragy;
	*x2=oldx;
	*y2=oldy;
        }

  XSelectInput(display,w,KeyPressMask|ButtonPressMask|ExposureMask|
ButtonReleaseMask|ButtonMotionMask);
  if(error)return(0);
  return(1);
 }
 

void rbox(i1,j1,i2,j2,w,f)
int i1,j1,i2,j2,f;
Window w;
{
 int x1=i1,x2=i2,y1=j1,y2=j2;
 if(f==RUBLINE){
   XDrawLine(display,w,gc,i1,j1,i2,j2);
   return;
 }
 if(x1>x2){x2=i1;x1=i2;}
 if(y1>y2){y1=j2;y2=j1;}
 rectangle(x1,y1,x2,y2,w);
}







void test_rot()
{
 int done=0;
 int kp;
 XEvent ev;
 double theta=MyGraph->Theta,phi=MyGraph->Phi;
 redraw_cube(theta,phi);
 while(done==0){
    XNextEvent(display,&ev);
   if(ev.type==KeyPress){
      kp=get_key_press(&ev);
      switch(kp){
      case UP:
        phi=phi+1;
        redraw_cube(theta,phi);
        break;
      case DOWN:
	phi=phi-1;
        redraw_cube(theta,phi);
        break;
      case LEFT:
	theta=theta+1;
        redraw_cube(theta,phi);
        break;
      case RIGHT:
	theta=theta-1;
        redraw_cube(theta,phi);
        break;
      case FINE:
       done=1;
       break;
      case ESC:
       done=-1;
       break;
      }
   }
 }
 if(done==1){
   MyGraph->Phi=phi;
   MyGraph->Theta=theta;
 }
 redraw_the_graph();
   
}

void x11_scroll_window()
{
  XEvent ev;
  int i=0,j=0;
  int state=0;
  float x,y,x0,y0;
  float xlo=MyGraph->xlo;
  float ylo=MyGraph->ylo;
    float xhi=MyGraph->xhi;
  float yhi=MyGraph->yhi;
  float dx,dy;
  int alldone=0;
  XSelectInput(display,draw_win,
   KeyPressMask|ButtonPressMask|ButtonReleaseMask|
		PointerMotionMask|ButtonMotionMask|ExposureMask);
  while(!alldone){
   XNextEvent(display,&ev);
   switch(ev.type){
   case KeyPress:
     alldone=1;
     break;
   case Expose:
     do_expose(ev);
     break;
   case ButtonPress:
     if(state==0){
     i=ev.xkey.x;
     j=ev.xkey.y;
     scale_to_real(i,j,&x0,&y0);
     state=1;
    
     }
     break;
   case MotionNotify:
     if(state==1){
     i=ev.xmotion.x;
     j=ev.xmotion.y;
     scale_to_real(i,j,&x,&y);
     dx=-(x-x0)/2;
     dy=-(y-y0)/2;

     update_view(xlo+dx,xhi+dx,ylo+dy,yhi+dy);
     }
     break;
   case ButtonRelease:
     state=0;
     xlo=xlo+dx;
     xhi=xhi+dx;
     ylo=ylo+dy;
     yhi=yhi+dy;
     break;

   }
  }
}
