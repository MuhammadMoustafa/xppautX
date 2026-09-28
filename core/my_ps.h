#ifndef _my_ps_h_
#define _my_ps_h_
#ifdef __cplusplus
extern "C" {
#endif



int ps_init(const char *filename, int color);
void ps_stroke(void);
void ps_do_color(int color);
void ps_end(void);
void ps_bead(int x, int y);
void ps_frect(int x, int y, int w, int h);
void ps_line(int xp1, int yp1, int xp2, int yp2);
void chk_ps_lines(void);
void ps_linetype(int linetype);
void ps_point(int x, int y);
void ps_write(const char *str);
void ps_fnt(int cf, int scale);
void ps_show(const char *str, int type);
void ps_abs(int x, int y);
void ps_rel(int x, int y);
void special_put_text_ps(int x, int y, const char *str, int size);
void ps_text(int x, int y, const char *str);
/* PostScript's own parameter dialog (BW/colour, portrait, axes font
   size, font, linewidth), asked before the file selector in
   graf_par.cpp's export_plot_picture (image_format.h's ask_params, W53,
   issue #101); 0 if the user cancelled */
int ps_ask_params(void);


#ifdef __cplusplus
}

#include <stdio.h>
#include <string>
/* a picture file's state (my_ps.cpp, my_svg.cpp, shared with
   graphics.cpp), a Session's (session.h): which format is being written
   (plt_fmt_flag), in colour, the line count, the last point drawn and
   whether a line goes on from it, the PostScript font, its size and the
   line width (@ ps_font=, ps_fsize=, ps_lw=), and the SVG file open */
struct PlotFileState {
  int plt_fmt_flag = 0, ps_color_flag = 1, ps_lines = 0;
  int last_ps_x = 0, last_ps_y = 0, no_break_line = 0;
  int ps_font_size = 14;
  double ps_lw = 5;
  std::string ps_font = "Times-Roman";
  FILE *svgfile = nullptr;
};
#endif
#endif
