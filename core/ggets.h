
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
void bottom_msg(int line, char *msg);
void err_msg(char *string);
void gpos_prn(char *string, int row, int col);
void put_command(char *string);
void cput_text(void);
void setfillstyle(int type, int color);
int new_float(char *name, double *value);
int new_int(char *name, int *value);
void display_command(char *name, char *value, int pos, int col);
void movmem(char *s1, char *s2, int len);
void memmov(char *s1, char *s2, int len);
int new_string(char *name, char *value);


#ifdef __cplusplus
}
#endif
#endif
 
