
#ifndef _delay_handle_h_
#define _delay_handle_h_
#ifdef __cplusplus
extern "C" {
#endif

/* delay_handle.cpp: the delay equations' settings (DelayFlag: the model
   has delays; DelayGrid, AlphaMax, OmegaMax: the stability search's) */
extern int DelayFlag,DelayGrid,del_stab_flag;
extern double AlphaMax,OmegaMax;
double delay_stab_eval(double delay, int var);
int alloc_delay(double big);
void free_delay(void);
void stor_delay(double *y);
void polint(double *xa, double *ya, int n, double x, double *y, double *dy);
double get_delay(int in, double tau);
int do_init_delay(double big);

#ifdef __cplusplus
}
#endif
#endif
