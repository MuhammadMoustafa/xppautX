
#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAXVNAM (XPP_NAME_MAX+1)
#define MAXLINES 5000


/*void break_up_list(char *rhs);
void compile_em();
void free_varinfo();
void remove_blanks(char *s1);
void read_a_line(FILE *fp,char *s);

void subsk(char *big,char *newstr,int k,int flag);
void free_comments();

void add_comment(char *s);
void init_varinfo();
void add_varinfo(int type,char *lhs,char *rhs,int nargs,char args[MAXARG][NAMLEN+1]);
*/

typedef struct {
  char *name,*value;} FIXINFO;
  
  
  
int make_eqn(void);
void strip_saveqn(void);
int disc(const char *string);
void format_list(const char *const *s, int n);
int get_a_filename(char *filename, char *wild);
void list_em(const char *wild);
int read_eqn(void);
int get_eqn(FILE *fptr);
int compiler(char *bob, FILE *fptr);
void welcome(void);
void show_syms(void);
void take_apart(const char *bob, double *value, char *name);
char *get_first(char *string, const char *src);
char *get_next(const char *src);
void find_ker(char *string, int *alt);
void clrscr(void);
int if_include_file(const char *old, char *nf);
int if_end_include(const char *old);
int do_new_parser(FILE *fp, const char *first, int nnn);
void create_plot_list(void);
void add_only(const char *s);
void break_up_list(const char *rhs);
int find_the_name(char list[][MAXVNAM], int n, const char *name);
void compile_em(void);
int formula_or_number(const char *expr, double *z);
void strpiece(char *dest, const char *src, int i0, int ie);
int parse_a_string(char *s1, VAR_INFO *v);
void init_varinfo(void);
void add_varinfo(int type, const char *lhs, const char *rhs, int nargs, char args[MAXARG][NAMLEN+1]);
void free_varinfo(void);
int extract_ode(const char *s1, int *ie, int i1);
int strparse(const char *s1, const char *s2, int i0, int *i1);
int extract_args(const char *s1, int i0, int *ie, int *narg, char args[MAXARG][NAMLEN+1]);
int find_char(const char *s1, const char *s2, int i0, int *i1);
int next_nonspace(const char *s1, int i0, int *i1);
void remove_blanks(char *s1);
void read_a_line(FILE *fp, char *s);
int search_array(char *old, char *newname, int *i1, int *i2, int *flag);
int check_if_ic(const char *big);
int not_ker(const char *s, int i);
int is_comment(const char *s);
void subsk(const char *big, char *newstr, int k, int flag);
void add_comment(const char *s);

/* for parsing par, init with whitespace correctly */
char* new_string2(const char * old, int length);
void advance_past_first_word(char** sptr);
char* get_next2(char** tokens_ptr);
void strcpy_trim(char * dest, const char * source);
void strncpy_trim(char * dest, const char * source, int n);

#ifdef __cplusplus
}
#endif
#endif 
