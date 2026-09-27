#ifndef _struct_h_
#define _struct_h_

#include "xpplim.h"
#include "xpp_types.h"
#include <string>

#define MAXPERPLOT 10
#define MAXFRZ 26
#define MAXPOP 21

typedef struct {
	       XppWinId w;

	       int Use;
		int Nullrestore;
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
	        std::string xlabel,ylabel,zlabel;
		} GRAPH;

typedef struct {
		XppWinId w;
		float x;
		float y;
		std::string s;
		short use;
		int font,size;
		} LABEL;

typedef struct {
                XppWinId w;
		std::string key,name; /* at most 19 and 9 characters */
		short use,type;
		float *xv,*yv,*zv;
		int len,color;
	      } CURVE;



#endif

