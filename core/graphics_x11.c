/* X11 drawing primitives, split out of graphics.c. graphics.c keeps the
   device-independent layer (scaling, 3D, clipping, PS/SVG dispatch) and
   reaches these through the XppUi table (ui_x11.c wires them in). */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "graphics.h"
#include "color.h"
#include "xpp_globals.h"
#include "axes2.h"
#include "ggets.h"
#include "graf_par.h"
#include "integrate.h"
#include "many_pops.h"
#include "nullcline.h"
#include "browse.h"

extern BROWSER my_browser;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Display *display;
extern Window draw_win;
extern GC gc_graph, small_gc, font_gc;
extern XFontStruct *small_font;
extern unsigned int GrFore, GrBack;
extern int DCURXs, DCURYs;
extern int colorline[];
extern int PointRadius, TextJustify;

char dashes[10][5] = { {0}, {1,6,0},
   {0}, {4,2,0}, {1,3,0}, {4,4,0}, {1,5,0}, {4,4,4,1,0}, {4,2,0}, {1,3,0}
   };

XFontStruct *symfonts[5], *romfonts[5];
int avsymfonts[5], avromfonts[5];

/* size of the active drawing window, used by graphics.c get_draw_area() */
void x11_get_draw_size(unsigned int *w, unsigned int *h)
{
    int x, y;
    unsigned int bw, de;
    Window root;
    XGetGeometry(display, draw_win, &root, &x, &y, w, h, &bw, &de);
}

/* ---- moved function bodies follow (appended by tools/move_funcs.py) ---- */

void point_x11(xp,yp)
     int xp,yp;
{
  int r=PointRadius;
  int  r2 = (int) (r / 1.41421356 + 0.5);
  int wh = 2 * r2;
  if(PointRadius==0)
    XDrawPoint(display,draw_win,gc_graph,xp,yp);
  else
    XFillArc(display,draw_win,gc_graph,xp-r2,yp-r2,wh, wh, 0, 360*64);
    
}

void set_line_style_x11(ls)
     int ls;
{
  /*int width=0;*/
  int type=0;
  if(ls==-2){  /*  Border  */
    set_color(0);
    XSetLineAttributes(display,gc_graph,2,LineSolid,CapButt,JoinBevel);
    return;
  }
 /*width=0;
 */
 if(ls==-1){
   set_color(0);
   XSetDashes(display,gc_graph,0,dashes[1],strlen(dashes[1]));
   XSetLineAttributes(display,gc_graph,0,LineOnOffDash,CapButt,JoinBevel);
   return;
 }
 if(!COLOR){  /* Mono  */
   ls=(ls%8)+2;
   if(ls==2)
     type=LineSolid;
   else{
     type=LineOnOffDash;
     XSetDashes(display,gc_graph,0,dashes[ls],strlen(dashes[ls]));
   }
   set_color(0);
   XSetLineAttributes(display,gc_graph,0,type,CapButt,JoinBevel);
   return;
 }
 /* color system  */
  ls=ls%11;
  XSetLineAttributes(display,gc_graph,0,LineSolid,CapButt,JoinBevel);
  set_color(colorline[ls]);
}

void bead_x11(x,y)
     int x,y;
{
 XFillArc(display,draw_win,gc_graph,x-2,y-2,4,4,0,360*64);
}

void rect_x11(x,y,w,h)
     int x,y,w,h;
{
 XFillRectangle(display,draw_win,gc_graph,x,y,w,h);
}

void line_x11(xp1,yp1,xp2,yp2)
     int xp1,yp1,xp2,yp2;
{
  XDrawLine(display,draw_win,gc_graph,xp1,yp1,xp2,yp2);  
}

void put_text_x11(x,y,str)
     int x,y;
     char *str;
{
  int sw=strlen(str)*DCURXs;
  switch(TextJustify){
  case 0: sw=0; break;
  case 1: sw=-sw/2; break;
  case 2: sw=-sw; break;
  }
  XSetForeground(display,small_gc,GrFore);
  XDrawString(display,draw_win,small_gc,x+sw,y+DCURYs/3,str,strlen(str));
  XSetForeground(display,small_gc,GrBack);
}

void special_put_text_x11(x,y,str,size)
     int x,y,size;
     char *str;
{
  int i=0,j=0;
  int cx=x,cy=y;
  int cf=0,cs;
  int n=strlen(str),dx=0;
  char tmp[256],c;
  int sub,sup;
  cs=size;
  if(avromfonts[size]==1){
    sup=romfonts[size]->ascent;
    sub=sup/2;


  }
  else {
    sup=small_font->ascent;
    sub=sup/2;
  }
  while(i<n){
    c=str[i];
    if(c=='\\'){      
      i++;
      c=str[i];
      tmp[j]=0; /* end the current buffer */
      
      fancy_put_text_x11(cx,cy,tmp,cs,cf); /* render the current buffer */
      if(cf==0){
	if(avromfonts[cs]==1)
	  dx=XTextWidth(romfonts[cs],tmp,strlen(tmp));
	else
	  dx=XTextWidth(small_font,tmp,strlen(tmp));
      }
      if(cf==1){
	if(avsymfonts[cs]==1)
	  dx=XTextWidth(symfonts[cs],tmp,strlen(tmp));
	else
	  dx=XTextWidth(small_font,tmp,strlen(tmp));
      }
      cx+=dx;
      j=0;
      if(c=='0'){

	cf=0;
      }
      if(c=='n'){

	cy=y;
	cs=size;
      }
      if(c=='s'){

	cy=cy+sub;
	if(size>0)
	  cs=size-1;
      }
      if(c=='S'){

	if(size>0)
	  cs=size-1;
	cy=cy-sup;
      }
      if(c=='1'){

	cf=1;
      }
    
      i++;
    }
    else {
      tmp[j]=c;
      j++;
      i++;
    }
  }
  tmp[j]=0;
      fancy_put_text_x11(cx,cy,tmp,cs,cf);
}

void fancy_put_text_x11(x,y,str,size,font)
     int x,y,size,font;
     char *str;
{
  /*int yoff;
  */
  if(strlen(str)==0)return;
  switch(font){
  
  case 1: 
    if(avsymfonts[size]==1){
      XSetFont(display,font_gc,symfonts[size]->fid);
      /*yoff=symfonts[size]->ascent;*/
    }
    else {
      XSetFont(display,font_gc,small_font->fid);
      /*yoff=small_font->ascent;*/
      
    }
    XSetForeground(display,font_gc,GrFore);
    XDrawString(display,draw_win,font_gc,x,y,str,strlen(str));
   XSetForeground(display,font_gc,GrBack);  
    break;
  default: 
    if(avromfonts[size]==1){
      XSetFont(display,font_gc,romfonts[size]->fid);
      /*yoff=romfonts[size]->ascent;*/
    
    }
    else {
      XSetFont(display,font_gc,small_font->fid);
      /*yoff=small_font->ascent;*/
    }
    XSetForeground(display,font_gc,GrFore);
    XDrawString(display,draw_win,font_gc,x,y,str,strlen(str));
    XSetForeground(display,font_gc,GrBack);  
    break;
  }
}

void x11_redraw_the_graph()
{
 blank_screen(draw_win);
 set_normal_scale();
 do_axes();
 hi_lite(draw_win);
 restore(0,my_browser.maxrow);
 draw_label(draw_win);
 draw_freeze(draw_win);
 redraw_dfield();
 if(MyGraph->Nullrestore)restore_nullclines();
}
