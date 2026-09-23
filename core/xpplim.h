#ifndef _xpplim_h_
#define _xpplim_h_

#define MAXODE 5000
#define MAXODE1 4999
#define MAXDELAY 50
#define MAXPRIMEVAR (MAXODE-10)/2
#define MAXPAR 400
#define MAXFLAG 2000
#define MAX_SYMBS 10000
#define MAXUFUN 50
#define MAX_TAB 50
#define MAXKER 50
#define MAXNET 50
#define MAXMARK 200
#define MAX_ANI_LINES 2000
#define MAX_INTERN_SET 500

/* Longest name, not counting the NUL, of anything a model names: variables,
   parameters, auxiliary quantities, functions and their arguments, tables,
   fixed quantities. Arrays that hold one are XPP_NAME_MAX+1 wide. Front ends
   that must fit a name into a fixed width shorten it for display only. */
#define XPP_NAME_MAX 64

/* one value of a string_box dialog (xpp_ui.h): a name, a number or a short
   formula; a name fits with room for a prime or an index */
#define MAX_LEN_SBOX (XPP_NAME_MAX+16)
#endif
