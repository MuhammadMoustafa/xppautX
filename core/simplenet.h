#ifndef _simplenet_h_
#define _simplenet_h_
#ifdef __cplusplus
extern "C" {
#endif

double net_interp(double x, int i);
double network_value(double x, int i);
double vector_value(double x, int i);
int get_vector_info(char *str, const char *name, int *root, int *length, int *il, int *ir);
int add_spec_fun(const char *name, char *rhs);
void add_special_name(const char *name, char *rhs);
int add_vectorizer(const char *name, char *rhs);
void add_vectorizer_name(const char *name, const char *rhs);
int is_network(char *s);
void eval_all_nets(void);
void evaluate_network(int ind);
void update_all_ffts(void);
void update_fft(int ind);
void fft_conv(int it, int n, double *values, double *yy, double *fftr, double *ffti, double *dr, double *di);


#ifdef __cplusplus
}
#endif
#endif
