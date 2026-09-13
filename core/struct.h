#ifndef _struct_h_
#define _struct_h_

#include "xpplim.h"
#include "xpp_types.h"
#define MAXCHAR 60
#define MAXENTRY 20
#define RADIO 0
#define CHOICE 1
#define ICMAX 25

#define MAXPERPLOT 10
#define MAXFRZ 26
#define MAXPOP 21

#define MAXNCLINE 26

#define ICLENGTH 30
#define NAMELENGTH 10

typedef struct {
		double xlo,xhi;
		char rv[10];
  		int nstep, ic,stor;
		} RANGE_INFO; 
		
typedef struct {
		XppWinId base,ok,cancel,old,last,more,range;
		XppWinId wrlo,wrhi,wstep,wreset,woldic;
		RANGE_INFO *rinf;
		double *yold,*y,*ylast;
    		int n;
                int node;
		char **name;
		char ascval[MAXODE][ICLENGTH];
		XppWinId wname[ICMAX],wval[ICMAX];
		} IC_BOX;
			       
		
		
	


typedef struct {
	       XppWinId w,w_info;

	       int Use;
                int state;
	       	int Restore;
		int Nullrestore;
		int x0;
		int y0;
		int Width;
		int Height;
                int x11Wid;
  int x11Hgt;
		int nvars;
		double rm[3][3];
		double min_scale,color_scale;
		double xmin,ymin,zmin,xmax,ymax,zmax,xorg,yorg,zorg;
		double xbar,ybar,zbar,dx,dy,dz;
		int xv[MAXPERPLOT],yv[MAXPERPLOT],zv[MAXPERPLOT];
		int line[MAXPERPLOT],color[MAXPERPLOT];
		double Theta,Phi;
		double ZPlane,ZView;
		double xlo,ylo,xhi,yhi,oldxlo,oldxhi,oldylo,oldyhi;
		int grtype,ThreeDFlag,TimeFlag,PerspFlag;
		int xshft,yshft,zshft;
	        int xorgflag,yorgflag,zorgflag;
		int ColorFlag,ColorValue;
	        char xlabel[30],ylabel[30],zlabel[30];
                char gr_info[256];
		} GRAPH;

#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
typedef struct {
		GC gc;
		int dx,dy,yoff;
 		unsigned int fcol,bcol;
		} TEXTGC;

#endif /* Xlib.h */
typedef struct {
		XppWinId w;
		float x;
		float y;
		char s[MAXCHAR];
		short use;
		int font,size;
		} LABEL;


typedef struct {
                XppWinId w;
		char key[20],name[10];
		short use,type;
		float *xv,*yv,*zv;
		int len,color;
	      } CURVE;


typedef struct {
                XppWinId w;
	        char name[10];
                short use;
		float *x_n,*y_n;
		int ix,iy,num_x,num_y;
	      } NCLINE;
		
typedef struct {
 		XppWinId mes;
		XppWinId ok;
		XppWinId cancel;
 		XppWinId input;
		XppWinId base;
		char mes_s[MAXCHAR];
		char input_s[MAXCHAR];
		char ok_s[MAXCHAR];
		char cancel_s[MAXCHAR];
		} DIALOG;


typedef struct {
		char title[MAXCHAR];
                int n;
		XppWinId base;
		XppWinId ok;
		XppWinId cancel;
		short type;
                int mc;
		 XppWinId cw[MAXENTRY];
                 char **name;
                 int *flag;
		} CHOICE_BOX;

typedef struct {
		XppWinId w;
		char name[MAXCHAR];
		char value[MAXCHAR];
		} PARAM;

typedef struct {
		XppWinId base;
		char title[MAXCHAR];
		PARAM *p;
		int n;
		XppWinId ok;
		XppWinId cancel;
		} PARAM_BOX;
		
		

typedef struct {
		char name[10];
 		char value[80];
		XppWinId w;
		} TCHOICE;

typedef struct {
		char title[100];
		XppWinId who,what,cancel,ok;
		TCHOICE tc[100];
		} TXTCHOICE;


  

#endif






