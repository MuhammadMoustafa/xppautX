
#ifndef _my_svg_h_
#define _my_svg_h_
#ifdef __cplusplus
extern "C" {
#endif


/* my_ps.c */
int svg_init(char *filename, int color);
void svg_do_color(int color);
void svg_end(void);
void svg_bead(int x, int y);
void svg_frect(int x, int y, int w, int h);
void svg_last_pt_off(void);
void svg_line(int xp1, int yp1, int xp2, int yp2);
void svg_linetype(int linetype);
void svg_point(int x, int y);
void svg_write(char *str);
void special_put_text_svg(int x, int y, char *str, int size);
void svg_text(int x, int y, char *str);


#ifdef __cplusplus
}
#endif
#endif
