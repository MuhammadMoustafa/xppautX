#ifndef _pop_list_h
#define _pop_list_h



#include "phsplan.h"
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include "xpplim.h"
#include "math.h"
#ifdef __cplusplus
extern "C" {
#endif














	
	


extern int DisplayWidth,DisplayHeight;
extern int screen;
extern int xor_flag;
extern unsigned int MyBackColor,MyForeColor;



/*  This is a string box widget which handles a list of 
	editable strings  
 */

typedef struct {
               char **list;
               int n;
}  SCRBOX_LIST;

extern int NUPAR,NEQ,NODE,NMarkov;
extern char  upar_names[MAXPAR][XPP_NAME_MAX+1],uvar_names[MAXODE][XPP_NAME_MAX+1];
extern  const char *color_names[];
extern SCRBOX_LIST scrbox_list[10];


/*  This is a new improved pop_up widget */





int do_string_box(int n, int row, int col, const char *title, const char *const *names, char values[][MAX_LEN_SBOX], int maxchar);

#ifdef __cplusplus
}
#endif
#endif
