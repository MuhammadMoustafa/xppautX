#ifndef _derived_h
#define _derived_h
#ifdef __cplusplus
extern "C" {
#endif


void free_derived(void);
int compile_derived(void);
void evaluate_derived(void);
int add_derived(char *name, char *rhs);

 
#ifdef __cplusplus
}
#endif
#endif
