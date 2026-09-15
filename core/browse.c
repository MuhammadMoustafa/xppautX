#include "xpp_ui.h"
#include <X11/Xlib.h>
#include "browse.h"
#include <string.h>
#include <strings.h>
#include "parserslow.h"
#include <sys/time.h>
#include "ggets.h"
#include "dialog_box.h"
#include "eig_list.h"
#include "integrate.h"
#include "menudrive.h"
#include "init_conds.h"
#include "many_pops.h"
#include "pop_list.h"
#include <stdlib.h> 
#include <stdio.h>

#include <sys/time.h>
#include <unistd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <math.h>
#ifndef WCTYPE
#include <ctype.h>
#else
#include <wctype.h>
#endif

#include "xpplim.h"
#include "browse.bitmap"
#include "newhome.h"
#include "mykeydef.h"

extern char *browse_hint[];
#define xds(a) { XDrawString(display,w,small_gc,5,CURY_OFFs,a,strlen(a));\
		return;}


#define BMAXCOL 20




extern int *my_ode[];

extern int *plotlist,N_plist;

extern char *ode_names[MAXODE];
double evaluate();
double atof();
extern int NEQ,MAXSTOR,NMarkov,FIX_VAR;
extern int NEQ_MIN;
extern int NODE,NJMP;
extern int Xup,TipsFlag;
extern double last_ic[MAXODE],DELTA_T;

extern int NSYM,NSYM_START,NCON,NCON_START;

extern Display *display;
extern int screen,storind;
extern GC gc, small_gc;
extern int DCURX,DCURXs,DCURY,DCURYs,CURY_OFFs,CURY_OFF;
extern unsigned int MyBackColor,MyForeColor,MyMainWinColor,MyDrawWinColor,GrFore,GrBack;


extern Window command_pop;

#define MYMASK  (ButtonPressMask 	|\
                ButtonReleaseMask |\
		KeyPressMask		|\
		ExposureMask		|\
		StructureNotifyMask	|\
		LeaveWindowMask		|\
		EnterWindowMask)

#define SIMPMASK (ButtonPressMask |\
		                  ButtonReleaseMask |\
		  KeyPressMask	  |\
		  ExposureMask    |\
                  StructureNotifyMask)



/*  The one and only primitive data browser   */

/*typedef struct {
		Window base,upper;
		Window find,up,down,pgup,pgdn,home,end,left,right;
		Window first,last,restore,write,get,close;
		Window load,repl,unrepl,table,addcol,delcol;
                Window main;
                Window label[BMAXCOL];
                Window time;
		Window hint;
		char hinttxt[256];
		int dataflag,xflag;
		int col0,row0,ncol,nrow;
		int maxrow,maxcol;
                float **data;
		int istart,iend;
                } BROWSER;
*/
extern BROWSER my_browser;
static void with_focus(void (*f)(BROWSER *), BROWSER *b);
extern float *old_rep;
extern int REPLACE,R_COL;




extern int noicon;
extern char uvar_names[MAXODE][12];

extern float **storage;


/*Excerpt from the man (Section 2) for  gettimeofday:
"The use of the timezone structure is obsolete; the tz argument should normally be spec-
ified as NULL.  The tz_dsttime field has never been used under Linux; it has  not  been
and will not be supported by libc or glibc.  Each and every occurrence of this field in
the kernel source (other than the declaration) is a bug."
*/

















 

 
 

void browse_but_on(b,i,w,yn)
     int i;
     Window w;
     int yn;
     BROWSER *b;
{
  int val=1;
  if(yn)val=2;
  XSetWindowBorderWidth(display,w,val);
  if(yn&&TipsFlag&&i>=0){
    strcpy(b->hinttxt,browse_hint[i]);
    display_browser(b->hint,*b);
  }

    

}

void enter_browser(ev,b,yn)
     XEvent ev;
     BROWSER *b;
     int yn;
{
  Window w=ev.xexpose.window;
  if(w==b->find)browse_but_on(b,0,w,yn);
 if(w==b->up)browse_but_on(b,1,w,yn);
 if(w==b->down)browse_but_on(b,2,w,yn);
 if(w==b->pgup)browse_but_on(b,3,w,yn);
 if(w==b->pgdn)browse_but_on(b,4,w,yn);
 if(w==b->left)browse_but_on(b,5,w,yn);
 if(w==b->right)browse_but_on(b,6,w,yn);
 if(w==b->home)browse_but_on(b,7,w,yn);
 if(w==b->end)browse_but_on(b,8,w,yn);
 if(w==b->first)browse_but_on(b,9,w,yn);
 if(w==b->last)browse_but_on(b,10,w,yn);
 if(w==b->restore)browse_but_on(b,11,w,yn);
 if(w==b->write)browse_but_on(b,12,w,yn);
 if(w==b->get)browse_but_on(b,13,w,yn);
 if(w==b->repl)browse_but_on(b,14,w,yn);
 if(w==b->unrepl)browse_but_on(b,15,w,yn);
 if(w==b->table)browse_but_on(b,16,w,yn);
 if(w==b->load)browse_but_on(b,17,w,yn);
 if(w==b->time)browse_but_on(b,18,w,yn);
 if(w==b->addcol)browse_but_on(b,19,w,yn);
 if(w==b->delcol)browse_but_on(b,20,w,yn);
  if(w==b->close)browse_but_on(b,-1,w,yn);

}


void display_browser(w,b)
Window w;
BROWSER b;
{
  int i,i0;
 if(w==b.hint){
   XClearWindow(display,b.hint);
   XDrawString(display,w,small_gc,8,CURY_OFFs,b.hinttxt,strlen(b.hinttxt));
   return;
 }
 
 if(w==b.find)xds("Find")
 if(w==b.up)xds("Up")
 if(w==b.down)xds("Down")
 if(w==b.pgup)xds("PgUp")
 if(w==b.pgdn)xds("PgDn")
 if(w==b.left)xds("Left")
 if(w==b.right)xds("Right")
 if(w==b.home)xds("Home")
 if(w==b.end)xds("End")
 if(w==b.first)xds("First")
 if(w==b.last)xds("Last")
 if(w==b.restore)xds("Restore")
 if(w==b.write)xds("Write")
 if(w==b.get)xds("Get")
 if(w==b.repl)xds("Replace");
 if(w==b.unrepl)xds("Unrepl");
 if(w==b.table)xds("Table");
 if(w==b.load)xds("Load");
 if(w==b.time)xds("Time")
 if(w==b.addcol)xds("Add col")
  if(w==b.close)xds("Close")
 if(w==b.delcol)xds("Del col")
 for(i=0;i<BMAXCOL;i++){
 
    if(w==b.label[i]){
    i0=i+b.col0-1;
     if(i0<b.maxcol-1)	XDrawString(display,w,small_gc,5,CURY_OFFs,
	uvar_names[i0],strlen(uvar_names[i0]));

    }

 }
 if(w==b.main)draw_data(b);
}


void  redraw_browser(b)
 BROWSER b;
 {
  int i,i0;
  Window w;
  draw_data(b);
  for(i=0;i<BMAXCOL;i++){
    w=b.label[i];
    i0=i+b.col0-1;
 if(i0<(b.maxcol-1)){
       XClearWindow(display,w);
	XDrawString(display,w,small_gc,5,CURY_OFFs,uvar_names[i0],
	strlen(uvar_names[i0]));
    }

  }
}


void  new_browse_dat(new_dat,dat_len)
 int dat_len;
 float **new_dat;
{
 my_browser.data=new_dat;
 refresh_browser(dat_len);
}



void draw_data(b)
 BROWSER b;
{
  int i,i0,j,j0;
  int x0;
  char string[50];
  int dcol=DCURXs*14;
  int drow=(DCURYs+6);
  if(b.dataflag==0)return;  /*   no data  */
  XClearWindow(display,b.main);
  
  /* Do time data first  */

  for(i=0;i<b.nrow;i++){
	i0=i+b.row0;
        if(i0<b.maxrow){
		sprintf(string,"%.8g",b.data[0][i0]);
		XDrawString(display,b.main,small_gc,DCURXs/2+5,i*drow+DCURYs
,
		string,strlen(string));
	 }
 }

/* Do data stuff   */
 for(j=0;j<b.ncol;j++)
 {
 x0=(j+1)*dcol+DCURXs/2;
 j0=j+b.col0;
 if(j0>=b.maxcol)return;  /* if this one is too big, they all are  */
 for(i=0;i<b.nrow;i++){
   i0=i+b.row0;
   if(i0<b.maxrow){
                   sprintf(string,"%.7g",b.data[j0][i0]);
		XDrawString(display,b.main,small_gc,x0+5,i*drow+DCURYs,
		string,strlen(string));
                }
  }

  
}
}


void kill_browser(BROWSER *b)
{
  b->xflag=0;
  waitasec(ClickTime);
  XDestroySubwindows(display,b->base);
   XDestroyWindow(display,b->base);
}

void make_new_browser()
{
  if(my_browser.xflag==1){
    XRaiseWindow(display,my_browser.base);
    return;
  }
  make_browser(&my_browser,"Data Viewer","Data",20,5);
  my_browser.xflag=1;
}
Window br_button(root,row,col,name,iflag)
int row,col,iflag;
 Window root;
 char *name;
{
  Window win;
  int dcol=12*DCURXs;
  int drow=(DCURYs+6);
  /*int width=strlen(name)*DCURXs;
  */
  int width=8*DCURXs;
  int x;
  int y;
  if(iflag==1)dcol=14*DCURXs;
   x=dcol*col+4;
   y=drow*row+4;
   win=make_window(root,x,y,width+5,DCURYs+1,1);
  XSelectInput(display,win,MYMASK);
 return(win);
 }
Window br_button_data(root,row,col,name,iflag)
int row,col,iflag;
 Window root;
 char *name;
{
  Window win;
  int dcol=12*DCURXs;
  int drow=(DCURYs+6);
  int width=strlen(name)*DCURXs;
  
  int x;
  int y;
  if(iflag==1)dcol=14*DCURXs;
   x=dcol*col+4;
   y=drow*row+4;
   win=make_window(root,x,y,width+5,DCURYs+1,1);
  XSelectInput(display,win,MYMASK);
 return(win);
 }
    


void make_browser(b,wname,iname,row,col)
BROWSER *b;
int row,col;
char *wname,*iname;
{
 int i;
 int ncol=col;
 int width,height;
 Window base;
/* XWMHints wm_hints;
*/
  XTextProperty winname,iconname;
  XSizeHints size_hints;
 int dcol=DCURXs*17;
 int drow=(DCURYs+6);
 int ystart=8;
 
 if(ncol<5)ncol=5;

 height=drow*(row+6);
 width=ncol*dcol;
 b->nrow=row;
 b->ncol=ncol;
 base=make_plain_window(RootWindow(display,screen),0,0,width,height,4);
 b->base=base;
XSelectInput(display,base,ExposureMask|KeyPressMask|ButtonPressMask|
		StructureNotifyMask);
/* plintf("Browser base: %d \n",base); */
  XStringListToTextProperty(&wname,1,&winname);
XStringListToTextProperty(&iname,1,&iconname);
  
 size_hints.flags=PPosition|PSize|PMinSize;
 size_hints.x=0;
 size_hints.y=0;
/* size_hints.width=width;
 size_hints.height=height; */
 size_hints.min_width=width-15;
 size_hints.min_height=height;
 /* wm_hints.initial_state=IconicState;
 wm_hints.flags=StateHint; 
 */
 XClassHint class_hints;
 class_hints.res_name="";
 class_hints.res_class="";
 
  XSetWMProperties(display,base,&winname,&iconname,NULL,0,&size_hints,NULL,&class_hints);
 make_icon((char*)browse_bits,browse_width,browse_height,base);
 b->upper=make_window(base,0,0,width,ystart+drow*6,1);
 XSetWindowBackground(display,b->upper,MyMainWinColor);
 b->main=make_plain_window(base,0,ystart+drow*6,width,row*drow,1);
 XSetWindowBackground(display,b->main,MyDrawWinColor);
 b->find=br_button(base,0,0,"find",0);
 b->get=br_button(base,1,0,"get ",0);
 b->repl=br_button(base,2,0,"replace",0);
 b->restore=br_button(base,0,1,"restore",0);
 b->write=br_button(base,1,1," write ",0);
 b->load=br_button(base,2,1," load  ",0);
 b->first=br_button(base,0,2,"first",0);
 b->last=br_button(base,1,2,"last ",0);
 b->unrepl=br_button(base,2,2,"unrepl",0);
 b->table=br_button(base,2,3,"table",0);
 b->up=br_button(base,0,3," up ",0);
 b->down=br_button(base,1,3,"down",0);
 b->pgup=br_button(base,0,4,"pgup",0);
 b->pgdn=br_button(base,1,4,"pgdn",0);
 b->left=br_button(base,0,5,"left ",0);
 b->right=br_button(base,1,5,"right",0);
 b->home=br_button(base,0,6,"home",0);
 b->end=br_button(base,1,6,"end ",0);
 b->addcol=br_button(base,2,4,"addcol",0);
 b->delcol=br_button(base,2,5,"delcol",0);
 b->close=br_button(base,2,6,"close",0);
 b->time=br_button(base,5,0,"time ",1);
 b->hint=make_window(base,0,4*drow,width-17,drow-3,1);
  XSelectInput(display,b->time,SIMPMASK);

 for(i=0;i<BMAXCOL;i++){
      /*  plintf("%d ",i); */
 	b->label[i]=br_button_data(base,5,i+1,"1234567890",1);
	 XSelectInput(display,b->label[i],SIMPMASK);
       /* plintf(" %d \n",i); */
  }
  if(noicon==0)XIconifyWindow(display,base,screen); 
/*  XMapWindow(display,base);  */

}

/*   These are the global exporters ...   */

void expose_my_browser(ev)
XEvent ev;
{
  if(my_browser.xflag==0)return;
 expose_browser(ev,my_browser);
}


void enter_my_browser(ev,yn)
XEvent ev;
int yn;
{
  if(my_browser.xflag==0)return;
 enter_browser(ev,&my_browser,yn);
}


void  my_browse_button(ev)
XEvent ev;
 {
  if(my_browser.xflag==0)return;
  browse_button(ev,&my_browser);
 }


void my_browse_keypress(ev,used)
int *used;
XEvent ev;
{
   if(my_browser.xflag==0)return;
 browse_keypress(ev,used,&my_browser);
 }


void resize_my_browser(win)
Window win;
{
  if(my_browser.xflag==0)return;
 resize_browser(win,&my_browser);
}






void expose_browser(ev,b)
XEvent ev;
BROWSER b;
{
   if(my_browser.xflag==0)return;
   if(ev.type!=Expose)return; 
 display_browser(ev.xexpose.window,b);
}


void resize_browser(win,b)
Window win;
BROWSER *b;
{
  unsigned int w,h,hreal;
  int dcol=17*DCURXs,drow=DCURYs+6;
  int i0;
  int newrow,newcol;
   if(my_browser.xflag==0)return;
  if(win!=b->base)return;
  /* w=ev.xconfigure.width;
  h=ev.xconfigure.height; */
  get_new_size(win,&w,&h);
  hreal=h;

  /* first make sure the size is is ok  and an integral 
     value of the proper width and height
   */  
 i0=w/dcol;
 if((w%dcol)>0)i0++;
 if(i0>b->maxcol)i0=b->maxcol;

 w=i0*dcol;
 if(i0<5)w=5*dcol;
 newcol=i0;
 h=hreal-8-5*drow;
 i0=h/drow;
 if((h%drow)>0)i0++;
 if(i0>b->maxrow)i0=b->maxrow;
 h=i0*drow+DCURXs/2;
 newrow=i0;
 /*  Now resize everything   */
 if(b->ncol==newcol&&b->nrow==newrow)
   return;
b->ncol=newcol;
b->nrow=newrow;

 XResizeWindow(display,b->base,w-17,hreal);
 XResizeWindow(display,b->upper,w-17,8+drow*3);
 XResizeWindow(display,b->main,w-17,h);

/* Let the browser know how many rows and columns of data  */
   
  
}

/*  if button is pressed in the browser 
    then do the following  */


void browse_button(ev,b)
BROWSER *b;
XEvent ev;
{
 XEvent zz;
 int done=1;
 Window w=ev.xbutton.window;
   if(my_browser.xflag==0)return;
 if(w==b->up||w==b->down||w==b->pgup||w==b->pgdn||w==b->left||w==b->right)
  {
    done=1;
    while(done){
      if(w==b->up)data_up(b);
      if(w==b->down)data_down(b);
      if(w==b->pgup)data_pgup(b);
      if(w==b->pgdn)data_pgdn(b);
      if(w==b->left)data_left(b);
      if(w==b->right)data_right(b);
      waitasec(100);
      if(XPending(display)>0)
	  {
	   
          XNextEvent(display,&zz);
          switch(zz.type){
	  case ButtonRelease:
	    done=0;
	    break;
	  }
	}
    }
    return;
  }

 if(w==b->home){data_home(b); return;}

 if(w==b->end){data_end(b); return;}

 if(w==b->first){data_first(b); return;}

if(w==b->last){data_last(b); return;}

 if(w==b->restore){data_restore(b); return;}

 if(w==b->write){data_write(b); return;}

 if(w==b->get){data_get(b); return;}
 
 if(w==b->find){with_focus(data_find,b); return;}

 if(w==b->repl){with_focus(data_replace,b);return;}
 
 if(w==b->load){data_read(b);return; }

 if(w==b->addcol){data_add_col(b);return;}

 if(w==b->delcol){data_del_col(b);return;}

 if(w==b->unrepl){data_unreplace(b);return;}

 if(w==b->table){with_focus(data_table,b);return;}
 
 if(w==b->close){kill_browser(b);return;}

}




void browse_keypress(ev,used,b)
BROWSER *b;
XEvent ev;
int *used;
{
 Window w=ev.xkey.window;


 char ks;
 Window w2;
 int rev;

*used=0;
  if(my_browser.xflag==0)return;
 XGetInputFocus(display,&w2,&rev);

 if(w==b->main||w==b->base||w==b->upper||w2==b->base)
 {
  *used=1;
  

 ks=(char)get_key_press(&ev);


 /* 
  XLookupString(&ev,buf,maxlen,&ks,&comp);
 
 */
 
   if(ks==UP){data_up(b); return;}

 if(ks==DOWN){data_down(b); return;}

 if(ks==PGUP){data_pgup(b); return;}

 if(ks==PGDN){data_pgdn(b); return;}

 if(ks==LEFT){data_left(b); return;}

 if(ks==RIGHT){data_right(b); return;}

 if(ks==HOME){data_home(b); return;}

 if(ks==END){data_end(b); return;}

 if(ks=='s'||ks=='S'){data_first(b); return;}

if(ks=='e'||ks=='E'){data_last(b); return;}

 if(ks=='r'||ks=='R'){data_restore(b); return;}

 if(ks=='W'||ks=='w'){data_write(b); return;}

 if(ks=='g'||ks=='G'){data_get(b); return;}
 
 if(ks=='f'||ks=='F'){with_focus(data_find,b); return;}

 if(ks=='l'||ks=='L'){data_read(b);return;}

 if(ks=='u'||ks=='U'){data_unreplace(b);return;}

 if(ks=='t'||ks=='T'){with_focus(data_table,b);return;}

 if(ks=='p'||ks=='P'){with_focus(data_replace,b);return;}
 
  if(ks=='a'||ks=='A'){data_add_col(b);return;}

 if(ks=='d'||ks=='D'){data_del_col(b);return;}


 

 if(ks==ESC){
			XSetInputFocus(display,command_pop,
			RevertToParent,CurrentTime);
		   	return;
                   }
   

		   } /* end of cases */

}



/* the commands in browse_data.c prompt with dialogs; put the keyboard focus
   back where it was afterwards */
static void with_focus(void (*f)(BROWSER *), BROWSER *b)
{
  Window w;
  int rev;
  XGetInputFocus(display,&w,&rev);
  f(b);
  XSetInputFocus(display,w,rev,CurrentTime);
}

void data_up(b)
BROWSER *b;
{
 if(b->row0>0){
	b->row0--;
	draw_data(*b);
        }
}


void data_down(b)
BROWSER *b;
{
 if(b->row0<(b->maxrow-1)){
	b->row0++;
	draw_data(*b);
        }
}


void data_pgup(b)
BROWSER *b;
{
 int i=b->row0-b->nrow;
 if(i>0)
   b->row0=i;
 else
   b->row0=0;
 draw_data(*b);
}
 

void data_pgdn(b)
BROWSER *b;
{
 int i=b->row0+b->nrow;
 if(i<(b->maxrow-1))
   b->row0=i;
 else
   b->row0=b->maxrow-1;
 draw_data(*b);
}


void  data_home(b)
BROWSER *b;
{
 b->row0=0;
 b->istart=0;
 b->iend=b->maxrow;
 draw_data(*b);
}
 

void  data_end(b)
BROWSER *b;
{
 b->row0=b->maxrow-1;
 draw_data(*b);
}



  



 





 


 












 


 




void data_left(b)
BROWSER *b;
{
 int i=b->col0;
 if(i>1){
 b->col0--;
 redraw_browser(*b);
}
}


void data_right(b)
BROWSER *b;
{
 int i=b->col0+b->ncol;
 if(i<=b->maxcol){
 b->col0++;
 redraw_browser(*b);
 }
}
 







   
  

    

 

