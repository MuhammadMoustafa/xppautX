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



#define MAX_N_SBOX 22


#define FORGET_ALL 0
#define DONE_ALL 2
#define FORGET_THIS 3
#define DONE_THIS 1





#define EV_MASK (ButtonPressMask 	|\
		KeyPressMask		|\
		ExposureMask		|\
		StructureNotifyMask)	

#define BUT_MASK (ButtonPressMask 	|\
		KeyPressMask		|\
		ExposureMask		|\
		StructureNotifyMask	|\
		EnterWindowMask		|\
		LeaveWindowMask)	



	
	


extern int DisplayWidth,DisplayHeight;
extern int screen;
extern int xor_flag;
extern unsigned int MyBackColor,MyForeColor;

char *get_next(const char *src); /* form_ode.c */
char *get_first(char *string, const char *src);


/*  This is a string box widget which handles a list of 
	editable strings  
 */

typedef struct {
               char **list;
               int n;
}  SCRBOX_LIST;

extern int NUPAR,NEQ,NODE,NMarkov;
extern char  upar_names[MAXPAR][XPP_NAME_MAX+1],uvar_names[MAXODE][XPP_NAME_MAX+1];
extern  char *color_names[];
extern SCRBOX_LIST scrbox_list[10];


/*  This is a new improved pop_up widget */
#define SB_PLOTTABLE 0
#define SB_VARIABLE 1
#define SB_PARAMETER 2
#define SB_PARVAR 3
#define SB_COLOR 4
#define SB_MARKER 5
#define SB_METHOD 6





void make_scrbox_lists(void);
int do_string_box(int n, int row, int col, char *title, char **names, char values[][MAX_LEN_SBOX], int maxchar);
void respond_box(char *button, char *message);
int yes_no_box(void);

#ifdef __cplusplus
}
#endif
#endif
