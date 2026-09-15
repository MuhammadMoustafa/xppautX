#include <X11/Xlib.h>
#include <stdlib.h>  

#include <sys/wait.h>
#include <unistd.h>
/* the menu for XPP commands 
   this calls any command
   it also has lots of the direct X Gui stuff
   
*/

#include "browse.h"
#include "calc.h"
#include "init_conds.h"
#include "kinescope.h"
#include "main.h"
#include "alert.bitmap"

#include "graf_par.h"
#include "integrate.h"
#include "lunch-new.h"
#include "edit_rhs.h"
#include "many_pops.h"
#include "torus.h"
#include "nullcline.h"
#include "numerics.h"
#include "markov.h"
#include "extra.h"
#include "pop_list.h"
#include "tabular.h"
#include "pp_shoot.h"
#include "adj2.h"
#include "txtread.h"
#include "auto.h"
#include "ggets.h"


#include <stdio.h>
#include <string.h>
#include "menudrive.h"
#include "menus.h"
#include "load_eqn.h"
extern int manual_expose;
extern char this_file[XPP_MAX_NAME];
extern char *info_message,*ic_hint[],*sing_hint[],
*null_hint[],*flow_hint[],*null_freeze[], *bvp_hint[],*color_hint[],
  *stoch_hint[];
extern char *no_hint[],*wind_hint[],*view_hint[],*frz_hint[];
extern char *graf_hint[], *cmap_hint[],*half_hint[],*map_hint[]; 
extern char *text_hint[];
extern char *edit_hint[];
extern char *adj_hint[];
extern char *phas_hint[],*kin_hint[],*view_hint[],*tab_hint[];
extern Window info_pop,main_win,draw_win;
extern int POIMAP;
extern int DCURY,DCURX;
extern int DF_FLAG,tfBell,TipsFlag;
extern int SimulPlotFlag,current_pop,num_pops,ActiveWinList[];
extern int DisplayHeight,DisplayWidth;
extern int AutoFreezeFlag,NTable;
extern Display *display;
extern int screen;

extern int TORUS;
typedef struct {
  Window w;
  char text[256];
  int here;} MSGBOXSTRUCT;

MSGBOXSTRUCT MsgBox;



void x11_MessageBox(char *m)
{
 int wid=strlen(m)*DCURX+20;
 int hgt=4*DCURY;
 MsgBox.w=make_plain_window(RootWindow(display,screen),
		      DisplayWidth/2,DisplayHeight/2, wid,hgt,4);
		      
 make_icon((char*)alert_bits,alert_width,alert_height,MsgBox.w);
 MsgBox.here=1;
 set_window_title(MsgBox.w,"Yo!");
 strcpy(MsgBox.text,m);
 ping(); 

}
void RedrawMessageBox(Window w)
{
  if(w==MsgBox.w){
    /*    plintf("%s \n",MsgBox.text); */
 Ftext(10,2*DCURY,MsgBox.text,MsgBox.w);
  }

}
void x11_KillMessageBox()
{
  if(MsgBox.here==0)return;
  MsgBox.here=0;
  waitasec(ClickTime);
  XDestroyWindow(display,MsgBox.w);
}
int x11_TwoChoice(char *c1,char *c2, char *q,char *key,char *title)
{
 return two_choice(c1,c2,q,key,DisplayWidth/2,DisplayHeight/2,
		   RootWindow(display,screen),title); 
}

int x11_menu_choose(const XppMenu *m,int def)
{
 Window temp=main_win;
 return pop_up_list(&temp,m->title,m->items,m->keys,m->n,m->width,def,
		    10,m->row<0?0:m->row*DCURY+8,m->hints,info_pop,info_message);
}
int x11_GetMouseXY(int *x,int *y)
{
 return get_mouse_xy(x,y,draw_win);
}

void x11_FlushDisplay()
{
 XFlush(display);
}
void x11_clear_draw_window()
{
  clr_scrn();
  hi_lite(draw_win);
}

void x11_drw_all_scrns(){
  int i;
  int me=manual_expose;
 int ic=current_pop;
 manual_expose=0;
 if(SimulPlotFlag==0){
    redraw_all();
    manual_expose=me;
 return;
 }
 
 for(i=0;i<num_pops;i++){
   make_active(ActiveWinList[i],1);
   redraw_all();
 }
 
 make_active(ic,1);
 hi_lite(draw_win);
     manual_expose=me;
}
 
void x11_clr_all_scrns()
{
 int i;
 int ic=current_pop;
 if(SimulPlotFlag==0){
 clr_scrn();
 hi_lite(draw_win);
 return;
 }
 
 for(i=0;i<num_pops;i++){
   make_active(ActiveWinList[i],1);
   clr_scrn();
 }
 
 make_active(ic,1);
 hi_lite(draw_win);
}

















