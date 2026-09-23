#include <X11/Xlib.h>


#include "auto_x11.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "auto_nox.h"
#include "init_conds.h"
#include "derived.h"
#include "diagram.h"
#include "ggets.h"
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include "auto.bitmap"
#include "newhome.h"
#include "mykeydef.h"
#include "xpplim.h"
#include "autlim.h"
#include "math.h"
#include "rubber.h"
#include "menudrive.h"
#include "many_pops.h"
#include "color.h"
#include "integrate.h"
#include "browse.h"
#include "numerics.h"

#include "pop_list.h"


#define RUBBOX 0
#define RUBLINE 1

#define RIGHT 6
#define LEFT 2
#define ESC 27
#define TAB 10
#define BAD 0
#define FINE 13


#define STD_WID 460	  /* golden mean  */
#define STD_HGT 284
#define MAX_LEN_SBOX 25

#define xds(a) { XDrawString(display,w,gc,5,CURY_OFFb,a,strlen(a));return;}

#define SBW XSetWindowBorderWidth(display,w,1)

#define MAX_AUT_PER 10



#define MYMASK  (ButtonPressMask|KeyPressMask|ExposureMask|StructureNotifyMask	|LeaveWindowMask|EnterWindowMask| ButtonMotionMask)

#define SIMPMASK (ButtonPressMask | KeyPressMask|ExposureMask    |StructureNotifyMask)

void set_ivar();
void sleep();
void storeautopoint();
void redo_all_fun_tables();
extern Display *display;
extern int TrueColorFlag;
extern unsigned int MyBackColor,MyForeColor,MyMainWinColor,MyDrawWinColor;

extern int screen,storind,NODE;
extern GC gc, small_gc;
extern int DCURX,DCURXs,DCURY,DCURYs,CURY_OFFs,CURY_OFFb,CURY_OFF;
int STD_HGT_var =0;
int STD_WID_var =0;
int Auto_extra_wid,Auto_extra_hgt;
int Auto_x0,Auto_y0;
extern int load_all_labeled_orbits;
/* stuff for marking a branch  */
extern Window command_pop;

extern double TEND;
extern int AutoTwoParam;
extern int NAutoPar;
extern int Auto_index_to_array[8];
extern int AutoPar[8];

extern int xorfix;

extern int TipsFlag;
extern unsigned int MyBackColor,MyForeColor,MyMainWinColor,MyDrawWinColor,GrFore,GrBack;

extern char *auto_hint[],*aaxes_hint[],*afile_hint[],*arun_hint[],*no_hint[],*aspecial_hint[];
double atof();

extern double constants[];

extern int DONT_XORCross;


AUTOWIN AutoW;


extern BIFUR Auto;


extern GRABPT grabpt;


extern DIAGRAM *bifd;
extern DIAGRAM *CUR_DIAGRAM;


extern int NBifs;


/* **************************************************** 
   Code here 
*****************************************************/

void x11_ALINE(a,b,c,d)
     int a,b,c,d;
{
  XDrawLine(display,AutoW.canvas,small_gc,(a),(b),(c),(d));
}



void x11_ATEXT(a,b,c) 
     int a,b;
     char *c;
{
  XDrawString(display,AutoW.canvas,small_gc,(a),(b),(c),strlen(c));
}



void x11_clr_stab()
{
  int r=Auto.st_wid/4;
  XClearWindow(display,AutoW.stab);
  XDrawArc(display,AutoW.stab,small_gc,r,r,2*r,2*r,0,360*64);
}



void x11_auto_stab_line(int x,int y,int xp, int yp)
{
   XDrawLine(display,AutoW.stab,small_gc,x,y,xp,yp);
}

void x11_clear_auto_plot()
{
  XClearWindow(display,AutoW.canvas);
  redraw_auto_menus();
}


void x11_redraw_auto_menus()
{
  display_auto(AutoW.axes);
  display_auto(AutoW.numerics);
  display_auto(AutoW.grab);
  display_auto(AutoW.run);
  display_auto(AutoW.redraw);
  display_auto(AutoW.clear);
  display_auto(AutoW.per);
  display_auto(AutoW.param);
  display_auto(AutoW.kill);
  display_auto(AutoW.file);
  display_auto(AutoW.abort);
}







void x11_clear_auto_info()
{
 XClearWindow(display,AutoW.info);
}

void x11_draw_auto_info(char *bob,int x,int y)
{
   XDrawString(display,AutoW.info,small_gc,x,y,bob,strlen(bob));
}

void x11_refreshdisplay()
{
  XFlush(display);
}

int x11_byeauto_(iflag)
     int *iflag;
{
  XEvent event;
  Window w;
  char ch;
  if(Auto.exist==0)return(1);
  *iflag=0;
 while(XPending(display)>0){
 XNextEvent(display,&event);
 switch(event.type){
	case Expose: do_expose(event);
	  	     break;
	case ButtonPress:
	  w=event.xbutton.window;
	  if(w==AutoW.abort){SBW;*iflag=1;return(1);}
          break;
        case KeyPress:
	   ch=get_key_press(&event);
          if(ch==ESC){*iflag=1;return(0);}
	  break;
	  
	}
 }
 
 return(0);


}




void x11_Circle(x,y,r)
     int x,y,r;
{
  XDrawArc(display,AutoW.canvas,small_gc,x-r,y-r,r<<1,r<<1,0,360*64);
}


void x11_autocol(int col)
{
  set_scolor(col);

}


void x11_autobw()
{
XSetBackground(display,small_gc,MyBackColor);
XSetForeground(display,small_gc,MyForeColor);
}


int x11_auto_rubber(i1,j1,i2,j2,flag)
     int *i1,*i2,*j1,*j2,flag;
{
  return(rubber(i1,j1,i2,j2,AutoW.canvas,flag));
}

int x11_auto_pop_up_list(title,list,key,n,max,def,x,y,hints,httxt)
int def,n,max,x,y;
char *title,**list,*key,**hints,*httxt;
{
  Window temp=AutoW.base;
  return pop_up_list(&temp,title,list,key,n,max,def,x,y,hints,AutoW.hint,httxt);
}

void x11_XORCross(x,y)
     int x,y;
{

   if (DONT_XORCross)
   {
	   return;
   }

   if(xorfix)
   {
	   XSetForeground(display,small_gc,MyDrawWinColor);
 	   XSetBackground(display,small_gc,MyForeColor);   
   }

     XSetFunction(display,small_gc,GXxor);
      LineWidth(2);
     ALINE(x-8,y,x+8,y);
     ALINE(x,y+8,x,y-8);
     XSetFunction(display,small_gc,GXcopy);
    LineWidth(1);
   if(xorfix)
   {
	   XSetForeground(display,small_gc,MyForeColor);   
 	   XSetBackground(display,small_gc,MyDrawWinColor);
   }
  
  XFlush(display);
}


void x11_FillCircle(x,y,r)
     int x,y;
     int r;
{
  
    int  r2 = (int) (r / 1.41421356 + 0.5);
    int wh = 2 * r2;

    XFillArc(display, AutoW.canvas, small_gc, x - r2, y - r2, wh, wh, 0, 360*64);

}


void x11_auto_scroll_window()
{
  XEvent ev;
  int i=0,j=0;
  int i0=0,j0=0;
  int state=0;
  float xlo=Auto.xmin;
  float ylo=Auto.ymin;
  float xhi=Auto.xmax;
  float yhi=Auto.ymax;
  float dx=0,dy=0;
  int alldone=0;
  /*    printf("xin: %g %g %g %g\n",xlo,xhi,ylo,yhi); */
  XSelectInput(display,AutoW.canvas,
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
     i0=ev.xkey.x;
     j0=ev.xkey.y;

     /*  x0=Auto.xmin+(double)(i-Auto.x0)*(Auto.xmax-Auto.xmin)/(double)Auto.wid;
	 y0=Auto.ymin+(double)(Auto.y0-j+Auto.hgt)*(Auto.ymax-Auto.ymin)/(double)Auto.hgt;
	 printf("%d %d %g %g \n",i,j,x0,y0); */
        state=1;
    
     }
     break;
   case MotionNotify:
     if(state==1){
       i0=ev.xmotion.x;
       j0=ev.xmotion.y;
       dx=0.0;
       dy=0.0;
             auto_update_view(xlo+dx,xhi+dx,ylo+dy,yhi+dy);

       state=2;
       break;
     }
     if(state==2){
     i=ev.xmotion.x;
     j=ev.xmotion.y;
     /* x=Auto.xmin+(double)(i-Auto.x0)*(Auto.xmax-Auto.xmin)/(double)Auto.wid;
    y=Auto.ymin+(double)(Auto.y0-j+Auto.hgt)*(Auto.ymax-Auto.ymin)/(double)Auto.hgt;
    printf("%d %d %g %g \n",i,j,x,y,x0,y0);
     dx=-(x-x0)/2;
     dy=-(y-y0)/2; */
     dx=(float)(i0-i)*(Auto.xmax-Auto.xmin)/(float)Auto.wid;
     dy=(float)(j-j0)*(Auto.ymax-Auto.ymin)/(float)Auto.hgt;
     /*    printf("%d %d %d %d %g %g\n",i,j,i0,j0,dx,dy); */
      auto_update_view(xlo+dx,xhi+dx,ylo+dy,yhi+dy);
     
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




  

void x11_LineWidth(wid)
     int wid;
{
 int ls=LineSolid;
 int cs=CapButt;
 int js=JoinRound;
 XSetLineAttributes(display,small_gc,wid,ls,cs,js);
}


void auto_motion(ev)
     XEvent ev;
{
  Window w=ev.xmotion.window;
  if(Auto.exist==0)
    return;
  if(w==AutoW.canvas)
    auto_motion_xy(ev.xmotion.x,ev.xmotion.y);
}

void display_auto(w)
Window w;
{
	
  int ix,iy;
  if(Auto.exist==0)return;
  if(w==AutoW.canvas){if(AutoRedrawFlag==1)redraw_diagram();};
  if(w==AutoW.stab)
  {
  	XFlush(display);
  	int r=Auto.st_wid/4;
  	XDrawArc(display,AutoW.stab,small_gc,r,r,2*r,2*r,0,360*64);
  	if (CUR_DIAGRAM != NULL)
	{
  		traverse_out(CUR_DIAGRAM,&ix,&iy,1);/*clr_stab();*/
	}
	XFlush(display);
  }
  if(w==AutoW.axes)xds("Axes");
  if(w==AutoW.numerics)xds("Numerics");
  if(w==AutoW.grab)xds("Grab");
  if(w==AutoW.run)xds("Run");
  if(w==AutoW.redraw)xds("reDraw");
  if(w==AutoW.clear)xds("Clear");
  if(w==AutoW.per)xds("Usr period");
  if(w==AutoW.kill)xds("Close");
  if(w==AutoW.param)xds("Parameter");
  if(w==AutoW.file)xds("File");
  if(w==AutoW.abort)xds("ABORT");
  if(w==AutoW.hint){
    XClearWindow(display,w);
    XDrawString(display,w,gc,8,CURY_OFF,Auto.hinttxt,strlen(Auto.hinttxt));
    return;
  }
}


Window lil_button(root,x,y,name)
     Window root;
     char *name;
     int x,y;
{
  Window win;
  /*int width=strlen(name)*DCURX+5;
  */
  int width=12*DCURX;
  win=make_window(root,x,y,width,DCURY+1,1);
  XSelectInput(display,win,MYMASK);
  return(win);
}
  
void aw()
{
  XFlush(display);
  sleep(5);
}
  

void x11_make_auto(wname,iname)  /* this makes the auto window  */
     char *wname,*iname;

{
 int x,y,wid,hgt,addwid=16*DCURX,addhgt=3.0*DCURY,hinthgt=DCURY+6;
 Window base=0;
 int dely=DCURY+5;
 STD_HGT_var =20*DCURY;
 /*STD_WID_var =1.62*STD_HGT_var;*/
 STD_WID_var = 67*DCURX;
 int ymargin=4*DCURYs,xmargin=12*DCURXs;
 XTextProperty winname,iconname;
 XSizeHints size_hints;
 Auto_extra_wid=10+addwid;
 Auto_extra_hgt=addhgt+2*DCURY+hinthgt;
 wid=10+addwid+STD_WID_var+xmargin;
 hgt=addhgt+2*DCURY+STD_HGT_var+ymargin+hinthgt;
 x=addwid+5;
 y=DCURY;
 Auto_x0=x;
 Auto_y0=y;
 base=make_plain_window(RootWindow(display,screen),0,0,wid,hgt,4);
  XSetWindowBackground(display,base,MyMainWinColor);
 AutoW.base=base;

 strcpy(Auto.hinttxt,"hint");

 XSelectInput(display,base,ExposureMask|KeyPressMask|ButtonPressMask|
		StructureNotifyMask);

 XStringListToTextProperty(&wname,1,&winname);
 XStringListToTextProperty(&iname,1,&iconname);
  
 size_hints.flags=PPosition|PSize|PMinSize;
 size_hints.x=0;
 size_hints.y=0;
 size_hints.min_width=wid;
 size_hints.min_height=hgt;
 
 XClassHint class_hints;
 class_hints.res_name="";
 class_hints.res_class="";
 
 XSetWMProperties(display,base,&winname,&iconname,NULL,0,
		  &size_hints,NULL,&class_hints);

 make_icon((char*)auto_bits,auto_width,auto_height,base);

 AutoW.canvas=make_plain_window(base,x,y,STD_WID_var+xmargin,STD_HGT_var+ymargin,1);
 XSetWindowBackground(display,AutoW.canvas,MyDrawWinColor);
   XSelectInput(display,AutoW.canvas,MYMASK);


 x=DCURX;
 y=DCURY+STD_HGT_var+ymargin-8*DCURX;
 AutoW.stab=make_plain_window(base,x,y,12*DCURX,12*DCURX,2);
 Auto.st_wid=12*DCURX;
 x=DCURX+2;
 y=2*DCURY;
 Auto.hgt=STD_HGT_var;
 Auto.wid=STD_WID_var;
 Auto.x0=10*DCURXs;
 Auto.y0=2*DCURYs;
 AutoW.kill=lil_button(base,2,2,"Close");
 AutoW.param=lil_button(base,x,y,"Parameter");
 y+=dely;
 AutoW.axes=lil_button(base,x,y,"Axes");
 y+=dely;
 AutoW.numerics=lil_button(base,x,y,"Numerics");
 y+=dely;
 AutoW.run=lil_button(base,x,y,"Run");
  y+=dely;
 AutoW.grab=lil_button(base,x,y,"Grab");
 y+=dely;
 AutoW.per=lil_button(base,x,y,"Usr Function");
 y+=dely;
 AutoW.clear=lil_button(base,x,y,"Clear");
 y+=dely;
 AutoW.redraw=lil_button(base,x,y,"reDraw");
 y+=dely;
 AutoW.file=lil_button(base,x,y,"File");

 y+=3*dely;
 AutoW.abort=lil_button(base,x,y,"ABORT");

 y=DCURY+STD_HGT_var+ymargin+5;
 x=addwid+5;
 AutoW.info=make_plain_window(base,x,y,STD_WID_var+xmargin,addhgt,2);
 AutoW.hint=make_plain_window(base,x,y+addhgt+6,STD_WID_var+xmargin,DCURY+2,2);
  
 draw_bif_axes(); 


}
 


void resize_auto_window(XEvent ev)
{

    int wid,hgt,addhgt=3.5*DCURY;
    STD_HGT_var =20*DCURY;
    /*STD_WID_var =1.62*STD_HGT_var;*/
    STD_WID_var = 50*DCURX;
    int ymargin=4*DCURYs,xmargin=12*DCURXs;
    if(ev.xconfigure.window==AutoW.base){
    wid=ev.xconfigure.width-Auto_extra_wid;
    hgt=ev.xconfigure.height-Auto_extra_hgt;
    
    addhgt=3.0*DCURY;
    
    XResizeWindow(display,AutoW.canvas,wid,hgt);
    Window root;
    int xloc;
    int yloc;
    unsigned int cwid;
    unsigned int chgt;
    unsigned int cbwid;
    unsigned int cdepth;
    
    XGetGeometry(display,AutoW.canvas,&root,&xloc,&yloc,&cwid,&chgt,&cbwid,&cdepth);

    Auto.hgt=chgt-ymargin;
    Auto.wid=cwid-xmargin;
    /*  printf("%l %d %d %d %d\n", AutoW.info,xloc,yloc+chgt+4,wid,addhgt);
	printf("%l %d %d %d %d\n", AutoW.hint,xloc,yloc+chgt+addhgt+10,wid,DCURY+2);  */
    /* XMoveWindow(display,AutoW.info,xloc,yloc+chgt+4); */
     if(TrueColorFlag>0){ 
     XMoveResizeWindow(display,AutoW.info,xloc,yloc+chgt+4,wid,addhgt);
         
     XMoveResizeWindow(display,AutoW.hint,xloc,yloc+chgt+addhgt+10,wid,DCURY+2);
      } 
    
    
    int ix,iy; 
    
    if(NBifs<2)return;
    traverse_out(CUR_DIAGRAM,&ix,&iy,1);
    
  }
}
    
    


void a_msg(i,v)
     int i;
     int v;
{
  if(v==0||TipsFlag==0)return;
  snprintf(Auto.hinttxt,255,"%s",auto_hint[i]);
  display_auto(AutoW.hint);
}

/*  Auto event handlers   */


void auto_enter(w,v)
     Window w;
     int v;
{
  if(Auto.exist==0)return;
  if(w==AutoW.axes){XSetWindowBorderWidth(display,w,v); a_msg(1,v); return;}
  if(w==AutoW.numerics){ XSetWindowBorderWidth(display,w,v);a_msg(2,v);  return;}
  if(w==AutoW.grab){ XSetWindowBorderWidth(display,w,v); a_msg(4,v); return;}
  if(w==AutoW.run){ XSetWindowBorderWidth(display,w,v); a_msg(3,v); return;}
  if(w==AutoW.redraw){ XSetWindowBorderWidth(display,w,v);a_msg(7,v); return;}
  if(w==AutoW.clear){ XSetWindowBorderWidth(display,w,v); a_msg(6,v);return;}
  if(w==AutoW.per){ XSetWindowBorderWidth(display,w,v); a_msg(5,v); return;}
  if(w==AutoW.param){ XSetWindowBorderWidth(display,w,v);a_msg(0,v);return;}
   if(w==AutoW.kill){ XSetWindowBorderWidth(display,w,v);return;}
  if(w==AutoW.file){ XSetWindowBorderWidth(display,w,v); a_msg(8,v);return;}
}


void auto_button(ev)
     XEvent ev;
{
  Window w=ev.xbutton.window;
  if(Auto.exist==0)return;
  if(w==AutoW.axes){SBW;auto_plot_par(); return;}
  if(w==AutoW.numerics){SBW; auto_num_par(); return;}
  if(w==AutoW.grab){SBW; auto_grab(); return;}
  if(w==AutoW.run){SBW; auto_run(); return;}
  if(w==AutoW.redraw){SBW; redraw_diagram(); return;}
  if(w==AutoW.clear){SBW; draw_bif_axes(); return;}
  if(w==AutoW.per){SBW; auto_per_par(); return;}
  if(w==AutoW.param){SBW; auto_params(); return;}
  if(w==AutoW.kill){SBW; auto_kill(); return;}
  if(w==AutoW.file){SBW;auto_file(); return;}
}


void auto_kill()
{
  Auto.exist=0;
  waitasec(ClickTime);
  XDestroySubwindows(display,AutoW.base);
  XDestroyWindow(display,AutoW.base);
  
}

void auto_keypress(ev,used)
     XEvent ev;
     int *used;
{
  Window w=ev.xkey.window;
 /* 
  int maxlen=64;
  char buf[65];
  XComposeStatus comp;
  KeySym ks;  */
  char ks;
  Window w2;
  int rev;
  
  *used=0;
  if(Auto.exist==0)return;
  XGetInputFocus(display,&w2,&rev);

 if(w==AutoW.base||w==AutoW.canvas||w2==AutoW.base)
 {
   *used=1;
   ks=(char)get_key_press(&ev);
   /* XLookupString(&ev,buf,maxlen,&ks,&comp); */

   if(ks=='a'||ks=='A'){ auto_plot_par(); return;}
   if(ks=='n'||ks=='N'){ auto_num_par(); return;}
   if(ks=='G'||ks=='g'){ auto_grab(); return;}
   if(ks=='R'||ks=='r'){ auto_run(); return;}
   if(ks=='D'||ks=='d'){ redraw_diagram(); return;}
   if(ks=='C'||ks=='c'){ draw_bif_axes(); return;}
   if(ks=='U'||ks=='u'){ auto_per_par(); return;}
   if(ks=='P'||ks=='p'){ auto_params(); return;}
   if(ks=='F'||ks=='f'){ auto_file(); return;}

   
   if(ks==ESC){
			XSetInputFocus(display,command_pop,
				       RevertToParent,CurrentTime);
		   	return;
		      }
   

 }
  
}
 

/* xpp_ui.auto_grab_event: wait for a key (its code) or a click on the
   diagram (XPP_AUTO_CLICK and the pixel) */
int x11_auto_grab_event(int *x,int *y)
{
  XEvent ev;
  while(1){
    XNextEvent(display,&ev);
    if(ev.type==ButtonPress){
      if(ev.xmotion.window==AutoW.canvas){
        *x=ev.xmotion.x;
        *y=ev.xmotion.y;
        return XPP_AUTO_CLICK;
      }
    }
    else if(ev.type==KeyPress)
      return get_key_press(&ev);
  }
}

void x11_auto_show_hint(void)
{
  display_auto(AutoW.hint);
}

/* the grab is over: the XOR cross cannot be trusted to erase itself, so
   a taken point redraws everything, as traverse_diagram() always did */
void x11_auto_grab_end(int done)
{
  if (done == 1) {
    redraw_diagram();
    RedrawMark();
  }
}
