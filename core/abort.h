#ifndef _abort_h_
#define _abort_h_
#ifdef __cplusplus
extern "C" {
#endif


/* abort.c */
int get_command_width(void);
void plot_command(int nit, int icount, int cwidth);
int my_abort(void);


#ifdef __cplusplus
}
#endif
#endif
