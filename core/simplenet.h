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

#include <array>
#include <string>
#include <vector>

#define MAXVEC 100

/* a network (a special function: conv, sparse, fftcon, gill, ...) as
   the model defines it, xpp::Model's (model.h): its kind, sizes, roots
   and compiled f(root,root2), and the tables whose values are its
   weights, indices and delays (Model::tables' own arrays) */
struct Network {
  int type=0,ncon=0,n=0;
  std::string name;
  int root=0,root2=0;
  std::array<int,20> f{};
  int iwgt=0;
  std::vector<int> gcom; /* a gillespie chain's commands */
  double *weight=nullptr,*index=nullptr,*taud=nullptr;
};

/* what a network computed last and its work space, a Session's
   (session.h): its values, a fftcon's kernel transform (update_fft) and
   the convolution's scratch, a gillespie chain's weights (made on its
   first step, make_gill_nu) */
struct NetworkValues {
  std::vector<double> values;
  std::vector<double> fftr,ffti,dr,di;
  std::vector<double> gill_nu;
  bool gill_ready=false;
};

/* a vectorizer (a variable's neighbours as one name), xpp::Model's */
struct Vectorizer {
  std::string name;
  int root=0,length=0,il=0,ir=0;
};
#endif
#endif
