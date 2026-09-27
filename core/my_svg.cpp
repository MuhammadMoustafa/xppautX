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

#define RIGHT 2
#define CENTER 1
#define POINT_TYPES 8

/* The open export's stream while svg_init..svg_end runs, NULL otherwise
   (integrate.cpp and nullcline.cpp group their curves with <g>). */
FILE *svgfile;


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

xpp::Writer svg_writer;
char svg_line_type;
int cur_rgb[3];
bool doing_svg_color = false;
bool do_marker = false;

/* The text-anchor of TextJustify. */
const char *svg_anchor()
{
  switch(TextJustify) {
  case CENTER: return "middle";
  case RIGHT: return "end";
  default: return "start";
  }
}

/* The style attribute of a coloured stroke (and fill) in cur_rgb. */
std::string svg_stroke()
{
  return xpp::format("stroke:rgb({},{},{});",cur_rgb[0],cur_rgb[1],cur_rgb[2]);
}

std::string svg_stroke_fill()
{
  return xpp::format("stroke:rgb({0},{1},{2}); fill:rgb({0},{1},{2})",cur_rgb[0],cur_rgb[1],cur_rgb[2]);
}

} // namespace

int svg_init(const char *filename, int /*color*/)
{
  init_svg();

  LastPSX=-10000;
  LastPSY=-10000;

  svg_writer=xpp::Writer(filename);
  if(!svg_writer){
    err_msg("Cannot open file ");
    return(0);
  }
  svgfile=svg_writer.file();
  PltFmtFlag=SVGFMT;
  svg_writer.print("{}",svg_head);

  if(const char *home=std::getenv("HOME")){
    /* copied in whole lines; a trailing newline is put back on each one,
       so a css file that itself ends without one gains a final newline. */
    const std::string css=xpp::format("{}/xppaut-stylesheet.css",home);
    xpp::LineReader lr(css.c_str());
    if(lr){
      xpp::log(XPP_LOG_INFO, "Styling svg image according to {}\n",css);
      while(auto line=lr.next())
        svg_writer.print("{}\n",*line);
    }
  }
  svg_writer.print("           ]]>\n      </style>\n\n");
  return(1);
}

void svg_write(const char *str)
{
  svg_writer.print("{}\n",str);
}

void svg_do_color(int color)
{
  if(PltFmtFlag==SCRNFMT)return;
  if(PltFmtFlag==PSFMT)return;
  if(PSColorFlag==0)return;
  get_svg_color(color,&cur_rgb[0],&cur_rgb[1],&cur_rgb[2]);
  doing_svg_color=true;
}

void svg_end(void)
{
  svg_write("</svg>");
  svg_writer.commit();
  svgfile=NULL;
  PltFmtFlag=SCRNFMT;
  doing_svg_color=false;
  if(program.interactive)init_x11();
}

void svg_bead(int /*x*/, int /*y*/)
{
  do_marker=true;
}

void svg_frect(int x, int y, int w, int h)
{
  if (doing_svg_color)
    svg_writer.print("      <rect x=\"{0}\" y=\"{1}\" width=\"{2}\" height=\"{3}\" style=\"stroke:rgb({4},{5},{6});fill:rgb({4},{5},{6});\"/>",
              x,y,w,h,cur_rgb[0],cur_rgb[1],cur_rgb[2]);
  else {
    const int gray = static_cast<int>(0.299*cur_rgb[0] + 0.587*cur_rgb[1] + 0.114*cur_rgb[2]);
    svg_writer.print("      <rect x=\"{0}\" y=\"{1}\" width=\"{2}\" height=\"{3}\" style=\"stroke:rgb({4},{4},{4});fill:rgb({4},{4},{4});\"/>",
              x,y,w,h,gray);
  }
}

void svg_line(int xp1, int yp1, int xp2, int yp2)
{
  /* the line's class: the axes, the box axes, a direction-field arrow
     or a curve of line type svg_line_type */
  std::string cls;
  if (DOING_AXES)
    cls = DOING_BOX_AXES ? "xppboxaxes" : "xppaxes";
  else if (xpp::session().nullclines.doing_dfield)
    cls = "xppdfield";
  else
    cls = xpp::format("xppline{}",svg_line_type);
  const std::string style = doing_svg_color ? " style=\""+svg_stroke()+"\"/>" : " />";
  /* a direction-field arrow with its bead is a group */
  const bool arrow = !DOING_AXES && xpp::session().nullclines.doing_dfield && do_marker;
  if (arrow)
    svg_writer.print("<g>\n");
  svg_writer.print("      <line class=\"{}\"  x1=\"{}\"  y1=\"{}\" x2=\"{}\"   y2=\"{}\"{}\n",cls,xp1,yp1,xp2,yp2,style);
  if (arrow) {
    if (doing_svg_color)
      svg_writer.print("      <use xlink:href = \"#xppbead\" x=\"{}\" y=\"{}\" style=\"{}\"/>\n",xp2,yp2,svg_stroke_fill());
    else
      svg_writer.print("      <use xlink:href = \"#xppbead\" x=\"{}\" y=\"{}\" />\n",xp2,yp2);
    svg_writer.print("</g>\n");
  }

  LastPSX=xp2;
  LastPSY=yp2;

  doing_svg_color=false;
  do_marker=false;
}

void svg_linetype(int linetype)
{
  constexpr std::string_view line = "ba0123456789c";
  svg_line_type=line[(linetype%11)+2];
  PSLines=0;
}

void svg_point(int x, int y)
{
  constexpr std::string_view point="PDABCTSKF";
  int number=PointType;
  number %= POINT_TYPES;
  if(number < -1)
    number = -1;
  if(PointRadius>0)number=7;

  if (doing_svg_color)
    svg_writer.print("      <use xlink:href = \"#xpppoint{}\" x=\"{}\" y=\"{}\" style=\"{}\"/>\n",
              point[number+1],x,y,svg_stroke_fill());
  else {
    const char *col = "000000", *fill = "#000000";
    if (number==7) {
      col = "00FF00";
      fill = "#00FF00";
    } else if (number==6) {
      col = "0000FF";
      fill = "none";
    }
    svg_writer.print("      <use xlink:href = \"#xpppoint{}\" x=\"{}\" y=\"{}\" style=\"stroke:#{}; fill:{}\"/>\n",
              point[number+1],x,y,col,fill);
  }

  PSLines=0;
  doing_svg_color=false;
}

void special_put_text_svg(int x, int y, const char *str, int size)
{
  svg_writer.print("\n      <text class=\"xpptext{}\" text-anchor=\"{}\" x=\"{}\"  y=\"{}\"\n",size,svg_anchor(),x,y);
  svg_writer.print("      >{}</text>\n",str);
}

void svg_text(int x, int y, const char *str)
{
  svg_writer.print("\n      <text class=\"{}\" text-anchor=\"{}\" x=\"{}\"  y=\"{}\"\n",
            DOING_AXES ? "xppaxestext" : "xpptext",svg_anchor(),x,y);
  svg_writer.print("      >{}</text>\n",str);
}

void svg_y_axis_label(int x, int y, const char *label)
{
  svg_writer.print("\n      <text class=\"xppyaxislabelv\" text-anchor=\"middle\" x=\"0\"  y=\"0\"\n");
  svg_writer.print("      transform=\"rotate(-90,75,180) translate(75,180)\"\n");
  svg_writer.print("      >{}</text>\n",label);
  svg_writer.print("\n      <text class=\"xppyaxislabelh\" text-anchor=\"end\" x=\"{}\"  y=\"{}\"\n",x,y);
  svg_writer.print("      >{}</text>\n",label);
}
