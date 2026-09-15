#include <X11/Xlib.h>
#include "color.h"
#include "colormap.h"
#include "xpp_globals.h"
#include <stdio.h>
#include <stdlib.h> 


#include "ggets.h"
#include <math.h>
#define COLOR_SCALE 0
#define GRAYSCALE 1
#define RGRAYSCALE 2
#define SOLID -1
#define RED	20
#define REDORANGE	21
#define ORANGE	22
#define YELLOWORANGE	23
#define YELLOW    24
#define YELLOWGREEN 25
#define GREEN      26
#define BLUEGREEN  27
#define BLUE   28
#define PURPLE 29

#define C_NORM 0
#define C_PERIODIC 1
#define C_HOT 2
#define C_COOL 3
#define C_REDBLUE 4
#define C_GRAY 5
#define C_CUBHLX 6

extern int Xup;

extern GC gc_graph,small_gc;
extern Display *display;
extern int screen;
extern Window main_win;
extern int DCURX,DCURY,CURY_OFF,CURS_X,CURS_Y,DCURXs,DCURYs;
extern unsigned int Black,White;
extern unsigned int MyBackColor,MyForeColor,GrFore,GrBack;
#define MAX_COLORS 256
#define COL_TOTAL 150
/* int rfun(),gfun(),bfun();
*/

XColor	color[MAX_COLORS];
/* int	pixel[MAX_COLORS];
 */
extern int TrueColorFlag;


void tst_color(w)
Window w;
{
 int i;
 for(i=0;i<color_total;i++){
   set_color(i+color_min);
   XDrawLine(display,w,gc_graph,0,2*i+20,50,2*i+20);
 }
}

void set_scolor(col)
int col;
{
 if(col<0)XSetForeground(display,small_gc,GrBack);
 if(col==0)XSetForeground(display,small_gc,GrFore);
 else{

   if(COLOR)XSetForeground(display,small_gc,ColorMap(col));
   else XSetForeground(display,small_gc,GrFore);
}


}

void x11_set_color(col)
int col;
{
 if(col<0)XSetForeground(display,gc_graph,GrBack);
 if(col==0)XSetForeground(display,gc_graph,GrFore);
 else{

   if(COLOR)XSetForeground(display,gc_graph,ColorMap(col));
   else XSetForeground(display,gc_graph,GrFore);
}

}

/* this makes alot of nice color maps */
/* this loads a color_map file and counts the 
   entries. It then does a simple interpolation to fill  
   n copies of rr,gg,bb
*/
  
void x11_NewColormap(int type)
{
  /*  printf(" My color map = %d\n",type); */
   if(TrueColorFlag==0){
   err_msg("New colormaps not supported without TrueColor");
   return;
   } 
 custom_color=type;
 MakeColormap();
}

int print_cust()
{
  printf("custom map =%d \n",custom_color);
  return 1;
}

int ColorMap(i)
int i;
{   if(i==-1)return(GrBack);
    if(i==0)return(GrFore);
    if(color_mode){
      if(i<0)i=0;
      if(i>=color_max)i=color_max;
	return(color[i].pixel);
    } else {
	return(i);
    }
}

void MakeColormap()
{
  Colormap cmap;
  int i;
  int clo=20;
  cmap=(Colormap)NULL;
  xpp_build_colormap();
  if (Xup){cmap = DefaultColormap(display,screen);}
  for (i = 0; i < clo; i++) {
    color[i].pixel = i;
  }
  for(i=20;i<=color_max;i++){
    if(i>=30 && i<color_min) continue;
    color[i].red=xpp_cmap_rgb[i][0];
    color[i].green=xpp_cmap_rgb[i][1];
    color[i].blue=xpp_cmap_rgb[i][2];
    color[i].flags = DoRed | DoGreen | DoBlue;
    if (Xup){XAllocColor(display,cmap,&color[i]);}
  }
}
