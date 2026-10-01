/* The SVG export (Graphic stuff/SVG): svg_init opens the file through an
   xpp::Writer (a temp file, renamed into place only at svg_end's commit),
   graphics.cpp's primitives call the svg_* functions below while the
   picture is redrawn. tests/golden/lecar.svg guards the output byte for
   byte (tools/goldencheck.py). */
#include "my_svg.h"
#include "session.h"
#include "my_ps.h"
#include "xpp_ui.h"
#include "colormap.h"
#include "xpp_log.h"
#include "graphics.h"
#include "graf_par.h"
#include "axes2.h"
#include "nullcline.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include "xpp_globals.h"

namespace xpp {

#define RIGHT 2
#define CENTER 1
#define POINT_TYPES 8

/* The open export's stream while svg_init..svg_end runs, NULL otherwise
   (integrate.cpp and nullcline.cpp group their curves with <g>). */


namespace {

/* The fixed head of every export: the point symbols and the default
   style block, up to where $HOME/xppaut-stylesheet.css is folded in
   (svg_init closes the block after it). */
constexpr std::string_view svg_head = R"svg(<!-- Uncomment following when using your own custom external stylesheet.-->
<!--
<?xml-stylesheet type="text/css" href="xppaut-stylesheet.css" ?>
-->
<svg  xmlns="http://www.w3.org/2000/svg"
      xmlns:xlink="http://www.w3.org/1999/xlink" font-size="12pt" width="640" height="400">


      <defs>
          <circle class="xpppointP" id = "xpppointP"  r = "1"  stroke-width = "1"/>
          <circle class="xppbead" id = "xppbead"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointD" id = "xpppointD"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointA" id = "xpppointA"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointB" id = "xpppointB"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointC" id = "xpppointC"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointT" id = "xpppointT"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointS" id = "xpppointS"  r = "1"  stroke-width = "1"/>
          <circle class="xpppointK" id = "xpppointK"  r = "3"  stroke-width = "0.75"/>
          <circle class="xpppointF" id = "xpppointF"  r = "2"  stroke-width = "0"/>
      </defs>



      <!-- Comment out the following style block when using your own custom external stylesheet.-->
      <!-- As a starting point for your custom external stylesheet, consider copying the style 
           information (between but not including CDATA tags) to a file named xppaut-stylesheet.css 
       -->
      <style type="text/css">
           <![CDATA[
      
                 circle.xpppointP {
                    stroke-width: 1.0;
                 }

                 circle.xpppointD {
                    stroke-width: 1.0;
                 }

                 circle.xpppointA {
                    stroke-width: 1.0;
                 }

                 circle.xpppointB {
                    stroke-width: 1.0;
                 }

                 circle.xpppointC {
                    stroke-width: 1.0;
                 }

                 circle.xpppointT {
                    stroke-width: 1.0;
                 }

                 circle.xpppointS {
                    stroke-width: 1.0;
                 }

                 circle.xpppointK {
                    stroke-width: 1.0;
                 }

                 circle.xpppointF {
                    stroke-width: 1.0;
                 }

                 line.xppaxes {
                    stroke: #000000;
                 }
                 line.xppboxaxes {
                    stroke: #000000;
                 }
                 line.xppdfield {
                    stroke: #000000;
                 }
                 line.xpplineb {
                    stroke: #000000;
                 }
                 line.xpplinea {
                    stroke-dasharray: 2,8;
                    stroke-width: 2;
                    stroke: #000000;
                 }
                 line.xppline0 {
                    stroke: #000000;
                 }
                 line.xppline1 {
                    stroke-width: 1;
                    stroke: #FF0000;
                 }
                 line.xppline2 {
                    stroke: #F06400;
                 }
                 line.xppline3 {
                    stroke: #FFA500;
                 }
                 line.xppline4 {
                    stroke: #FFCD00;
                 }
                 line.xppline5 {
                    stroke: #C8C800;
                 }
                 line.xppline6 {
                    stroke: #00FF00;
                 }
                 line.xppline7 {
                    stroke: #32CD32;
                 }
                 line.xppline8 {
                    stroke: #00C8C8;
                 }
                 line.xppline9 {
                    stroke: #0000FF;
                 }
                 line.xpplinec {
                    stroke: #000000;
                 }
                 
                 text.xpptext {
                    font-family: sans-serif;
                    font-size  : 1em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 text.xppyaxislabelh {
                    font-family: sans-serif;
                    font-size  : 1em;
                    stroke	: none;
                    fill	: none;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 text.xppyaxislabelv {
                    font-family: sans-serif;
                    font-size  : 1em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 text.xppaxestext {
                    font-family: sans-serif;
                    font-size  : 1em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 
                 text.xpptext0 {
                    font-family: sans-serif;
                    font-size  : 0.5em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 
                 text.xpptext1 {
                    font-family: sans-serif;
                    font-size  : 0.75em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 
                 text.xpptext2 {
                    font-family: sans-serif;
                    font-size  : 1em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 
                 text.xpptext3 {
                    font-family: sans-serif;
                    font-size  : 1.25em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
                 
                 text.xpptext4 {
                    font-family: sans-serif;
                    font-size  : 1.5em;
                    stroke	: #000000;
                    fill	: #000000;
                    baseline-shift:-33%;
                    dominant-baseline: central;
                 }
)svg";


/* The text-anchor of TextJustify. */
const char *svg_anchor(const xpp::Session &s)
{
  switch(s.drawing.text_justify) {
  case CENTER: return "middle";
  case RIGHT: return "end";
  default: return "start";
  }
}

/* The style attribute of a coloured stroke (and fill) in cur_rgb. */
std::string svg_stroke(const PlotFileState &pf)
{
  return xpp::format("stroke:rgb({},{},{});",pf.svg_rgb[0],pf.svg_rgb[1],pf.svg_rgb[2]);
}

std::string svg_stroke_fill(const PlotFileState &pf)
{
  return xpp::format("stroke:rgb({0},{1},{2}); fill:rgb({0},{1},{2})",pf.svg_rgb[0],pf.svg_rgb[1],pf.svg_rgb[2]);
}

} // namespace

xpp::Result<> svg_init(xpp::Session &s, const char *filename, int /*color*/)
{
  init_svg(s);

  s.plot_file.last_ps_x=-10000;
  s.plot_file.last_ps_y=-10000;

  s.plot_file.writer=xpp::Writer(filename);
  if(!s.plot_file.writer)return xpp::fail("SVG export",xpp::format("Cannot write {}",filename),command_place());
  s.plot_file.svgfile=s.plot_file.writer.file();
  s.plot_file.plt_fmt_flag=SVGFMT;
  s.plot_file.writer.print("{}",svg_head);

  if(const char *home=std::getenv("HOME")){
    /* copied in whole lines; a trailing newline is put back on each one,
       so a css file that itself ends without one gains a final newline. */
    const std::string css=xpp::format("{}/xppaut-stylesheet.css",home);
    xpp::LineReader lr(css.c_str());
    if(lr){
      xpp::log(XPP_LOG_INFO, "Styling svg image according to {}\n",css);
      while(auto line=lr.next())
        s.plot_file.writer.print("{}\n",*line);
    }
  }
  s.plot_file.writer.print("           ]]>\n      </style>\n\n");
  return {};
}

void svg_write(PlotFileState &pf, const char *str)
{
  pf.writer.print("{}\n",str);
}

void svg_do_color(PlotFileState &pf, int color)
{
  if(pf.plt_fmt_flag==SCRNFMT)return;
  if(pf.plt_fmt_flag==PSFMT)return;
  if(pf.ps_color_flag==0)return;
  get_svg_color(color,&pf.svg_rgb[0],&pf.svg_rgb[1],&pf.svg_rgb[2]);
  pf.svg_color=true;
}

void svg_end(xpp::Session &s)
{
  svg_write(s.plot_file,"</svg>");
  s.plot_file.writer.commit();
  s.plot_file.svgfile=NULL;
  s.plot_file.plt_fmt_flag=SCRNFMT;
  s.plot_file.svg_color=false;
  if(program.interactive)init_x11(s);
}

void svg_bead(xpp::Session &s, int /*x*/, int /*y*/)
{
  s.plot_file.svg_marker=true;
}

void svg_frect(xpp::Session &s, int x, int y, int w, int h)
{
  if (s.plot_file.svg_color)
    s.plot_file.writer.print("      <rect x=\"{0}\" y=\"{1}\" width=\"{2}\" height=\"{3}\" style=\"stroke:rgb({4},{5},{6});fill:rgb({4},{5},{6});\"/>",
              x,y,w,h,s.plot_file.svg_rgb[0],s.plot_file.svg_rgb[1],s.plot_file.svg_rgb[2]);
  else {
    const int gray = static_cast<int>(0.299*s.plot_file.svg_rgb[0] + 0.587*s.plot_file.svg_rgb[1] + 0.114*s.plot_file.svg_rgb[2]);
    s.plot_file.writer.print("      <rect x=\"{0}\" y=\"{1}\" width=\"{2}\" height=\"{3}\" style=\"stroke:rgb({4},{4},{4});fill:rgb({4},{4},{4});\"/>",
              x,y,w,h,gray);
  }
}

void svg_line(xpp::Session &s, int xp1, int yp1, int xp2, int yp2)
{
  /* the line's class: the axes, the box axes, a direction-field arrow
     or a curve of line type s.plot_file.svg_line_type */
  std::string cls;
  if (s.drawing.doing_axes)
    cls = s.drawing.doing_box_axes ? "xppboxaxes" : "xppaxes";
  else if (s.nullclines.doing_dfield)
    cls = "xppdfield";
  else
    cls = xpp::format("xppline{}",s.plot_file.svg_line_type);
  const std::string style = s.plot_file.svg_color ? " style=\""+svg_stroke(s.plot_file)+"\"/>" : " />";
  /* a direction-field arrow with its bead is a group */
  const bool arrow = !s.drawing.doing_axes && s.nullclines.doing_dfield && s.plot_file.svg_marker;
  if (arrow)
    s.plot_file.writer.print("<g>\n");
  s.plot_file.writer.print("      <line class=\"{}\"  x1=\"{}\"  y1=\"{}\" x2=\"{}\"   y2=\"{}\"{}\n",cls,xp1,yp1,xp2,yp2,style);
  if (arrow) {
    if (s.plot_file.svg_color)
      s.plot_file.writer.print("      <use xlink:href = \"#xppbead\" x=\"{}\" y=\"{}\" style=\"{}\"/>\n",xp2,yp2,svg_stroke_fill(s.plot_file));
    else
      s.plot_file.writer.print("      <use xlink:href = \"#xppbead\" x=\"{}\" y=\"{}\" />\n",xp2,yp2);
    s.plot_file.writer.print("</g>\n");
  }

  s.plot_file.last_ps_x=xp2;
  s.plot_file.last_ps_y=yp2;

  s.plot_file.svg_color=false;
  s.plot_file.svg_marker=false;
}

void svg_linetype(xpp::Session &s, int linetype)
{
  constexpr std::string_view line = "ba0123456789c";
  s.plot_file.svg_line_type=line[(linetype%11)+2];
  s.plot_file.ps_lines=0;
}

void svg_point(xpp::Session &s, int x, int y)
{
  constexpr std::string_view point="PDABCTSKF";
  int number=s.drawing.point_type;
  number %= POINT_TYPES;
  if(number < -1)
    number = -1;
  if(s.drawing.point_radius>0)number=7;

  if (s.plot_file.svg_color)
    s.plot_file.writer.print("      <use xlink:href = \"#xpppoint{}\" x=\"{}\" y=\"{}\" style=\"{}\"/>\n",
              point[number+1],x,y,svg_stroke_fill(s.plot_file));
  else {
    const char *col = "000000", *fill = "#000000";
    if (number==7) {
      col = "00FF00";
      fill = "#00FF00";
    } else if (number==6) {
      col = "0000FF";
      fill = "none";
    }
    s.plot_file.writer.print("      <use xlink:href = \"#xpppoint{}\" x=\"{}\" y=\"{}\" style=\"stroke:#{}; fill:{}\"/>\n",
              point[number+1],x,y,col,fill);
  }

  s.plot_file.ps_lines=0;
  s.plot_file.svg_color=false;
}

void special_put_text_svg(xpp::Session &s, int x, int y, const char *str, int size)
{
  s.plot_file.writer.print("\n      <text class=\"xpptext{}\" text-anchor=\"{}\" x=\"{}\"  y=\"{}\"\n",size,svg_anchor(s),x,y);
  s.plot_file.writer.print("      >{}</text>\n",str);
}

void svg_text(xpp::Session &s, int x, int y, const char *str)
{
  s.plot_file.writer.print("\n      <text class=\"{}\" text-anchor=\"{}\" x=\"{}\"  y=\"{}\"\n",
            s.drawing.doing_axes ? "xppaxestext" : "xpptext",svg_anchor(s),x,y);
  s.plot_file.writer.print("      >{}</text>\n",str);
}

void svg_y_axis_label(PlotFileState &pf, int x, int y, const char *label)
{
  pf.writer.print("\n      <text class=\"xppyaxislabelv\" text-anchor=\"middle\" x=\"0\"  y=\"0\"\n");
  pf.writer.print("      transform=\"rotate(-90,75,180) translate(75,180)\"\n");
  pf.writer.print("      >{}</text>\n",label);
  pf.writer.print("\n      <text class=\"xppyaxislabelh\" text-anchor=\"end\" x=\"{}\"  y=\"{}\"\n",x,y);
  pf.writer.print("      >{}</text>\n",label);
}

} // namespace xpp
