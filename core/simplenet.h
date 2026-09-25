#ifndef _simplenet_h_
#define _simplenet_h_
#ifdef __cplusplus
extern "C" {
#endif

double net_interp(double x, int i);
double network_value(double x, int i);
double vector_value(double x, int i);
int get_vector_info(char *str, char *name, int *root, int *length, int *il, int *ir);
void init_net(double *v, int n);
int add_spec_fun(char *name, char *rhs);
void add_special_name(char *name, char *rhs);
int add_vectorizer(char *name, char *rhs);
void add_vectorizer_name(char *name, char *rhs);
int is_network(char *s);
void eval_all_nets(void);
void evaluate_network(int ind);
void update_all_ffts(void);
void update_fft(int ind);
void fft_conv(int it, int n, double *values, double *yy, double *fftr, double *ffti, double *dr, double *di);
int gilparse(char *s, int *ind, int *nn);
int g_namelist(char *s, char *root, int *flag, int *i1, int *i2);


#ifdef __cplusplus
}
#endif
#endif
