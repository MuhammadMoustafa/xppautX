#ifndef _auto_x11_h_
#define _auto_x11_h_
#ifdef __cplusplus
extern "C" {
#endif


void ALINE(int a, int b, int c, int d);
void DLINE(double a, double b, double c, double d);
void ATEXT(int a, int b, char *c);
void clear_auto_plot(void);
void redraw_auto_menus(void);
void clear_auto_info(void);
void draw_auto_info(char *bob, int x, int y);
void refreshdisplay(void);
int byeauto_(int *iflag);
void Circle(int x, int y, int r);
void autocol(int col);
void autobw(void);
int auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag);
int auto_pop_up_list(char *title, char **list, char *key, int n, int max, int def, int x, int y, char **hints, char *httxt);
void XORCross(int x, int y);
void FillCircle(int x, int y, int r);
void LineWidth(int wid);
void make_auto(char *wname, char *iname);
void a_msg(int i, int v);
void auto_kill(void);
void find_point(int ibr,int pt);
void auto_get_info( int *n, char *pname);
void auto_set_mark(int i);
void do_auto_range();

#ifdef __cplusplus
}
#endif
#endif
