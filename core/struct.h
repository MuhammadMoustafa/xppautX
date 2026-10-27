#pragma once

#include "xpp_types.h"
#include "colormap.h"
#include "line_style.h"
#include <string>
#include <array>
#include <vector>

/* A phase portrait and stored points have three spatial axes. */
inline constexpr int plot_axes = 3;
/* Default plot surface dimensions, in pixels; copies have their own smaller surface. */
inline constexpr int main_plot_width=640, main_plot_height=480;
inline constexpr int extra_plot_width=450, extra_plot_height=350;

struct GRAPH {
	       XppWinId w;

	       int Use;
		int Nullrestore;
                int x11Wid = main_plot_width;
  int x11Hgt = main_plot_height;
		int nvars;
		double rm[3][3];
		double min_scale,color_scale;
		double xmin,ymin,zmin,xmax,ymax,zmax,xorg,yorg,zorg;
		double xbar,ybar,zbar,dx,dy,dz;
		std::vector<int> xv{0},yv{0},zv{0};
                std::vector<xpp::LineStyle> style{xpp::LineStyle{}}; /* each curve's */
                std::vector<xpp::Color> color{xpp::foreground_color};
                /* Resize only when curves are added, removed or read, never per step. */
                void resize_curves(int count) {
                  xv.resize(count, xv.front()); yv.resize(count, yv.front()); zv.resize(count, zv.front());
                  style.resize(count, style.front()); color.resize(count, xpp::foreground_color);
                  for (auto &v : plot_points) v.resize(count);
                  nvars=count;
                }
                std::array<std::vector<float>, 2 * plot_axes> plot_points{
                  std::vector<float>(1),std::vector<float>(1),std::vector<float>(1),
                  std::vector<float>(1),std::vector<float>(1),std::vector<float>(1)};
		double Theta,Phi;
		double ZPlane,ZView;
		double xlo,ylo,xhi,yhi,oldxlo,oldxhi,oldylo,oldyhi;
		int grtype,ThreeDFlag,TimeFlag,PerspFlag;
		int xshft,yshft,zshft;
	        int xorgflag,yorgflag,zorgflag;
		int ColorFlag,ColorValue;
	        std::string xlabel,ylabel,zlabel;
		};

typedef struct {
		XppWinId w;
		float x;
		float y;
		std::string s;
		int id; /* grobs.h: the annotation's id */
		int font,size;
		int style; /* grobs.h: LABEL_STYLE_COUNT */
		xpp::Color color;
		} LABEL;

typedef struct {
                XppWinId w;
		std::string key,name; /* at most 19 and 9 characters */
		short use,type;
		float *xv,*yv,*zv;
		int len;
		xpp::Color color;
		xpp::LineStyle style;
	      } CURVE;




