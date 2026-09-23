#ifndef _edit_rhs_h_
#define _edit_rhs_h_


#include "xpplim.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif


#define NEQMAXFOREDIT 20
#define MAXARG 20
#define MAX_N_EBOX MAXODE
#define MAX_LEN_EBOX 86
#define FORGET_ALL 0
#define DONE_ALL 2
#define FORGET_THIS 3
#define DONE_THIS 1
#define RESET_ALL 4

#define MAXUFUN 50





/*  This is a edit box widget which handles a list of 
	editable strings  
 */

int do_edit_box(int n, char *title, char **names, char **values);
void edit_menu(void);
void edit_rhs(void);
void user_fun_info(FILE *fp);
void edit_functions(void);
int save_as(void);

#ifdef __cplusplus
}
#endif
#endif
