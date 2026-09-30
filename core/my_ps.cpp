/* The PostScript export (Graphic stuff/Postscript): ps_init opens the
   file through an xpp::Writer (a temp file, renamed into place only at
   ps_end's commit), graphics.cpp's primitives call the ps_* functions
   below while the picture is redrawn. the tests/golden .ps files guard the output
   byte for byte (tools/goldencheck.py). */
#include "my_ps.h"
#include "session.h"
#include "xpp_ui.h"
#include "colormap.h"
#include "lunch-new.h"
#include "graphics.h"

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include "xpp_io.h"
#include "xpp_globals.h"
#define MAXPSLINE 100

#define PS_XOFF 50
#define PS_YOFF 50
#define PS_XMAX 7200
#define PS_YMAX 5040

#define PS_VTIC (PS_YMAX/80)
#define PS_HTIC (PS_YMAX/80)

#define PS_SC (10)				/* scale is 1pt = 10 units */
		

#define LEFT 0
#define RIGHT 2
#define CENTER 1
#define POINT_TYPES 8

namespace {

/* this header stuff was stolen from GNUPLOT I have added  filled circles
    and open circles for bifurcation diagrams I also use Times Roman
    since Courier is an ugly font!!  
*/

constexpr std::string_view ps_header[]={
"/vpt2 vpt 2 mul def\n",
"/hpt2 hpt 2 mul def\n",
"/Romfnt {/Times-Roman findfont exch scalefont setfont} def ",
"/Symfnt {/Symbol findfont exch scalefont setfont} def ",
/* flush left show */
"/Lshow { currentpoint stroke moveto\n",
"  0 vshift rmoveto show } def\n", 
/* flush right show */
"/Rshow { currentpoint stroke moveto\n",
"  dup stringwidth pop neg vshift rmoveto show } def\n", 
/* centred show */
"/Cshow { currentpoint stroke moveto\n",
"  dup stringwidth pop -2 div vshift rmoveto show } def\n", 
/* Dash or Color Line */
"/DL { Color {setrgbcolor [] 0 setdash pop}\n",
" {pop pop pop 0 setdash} ifelse } def\n",
/* Border Lines */
"/BL { stroke xpplinewidth 2 mul setlinewidth } def\n",
/* Axes Lines */
"/AL { stroke xpplinewidth 2 div setlinewidth } def\n",
/* Plot Lines */
"/PL { stroke xpplinewidth setlinewidth } def\n",
/* Line Types */
"/LTb { BL [] 0 0 0 DL } def\n", /* border */
"/LTa { AL [1 dl 2 dl] 0 setdash 0 0 0 setrgbcolor } def\n", /* axes */
"/LT0 { PL [] 0 0 0 DL } def\n",
"/LT1 { PL [4 dl 2 dl] 1 0 0 DL } def\n",
"/LT2 { PL [2 dl 3 dl] .95 .4 0 DL } def\n",
"/LT3 { PL [1 dl 1.5 dl] 1 .65 0 DL } def\n",
"/LT4 { PL [5 dl 2 dl 1 dl 2 dl] 1 .8 0 DL } def\n",
"/LT5 { PL [4 dl 3 dl 1 dl 3 dl] .85 .85 0 DL } def\n",
"/LT6 { PL [2 dl 2 dl 2 dl 4 dl]  .6 .8 .2 DL } def\n",
"/LT7 { PL [2 dl 2 dl 2 dl 2 dl 2 dl 4 dl] 0 .9 0 DL } def\n",
"/LT8 { stroke 16. setlinewidth [] 0 .85 .85 DL } def\n", /* really fat line */
"/LT9 { stroke 16. setlinewidth [4 dl 2 dl] 0 0 1 DL } def\n",
"/LTc { stroke 16. setlinewidth [2 dl 3 dl] .62 .125 .93 DL } def\n",
"/M {moveto} def\n",
"/L {lineto} def\n",
"/R {rlineto} def\n",
"/P { stroke [] 0 setdash\n", /* Point */
"  currentlinewidth 2 div sub moveto\n",
"  0 currentlinewidth rlineto  stroke } def\n",
"/D { stroke [] 0 setdash  2 copy  vpt add moveto\n", /* Diamond */
"  hpt neg vpt neg rlineto  hpt vpt neg rlineto\n",
"  hpt vpt rlineto  hpt neg vpt rlineto  closepath  stroke\n",
"  P  } def\n",
"/A { stroke [] 0 setdash  vpt sub moveto  0 vpt2 rlineto\n", /* Plus (Add) */
"  currentpoint stroke moveto\n",
"  hpt neg vpt neg rmoveto  hpt2 0 rlineto stroke\n",
"  } def\n",
"/B { stroke [] 0 setdash  2 copy  exch hpt sub exch vpt add moveto\n", /* Box */
"  0 vpt2 neg rlineto  hpt2 0 rlineto  0 vpt2 rlineto\n",
"  hpt2 neg 0 rlineto  closepath  stroke\n",
"  P  } def\n",
"/C { stroke [] 0 setdash  exch hpt sub exch vpt add moveto\n", /* Cross */
"  hpt2 vpt2 neg rlineto  currentpoint  stroke  moveto\n",
"  hpt2 neg 0 rmoveto  hpt2 vpt2 rlineto stroke  } def\n",
"/T { stroke [] 0 setdash  2 copy  vpt 1.12 mul add moveto\n", /* Triangle */
"  hpt neg vpt -1.62 mul rlineto\n",
"  hpt 2 mul 0 rlineto\n",
"  hpt neg vpt 1.62 mul rlineto  closepath  stroke\n",
"  P  } def\n",
"/S { 2 copy A C} def\n", /* Star */
"/K { stroke [] 0 setdash vpt 0 360 arc stroke} def ", /* Circle */
"/F { stroke [] 0 setdash vpt 0 360 arc fill stroke } def ", /* Filled circle */
};

xpp::Writer ps_writer;

/* str as a PostScript string: '(', ')' and '\' escaped, in parentheses */
std::string ps_string(std::string_view str)
{
  std::string s = "(";
  for (const char ch : str) {
    if (ch == '(' || ch == ')' || ch == '\\')
      s += '\\';
    s += ch;
  }
  s += ')';
  return s;
}

} // namespace

xpp::Result<> ps_init(xpp::Session &s, const char *filename, int color)
{
  ps_writer = xpp::Writer(filename);
  if (!ps_writer) return xpp::fail("PostScript export","Cannot open file ");
  init_ps(s);
  s.plot_file.plt_fmt_flag=1;
  s.plot_file.ps_lines=0;
  s.plot_file.last_ps_x=-10000;
  s.plot_file.last_ps_y=-10000;
  const int ps_vchar=s.plot_file.ps_font_size*PS_SC; /* the characters' height */
  ps_writer.print("%!PS-Adobe-2.0\n");
  ps_writer.print("%Creator: xppaut\n");
  ps_writer.print("%%BoundingBox: {} {} {} {}\n",PS_XOFF,PS_YOFF,
           static_cast<int>(PS_YMAX/PS_SC+.5+PS_YOFF+0.1*ps_vchar),static_cast<int>(PS_XMAX/PS_SC+.5+PS_XOFF+0.1*ps_vchar));
  ps_writer.print("/xppdict 40 dict def\nxppdict begin\n");
  if(color==0){
    ps_writer.print("/Color false def \n");
    s.plot_file.ps_color_flag=0;
  }
  else {
    ps_writer.print("/Color true def \n");
    ps_writer.print("/RGB {{setrgbcolor currentpoint stroke moveto}} def\n");
    ps_writer.print("/RGb {{setrgbcolor }} def\n");
    s.plot_file.ps_color_flag=1;
  }
  ps_writer.print("/xpplinewidth {:.3f} def\n",s.plot_file.ps_lw);
  ps_writer.print("/vshift {} def\n", static_cast<int>(ps_vchar)/(-3));
  ps_writer.print("/dl {{{} mul}} def\n",PS_SC); /* dash length */
  ps_writer.print("/hpt {:.1f} def\n",PS_HTIC/2.0);
  ps_writer.print("/vpt {:.1f} def\n",PS_VTIC/2.0);
  for (const std::string_view h : ps_header)
    ps_writer.print("{}",h);
  ps_writer.print("end\n");
  ps_writer.print("%%EndProlog\n");
  ps_writer.print("xppdict begin\n");
  ps_writer.print("gsave\n");
  ps_writer.print("{} {} translate\n",PS_XOFF,PS_YOFF);
  ps_writer.print("{:.3f} {:.3f} scale\n", 1./PS_SC,1./PS_SC);
  if(!s.drawing.ps_port)
    ps_writer.print("90 rotate\n0 {} translate\n", -PS_YMAX);
  ps_writer.print("/{} findfont {} ",s.plot_file.ps_font,s.plot_file.ps_font_size*PS_SC);
  ps_writer.print("scalefont setfont\n");
  ps_writer.print("newpath\n");
  return {};
}

void ps_stroke()
{
  ps_writer.print("stroke\n");
}

void ps_do_color(const PlotFileState &pf, int color)
{
  float r,g,b;
  if(pf.plt_fmt_flag==0)return;
  if(pf.ps_color_flag==0)return;
  get_ps_color(color,&r,&g,&b);
  ps_writer.print("{:f} {:f} {:f} RGb\n",r,g,b);
}

void ps_end(xpp::Session &s)
{
  ps_write("stroke");
  ps_write("grestore");
  ps_write("end");
  ps_write("showpage");
  ps_write_pars(s,ps_writer.file());
  ps_writer.commit();
  s.plot_file.plt_fmt_flag=0;
  if(program.interactive)init_x11(s);
}

void ps_bead(xpp::Session &, int /*x*/, int /*y*/)
{
}

void ps_frect(xpp::Session &, int x, int y, int w, int h)
{
  ps_writer.print(" newpath {} {} M {} {} R {} {} R {} {} R closepath fill\n",x,y,0,-h,w,0,0,h);
}

void ps_line(xpp::Session &s, int xp1, int yp1, int xp2, int yp2)
{
  if(s.plot_file.no_break_line!=1 && xp1==s.plot_file.last_ps_x && yp1==s.plot_file.last_ps_y){
    s.plot_file.last_ps_x=xp2;
    s.plot_file.last_ps_y=yp2;
    ps_writer.print("{} {} L\n",xp2,yp2);
  }
  else if(s.plot_file.no_break_line!=1 && xp2==s.plot_file.last_ps_x && yp2==s.plot_file.last_ps_y){
    s.plot_file.last_ps_x=xp1;
    s.plot_file.last_ps_y=yp1;
    ps_writer.print("{} {} L\n",xp1,yp1);
  }
  else {
    ps_writer.print("{} {} M\n{} {} L\n",xp1,yp1,xp2,yp2);
    s.plot_file.last_ps_x=xp2;
    s.plot_file.last_ps_y=yp2;
  }
  chk_ps_lines(s);
}

void chk_ps_lines(xpp::Session &s)
{
  s.plot_file.ps_lines++;
  if(s.plot_file.ps_lines>=MAXPSLINE){
    ps_writer.print("currentpoint stroke moveto\n");
    s.plot_file.ps_lines=0;
  }
}

void ps_linetype(xpp::Session &s, int linetype)
{
  constexpr std::string_view line = "ba0123456789c";
  ps_writer.print("LT{}\n", line[(linetype%11)+2]);
  s.plot_file.ps_lines=0;
  s.plot_file.last_ps_x=-100000000;
  s.plot_file.last_ps_y=-100000000;
}

void ps_point(xpp::Session &s, int x, int y)
{
  constexpr std::string_view point="PDABCTSKF";
  int number=s.drawing.point_type;
  number %= POINT_TYPES;
  if(number < -1)
    number = -1;
  if(s.drawing.point_radius>0)number=7;
  ps_writer.print("{} {} {}\n",x,y,point[number+1]);
  s.plot_file.ps_lines=0;
}

void ps_write(const char *str)
{
  ps_writer.print("{}\n",str);
}

void ps_fnt(const xpp::Session &s, int cf,int scale)
{
  if(cf==0)
    ps_writer.print("/{} findfont {} scalefont setfont \n",s.plot_file.ps_font,scale);
  else
    ps_writer.print("{} Symfnt\n",scale);
}

void ps_show(xpp::Session &s, const char *str,int type)
{
  ps_writer.print("{} {}\n",ps_string(str),type==1 ? "Lshow" : "show");
  s.plot_file.ps_lines=0;
}

void ps_abs(int x, int y)
{
  ps_writer.print("{} {} moveto \n",x,y);
}

void ps_rel(int x, int y)
{
  ps_writer.print("{} {} rmoveto \n",x,y);
}

/* str with its escapes: \0 and \1 the text and the symbol font, \s and
   \S a sub- and superscript, \n back to the base line */
void special_put_text_ps(xpp::Session &s, int x, int y, const char *str, int size)
{
  static constexpr std::array<int,5> sz={8,10,14,18,24};
  int type=1;
  int cf=0;
  int cy=0;
  std::string tmp;
  ps_writer.print("0 0 0 setrgbcolor \n");
  ps_abs(x,y);
  int pssz=sz[size]*PS_SC;
  const int sub=static_cast<int>(.3*pssz);
  const int sup=static_cast<int>(.6*pssz);
  ps_fnt(s,cf,pssz);
  const std::string_view text(str);
  for(size_t i=0;i<text.size();i++){
    const char c=text[i];
    if(c!='\\'){
      tmp+=c;
      continue;
    }
    i++;
    const char e=i<text.size() ? text[i] : '\0';
    if(!tmp.empty()){
      ps_show(s,tmp.c_str(),type);
      type=0;
    }
    tmp.clear();
    if(e=='0'){
      cf=0;
      ps_fnt(s,cf,pssz);
    }
    if(e=='n'){
      ps_rel(0,-cy);
      cy=0;
      pssz=PS_SC*sz[size];
      ps_fnt(s,cf,pssz);
    }
    if(e=='s'){
      cy=cy-sub;
      ps_rel(0,-sub);
      pssz=3*PS_SC*sz[size]/5;
      ps_fnt(s,cf,pssz);
    }
    if(e=='S'){
      pssz=3*PS_SC*sz[size]/5;
      cy=cy+sup;
      ps_rel(0,sup);
      ps_fnt(s,cf,pssz);
    }
    if(e=='1'){
      cf=1;
      ps_fnt(s,cf,pssz);
    }
  }
  if(!tmp.empty())
    ps_show(s,tmp.c_str(),type);
}

void ps_text(xpp::Session &s, int x, int y, const char *str)
{
  ps_writer.print("0 0 0 setrgbcolor \n");
  ps_writer.print("/{} findfont {} ",s.plot_file.ps_font,s.plot_file.ps_font_size*PS_SC);
  ps_writer.print("scalefont setfont\n");
  ps_writer.print("{} {} moveto\n",x,y);
  if (TextAngle != 0)
    ps_writer.print("currentpoint gsave translate {} rotate 0 0 moveto\n",TextAngle*90);
  ps_writer.print("{}",ps_string(str));
  switch(s.drawing.text_justify) {
  case LEFT : ps_writer.print(" Lshow\n");
    break;
  case CENTER : ps_writer.print(" Cshow\n");
    break;
  case RIGHT : ps_writer.print(" Rshow\n");
    break;
  }
  if (TextAngle != 0)
    ps_writer.print("grestore\n");
  s.plot_file.ps_lines=0;
}

int ps_ask_params(xpp::Session &s)
{
  static const char *nn[]={"BW-0/Color-1","Land(0)/Port(1)","Axes fontsize","Font","Linewidth"};
  std::array<std::string,5> values;
  values[0]=xpp::format("{:d}",s.plot_export.color);
  values[1]=xpp::format("{:d}",s.drawing.ps_port);
  values[2]=xpp::format("{:d}",s.plot_file.ps_font_size);
  values[3]=xpp::format("{:.24}",s.plot_file.ps_font);
  values[4]=xpp::format("{:g}",s.plot_file.ps_lw);
  static const int kinds[]={XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_INTEGER,XPP_FIELD_TEXT,XPP_FIELD_NUMBER};
  if(!do_string_box_of(5,1,"Postscript parameters",nn,values,kinds))return 0;
  s.plot_export.color=atoi(values[0].c_str());
  s.drawing.ps_port=atoi(values[1].c_str());
  s.plot_file.ps_font_size=atoi(values[2].c_str());
  s.plot_file.ps_lw=atof(values[4].c_str());
  s.plot_file.ps_font=values[3];
  ping();
  return 1;
}
