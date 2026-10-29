// Study-only C ABI: private core symbols prevent the two CVODE ABIs colliding.
#pragma once
extern "C" {
void *study_load(const char *path);
const char *study_error(void *p);
int study_size(void *p);
int study_nnz(void *p);
int study_band(void *p);
int study_initial(void *p, double *y);
int study_rhs(void *p, double t, double *y, double *f);
int study_sparse(void *p, double t, double *y, int *row, int *col, double *value);
double study_rhs_parity(void *p);
int study_legacy(void *p, const char *solver, double tolerance, double total, int rows, const char *output, const char *stats);
void study_free(void *p);
}
