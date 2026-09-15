#include <X11/Xlib.h>
/* The animation (toon) window: the VCR buttons, the off-screen pixmap the
   frames are drawn into, playback and frame grabbing. Moved out of
   aniparse.c; the animation language, its evaluation and the geometry of
   every drawing command are core (aniparse.c) and draw through the
   xpp_ui.ani_* primitives implemented at the end of this file. */
#include "aniparse.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "color.h"
#include "my_rhs.h"
#include "ggets.h"
#include "many_pops.h"
#include "pop_list.h"
#include "browse.h"
#include "scrngif.h"
#include "integrate.h"
#include "toons.h"
#include "aniwin.bitmap"
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define INIT_C_SHIFT 0

/* who knows how the colors are ordered */
#ifdef BGR
#define MY_BLUE hibits
#define MY_GREEN midbits
#define MY_RED lobits
#else

#define MY_BLUE lobits
#define MY_GREEN midbits
#define MY_RED hibits
#endif

#define FIRSTCOLOR 30

extern int TrueColorFlag;
extern int colorline[];
extern Display *display;
extern XFontStruct *symfonts[5],*romfonts[5];
extern int color_total,screen;
extern int DCURX,DCURXs,DCURY,DCURYs,CURY_OFFs,CURY_OFF,NODE;
extern int NMarkov;
extern GC small_gc;
extern BROWSER my_browser;
extern int ani_speed,ani_speed_inc,n_anicom,n_ani_grab,ani_grab_flag;
extern int use_ani_file;
extern MPEG_SAVE mpeg;
extern int show_grab_points;

Pixmap ani_pixmap;
GC ani_gc;

void x11_new_vcr()
{
  int tt,i;
  if(vcr.iexist==1)return;
  tt=gettimenow();
  i=(10+(tt%10))%10;
  if(i>=0&&i<10)
    create_vcr(toons[i]);
  else
    create_vcr("Wanna be a member");
}

void create_vcr(name)
     char *name;
{
 unsigned int valuemask=0;
 XGCValues values;
 Window base;
 int wid=280,hgt=350;
 /*XWMHints wm_hints;*/
 XSizeHints size_hints;
  
 XTextProperty winname,iconname;

 base=make_plain_window(RootWindow(display,screen),0,0,5*12*DCURXs+8*DCURXs+4,20*(DCURYs+6),1);
 vcr.base=base;
  size_hints.flags=PPosition|PSize|PMinSize;
  size_hints.min_width=51*DCURXs;
  size_hints.min_height=300;
 XStringListToTextProperty(&name,1,&winname);
 XStringListToTextProperty(&name,1,&iconname);
 XSetWMProperties(display,base,&winname,&iconname,NULL,0,&size_hints,NULL,NULL);
  make_icon((char *)aniwin_bits,aniwin_width,aniwin_height,base);
 vcr.wfile   = br_button(base,0,0,"File",0);
 vcr.wgo = br_button(base,0,1,"Go",0);
 vcr.wreset = br_button(base,0,2,"Reset",0);
 vcr.wskip=br_button(base,0,3,"Skip",0);
 vcr.wfast   = br_button(base,1,0,"Fast",0);
 vcr.wslow = br_button(base,1,1,"Slow",0);
  vcr.wup = br_button(base,1,2,">>>>",0);
 vcr.wdn = br_button(base,1,3,"<<<<",0);
 vcr.wgrab=br_button(base,2,3,"Grab",0);
 vcr.slider=make_window(base,DCURXs,7+4*DCURYs,48*DCURXs,DCURYs+4,1);
 vcr.slipos=0;
 vcr.sliwid=48*DCURXs;
 vcr.wpause = br_button(base,2,0,"Pause",0);
 vcr.wmpeg = br_button(base,2,1,"MPeg",0);
 vcr.kill=br_button(base,2,2,"Close",0);

 vcr.wfly=make_window(base,4*12*DCURXs,4,5+DCURXs+5,(DCURYs+6)-4,1);
 /*   vcr.kill=make_window(base,5*12*DCURXs,(DCURYs+6)+4,8*DCURXs,DCURYs+1,1); */
 vcr.view=make_plain_window(base,10,100,wid,hgt,2);
 ani_gc=XCreateGC(display,vcr.view,valuemask,&values);
 vcr.hgt=hgt;
 vcr.wid=wid;
 ani_pixmap=  XCreatePixmap(display,RootWindow(display,screen),vcr.wid,vcr.hgt,
		  DefaultDepth(display,screen));
 if(ani_pixmap==0){
   err_msg("Failed to get the required pixmap");
   XFlush(display);
   waitasec(ClickTime);
   XDestroySubwindows(display,base);
   XDestroyWindow(display,base);
   vcr.iexist=0;
   return;
 }
 vcr.iexist=1;

 XSetFunction(display,ani_gc,GXcopy);
 XSetForeground(display,ani_gc,WhitePixel(display,screen));
 XFillRectangle(display,ani_pixmap,ani_gc,0,0,vcr.wid,vcr.hgt);
 XSetForeground(display,ani_gc,BlackPixel(display,screen));
 XSetFont(display,ani_gc,romfonts[0]->fid);
 tst_pix_draw();
 get_global_colormap(ani_pixmap);
 ani_view_created();
}

void ani_border(w,i)
     Window w;
     int i;
{
    if(w==vcr.wgrab||w==vcr.wgo||w==vcr.wreset||w==vcr.wpause||w==vcr.wfast||w==vcr.wfile
       ||w==vcr.wslow||w==vcr.wmpeg||w==vcr.wup||w==vcr.wdn||w==vcr.wskip||w==vcr.kill)
      XSetWindowBorderWidth(display,w,i);
}

void destroy_vcr()
{
  vcr.iexist=0;
  XDestroySubwindows(display,vcr.base);

   XDestroyWindow(display,vcr.base);
}

int check_ani_pause(ev)
     XEvent ev;
{
  if((vcr.iexist==0)||(!animation_on_the_fly)) return 0;
  if(ev.type==ButtonPress && ev.xbutton.window==vcr.wpause) return(27);
  return(0);
}

void do_ani_events(ev)
     XEvent ev;
{
 int x,y;
 /*Window w;*/
 if(vcr.iexist==0)return;
 switch(ev.type){
 case ConfigureNotify:
   if(ev.xconfigure.window!=vcr.base)return;
   x=ev.xconfigure.width;
   y=ev.xconfigure.height;
   x=(x)/8;
   x=8*x;
   y=(y)/8;
   y=y*8;
   ani_resize(x,y);
   break;
 case EnterNotify:
   ani_border(ev.xexpose.window,2);
   break;
 case LeaveNotify:
   ani_border(ev.xexpose.window,1);
   break;
 case MotionNotify:
   do_ani_slider_motion(ev.xmotion.window,ev.xmotion.x);
   if(ani_grab_flag == 0)break;
   ani_motion_stuff(ev.xmotion.window,ev.xmotion.x,ev.xmotion.y);
   break;
 case ButtonRelease:
   if(ani_grab_flag==0)break;
   ani_buttonx(ev,0);
   break;
 case ButtonPress:
   ani_buttonx(ev,1);
    break;
 }
}

void ani_motion_stuff(Window w,int x,int y)
{
  if(w==vcr.view)
    update_ani_motion_stuff(x,y);
}

void ani_buttonx(XEvent ev,int flag)
{
  Window w=ev.xbutton.window;
  /*   ADDED FOR THE GRAB FEATURE IN ANIMATOR  This is BUTTON PRESS */
  if((w==vcr.view)&&(ani_grab_flag==1)){
    ani_grab_mouse(flag,ev.xbutton.x,ev.xbutton.y);
    return;
  }
  if(flag==0)return;
  /*   END OF ADDED STUFF  ************************/


  ani_button(w);
}

void ani_button(w)
     Window w;
{
  if((ani_grab_flag==1))return; 
   /* Grab button resets and shows first frame */ 
   if(w==vcr.wgrab){
     if(n_ani_grab==0)return;
     ani_grab_start();
    }
  if(w==vcr.wmpeg)
    ani_create_mpeg();
  if(w==vcr.wgo)
  
    {ani_flip();} 
  if(w==vcr.wskip){
    Window fw;
    int rev;
    XGetInputFocus(display,&fw,&rev);
    ani_newskip();
    XSetInputFocus(display,fw,rev,CurrentTime);
  }
  if(w==vcr.wup)
    ani_flip1(1);
  if(w==vcr.wdn)
    ani_flip1(-1);
  if(w==vcr.wfile)
    get_ani_file(NULL);
  if(w==vcr.wfly){
    animation_on_the_fly=1-animation_on_the_fly;
    check_on_the_fly();
  }
  if(w==vcr.wreset){
    ani_reset();
  }
  if(w==vcr.kill){
    destroy_vcr();
  }
}

void do_ani_slider_motion(Window w, int x)
{
  int l=48*DCURXs,x0=x;
  int mr=my_browser.maxrow;
  int k;
  if(w!=vcr.slider)
    return;
  if(mr<2)return;
  if(x0>l-2)x0=l-2;
  vcr.slipos=x0;
  draw_ani_slider(w,x0);
  k=x0*mr/l;
  vcr.pos=0;
  ani_flip1(0);
  ani_flip1(k);

  
}

void redraw_ani_slider()
{
  int k=vcr.pos;
  int l=48*DCURXs;
  int xx;
  int mr=my_browser.maxrow;
  if(mr<2)return;
  xx=(k*l)/mr;
  draw_ani_slider(vcr.slider,xx);
}

void draw_ani_slider(Window w,int x)

{
  int hgt=DCURYs+4,l=48*DCURXs;
  int x0=x-2,i;
  if(x0<0)x0=0;
  if(x0>(l-4))x0=l-4;
  XClearWindow(display,w);
  for(i=0;i<4;i++)
    XDrawLine(display,w,small_gc,x0+i,0,x0+i,hgt);
}

void ani_expose(w)
Window w;
{
  if(vcr.iexist==0)return;
  if(w==vcr.wgrab)XDrawString(display,w,small_gc,5,CURY_OFFs,"Grab",4);
  if(w==vcr.view)
    XCopyArea(display,ani_pixmap,vcr.view,ani_gc,0,0,vcr.wid,vcr.hgt,0,0);
  if(w==vcr.wgo)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Go  ",4);
  if(w==vcr.wup)
    XDrawString(display,w,small_gc,5,CURY_OFFs," >>>>",5);
   if(w==vcr.wskip)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Skip",4);
  if(w==vcr.wdn)
    XDrawString(display,w,small_gc,5,CURY_OFFs," <<<<",5);
  if(w==vcr.wfast)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Fast",4);
  if(w==vcr.wslow)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Slow",4);
 
  if(w==vcr.slider)
    draw_ani_slider(w,vcr.slipos);
  if(w==vcr.wpause)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Pause",5);
  if(w==vcr.wreset)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Reset",5);
   if(w==vcr.kill)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"Close",5);
  if(w==vcr.wfile)
     XDrawString(display,w,small_gc,5,CURY_OFFs,"File",4);
  if(w==vcr.wmpeg)
    XDrawString(display,w,small_gc,5,CURY_OFFs,"MPEG",4);
  if(w==vcr.wfly)
    check_on_the_fly();
}

void ani_resize(x,y)
     int x,y;
{
 int ww=x-(2*4);
 int hh=y-((2.5*(DCURYs+6))+5);
 if(ww==vcr.wid&&hh==vcr.hgt)return;
 XFreePixmap(display,ani_pixmap);

 vcr.hgt=5*((y-((4.5*(DCURYs+6))+5))/5);
 vcr.wid=4*((x-(2*4))/4);
 
 /*This little safety check prevents a <X Error of failed request:  BadValue>
 from occuring if the user shrinks the window size smaller than the vcr.hgt | vcr.wid
 */
 if (vcr.hgt < 1)
 	vcr.hgt = 1;
 if (vcr.wid < 1)
 	vcr.wid = 1;
	
	
 XMoveResizeWindow(display,vcr.view,4,4.5*(DCURYs+6),vcr.wid,vcr.hgt);
 ani_pixmap=  XCreatePixmap(display,RootWindow(display,screen),vcr.wid,vcr.hgt,
		  DefaultDepth(display,screen));
 if(ani_pixmap==0){
   err_msg("Failed to get the required pixmap");
   XFlush(display);
   XDestroySubwindows(display,vcr.base);
   XDestroyWindow(display,vcr.base);
   vcr.iexist=0;
   return;
 } 
/*  XSetFunction(display,ani_gc,GXclear);
 XCopyArea(display,ani_pixmap,ani_pixmap,ani_gc,0,0,vcr.wid,vcr.hgt,0,0);
 */
 XSetFunction(display,ani_gc,GXcopy);
 XSetForeground(display,ani_gc,WhitePixel(display,screen));
 XFillRectangle(display,ani_pixmap,ani_gc,0,0,vcr.wid,vcr.hgt);
 XSetForeground(display,ani_gc,BlackPixel(display,screen));
 tst_pix_draw();
}

void check_on_the_fly()
{
  XClearWindow(display,vcr.wfly);
  if(animation_on_the_fly)
  {  
    	XDrawString(display,vcr.wfly,small_gc,5,1.5*CURY_OFFs,"*",1); 
  }
}

void ani_flip()
{
 double y[MAXODE];
 double t;
 char fname[256];
 FILE *angiffile=NULL;
 float **ss;
 int i,row,done;
 int mpeg_frame=0,mpeg_write=0,count=0;
 XEvent ev;
 Window w;
 /*Window root;
 unsigned int he,wi,bw,d;
 int x0,y0;
 */
 done=0;
 if(n_anicom==0)return;
 if(my_browser.maxrow<2)return;
 ss=my_browser.data;
 set_ani_perm(); /* evaluate all permanent structures  */
 /* check avi_flags for initialization */
 if(mpeg.aviflag==1){
  angiffile=fopen("anim.gif","wb");
  set_global_map(1);
 }
 count=0;  
 while(!done){ /* Ignore all events except the button presses */
 if(XPending(display)>0)
   {
     XNextEvent(display,&ev);
     switch(ev.type){
     case ButtonPress:
       w=ev.xbutton.window;
       if(w==vcr.wpause){
	 done=1;
	 break;
       }
       if(w==vcr.wfast){
	 ani_speed=ani_speed-ani_speed_inc;
	 if(ani_speed<0)ani_speed=0;
	 break;
       }
       if(w==vcr.wslow){
	 ani_speed=ani_speed+ani_speed_inc;
	 if(ani_speed>100)ani_speed=100;
	 break;
       }
       break;
     }
   }
  /* Okay no events  so lets go! */     
 
 /* first set all the variables */
 XSetForeground(display,ani_gc,WhitePixel(display,screen));
 XFillRectangle(display,ani_pixmap,ani_gc,0,0,vcr.wid,vcr.hgt);
 XSetForeground(display,ani_gc,BlackPixel(display,screen));
 row=vcr.pos;
 t=(double)ss[0][row];
 for(i=0;i<NODE+NMarkov;i++)
   y[i]=(double)ss[i+1][row];
 set_fix_rhs(t,y);


 /* now draw the stuff  */

 render_ani();
 
 /*  done drawing   */
 
 XCopyArea(display,ani_pixmap,vcr.view,ani_gc,0,0,vcr.wid,vcr.hgt,0,0);

 XFlush(display);
 
  waitasec(ani_speed); 
  if(mpeg.aviflag==1||mpeg.flag>0)
    waitasec(5*ani_speed);
 vcr.pos=vcr.pos+vcr.inc;
 if(vcr.pos>=my_browser.maxrow){
   done=1;
   vcr.pos=0;
   reset_comets();
 }

/* now check mpeg stuff */
 if(mpeg.flag>0&&((mpeg_frame%mpeg.skip)==0)){
     sprintf(fname,"%s_%d.ppm",mpeg.root,mpeg_write);
     mpeg_write++;
     writeframe(fname,ani_pixmap,vcr.wid,vcr.hgt);
 }
 mpeg_frame++;
 /* now check AVI stuff */

 if(mpeg.aviflag==1)
   /* add_ani_gif(ani_pixmap,angiffile,count); */
   {
     add_ani_gif(vcr.view,angiffile,count);

   }

 count++;
 }
/* always stop mpeg writing */
mpeg.flag=0;
 if(mpeg.aviflag==1){
   end_ani_gif(angiffile);
   fclose(angiffile);
   set_global_map(0);
 }
   
}

int getppmbits(Window window,int *wid,int *hgt, unsigned char *out)
{
  XImage *ximage;
  Colormap cmap;
  unsigned long value;
  int i;
  int CMSK=0,CSHIFT=0,CMULT=0;
  int bbp=0,bbc=0;
  int lobits,midbits,hibits;
  /*int vv; Not used anywhere?*/
  unsigned x,y;
  XColor palette[256];
  XColor pix;
  unsigned char *dst,*pixel;
  cmap = DefaultColormap(display,screen);

  ximage=XGetImage(display,window,0,0,*wid,*hgt,AllPlanes,ZPixmap);
  
  if(!ximage){
  
    return -1;
  }
  /* this is only good for 256 color displays */
  for(i = 0; i < 256; i++)
    palette[i].pixel = i;
  XQueryColors(display,
	       cmap,
	       palette,
	       256);
  if(TrueColorFlag==1){
    bbp=ximage->bits_per_pixel; /* is it 16 or 24 bit */
    if(bbp>24)bbp=24;
    bbc=bbp/3;  /*  divide up the 3 colors equally to bbc bits  */
    CMSK=(1<<bbc)-1;  /*  make a mask  2^bbc  -1  */
    CSHIFT=bbc;       /*  how far to shift to get the next color */
    CMULT=8-bbc;       /* multiply 5 bit color to get to 8 bit */
  }
  /* plintf("CMULT=%d CMSK=%d CSHIFT=%d \n",CMULT,CMSK,CSHIFT); */
  *wid=ximage->width;
  *hgt=ximage->height;
  pixel=(unsigned char*)ximage->data;
  dst=out;
  for(y=0;y < (unsigned)(ximage->height); y++) {
    for (x = 0; x < (unsigned)(ximage->width); x++) {
      if(TrueColorFlag==1){
       
	/*  use the slow way to get the pixel 
            but then you dont need to screw around
            with byte order etc  
	*/
	value=XGetPixel(ximage,x,y)>>INIT_C_SHIFT;
	/*vv=value; Not used?*/
	/*  get the 3 colors   hopefully  */
	lobits=value&CMSK;
	value=value>>CSHIFT;
	if(bbc==5)
	  value=value>>1;
	midbits=value&CMSK;
	value=value>>CSHIFT;
	hibits=value&CMSK;
	/*	       if(y==200&&(x>200)&&(x<400))
	 plintf("(%d,%d): %x %x %x %x \n",x,y,vv,MY_RED,MY_GREEN,MY_BLUE);
	*/
	 /* store them for ppm dumping  */
	*dst++=(MY_RED<<CMULT);
	*dst++=(MY_GREEN<<CMULT);
	*dst++=(MY_BLUE<<CMULT);
      }
      else
	{
	  /* 256 color is easier sort of  */
	    pix = palette[*pixel++];
	    *dst++ = pix.red;
	    *dst++ = pix.green;
	    *dst++ = pix.blue;
	}
    }
  }
  /* XDestroyImage(ximage); */

  return(1);
}

int writeframe(filename,window,wid,hgt)
     Window window;
     char *filename;
     int wid,hgt;
{
  int fd;
  XImage *ximage;
  Colormap cmap;
  unsigned long value;
  int i;
  int CMSK=0,CSHIFT=0,CMULT=0;
  int bbp=0,bbc=0;
  int lobits,midbits,hibits;
  /*int vv; Not used anywhere...*/
  unsigned x,y;
  char head[100];
  XColor palette[256];
  XColor pix;
  unsigned char *pixel;
  unsigned area;
  unsigned char *out,*dst;
  cmap = DefaultColormap(display,screen);
  ximage=XGetImage(display,window,0,0,wid,hgt,AllPlanes,ZPixmap);
  if(!ximage){
    return -1;
  }
  /* this is only good for 256 color displays */
  for(i = 0; i < 256; i++)
    palette[i].pixel = i;
  XQueryColors(display,
	       cmap,
	       palette,
		 256);
  fd=creat(filename,0666);
  if(fd==-1){
    return -1;
  }
  /*    this worked for me - but you may want to change
        it for your machine  
  */
  if(TrueColorFlag==1){
    bbp=ximage->bits_per_pixel; /* is it 16 or 24 bit */
    if(bbp>24)bbp=24;
    bbc=bbp/3;  /*  divide up the 3 colors equally to bbc bits  */
    CMSK=(1<<bbc)-1;  /*  make a mask  2^bbc  -1  */
    CSHIFT=bbc;       /*  how far to shift to get the next color */
    CMULT=8-bbc;       /* multiply 5 bit color to get to 8 bit */
   /* plintf(" bbp=%d CMSK=%d CSHIFT=%d CMULT=%d \n",
      bbp,CMSK,CSHIFT,CMULT); */
  }
  sprintf(head,"P6\n%d %d\n255\n",ximage->width,ximage->height);
  write(fd,head,strlen(head));
  area=ximage->width*ximage->height;
  pixel=(unsigned char*)ximage->data;
  out=(unsigned char *)malloc(3*area);
  dst=out;
  for(y=0;y < (unsigned)(ximage->height); y++) {
    for (x = 0; x < (unsigned)(ximage->width); x++) {
      if(TrueColorFlag==1){
       
	/*  use the slow way to get the pixel 
            but then you dont need to screw around
            with byte order etc  
	*/
	value=XGetPixel(ximage,x,y)>>INIT_C_SHIFT;
	/*vv=value;
	*/
	/*  get the 3 colors   hopefully  */
	lobits=value&CMSK;
	value=value>>CSHIFT;
        if(bbc==5)
	  value=value>>1;
	midbits=value&CMSK;
	value=value>>CSHIFT;
	hibits=value&CMSK;
	/* store them for ppm dumping  */
	*dst++=(MY_RED<<CMULT);
	*dst++=(MY_GREEN<<CMULT);
	*dst++=(MY_BLUE<<CMULT);
      }
      else
	{
	  /* 256 color is easier sort of  */
	    pix = palette[*pixel++];
	    *dst++ = pix.red;
	    *dst++ = pix.green;
	    *dst++ = pix.blue;
	}
    }
  }
  write(fd,out,area*3);
  close(fd);
  free(out);
  free(ximage);
  return 1;
}

void tst_pix_draw()
{
 int i;
 XSetForeground(display,ani_gc,BlackPixel(display,screen));
 XDrawLine(display,ani_pixmap,ani_gc,0,2,vcr.wid,2);
 for(i=1;i<11;i++){
   XSetForeground(display,ani_gc,ColorMap(colorline[i]));
   XDrawLine(display,ani_pixmap,ani_gc,0,2+i,vcr.wid,2+i);
 }
 for(i=0;i<=color_total;i++){
    XSetForeground(display,ani_gc,ColorMap(i+FIRSTCOLOR));
     XDrawLine(display,ani_pixmap,ani_gc,0,14+i,vcr.wid,14+i);
 }
  XSetForeground(display,ani_gc,BlackPixel(display,screen));
  XDrawString(display,ani_pixmap,ani_gc,10,vcr.hgt-(DCURYs+6),"THIS SPACE FOR RENT",20);
  /* plintf(" color_tot=%d \n",color_total); */
}

/* ---- xpp_ui.ani_*: drawing into the animation frame ---- */

void x11_ani_clear(void)
{
 XSetForeground(display,ani_gc,WhitePixel(display,screen));
 XFillRectangle(display,ani_pixmap,ani_gc,0,0,vcr.wid,vcr.hgt);
 XSetForeground(display,ani_gc,BlackPixel(display,screen));
}

void x11_ani_show(void)
{
 XCopyArea(display,ani_pixmap,vcr.view,ani_gc,0,0,vcr.wid,vcr.hgt,0,0);
 XFlush(display);
}

void x11_ani_color(int icol)
{
  if(icol==0)
    XSetForeground(display,ani_gc,BlackPixel(display,screen));
  else
    XSetForeground(display,ani_gc,ColorMap(icol));
}

void x11_ani_thick(int t)
{
  XSetLineAttributes(display,ani_gc,t,LineSolid,CapButt,JoinRound);
}

void x11_ani_font(int size,int font,int color)
{
 if(color==0)
    XSetForeground(display,ani_gc,BlackPixel(display,screen));
 else
   XSetForeground(display,ani_gc,ColorMap(color));
 if(font==0)
   XSetFont(display,ani_gc,romfonts[size]->fid);
 else
   XSetFont(display,ani_gc,symfonts[size]->fid);
}

void x11_ani_line(int x1,int y1,int x2,int y2)
{
  XDrawLine(display,ani_pixmap,ani_gc,x1,y1,x2,y2);
}

void x11_ani_rect(int x,int y,int w,int h,int fill)
{
  if(fill)
    XFillRectangle(display,ani_pixmap,ani_gc,x,y,w,h);
  else
    XDrawRectangle(display,ani_pixmap,ani_gc,x,y,w,h);
}

void x11_ani_arc(int x,int y,int w,int h,int fill)
{
  if(fill)
    XFillArc(display,ani_pixmap,ani_gc,x,y,w,h,0,360*64);
  else
    XDrawArc(display,ani_pixmap,ani_gc,x,y,w,h,0,360*64);
}

void x11_ani_text(int x,int y,char *s)
{
  XDrawString(display,ani_pixmap,ani_gc,x,y,s,strlen(s));
}

/* ---- GIF output of a window or pixmap: grab its pixels, then the core
   encoder in scrngif.c (was scrngif.c) ---- */

void add_ani_gif(Window win,FILE *fp,int count)
{
  plintf("Frame %d \n",count);
  if(count==0)
    gif_stuff(win,fp,FIRST_ANI_GIF);
  else
    gif_stuff(win,fp,NEXT_ANI_GIF);
}

void screen_to_gif(Window win, FILE *fp)
{
 gif_stuff(win,fp,MAKE_ONE_GIF);
}

void get_global_colormap(Window win)
{
  FILE *junk=NULL;
  gif_stuff(win,junk,GET_GLOBAL_CMAP);
}

void gif_stuff(Window win,FILE *fp,int task)
{
 Window root;
 unsigned int h,w,bw,d;
 int x0,y0;
 unsigned char *ppm;

 XGetGeometry(display,win,&root,&x0,&y0,&w,&h,&bw,&d);
 ppm=(unsigned char *)malloc(w*h*3);
 /* plintf(" h=%d w=%d \n",h,w);*/

 getppmbits(win,(int*)&w,(int*)&h,ppm);
 gif_stuff_ppm(ppm,w,h,fp,task);
 free(ppm);
}
