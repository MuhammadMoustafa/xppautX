#ifndef _txtread_h
#define _txtread_h
#ifdef __cplusplus
extern "C" {
#endif



void do_txt_action(char *s);
void resize_txtview(int w, int h);
void redraw_txtview_text(void);
void init_txtview(void);
void make_txtview(void);


#ifdef __cplusplus
}
#endif
#endif
