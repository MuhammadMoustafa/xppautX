#ifndef _simplenet_h_
#define _simplenet_h_
#ifdef __cplusplus
extern "C" {
#endif

int is_network(char *s);
void fft_conv(int it, int n, double *values, double *yy, double *fftr, double *ffti, double *dr, double *di);


#ifdef __cplusplus
}

#include <array>
#include <string>
#include <vector>


namespace xpp {
struct Session; /* session.h */
}
/* vector(var,length,e|z|p,e|z|p) in str: its first variable (ses's),
   length and ends' kinds */
int get_vector_info(const xpp::Session &ses, char *str, const char *name, int *root, int *length, int *il, int *ir);

double net_interp(xpp::Session &s, double x, int i);
int add_vectorizer(xpp::Session &s, const char *name, char *rhs);
void add_vectorizer_name(xpp::Session &s, const char *name, const char *rhs);
double vector_value(xpp::Session &s, double x, int i);
double network_value(xpp::Session &s, double x, int i);
int add_spec_fun(xpp::Session &s, const char *name, char *rhs);
void add_special_name(xpp::Session &s, const char *name, char *rhs);
void eval_all_nets(xpp::Session &s);
void evaluate_network(xpp::Session &s, int ind);
void update_all_ffts(xpp::Session &s);
void update_fft(xpp::Session &s, int ind);

#define MAXVEC 100

/* a network (a special function: conv, sparse, fftcon, gill, ...) as
   the model defines it, xpp::Model's (model.h): its kind, sizes, roots
   and compiled f(root,root2), and the tables whose values are its
   weights, indices and delays */
struct Network {
  int type=0,ncon=0,n=0;
  std::string name;
  int root=0,root2=0;
  std::array<int,20> f{};
  int iwgt=0;
  std::vector<int> gcom; /* a gillespie chain's commands */
  /* the tables (Session::tables) of its weights, indices and delays, -1
     for none */
  int weight_table=-1,index_table=-1,taud_table=-1;
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
