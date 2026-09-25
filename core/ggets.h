
#ifndef _ggets_h
#define _ggets_h
#ifdef __cplusplus
extern "C" {
#endif


#define MaxIncludeFiles 10
#define ClickTime 200

void ping(void);
void reset_graphics(void);
void set_fore(void);
void set_back(void);
void chk_xor(void);
void set_gcurs(int y, int x);
void clr_command(void);
void bottom_msg(int line, const char *msg);
void err_msg(const char *string);
void cput_text(void);
void setfillstyle(int type, int color);
int new_float(const char *name, double *value);
int new_int(const char *name, int *value);
int new_string(const char *name, char *value);


#ifdef __cplusplus
}
#endif
#endif
 
