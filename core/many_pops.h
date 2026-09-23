#ifndef _many_pops_h
#define _many_pops_h
#ifdef __cplusplus
extern "C" {
#endif


int select_table(void);
void get_intern_set(void);
void title_text(char *string);
void restore_off(void);
void restore_on(void);
#ifdef __cplusplus
}
#endif
#include "grobs.h"
#ifdef __cplusplus
extern "C" {
#endif
void destroy_a_pop(void);
void init_grafs(int x, int y, int w, int h);
void ps_restore(void);
void svg_restore(void);
void resize_all_pops(int wid, int hgt);
void kill_all_pops(void);
void create_a_pop(void);
void GrCol(void);
void BaseCol(void);
void SmallGr(void);
void SmallBase(void);
void make_active(int i,int flag);
void set_gr_fore(void);
void set_gr_back(void);
void canvas_xy(char *buf);
void set_active_windows();


#ifdef __cplusplus
}
#endif
#endif
