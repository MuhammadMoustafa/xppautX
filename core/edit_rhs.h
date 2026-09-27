#ifndef _edit_rhs_h_
#define _edit_rhs_h_


#include "xpplim.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif


#define NEQMAXFOREDIT 20
#define MAXARG 20
#define MAX_LEN_EBOX 86






/*  This is a edit box widget which handles a list of 
	editable strings  
 */

void edit_menu(void);
void edit_rhs(void);
void user_fun_info(FILE *fp);
void edit_functions(void);
int save_as(void);

#ifdef __cplusplus
}
#endif
#endif
