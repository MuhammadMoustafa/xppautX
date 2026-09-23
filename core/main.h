#ifndef _main_h__

#define _main_h__
#ifdef __cplusplus
extern "C" {
#endif



void check_for_quiet(int argc, char **argv);
void do_vis_env(void);
void bye_bye(void);
void clr_scrn(void);
void redraw_all(void);
void commander(int ch);


#ifdef __cplusplus
}
#endif
#endif
 
