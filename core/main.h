#ifndef _main_h__

#define _main_h__
#ifdef __cplusplus
extern "C" {
#endif



void do_main(int argc, char **argv);
void check_for_quiet(int argc, char **argv);
void do_vis_env(void);
void init_X(void);
void set_big_font(void);
void set_small_font(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void xpp_events(XEvent report, int min_wid, int min_hgt);
#endif /* Xlib.h */
void do_events(unsigned int min_wid, unsigned int min_hgt);
void bye_bye(void);
void clr_scrn(void);
void redraw_all(void);
void commander(int ch);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
Window init_win(unsigned int bw, char *icon_name, char *win_name, int x, int y, unsigned int min_wid, unsigned int min_hgt, int argc, char **argv);
void top_button_draw(Window w);
void top_button_cross(Window w, int b);
void top_button_press(Window w);
void top_button_events(XEvent report);
#endif /* Xlib.h */
void make_top_buttons(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void getGC(GC *gc);
#endif /* Xlib.h */
void load_fonts(void);
void make_pops(void);
#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)
void FixWindowSize(Window w, int width, int height, int flag);
int getxcolors(XWindowAttributes *win_info, XColor **colors);
#endif /* Xlib.h */
void test_color_info(void);


#ifdef __cplusplus
}
#endif
#endif
 
