#ifndef _storage_h_
#define _storage_h_
#ifdef __cplusplus
extern "C" {
#endif

void init_alloc_info(void);
void alloc_meth(void);
void init_stor(int nrow, int ncol);
void free_storage(int ncol);
int reallocstor(int ncol,int nrow);

#ifdef __cplusplus
}
#endif
#endif
